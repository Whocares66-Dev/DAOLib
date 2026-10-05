# One of a plugin's FILES copied beside it after a build (add_daolib_plugin,
# cmake/DAOLib.cmake), run as `cmake -D src=... -D dst=... -P` this file. A
# file another tool makes, a movie built outside CMake, may not be there on
# a fresh build: that warns and copies nothing rather than failing the build.
if(EXISTS "${src}")
    file(COPY "${src}" DESTINATION "${dst}")
else()
    message(
        WARNING
        "add_daolib_plugin: ${src} is not there; not copied beside the plugin"
    )
endif()
