cmake_minimum_required(VERSION 3.21)

project(DAOLib VERSION 0.1.0 LANGUAGES CXX)

# Everything here targets the game's own architecture: DAOrigins.exe is a
# 32-bit x86 executable, so the loader it imports and the plugins the loader
# loads must be too. CMakePresets.json selects the x86 toolset and the
# x86-windows-static-md vcpkg triplet; this is the check that a stray
# configure from a 64-bit shell fails early with a sentence, not late with a
# DLL the game cannot load.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(
        FATAL_ERROR
        "DAOLib must be built for 32-bit x86 (the game's architecture); "
        "configure with a preset from CMakePresets.json, through tools/build.ps1"
    )
endif()

option(DAO_BUILD_TESTS "Build the unit tests (no game required)" ON)
option(DAO_SANITIZE "Build with AddressSanitizer" OFF)
option(DAO_ANALYZE "MSVC static analysis (/analyze) on our own targets" OFF)

# ---------------------------------------------------------------------------
# Shared compile hygiene, per target: max warnings, conformance, and third
# party headers (spdlog, Detours, Catch2, the STL) kept quiet.
# ---------------------------------------------------------------------------
function(dao_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${target} PRIVATE /Zc:preprocessor)
            target_compile_options(
                ${target}
                PRIVATE /external:anglebrackets /external:W0
            )
        else()
            target_compile_options(
                ${target}
                PRIVATE -Wno-missing-field-initializers
            )
        endif()
        if(DAO_ANALYZE AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(
                ${target}
                PRIVATE /analyze /analyze:external-
            )
        endif()
    else()
        target_compile_options(
            ${target}
            PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        )
    endif()
    target_compile_features(${target} PRIVATE cxx_std_20)
endfunction()

# AddressSanitizer has to cover EVERY translation unit, third-party code
# included, so it is global and set before any target is declared. Only the
# tests are meant to run under it: a sanitized DLL inside the game would need
# the ASan runtime beside the game's exe.
if(DAO_SANITIZE)
    if(MSVC)
        add_compile_options(/fsanitize=address /Oy-)
        add_link_options(/INCREMENTAL:NO)
        string(
            REPLACE "/RTC1"
            ""
            CMAKE_CXX_FLAGS_DEBUG
            "${CMAKE_CXX_FLAGS_DEBUG}"
        )
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
    message(
        STATUS
        "DAO_SANITIZE: AddressSanitizer enabled for the whole build tree"
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Quality)

find_package(spdlog CONFIG REQUIRED)
find_path(DETOURS_INCLUDE_DIRS "detours/detours.h" REQUIRED)
find_library(DETOURS_LIBRARY detours REQUIRED)

