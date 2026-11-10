#!/usr/bin/env bash
# Checks build prerequisites for DropIt on macOS / Linux and downloads JUCE.
#
#   ./scripts/setup.sh                 # check + download JUCE
#   ./scripts/setup.sh --install-deps  # also install Linux packages via apt/dnf/pacman
#   JUCE_TAG=8.0.9 ./scripts/setup.sh  # choose a JUCE version
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JUCE_TAG="${JUCE_TAG:-8.0.9}"
INSTALL_DEPS=0
[[ "${1:-}" == "--install-deps" ]] && INSTALL_DEPS=1

ok()   { printf "  \033[32m[ OK ]\033[0m %s\n" "$1"; }
bad()  { printf "  \033[31m[FAIL]\033[0m %s\n" "$1"; }
info() { printf "         %s\n" "$1"; }

OS="$(uname -s)"
LINUX_PKGS_APT="build-essential cmake git pkg-config libasound2-dev libjack-jackd2-dev ladspa-sdk \
libcurl4-openssl-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev"
LINUX_PKGS_DNF="gcc-c++ make cmake git pkgconf-pkg-config alsa-lib-devel jack-audio-connection-kit-devel \
freetype-devel fontconfig-devel libX11-devel libXcomposite-devel libXcursor-devel libXext-devel \
libXinerama-devel libXrandr-devel libXrender-devel mesa-libGL-devel libcurl-devel"
LINUX_PKGS_PACMAN="base-devel cmake git pkgconf alsa-lib jack2 freetype2 fontconfig libx11 libxcomposite \
libxcursor libxext libxinerama libxrandr libxrender mesa curl"

if [[ "$OS" == "Linux" && $INSTALL_DEPS -eq 1 ]]; then
    echo "Installing Linux dependencies (sudo required)..."
    if command -v apt-get >/dev/null; then
        sudo apt-get update && sudo apt-get install -y $LINUX_PKGS_APT
    elif command -v dnf >/dev/null; then
        sudo dnf install -y $LINUX_PKGS_DNF
    elif command -v pacman >/dev/null; then
        sudo pacman -S --needed --noconfirm $LINUX_PKGS_PACMAN
    else
        bad "Unknown package manager - install the packages listed in README.md manually."
    fi
fi

printf "\n\033[36mDropIt setup - checking prerequisites\033[0m\n\n"
missing=0

if command -v git >/dev/null; then ok "git: $(git --version)"; else bad "git not found"; missing=1; fi

if command -v cmake >/dev/null; then
    ver="$(cmake --version | head -n1 | awk '{print $3}')"
    if [[ "$(printf '%s\n%s\n' 3.22 "$ver" | sort -t. -k1,1n -k2,2n | head -n1)" != "3.22" ]]; then
        bad "cmake $ver is too old (need 3.22+)"; missing=1
    else
        ok "cmake: $ver"
    fi
else
    bad "cmake not found"; missing=1
fi

if [[ "$OS" == "Darwin" ]]; then
    if xcode-select -p >/dev/null 2>&1; then ok "Xcode command line tools: $(xcode-select -p)"
    else bad "Xcode command line tools missing"; info "Run: xcode-select --install"; missing=1; fi
    command -v cmake >/dev/null || info "Install CMake: brew install cmake"
else
    if command -v c++ >/dev/null; then ok "C++ compiler: $(c++ --version | head -n1)"
    else bad "No C++ compiler found"; missing=1; fi

    if command -v pkg-config >/dev/null; then
        for lib in alsa freetype2 x11 xrandr xinerama xcursor; do
            if pkg-config --exists "$lib"; then ok "dev package: $lib"
            else bad "dev package missing: $lib"; missing=1; fi
        done
    else
        bad "pkg-config not found"; missing=1
    fi
    [[ $missing -eq 1 ]] && info "Re-run with --install-deps, or see README.md for the package list."
fi

if [[ $missing -eq 1 ]]; then
    printf "\n\033[33mInstall the missing tools above and run setup again.\033[0m\n\n"
    exit 1
fi

JUCE_DIR="$ROOT/external/JUCE"
if [[ -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    ok "JUCE already present at external/JUCE"
else
    printf "\nDownloading JUCE %s into external/JUCE ...\n" "$JUCE_TAG"
    mkdir -p "$ROOT/external"
    git clone --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git "$JUCE_DIR"
    ok "JUCE $JUCE_TAG downloaded"
fi

printf "\n\033[32mSetup complete. Next: ./scripts/build.sh\033[0m\n\n"
