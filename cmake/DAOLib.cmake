# add_daolib_plugin: declares a plugin, a DLL the loader (version.dll) loads
# from bin_ship\daolib\plugins\ -- DAOLib's counterpart of CommonLibSSE's
# add_commonlibsse_plugin. DAOLib's CMakeLists includes this file, so a
# plugin's repository that brings DAOLib in as a subdirectory has the
# function afterwards:
#
#   add_subdirectory(extern/DAOLib EXCLUDE_FROM_ALL)
#   add_daolib_plugin(myplugin SOURCES src/Plugin.cpp src/Hooks.cpp)
#
# That repository's own vcpkg manifest lists DAOLib's dependencies (detours,
# spdlog), since a manifest build reads only the top-level one.
#
# The plugin is a MODULE, a DLL nothing links against, named <name>.dll with
# no prefix, linked to DAOLib::daolib, which makes it C++20. It is written to
# <build>\daolib\plugins\, the layout of the game's bin_ship folder, so a
# build tree holds a game folder's worth of files for a deploy script to
# copy. Warnings are the caller's to set: DAOLib's own plugins add
# dao_apply_warnings.
function(add_daolib_plugin name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "add_daolib_plugin(${name}): no SOURCES given")
    endif()
    add_library(${name} MODULE ${ARG_SOURCES})
    set_target_properties(
        ${name}
        PROPERTIES
            PREFIX ""
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/daolib/plugins"
    )
    target_link_libraries(${name} PRIVATE DAOLib::daolib)
endfunction()
