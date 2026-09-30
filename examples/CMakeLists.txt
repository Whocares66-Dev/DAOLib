# DAOLib's examples: one small plugin per part of the library, each short
# enough to read in a sitting and to copy from, and each run in the game
# as a check of that part (examples/README.md). They build like any
# plugin, with add_daolib_plugin, into <build>\daolib\plugins\, so
# tools\deploy.ps1 puts them in the game with the rest. The DLLs are named
# example_<part>.dll, and each logs to example_<part>.log.

add_daolib_plugin(example_camera SOURCES camera/Camera.cpp)
dao_apply_warnings(example_camera)

add_daolib_plugin(example_damage SOURCES damage/Damage.cpp)
dao_apply_warnings(example_damage)

add_daolib_plugin(example_saves SOURCES saves/Saves.cpp)
dao_apply_warnings(example_saves)

add_daolib_plugin(example_speed SOURCES speed/Speed.cpp)
dao_apply_warnings(example_speed)

add_daolib_plugin(example_world SOURCES world/World.cpp)
dao_apply_warnings(example_world)
