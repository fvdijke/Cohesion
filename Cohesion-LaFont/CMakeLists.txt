cmake_minimum_required(VERSION 3.22)

project(CohesionLaFont VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(JUCE_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-ADT/build/_deps/juce-src")
if(EXISTS "${JUCE_LOCAL}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_LOCAL}")
else()
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12 GIT_SHALLOW TRUE)
endif()
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CohesionLaFont
    COMPANY_NAME            "Pepperwood"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ppwd
    PLUGIN_CODE              Claf
    FORMATS                  VST3
    PRODUCT_NAME             "Cohesion LA Font")

target_sources(CohesionLaFont PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# look-and-feel and effects are shared with COHESION - SYNTH
target_include_directories(CohesionLaFont PRIVATE Source ../Cohesion-Synth/Source)

target_compile_definitions(CohesionLaFont PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CohesionLaFont
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Sound engine: FluidSynth (LGPL 2.1+), built as the separate libfluidsynth-3.dll and loaded at
# run time from the plugin's own folder - never linked in. Clone it into ../_ref/fluidsynth:
#   git clone --depth 1 https://github.com/FluidSynth/fluidsynth.git ../_ref/fluidsynth
set(FLUID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../_ref/fluidsynth")
if(EXISTS "${FLUID_DIR}/CMakeLists.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/fluidsynth.cmake")
    add_dependencies(CohesionLaFont_VST3 libfluidsynth)
    add_custom_command(TARGET CohesionLaFont_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:libfluidsynth>" "$<TARGET_FILE_DIR:CohesionLaFont_VST3>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FLUID_DIR}/LICENSE" "$<TARGET_FILE_DIR:CohesionLaFont_VST3>/fluidsynth-LICENSE.txt")
endif()
