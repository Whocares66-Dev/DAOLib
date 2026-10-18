cmake_minimum_required(VERSION 3.21)

project(DAOLib VERSION 0.4.0 LANGUAGES CXX)

# What goes into the game targets its architecture: DAOrigins.exe is a 32-bit
# x86 executable, so the loader it imports and the plugins the loader loads
# must be too. CMakePresets.json selects the x86 toolset and the
# x86-windows-static-md vcpkg triplet for them. dao/core, the part that never
# touches the game, builds 64-bit as well, for tools that read the game's
# files with no game (Eluvian's ghilana, DAOLib #106): a 64-bit build is the
# core and its tests alone, and asking it for more fails early with a
# sentence, not late with a DLL the game cannot load.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DAO_CORE_ONLY_DEFAULT ON)
else()
    set(DAO_CORE_ONLY_DEFAULT OFF)
endif()
option(
    DAO_CORE_ONLY
    "Build dao/core and its tests alone (the only build a 64-bit one can be)"
    ${DAO_CORE_ONLY_DEFAULT}
)
if(NOT DAO_CORE_ONLY AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(
        FATAL_ERROR
        "The loader, daolib and the plugins must be built for 32-bit x86 "
        "(the game's architecture); a 64-bit build is dao/core alone "
        "(DAO_CORE_ONLY). Configure with a preset from CMakePresets.json, "
        "through tools/build.ps1"
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
# add_daolib_plugin and dao_release_symbols. A function is global once
# defined, so a repository that adds this directory can call it afterwards.
include(DAOLib)

# ---------------------------------------------------------------------------
# daolib_core -- dao/core: what never touches the running game, pure
# functions over bytes and numbers. The game's files and formats, PE headers,
# address arithmetic, name undecoration. It is what tests/ covers, it is all
# a tool that reads the game's files with no game links, and it builds for
# x86 and x64 alike (DAOLib #106).
#
# It never reads the running game (CLAUDE.md's one architectural rule).
# Windows and its libraries are fine in it (the user, 2026-10-14): DirectXTex
# compresses the textures dao/core/DdsWrite.cpp writes (DAOLib #120).
# ---------------------------------------------------------------------------
add_library(
    daolib_core
    STATIC
    src/dao/core/AddIns.cpp
    src/dao/core/ArchiveCache.cpp
    src/dao/core/Area.cpp
    src/dao/core/CallSite.cpp
    src/dao/core/Character.cpp
    src/dao/core/Clip.cpp
    src/dao/core/Conversation.cpp
    src/dao/core/DaZip.cpp
    src/dao/core/Dds.cpp
    src/dao/core/DdsWrite.cpp
    src/dao/core/Erf.cpp
    src/dao/core/ErfWrite.cpp
    src/dao/core/Exceptions.cpp
    src/dao/core/Fingerprint.cpp
    src/dao/core/FunctionTable.cpp
    src/dao/core/GdaFile.cpp
    src/dao/core/Grid.cpp
    src/dao/core/Gff3.cpp
    src/dao/core/Gff3Write.cpp
    src/dao/core/Gff4.cpp
    src/dao/core/LogNames.cpp
    src/dao/core/GfxMovie.cpp
    src/dao/core/Inflate.cpp
    src/dao/core/Install.cpp
    src/dao/core/Level.cpp
    src/dao/core/Model.cpp
    src/dao/core/ModelWrite.cpp
    src/dao/core/LightProbes.cpp
    src/dao/core/M2da.cpp
    src/dao/core/MaterialDefinition.cpp
    src/dao/core/MaterialObject.cpp
    src/dao/core/Morph.cpp
    src/dao/core/OptionsMovie.cpp
    src/dao/core/Pe.cpp
    src/dao/core/Picture.cpp
    src/dao/core/RayCast.cpp
    src/dao/core/Rtti.cpp
    src/dao/core/SaveMeta.cpp
    src/dao/core/ScriptValue.cpp
    src/dao/core/StackText.cpp
    src/dao/core/TalkTable.cpp
)
dao_apply_warnings(daolib_core)
dao_release_symbols(daolib_core)
target_compile_features(daolib_core PUBLIC cxx_std_20)
target_include_directories(daolib_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
find_package(directxtex CONFIG REQUIRED)
target_link_libraries(daolib_core PRIVATE Microsoft::DirectXTex)
# <Windows.h>, which DirectXTex's headers bring, without its min and max macros.
target_compile_definitions(daolib_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_library(DAOLib::core ALIAS daolib_core)

if(DAO_CORE_ONLY)
    if(DAO_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
    return()
endif()

# The format and lint targets cover the top-level tree's own sources, so they
# are the including repository's to define when DAOLib is not the top level.
if(PROJECT_IS_TOP_LEVEL)
    include(Quality)
endif()

find_package(spdlog CONFIG REQUIRED)
find_path(DETOURS_INCLUDE_DIRS "detours/detours.h" REQUIRED)
find_library(DETOURS_LIBRARY detours REQUIRED)

# ---------------------------------------------------------------------------
# daolib -- the library plugins are written against: where the game's code
# and data are (Addresses.h, relocated if the exe is ever loaded off its
# preferred base), the exe fingerprint every address was read from, class
# names from the engine's RTTI, a detour wrapper, and a log per DLL.
#
# It links daolib_core, above, and adds what reads process memory, which is
# verified in play.
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
    src/dao/Data.cpp
    src/dao/Dialogue.cpp
    src/dao/Journal.cpp
    src/dao/Animation.cpp
    src/dao/ScriptCall.cpp
    src/dao/ObjectIds.cpp
    src/dao/Orders.cpp
    src/dao/Abilities.cpp
    src/dao/Attack.cpp
    src/dao/Damage.cpp
    src/dao/Vfx.cpp
    src/dao/CommandEvents.cpp
    src/dao/Equipment.cpp
    src/dao/Conjure.cpp
    src/dao/Strings.cpp
    src/dao/SubActions.cpp
    src/dao/Atmosphere.cpp
    src/dao/Lights.cpp
    src/dao/Framebuffer.cpp
    src/dao/Party.cpp
    src/dao/Portraits.cpp
    src/dao/Resources.cpp
    src/dao/Events.cpp
    src/dao/Frame.cpp
    src/dao/Camera.cpp
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
    src/dao/Screenshot.cpp
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
dao_release_symbols(daolib)
# The headers are C++20 (std::span among them), so whatever links daolib is.
target_compile_features(daolib PUBLIC cxx_std_20)
target_include_directories(daolib PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_include_directories(daolib SYSTEM PUBLIC ${DETOURS_INCLUDE_DIRS})
target_link_libraries(
    daolib
    PUBLIC
        daolib_core
        spdlog::spdlog
        ${DETOURS_LIBRARY}
        DAOAddressLibrary::daoaddr
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
# dinput8.dll -- the loader. Dropped beside DAOrigins.exe it is loaded in
# place of the system's dinput8.dll (the game imports one of its functions,
# which is forwarded to the real one), detours the exe's entry point, and
# from there loads each plugin, daolib\plugins\<name>\<name>.dll, on the game's main thread
# before any game code runs.
# ---------------------------------------------------------------------------
add_library(
    loader
    SHARED
    src/loader/Asi.cpp
    src/loader/CrashLog.cpp
    src/fix/ArchiveRead.cpp
    src/fix/LightFlicker.cpp
    src/fix/MouseRecords.cpp
    src/fix/SavedFileName.cpp
    src/perf/EngineHeap.cpp
    src/perf/AllProcessors.cpp
    src/perf/FastLoad.cpp
    src/perf/fastload/AddressSpace.cpp
    src/perf/fastload/AreaLists.cpp
    src/perf/fastload/AreaRooms.cpp
    src/perf/fastload/LoadingScreen.cpp
    src/loader/Instance.cpp
    src/loader/Loader.cpp
    src/loader/Proxy.cpp
)
dao_apply_warnings(loader)
dao_release_symbols(loader)
# At the top of the build tree, where the game folder's layout starts, when
# DAOLib is built inside another repository too.
set_target_properties(
    loader
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "dinput8"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
# dbghelp names the frames of a crash log; psapi gives the process's memory.
target_link_libraries(loader PRIVATE daolib dbghelp psapi)

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
            src/plugins/probe/ColourCurve.cpp
            src/plugins/probe/Conjure.cpp
            src/plugins/probe/Hold.cpp
            src/plugins/probe/Hud.cpp
            src/plugins/probe/Movies.cpp
            src/plugins/probe/Reads.cpp
            src/plugins/probe/Events.cpp
            src/plugins/probe/Loading.cpp
            src/plugins/probe/Probe.cpp
            src/plugins/probe/Recorder.cpp
            src/plugins/probe/ScriptChecks.cpp
            "${DAO_PROBE_TARGETS}"
    )
    dao_apply_warnings(probe)
    target_link_libraries(probe PRIVATE winmm)
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
