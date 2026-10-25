# DAOLib's examples: one small plugin per part of the library, each short
# enough to read in a sitting and to copy from, and each run in the game
# as a check of that part (examples/README.md). They build like any
# plugin, with add_daolib_plugin, into <build>\daolib\plugins\<name>\, so
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

add_daolib_plugin(example_clock SOURCES clock/Clock.cpp)
dao_apply_warnings(example_clock)

add_daolib_plugin(example_world SOURCES world/World.cpp)
dao_apply_warnings(example_world)

add_daolib_plugin(example_dialogue SOURCES dialogue/Dialogue.cpp)
dao_apply_warnings(example_dialogue)

add_daolib_plugin(example_preview SOURCES preview/Preview.cpp)
dao_apply_warnings(example_preview)

add_daolib_plugin(example_commands SOURCES commands/Commands.cpp)
dao_apply_warnings(example_commands)

add_daolib_plugin(example_audio SOURCES audio/Audio.cpp)
dao_apply_warnings(example_audio)

add_daolib_plugin(example_cooldown SOURCES cooldown/Cooldown.cpp)
dao_apply_warnings(example_cooldown)

add_daolib_plugin(example_resume SOURCES resume/Resume.cpp)
dao_apply_warnings(example_resume)

add_daolib_plugin(example_input SOURCES input/Input.cpp)
dao_apply_warnings(example_input)

add_daolib_plugin(example_journal SOURCES journal/Journal.cpp)
dao_apply_warnings(example_journal)

add_daolib_plugin(example_animation SOURCES animation/Animation.cpp)
dao_apply_warnings(example_animation)
add_daolib_plugin(example_hold SOURCES hold/Hold.cpp)
dao_apply_warnings(example_hold)

add_daolib_plugin(example_options SOURCES options/Options.cpp)
dao_apply_warnings(example_options)

add_daolib_plugin(example_party SOURCES party/Party.cpp)
dao_apply_warnings(example_party)

add_daolib_plugin(example_messages SOURCES messages/Messages.cpp)
dao_apply_warnings(example_messages)

add_daolib_plugin(example_moments SOURCES moments/Moments.cpp)
dao_apply_warnings(example_moments)

add_daolib_plugin(example_levelup SOURCES levelup/LevelUp.cpp)
dao_apply_warnings(example_levelup)
add_daolib_plugin(example_worldmap SOURCES worldmap/WorldMap.cpp)
dao_apply_warnings(example_worldmap)
add_daolib_plugin(example_loot SOURCES loot/Loot.cpp)
dao_apply_warnings(example_loot)
add_daolib_plugin(example_partypicker SOURCES partypicker/PartyPicker.cpp)
dao_apply_warnings(example_partypicker)
add_daolib_plugin(example_store SOURCES store/Store.cpp)
dao_apply_warnings(example_store)
add_daolib_plugin(example_crafting SOURCES crafting/Crafting.cpp)
dao_apply_warnings(example_crafting)
add_daolib_plugin(example_items SOURCES items/Items.cpp)
dao_apply_warnings(example_items)
add_daolib_plugin(example_tactics SOURCES tactics/Tactics.cpp)
dao_apply_warnings(example_tactics)

add_daolib_plugin(example_attack SOURCES attack/Attack.cpp)
dao_apply_warnings(example_attack)

add_daolib_plugin(example_atmosphere SOURCES atmosphere/Atmosphere.cpp)
dao_apply_warnings(example_atmosphere)

add_daolib_plugin(example_lights SOURCES lights/Lights.cpp)
dao_apply_warnings(example_lights)

add_daolib_plugin(example_emitters SOURCES emitters/Emitters.cpp)
dao_apply_warnings(example_emitters)

add_daolib_plugin(example_textures SOURCES textures/Textures.cpp)
dao_apply_warnings(example_textures)

add_daolib_plugin(
    example_resources
    SOURCES resources/Resources.cpp
    FILES resources/extra
    OVERRIDE resources/example_resources_always.dds
)
dao_apply_warnings(example_resources)

add_daolib_plugin(example_objectids SOURCES objectids/ObjectIds.cpp)
dao_apply_warnings(example_objectids)

add_daolib_plugin(example_patch SOURCES patch/Patch.cpp)
dao_apply_warnings(example_patch)
