# Offline checks for COHESION - LA FONT (separate from the plugin build).
cmake_minimum_required(VERSION 3.22)
project(CohesionLaFontTests VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(JUCE_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/../../Cohesion-ADT/build/_deps/juce-src")
if(EXISTS "${JUCE_LOCAL}/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_LOCAL}")
else()
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12 GIT_SHALLOW TRUE)
endif()
FetchContent_MakeAvailable(JUCE)

juce_add_console_app(FontTest PRODUCT_NAME "FontTest")
target_sources(FontTest PRIVATE FontTest.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_include_directories(FontTest PRIVATE ../Source ../../Cohesion-Synth/Source)
target_compile_definitions(FontTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(FontTest PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

set(FLUID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../_ref/fluidsynth")
if(EXISTS "${FLUID_DIR}/CMakeLists.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/fluidsynth.cmake")
    add_dependencies(FontTest libfluidsynth)
    add_custom_command(TARGET FontTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:libfluidsynth>" "$<TARGET_FILE_DIR:FontTest>/")
endif()
