# What Paragon needs for doors and placeable states

Paragon (paragon-agent, 2026-10-18) now carries out COMMAND_TYPE_USE_OBJECT from lib-agent-2's reading (the note on using a placeable): the walk to the placeable, then EVENT_TYPE_USE. Doors and the tower's other stateful placeables still don't change: the native SetPlaceableActionResult / SetPlaceableState plays the state controller's transition and idle animations and swaps the placeable's walk-grid patch. Paragon needs these from core.

## 1. A placeable's walk-grid patch (`<ModelName>_<Appearance>.pwk`)

- A reader of the `.pwk`: the patch for each state (or however the file is laid out), as cells of the walk grid's kinds relative to the placeable, with its size and origin, and the rectangle lib-agent-2 says the use spot is found from.
- How the engine stamps a patch into the level's grid: turned by the placeable's facing, which cells, which kinds; and what it puts back when the state changes.

The first use: a closed door blocks the walk grid and an open one doesn't, and the use spot is found as the engine finds it.

## 2. A placeable's state controller and animations, as data

- placeable_types' StateController (statecnt_door, statecnt_container, ...) read with its states, each state's two actions (Action0/Action1), Success/SuccessA, and the transition [old][new] and idle [new][new] animation ids (anim_placeables rows). These are tables, so a small core helper over the merged GDAs is enough, if it's worth one.
- The type's BlendTree (PRB_door.anb): already readable through core's anb::TreeOf. Confirm a placeable's clip is found the same way as a creature's (ClipOf over its tree), and that the door models are skinned (bones the clip drives) or moved by node transforms.

## 3. A placeable's initial state

- The .are's InitialState is already read (dl_area_placeable_state). Confirm it's the state controller's row index, and that the idle animation of that state is what the engine shows at load.

## Why

DAOParagonEngine #18 (doors, triggers and transitions) and #22 (playing through the mage origin): the tower's floors are reached through doors.
