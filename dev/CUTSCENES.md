# Cutscenes

How the engine plays a cutscene, read in the exe; the files themselves are `dao/core/Cutscene.h`'s (DAOLib #170).

## As the engine plays them

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and `script.ldf`, for DAOParagonEngine's mage origin (#22). Builds on `src/dao/Dialogue.cpp`'s note (LoadCutscene `0x0066D230`, PlayCutscene `0x00651240`, read 2026-10-01 and seen in play: loading is playing, `SkipCutscene` returns the game to exploring with the hero where she stood). "Read" is what the code shows; "inferred" is marked.

### LoadCutscene (647, `0x0066D230`)

- Declared `void LoadCutscene(resource rCutscene, object oPlayer, int bPlayImmediately, string[] aTargetTagsToReplace, object[] aReplacementObjects, int bActorsHaveWeapons)`. Pops in that order: resource, object, int, array, array, int. Read.
- It finds the resource by its full name (`Resources::Find(name, 3)`; a name without `.cut` finds nothing, seen in play 2026-10-01) and checks its header (`0x00A38EA0`). Read.
- If the application's byte +0x208 is set (inferred: the "skip cutscenes" option), it does not play: it reads the file's `CutsceneEndScript` (field 5001) and runs that script at once with `ExecuteScript(script, OBJECT_INVALID, 0, -1)`. Read.
- Otherwise it builds the cutscene (`0x00692150`: a 0x250-byte player made by `0x007E26E0`, wrapped in `GameModeCutsceneData`) and, if that loaded (its state +0x94 is not 6), sets the game mode to 5, cutscene, at once (`GameModeController::SetGameMode(5, 1, data)`). Read.
- The player fails to load (state 6), and nothing happens, when the file's `CutsceneAreaRequired` (5019) is set and there is no current area (`0x007E1910`). Read; matches play 2026-10-01: a cutscene of an area not loaded did nothing.
- `oPlayer` and `bPlayImmediately` are popped and not used. `bActorsHaveWeapons` is popped and not used either: each replacement is made with "weapons" 1. Read.
- Replacements: for each index i, `0x007E3390(aTargetTagsToReplace[i], aReplacementObjects[i], 1)` finds the cutscene's actor with that tag (`0x007D0510`) and hands the replacement object to it (`0x007E3330` → `0x007E30B0`). Read. `0x007E0D20` makes an actor object per actor in the file (0x560 bytes, `0x007DFC60`). Corrected 2026-10-18 by lib-agent-2 (below, "The actors"): those are the cutscene's own objects only for an unmapped actor. A mapped actor, one whose `CutsceneActorMappingTag` (5216) a replacement or the area resolves, drives the area's creature's model, and at the end of a script's cutscene the creature is put back where it stood at binding. `script.ldf`'s "the objects will be used to take the appearance only of the creatures" does not say that.

### PlayCutscene (648, `0x00651240`)

- Pops its object and does nothing: loading is playing (read 2026-10-01).

### Entering cutscene mode (`GameModeCutscene` slot 7, `0x00692980`)

- Takes the pending cutscene (+0x50 → +0x4C), saves the camera (the mode's +0x70..+0xA0 into its own +0x0..+0x1C) and starts the player (`0x007CDC90`, `0x006C4AF0`, `0x006C2890(0, 1)`, `0x007CD0D0`). Read.
- Calls the application's `0x005D5540(1)`: it stops timers (`0x00A35DE0`, `0x00A35A10`) and tells the graphics (slot 0x6C) and an object (slot 0x60) a cutscene began; the world timer at +0xD4 is saved on leaving. Inferred: the game's clock is held during a cutscene. Read in outline.
- Calls `0x006A0D40` on each party member (`0x006300B0`, party at `0x005D57C0()`). Not read what it does; inferred: stops their movement or actions.
- Opens the cutscene GUI (`*(app+0x158)` slot 0x88) and sets the input mode "GameModeCutscene". Read.
- Whether the area's creatures that are not actors are hidden is not read; the file's `CutsceneShowAreaDynamics` (5009) is a likely switch (inferred).

### The file (`0x007E1910`, the player's load)

- Fields read from the `.cut` (GFF4, `dev/gff4_fields.csv`): `CutsceneAnimatic` (5008), `CutsceneAreaRequired` (5019), `CutsceneStageResRef` (5017), `CutsceneRunTime` (5000, seconds), `CutsceneFPS` (5016, default 30), `CutsceneEndScript` (5001), `CutsceneBlendtree` (5007), the actors' list (field 0x1450) and more (fields 0x1393..0x139D). Read.

### The end, and the end script (`GameModeCutscene` slot 8, `0x00692450`)

- When cutscene mode is left, slot 8 restores the camera, takes the player's end script (`0x00691CF0`, the file's `CutsceneEndScript`), deletes the player, and, if the script is not empty, runs it at once: `ExecuteScript(script, OBJECT_INVALID, 0, -1)`. Read. So the end script runs with no OBJECT_SELF of the cutscene's choosing (OBJECT_INVALID passed; what the VM falls back to is not read) and no event; the generic one, `gen00cs_cutscene_end`, calls `CS_CutsceneEnd()`, which reads module variables. There is no `EVENT_TYPE_CUTSCENE_END` in `script.ldf`.
- Then `0x005D5540(0)` restarts the timers, the cutscene GUI is closed (slot 0x8C), the input mode restored, and `GameMode` slot 8 runs. Read. The mode the game returns to is the controller's (not read); in play it was exploring (2026-10-01).

### Skipping (`GameModeCutscene::HandleMessage_SkipCutsceneMessage`, `0x00691D60`)

- Sends a telemetry line once, then, unless the player is already ending (`0x007CF2E0`), sets the player's skip flag (+0x1B4) and calls `0x007CD0F0`. Read.
- Corrected 2026-10-18 by lib-agent-2: a skip does not end the cutscene at once (the inference made here before). It first jumps to the next skip marker (`SkipMarker`, type 47, `ASMK`; `0x007CF2E0`, the markers' starts at player +0x190): the actions are reset and the voices stopped. Only with no marker left does the cutscene end, the mode is left and the end script runs as at a natural end (slot 8). The area's creatures that played actors are put back where they stood at binding, which is what was seen in play 2026-10-01: the hero where she had stood.

### Classes

- `GameModeCutscene` (vftable `0x00B0E76C`), `GameModeCutsceneData` (`0x00B0E6BC`, holding the player), `GUI::GUICutscene`, and the actors' actions `EC::Cutscene::Actor::Action*` (PlayAnimation, MoveSpline, JumpToPlace, ChangeVisibility, DrawWeapon, CameraFOV, PlayMovie, Light*, FrameBufferEffect and kin, vftables `0x00B231C8`..`0x00B23630`). Read.

## The player, frame by frame

Read 2026-10-18 by lib-agent-2 (with subagents) in the GOG exe and the game's files, for DAOParagonEngine, which is to play the mage origin's cutscenes (`bhmcs_intro`, `bhm500cs_harrowing`, `jowan_attacks`). Not seen in play. Checked against `jowan_attacks.cut` and its `.cub`, `bhm500cs_harrowing.cut`, `bhmcs_intro.cut`, `bhm100_jowan.dlg`, `bhm100ar_tower_level_1.are` and `jowan_attack.stg`; the key claims were checked again in the disassembly. "Checked" is that; "inferred" is marked. Core samples the curves and places the tracks as below (`dao/core/Cutscene.h`'s `ValueAt`, `PositionAt`, `OrientationAt`, `FrameTime`; DAOLib #184), and its tests hold the 57 actors' final places.

### The clock and the actions

- **The clock** (the player's update `0x007E3D30`, state 3): the accumulator +0x10 grows by dt, the first two updates by 0. Time is quantised to the cut's frames: `frame = (int)(acc·FPS + 0.5)`, `t = frame/FPS` (`CutsceneFPS`, 100 or 30). At the last frame (+0x14) the ended flag +0x18 is set.
- **A step** (`0x007DD4C0`) gathers every action's start and stop in (prev, t] and advances each actor through each of those times in turn (`0x007DA1E0`). So an action whose start equals its stop fires once, exactly then.
- **An actor's actions** run in file order, grouped by category (5304). An action is live while `start − 1e-4 ≤ t ≤ stop + 1e-4`, and gets `Update(dt, t − start)` (slot 1). Between categories the animations are composed (at 20) and the camera re-aimed at its target (at 50) (`0x007D8610`).
- **The action types** (the factory `0x007DDAA0`):
  - 4 PlayAnimation, 11 MakeActiveCamera, 13 CameraFOV;
  - 17 MoveSpline, 18 Rotate, 19 FrameBufferEffect, 20 Shake;
  - 23 SpeakLine, 24 JumpToCamera, 25 JumpToPlace, 27 LinkToActor, 28 Headtracking;
  - 31 Sound, 32 ChangeVisibility, 33/34/35 a light's radius, intensity and colour;
  - 36 TogglePhysics, 38 SnapToWalkmesh, 39 OrientationFollowsCurve;
  - 41 DrawWeapon, 42 PlayMovie, 43 AutoAdjustHeight, 44 AnimateFace;
  - 45 SetGore, 46 SetFog, 47 SkipMarker (`ASMK`).
- **The base fields** (`0x007DA530`): start (5301), stop (5302), type (5300), category (5304), curves.

### A curve (`0x007D30B0` → `0x007D2440`, checked)

A curve is evaluated at the cut's own time, `start + tLocal`; the vertices' times are absolute.

- With no vertices it is the base (5350). At or before the first vertex it is the first value; at or after the last, the last. Within 1e-4 of an inner vertex (the second on) it is that vertex's value.
- Between vertices k and k+1, the transition's type (5380; `0x007D25C3`–`0x007D25D7`):
  - **1, linear:** `v_k + (v_k+1 − v_k)·(T − t_k)/(t_k+1 − t_k)`.
  - **2, step:** `v_k`. Common: 145 of the 308 transitions in `bhm500cs_harrowing`.
  - **0, a 2-D cubic Bézier** through (t_k, v_k), the controls 5381 and 5382 as absolute (time, value), and (t_k+1, v_k+1). s is found by bisection on [0, 1], at most 20 steps, to |Bx(s) − T| ≤ 1e-5 (`0x007CCDF0`). The value is `(1−s)³v_k + 3(1−s)²s·c1 + 3(1−s)s²·c2 + s³v_k+1` (`0x007CCEF0`).
  - Any other type gives the base.

### A clip's travel: the cut's own GAD, not the clip's root channel (read 2026-10-18 by lib-agent-2; checked unless said)

- **The clips are in place.** `mh.dg_f_5p.ani` (a walk forward of 5.77 s), `cs_jowan.bloodmage.ani` and `mov.walkf.ani` have `god_translation` all zero and a `root_translation` with x = 0, y = −0.007 and z between −0.21 and 0.005: a bob or crouch, no travel. No other bone of either moves far, beyond the eye-target and spawn nodes. So a clip's own root channel has no travel to keep or drop.
- **The travel is stored in the cut, per animation action,** in fields the AANI loader reads (`0x007DAEE3`–`0x007DB05E`):

  | Field | Action offset | What it is |
  |---|---|---|
  | 5405 `PlayGad` | +0x80 | apply it |
  | 5408 `GadKeysPos` | +0xB4/+0xB8 | floats in fours, `(t, x, y, z)` |
  | 5409 `GadKeysOri` | +0xBC/+0xC0 | floats in fives, `(t, x, y, z, w)` |
  | 5410 `BlendGad` | +0x81 | |
  | 5411 `ExtendGad` | +0x82 | |
  | 5413 `ApplyFutureGads` | +0x83 | |
  | 5412 `LinkToMoveDist` | +0xC4/+0xC8 | |

  With `PlayGad` set, `LinkToMovement` (+0x6C) is zeroed (`0x007DB058`–`0x007DB072`). The keys are the clip's root travel, cumulative from the clip's start, in the stage frame with the actor's facing already baked in. For `bhm100cr_door_templar_1`'s `mh.dg_f_5p__1` the last key (−0.452, 3.326) is 3.36 m along the actor's world heading at t = 0 (O.q's 79.6° plus the Rotate track's −72.5°, so 7.1°: (−0.41, 3.33)).
- **The reader of the keys** (`0x00A38DE0` for positions, `0x00A38D20` for orientations; read in the disassembly 2026-10-18 for core): at or before the first key's time it is the first key, after the last it is the last, between two a line weighted by the times (`0x00A38C60`). How two orientations are blended was not read; core normalises a line between them, and the keys come thirty to a second.
- **The order within a step:**
  1. **Category 0, the tracks:** MoveSpline and Rotate (every 17/18 in `jowan_attacks` is category 0) set the actor's place and orientation **absolutely** through `0x007D0660` and `0x007CF6B0`.
  2. **Category 20, each live AANI with `PlayGad`** (update `0x007DB400` → `0x007D92E0`). The keys are evaluated at the action's local time. The position delta is rotated by the turns of the actor's earlier extended GADs (`0x007D3C80` → `0x007D3A90`, identity composed with the `+0x548` entries' orientations). It is also rotated by the actor's own orientation, but only for the actor that is the player's `+0x22C`, the one named by the string at player `+0x210` (inferred: `CutsceneHenchmanTag`).
     - **Without `BlendGad`:** `local = current local (the track's) + delta` (`0x007CF640` + `0x007D0660`) and `ori = ori ⊗ gadOri` (`0x004CB640` → `0x007CF6B0`).
     - **With `BlendGad`:** the pair goes onto the actor's blend list `+0x538` (`0x007D8180`) and is resolved by weight after the category (`0x007D8240`, not read in detail).
  3. **After category 20** (`0x007D8610` → `0x007D84A0`), every entry of the actor's extended list `+0x548` is added the same way, position plus orientation, each step.
- **When a clip ends:** with `ExtendGad`, once its local time passes `(stop − start) − 1e-5`, the last key's position and orientation go onto `+0x548` (`0x007D9450` → `0x007D81E0`). The time stored is −1 without `BlendGad` (added directly each step) or the time with it (blended). The entry carries `ApplyFutureGads`, whose own use was not read.
- **So the engine's answer:**
  ```
  actorLocal(t) = trackLocal(t)                                  // O.p + R(O.q)·c(t), the MoveSpline
                + Σ live clips   R(extOri)·gadPos_i(tLocal_i)
                + Σ ended clips with ExtendGad   gadPos_j(end)
  world = S.p + R(S.q)·actorLocal        (orientation likewise: track ⊗ gadOri_i ⊗ extended oris)
  ```
  Core's `PlacementAt` (DAOLib #184) is this, with `BlendGad` clips and the henchman's turn left out; `TravelAt` is one clip's keys at a time. The order of the ended clips among themselves is the file's, which is inferred.
- **Notes:**
  - **`FinalPos` (5221) excludes the GADs** (checked for templar_1: `O.p + R(O.q)·track(end)` = (−10.88, 34.73) = `FinalPos`, while its three GADs sum to metres). A line cut's end puts creatures at `S.p + R(S.q)·FinalPos`, so a creature that a GAD has moved jumps back at the end (inferred, not seen). Core's check of the tracks against `FinalPos` therefore uses `PositionAt` and `OrientationAt`, the tracks alone.
  - **The GAD moves the model's place, not the object:** `0x007D0660` ends in `0x007CF450`.
  - An actor with no MoveSpline would gather the delta every step, since nothing resets its place (inferred). Every mapped actor in `jowan_attacks` has a track.
  - Without the GADs, `jowan_attacks`' templars stay on their tracks, which move about 0.65 m where the walk carries them about 3.3 m. That fits the empty wall.
  - `cs_duncan.hn_frwll01__1` (OWNER, `BlendGad`) has a last key of (78.7, −30.0, −3.0), far too large for a real move. The blend path wasn't read far enough to say what it does with it.

### The stage, position and orientation (checked)

Quaternions are (x, y, z, w), `v' = q v q*`, the product Hamilton's (`0x004CB640`). Forward is +Y.

- **Position** (MoveSpline, type 17, `0x007D32F0` → `0x007D0660`): `world = S.p + R(S.q)·(O.p + R(O.q)·c)`. S is the stage (player +0xA0, +0xB0), O the actor's `CutsceneActorOriginPos` (5214) and `OriginOri` (5215), c the three curves (metres).
- **Orientation** (Rotate, type 18, `0x007D3460` → `0x007CF6B0`): `world = S.q ⊗ O.q ⊗ E`, with `E = Rz(c0) ⊗ Rx(c1) ⊗ Ry(c2)` (`0x004EE200`), each curve in degrees: yaw about z, pitch about x, roll about y.
- **Check:** for all 57 track-driven actors in the three files, the tracks at `CutsceneRunTime` give `FinalPos` (5221) and `FinalOri` (5222) within 1e-4, cameras with pitch and roll among them.
- **No stage** (a script's `LoadCutscene`): the player's constructor sets S to the identity (`0x007E27A7`), and only a conversation's line sets it. **So a script cutscene is in world coordinates**, as are the three files above (none has `CutsceneStageResRef`, 5017).
- **A stage** (`0x007D2AA0`(name), from a line's `ConversationStageName`, 12100), only when the cut names a `.stg` in 5017:
  - The area's `StageList` entry with that `Tag` (the `.are`'s list, loaded at `0x00682F12`–`0x006831DE`), the nearest to the player's creature in XY if there are several.
  - S.p is its position, and **S.q its yaw alone**, `Rz(atan2(−f.x, f.y))` of its forward.
  - The `.stg` is loaded into player +0xC4.
- **`StageAtCurrentLocation`** (12104; `0x007CD030`, `0x007CD040`): S.p is the speaker's position and S's heading points from the speaker at the player's creature.
- **`JumpToPlace`** (type 25, `0x007DC720`): the `.stg`'s `StagePlaceList` place by tag; with `CutsceneActionStagePlaceLookAt` (5580) the actor faces the look-at place. It is set to `S.p + R(S.q)·place.pos` (snapped to the walk mesh, `0x007DA4F0`) facing `S.q ⊗ place.ori`. In staged cuts the tracks are empty and the pose comes from here, and `FinalPos` is the stage-local place.

### The actors: binding and the end (checked unless said)

- **A mapped actor** (`CutsceneActorMappingTag` 5216, resolved through the conversation's resolver or `LoadCutscene`'s replacements; `HENCHMAN` cycles) **drives the area's creature's model** (actor +0x1C0; `0x007CF450` sets the model's place, not the object's +0x120). So the actors are the cutscene's own objects only when unmapped.
- **At binding** (`0x007F2460`; a line cut's through the conversation, `0x007F1E90`, not read): SetActive(true) and Show(true), the state saved (`0x006952A0`); for a creature, `EVENT_TYPE_CREATURE_ENTERS_DIALOGUE` (86) to the module and `ClearAllCommands(1, 0)`. The release (`0x007F2800`) restores the state.
- **An unmapped actor**, or a tag not found (`0x007E3862`): a bare model from `ModelResRef`, or one assembled from `CreatureResRef`'s appearance and items (`0x006A4F00`). **No game creature is made**: no object, no AI.
- **At the end** (the player's teardown `0x007E10A0`), from the bind record (0x4C bytes at player +0xEC):
  - **A script cutscene** (no conversation, put back = 1): each creature goes **back to its place and facing at binding**.
  - **A line cut** (put back = 0, except the player's creature when `StageAtCurrentLocation`): a creature not moved since binding goes to `S.p + R(S.q)·FinalPos`, facing `R(S.q ⊗ FinalOri)·(0, 1, 0)` flattened (`0x007E1396`–`0x007E14CA`); one a script moved stays. So `jowan_attacks`' creatures end at their world `FinalPos`.

### Animations (AANI, type 4; `0x007DAD20`, update `0x007DB400`; checked unless said)

- **The clip comes from the cut's own tree,** `CutsceneBlendtree` (5007, e.g. `jowan_attacks.cub`, an ANBT tree like an `.anb`), not the actor's trees. The action's name (5400) is a node of it (`mh_c.tw_die_v1__1` → `mh_c.tw_die_v1.ani`). The action's own `BlendtreeName` (5401) is never read. The clip's `.evt` events fire as its time passes them.
- **Time:** `tc = min(tLocal, (stop − start) − 0.001)`, `clipTime = speed (5402) · tc + startOffset (5403)` seconds; a non-zero `LinkToMovement` (5407) uses the distance table (5412) instead.
- **Weight:** `clamp(curve0(start + tLocal) / 100, 0, 1)` every frame (`0x007DB450`). **There is no automatic cross-fade:** blends are authored as overlapping actions with ramps (Jowan: `bloodmage__4` 100 → 0 over 6.22–6.567 as `bloodmage__2` goes 0 → 100).
- **The pose fills the rest:** the actor's pose clip (`pose_<CutsceneActorPose 5211>`) gets `max(0, 1 − Σ weights)` (`0x007D62A3`). A clip outside its action's time is removed, so the pose takes over. Additive and override clips (e.g. `mh_o.eyelids`) aren't counted or removed. A clip past its length wraps (inferred).

### The camera (checked unless said)

- **`MakeActiveCamera`** (type 11, `0x007D11E0`): the active camera is the actor with id 5610, a hard cut at once.
- **A camera actor's tracks** are types 17 and 18 as above. It looks along its local +Y, +Z up: yaw c0, pitch c1 (up positive), roll c2.
- **The field of view** (`CameraFOV`, type 13, `0x007D3500`) is curve 0 in degrees. **It is not vertical:** the scene camera is given `cutFOV / aspect` (`0x007CDC90`–`0x007CDCFB`), so the vertical field is the cut's value divided by the screen's aspect (45 → 25.3° at 16:9). A conversation's camera does the same with 40 (`0x0068D74E`).
- **Near and far** aren't set.
- **How it drives the scene camera:**
  - Attaching (`0x006C4AF0`) installs camera behaviour 4 with no collision.
  - Each step the player copies the active actor's position, orientation and FOV (`0x007CD050`).
  - Each frame in state 3 the behaviour (`0x006C27B0`) writes them straight into the scene camera, unsmoothed.
  - The setters `0x006C45F0`, `0x006C4EC0` and `0x006C4B20` only save and restore at the mode's entry and exit.
- **A camera target** (5209): at category 50 the camera is re-aimed at that actor each step, yaw and pitch, its roll kept.
- **`JumpToCamera`** (type 24, `0x007DF2F0`): a `.stg` camera by tag (FOV 11003, LookingFrom 11006, LookingAt 11007–11009), placed by the stage's transform, its FOV through the same divide.
- **Shake** (type 20) multiplies an Euler noise onto the camera (not read in detail).
- **Depth of field** is the `FrameBufferEffect` (type 19) `da_dof`, not a camera property.

### The dialogue camera on a line with no cut (`0x0068D4B0`; read 2026-10-18 by lib-agent-2; checked unless said)

A line with no cut gets this camera; a line with one gets the cut's own (above). `dev/CAMERA.md` has the camera's behaviours it sets.

- **Who calls it:**
  - `ShowLine` (`0x0068E250`) at `0x0068E700`, on the no-cut branch, after the facing (`0x0069EAA0`).
  - Dialogue mode's per-frame update (`0x0068DDD0`) at `0x0068E0D1`, **every frame** while the current line has no cut (dialog `+0x20C` = 0) and `+0x2BC` is clear.
  - Dialogue mode's entry (`0x00691576`), where it does nothing: the reset at `0x0068F420` has just set `+0x16C` to −1.
- **Its inputs:**
  - **A, the subject, is the line's speaker:** dialog `+0x16C`, set by `ShowLine` from its first argument (`0x0068E3E9`). It returns at once if `+0x16C` is −1, the camera (`+0x58`) is null, or the object is gone.
  - **B, the other party, is not the line's listener.** `0x0068D270` returns the **controlled creature** (`[[app+0xD0]+0x5C]`, `0x005D5CC0`). If that creature is the conversation's OWNER (dialog `+0x170` = conversation `+0x28`, copied at `0x006914AF`), it returns the creature whose id is in the party's `+0x1F8` instead (`0x0068CFA0`; what `+0x1F8` holds was not read).
  - **A's eyes** are `off = R(qA)·eyesLocal`. `qA = Euler(ToAngle(fA), Elevation(fA), 0)` (`0x004EE200`) is A's facing (`+0x130`), normalised. `eyesLocal` (CCCreature `0x006A1BF0`) is the midpoint of the bones `eye_Left`/`eye_Right`, or else `LeftEye`/`RightEye`. Failing both it is the `Head` bone, and with no model (0, 0, 1.75). A that isn't a creature (cast slot 4) gets `off = (0, 0, 1)`, not rotated.
- **The placement** (constants at `0x00B0E1D0`–`0x00B0E1F8`, `0x00AF91D0`, `0x00AF70C8`):
  ```
  E   = A.pos + off                                  // A's eyes, world
  d   = (B.pos - A.pos - off) with z = 0             // flat, from A's eyes to B's feet
  v   = 0.9 * normalize(d)        if A is a creature  // 0.9 at 0x00AF91D0
  v   = 0.9 * d                   otherwise           // not normalised: 90% of the way to B
  v'  = Rz(+15°) v                                   // cos 15 0x00B0E1E8, sin 15 0x00B0E1E0; counter-clockwise seen from +z
  C   = E + v'                                       // camera position, at A's eye height
  T   = off - (0,0,0.06) + v' - Rz(+2°) v'          // look-at offset from A.pos (0.06 at 0x00B0E1F0;
                                                     //  1-cos2° 0x00B0E1D8 as a static, sin2° 0x00B0E1D0)
  SceneCamera::SetPosition(C, 0.0, 0)                // 0x006C45F0, seconds 0 = stored at once (0x006C4782)
  SetLookAt(A.id, T, 0)                              // 0x006C4A00: look-at behaviour (type 3), target A.pos(+0x120) + T
  SetFov(40 / aspect, 0.0, 0)                        // 0x006C4B20; 40 at 0x00AF70C8, aspect from the renderer
  ```
- **What the shot is** (geometry checked; the screen-side reading inferred): a single close-up of the speaker, not an over-the-shoulder shot. The camera is 0.9 m from A's eyes, swung 15° off the line from A to B, at A's eye height. It looks back at A's eyes from 6 cm below them (about 3.8° down). The aim is turned 2° off the eyes, so the eyes sit about 2° off centre. For a placeable speaker the camera is 90% of the way to B, 1 m above A's base.
- **The look-at behaviour** (`0x006C3010`) aims every frame at the object's current `+0x120` plus T: yaw and elevation, no roll. If the object is gone, it aims at the absolute point at `+0x20`.
- **Vertical field:** 40/aspect degrees (22.5° at 16:9), the same divide as a cut's FOV.
- **The controlled creature is hidden in a no-cut line:** every frame the update calls `0x00695060` on B with alpha 0.0, flag 0 (`0x0068E0C2`). That sets the object's `+0x190`, the fade target `dev/PLACEABLES.md` reads, and `0x00695080` gives the model alpha `+0x198` × `+0x190`. On a line with a cut it gives 1.0 (`0x0068DE58`).
- **The camera position is also copied** to `[area +0x658]+0x550` (`0x0068D787`; the area found through the controlled creature's `+0x254`, `0x005D6B30`). Inferred: a listener or fade point.
- **Cut or ease:** nothing is eased, the position, the FOV and the look-at are all set with 0 seconds. Within a line the camera is recomputed every frame from A's and B's current places and A's facing, so it follows them rigidly. Between lines it is a hard cut when the speaker changes. At a line's start `ShowLine` first clears the look-at (`SetLookAt(−1, 0, 0)`, `0x0068E2C9`); if the previous line had a cut, it freezes the camera where that cut left it and deletes its player (`0x0068E3F8`–`0x0068E43D`).
- **Edge cases:**
  - **Speaker = the controlled creature** (inferred from the formula, not seen): d becomes −off's flat part, so the camera ends up 0.9 m roughly behind the speaker's head, and the speaker is alpha 0. Whether `ShowLine` is ever called with the player as a no-cut speaker was not read.
  - **OWNER = the controlled creature:** B is the party's `+0x1F8` creature, as above.
  - **A placeable speaker:** the `(0, 0, 1)` eyes and the unnormalised 0.9·d, as above.
  - **Ambient (bark) lines leave the camera alone** (`dev/PLOTS.md`): an ambient starting line never enters dialogue mode (`0x007F231E`), so `ShowLine` and this function never run. The text is a floaty.
  - **After dialogue** the camera saved at the mode's entry (dialog `+0x180`..`+0x1A0`, `0x0069150B`–`0x0069155C`) is put back at once.

### The cutscene screen and the conversation screen's letterbox (read 2026-10-18 by lib-agent-2; checked unless said)

- **The movies:**
  - **Cutscene mode:** `GUI::GUICutscene` (vtable `0x00B1A8A8`, constructor near `0x00733500`) loads the scene `Cutscene` (string `0x00B0E834`), i.e. **`cutscene.gfx`**, on layer 5. The copy the game takes is patch003.erf's (29,094 bytes).
  - **Dialogue mode:** `GUI::GUIConversation` (`0x00B1A950`) loads **`conversation.gfx`** (scene `Conversation`, file name `conversation`, `0x00B0E4A4`/`0x00B0E0C8`), also layer 5.
- **The letterbox, `cutscene.gfx`** (authored stage 1024 × 768):
  - `LetterboxTop_mc` sits at (0, 0) and `LetterboxBottom_mc` at (0, 668), both black-bar shapes 1024 × 100 at authoring (`Cutscene.StartScene`).
  - At run time each bar is `_width = Stage.width`, `_height = (Stage.height − Stage.width / 1.7777…) / 2`, the top one top-aligned and the bottom one bottom-aligned. `GUISystem.AspectRatio` = 16/9 is never changed.
  - **So the bars crop the screen to 16:9:** each is `(1 − (W/H) / (16/9)) / 2` of the screen's height. That is 0 at 16:9, 5% at 16:10 (60 px of 1200) and 12.5% at 4:3. It is not a 2.35 cinema crop.
  - Inferred: `Stage` reports the visible area in stage units, as `dev/GUI.md` saw in play, so the fraction is the same under the engine's scale mode.
  - Wider than 16:9 the height comes out negative. What Flash draws then was not checked.
- **The cutscene subtitle** (`SubtitleBox`):
  - Placed at (26, 24.8) on the stage; its text field runs from x −2 to 970 and y −2 to 61, centred.
  - The text is HTML, `$BodyFont` size 16, colour `#cfbe5f`.
  - The box has no layout behaviour, so it stays at the **top** of the screen, horizontally centred: about 3.5% of the height from the top, inside the top bar when there is one.
  - It is hidden while the text is empty, and filled through the token `Cutscene.SubtitleText`; the engine's side is `0x00691FF0` ("Spoken lines", below).
- **Dialogue mode letterboxes too.** `conversation.gfx` has the same two bars at the same places, sized by the same formula (`Conversation.m_nLetterboxHeight`).
  - The NPC's text (`MainTextContainer_mc`) is aligned top-centre.
  - The replies (`ButtonContainer_mc`) are aligned bottom-left, 21 stage units a row.
  - When the bar is under `MAX_LETTERBOX_HEIGHT` 40 (16:9, 16:10), the text is pushed down by the bar's height over a full-width backing. Above it (4:3), the text sits in the bar and the replies move into the bottom bar.

### Spoken lines (ASLN, type 23; `0x007DC0D0`, update `0x007CF840`; checked)

- **The voice** is the FMOD event `<Vobank>/VO/<strref>_m`, or `_f` when the main character is female and the event exists (`0x007F2B70`). For example `bhm100_jowan/VO/36658_m` lives in `audio/vo/en-us/vo/bhm100_jowan.fev`/`.fsb`.
- **The face** is `<strref>_m.fxe` (`modulefacialanimations.erf`). `LipsynchSet` (5562) is overwritten by that name, so it is unused.
- A missing voice is no error: the sound is dropped. With `NoVOIngame` (5565) neither is loaded.
- **The action's stop is replaced by 1e10**; the line ends itself on its voice. It starts once:
  - The sound is placed at the actor and played, through the face when FaceFX has the animation and the actor a face.
  - The player's **+0xE8, the count of lines still speaking**, goes up, and down when the voice (or, with no voice, the face) ends.
  - With no voice and no face, nothing counts, and the subtitle stays until the next line or the end. **There is no duration from the text.**
- **The subtitle:**
  - In cutscene mode it is the cutscene screen's text (`0x00691FF0`). With a voice it is hidden when the subtitles option (`ClientOptions` +0x78) is off; with none it always shows.
  - In dialogue mode it is the conversation's `NPCLine` with `HasVO`.
- **The clock never waits for a voice.** `CutscenePlayUntilVoCompletes` (5018, player +0x11C) is read only by "finished" (`0x007CD290`: failed to load, or ended and +0xE8 = 0), whose only callers are dialogue mode's. So **5018 has no effect on a full cutscene**, which ends at its `RunTime` with any voice left to play out.

### A line's cut against a full cutscene

- **A line cut** is the same player and the same actions (`MakeLineCutscene` `0x007F0960`, with `LoadCutscene`'s builder `0x007E26E0` or the inline `0x007E2A70`), played in dialogue mode: it holds its last frame, its voices come from its `ASLN`s, 5018 matters only to it, its skip offers the replies, and its end script is never run. `dev/PLOTS.md`'s "A line's cut" has it.
- **A full cutscene** (`LoadCutscene`, mode 5):
  - **Skipping jumps to the next skip marker first** (`0x00691D60` → `0x007CF2E0`, the markers' starts at player +0x190): the actions are reset, the voices stopped. Only with no marker left does it end (above, "Skipping").
  - At its natural end (frame ≥ the last), the mode's exit runs the `EndScript` (5001) with `ExecuteScript(script, OBJECT_INVALID, 0, −1)` (`0x006925E9`), and the player is deleted, its creatures put back.

## A cutscene, the area transition after it, its actors' binding (read 2026-10-18 by lib-agent-2)

Read in the GOG exe (daoexe.py disassembly) and the game's files (ghilana) by lib-agent-2 (a subagent, prefix cq1) for DAOParagonEngine #29. Not seen in play. "Checked" is read in the disassembly or the files; "inferred" is marked. Builds on `%TEMP%\lib-agent-2-cutscenes.md`, master's `dev/TURN_BASED.md` ("The game mode's change"), `dev/LOAD_EVENTS.md` and `dev/CUTSCENES.md`.

### 1. DoAreaTransition and cutscene mode: the order of events

**What the harrowing's scripts actually do (checked, files):**
- `bhm400_greagoir.dlg`'s last reply (strref 122105, no children, so the conversation ends on it) has the action plot `5544441D…` = `plt_bhm600pt_harrowing` flag 0, `INTRO_CUTSCENE`.
- `bhm600pt_harrowing.nss`:51–60, `INTRO_CUTSCENE`: `CS_LoadCutscene(bhm500cs_harrowing.cut, PLT_BHM600PT_HARROWING, GOTO_FADE)`.
- `CS_LoadCutscene` → `CS_LoadCutsceneWithReplacements` (`utility_h.nss`:2094–2125) only **stores** the plot and flag in module locals (`CUTSCENE_SET_PLOT`, `CUTSCENE_SET_PLOT_FLAG`, `CUTSCENE_TALK_SPEAKER`) and calls `LoadCutscene(cut, OBJECT_INVALID, TRUE, [], [], TRUE)`: **no replacements**.
- The cut's `CutsceneEndScript` is `gen00cs_cutscene_end` → `CS_CutsceneEnd()` (`utility_h.nss`:2051), which reads those locals, clears them, and calls `WR_SetPlotFlag(plot, GOTO_FADE, TRUE, TRUE)`.
- `GOTO_FADE` (flag 5): `UT_DoAreaTransition(bhm600ar_fade_harrowing, BHM_WP_PC_TO_FADE)` → `DoAreaTransition` (`utility_h.nss`:1051–1093, after `SetLoadHint`).
- **So vanilla asks for the Fade at the cutscene's end, from its end script, not when it loads.** The one exception is below (the skip-cutscenes byte). A port that runs `GOTO_FADE` at load is running the end script early.

**The command (checked):** `DoAreaTransition` (745, handler 0x0065F5F0) pops two strings (−639 on a failed pop), and if there is a module calls `CCModule::ForScript_DoAreaTransition` 0x00600A40, pushing its bool (0 with no module). No game-mode check anywhere on this path.
- 0x00600A40 looks the area's tag up in the module's map at +0x2D0 and, failing that, in each 0x2C-byte entry of the vector at +0x300 (0x00600AB0–0x00600B5A; inferred: the area lists' areas). Not found: returns 0, no request.
- Found: a 0x110-byte loading data of kind 7 (0x006C5640(7, args)), then `SetGameMode(9, force 1, data)` (0x005D5E90 → 0x006414A0), returns 1. **The command's 1 means "queued", not "loaded".**

**The request (SetGameMode 0x006414A0, checked):** a request is 16 bytes: mode, the current mode's number when asked (current mode +0x5C, 0x00640060), force, data.
- Loading (9) is a stack mode, so it is pushed on the stack (+0x9C; begin +0xA0, end +0xA4) by 0x00641220.
- Special case (0x006414EA–0x00641649): if the new request is mode 9 of kind 4, 5 or 7 (data slot 5 → +4) and the stack already holds a mode-9 request of kind 4, 5 or 7, **the waiting one is replaced in place** (its data destroyed). So two transitions asked in a row: the last wins.

**Who takes it (the controller 0x00640C60, checked):**
- A request is taken only while the current mode (+0x74) answers true to slot 10 (+0x28, base mode) or slot 11 (+0x2C, finished), or to slot 12 (+0x30, interruptible) with the stack not empty.
- `GameModeCutscene` (vftable 0x00B0E76C): slot 10 = 0x005C5E80 (`xor al,al`), slot 12 = 0x005C5E80, slot 11 = 0x006922D0 (`mov al,[ecx+0xCC]`). **So nothing is taken while a cutscene plays; it waits until the cutscene's +0xCC is set.**
- +0xCC is set only by 0x00691F40, called from the mode's update (slot 8, 0x00692C60) when the player's update (0x007E3D30) answers false: the cut ended, naturally or by skipping past the last marker. 0x00691F40 also restores the camera (0x006C45F0/0x006C4EC0/0x006C4B20).
- The stack is taken first, last in first out (0x00640330 empty, 0x00640AB0 back, pop at 0x00640D0F); the base slot (+0x8C) only when the stack is empty. **The base slot isn't cleared when taken**: it is the standing base mode (explore), which is why a finished stack mode falls back to it.

**The switch (0x00641D30, checked):**
1. Take a request (0x00640C60). At the cutscene's end the stack is empty, so it is the base slot's explore (0).
2. Old mode number to +0x88; **the old mode's leave** (slot 7, +0x1C) with the new mode number.
3. **If the request was the base slot's and the stack is no longer empty** (a request filed during the leave), the controller takes again and switches to that one instead, dropping the first (0x00641DB2–0x00641E13).
4. +0x78 the new mode; the new mode's enter (slot 6); `EVENT_TYPE_GAMEMODE_CHANGE` (60) to the module.

**Cutscene mode's leave (0x00692450, checked), in order:**
1. Camera restored; the end script name taken (0x00691CF0).
2. The player torn down (0x007E10A0: a script cutscene puts each bound creature back to its place and facing at binding) and deleted.
3. **The end script run at once**, `ExecuteScript(script, OBJECT_INVALID, 0, -1)` (0x006925E9).
4. Timers restarted (`0x005D5540(0)`), the cutscene GUI closed (app +0x158 slot 0x8C), input mode restored, then 0x006403F0(1) on the controller and the base leave 0x006945A0.

**So the order a port should follow (checked, but not seen in play):**
1. In dialogue (4), the reply's action sets `INTRO_CUTSCENE` → `LoadCutscene` builds the player and pushes request 5. Dialog can't be interrupted, so it waits until the conversation is over (it ends on that reply).
2. Dialog → Cutscene (event 60: 5, 4). The cut plays its 50.5 s (or skips through its markers). Its actors are resolved and bound in the player's update states 1–2 (section 5).
3. The cut ends → +0xCC → the controller takes explore from the base slot → the cutscene's leave runs: creatures put back, player deleted, then `gen00cs_cutscene_end` → `GOTO_FADE` → `DoAreaTransition` → request 9 (kind 7) pushed.
4. Same switch: the stack isn't empty now, so explore is dropped and **Cutscene → Loading directly** (event 60: 9, 5). **No frame of explore in between, and the cutscene is gone before the load starts.**
5. Loading (state 7 → 4: `bhm600ar_fade_harrowing` is in another area list, `al_bhm600ar_fade_harrowing.rim`, so it is a list swap) → state 6. Its step 1 asks `SetGameMode(0, 1, 0)` (0x006C8F8B–0x006C8F91, checked: explore, explicitly; `Addresses.h`'s "sets the game mode the loading began in" is wrong on that point). It leaves when its last step runs → explore (event 60: 0, 9).
- **The cutscene never continues into the new area and is never torn down by the load:** the load can't start while it plays.
- **A transition asked during a cutscene** (not vanilla here, but the rule): the request waits on the stack. At the cut's end the controller takes it **instead of** explore (the stack goes first), so it is Cutscene → Loading as above, with the end script run in the leave. A transition the end script asks for is then a second kind-7 request filed during the switch: it isn't taken in that switch (step 3 only applies to a base-slot request) and goes in on Loading's next take (not read: Loading's slots 10–12).

**The exception, the skip-cutscenes byte (checked):** `LoadCutscene` (0x0066D3BF–0x0066D43A) reads `g_eclipseGame` (0x00C1D0A0, through 0x00402CA0) +0x208. When it is set, the cut isn't played: its `CutsceneEndScript` (5001) is run at once, inside `LoadCutscene`, so `GOTO_FADE` → `DoAreaTransition` happens **during the dialogue's action**. The loading request then waits behind Dialog, and Dialog → Loading follows at the conversation's end. What sets +0x208 is not read (inferred: a debug or skip option); vanilla play doesn't skip the harrowing cut.

### 3. EVENT_TYPE_CREATURE_ENTERS_DIALOGUE (86) at a script cutscene's binding

**Built in 0x007F2460 (checked), for an object bound with no conversation** (0x007CF390: the player's +0x23C, a conversation, is 0 → 0x007F2460(obj); with a conversation its resolver 0x007F1E90 is used instead):
- First, on the object: slot 0x140 (1) and slot 0x13C (1), then the state saved (0x006952A0); `dev`'s earlier reading has these as SetActive(true), Show(true).
- If it is a creature (slot 0x10 non-null) and there is a module (0x005D4FC0): a `CScriptEvent` (0x50 bytes, 0x0069BAC0):
  - **type** +0x20 = 0x56 (86);
  - **creator** +0x24 = the creature's id;
  - **object 0** = the creature's id (0x00A5B950(0, id));
  - **target** +0x28 = the module's id;
  - **no ints, floats or strings** set.
- Queued by `AIMaster` 0x0064AD80(caller = creature id, target = module id, 10, event) with no delay (its node's time fields +4/+8 are 0), so delivered at the next queue drain, not at once.
- Then the creature: +0xAA5 = 0, its +0xA30 reset (0x00A63AF0), the movement and actions reset (0x006CAB90, 0x006CA130, +0x16C = 0, 0x0069EDC0), **`ClearAllCommands(1, 0)`** (0x0073C930), the animation state's slot 0x68, 0x0069E590, then 0x006A64B0(5), 0x006A64B0(6), 0x006A7F30(0), and its weapon set (+0x7B0) re-applied (0x006C1400). Not read in detail.
- A non-creature (the slot 0x24 result) gets slot 0x60 on its +0x4C and `ClearAllCommands(1, 0)` (0x007F26A3–0x007F26C2).
- **The release** (0x007F2800) calls 0x007F26F0 (at 0x007F29AC), which builds type 0x402, **1026 `EVENT_TYPE_AMBIENT_CONTINUE`**, creator and target the object itself, object 0 itself, and queues it to the object the same way (checked). So at a script cutscene's end each bound creature gets AMBIENT_CONTINUE (`creature_core.nss`:680).

**When it reaches the module (inferred):** the queue is drained by `AIMaster::Update` in the world pass, which cutscene mode doesn't run (slot 18 false, `dev/TURN_BASED.md`). So the 86s queued in the player's update states 1–2 wait until the cutscene is over: for the harrowing, until the loading states drain the queue (state 6 step 4), after the list swap. By then the tower's creatures are gone with their area, and `GetEventObject(ev, 0)` is invalid for all but the hero.

**Where it is handled (checked, scripts):** only `module_core.nss`:461–471 (`sp_module` passes on what it doesn't handle, `dev/CHARGEN.md`):
- `oCreature = GetEventObject(ev, 0)`;
- `RemoveEffectsDueToPlotEvent(oCreature)` (`effects_h.nss`:1064): each valid effect whose type has `CancelOnPlotEvent` set in `TABLE_EFFECTS` is removed, by itself if its ability id is 0, otherwise every effect of that ability (`RemoveEffectsByParameters(oCreature, EFFECT_TYPE_INVALID, id)`);
- `DEBUG_PrintToScreen`.
- No other script handles 86.

### 5. A mapping tag with no replacement: resolved by tag, nearest to the hero (checked)

**The actor's resolution** is in the player's update, state 1 (0x007E3DEE → 0x007E36B0 per actor with no bind record +0x4F4) and state 2 (0x007E4124). So it runs in cutscene mode's first updates, **after** `LoadCutscene` has applied its replacements (an actor already given an object, +0x1C0 ≠ −1, is skipped).

**In 0x007E36B0 (0x007E36F0–0x007E3862):** for an actor not yet mapped (+0x154 = 0, +0x1C0 = −1, no model +0x1E0) whose `CutsceneActorMappingTag` (+0x44) isn't empty:
- `HENCHMAN` (0x00B23680): the player's counter +0x17C is bumped and 0x007CD0A0(tag, n) asked until an id not already used (the set at player +0xFC, 0x008663A0) comes back. Any other tag: one ask with n = 0.
- **0x007CD0A0(tag, n) is the conversation's resolver, 0x007F2D80(tag, player +0x244, 0, n, &found)**, so a script cutscene's tags resolve exactly as a conversation's:
  - `OWNER` (0x00B246A8): the id passed (player +0x244);
  - `PLAYER` (0x00B24698): the party's +0x1E8 (the hero);
  - `HENCH…` (0x00B2468C) tags of more than 9 characters: party members (party +0x234) matched by tag; not read in detail;
  - **anything else (0x007F2F91):** the reference creature is `GetCreatureById(player +0x244)`, falling back to the hero (party +0x1E8) when that is invalid, as it is for `LoadCutscene` (inferred: +0x244 is a conversation's owner and unset here). Then **0x006973B0 on that creature: the nearest object with that tag in the creature's area** (the area's objects, 0x0067CC00/0x0067CC80; only those whose slot 0x6C answers true, inferred active; same area id +0x254; tag +0xB8; distance from the reference's +0x120; max distance 0 = none). −1 when none, or when the reference has no area.
- A found id not already used is bound: `0x007E30B0(actor, object, put back = (no conversation))`, which drives the object's model, makes the 0x4C bind record and calls the bind (0x007E3A00 → 0x007CF390 → 0x007F2460 above); the id goes in the used set (0x006F1340).
- **Not found** (−1, or the object missing): the actor falls through to 0x007E3862, its own bare model from `ModelResRef` or one assembled from `CreatureResRef` (0x006A4F00). No game creature is made. `CutsceneActorMappingRequired` (255 on the templars, Cullen, Irving and Greagoir, 0 on `PLAYER`) isn't read on this path; where it is read, if anywhere, is not read.

**On `bhm500cs_harrowing.cut` (checked, files):**
- Mapped actors (tag = `CreatureResRef` for all but the player): `bhm100cr_door_templar_2` (id 12), `bhm100cr_door_templar_1` (13), `bhm300cr_cullen` (14), `bhm400cr_irving` (16), `bhm400cr_greagoir` (17), and `PLAYER` (22, `player_placeholder_human_m`).
- Unmapped: `MASTER` (0), `FreeCamera1` (18, the active camera), `VFX 1` (19, `fxm_mercury_c`), `VFX 2` (23, `fxa_trans_in_c`), two sound holders (24, 25), all `tools_null` or a VFX model.
- `bhm500ar_tower_harrowing.are` (the cut's `EnvArea`) places all five tagged creatures, `Active` 1, tag = template. So in vanilla **all six mapped actors are the area's creatures and the hero**, found by tag nearest the hero, and none is a bare model. The script passes no replacements.
- No `CutsceneStageResRef`, so the actors are in world coordinates (as the earlier note says).

### Not read

- Loading mode's slots 10–12 (whether a request filed during the switch into Loading waits for its end).
- What sets `g_eclipseGame` +0x208 (the skip-cutscenes byte) and +0x158/+0x144: the loading code's own `SetGameMode(5, 1, data)` (0x006C94A5–0x006C94F6) builds a cutscene from +0x144 when +0x158 > 0 (inferred: a start-up or debug cutscene, not a script's).
- 0x006973B0's choice among several matches past the first (the nth for `HENCHMAN`), and its slot 0x6C.
- The creature calls after `ClearAllCommands` in 0x007F2460, and 0x007F1E90 (a line cut's bind through the conversation).
- Whether anything drains the event queue during cutscene mode other than the world pass (inferred not, from `dev/TURN_BASED.md` and `dev/LOAD_EVENTS.md`).
- What the party's `+0x1F8` is (the dialogue camera's B when the owner is the controlled creature), and whether `ShowLine` runs for the player's own chosen reply lines.
- The `BlendGad` blend (`0x007D8240`), what `ApplyFutureGads` changes, what fills player `+0x210`, and the engine's blend of two GAD orientation keys.
- The engine's GFx scale mode for the game's movies, and what a negative letterbox height draws.
