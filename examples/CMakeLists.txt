# DAOLib's examples: one small plugin per part of the library, each short
# enough to read in a sitting and to copy from, and each run in the game
# as a check of that part (examples/README.md). They build like any
# plugin, with add_daolib_plugin, into <build>\daolib\plugins\, so
# tools\deploy.ps1 puts them in the game with the rest. The DLLs are named
# example_<part>.dll, and each logs to example_<part>.log.

add_daolib_plugin(example_saves SOURCES saves/Saves.cpp)
dao_apply_warnings(example_saves)
