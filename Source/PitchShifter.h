#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
    Real-time pitch shifter based on a modulated delay line with
    correlation-aligned splicing.

    How it works
    ------------
    Input is written into a circular buffer. A read "tap" moves through the
    buffer at `ratio` samples per output sample, so for ratio < 1 (pitch down)
    its delay grows by (1 - ratio) every sample. When the delay reaches
    `maxDelay` the tap has to jump back toward the write head. To hide the jump:

      1. Search a range of candidate delays near the write head and pick the
         one whose recent waveform best matches (normalised cross-correlation)
         the waveform under the current tap. This lines up the phases of the
         two signals, which avoids the "warble"/pitch-pull a plain two-tap
         shifter has on low notes.
      2. Crossfade from the old tap to the new one with a raised-cosine fade.

    For ratio > 1 (the "fine" control can tune slightly sharp) the same thing
    happens in reverse: the delay shrinks and the tap jumps back toward maxDelay.

    All channels share the same tap positions (the search runs on the mono
    sum), so the stereo image stays intact.

    When the shift is (almost) exactly zero, the output crossfades to a plain
    copy delayed by exactly getLatencySamples(), so the dry/wet mix never
    comb-filters and latency stays constant.
*/
class PitchShifter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        const auto ms = [sampleRate] (double t) { return juce::jmax (16, (int) std::round (sampleRate * t * 0.001)); };

        searchLen = ms (15.0);   // longest period searched (~67 Hz)
        corrLen   = ms (10.0);   // correlation window
        fadeLen   = ms (10.0);   // crossfade length
        maxDelay  = (double) ms (50.0);

        coarseStep = juce::jmax (1, (int) std::round (sampleRate / 24000.0));

        const int needed = (int) maxDelay + fadeLen + corrLen + searchLen + 16;
        bufferSize = juce::nextPowerOfTwo (needed);
        bufferMask = bufferSize - 1;

        buffers.assign ((size_t) numChannels, std::vector<float> ((size_t) bufferSize, 0.0f));
        mono.assign ((size_t) bufferSize, 0.0f);

        ratio.reset (sampleRate, 0.05);
        unityMix.reset (sampleRate, 0.03);

        reset();
    }

    void reset()
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        std::fill (mono.begin(), mono.end(), 0.0f);

        writePos    = 0;
        activeDelay = (double) getLatencySamples();
        fading      = false;
        fadePos     = 0;

        ratio.setCurrentAndTargetValue (targetRatio);
        unityMix.setCurrentAndTargetValue (isUnity (targetRatio) ? 1.0f : 0.0f);
    }

    /** ratio = 2^(semitones / 12). 0.5 = one octave down, 1 = no change. */
    void setPitchRatio (float newRatio)
    {
        targetRatio = newRatio;
        ratio.setTargetValue (newRatio);
        unityMix.setTargetValue (isUnity (newRatio) ? 1.0f : 0.0f);
    }

    /** Average delay of the wet signal; report this to the host. */
    int getLatencySamples() const noexcept
    {
        return (int) std::round (0.5 * ((double) minDelay + 0.5 * searchLen + maxDelay));
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const auto numChannels = juce::jmin ((int) block.getNumChannels(), (int) buffers.size());
        const auto numSamples  = (int) block.getNumSamples();
        const auto latency     = (double) getLatencySamples();
        const auto monoScale   = 1.0f / (float) juce::jmax (1, numChannels);
        const auto maxSafe     = (double) (bufferSize - corrLen - 8);

        for (int i = 0; i < numSamples; ++i)
        {
            const double r    = (double) ratio.getNextValue();
            const float  u    = unityMix.getNextValue();
            const double rate = 1.0 - r;   // delay change per sample

            // Write input
            float m = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x = block.getChannelPointer ((size_t) ch)[i];
                buffers[(size_t) ch][(size_t) writePos] = x;
                m += x;
            }
            mono[(size_t) writePos] = m * monoScale;

            // Need to splice?
            if (! fading)
            {
                if (rate > 0.0 && activeDelay >= maxDelay)
                    startSplice (minDelay, minDelay + searchLen);
                else if (rate < 0.0 && activeDelay <= (double) (minDelay + fadeLen))
                    startSplice ((int) maxDelay - searchLen, (int) maxDelay);
            }

            const float w = fading ? fadeGain (fadePos) : 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto& buf = buffers[(size_t) ch];
                float out = 0.0f;

                if (u < 1.0f)
                {
                    float shifted = readDelayed (buf, activeDelay);
                    if (fading)
                        shifted += w * (readDelayed (buf, newDelay) - shifted);
                    out = (1.0f - u) * shifted;
                }

                if (u > 0.0f)
                    out += u * readDelayed (buf, latency);

                block.getChannelPointer ((size_t) ch)[i] = out;
            }

            // Advance taps
            activeDelay = juce::jlimit ((double) minDelay, maxSafe, activeDelay + rate);

            if (fading)
            {
                newDelay = juce::jlimit ((double) minDelay, maxSafe, newDelay + rate);

                if (++fadePos >= fadeLen)
                {
                    activeDelay = newDelay;
                    fading = false;
                }
            }

            writePos = (writePos + 1) & bufferMask;
        }
    }

private:
    static bool isUnity (float r) noexcept { return std::abs (r - 1.0f) < 1.0e-4f; }

    float fadeGain (int pos) const noexcept
    {
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) pos / (float) fadeLen);
    }

    float monoAt (int delay) const noexcept { return mono[(size_t) ((writePos - delay) & bufferMask)]; }

    /** Normalised correlation between the active tap's recent past and a candidate delay. */
    float score (int activeInt, int candidate, int stride) const noexcept
    {
        float dot = 0.0f, energy = 0.0f;
        for (int j = 0; j < corrLen; j += stride)
        {
            const float ref = monoAt (activeInt + j);
            const float c   = monoAt (candidate + j);
            dot    += ref * c;
            energy += c * c;
        }
        return dot / std::sqrt (energy + 1.0e-9f);
    }

    /** Find the best-matching delay in [lo, hi] and begin a crossfade to it. */
    void startSplice (int lo, int hi)
    {
        const int activeInt = (int) std::floor (activeDelay);
        const double frac   = activeDelay - (double) activeInt;

        // Coarse search, then refine around the best coarse candidate.
        int best = lo;
        float bestScore = -1.0e30f;
        for (int d = lo; d <= hi; d += coarseStep)
        {
            const float s = score (activeInt, d, 2);
            if (s > bestScore) { bestScore = s; best = d; }
        }

        const int refineLo = juce::jmax (lo, best - coarseStep);
        const int refineHi = juce::jmin (hi, best + coarseStep);
        bestScore = -1.0e30f;
        for (int d = refineLo; d <= refineHi; ++d)
        {
            const float s = score (activeInt, d, 1);
            if (s > bestScore) { bestScore = s; best = d; }
        }

        newDelay = (double) best + frac;  // keep the sub-sample offset so waveforms line up
        fading   = true;
        fadePos  = 0;
    }

    /** 4-point, 3rd-order Hermite interpolated read, `delay` samples behind the write head. */
    float readDelayed (const std::vector<float>& buf, double delay) const noexcept
    {
        double readPos = (double) writePos - delay;
        if (readPos < 0.0)
            readPos += (double) bufferSize;

        const int   i0   = (int) readPos;
        const float frac = (float) (readPos - (double) i0);

        const float xm1 = buf[(size_t) ((i0 - 1) & bufferMask)];
        const float x0  = buf[(size_t) ( i0      & bufferMask)];
        const float x1  = buf[(size_t) ((i0 + 1) & bufferMask)];
        const float x2  = buf[(size_t) ((i0 + 2) & bufferMask)];

        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    static constexpr int minDelay = 4;   // keeps the interpolator behind the write head

    std::vector<std::vector<float>> buffers;
    std::vector<float> mono;

    int bufferSize = 0, bufferMask = 0, writePos = 0;
    int searchLen = 720, corrLen = 480, fadeLen = 480, coarseStep = 2;
    double maxDelay = 2400.0;

    double activeDelay = 0.0, newDelay = 0.0;
    bool fading = false;
    int fadePos = 0;

    float targetRatio = 1.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> ratio { 1.0f };
    juce::SmoothedValue<float> unityMix { 1.0f };
};
