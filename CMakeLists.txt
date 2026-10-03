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

# DAOLib is built two ways: on its own, and inside a plugin's repository
# that brings it in with add_subdirectory (extern/DAOLib, as Skyrim plugins
# bring in CommonLibSSE). Inside another, it gives the library and
# add_daolib_plugin (cmake/DAOLib.cmake) and leaves its own plugins, tests
# and quality targets out; the loader is declared either way, for a build
# that asks for it by name.
option(
    DAO_BUILD_TESTS
    "Build the unit tests (no game required)"
    ${PROJECT_IS_TOP_LEVEL}
)
option(
    DAO_BUILD_PLUGINS
    "Build the probe and example plugins"
    ${PROJECT_IS_TOP_LEVEL}
)
option(
    DAO_BUILD_EXAMPLES
    "Build the example plugins in examples/"
    ${PROJECT_IS_TOP_LEVEL}
)
option(DAO_SANITIZE "Build with AddressSanitizer" OFF)
# The debug-analyze preset; its own build tree, as clang-tidy reads the
# debug tree's compile database and clang rejects /analyze.
option(DAO_ANALYZE "MSVC static analysis (/analyze) on our own targets" OFF)

# ---------------------------------------------------------------------------
# Shared compile hygiene, per target: max warnings, conformance, and third
# party headers (spdlog, Detours, Catch2, the STL) kept quiet.
# ---------------------------------------------------------------------------
function(dao_apply_warnings target)
    if(MSVC)
        # /WX: a warning in our own code fails the build (third-party
        # headers are kept at /external:W0 below).
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
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
        # vcpkg's spdlog, fmt and Catch2 are built without ASan, so the STL's
        # container annotations, on under /fsanitize=address, would differ
        # between them and us: LNK2038 on annotate_string and annotate_vector.
        # Microsoft's advice for linking such libraries is to turn them off.
        add_compile_definitions(
            _DISABLE_STRING_ANNOTATION
            _DISABLE_VECTOR_ANNOTATION
        )
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

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
# The format and lint targets cover the top-level tree's own sources, so they
# are the including repository's to define when DAOLib is not the top level.
if(PROJECT_IS_TOP_LEVEL)
    include(Quality)
endif()
# add_daolib_plugin. A function is global once defined, so a repository that
# adds this directory can call it afterwards.
include(DAOLib)

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
# The address library: an ID for every function and global of the game, and
# where each is in each build of it. daolib holds IDs (dao/Addresses.h) and
# reads the file for the running build at run time (dao/Module.h); the
# library's own build packs those files into <build>/daolib/addresses,
# beside the loader, where they are looked for.
if(NOT TARGET DAOAddressLibrary::daoaddr)
    add_subdirectory(extern/DAOAddressLibrary)
endif()

add_library(
    daolib
    STATIC
    src/dao/core/CallSite.cpp
    src/dao/core/Exceptions.cpp
    src/dao/core/Conversation.cpp
    src/dao/core/Fingerprint.cpp
    src/dao/core/FunctionTable.cpp
    src/dao/core/Gff4.cpp
    src/dao/core/GfxMovie.cpp
    src/dao/core/Inflate.cpp
    src/dao/core/OptionsMovie.cpp
    src/dao/core/Pe.cpp
    src/dao/core/Rtti.cpp
    src/dao/core/SaveMeta.cpp
    src/dao/core/StackText.cpp
    src/dao/core/ScriptValue.cpp
    src/dao/core/TalkTable.cpp
    src/dao/Data.cpp
    src/dao/Dialogue.cpp
    src/dao/Journal.cpp
    src/dao/Animation.cpp
    src/dao/ScriptCall.cpp
    src/dao/Orders.cpp
    src/dao/Abilities.cpp
    src/dao/Vfx.cpp
    src/dao/CommandEvents.cpp
    src/dao/Conjure.cpp
    src/dao/Strings.cpp
    src/dao/Party.cpp
    src/dao/Portraits.cpp
    src/dao/Resources.cpp
    src/dao/Events.cpp
    src/dao/Frame.cpp
    src/dao/CameraMoves.cpp
    src/dao/Clock.cpp
    src/dao/World.cpp
    src/dao/Effects.cpp
    src/dao/GameModes.cpp
    src/dao/Gda.cpp
    src/dao/Gui.cpp
    src/dao/Input.cpp
    src/dao/Options.cpp
    src/dao/Log.cpp
    src/dao/Messages.cpp
    src/dao/Saves.cpp
    src/dao/Module.cpp
    src/dao/SafeRead.cpp
    src/dao/Script.cpp
    src/dao/ScriptHooks.cpp
    src/dao/String.cpp
    src/dao/Timers.cpp
    src/dao/Tlk.cpp
    src/dao/TypeName.cpp
)
dao_apply_warnings(daolib)
# The headers are C++20 (std::span among them), so whatever links daolib is.
target_compile_features(daolib PUBLIC cxx_std_20)
target_include_directories(daolib PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_include_directories(daolib SYSTEM PUBLIC ${DETOURS_INCLUDE_DIRS})
target_link_libraries(
    daolib
    PUBLIC spdlog::spdlog ${DETOURS_LIBRARY} DAOAddressLibrary::daoaddr
)
target_compile_definitions(
    daolib
    PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX DAO_VERSION_STRING="${PROJECT_VERSION}"
)
add_library(DAOLib::daolib ALIAS daolib)
# The address files are built with whatever links daolib. On its own the
# address library builds them as part of `all`, which a project that adds
# DAOLib with EXCLUDE_FROM_ALL (as the plugins' own repos do) leaves out:
# its build made a loader and plugins and no file for them to read, found
# by the Eluvian server 2026-10-01.
add_dependencies(daolib daoaddr_data)

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
# At the top of the build tree, where the game folder's layout starts, when
# DAOLib is built inside another repository too.
set_target_properties(
    version
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "version"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
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
if(DAO_BUILD_PLUGINS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(DAO_PROBE_TARGETS
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ProbeTargets.inc"
    )
    add_custom_command(
        OUTPUT "${DAO_PROBE_TARGETS}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/probe_targets.py"
            "${DAO_PROBE_TARGETS}"
        DEPENDS
            "${PROJECT_SOURCE_DIR}/tools/probe_targets.py"
            "${PROJECT_SOURCE_DIR}/dev/script_commands.csv"
            "${PROJECT_SOURCE_DIR}/dev/message_handlers.csv"
        COMMENT "Generating the probe's census targets from dev/*.csv"
        VERBATIM
    )
    add_daolib_plugin(
        probe
        SOURCES
            src/plugins/probe/Census.cpp
            src/plugins/probe/Conjure.cpp
            src/plugins/probe/Events.cpp
            src/plugins/probe/Probe.cpp
            src/plugins/probe/Recorder.cpp
            src/plugins/probe/ScriptChecks.cpp
            "${DAO_PROBE_TARGETS}"
    )
    dao_apply_warnings(probe)
    target_include_directories(
        probe
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )

    # The example plugin: how a plugin uses daolib, and a test of it in play.
    add_daolib_plugin(example SOURCES src/plugins/example/Example.cpp)
    dao_apply_warnings(example)
endif()

# ---------------------------------------------------------------------------
# Tests, over dao/core: what can be checked without the game.
# ---------------------------------------------------------------------------
if(DAO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# The examples: a small plugin per part of the library (examples/README.md).
if(DAO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
