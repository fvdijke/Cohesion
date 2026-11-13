cmake_minimum_required(VERSION 3.22)

# COHESION - SYSEX: DX7 bank builder (standalone app) + offline round-trip test.
#   cmake -S . -B build && cmake --build build --config Release
# Optional: -DCOHESION_JUCE_DIR=<path to a JUCE 7.0.12 source tree>
# Version comes from <repo>/versions.json (see Common/CohesionVersion.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/../Common/CohesionVersion.cmake")
cohesion_app_version(sysex COHESION_APP_VER)
project(CohesionSysEx VERSION ${COHESION_APP_VER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COHESION_JUCE_DIR "" CACHE PATH "Existing JUCE source tree (optional)")
include(FetchContent)
set(_juce_found "")
foreach(cand "${COHESION_JUCE_DIR}"
             "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-ADT/build/_deps/juce-src"
             "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-Glue/build/_deps/juce-src"
             "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-EQComp/build/_deps/juce-src"
             "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-Transient/build/_deps/juce-src")
    if(NOT _juce_found AND NOT cand STREQUAL "" AND EXISTS "${cand}/CMakeLists.txt")
        set(_juce_found "${cand}")
    endif()
endforeach()
if(_juce_found)
    FetchContent_Declare(JUCE SOURCE_DIR "${_juce_found}")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
        GIT_SHALLOW    TRUE)
endif()
FetchContent_MakeAvailable(JUCE)

# shared with COHESION - SYNTH: DX7 parser / packer / FM engine, house style, algorithm diagram
set(SYNTH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../Cohesion-Synth/Source")

juce_add_gui_app(CohesionSysEx
    COMPANY_NAME "Pepperwood"
    PRODUCT_NAME "Cohesion SysEx"
    VERSION      ${PROJECT_VERSION})

target_sources(CohesionSysEx PRIVATE Source/Main.cpp)
target_include_directories(CohesionSysEx PRIVATE "${SYNTH_SRC}")
target_compile_definitions(CohesionSysEx PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CohesionSysEx,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CohesionSysEx,JUCE_VERSION>")

# start-up update check (UPDATE AVAILABLE in the header)
cohesion_update_check(CohesionSysEx sysex)

target_link_libraries(CohesionSysEx
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

option(COHESION_SYSEX_TESTS "Build the BankTest round-trip check" ON)
if(COHESION_SYSEX_TESTS)
    juce_add_console_app(BankTest PRODUCT_NAME "BankTest")
    target_sources(BankTest PRIVATE tests/BankTest.cpp)
    target_include_directories(BankTest PRIVATE "${SYNTH_SRC}")
    target_compile_definitions(BankTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BankTest PRIVATE juce::juce_core PUBLIC juce::juce_recommended_config_flags)
endif()
