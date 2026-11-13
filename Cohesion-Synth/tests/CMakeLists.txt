# Offline DSP checks for COHESION - SYNTH (separate from the plugin build).
#   cmake -S tests -B build-tests && cmake --build build-tests --config Release
cmake_minimum_required(VERSION 3.22)
project(CohesionSynthTests VERSION 0.1.0 LANGUAGES C CXX)
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

juce_add_console_app(FxTest PRODUCT_NAME "FxTest")
target_sources(FxTest PRIVATE FxTest.cpp)
target_compile_definitions(FxTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(FxTest PRIVATE juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(ArpTest PRODUCT_NAME "ArpTest")
target_sources(ArpTest PRIVATE ArpTest.cpp)
target_compile_definitions(ArpTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ArpTest PRIVATE juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(PresetTest PRODUCT_NAME "PresetTest")
target_sources(PresetTest PRIVATE PresetTest.cpp)
target_compile_definitions(PresetTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PresetTest PRIVATE juce::juce_audio_processors PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(UiSnapshot PRODUCT_NAME "UiSnapshot")
target_sources(UiSnapshot PRIVATE UiSnapshot.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(UiSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(UiSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(PresetRender PRODUCT_NAME "PresetRender")
target_sources(PresetRender PRIVATE PresetRender.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(PresetRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(PresetRender PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(SwitchTest PRODUCT_NAME "SwitchTest")
target_sources(SwitchTest PRIVATE SwitchTest.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(SwitchTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(SwitchTest PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(VoiceTest PRODUCT_NAME "VoiceTest")
target_sources(VoiceTest PRIVATE VoiceTest.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(VoiceTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(VoiceTest PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(AuditCheck PRODUCT_NAME "AuditCheck")
target_sources(AuditCheck PRIVATE AuditCheck.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(AuditCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(AuditCheck PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(FmTest PRODUCT_NAME "FmTest")
target_sources(FmTest PRIVATE FmTest.cpp)
target_compile_definitions(FmTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(FmTest PRIVATE juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(LaTest PRODUCT_NAME "LaTest")
target_sources(LaTest PRIVATE LaTest.cpp)
target_compile_definitions(LaTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(LaTest PRIVATE juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

# Local reference comparison against Munt (mt32emu, LGPL). Only built when Munt has been
# cloned into ../../_ref/munt (outside git); never linked into the plugin.
set(MUNT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../_ref/munt/mt32emu")
if(EXISTS "${MUNT_DIR}/CMakeLists.txt")
    set(libmt32emu_SHARED OFF CACHE BOOL "" FORCE)
    set(libmt32emu_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MUNT_DIR}" munt EXCLUDE_FROM_ALL)
    juce_add_console_app(LaCompare PRODUCT_NAME "LaCompare")
    target_sources(LaCompare PRIVATE LaCompare.cpp)
    target_compile_definitions(LaCompare PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(LaCompare PRIVATE MT32Emu::mt32emu juce::juce_audio_formats juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()

# Targets that compile the editor need the shared update-check header (../../Common).
# Only UiSnapshot gets an app id + version, so it can render the UPDATE AVAILABLE badge;
# the others never go online. Test a lower version with -DCOHESION_VERSION_OVERRIDE=1.0.0.
include("${CMAKE_CURRENT_SOURCE_DIR}/../../Common/CohesionVersion.cmake")
cohesion_app_version(synth SYNTH_APP_VER)
cohesion_update_check(UiSnapshot synth ${SYNTH_APP_VER})
foreach(t PresetRender SwitchTest VoiceTest AuditCheck)
    cohesion_update_check(${t})
endforeach()
