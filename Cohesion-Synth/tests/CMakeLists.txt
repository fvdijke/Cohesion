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
