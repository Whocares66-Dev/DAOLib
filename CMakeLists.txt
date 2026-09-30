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

# ---------------------------------------------------------------------------
# daolib -- the library plugins are written against: where the game's code
# and data are (Addresses.h, relocated if the exe is ever loaded off its
# preferred base), the exe fingerprint every address was read from, class
# names from the engine's RTTI, a detour wrapper, and a log per DLL.
#
# `dao/core` is the part that never touches the running game -- pure
# functions over bytes and numbers -- and is what tests/ covers. The rest
# reads process memory and is verified in play.
# ---------------------------------------------------------------------------
add_library(
    daolib
    STATIC
    src/dao/core/CallSite.cpp
    src/dao/core/Exceptions.cpp
    src/dao/core/Fingerprint.cpp
    src/dao/core/FunctionTable.cpp
    src/dao/core/Pe.cpp
    src/dao/core/Rtti.cpp
    src/dao/Log.cpp
    src/dao/Module.cpp
    src/dao/SafeRead.cpp
    src/dao/Script.cpp
    src/dao/ScriptHooks.cpp
    src/dao/String.cpp
    src/dao/TypeName.cpp
)
dao_apply_warnings(daolib)
target_include_directories(daolib PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(daolib SYSTEM PUBLIC ${DETOURS_INCLUDE_DIRS})
target_link_libraries(daolib PUBLIC spdlog::spdlog ${DETOURS_LIBRARY})
target_compile_definitions(
    daolib
    PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX DAO_VERSION_STRING="${PROJECT_VERSION}"
)

# ---------------------------------------------------------------------------
# version.dll -- the loader. Dropped beside DAOrigins.exe it is loaded in
# place of the system's version.dll (the game imports three of its functions,
# which are forwarded to the real one), detours the exe's entry point, and
# from there loads every DLL in daolib\plugins\ on the game's main thread
# before any game code runs.
# ---------------------------------------------------------------------------
add_library(
    version
    SHARED
    src/loader/CrashLog.cpp
    src/loader/Loader.cpp
    src/loader/Proxy.cpp
)
dao_apply_warnings(version)
set_target_properties(version PROPERTIES PREFIX "" OUTPUT_NAME "version")
# dbghelp names the frames of a crash log; psapi gives the process's memory.
target_link_libraries(version PRIVATE daolib dbghelp psapi)

# ---------------------------------------------------------------------------
# Plugins. Each is a DLL exporting DAOLib_Load (src/dao/Plugin.h); the build
# puts them where the loader looks relative to itself, so build\<preset>\ is
# a game folder's worth of files, and tools\deploy.ps1 copies it over.
# ---------------------------------------------------------------------------
#
# The probe counts calls to every function the tables in dev/ name; its list
# of them is generated from those tables at build time.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(DAO_PROBE_TARGETS "${CMAKE_BINARY_DIR}/generated/ProbeTargets.inc")
add_custom_command(
    OUTPUT "${DAO_PROBE_TARGETS}"
    COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/probe_targets.py"
        "${DAO_PROBE_TARGETS}"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/probe_targets.py"
        "${CMAKE_SOURCE_DIR}/dev/script_commands.csv"
        "${CMAKE_SOURCE_DIR}/dev/message_handlers.csv"
    COMMENT "Generating the probe's census targets from dev/*.csv"
    VERBATIM
)
add_library(
    probe
    MODULE
    src/plugins/probe/Census.cpp
    src/plugins/probe/Events.cpp
    src/plugins/probe/Probe.cpp
    src/plugins/probe/Recorder.cpp
    src/plugins/probe/ScriptChecks.cpp
    "${DAO_PROBE_TARGETS}"
)
dao_apply_warnings(probe)
set_target_properties(
    probe
    PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/daolib/plugins"
)
target_include_directories(probe PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(probe PRIVATE daolib)

# The example plugin: how a plugin uses daolib, and a test of it in play.
add_library(example MODULE src/plugins/example/Example.cpp)
dao_apply_warnings(example)
set_target_properties(
    example
    PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/daolib/plugins"
)
target_link_libraries(example PRIVATE daolib)

# ---------------------------------------------------------------------------
# Tests, over dao/core: what can be checked without the game.
# ---------------------------------------------------------------------------
if(DAO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
