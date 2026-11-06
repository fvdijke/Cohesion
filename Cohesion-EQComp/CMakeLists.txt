cmake_minimum_required(VERSION 3.22)

project(CohesionEQComp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Reuse the JUCE source already fetched by the GLUE project (fast, no download).
# Falls back to a fresh fetch if that path is missing.
# ---------------------------------------------------------------------------
include(FetchContent)
# Reuse the JUCE source fetched by the GLUE project. Paths are relative to this
# project so they survive renaming the parent folder, and we accept either the
# old ("Cohesion") or new ("COHESION - GLUE") sibling-folder name.
set(JUCE_CAND0 "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-ADT/build/_deps/juce-src")
set(JUCE_CAND1 "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-Glue/build/_deps/juce-src")
set(JUCE_CAND2 "${CMAKE_CURRENT_SOURCE_DIR}/../COHESION - GLUE/build/_deps/juce-src")
set(JUCE_CAND3 "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion/build/_deps/juce-src")
if(EXISTS "${JUCE_CAND0}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_CAND0}")
elseif(EXISTS "${JUCE_CAND1}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_CAND1}")
elseif(EXISTS "${JUCE_CAND2}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_CAND2}")
elseif(EXISTS "${JUCE_CAND3}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_CAND3}")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
        GIT_SHALLOW    TRUE)
endif()
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CohesionEQComp
    COMPANY_NAME            "Pepperwood"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ppwd
    PLUGIN_CODE              Ceqc
    FORMATS                  VST3
    PRODUCT_NAME             "Cohesion EQ-Comp")

target_sources(CohesionEQComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(CohesionEQComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CohesionEQComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
