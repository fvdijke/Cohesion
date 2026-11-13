cmake_minimum_required(VERSION 3.22)

# Version comes from <repo>/versions.json (see Common/CohesionVersion.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/../Common/CohesionVersion.cmake")
cohesion_app_version(glue COHESION_APP_VER)
project(Cohesion VERSION ${COHESION_APP_VER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE is fetched automatically on first configure (needs internet once).
# Pinned to a known-good stable tag.
# ---------------------------------------------------------------------------
include(FetchContent)
# Reuse JUCE source already fetched by a sibling project if present (no download).
set(JUCE_REUSE1 "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-ADT/build/_deps/juce-src")
set(JUCE_REUSE2 "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-EQComp/build/_deps/juce-src")
if(EXISTS "${JUCE_REUSE1}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_REUSE1}")
elseif(EXISTS "${JUCE_REUSE2}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_REUSE2}")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
        GIT_SHALLOW    TRUE)
endif()
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Cohesion
    COMPANY_NAME            "Pepperwood"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE         # we copy manually so a locked file never fails the build
    PLUGIN_MANUFACTURER_CODE Ppwd
    PLUGIN_CODE              Cohs
    FORMATS                  VST3
    PRODUCT_NAME             "Cohesion Glue")

target_sources(Cohesion PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Cohesion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# start-up update check (UPDATE AVAILABLE in the header)
cohesion_update_check(Cohesion glue)

target_link_libraries(Cohesion
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
