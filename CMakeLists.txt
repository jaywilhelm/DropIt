cmake_minimum_required(VERSION 3.22)

project(DropIt VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
#   1. If external/JUCE exists (created by scripts/setup.*), use it.
#   2. Else if -DJUCE_DIR=<path> is given, use that checkout.
#   3. Else download it with FetchContent at the pinned tag.
# ---------------------------------------------------------------------------
set(DROPIT_JUCE_TAG "8.0.9" CACHE STRING "JUCE git tag to use when downloading")
set(JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout (optional)")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(STATUS "DropIt: using JUCE from external/JUCE")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" JUCE EXCLUDE_FROM_ALL)
elseif(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "DropIt: using JUCE from ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "DropIt: downloading JUCE ${DROPIT_JUCE_TAG} via FetchContent")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${DROPIT_JUCE_TAG}
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
option(DROPIT_COPY_AFTER_BUILD "Copy the plugin to the system plugin folder after building" OFF)

set(DROPIT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DROPIT_FORMATS AU)
endif()

juce_add_plugin(DropIt
    COMPANY_NAME                "DropIt Audio"
    PRODUCT_NAME                "DropIt"
    BUNDLE_ID                   "com.dropitaudio.dropit"
    PLUGIN_MANUFACTURER_CODE    Drpi
    PLUGIN_CODE                 Drp1
    FORMATS                     ${DROPIT_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Pitch Shift
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${DROPIT_COPY_AFTER_BUILD})

target_sources(DropIt PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(DropIt PRIVATE Source)

target_compile_definitions(DropIt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DropIt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
