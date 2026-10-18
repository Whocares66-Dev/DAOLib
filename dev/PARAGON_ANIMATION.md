# What Paragon needs for animation's gaps

Paragon (paragon-agent, 2026-10-18) plays a creature's clips by animation id through its blend trees (`dl_anim_clip`, `dl_anim_clip_combat`), and moves weapons between their sheathed and drawn nodes when the combat state changes, with no clip (Paragon 2a782c3). The gaps, Paragon issue #33, in the order the mage origin shows them:

## 1. Drawing and sheathing (lib-agent-2: the exe)

- What makes a creature draw on entering combat and sheathe on leaving it: the engine queueing command type 39 (Sheathing, dev/script_commands/commands.md), from where, with which int 1 (its delay).
- Which animation type 39 plays: anim_base's rows for drawing and sheathing, chosen by what (the weapon style: two-handed, sword and shield, dual, bow, staff, none), and through which blend tree.
- When the weapon moves: the clip's `EquipR`/`EquipL`/`UnequipR`/`UnequipL` events (events.gda, dev/CHARACTERS.md), at their times in the clip; and what happens when there is no such clip (an instant move?).
- Whether the hands' grip (a held pose or an overlay) changes with the weapon drawn.

## 2. Core: an animation's events (lib-agent)

- Where a clip's events are stored (in the .ani, a .evt beside it, or the blend tree), their format, and a reader: each event's time (as a share of the clip, like the keys?), its type (events.gda's row) and its data (a sound, a VFX, the hand).
- The events the engine acts on, by events.gda's rows: weapon equip and unequip, footsteps and sounds, effects (VFX) attach, damage (the hit's moment in an attack), the projectile's release.

## 3. Faces (later; after sound, Paragon #30)

- The .fxe/.fxa (FaceFX) data a talking face plays, and how the engine drives the head's bones or morphs from it.
- The overlay tree (head and eyes): blinking and looking, as apr_base's OverlayTree names it.
