# add_daolib_plugin: declares a plugin, a DLL the loader (version.dll) loads
# from its own folder, bin_ship\daolib\plugins\<name>\ -- DAOLib's
# counterpart of CommonLibSSE's add_commonlibsse_plugin. DAOLib's
# CMakeLists includes this file, so a plugin's repository that brings
# DAOLib in as a subdirectory has the function afterwards:
#
#   add_subdirectory(extern/DAOLib EXCLUDE_FROM_ALL)
#   add_daolib_plugin(myplugin SOURCES src/Plugin.cpp src/Hooks.cpp
#                     FILES ui/myscreen.swf myplugin_en-us.tlk)
#
# That repository's own vcpkg manifest lists DAOLib's dependencies (detours,
# spdlog), since a manifest build reads only the top-level one.
#
# The plugin is a MODULE, a DLL nothing links against, named <name>.dll with
# no prefix, linked to DAOLib::daolib, which makes it C++20. It is written to
# <build>\daolib\plugins\<name>\, the layout of the game's bin_ship folder,
# so a build tree holds a game folder's worth of files for a deploy script
# to copy. FILES, if given, are copied beside it on every build (one not
# there yet, made by another tool, warns and is skipped): the
# plugin's own movies (served from there when the game has none by that
# name, dao/Gui.h), talk tables (dao/Strings.h) and data, so the whole
# plugin is that one folder (DAOLib #26). OVERRIDE files go to its
# override\ subfolder, which the game takes as a source of its own
# resources, as packages\core\override (DAOLib #43): textures, materials,
# models, M2DA_ tables. Warnings are the caller's to set:
# DAOLib's own plugins add dao_apply_warnings.

# dao_release_symbols: a release build of the target keeps its symbols, as a
# debug build does. For a DLL that is <name>.pdb beside it, where the crash
# logger looks to name our frames (dev/CRASHES.md); a player's crash log is
# from a release build, and names nothing of ours without one (DAOLib #55).
# /Z7 puts a static library's symbols in its objects, so they reach the PDB
# of whichever DLL links it. /DEBUG turns the linker's /OPT:REF and /OPT:ICF
# off, which a release build has on, so they are put back. Call it in the
# directory that declares the target, as add_daolib_plugin does.
function(dao_release_symbols target)
    if(NOT MSVC)
        return()
    endif()
    target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/Z7>)
    get_target_property(type ${target} TYPE)
    if(NOT type STREQUAL "STATIC_LIBRARY")
        target_link_options(
            ${target}
            PRIVATE "$<$<CONFIG:Release>:/DEBUG;/OPT:REF;/OPT:ICF>"
        )
        # The linker adds to a PDB that is there already and keeps what
        # earlier links wrote: twice the size after a few, and the path of a
        # temporary folder the build no longer uses, which is under the
        # user's name (seen by Eluvian's session 2026-10-10, DAOLib #58). A
        # link that is not incremental needs nothing of the old one, so it
        # starts from none; RelWithDebInfo is one too, in a repository that
        # builds with it. A debug link is incremental and keeps its PDB.
        add_custom_command(
            TARGET ${target}
            PRE_LINK
            COMMAND
                ${CMAKE_COMMAND} -E
                "$<IF:$<CONFIG:Release,RelWithDebInfo>,rm;-f;$<TARGET_PDB_FILE:${target}>,true>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
endfunction()

function(add_daolib_plugin name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;FILES;OVERRIDE")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "add_daolib_plugin(${name}): no SOURCES given")
    endif()
    add_library(${name} MODULE ${ARG_SOURCES})
    set(folder "${CMAKE_BINARY_DIR}/daolib/plugins/${name}")
    set_target_properties(
        ${name}
        PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${folder}"
    )
    target_link_libraries(${name} PRIVATE DAOLib::daolib)
    dao_release_symbols(${name})
    # The plugin's own files, copied beside it by a target that runs on
    # every build: a file another tool remade (a movie) is copied again
    # though nothing relinks, and one not there yet warns and is skipped.
    # The script is found beside this file, from whichever repository calls
    # the function.
    set(copies "")
    foreach(file IN LISTS ARG_OVERRIDE)
        cmake_path(
            ABSOLUTE_PATH file
            BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        list(
            APPEND copies
            COMMAND
            ${CMAKE_COMMAND}
            -D
            "src=${file}"
            -D
            "dst=${folder}/override"
            -P
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopyPluginFile.cmake"
        )
    endforeach()
    if(ARG_FILES)
        foreach(file IN LISTS ARG_FILES)
            cmake_path(
                ABSOLUTE_PATH file
                BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
            list(
                APPEND copies
                COMMAND
                ${CMAKE_COMMAND}
                -D
                "src=${file}"
                -D
                "dst=${folder}"
                -P
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopyPluginFile.cmake"
            )
        endforeach()
    endif()
    if(copies)
        add_custom_target(${name}_files ALL ${copies} VERBATIM)
        add_dependencies(${name} ${name}_files)
    endif()
endfunction()
