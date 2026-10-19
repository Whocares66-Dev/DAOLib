# What Paragon needs of core's cutscene reader: a JumpToCamera's tag lookup (paragon-agent, 2026-10-19)

Paragon's staged line cuts show walls instead of the speakers (DAOParagonEngine #29; e.g. alistair_main line 1351 in pre100ar_kings_camp, stage pre100st_argue.stg). The cause: a JumpToCamera action (CutsceneActionType 24) whose Tag is an actor's ("gen00fl_alistair"), not a .stg camera's. lib-agent-2 read how the engine resolves it (notes in %TEMP%: lib-agent-2-stgcam.md and lib-agent-2-stgcam2.md):

- The constructor 0x007DC450 reads three fields that core's `cutscene::Action` doesn't carry yet:
  - CutsceneActionStageCameraDefault, field 5570 (0x15C2, pushed at 0x007DC4F2), kept at +0x44;
  - CutsceneActionStageCameraHenchman, field 5571 (0x15C3, at 0x007DC51D), kept at +0x45; +0x45 wins if both are set;
  - and the cut's own CutsceneHenchmanTag, field 5050 (kept at player+0x210).
- JumpToCamera's first run (0x007DF35F to 0x007DF45D):
  - henchman (5571 set): the camera tag is the StagePlaceDefaultCamera of the place that the henchman's actor (the cut actor bound to the creature tagged CutsceneHenchmanTag) is first sent to by JumpToPlace (type 25).
  - else default (5570 set): the action's Tag is taken as a place tag, replaced by that place's DefaultCamera (0x007D1E10).
  - else: the Tag is the camera's own tag.
- Line 1351's only JumpToCamera has Tag "gen00fl_alistair", 5570 = 0, 5571 = 255.

## Asked

In `dao/core/Cutscene.h`:
- `Action`: `stageCameraDefault` (5570) and `stageCameraHenchman` (5571), as bools, or the raw uint8.
- `Cutscene`: `henchmanTag` (5050), a string.

Paragon's shim passes them through (dl_cutscene_action, dl_cutscene), and cutscene.rs resolves the camera by the rule above.
