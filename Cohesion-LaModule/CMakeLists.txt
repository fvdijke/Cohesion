cmake_minimum_required(VERSION 3.22)

# Version comes from <repo>/versions.json (see Common/CohesionVersion.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/../Common/CohesionVersion.cmake")
cohesion_app_version(lamodule COHESION_APP_VER)
project(CohesionLaModule VERSION ${COHESION_APP_VER} LANGUAGES C CXX)

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

juce_add_plugin(CohesionLaModule
    COMPANY_NAME            "Pepperwood"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ppwd
    PLUGIN_CODE              Clam
    FORMATS                  VST3
    PRODUCT_NAME             "Cohesion LA Module")

target_sources(CohesionLaModule PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# The LA engine, ROM library, effects and look-and-feel are shared with COHESION - SYNTH.
target_include_directories(CohesionLaModule PRIVATE Source ../Cohesion-Synth/Source)

target_compile_definitions(CohesionLaModule PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# start-up update check (UPDATE AVAILABLE in the header)
cohesion_update_check(CohesionLaModule lamodule)

target_link_libraries(CohesionLaModule
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Optional MUNT engine: Munt (mt32emu, LGPL 2.1+) is built as a separate shared library
# (mt32emu.dll) and loaded at run time from the plugin's own folder - never linked in.
# Clone https://github.com/munt/munt into ../_ref/munt to build it.
set(MUNT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../_ref/munt/mt32emu")
if(EXISTS "${MUNT_DIR}/CMakeLists.txt")
    set(libmt32emu_SHARED ON CACHE BOOL "" FORCE)
    set(libmt32emu_C_INTERFACE ON CACHE BOOL "" FORCE)
    set(libmt32emu_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MUNT_DIR}" munt)
    add_dependencies(CohesionLaModule_VST3 mt32emu)
    add_custom_command(TARGET CohesionLaModule_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:mt32emu>"
                "$<TARGET_FILE_DIR:CohesionLaModule_VST3>/mt32emu.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MUNT_DIR}/COPYING.LESSER.txt"
                "$<TARGET_FILE_DIR:CohesionLaModule_VST3>/mt32emu-COPYING.LESSER.txt")
endif()
