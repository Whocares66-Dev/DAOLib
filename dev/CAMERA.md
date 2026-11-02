# Camera: the explore camera as the engine runs it

How the engine runs the explore camera: its two zooms and the switch between the close and tactical views, distance, pivot, pitch and field of view, the controls, the reset on a load and after a scene, and the pull-in that keeps it out of walls. Read 2026-10-18 in the GOG exe by lib-agent-2 (with subagents) for DAOParagonEngine, whose camera is to be vanilla's. Not seen in play beyond `src/dao/Camera.h`'s sightings of 2026-09-30. The constants below were checked in the exe, and the key steps in the disassembly. It builds on `Camera.h`'s notes and `Addresses.h`'s camera rows (the controller `CameraControllerExplore`, `ctl`; the scene camera, `cam`; its orbit behaviour, `orbit`).

## Two zooms

- **The controller's zoom setting** `ctl+0x48` runs from -1 (tactical) to 1 (close), between `ctl+0x64` = -1 and `ctl+0x68` = 1. It starts at 1 when the controller is built (0x0076F890), once per process, in `GameModeController`'s constructor.
- **The scene camera's zoom** `cam+0x24` runs 0..1 and eases toward (setting + 1) / 2 (0x007EE770, into `orbit+0x88`). The controller's `Zoom()` (0x007EE7E0) maps it back: zc = 2s − 1.
- **The views:** the close view is zc ≥ 0.5, i.e. s ≥ 0.75; the tactical view is s = 0, with a band between that the camera only passes through. The view flag `ctl+0x90` is zc < 0.5 (0x0076FC92–0x0076FCAD).
  - `Camera.h`'s `SceneCamera::Zoom` says "Below 0.5 the camera is in the tactical view": on that 0..1 scale it is below 0.75.
- **The switch** (0x0076FBF0, checked):
  - Zooming out: on the first frame zc < 0.5 with the setting below 0.5 (`ctl+0x48` against 0.5, 0x0076FC15-0x0076FC1F, read 2026-11-01) and the flag not yet set, the pitch is kept in degrees at `ctl+0x98` (65505 = none, 0x00B1D9C8). The setting snaps to -1, so the camera always goes on to the tactical view.
  - Zooming in from the tactical view: the setting becomes max(setting, 0.5), so the camera always comes back to s ≥ 0.75.
  - Unless 0x0076F380 says the tactical view is blocked (`ctl+0x9D`, then a ray test 0x00648DA0): then zooming out stops at zc = 0.5.
- **The easing** (in the orbit, 0x006C2DB5–0x006C2E9A), per substep h:
  ```
  d = |T - s|;
  if (d < 1e-6) { s = T; spd = 0; }                                   // 1e-6 at 0x00B107FC
  else { spd = min(spd + 20*d*h, 2.0); s += sign(T - s) * min(spd*h, d); }   // 20 at 0x00C038D4, 2.0 at 0x00C038D0; spd = orbit+0x8C
  ```
  It accelerates toward the target, capped at 2 a second, and doesn't slow before arriving.
- **The script's `SetZoomLevel`** (0x00666790) sets `ctl+0x50` = clamp(z, 0, 1) × 2 − 1, and the base update then moves the setting that way at dt a second (-9999 = none pending). It stops only at an end of the range, `ctl+0x64` or `ctl+0x68`, where the pending value clears, and never tests the value itself (0x007EEC45-0x007EEC99, read 2026-11-01), so the camera runs past the level asked for. Only `genev_tutorial` calls it.

## Distance, pivot, pitch and field of view

- **The radius** (0x006C2360): r = s × close + (1 − s) × far, with close 3.0 (0x00C0E010) and far 20.0 (0x00C0E00C), written into the controller every frame. It is linear in s, with no table or curve.
- **The pivot** is the focus plus (0, 0, h). The camera sits on the sphere of radius r about the pivot, and looks at the pivot (the look-at behaviour, type 3, 0x006C3039). The focus is the creature's position (+0x120, its feet) plus the controller's offsets (+0xF0, +0x100), with no height of its own in the close view.
  - **h is the creature's height:** its slot 108 (`CCCreature` 0x006A1A30), the appearance's `Height` (`apr_base`, hash 0x0DD1F4C1, default 1.8). Human 1.75, elf 1.60, dwarf 1.426. With no appearance, it is the `head` bone's height (fallback 1.75).
  - `ctl+0x2C` eases toward it: step = min(|d| × dt / 0.15, |d|) (0x007EEA70; 0.15 at 0x00C11C30).
  - **Blended toward 1.8 in the band** (slot 13, 0x0076F640, checked): `t = zc < 0.5 ? (0.5 − zc)/1.5 : 0` (= 1 − s/0.75), `h = (1 − t)·h + t·1.8` (0x00C0E028).
  - So the tactical camera is √(20² + 1.8² + 2·20·1.8·cos 45°) = 21.31 m from the focus, the 21.3 seen. Close in, √(3² + h² + 2·3·h·cos p) is 3.8 m at p ≈ 77° for h 1.75.
  - Read 2026-10-31 by paragon-agent's helper, checked by lib-agent-2 with a subagent [C unless marked]:
  - **In the band and the tactical view the focus's z is the party's height.** UpdateFocus (0x00771250) blends it: for zc < 0.5, z = (1 − t)·z + t·`ctl+0xE8`, with t the pitch's (0x0076EAB0(1), from the scene camera's eased zoom, 0x007EE7E0) (0x00771306–0x0077133E).
    - **`ctl+0xE8`** eases (0x00770030, each frame at the tactical step's head, 0x00771079) toward T = max(maxZ − m, meanZ) over the active party's creatures (party +0x234, built by 0x00770E80), with m = 0.25 × 20 × cos 45° = 3.54 m (20.0 at 0x00C0E00C, 45.0 at 0x00C0E014, 0.75 at 0x00C0E034). Step min(|gap| / 0.33 × dt, |gap|) (0.33 at 0x00C0E03C).
    - **A reset snaps it:** the explore reset (slot 9, 0x00770FF0, at 0x00771041) passes dt = 65505.0 (0x00B07ECC) in the tactical view.
    - So the 21.31 m below holds with the party on level ground; on a slope the focus is the party's height, at most 3.54 m below the highest.
  - **While a retarget ease runs** (`ctl+0x1AC`), h is that ease's height, `ctl+0x19C`, not `ctl+0x2C` (0x0076F657–0x0076F66B; "A new target" below).
- **The pitch** is from straight above.
  - **Close view (s ≥ 0.75):** the player's, clamped to 30°..150° every frame (0x0076F1B0; `cam+0x0C`, `cam+0x08`; 0x00C0E050, 0x00C0E054).
  - **Zooming out through the band:** `SetPitch((1 − t)·kept + t·45°)` every frame (45 at 0x00C0E014).
  - **Tactical (s = 0):** `SetPitch(45°)` every frame; the kept pitch is cleared. Mouse pitch and Home/End are zeroed there (`ctl+0x5C` = 1).
  - **Zooming back in:** from 45° toward `ctl+0x94` as s rises, so the close view resumes there. `ctl+0x94` starts at 85° (0x00C0E018) and eases toward 85°, or 45° while `ctl+0x9E` is set, in the tactical range (0x0076F337).
- **The field of view** (0x0076F120), vertical, in degrees, set at once: 60 in the close view (0x00C0E000), `(1 − t)·60 + t·42.5` in the band (0x00C0E004), so 42.5 tactical.
- **Near and far:** explore sets neither. The fly camera sets 0.01/4000 when it is entered (0x0076D530-0x0076D54D: the renderer camera's slot 0x18 with the floats at 0x00AF7498 and 0x00ADEEAC) and puts back 0.25/4000 when it leaves (0x0076D5D9-0x0076D5F6, 0.25 at 0x00AFF3E8), so explore runs at the area's scene camera's 0.15/4000 (made at the area's load, 0x0040AE60 from 0x006892A7; seen 0.15 exploring in play, DAOLib #205, src/dao/Camera.cpp) until a fly camera has been left, and at 0.25 after it until the next area's camera (corrected 2026-11-01, where this had the two the other way round, and then explore as 0.25).
- **The aspect correction** (`Renderer::PerspectiveCamera::GetAspectCorrection`, 0x00A25AA0; aspect, read 2026-11-01; DAOParagonEngine #351). The projection's aspect is the viewport's w/h (+0x144) times this factor: slot 41 (0x00A25AD0) builds `PerspectiveFov(out +0x40, fov +0x140, aspect +0x144 × factor, near +0x130, far +0x134)`, and the frustum's corners (slot 21, 0x00A25C00) and the screen-point-to-ray (slot 18, 0x00A262D0) use the same product, so the picture, the culling and the pick agree [C]. The factor is 1.0 when the byte at +0x148 is 0 [C 0x00A25AA1-0x00A25AA8]; +0x148 is set by the constructor (0x00A25B30) only for a camera made with the flag on, and the only such camera is the area's world camera (0x0040AE60 passes `1`; the portrait, paperdoll and graphics-default cameras pass 0), and only while the `capturemovie` launch option (`g_eclipseGame+0x259`) is off [C]. With it set, the camera calls the current driver's vtable slot 97 (+0x184) [C]: `D3DGraphicsDriver`'s is 0x0086B530, `fld [ecx+0x7368]`; the threaded wrapper's (`g_graphics+0xB8`, used inside the 0x00527930(1) bracket) is 0x0096BDE0, `fld1`, so a projection built there gets 1.0 [C for the slots; I for which side of the bracket the projection is built on]. The float at +0x7368 is 1.0 from the constructor and is written by `UpdateAspectCorrection` (0x0086A500), run after the device is created and after it is reset to a new size, not every frame [C]. **It stays 1.0** when `ClientOptions+0x520` is set, which is `[VideoOptions] DisableAspectRatioCorrection=1` in DragonAge.ini [C 0x0086A516-0x0086A536; the key's binding 0x005EC01B]; when the device-settings record (`this+0x12C`) is absent or its +0x30 is non-zero, which is Windowed [C the test 0x0086A540-0x0086A54C; I that +0x30 is `Windowed`]; when `EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, ...)` fails or lacks DM_PELSWIDTH and DM_PELSHEIGHT [C 0x0086A571-0x0086A593]; and when the ratio is within 0.05 of 1 (`|ratio − 1| ≤ 0.05`, the double 0x00AF70B0, `test ah,0x41`) [C 0x0086A6BF-0x0086A6EA]. Otherwise it is (display w/h) / (back buffer w/h), the display's from the desktop mode Windows reports and the back buffer's from the device settings' +0x10/+0x14 [C]. A 1024x768 back buffer on a display reporting 1920x1080 gives 1.333, a 16:9 frustum for a 4:3 buffer that the panel's stretch then undoes [C for the arithmetic; I for the scenario, since an exclusive fullscreen mode normally makes the reported mode the back buffer's size and the ratio 1.0]. **A slip in the display-orientation test** [C]: with DM_DISPLAYORIENTATION in dmFields, the word at dm+0x4C is compared with 0 and 2 and w and h are swapped otherwise, but dm+0x4C is `dmPosition.x` and `dmDisplayOrientation` is dm+0x54, so the swap follows the monitor's desktop x, never taken on the primary monitor (two readings reconciled; the intent is [I]). The same float is the Scaleform viewport's AspectRatio (GUI.md, "AspectRatio" under the viewport at 0x00511242). In a window, and in fullscreen at the native mode on a square-pixel display, the factor is 1.0 and the projection's aspect is the viewport's w/h alone.

## The controls

**The default bindings** (`ResetKeys` 0x006EA9F0, whose defaults function is 0x006E9510 and which makes the 56 SetKey calls [C], `RegisterActions` 0x006E30D0; matching `KeyBindings.ini`; INPUT.md has the bindings and the ini):

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

Read 2026-10-31, checked by lib-agent-2 with a subagent [C]:
- **The explore update's order** (0x007714B0):
  1. `ctl+0x4C`, close 3, far 20.
  2. The zoom switch 0x0076FBF0, which ends with the view flag, the pitch (0x0076F1B0) and the FOV (0x0076F120).
  3. The base update 0x007EEB70: the reset if `ctl+0x25`, then the height ease, the input, the zoom, the place.
  4. UpdateFocus 0x00771250: the party's height, the tactical step, the focus.
  5. The orbit and look-at set-up 0x0076EFA0.
  6. The roof fade's switch 0x0076F5A0(0).
  7. The scene camera's mode 0x0076F0B0 (0x006C2890).
  8. 0x0076FBC0, which lifts a blocked tactical setting back to −1 once 0x0076F380(1) passes.
  9. The usage count 0x0076F750: once a second in modes 0 and 1, counting seconds zoomed in or out and summing the setting. They go out as telemetry ('GAME'/'GCMD', 'GCTP') by slot 19 (0x0076EC50) when a telemetry object exists.

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

These pan only when the setting is all the way out (`TacticalStep` 0x00771060). A step is kept only while the target or a party member stays on screen (0x00770230, the creature's position plus radius + 0.5 m toward the focus, 0x00C0E024), and the moved camera clears the level by a 0.25 m sphere (0x006C28B0; skipped where `ctl+0x9D`). There is no distance limit. `CameraControllerExplore::UpdateFocus` 0x00771250 writes the focus `ctl+0x70` (0x00771479); its sideways term is multiplied by the float at 0x00C379A0, which nothing writes (its one reference is the read at 0x007713F0, and it is zero in the file's uninitialised data), so the term is absent. With the right mouse held, or no pan input, it keeps the party in view (`KeepInView`). `KeepInView` 0x00770BB0 (corrected 2026-11-02, this said it does nothing unless the first test passes; paragon-agent's reading, checked) takes the creature's move since last frame, flat (`ctl+0x80` less its place), and either adds it to the pan `ctl+0x100`, which holds the view still while the creature walks, or leaves the pan alone, so the view follows. It holds in two cases [C]: 0x00770A10 says some listed party member's attack target is a creature hostile to the given one and on screen inside margins of 33% of the width and height (0.33, 0x00C0E030), and every listed member with its byte +4 set is on screen (`IsObjectOnScreen` 0x007C4D20, 0x00770DA3) inside 25% (0x00C0E02C) on the axes the creature moved along (a margin is set only where the move along `ctl+0x120` or `ctl+0x110` is above zero, 0x00770CBE, 0x00770D2C); or, failing that, the creature itself is off screen (0x0076EBE0, 0x00770E6B). Otherwise it follows. Either way the pan is then capped at the creature's distance from last frame's focus: if the pan's squared length is over that of `ctl+0x70` less the creature's place, flat, it becomes that vector (0x00770DDB-0x00770E52).

**Back to the close view** (0x0076F470): both offsets are zeroed. The focus eases from where it was to the leader over max(0.3, 0.6 × (1 − d/60)) s for d ≤ 60 m (60 at 0x00B03BF8), and snaps beyond that.

Read 2026-10-31 by paragon-agent's helper, checked by lib-agent-2 with a subagent [C unless marked]:
- **A new target** (`SetTarget` 0x007EE840, then slot 0, `OnTargetChanged` 0x007709B0, with the new and old creatures):
  - **Tactical view:** the offset `ctl+0x100` becomes the focus minus the new target's position, its z set to 0, so the view stays where it was.
  - **Close view, with an old target:** `BeginFocusEase` (0x007707D0). For d = the distance between the old and new targets' positions (or from the eased place `ctl+0x160` while an ease runs) ≤ 15 m (0x00C0E04C), the focus eases over max(0.3, 0.6 × (1 − d/15)) s (vector record at `ctl+0x130`, current `ctl+0x160`, active `ctl+0x188`). A height record at `ctl+0x190` (current `ctl+0x19C`, active `ctl+0x1AC`) eases from the live height to the new creature's slot-108 height. Each frame its end is set to the live `ctl+0x2C` (0x0076FB60). Beyond 15 m the camera cuts. (`Camera.h` has the 15 m ease; this is a different one from the 60 m one above.)
  - While the ease runs, the scene camera's `cam+0x3C` is 0, and the pull-in's release (0x006C35A0, which tests it at 0x006C35AC) is skipped.
- **The zoom input's details:**
  - The wheel and PageUp/PageDown reach the setting through 0x007EDBD0: p = d × `ctl+0x4C`, the setting += p, then slot 1 (`OnZoomChanged` 0x0076FAA0 in explore, `ClampZoom` 0x007EE680 in the base). So a held key moves the setting by `MouseZoomSensitivity`² × 0.25 a second (1.0 at the default 2.0).
  - **`ctl+0x26`**, set by the tactical mode's sphere test (0x006C3180 → 0x006C2930; "Collision" below), lets a zero change through. In the tactical view it then lifts the setting to 0.5, with no input.
  - **ClampZoom** clamps to [−1, 1], clears `ctl+0x26`, and for a positive change (or a setting at 1) sends the tutorial event 0x429 (`TutorialControl::SendEvent` 0x00693290, module +0x498, the TutorialControl that 0x006939E0 builds (vftables 0x00B0EAB4 and 0x00B0EA9C, stored at 0x0060487F); corrected 2026-10-31 by lib-agent-2 from TrainingTutorial).
- **Mouse y in the tactical view:** `ReadInput` (0x007EDC60) asks slot 3 (0x0076EF10, `ctl+0x90 != 0`; the table's `MouseYZooms` is a misnomer) at 0x007EDEBF and 0x007EDF91. When it is true, mouse y adds neither to the drag's latch nor to the pitch.

### The mouse buttons, and a click in the world (read 2026-10-23 by lib-agent-2 with subagents, for DAOParagonEngine; static reading, nothing played)

- **The codes are DirectInput's button offsets** [C, `ReadMouse` 0x0087FF40 and the polled query 0x0087E0E0]: 0x0C is button 0, the left; 0x0D is button 1, the right; 0x0E is button 2, the middle.
- **The swap** [C]: `SwapMouseButtons` (`ClientOptions` +0x350, through 0x00838210) swaps 0x0C and 0x0D, both where a raw event is made (0x00880230) and in the polled query (0x0087E13B). It is 0 in the player's DragonAge.ini.
- **So in vanilla, with the default options, the RIGHT button acts** (move, attack, talk, use) and the LEFT selects. This agrees with the Fade tutorial's "right-click", with GUI.md's key label "Right-click" for PerformPrimaryAction, and with DAOTurnBased's playtest ("Right-click is the action button in this install").
- **The right button's bindings** [C, 0x006E6AC5–0x006E6D97]:
  - **The press** (and a double-click's second press) sends `PreMoveCharacterToLocationMessage`, which only arms the byte 0xC35E77.
  - **The release** sends `MoveCharacterToLocationMessage` (0x0070C9A0), which moves only when that byte is armed. It also sends `PerformPrimaryActionOnTargetUnderCursor`; with Shift, the secondary instead.
  - So the action fires on the **release**.
  - **A double-click** is two presses of the same button within 200 ms (0x0088028F). The second comes as a "double-click" event, and on the right button it still arms the move.
- **The right button's orbit** [C]: input 14, polled by the camera (`CameraControllerBase::ReadInput` 0x007EDC60), not an event.
  - It latches once the summed mouse x and y movement passes 30 mickeys (`DelayCameraLookOffset`). A hold time applies only when `DelayCameraLookTime` is above 0; its default is 0.0.
  - While latched, the cursor is hidden and held at the press point.
  - **Not settled:** whether a release after an orbit still acts. No code was found that disarms the move or cancels the release's action after an orbit, and the cursor stays where the press was. The orbit does zero a record through ctl+4 (0x007EE460), which was not traced. A play test would settle it: orbit more than 30 mickeys over the ground, release, and see whether a move is ordered.
- **The left button's bindings** [C]:
  - Its release (0x0C with flag 0x10000) sends `SelectPartyMemberUnderCursorMessage 0`, and with Shift `… 1`, so Shift adds to the selection [I for what 1 means] (0x006E63C3, 0x006E6447).
  - It also sends `SelectHostileUnderCursor` (0x00712DE0), which sends the primary action for each selected member passing a test that was not read.
  - A double-click sends `CenterOnObjectUnderCursor`, which selects the creature and calls a game-mode slot, the camera's centring [I].
  - A release, and Shift+release, sends `StopDragSelect`, which ends the box (inputs 48 and 49 start it).
  - On bare ground the left button does nothing else.
- **The middle button** pans, and latches the same way as the orbit.
- **Over the interface:** while the mouse is over any movie element, the world pick returns nothing (`MouseOverInterface`, gated at 0x0070EAAC; GUI.md). A portrait's click is the Flash UI's.

## The scene camera's behaviours (read 2026-10-31 by paragon-agent's helpers, checked by lib-agent-2 with subagents)

- **Who holds it:** the GameModeController (`[app+0x114]`) holds the rig at +0x5C (0x006418ED) and the explore controller at +0x60 (0x00641929), read 2026-11-02.
- **One object:** the controller's `cam` (`ctl+4`) is one plain object, `CameraRig` in the table (no vftable; the renderer's `GraphicsStructures::SceneCamera`, vftable 0x00AF77AC, is another thing). `CameraRig::Update` 0x006C4C60 runs every behaviour with its own `this`.
- **The behaviours** are records in a vector at `cam+0x104`, found by (kind, key) (0x006C2A90; 0x006C29F0 answers whether one exists). Each updater is `__thiscall(cam, dt, record)` and its true return removes the record. The dispatch is at 0x006C4EA4:
  - 0, the position blend (0x006C2580), key 0: `cam+0x80` lerps from record +0x20 to +0x10.
  - 1, the rotation blend (0x006C2680): between the quaternions at +0x70 and +0x60, into `cam+0x90`; it ends at +0x60 [I for which is the start].
  - 2, the orbit (0x006C2C20), key 0.
  - 3, the look-at (0x006C3010), key 1.
  - 4, the cutscene's camera (0x006C27B0; CUTSCENES.md). It writes the position, orientation and FOV and never ends.
  - 5, the scalar blend (0x006C2220), key 3 the FOV (`cam+0xA0`, SetFOV 0x006C4B20).
  - Blends 0, 1 and 5 end at progress ≥ 1; 5 also at a duration of 0.
- **The easing:** the record's mode word +0x90 picks:
  - 1: table(p);
  - 2: (table(p) + p) / 2;
  - 3: 2·(table(p/2 + 0.5) − 0.5);
  - anything else: linear.
  - The table is 1001 floats at 0x00AF16A8, (1 − cos(π·i/1000)) / 2, indexed by p × 1000 (0x004EDF40). The rotation blend always uses table(p).
- **The orbit and the look-at as explore sets them** (0x0076EFA0, each frame):
  - **When they are rebuilt:** unless both exist, StartOrbit (0x006C4790) and StartLookAt (0x006C4A00) are run with id −1.
  - **SetOrbit** (0x006C30D0): (pivot offset (0, 0, h) from slot 13, close `ctl+0x1C`, far `ctl+0x0C`, `ctl+0x20`, focus `ctl+0x70`). It writes `orbit+0x10` offset, **+0x30 far, +0x40 close**, +0x48, +0x50 focus, and `cam+0x4C` far.
  - **SetLookAt** (0x006C3130): (offset, focus) into +0x10 and +0x20. The look-at aims at the object with id `record+0x90` (its +0x120), else +0x20, plus the offset +0x10.
  - **The orbit's centre** (0x006C2950) is formed the same way. So the first vector argument is the offset; the focus is the last.
- **The final position** (0x006C4E1F–0x006C4E6A): `cam+0x70` = `cam+0x80` + `cam+0x34` × the unit vector from `cam+0x80` toward the orbit's centre (0x006C31F0). `+0x34` is the pull-in ("Collision" below).
- **The shake** is its own object at `cam+0x114` (0x006C2440, with the full dt):
  - **Its output:** x = sin(f·fx)·ax·(1 − f), y = cos(f·fy)·ay·(1 − f), with f the elapsed fraction.
  - **Its start:** 0x006C2390, from 0x0079D519, only once the previous shake is over.
  - **Its use:** `CClientExoApp::FrameUpdate` reads it (0x005D7607, 0x005D7718) as a view jitter; it is never added to the position [I for its full effect].
- **Not the camera's:** `CCameraFixedPolicy` (vtable 0x00AFFE7C) is a layout policy (thresholds by distance) that the retail game never builds. The factory's one call (0x005C8FF7) passes kind 4, `EC::IndoorStreamPolicy`.
- **The controllers' vtables:**
  - **Explore:** 0x00B1D954, 20 slots. 0 OnTargetChanged, 1 OnZoomChanged, 3 0x0076EF10, 4 the deleting destructor, 5 0x0076FD70, 7 Update, 9 the reset, 11 the mode name L"Exploration Mode", 12 the focus (0x0076EC30, its +0x70..+0x78; no direct caller, but 0x007C4A10 calls it through the vtable for the pathfinder's far-goal test, PATHFINDING.md section 11), 13 the pivot offset, 14 0x0076F6E0, 19 the telemetry. `Addresses.h`:1750's "25 slots" is 20 by `daoexe.py --vtables`.
  - **Base:** 0x00B2456C. 1 ClampZoom, 5 0x007EE980, 7 the update, 9 the turn behind the target (0x007EE9A0 → 0x007EE880 with the target id `ctl+8`), 13 the pivot offset (0x007EDC40, which returns its out pointer). The base constructor 0x007EE9B0 sets `ctl+0x4C` 0.4, `+0x50` −9999, `+0x64` −1, `+0x68` 1, `+0x5C` 1 and `+0x1C` 1.0.

### What the behaviours and the explore update add (read 2026-11-01, two readings)

- **The field of view's blend** (`SetFOV(fov, seconds, kind)` 0x006C4B20; the step 0x006C2220): zero seconds sets `cam+0xA0` at once. Otherwise t += dt / seconds, and at t >= 1 the value is the target and the behaviour ends. With e(t) = (1 - cos(pi t)) / 2, from a 1001-entry table (0x004EDF40), kind 0 is t, 1 is e(t), 2 is 0.5 e(t) + 0.5 t, 3 is 2 (e(0.5 + 0.5 t) - 0.5). The value is from + (to - from) x that.
- **The view's weight** (0x0076EAB0, by the eased zoom z): toward tactical (z < 0.5) it is (0.5 - z) / 1.5, toward close (z > 0.5) it is (z - 0.5) / 0.5, else 0. The field of view is 60 degrees at z >= 0.5, else (1 - w) 60 + w 42.5, set at once.
- **The pull-in's release** (0x006C35A0): the speed `cam+0x38` rises by 10 x dt; a 0.25 m capsule sweep that would hit zeroes the speed and keeps the pull; a pull below 1e-6 resets both.
- **A long snap pauses the game:** the explore controller's viewer (`ctl+0xB0`, PARTY.md) follows the camera, and registering it (0x005C97F0) in game mode 0, more than 30 m from the old viewer, raises the chunk manager's wait flags (+0x581..+0x584) and pauses with `SetPaused(1, 0)`, noting whether the game was already paused. The wait ends once the pending chunks reach 0 (0x005CA490-0x005CA55A).
- **The look-at** (`StartLookAt(id, offset, focus)` 0x006C4A00; the step 0x006C3010): the direction is the target (the object's +0x120, else the focus) + offset - `cam+0x80`, normalised, and its yaw and elevation make the orientation. An id of -1 with no focus removes it.

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
  - **The near fade (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151) is CameraRig's 0x006C4130 (`void __thiscall(this = the manager, float dt)`)** [C]. It runs each manager update after the camera is placed [I: its caller was not followed; the manager's +0x70 it reads is the place the pull-in writes, CAMERA.md "Collision"].
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

- **The test is the explore controller's vtable slot 14 (+0x38; Ghidra's `vfunction15`), `CameraControllerExplore` 0x0076F6E0** [C: the word at the explore controller's vtable 0x00B1D954 + 0x38 (corrected 2026-10-24 by lib-agent-2 from 0x00B1D978, whose +0x38 holds no code address; `daoexe.py --vtables`, 20 slots); its +0x3C is 0x0076EF50, `ctl+0x90 == 1`, which the cursor's pick also asks (GUI.md, "The cursor's pick"); slots counted from 0, as CAMERA.md's "explore slot 9, 0x00770FF0" is +0x24]: it answers 1 when `ctl+0xA0 < sceneCamera+0x78 − ctl+0x78` [C], the scene camera's z (its place is at +0x70, so +0x78 is z [I]) less the controller's +0x78 [C]. That +0x70..+0x78 of the controller is the focus, so +0x78 its z, is [I: only its zeroing in the constructor 0x0076F890 was found; the store is presumably a vector copy].
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
  - It first stores the value in the global `0x00BF73FC` [C]; then if its +0x4C is 0 the value is taken as 1.0 [C].
  - If the value equals its +0x48 and the flag is 0 it returns [C]; else +0x48 = value, and it walks every layer of the scene (`0x00537D70` on +0x10's scene; each layer's vtable +4 gives its nodes, gathered by `0x005D2480`) and asks `0x00466F40(node)` of each [C].
  - A node it answers 1 for gets flag 0x100 in its +0x24 (hidden) [C]; one it answers 0 for has 0x100 cleared and its Umbra object's flag 8 set again from `(+0x24 & 0x11) == 0` [C]. That 0x100 means "not drawn" is [I: by the Umbra visibility set beside it].
  - **What that means outdoors** [I, by the readers below]: a level whose cutoff system is off still hands the roof fade to everything that reads 0x00BF73FC. In practice these are the SpeedTree draws: outdoors, the 5.5 m height of 0x0076FD30 applies, so the tactical view hides the trees there (see (b)).
  - Only a cut-away mesh at a value of 0 or less makes the node test return non-zero: 0x00466930 returns 1 there [C]. That 0x100 hides the node is [I].
- **The cutoff's +0x4C is the area's switch** [C, read 2026-11-01]: `EC::ChunkManager::Initialize` (0x005C9690) copies the layout's +0x74 into the scene cutoff's (scene +0xD4) +0x4C at 0x005C9788. That field is `EnvAreaCutoffSystemEnabled` (3134), which the layout load reads into ChunkManager +0x74: first +0x74 = `!EnvAreaChunkIsChunk` (`setz dl` at 0x005D1625, stored at 0x005D162D), then, if the file has field 0xC3E = 3134, its value (0x005D1640-0x005D1673) [C for the ChunkManager field].
- **0x00466F40, the test of a node** (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151) [C]:
  - **The filter and the kind** [C]: the interface's slot +0x18 must return non-zero; then slot +0x1C returns the node's kind [C].
  - **Kind 1, a mesh** [C]: ObjectNodeMesh (interface vtable 0x00AF8E7C, slot +0x1C 0x0055B480 → `mov eax,1`) and ObjectNodeVFXMesh (0x00AF900C).
    - The full node's slot +0x8C must return true: ObjectNodeMesh primary vtable 0x00AF8F4C, slot +0x8C = 0x0055B460, `mov al,[ecx+0x1C9]` [C]. Then 0x00466930 runs on it (below).
  - **Kinds 6 and 7, the sprite emitters** [C, the interface vtables' slot +0x1C, each a thunk to `mov eax,imm`]:
    - 6: `ObjectNodeEmitterSprite<SpriteParticle>` (0x00AF8724) and `ObjectNodeEmitterSpriteLinked<SpriteParticle>` (0x00AFACDC).
    - 7: `ObjectNodeEmitterSprite<PhysicsSpriteParticle>` (0x00AF88B4) and `ObjectNodeEmitterSpriteLinked<PhysicsSpriteParticle>` (0x00AFAE6C).
  - **Kind 0x13, the mesh emitter** [C]: `ObjectNodeEmitterMesh<MeshParticle>` (0x00AFAB64).
  - **Kind 0x14** [I]: another emitter. No class among the RTTI vtables returns it; it is handled together with 6, 7 and 0x13.
  - **Kind 0x16, the emitter group** [C]: ObjectNodeEmitterGroup (0x00AF7D44, slot +0x1C 0x0053D970 → 0x008CCE30, `mov eax,0x16`).
  - **For 6, 7, 0x13 and 0x14** [C]: if the owner graphics Object's flags (`[node+0x154]+8`, its +8 interface, slot +0xA4 → `return flags`, +0x260) have bit 8, then node +0x4E0 = the cutoff's +0x48. The write is `[entry+0x360]` with the entry at node+0x180.
  - **For 0x16** [C]: under the same bit-8 test, 0x0053D790 writes the value into +0x4E0 of every emitter in the group's list +0x244..+0x248.
    - The group's list is a `std::vector`: its begin at +0x244 and end at +0x248 [C: 0x0053D790].
  - **The test returns 0 for every kind other than a cut-away mesh at 0** [C]. So the walk never hides an emitter: it only clears its 0x100 and resets its Umbra flag [C].
  - **What +0x4E0 does** is EFFECTS.md's existing reading [C there]:
    - the update runs only while it is above 0 (0x00552D4C);
    - the fill draws only while it is above 0 (0x00550271);
    - the fill's alpha is multiplied by `+0x4F4 · +0x4E0` (0x005509BE).
    - The other fills read it the same way: 0x00550EC7, 0x005515B4, 0x00551ADD, and the linked emitters' Fill at 0x00585116/0x00585161 and 0x00585976/0x005859C1 [C].
  - **The cutoff touches no lights** [C]. None of the walk's kinds is a light class: ObjectNodeLight, PointLight, AmbientLight and LightProbe have no thunk to 1, 6, 7, 0x13 or 0x16 at slot +0x1C.
- **The cut-away test, slot +0x8C, is the mesh's `MMHMeshCutAway` (6193)** [C] (corrected 2026-10-22 by the camera helper's reading, checked by lib-agent; this said +0x1A7 and the link inferred). The mesh-node loader 0x00594D40 stores each read's result at its own block, after the next push: the byte at +0x1A7 (0x00595034) is the read before, and field 0x1831 = 6193's result goes to the blueprint's +0x1A8 (0x00595069). The blueprint's slot 0xA8 (0x0057CEE0) returns +0x1A8; the mesh's constructor (0x0055B972) fills its own +0x1C9 from that slot, and slot +0x8C (0x0055B460) returns +0x1C9. the shipped cut-away meshes are named `MshFade_*` (e.g. `cav_roof.mmh`: mesh `MshFade_CAV_Roof_0`, `MMHMeshCutAway` 1) [C, the file]; Ishal's first floor has 104 such mesh nodes [C, counted].
- **0x00466930, what a cut-away mesh does with the value v** [C]:
  - it clears bit 1 of the mesh's +0x1A4; then, only when v < 1:
  - v ≤ 0: sets bit 1 and returns 1, so the node is hidden [C];
  - 0 < v < 1: `Graphics::SetMaterialOpacity(0, v)`; if the material has no opacity parameter (the call answers 0) and `0x004F25B0` allows it, it switches the material (`0x004F44C0(0)`, then `0x004F4380(0xB, 1)`, `0x004F4380(5, 0xC)`, `0x004F4380(6, 0xD)`) and sets the opacity again [C]. That those are blending on with SRCALPHA/INVSRCALPHA is [I: the D3D blend numbers 5 and 6]. It sets bit 1 and returns 0 [C].
  - **What the opacity's modes set** (read 2026-11-02; the object at the material instance's +0xB4, first pass only): mode 0, this cutoff: SrcAlpha, InvSrcAlpha, depth write left on. Mode 2, SpeedTree: the same, depth write off. Mode 4, `AnimBase::ApplyModelOpacity`: as mode 0, and node flag 0x10 below 1. Mode 5, `ChunkTile::SetModelOpacity`: depth write off, depth test on, a depth bias. Exactly 0 isn't drawn (0x004F58DD-0x004F58EA).
  - So a cut-away mesh is drawn at opacity v while 0 < v < 1 and hidden at 0 [C]; at v = 1 bit 1 is cleared and the opacity left as it was [C], drawn as authored [I: that bit 1 selects the opacity's use].
- **The global 0x00BF73FC is read by the SpeedTree and terrain draws** [C, xrefs: 0x0052CC60, 0x00530C20, 0x00536200, 0x005333D0, `EC::CExteriorChunk` slot 25 0x005CE410]: 0x00530C20 draws nothing below 1 when `0x00C1EBF4` is set; 0x005333D0 skips its four passes below 1 unless its +0x2C; 0x0052CC60 picks a "transparent" technique below 1 for objects with +0x1BC bit 0x40000000 [C]. Those objects are the trees that have collision objects (TREES.md, "The trees under the area's cutoff").
  - The forests, their faded-tree pass and shadows: TREES.md, "The trees under the area's cutoff".
- **Files** [C, ghilana]: Ishal's floors and other room levels have cut-away roofs; the camp (`brc999d`) has `EnvAreaCutoffHeight` 2 and `EnvAreaCutoffSystemEnabled` 0, so its tents never fade [C for the fields, I for "never", through the +0x4C reading above].
- Seen (Ishal floor 1, the tactical view): the upper walls and the floor above are gone in the game. With the meshes shown, the floor above covers the hall from that camera.

#### Object flag 8: EnvModelCutAwayOverride (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151)

- **The only writer is slot 0x148** of the Object's +8 interface [C]: `GraphicsStructures::Object::vfunction83`, 0x00546650 (the vtable word at 0x00AF857C, in `Object::vftable_at_8` at +0x148).
  - It takes a bool b. It sets byte +0x1C9 of every kind-1 node in the Object's node list (+0x5C..+0x60) to b, then sets (b) or clears (not b) bit 8 of +0x258 on the interface, which is the Object's +0x260 [C].
- **Its caller is a room model's creation, 0x0053A510** (0x0053A737–0x0053A755) [C]:
  - the record's byte +0x13C = 1 → slot 0x148(false);
  - 2 → slot 0x148(true);
  - anything else → no call.
  - One other call site, 0x00A1F4A9 in 0x00A1EDA0, was not read [C for the site].
- **The record's +0x13C is the room model's EnvModelCutAwayOverride** (3109, 0xC25) [C]: the room-model reader 0x00A29A80 checks for the field and reads it at 0x00A29C97–0x00A29CDD.
- **So** [C for the chain]:
  - override 2 makes every mesh of the placed model cut away, and its emitters take the cutoff's value as their opacity;
  - override 1 makes none of its meshes cut away;
  - 0 leaves each mesh's own MMHMeshCutAway.
- **The meshes' +0x1C9 by default** [C]: the ObjectNodeMesh constructor (0x0055B972) stores the blueprint's slot 0xA8 there. That slot is `ObjectNodeMeshBlueprint` 0x0057CEE0, `mov al,[ecx+0x1A8]`.
- **The files** [C, the install's 3,526 .rml, every EnvRoomModelList element]: EnvModelCutAwayOverride is
  - 0 on 134,325 models;
  - 1 on 263;
  - 2 on 2,784, mostly placed effects and props: fxe_fire_cnd_p 737, fxe_fire_small_p 200, prp_chi_window_01 174, fxe_sunbeam_s_p 170, fxe_moonbm_s_p 168, prp_chain02_0 95, prp_ferelden_torch_0 62, fxe_sunbeam_w_p 61;
  - absent on 1,727.
  - The levels with the most 2s: den505d 235, den500d 224, lak302d 193, lak506d 192, lak304d 120.
- **What the player sees** [I, from the chain above]: the candle and fire flames, torches and sun beams placed in a room level fade with the roofs in the tactical view, and are not updated while at 0. Not seen in play.
- Core's room reader (`RoomOf`, `Placed`) does not read 3109 [C, Level.cpp].

#### Not read (the fades)

- The chain from the model's slot +0x20 (0x00695080) to `mml_fOpacityMultiplier`.
- What the world's +0xC0 is (0x00538060's second call).
- 0x004F25B0 and the blend states 0x004F4380 sets.

## The tactical view at the Fade's start is the tutorial's (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe, the game's files and the scripts by lib-agent-2 (with subagents) for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the disassembly or the files.

- **The area script:** `bhm600ar_fade_harrowing.are` runs `bhm000ar_generic_area`. On `AREALOAD_POSTLOADEXIT` in `BHM_AR_FADE`, the first time, it saves and calls `BeginTrainingMode(TRAINING_SESSION_TACTICAL_VIEW)` (`bhm000ar_generic_area.nss`:144–151). The `.are` has no camera field.
- **`SetTrainingMode`** (command 842, 0x006636F0 → 0x00692E90 on the module's +0x498, `CClientExoApp::GetModule` `0x005D4FC0` then `mov ecx, [eax+0x498]` at `0x0066374B`; corrected 2026-10-18 by lib-agent-2, not the app's):
  - It drops the session when `ShowTutorials` (`ClientOptions` +0x98) is off, or the session's bit is set in `CompletedTraining` (+0x1C8, kept in the options, not the save).
  - It pauses in explore or combat.
  - It raises `EVENT_TYPE_TRAINING_BEGIN` (1059) through `genev_tutorial`.
- **`genev_tutorial.nss`**:78, session 1: `SetZoomLevel(0.0)`, the only call in the 2,101 scripts. It writes the controller's +0x50 = −1, a pending move that runs on to an end of the range (above), snapping to the tactical view. The tutorial then waits for `MAXIMUM_ZOOM_IN`.
- **So it is conditional:** with tutorials on and the session not yet done in this install. Six other origins start the same session; after any of them, or with tutorials off, the Fade opens at the kept zoom, normally close.
- **The controller is built once per process** (0x005DB7F0 → `GameModeController` 0x00641370 → 0x00641870 → `CameraControllerExplore` 0x0076F890, one call site each). Nothing in character creation, a new game, a load or a cutscene writes the setting (+0x48) or its target (+0x50). Only the wheel, the clamp, the easing, the view switch and `SetZoomLevel` do.
