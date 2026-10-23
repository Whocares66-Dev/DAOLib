# Camera: the explore camera as the engine runs it

How the engine runs the explore camera: its two zooms and the switch between the close and tactical views, distance, pivot, pitch and field of view, the controls, the reset on a load and after a scene, and the pull-in that keeps it out of walls. Read 2026-10-18 in the GOG exe by lib-agent-2 (with subagents) for DAOParagonEngine, whose camera is to be vanilla's. Not seen in play beyond `src/dao/Camera.h`'s sightings of 2026-09-30. The constants below were checked in the exe, and the key steps in the disassembly. It builds on `Camera.h`'s notes and `Addresses.h`'s camera rows (the controller `CameraControllerExplore`, `ctl`; the scene camera, `cam`; its orbit behaviour, `orbit`).

## Two zooms

- **The controller's zoom setting** `ctl+0x48` runs from -1 (tactical) to 1 (close), between `ctl+0x64` = -1 and `ctl+0x68` = 1. It starts at 1 when the controller is built (0x0076F890), once per process, in `GameModeController`'s constructor.
- **The scene camera's zoom** `cam+0x24` runs 0..1 and eases toward (setting + 1) / 2 (0x007EE770, into `orbit+0x88`). The controller's `Zoom()` (0x007EE7E0) maps it back: zc = 2s − 1.
- **The views:** the close view is zc ≥ 0.5, i.e. s ≥ 0.75; the tactical view is s = 0, with a band between that the camera only passes through. The view flag `ctl+0x90` is zc < 0.5 (0x0076FC92–0x0076FCAD).
  - `Camera.h`'s `SceneCamera::Zoom` says "Below 0.5 the camera is in the tactical view": on that 0..1 scale it is below 0.75.
- **The switch** (0x0076FBF0, checked):
  - Zooming out: on the first frame zc < 0.5 with the setting below zc and the flag not yet set, the pitch is kept in degrees at `ctl+0x98` (65505 = none, 0x00B1D9C8). The setting snaps to -1, so the camera always goes on to the tactical view.
  - Zooming in from the tactical view: the setting becomes max(setting, 0.5), so the camera always comes back to s ≥ 0.75.
  - Unless 0x0076F380 says the tactical view is blocked (`ctl+0x9D`, then a ray test 0x00648DA0): then zooming out stops at zc = 0.5.
- **The easing** (in the orbit, 0x006C2DB5–0x006C2E9A), per substep h:
  ```
  d = |T - s|;
  if (d < 1e-6) { s = T; spd = 0; }                                   // 1e-6 at 0x00B107FC
  else { spd = min(spd + 20*d*h, 2.0); s += sign(T - s) * min(spd*h, d); }   // 20 at 0x00C038D4, 2.0 at 0x00C038D0; spd = orbit+0x8C
  ```
  It accelerates toward the target, capped at 2 a second, and doesn't slow before arriving.
- **The script's `SetZoomLevel`** (0x00666790) sets `ctl+0x50` = clamp(z, 0, 1) × 2 − 1, toward which the base update eases the setting (-9999 = none pending). Only `genev_tutorial` calls it.

## Distance, pivot, pitch and field of view

- **The radius** (0x006C2360): r = s × close + (1 − s) × far, with close 3.0 (0x00C0E010) and far 20.0 (0x00C0E00C), written into the controller every frame. It is linear in s, with no table or curve.
- **The pivot** is the focus plus (0, 0, h). The camera sits on the sphere of radius r about the pivot, and looks at the pivot (the look-at behaviour, type 3, 0x006C3039). The focus is the creature's position (+0x120, its feet) plus the controller's offsets (+0xF0, +0x100), with no height of its own.
  - **h is the creature's height:** its slot 108 (`CCCreature` 0x006A1A30), the appearance's `Height` (`apr_base`, hash 0x0DD1F4C1, default 1.8). Human 1.75, elf 1.60, dwarf 1.426. With no appearance, it is the `head` bone's height (fallback 1.75).
  - `ctl+0x2C` eases toward it: step = min(|d| × dt / 0.15, |d|) (0x007EEA70; 0.15 at 0x00C11C30).
  - **Blended toward 1.8 in the band** (slot 13, 0x0076F640, checked): `t = zc < 0.5 ? (0.5 − zc)/1.5 : 0` (= 1 − s/0.75), `h = (1 − t)·h + t·1.8` (0x00C0E028).
  - So the tactical camera is √(20² + 1.8² + 2·20·1.8·cos 45°) = 21.31 m from the focus, the 21.3 seen. Close in, √(3² + h² + 2·3·h·cos p) is 3.8 m at p ≈ 77° for h 1.75.
- **The pitch** is from straight above.
  - **Close view (s ≥ 0.75):** the player's, clamped to 30°..150° every frame (0x0076F1B0; `cam+0x0C`, `cam+0x08`; 0x00C0E050, 0x00C0E054).
  - **Zooming out through the band:** `SetPitch((1 − t)·kept + t·45°)` every frame (45 at 0x00C0E014).
  - **Tactical (s = 0):** `SetPitch(45°)` every frame; the kept pitch is cleared. Mouse pitch and Home/End are zeroed there (`ctl+0x5C` = 1).
  - **Zooming back in:** from 45° toward `ctl+0x94` as s rises, so the close view resumes at about 75° (inferred). `ctl+0x94` starts at 85° (0x00C0E018) and eases toward 85°, or 45° while `ctl+0x9E` is set, in the tactical range (0x0076F337).
- **The field of view** (0x0076F120), vertical, in degrees, set at once: 60 in the close view (0x00C0E000), `(1 − t)·60 + t·42.5` in the band (0x00C0E004), so 42.5 tactical.
- **Near and far:** explore sets neither. The fly camera sets 0.25/4000 and puts back 0.01/4000 when it leaves (0x0076D52B), so explore runs at 0.01/4000 (inferred).

## The controls

**The default bindings** (`ResetKeys` 0x006E9510, `RegisterActions` 0x006E30D0; matching `KeyBindings.ini`):

| Input | Default |
|---|---|
| 2/3 forward/back (RunCharacterForward/Backward) | W, S |
| 4/5 strafe left/right (RunCharacterLeft/Right) | **Q, E** |
| 6 LookAndRunForward | left and right mouse together |
| 26/27 MoveCameraLeft/Right (turn) | **A, D** (and Shift+A, Shift+D) |
| 14 CameraControlMode (orbit) | right mouse; 11 yaw = mouse X, 12 pitch = mouse Y |
| 15 CameraControlMode_SuppressZoom | right mouse, so mouse Y never zooms during a drag |
| 16 CameraPanMode | middle mouse; 17/18 = mouse X/Y |
| 19–22 ControlCameraUp/Down/Left/Right (pan) | arrows |
| 9/10 pitch up/down | Home, End |
| 23/24 zoom in/out | wheel up/down; PageDown/PageUp |
| ToggleRunWalk | numpad / |

Left drag is box selection (48, 49 with Shift); it doesn't move the camera.

**The options** (`ClientOptions` at [0x00C32938]):
- `MouseSensitivity` 1.0; `MouseZoomSensitivity` 2.0; `InvertXAxis` 0; `InvertYAxis` 0.
- `DelayCameraLookTime` 0.0; `DelayCameraLookOffset` 30.0.
- `ScreenEdgePanning` 1; `ScreenEdgePanningPixels` 1 (inferred).

**The frame:**
1. The explore update (0x007714B0) sets the wheel step `ctl+0x4C` = `MouseZoomSensitivity` × 0.05 (0x00C0E008).
2. The base update (0x007EEB70) reads the input (0x007EDC60) as three rates.
3. It writes `cam+0x44` = −yaw and `cam+0x48` = −pitch (0 in tactical), and the setting += zoom × `ctl+0x4C`, clamped to [-1, 1].
4. The scene camera's manager (0x006C4C60) caps dt at 0.15 s (0x00C038F4), runs the behaviours in substeps of at most 0.015 s, then zeroes `cam+0x44`/`+0x48`.

**Zoom:**
- **A wheel notch** is a message (0x0070BD70/0x0070BD90) adding ±1 to `ctl+0x44`, so the setting moves by ±2 × 0.05 = **0.1** (0.05 of s).
- **PageDown/PageUp held:** ±`MouseZoomSensitivity` × dt × 5.0 (0x00B02CD0), i.e. 1.0 of the setting a second.

**The mouse orbit** (right button; 0x007EE0BE–0x007EE1BD):
- The drag starts once the signed sum of the mouse's x and y movement passes ±30 mickeys (`DelayCameraLookOffset`); movement before that is dropped. While it lasts, the cursor is hidden and held.
- The rates:
  ```
  yaw   = clamp(dx * sens*0.04 * 0.05/dt, -20, 20);   // 0.04 at 0x00B24560, 0.05 at 0x00AF70B0 (doubles)
  pitch = clamp(dy * sens*0.04 * 0.05/dt, -20, 20);   // close view only
  if (InvertX) yaw = -yaw;  if (InvertY) pitch = -pitch;
  if (Home) pitch += 0.4; else if (End) pitch -= 0.4;  // 0x00AE7128
  ```
- **The orbit's lag** (0x006C2C8A–0x006C2D78, per substep h; 60.0 at 0x00B03BF8, 20.0 at 0x00AFEBA8):
  ```
  vYaw   += (60*cam[0x44] - 20*vYaw)   * h;  yaw  += vYaw*h;                                      // orbit+0x80
  vPitch += (60*cam[0x48] - 20*vPitch) * h;  pitch = clamp(pitch + vPitch*h, cam[0x0C], cam[0x08]);  // orbit+0x84
  ```
  A first-order lag, τ = 0.05 s, gain 3: about 0.3% is left after 0.25 s. So Δyaw = −0.006 × sens × dx rad (0.34° a mickey), and the same for pitch. Mouse right turns the view right; mouse down looks down more. Home/End settle at 1.2 rad/s.

**The keyboard turn, A/D** (0x007EE28D–0x007EE3F9), only while no drag is on:
```
if (A)      r = clamp(r - 3*dt, -1, -0.1);   // 3.0 at 0x00AF0C38; r = ctl+0x6C
else if (D) r = clamp(r + 3*dt, 0.1, 1);
else        r = 0;
yaw = r;                                      // into the same orbit; InvertX applies
```
So 0.3 rad/s at once, rising to 3 rad/s (172°/s) in 0.3 s. A turns the view left. With the right mouse held, A/D strafe instead. It works in the tactical view as well.

**WASD in the close view** (the steering, 0x00764150 → 0x00763A00, 0x00763850, 0x007636A0): the leader moves relative to the camera. Angles are in degrees, counter-clockwise from +y; the world's direction is (−sin a, cos a).
```
x = (E || (D && RMB)) - (Q || (A && RMB));  y = (W || LMB&&RMB) - S       // then normalized
if (prev != 0 && d != 0 && dot(prev, d) > cos(100°) && dot < 1)           // 100 at 0x00C0D5B4
    a = aPrev + clamp(wrap(aNew - aPrev, -180, 180), -360*dt, 360*dt);    // 360°/s at 0x00C0D5B0
camYaw  = wrap(deg(cam yaw) - 270, 0, 360);                               // 0x006C21C0
heading = wrap(deg(atan2(-d.x, d.y)) + camYaw, 0, 360);                   // facing = heading
```
- S runs toward the camera, the creature turned round; it never walks backward. A change of 100° or more snaps.
- When the player stops turning the camera and the leader is idle, the leader turns to the camera's way if it is more than 90° off (0x00B00618; command 0x29, 0x006ADE40).
- The speeds are the creature's (`apr_base` `Game_WalkSpeed`/`Game_RunSpeed`, inferred); the body's own turn rate is drive sub-action 16's (0x007A8C20, not read).

**In the tactical view WASD still drive the leader**: the steering never reads the view. The camera pans only by:
- **The arrows:** 20 m/s (0x00C0E020), not scaled by height, a diagonal not normalized: `off += right·x·20·dt + fwd·y·20·dt`, along the camera's flattened forward.
- **The screen's edge:** within `ScreenEdgePanningPixels`, the same speed, only with no arrow key down and the middle button up.
- **A middle-button drag:** it grabs the ground plane through the focus.

These pan only when the setting is all the way out (`TacticalStep` 0x00771060). A step is kept only while the target or a party member stays on screen (0x00770230, the creature's position plus radius + 0.5 m toward the focus, 0x00C0E024), and the moved camera clears the level by a 0.25 m sphere (0x006C28B0; skipped where `ctl+0x9D`). There is no distance limit. With the right mouse held, or no pan input, it keeps the party in view (`KeepInView`).

**Back to the close view** (0x0076F470): both offsets are zeroed. The focus eases from where it was to the leader over max(0.3, 0.6 × (1 − d/60)) s for d ≤ 60 m (60 at 0x00B03BF8), and snaps beyond that.

## On a load, and after a scene

- **Every load and transition ends with the camera reset behind the leader.** Three places set `ctl+0x25`: the loading state 6 (0x006C60F0, after the party is placed), the area's object load (0x00688625) and `GameModeLoading` slot 6 (0x006C5950). Each does `SetTarget(controlled creature)`; the next update calls the reset (explore slot 9, 0x00770FF0):
  - **TurnBehind** (0x007EE880, checked) sets the yaw at once: `wrap(deg(atan2(−f.x, f.y)) + 270, 0, 360)` in radians (270 at 0x00B10808), f the creature's forward (+0x130). On a transition that is the waypoint's facing.
  - **The pitch:** `SetPitch(((1 − z)·45 + z·ctl+0x94)°)` with z = (setting + 1)/2 (0x0076F550). At the close zoom that is **85°**.
  - The pan offsets are zeroed, the pull-in cleared, the roof fade set at once.
- **The zoom is kept across loads**, never reset; a save keeps nothing of the camera.
- **No key turns the camera behind the leader:** `TurnBehind`'s only caller is the reset. `CenterOnObject` and kin only retarget or snap the focus.
- **After a cutscene**, the camera's position, orientation and field of view saved at its entry are put back at once (0x00692450). **After a conversation**, the same, and the orbit is made again from the kept yaw and pitch, the zoom snapped to the setting. Either way the explore camera returns as it was, at once.

## Collision: a pull-in, close in only

The orbit tests nothing, but the scene camera's manager (0x006C4C60, checked) acts on the controller's mode `+0x30` after the behaviours. The mode is set each frame (0x0076F0B0): 1 in the close view (zc ≥ 0.5), 2 in the band, and in tactical 0 where `ctl+0x9D`, else 3.
- **The pull-in, mode 1** (0x006C3710 → 0x006C32C0): a capsule of radius 0.25 m and 0.1 m tall in z is swept from the pivot out to the orbit's place (0x006490B0, mask 0x1024; COLLISION.md section 6; corrected 2026-10-22: a sphere is in fact that capsule, and the direction is as this first said, after a wrong correction the same day):
  ```
  want = min(|hit - raw|, max(|pivot - raw| - 0.25, 0))
  if want <= pull: pull = want                                    // in at once
  else { pull = min(pull + 15*dt, want);                          // 15 at 0x00C038E4
         if (!RayClear(raw + dir*pull -> pivot)) pull = want; }   // 0x00648DA0
  camera = raw + normalize(pivot - raw) * pull
  ```
- **The release** (0x006C35A0), each frame the cast doesn't hold it: `pull -= min(speed, 15)·dt` (0x00C038E0); `speed += 10·dt` (×30 in mode 2). It is held while a 0.25 m sphere at the new place would hit.
- **Mode 3** (tactical) only tests a 0.25 m sphere at the camera (0x006C3180) and flags `ctl+0x26` for the wheel.
- **Objects within 0.25 m of the camera fade** to 0.05 at 5 a second (0x006C4130, 0x00C038EC), except in dialogue, cutscene and fly-camera modes. Nothing between the camera and the leader fades. In detail:
  - **The near fade (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151) is SceneCameraManager's 0x006C4130 (`void __thiscall(this = the manager, float dt)`)** [C]. It runs each manager update after the camera is placed [I: its caller was not followed; the manager's +0x70 it reads is the place the pull-in writes, CAMERA.md "Collision"].
  - **Every object it holds is first given a target of 1** [C]: `0x006C3C80(1.0, -1)` walks the manager's map at +0xA8 (a std::map keyed by object id, its node's +0xC the id and +0x10 the target) and writes 1.0 into each target (0x006C3C80, the `param_3 == -1` branch). With an id other than -1 it writes that one id's target (`0x005007F0`, the map's operator[]) [C].
  - **Then, unless the game mode is 4, 5 or 6, the scene is asked what touches the camera** [C]: the mode is `0x005D5E70` (app+0x114's `GameModeController::GetGameMode`, -1 with none); for any other mode it calls `0x00648C00(&hits, manager+0x70, 0.25 (0x3E800000), 2)`, which is `World::CurrentScene()` vtable +0x30 with `(out, centre, radius, mask, exclude = 0)` [C 0x00648C00]. Modes 4, 5, 6 are the conversation, the cutscene and the fly camera [I: from the mode numbers elsewhere in the notes; this function only compares the numbers].
  - **Scene slot 12 (+0x30) is `Physics::PhysicsScene::OverlapSphere` 0x0082E980** [C: the word at vtable 0x00B272D4 + 0x30]. It is OverlapCapsule's sibling (COLLISION.md section 5: the same scene lock, `PhysicsEntityReport<NxShape*>` report, `PushFilter`/`PushFilterOps` AND filter, so a shape is met when `(bits0 & mask) != 0` [I for PhysX's equation, as there]); the PhysX call is NxScene vtable +0x1C4 with shape type 3 (all shapes) [C 0x0082E980]. Each shape met is pushed as a 0x40-byte record whose +0x28 is `GetActorObjectId(shape)` and whose +0x34 is `GetActorRoomId` [C], the record of COLLISION.md section 6's table. It returns whether any was met [C].
  - **The mask is 2, the `.phy` Creatures flag** (COLLISION.md section 1's table) [C for the value 2]. Shapes that carry it are a creature's body capsules: a humanoid's armour `.phy` (`hm_arm_hvya_0.phy`, read with ghilana) has capsules under Chest, Chest1, the collars, shoulders and hips, each `MMHShapeCollisionMaskCreatures` 1 and `Nonwalkable` 1, so bits0 0x81000002 [C, the file]; no Head capsule in that file [C]. A creature's controller (groupsMask 0x10) and placeables (0x4) are not met [I: by their masks, COLLISION.md sections 1-2]; so in practice only creatures fade [I].
  - **Each object met gets a target of 0.05** [C]: `0x006C3C80([0x00C038E8], record+0x28)` for every record (stride 0x40) [C 0x006C41D0 loop]; 0x00C038E8 reads 0.05 [C].
  - **Every held object's alpha moves toward its target at 5 a second** [C]: step = `[0x00C038EC] × dt`, 0x00C038EC reading 5.0 [C]; the alpha is read from the object by `0x006C2550(id)` (`GetObjectById`, then `0x00695140`, which returns the object's +0x194, 1.0 for no object) [C], moved by the step toward the target without passing it, and written back by `0x006C2520(alpha, id)` → `0x00695060(alpha, 0)`, which stores the alpha at the object's +0x190 and the flag at +0x1A4 [C].
  - **An object whose alpha reaches 1.0 leaves the map** [C: the `fVar1 == 1.0` branch erases the id's node, `0x0050B9A0`].
  - **The model takes the alpha as its opacity** [C]: `0x00695080` calls the model's (object +0x154) vtable +0x20 with `+0x198 × +0x190` and the +0x1A4 flag when that product, or +0x198, or the flag changed (or always with its argument set), then keeps +0x194 = +0x190, +0x1A5 = +0x1A4, +0x19C = +0x198 [C]. So +0x194 is the alpha last given the model, which is what the next step starts from [C]. That the model's slot ends in each mesh's `mml_fOpacityMultiplier` (the generic object fade 0x004F43C0, SKY.md) is [I: 0x004F43C0 writes that parameter, the chain from slot +0x20 to it was not followed].
  - **dt** is the manager's argument [C]; that it is the manager's capped step (at most 0.15 s, as the pull-in's) is [I].
  - **Nothing between the camera and the leader fades** [C: the only query is the sphere at +0x70].
  - Seen: a creature stood 0.3 m in front of the close camera (its chest capsule within 0.25 m) is not drawn; one at 0.6 m is drawn whole.
- **A roof fade:** when the camera is more than 3, 4 or 5.5 m (0x00B1D91C, indexed by the layout's `EnvAreaCutoffHeight`, in a level of rooms; corrected 2026-10-22: it was noted as "by area type") above the focus, the game-mode controller's fade (0x00640250) turns on over 0.2 s. It fades the cut-away meshes of the level's rooms (read 2026-10-22; "The roof fade" and "The cutoff" below).

### The roof fade: the trigger and the eased fade (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151)

- **The test is the explore controller's vtable slot 14 (+0x38; Ghidra's `vfunction15`), `CameraControllerExplore` 0x0076F6E0** [C: the word at the explore vtable 0x00B1D978 + 0x38; slots counted from 0, as CAMERA.md's "explore slot 9, 0x00770FF0" is +0x24]: it answers 1 when `ctl+0xA0 < sceneCamera+0x78 − ctl+0x78` [C], the scene camera's z (its place is at +0x70, so +0x78 is z [I]) less the controller's +0x78 [C]. That +0x70..+0x78 of the controller is the focus, so +0x78 its z, is [I: only its zeroing in the constructor 0x0076F890 was found; the store is presumably a vector copy].
- **The height ctl+0xA0 is set in slot 5 (+0x14, 0x0076FD70) from 0x0076FD30** [C]: 5.5 (0x00B1D924) unless ctl+0x9D is set and the area's ChunkManager (area +0x658, through `0x0076F720`) has `+0x78 < 3`, then `[0x00B1D91C + 4 × (+0x78)]`, reading 3.0, 4.0, 5.5 [C].
- **Correction (corrected 2026-10-22; the line said "by area type")** [C]: the index is not an area type. ChunkManager +0x78 is the layout's `EnvAreaCutoffHeight` (3129): the layout load 0x005D1290 reads field 0xC39 = 3129 and stores it at +0x78 (0x005D15AA push, 0x005D15E4 `mov [esi+0x78], eax`) [C]. And it counts only where ctl+0x9D is set, which is a level of rooms (below).
- **ctl+0x9D is "a level of rooms"** [C]: slot 5 sets it from `0x0067B860`, which is `ChunkManager+0x7C == 2` (0x0076FD7F store; 0x0067B860 body) [C].
- **Correction (corrected 2026-10-22; PATHFINDING.md said "stored as a dword 0/1")** [C]: ChunkManager +0x7C is 1 for a chunked level (0x005D168E), and for a room level it is EDI at 0x005D1A7B, which holds 2 from 0x005D13F6 (`mov edi, 2`, no other write to EDI on that path to the store; the chunk branch's `xor edi, edi` at 0x005D1688 is on the other side of the `je` at 0x005D167C) [C]. So 0 only when the layout is not read [I]. PATHFINDING's rule (`== 1` chooses the chunked mask, 0x0067B4D0) is unchanged.
- **The switch is 0x0076F5A0 (`__thiscall(ctl, bool immediate)`)** [C]: with `immediate` the fade's length is 0, else 0.2 s (`[0x00C0E040]`, reading 0.2) [C]. If the controller's +0x9C is clear (or `immediate`) and slot 14 answers 1, it calls `GameModeController::0x00640250(1, length, immediate)` and sets +0x9C; else if +0x9C is set (or `immediate`) and slot 14 answers 0, it calls `0x00640250(0, length, immediate)` and clears +0x9C [C]. The GameModeController is `[0x00C327EC]+0x114` [C 0x0076F5D9-0x0076F5E0].
- **Its callers** [C, xrefs]: 0x007714B0 (explore vtable slot 7, +0x1C [C: the vtable word]; each update [I]) and 0x00771019 in 0x00770FF0 (slot 9, +0x24, the reset of CAMERA.md "On a load"), the latter immediate [I: by the argument it passes, not read]. So "the roof fade set at once" on a reset (CAMERA.md :126) is this call [I].
- **0x00640250(bool on, float length, bool flag)** [C]: start (+0xB0) = current (+0xBC); target (+0xB4) = 0 when `on`, 1 otherwise; length (+0xB8) = `length`; time (+0xC4) = 0; with `length > 0` running (+0xCC = 1), else current = target at once (+0xCC = 0); +0xD4 = `flag` [C]. So "on" fades the roofs **out** (to 0) [C].
- **The step is 0x005CC4D0(dt) on the struct at +0xB0** [C]: while length > 0 and time < length, time += dt (clamped to length), t = time / length (+0x18; at 1 the running byte +0x1C is cleared), and current = `0x005CBC90(start, target, t)` [C].
- **The curve 0x005CBC90 is a sine ease** [C]: `start + (target − start) × (sin(t·π − π/2) + 1) × 0.5` (constants π at 0x00AF14C8, π/2 at 0x00AFFE90, 1.0 at 0x00ADE218, 0.5 at 0x00AE70F8, read as doubles). This also fills COLLISION.md :157's "the fade's curve 0x005CBC90 not read" for the room darkness, which uses the same function [C for the function; that the room's fade calls it is that line's own claim].
- **Each GameModeController update** (0x00641F90) runs the step and then copies the value into +0xD0 [C 0x00641FF1-0x00642005].
- **A conversation or cutscene shows the roofs** [C]: `0x006403F0(bool)` (called from 0x006910D0, 0x006913E0, 0x00692450, 0x00692980 [C, xrefs]) with 0 saves the current value at +0xC0 and sets it to 1 at once; with 1 it puts the saved value back at once [C]. That 0 is entering and 1 leaving the scene is [I: by the callers' names, not read]. The same function then walks the area's object list (+0x694) and calls `0x0074E850` on each placeable (below) [C].

### The cutoff: what the roof fade fades (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151)

- **The area hands the fade to its world each update** [C]: 0x0068AF30 (an area update, callers 0x005FE800, `GameModePartyPicker` slot 9, 0x007B4BB0) calls `0x00538060(GameModeController+0xD0, GameModeController+0xD4)` on the area's world (area +0x62C), then clears +0xD4 [C 0x0068AF44-0x0068AF61].
- **0x00538060** [C]: if the world's +0xD4 (the cutoff) exists, `0x004670C0(value, flag)`; if its +0xC0 exists, that object's vtable +0x20 with the value [C]. What +0xC0 is was not read.
- **0x004670C0, the cutoff** (this = world +0xD4) [C]:
- **The cutoff's +0x4C is the area's switch** [I]: where +0x4C is written was not read. The candidate is `EnvAreaCutoffSystemEnabled` (3134), which the layout load reads into ChunkManager +0x74: first +0x74 = `!EnvAreaChunkIsChunk` (`setz dl` at 0x005D1625, stored at 0x005D162D), then, if the file has field 0xC3E = 3134, its value (0x005D1640-0x005D1673) [C for the ChunkManager field].
- **0x00466F40, the test of a node** [C]: the node's vtable +0x18 must answer non-zero, then its +0x1C is its kind:
- **The cut-away test, slot +0x8C, is the mesh's `MMHMeshCutAway` (6193)** [C] (corrected 2026-10-22 by the camera helper's reading, checked by lib-agent; this said +0x1A7 and the link inferred). The mesh-node loader 0x00594D40 stores each read's result at its own block, after the next push: the byte at +0x1A7 (0x00595034) is the read before, and field 0x1831 = 6193's result goes to the blueprint's +0x1A8 (0x00595069). The blueprint's slot 0xA8 (0x0057CEE0) returns +0x1A8; the mesh's constructor (0x0055B972) fills its own +0x1C9 from that slot, and slot +0x8C (0x0055B460) returns +0x1C9. the shipped cut-away meshes are named `MshFade_*` (e.g. `cav_roof.mmh`: mesh `MshFade_CAV_Roof_0`, `MMHMeshCutAway` 1) [C, the file]; Ishal's first floor has 104 such mesh nodes [C, counted].
- **0x00466930, what a cut-away mesh does with the value v** [C]:
- **The global 0x00BF73FC is read by the SpeedTree and terrain draws** [C, xrefs: 0x0052CC60, 0x00530C20, 0x00536200, 0x005333D0, `EC::CExteriorChunk` slot 25 0x005CE410]: 0x00530C20 draws nothing below 1 when `0x00C1EBF4` is set; 0x005333D0 skips its four passes below 1 unless its +0x2C; 0x0052CC60 picks a "transparent" technique below 1 for objects with +0x1BC bit 0x40000000 [C]. What those objects are was not read.
- **Files** [C, ghilana]: Ishal's floors and other room levels have cut-away roofs; the camp (`brc999d`) has `EnvAreaCutoffHeight` 2 and `EnvAreaCutoffSystemEnabled` 0, so its tents never fade [C for the fields, I for "never", through the +0x4C reading above].
- Seen (Ishal floor 1, the tactical view): the upper walls and the floor above are gone in the game. With the meshes shown, the floor above covers the hall from that camera.

#### Not read (the fades)

- Where the cutoff's +0x4C is written (the link to `EnvAreaCutoffSystemEnabled`).
- The chain from the model's slot +0x20 (0x00695080) to `mml_fOpacityMultiplier`.
- The writer of the explore controller's +0x70..+0x78 (the focus).
- What the world's +0xC0 is (0x00538060's second call).
- The kinds 6, 7, 0x13, 0x14 and 0x16 and their flag 8.
- 0x004F25B0 and the blend states 0x004F4380 sets.

## The tactical view at the Fade's start is the tutorial's (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe, the game's files and the scripts by lib-agent-2 (with subagents) for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the disassembly or the files.

- **The area script:** `bhm600ar_fade_harrowing.are` runs `bhm000ar_generic_area`. On `AREALOAD_POSTLOADEXIT` in `BHM_AR_FADE`, the first time, it saves and calls `BeginTrainingMode(TRAINING_SESSION_TACTICAL_VIEW)` (`bhm000ar_generic_area.nss`:144–151). The `.are` has no camera field.
- **`SetTrainingMode`** (command 842, 0x006636F0 → 0x00692E90 on the module's +0x498, `CClientExoApp::GetModule` `0x005D4FC0` then `mov ecx, [eax+0x498]` at `0x0066374B`; corrected 2026-10-18 by lib-agent-2, not the app's):
  - It drops the session when `ShowTutorials` (`ClientOptions` +0x98) is off, or the session's bit is set in `CompletedTraining` (+0x1C8, kept in the options, not the save).
  - It pauses in explore or combat.
  - It raises `EVENT_TYPE_TRAINING_BEGIN` (1059) through `genev_tutorial`.
- **`genev_tutorial.nss`**:78, session 1: `SetZoomLevel(0.0)`, the only call in the 2,101 scripts. It writes the controller's +0x50 = −1, which the setting eases to, snapping to the tactical view. The tutorial then waits for `MAXIMUM_ZOOM_IN`.
- **So it is conditional:** with tutorials on and the session not yet done in this install. Six other origins start the same session; after any of them, or with tutorials off, the Fade opens at the kept zoom, normally close.
- **The controller is built once per process** (0x005DB7F0 → `GameModeController` 0x00641370 → 0x00641870 → `CameraControllerExplore` 0x0076F890, one call site each). Nothing in character creation, a new game, a load or a cutscene writes the setting (+0x48) or its target (+0x50). Only the wheel, the clamp, the easing, the view switch and `SetZoomLevel` do.
