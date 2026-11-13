# Offline checks for COHESION - LA MODULE (separate from the plugin build).
cmake_minimum_required(VERSION 3.22)
project(CohesionLaModuleTests VERSION 0.1.0 LANGUAGES C CXX)
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

juce_add_console_app(ModuleTest PRODUCT_NAME "ModuleTest")
target_sources(ModuleTest PRIVATE ModuleTest.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_include_directories(ModuleTest PRIVATE ../Source ../../Cohesion-Synth/Source)
target_compile_definitions(ModuleTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(ModuleTest PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

set(MUNT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../_ref/munt/mt32emu")
if(EXISTS "${MUNT_DIR}/CMakeLists.txt")
    set(libmt32emu_SHARED ON CACHE BOOL "" FORCE)
    set(libmt32emu_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MUNT_DIR}" munt)
    add_dependencies(ModuleTest mt32emu)
    add_custom_command(TARGET ModuleTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:mt32emu>" "$<TARGET_FILE_DIR:ModuleTest>/mt32emu.dll")
endif()

# The editor needs the shared update-check header (no app id: never goes online).
include("${CMAKE_CURRENT_SOURCE_DIR}/../../Common/CohesionVersion.cmake")
cohesion_update_check(ModuleTest)
