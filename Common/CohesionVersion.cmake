# ---------------------------------------------------------------------------
# COHESION version helpers (shared by every app; include it before project()).
#
#   include("${CMAKE_CURRENT_SOURCE_DIR}/../Common/CohesionVersion.cmake")
#   cohesion_app_version(synth SYNTH_VERSION)          # -> "1.3.1" from versions.json
#   project(CohesionSynth VERSION ${SYNTH_VERSION} ...)
#   ...
#   cohesion_update_check(CohesionSynth synth)         # after juce_add_plugin / juce_add_gui_app
#
# The version of each app lives in <repo>/versions.json ("apps": { "<id>": "x.y.z" }).
# For testing the update badge, configure with -DCOHESION_VERSION_OVERRIDE=1.0.0 to build an
# app that pretends to be older than what versions.json says.
# ---------------------------------------------------------------------------
set(COHESION_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(COHESION_VERSIONS_JSON "${CMAKE_CURRENT_LIST_DIR}/../versions.json")
set(COHESION_VERSION_OVERRIDE "" CACHE STRING "Build with this version instead of the one in versions.json (testing only)")

function(cohesion_app_version app_id out_var)
    file(READ "${COHESION_VERSIONS_JSON}" _json)
    string(JSON _ver ERROR_VARIABLE _err GET "${_json}" apps ${app_id})
    if(_err)
        message(FATAL_ERROR "versions.json has no entry apps.${app_id}: ${_err}")
    endif()
    if(NOT COHESION_VERSION_OVERRIDE STREQUAL "")
        message(STATUS "COHESION ${app_id}: version override ${COHESION_VERSION_OVERRIDE} (versions.json says ${_ver})")
        set(_ver "${COHESION_VERSION_OVERRIDE}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${COHESION_VERSIONS_JSON}")
    set(${out_var} "${_ver}" PARENT_SCOPE)
endfunction()

# Makes CohesionUpdateCheck.h available to a target and tells it who it is.
# Without app_id (tests) the header compiles but never goes online.
function(cohesion_update_check target)
    target_include_directories(${target} PRIVATE "${COHESION_COMMON_DIR}")
    if(ARGC GREATER 1)
        set(_id "${ARGV1}")
        if(ARGC GREATER 2)
            set(_ver "${ARGV2}")
        else()
            set(_ver "${PROJECT_VERSION}")
        endif()
        target_compile_definitions(${target} PRIVATE
            COHESION_APP_ID="${_id}"
            COHESION_APP_VERSION="${_ver}")

        # JUCE generates <target>_resources.rc (the Windows file version, which the installer reads)
        # only when it is missing: it has no dependency on the version. Drop a stale one.
        get_target_property(_gen ${target} JUCE_GENERATED_SOURCES_DIRECTORY)
        if(_gen)
            set(_rc "${_gen}/${target}_resources.rc")
            if(EXISTS "${_rc}")
                file(READ "${_rc}" _rc_text)
                string(REPLACE "." "," _commas "${_ver}")
                string(FIND "${_rc_text}" "FILEVERSION  ${_commas}" _pos)
                if(_pos EQUAL -1)
                    message(STATUS "COHESION ${_id}: version changed to ${_ver}, regenerating ${target}_resources.rc")
                    file(REMOVE "${_rc}")
                endif()
            endif()
        endif()
    endif()
endfunction()
