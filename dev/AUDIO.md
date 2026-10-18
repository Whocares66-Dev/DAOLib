# Audio: FMOD's files, and the engine's sound at run time

The game's sound is FMOD Ex's (`fmodex.dll`, `fmod_event.dll`): event projects (`.fev`, FEV1, FMOD Designer 4.x) and the sample banks they play from (`.fsb`, FSB4). The files come first: where they are, which copy the game takes, and the bank format as read from them (DAOLib #191, for Paragon's sound; its request is `dev/PARAGON_AUDIO.md`). Then the engine at run time, each section with who read it. `dao::core::install::Index` lists the files (`dao/core/Install.h`), and `dao::core::fsb` reads a bank and decodes its IMA ADPCM (`dao/core/Fsb.h`); core does not read the projects yet.

## The files (read 2026-10-18 for DAOLib #191)

### Where they are

All loose files, none in an archive, none enciphered (the add-ins' and offers' too): 9,052 banks and 9,020 projects in the GOG install. Each in an `audio\sound\` folder, or `audio\vo\<language>\vo\` for voice, of the core (`packages\core`), a package (`packages\core_ep1`), the campaign (`modules\single player`) or an add-in's `core\` or `module\`. A voice bank is one conversation's or one creature's sound set (`bhm100_jowan.fsb`, `ss_goat.fsb`), a sample a line, named by its string and the speaker's gender (`36451_m.wav`).

### Which copy the game takes

The resource manager's, by the order of `dev/FILES.md`: `AUDIO_VO:` 100006 and `AUDIO_SOUND:` 100008 for the core's, the campaign's voice 50006 and sound 50007, a package's +7 and +8 of its base, an add-in's sound at its base −1 and voice −2. A voice folder of another language than the game's is not taken.

That FMOD goes through the resource manager is read in the exe, not seen in play: the game gives FMOD its own file system (`FMOD::System::setFileSystem`, called at 0x005C59E4 with the open callback 0x005C1070, close 0x005C02D0, read 0x005BE2D0 and seek 0x005BE390). The open callback asks the resource manager for the name it is given (0x004C5950, which walks its sources in their order for the first that has it) and opens the file found through its alias (0x004CA780). That the name FMOD passes is the bank's alone, as a project names its banks, is inferred.

In the index they cost little: 18,072 copies more of some 540,000, in folders the walk lists already for the files beside them. `Index::FileOf` gives a copy's path, so a reader can open a bank of hundreds of megabytes (`music_bank_strm_new.fsb`, 310 MB) and read its headers alone.

### A bank (FSB4)

Read over all 9,052 banks. Little-endian throughout.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `FSB4` |
| 4 | 4 | sample count |
| 8 | 4 | size of the samples' headers |
| 12 | 4 | size of the data |
| 16 | 4 | version, 0x00040000 in every bank |
| 20 | 4 | header mode, 0x20 in every bank (MPEG frames padded) |
| 24 | 8 | zero in every bank |
| 32 | 16 | a hash |

The samples' headers follow at 48, each its own size first, then the data at 48 plus their size. 4,465 banks pad the headers' size by 16 bytes past the last header, so the data starts at a 32-byte boundary. The samples' data follow one another in their headers' order, each a multiple of 32 bytes: the data's size is their sum in every bank. Header modes no bank has (`0x2`, the samples after the first with 8-byte headers; `0x4`, enciphered) the reader refuses.

A sample's header is 80 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | the header's size |
| 2 | 30 | name, NUL-padded (`gui_mute_hit2.wav`) |
| 32 | 4 | length in frames |
| 36 | 4 | stored size in bytes, padded to 32 |
| 40 | 4 | loop start, 0 in every sample |
| 44 | 4 | loop end, the length less one in every sample |
| 48 | 4 | mode bits |
| 52 | 4 | default rate, 4,000 to 48,000 Hz |
| 56 | 2 | volume, 255 |
| 58 | 2 | pan, 128 (191 in 902) |
| 60 | 2 | priority, 128 (1 in 902) |
| 62 | 2 | channels, 1 or 2 (1,240 stereo) |
| 64 | 4 | 3-D minimum distance, a float, 1 in every sample |
| 68 | 4 | 3-D maximum distance, 10,000 in every sample |
| 72 | 4 | frequency variation, 0 |
| 76 | 2 | volume variation, 0 |
| 78 | 2 | pan variation, 0 |

245 of the 366,244 headers are larger, 92 to 284 bytes: the sync points, `SYNC`, a u32 count and that many u32 offsets, 88 bytes and 4 a point. Every one is in a `music_bank_strm_new.fsb` (the core's, 150 of its 240 samples with 3,442 points, and the Darkspawn Chronicles'), with the mode bits 0x80000000 and 0x4000 (sync points, without names). The offsets ascend from 0 and stay below the sample's length, so they are frames: one every four seconds or so of the music, the markers a switch between `main` and `combat` waits for (below, "Music").

The mode bits seen (FMOD's `FSOUND_` flags): 0x20 mono, 0x40 stereo, 0x200 MPEG, 0x2000 2-D, 0x4000 sync points without names, 0x100000 3-D, 0x400000 IMA ADPCM, 0x20000000 with IMA ADPCM a stereo one, 0x80000000 sync points. No sample has FMOD's loop flags: loops are the projects'.

### The codecs

- **MPEG**, 349,405 samples: the voice (MPEG-2 layer III, 24 kHz, mono) and the music (MPEG-1 layer III, 48 kHz, stereo). Each sample's bytes are its frames, from the first frame's sync, with the padding the header mode names. The reader leaves them as bytes.
- **IMA ADPCM**, 16,839 samples: the effects, the ambience and the interface. Decoded by `fsb::DecodeIma`.

**IMA ADPCM's blocks** (Xbox IMA, as vgmstream names it). A mono block is 36 bytes: an i16 sample, a u8 step index, a byte unused, then 32 bytes of nibbles, the low nibble of each byte first. The header's sample is the block's first frame, and the first 63 nibbles give the other 63, by IMA's standard expansion from that sample and step index: 64 frames a block, the last nibble not played. A sample's length is its blocks times 64, and its stored size those blocks of 36 bytes rounded up to 32.

Read by continuity: over twelve samples of `global_amb_fade.fsb`, the mean step from the last frame of a block so decoded to the next block's first is the mean step within the blocks (264 to 261, 288 to 283, 99.3 to 99.1). Taken instead as history alone, with all 64 nibbles played, it is twice that (502, 531, 184). vgmstream's `decode_xbox_ima` does the same ("even samples per block, skips last nibble").

A stereo block is 72 bytes: the two channels' headers, then 4 bytes of the first channel's nibbles and 4 of the second's in turn, eight words each. Read the same way over `gui.fsb`'s `shale_pp.wav`, `glo_fly_mv.fsb`'s `kneel_no_armor.wav`, and `vfx_spells.fsb`'s and `glo_fly_plc.fsb`'s stereo samples: the steps across blocks are those within (429 to 427, 18.2 to 19.0). The channels' blocks one after another (36 bytes each) give steps of thousands and step indices past the table's end.

### What is not known

- The projects (`.fev`): core has no reader yet (`dev/PARAGON_AUDIO.md`, section 2).
- That the game plays a bank through its own file system as read, not seen in play: no bank was put in a source of its own and heard.

## The engine at run time

### Music, as the engine plays it (read 2026-10-18 by lib-agent-2)

The music machine, `dev/PARAGON_AUDIO.md`'s question 4. Its sync points are the music banks' (above, "A bank"), which FMOD gives the engine in milliseconds.

Read 2026-10-18 by lib-agent-2 (subagent au3) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000), with capstone over the whole `.text` and the FMOD imports resolved by name; the data (music.fev, the areas, the scripts, the tables) in section 8. "(checked)" is read in the disassembly or the data; "(inferred)" is not. Not seen in play.

#### 0. In one paragraph

Music is three layers. **The area** names a theme: its `.are` field `MusicPreset` (`music\Mage Origin`) is `<project>\<event group>` of an FMOD project (`music.fev`), set when an area finishes loading; the same name as the one playing changes nothing, so music runs on across areas of one theme. **The game mode** picks the track inside the theme: explore, pregame, dialogue and cutscene play the group's event `main`, combat `combat`, the death screen `death`, character creation `chargen`; there is no other "combat music" signal: the scripts' GM_COMBAT/GM_EXPLORE switches are the music's switches. **Events with a parameter named `#music:<track>`** (VO lines, and cutscenes' switch events) play a named event of the theme (`bhm_jowan_mage`) when they start. Between main and combat the new track is started at the old one's position, snapped forward to the stream's next sync point (marker), with a stinger (`stingers/from_main`, `stingers/to_main`, `stingers/to_death`) and the new track faded in over its own designed fade-in. One float, the event parameter `music_state`, is set on the theme's events by `SetMusicIntensity`, by music-volume triggers and by area changes (0). `PlayMusic` and `StopMusic` do nothing.

#### 1. The objects (checked)

| Object | Where | What |
|---|---|---|
| Sound manager | singleton, getter 0x005B9020 | +0x5C the current game mode, +0x60 the previous (set by its update 0x005BD880); +0x120 the music controller; +0x90 the FMOD renderer (`CContentRenderer_FMOD`) |
| Music controller | sound manager +0x120, constructor 0x005B6BF0, no vtable | the engine's policy: theme by area, track by mode, `#music:` tracks, the `music_state` value and its delay |
| Theme wrapper | controller +0x58, 4 bytes (constructor 0x005B9710), forwarding 0x005B6D10..0x005B6E10 | holds the theme object |
| Theme | `EclipsePrivate::CMusicThemeContent_FMOD`, vftable 0x00AFF86C (14 slots), 0x160 bytes, made by the renderer's slot 11 (0x005C4910, constructor 0x005C4680) | one FMOD event group and its events; a state machine updated every frame |

**Music controller fields** (from its constructor 0x005B6BF0 and uses):
- +0x00 wstring: the track last seen by the ducking update.
- +0x1C wstring: the current theme name (as given, e.g. `music\Mage Origin`); +0x38 the theme to return to after a one-shot theme.
- +0x54 the last game mode acted on (-1 at a theme change).
- +0x58 the theme wrapper.
- +0x5C wstring, +0x78 byte, +0x7C float: a pending track, its "return to main" flag and its delay in seconds (-1 none).
- +0x80 a volume (1.0), +0x84 the options' music volume, linear (0x005BC735; -60 dB when music is off).
- +0x88 float delay (s) and +0x8C float value: a pending `music_state`.
- +0x90 initialised, +0x91 music disabled (the option `MusicSystemDisabled`, settings +0x3D0, passed at 0x004042EF by 0x005B6CB0), +0x92 a one-shot theme plays, +0x93 last "playing" seen by the ducking update, +0x94 "credits at the end of the game" (`ShowStartMenu(bShowCredits)`, command 362, handler 0x0066A5A0, sets it at 0x0066A61D), +0x95 a pregame flag (1 at construction; set at 0x0064D0C5 from a menu index ≠ 0; meaning unread).

**Theme fields** (constructor 0x005C4680, debug print 0x005C13A0 "Music theme= '%s', track= '%s'"):
- +0x04 wstring the loaded project's resolved path; +0x20 the FMOD `EventProject`; +0x24 the `EventGroup` (the theme); +0x28 its `stingers` subgroup.
- FMOD events: +0x2C `main`, +0x30 `combat`, +0x34 `death` (made on demand), +0x38 the stinger playing, +0x3C `stingers/from_main`, +0x40 `stingers/to_main` (both made at load, paused, started so they are loaded), +0x44 the track playing ("main" in the debug print), +0x48 the track being left ("combat" in the debug print).
- wstrings: +0x4C the project name (`music`), +0x68 the group path (`Mage Origin`), +0x84 the previous track's name, +0xA0 the current track's name, +0xBC the current theme, +0xD8 the pending track, +0xF4 the pending theme.
- +0x110 the `music` category's volume and +0x118 its pitch offset (read each update from the engine's category table 0x00C3251C), +0x114 the volume given (controller +0x80 × +0x84), +0x11C the main event's designed pitch (FMOD property 3).
- +0x120 a delay in frames for a theme change, +0x124 the `music_state` value, +0x128 the state (below), +0x12C a timer (s), +0x130 the start position (ms) for the new track, +0x134 the old track's position seen, +0x138 the new track's fade factor (0..1), +0x13C the fade-in time in use (ms), +0x140 `combat`'s designed fade-in (ms), +0x144 `main`'s (ms), +0x148 the new track's position at its start, +0x14C a vector of the stream's sync points (ms), +0x15C "return to main when this track ends", +0x15D that flag for the pending track.

#### 2. The theme: set by the area (checked)

- **The field.** An area's `MusicPreset` (string, 0x00B0DB04) is read at 0x006832D1 into area +0x440.
- **Set on load.** The area-load machine (0x006C8EF0; the call at 0x006C937E, after the area's objects are placed) calls the area's 0x0067C160(1): every sound object in the area's list (+0x694) is put in the world (vtable +0x140), then `MusicController::SetTheme(area+0x440, 0)` (0x005BB510).
- **Cleared on leaving.** 0x006C5E40 (called at 0x006C84EE as a load starts) calls 0x0067C160(0) on the current area: its sounds leave the world and the `music_state` is set to 0 with no delay (0x005B6CD0(0, 0)). The area's teardown sets it to 0 too (0x0068498E).
- **SetTheme 0x005BB510 (name, oneShot):**
  - nothing unless the controller is initialised and music is not disabled; if disabled it stops the theme (0x005B7080);
  - **the same name as the current theme (+0x1C) returns at once: the music goes on, not restarted.** The compare is the exact wide string (0x004C0DE0);
  - else the wrapper is made if needed and the theme's SetTheme(name, 5) is called (slot 1, 0x005C0E20); the volume is applied again (0x005B7130); +0x1C = name; +0x92 = oneShot, and when not one-shot +0x38 = name; +0x54 = -1, so the next update applies the mode's track again.
  - Every caller passes oneShot 0 (the area 0x0067C201, the pregame 0x0064CC91, the credits 0x005BBA6E and 0x005BBB4E, the one-shot's own return 0x005BB9D0), so +0x92 is never set in vanilla.
- **The theme's SetTheme, slot 1 (0x005C0E20) (name, delay):**
  - `\` is replaced by `/`; the name is split at the first `/`: +0x4C the project (`music`), +0x68 the group path (`Mage Origin`); no `/` fails.
  - The same as the pending theme, or the current one while not stopped (state ≠ 1): nothing.
  - The pending track is cleared, the pending theme set; with delay > 0, +0x120 = delay and state 9; else every event is stopped (0x005BFEC0) and state 2.
- **Loading the theme (0x005C27A0, from state 2 once nothing plays):**
  - `<project>.fev` is loaded through the event system unless it is the loaded one (its resolved path at +0x04); a newly loaded project has its group `_stingerprecache` loaded (`EventGroup` vtable +4, mode 2: inferred to be loadEventData).
  - The group (+0x68) is got from the project; a missing group or project clears the pending theme and leaves nothing playing.
  - +0xBC = the theme, state 1, the current track (+0xA0) is emptied.
  - `main` and `combat` are got (mode 1): for each, FMOD property 0x22 is set to -60.0, the designed property 0x24 is read (+0x144 for main, +0x140 for combat) and property 0x24 set to 0, and main's property 3 (pitch) kept at +0x11C. By FMOD 4's `FMOD_EVENTPROPERTY` order 0x22 is REVERBWETLEVEL, 0x24 FADEIN and 3 PITCH (inferred from the enum; consistent with the uses: -60 dB of reverb, a fade time in ms the engine then runs itself).
  - The subgroup `stingers` (+0x28) is got, and its `from_main` (+0x3C) and `to_main` (+0x40) events got, paused and started.
  - The stored `music_state` (+0x124) is applied (slot 7).
  - **Nothing plays yet**: a track is set by the controller's next mode change or a pending track.
- **The menu.** `GameModePregame`'s enter (slot 6, 0x0064CB50) sets the theme `music/_menu` (0x00B06C0C). Pregame is mode 8, which then sets the track `main`.
- **The credits** (`GUI::GUICredits`): their start (0x0071FD0B → 0x005BBA20) sets the theme `music/_EndOfGameCredits` when the controller's +0x94 is set (the game's end, `ShowStartMenu(TRUE)`); otherwise the track `chargen` if +0x95, else `credits_df`, in the theme playing (`music/_menu`). Their end (0x0071F660 → 0x005BBB00) sets `music/_menu` again after the end-of-game credits (and clears +0x94), else the track `main`.
- **Restart (theme slot 4, 0x005C4060):** after a load (the controller sees mode 9 left, 0x005BB6D1), the theme is reloaded with its current track, but only if `<project>.fev` now resolves to another file than the loaded one (0x005C0D50; inferred: an add-in or override changed). Normally false: a load does not restart the music.

#### 3. The track: set by the game mode (checked)

`MusicController::Update(dt)` (0x005BB630) runs every frame from the sound manager's update (0x005BD880 at 0x005BDC98), which the app calls with the mode number of the current game mode (0x005D5529; the controller's current mode object's +0x5C, 0x00640060) and dt in seconds. The cutscene player also calls it with mode 5 (0x007DD75E).

1. **The `music_state` delay.** Unless the mode is 9 (loading): if +0x88 ≥ 0, it counts down by dt; at ≤ 0 it becomes -1 and the theme's SetMusicState(+0x8C) (slot 7) runs. So a value asked for during a load waits for its end, and a later writer (a trigger entered as the party is placed) overwrites it.
2. **On a mode change** (mode ≠ +0x54). Leaving 9 runs the restart check above. The pending track's timer is cancelled (+0x7C = -1). Then by the new mode:

| New mode | Track | Condition |
|---|---|---|
| 1 Combat | `combat` | at once; but coming from 3 GUI, 4 Dialog or 5 Cutscene, pending with a delay of **6.0 s** (0x00AFED38) |
| 0 Explore, 8 Pregame | `main` | always |
| 4 Dialog | `main` | unless coming from 5 Cutscene |
| 5 Cutscene | `main` | unless coming from 4 Dialog |
| 2 Dead | `death` | always |
| 11 CharGen | `chargen` | always |
| 3 GUI, 6, 7, 9 Loading, 10 Movie, 12 | no change | |

   Each sets the track through the theme's slot 2 (0x005BFDF0) with (name, 0, 0); +0x54 = mode.
3. **The same mode:** the pending track's timer counts down (s); at ≤ 0 the pending track (+0x5C) is set with its flag (+0x78) and cleared.
4. A one-shot theme that has finished (theme slot 11) returns to +0x38 (never in vanilla, above).
5. The theme's update (slot 3, 0x005C4B10) runs with dt and "in a conversation or cutscene" (mode 4 or 5).

So the scripts' combat state is the music's: `rules_core`'s GM_COMBAT on perceiving a hostile and `module_core`'s return to GM_EXPLORE (dev/TURN_BASED.md) start the combat and main tracks. No script event or `music_state` value marks combat.

**SetTrack, theme slot 2 (0x005BFDF0) (name, returnToMain, force):** with nothing pending, the same name as the current track and state 8 (playing): nothing. Else if force or the name is `death`, +0x15C (return to main) is cleared; the pending track (+0xD8) = name and +0x15D = returnToMain. The switch is made by the theme's update in state 1 or 8, so a track asked for during a switch waits for it to finish.

#### 4. The switch between tracks (checked)

**The switch itself (0x005C2D00), run from state 1 or 8 when a track is pending and +0x15C is clear:**
- Nothing if the name is the current track's. +0x15C is cleared.
- `death`: the event `death` is got from the theme's group if not yet (reverb wet -60, `music_state` applied); it becomes the new track; a stinger is wanted. The fade-in time (+0x13C) is left as it was.
- `combat`: the preloaded `combat` (none: the switch fails); a stinger is wanted; fade-in +0x140 (combat's designed fade-in).
- `main`: `main`; a stinger is wanted only when leaving `combat`; fade-in +0x144.
- **Any other name: an event of that name in the theme's group** (e.g. `bhm_jowan_mage`), got with reverb wet -60 and the `music_state` applied; no stinger; +0x15C = +0x15D (return to main when it ends); fade-in +0x144. A missing event fails and nothing changes.
- The old track goes to +0x48, its name to +0x84; the new to +0x44 and +0xA0.
- **The stinger**, when one is wanted and an old track plays: the playing stinger is stopped at once; `combat` takes `stingers/from_main` (+0x3C), `main` takes `stingers/to_main` (+0x40), `death` gets `stingers/to_death`; reverb wet -60, paused, started (so loaded). A theme without a `stingers` group has none.
- The new track is paused, its fade factor +0x138 = 1.0, state 3.

**The states (+0x128; the update 0x005C4B10, jump table 0x005C5254):**

| State | What runs |
|---|---|
| 1 stopped / 8 playing | (0x005C51C7) if +0x15C and the track has stopped sounding: the pending track becomes `main` (if none) and the track is stopped. Then a pending track (with +0x15C clear) is switched to (above) |
| 2 changing theme | (0x005C512F) waits until the stinger, the track and the old track have stopped; drops the event pointers when the project is set; clears the theme; state 1; loads the pending theme (0x005C27A0) |
| 3 | (0x005C4B43) waits while the stinger is loading; starts the new track (still paused); state 4 |
| 4 | (0x005C4BA0) while the new track is loading, waits up to **12 s** (0x00AFF8A4). If it is not yet sounding and another track is now pending, **the switch is abandoned**: the new track and the stinger are stopped at once and the old track stays (state 8). Otherwise it waits up to 12 s, then stops the stinger. Then the start position (below); state 5 |
| 5 | (0x005C4E79) waits (at most 12 s) until the old track's position reaches +0x130. Then, if the old track still plays, the stinger's reverb is turned off and it is unpaused (and stopped instead in a conversation or cutscene); the new track's fade factor is set to 0.0001, its reverb off, it is unpaused, its position kept (+0x148); state 6 |
| 6 | (0x005C5015) waits (at most 12 s) until the new track's position moves past +0x148; stops the old track (`Event::stop(false)`: FMOD's own fade-out, inferred); state 7 |
| 7 | (0x005C50C2) waits until the stinger has finished, stops it; state 8 |
| 9 | (0x005C50FD) a theme change's delay: counts **frames** (+0x120, one per update); at 0, stops every event, state 2 |

**The start position: the markers (state 4, checked).** Only when both the new and the old track are `main` or `combat`, and not leaving `combat` in a conversation or cutscene:
- p = the old track's channel position in ms (0x005BF2C0: the first channel of its channel group, `Channel::getPosition`, unit 1 = ms); if p > 0, +0x130 = p + 400.
- The sync points of the new track's sound (0x005C47D0: `Channel::getCurrentSound`, `Sound::getNumSyncPoints`, `getSyncPointInfo`; FSB4 sync points, the extra bytes of the music banks' sample headers) are collected; the first one past p + 400, if less than **15,000 ms** (0x3A98) ahead, becomes +0x130.
- The new track's channels are set to +0x130 (0x005BF470, `Channel::setPosition`, ms).
- So combat starts where main was (and the other way), at the next marker of the shared timeline, the switch waiting for the old track to reach it. Any other pair starts the new track from 0 at once.
- `sound_settings` row 7 ("max time between markers") was not found read: the sound settings' loader (0x005B5F20) reads rows 0, 1, 9, 14, 15, 17 and 20 of table 273 only; the 15 s window and the 400 ms lead are constants in the code.

**The fade (0x005C3E60, at the head of every theme update, dt in s):**
- The `music` category's volume and pitch offset come from the engine's category table (0x00C3251C; the ducking is the engine's own, PARAGON_AUDIO.md section 3).
- Volumes (slot 6, 0x005BFFC0): the playing track gets given × category × fade factor; the old track, the death event, the stinger and the two preloaded stingers get given × category. Pitch (0x005C00B0): main's designed pitch plus the category's offset, on main, combat and death.
- The fade factor, when between 0 and 1, rises by dt / (fade-in ms × 0.001) to 1; a fade-in time below 1 ms makes it 1 at once. So the new track fades in over its designed FADEIN, run by the engine, FMOD's own fade-in having been set to 0.

**Pause (slot 5, 0x005BFF20):** `Event::setPaused` on main, combat, death and the stinger (who calls it: not read).

#### 5. The `music_state` parameter (checked)

- The parameter's name is **`music_state`** (0x00AFF3F8; the `@` before it in a dump is the preceding byte). It is set with `Event::getParameter` and `EventParameter::setValue` (0x005C0110).
- **Theme slot 7 (0x005C0150):** stores the value (+0x124) and, unless a theme change is under way (state 9 or 2), sets it on `main`, `combat` and `death`. A named track gets the value when it is made, not later.
- **Its writers**, all through the controller's pending value (+0x8C) and delay (+0x88), applied by its next update after the delay:
  - **`SetMusicIntensity(n)`** (486, 0x0065D070 → 0x005B6CD0): value n, delay 0. No clamp.
  - **A music-volume trigger** (`CCTriggerAudioMusicVolume`; fields read at 0x00799AD0: `EnterState` int +0x510, `ExitState` int +0x514, `EnterStateDelay` int ms → +0x518 s, `ExitStateDelay` int ms → +0x51C s). Entering (vtable +0x1E4, 0x00799380): value EnterState after EnterStateDelay; +0x520 = inside. Leaving (0x00799A80): value ExitState after ExitStateDelay, but not while loading (mode 9).
  - **`SetMusicVolumeStateByTag(tag, n)`** (488): sets the trigger's EnterState to n and, if the player is inside it, applies it at once (with its EnterStateDelay).
  - **Leaving an area**: 0, no delay (above).
- A value set during a theme change is applied when the new theme loads.
- What the values do is the project's: the layers or effects of `main`, `combat` and the named tracks over `music_state` (section 8).

#### 6. `#music:` and the scripts' music (checked)

- **It is a parameter's name, not a user property** (correcting PARAGON_AUDIO.md section 2; `#duck:` and `#pause:` too). A line's event has a parameter named e.g. `#music:bhm_mage_end` (range 0..1). When a sound event instance is made, 0x005B9750 walks its event's parameters by index (0x005B6720 → `CContent_FMOD` slot 29, 0x005BF7A0: `Event::getParameterByIndex`, `EventParameter::getInfo` for the name, `getRange`) and parses each name. A name holding `#music:` (0x00AFEC38) is parsed (0x005B9D20..0x005B9FDF) into the instance:
  - spaces are removed (0x00AF0CA4); **+0x98 the track** = up to the first `,`;
  - **+0xB4 = 1 when the second field n > 0 and the track is neither `main` nor `combat`**, else 0;
  - **+0xB8 = the third field / 1000**, a delay in seconds (0 when absent).
- **On the instance's start** (0x005BAC80, at 0x005BACD7..0x005BAD02): a nonempty track goes to the controller's PlayTrack (0x005B70C0): with a delay > 0 it becomes the pending track (+0x5C, its flag +0x78, its delay +0x7C), set after that many seconds of one game mode (a mode change cancels it); otherwise it is set at once, SetTrack(track, flag, 0).
- **So n is "return to main": a nonzero n makes a named track hand back to `main` when its event ends (+0x15C); n = 0 leaves it, and a non-looping track then ends in silence until the next mode change asks for a track. ms is the delay before the switch, after the line starts.**
- **A conversation's end returns to main:** 0x007CF2E0 (from the dialogue code, 0x0068DCA8 and 0x00691E23) steps a list of times (+0x194); when it has none left and the theme's +0x15C is set, it calls SetTrack(`main`, 0, force 1) (0x005B73F0). (Checked as code; that the object is the conversation's line timer is inferred from its callers.)
- **PlayMusic (484) and StopMusic (485) do nothing** (one handler, 0x0065CFD0: pops the string, returns; %TEMP%\lib-agent-2-scg-sounds.md).
- **What the scripts have instead:** `SetMusicIntensity` (the `music_state`), `SetMusicVolumeStateByTag` (a trigger's state), the game mode (GM_COMBAT and the rest), conversation lines' `#music:` and the area's `MusicPreset`. There is no command that sets a theme or a track by name.
- **The ducking events** (0x005BCCD0, from the sound manager's update at 0x005BD481): when the track or its playing changes, track `main` playing with the music volume > 0 plays `audio_categories/ducking/OnExploreMusic` and stops `OnCombatMusic`; `combat` the other way; any other track, music off or nothing playing stops both. The options' change (0x005BC6F6..) stops both when music goes to -60 dB. `OnCutscene`, `OnDeath` and `OnGameLoading` are played by mode changes in 0x005BD220 (a jump table over the mode at 0x005BD27F; question 12).

#### 7. Edge cases that follow from the code (checked as code; not seen in play)

- **An empty `MusicPreset`** (10 areas, e.g. `cam900ar_endgame`, `char_stage`): the theme's SetTheme finds no `/` and returns before stopping anything, so **the previous theme plays on** (the controller's own name becomes empty, so the next area's theme is always taken).
- **A `MusicPreset` naming no group** (`music\city_elf`, `music\pre_camp`, `music\pre_kwilds`): the old theme is stopped, the load fails, nothing plays until another theme.
- **A track with no event** (16 cutscene switch tracks such as `bhm_intro`; `#music:44041_city_elf`, `#music:branka_in`): the switch fails and the pending track is dropped: the music does not change.
- **`music_state` 0** (the leaving value, and the value when no trigger holds the party): `main`, `combat` and `Death` have no sound at 0 (section 8), so the theme is silent although its events play. An area's music therefore needs a music-volume trigger around the party (209 of 240 areas have one).
- **`Death` is spelt so in the data; the exe asks for `death`.** FMOD's lookup by name is taken to be case-insensitive (inferred; otherwise there would be no death music).
- **A track asked for while a switch runs** waits until state 8. A quick flip (combat, then explore before combat sounds) abandons the switch and keeps the old track (state 4).
- **Combat after a conversation, a cutscene or a menu** waits 6 s of combat mode before the combat track; a mode change in those 6 s cancels it.

#### 8. The data (from %TEMP%\lib-agent-2-au3-data.md, a subagent's reading; checked in the files unless marked)

That file has the FEV1 layout (a reader, `lib-agent-2-au3d-fev.py`, parses all 9,020 `.fev`), every `music.fev` event and sound definition (`lib-agent-2-au3d-compact-music.txt`), and every `#music:` parameter in every language. The essentials:

- **`MusicPreset`** is a CExoString of the `.are` (GFF3), `music\<group>`. No other music field in `.are`, `.arl`, the area list or the campaign. 82 distinct values over 240 areas.
- **A theme group** of `packages/core/audio/sound/music.fev` holds `main`, `combat`, `Death`, `stingers/{from_main,to_main,to_death}`, and the theme's own tracks and hits. Banks: `music_notstream_sting` (in memory: stingers, hits, the end credits), `music_bank_strm_new` (streamed: themes and tracks), `music_bank_strm_death` (streamed).
- **Fades** (event fade-in/out, ms): `main` 4000/2000, `combat` 2000 (some 1000)/1000, `Death` 1000/0, tracks 0 or 500 / 1000..6000, stingers none. These are the fade-ins the engine runs itself (+0x140, +0x144) and the fade-outs FMOD runs on `stop(false)` (inferred).
- **`music_state` in the data:** `main`, `combat` and `Death` each have one layer on it (range -1..N, N 5 to 15), an instance per integer k = 1..4 covering about [k-0.1, k+0.85]; **-1 and 0 play nothing**. Single-track events (`bhm_jowan_mage`, `_menu/main`) cover the whole range.
- **`#music:` fields in use** (en-us VO, 143, all in `modules/single player/audio/vo/en-us/vo/`): `<track>` 121, `<track>,1` 18, `<track>,0,2000` 2, `<track>,0,6000` 1, `<track>,1,4000` 1. The cutscene switch events (`music.fev` group `cutscene`, 60, and `music_cutscene.fev`, 49) carry `<track>` or `<track>,1` and have no sound. With section 6: `,1` = return to `main` when the track ends; the third field = a delay in ms before the switch.
- **Triggers** (`Type` 5 in an area's `TriggerList`; no `.utt`): `EnterState`, `EnterStateDelay` (ms), `ExitState`, `ExitStateDelay`, `Geometry`. 227 in 209 areas; states 1..6 and one 10; tags `castle_1`..`4`, `end_plot`, `save_elves`, `proving_music`, `miniboss` are what `SetMusicVolumeStateByTag` names.
- **The scripts:** `PlayMusic` once (`area_core.nss`:62, from `music_projects` by tag: almost surely `""`; a no-op anyway), `StopMusic` never; `SetMusicIntensity` 6 times (0..4: the cutscene end scripts `gen00cs_cs_end_music_int_0..4`, and 0 at Cailan's charge); `SetMusicVolumeStateByTag` 7 times (`end_plot` 4, `save_elves` 2, `castle_1..4` 3). No music call in combat, explore or death handling.
- **Ducking:** `OnCombatMusic` ducks ambient and amb_oneshots -4 dB and vfxamb_combat -60; `OnExploreMusic` amb_oneshots -4; `OnDeath` has no parameters (ducks nothing); `OnCutscene` ducks music -3 dB and ambience more. The stinger hits duck `music` -15 to -30 dB through their own `#duck:music,...`.
- **`sound_settings`** row 7 "max time between markers" 15000 and row 10 "delay with switching music themes" 5 frames are marked "NOT IMPLEMENTED YET" by the designers; the exe has the same numbers as constants (sections 2 and 4). `internal_soundsettings` row 8 "music precache time 400 ms (time to delay switching on next marker)" is the exe's 400 ms lead.

#### 9. The mage origin

| Area | `MusicPreset` | Trigger state | `main` plays | `combat` | `Death` |
|---|---|---|---|---|---|
| `bhm100ar_tower_level_1`, `bhm200ar_tower_level_2` | `music\Mage Origin` | 3 | `DA_music_mage_origin_a` (5 waves, 123 s) | `DA_music_Tower_c` | `da_death_tower` |
| `bhm250ar_spider_cave` | same | 2 | `DA_music_nb_cavern_a` | `DA_music_nb_undead_c2` | `da_death_forest` |
| `bhm500ar_tower_harrowing`, `bhm600ar_fade_harrowing` | same | 1 | `DA_music_fade_a` (the Fade) | `DA_music_fade_c2` | `da_death_fade` |
| `bhm700ar_tower_basement` | same | 4 | `DA_music_mag_dungeon_a` | `DA_music_IM_dungeon_c` | `da_death_tower` |

All triggers have delay 0. Stingers: `from_main`, `to_main`, `to_death` all play one random hit of 18 (`/music_ingame/DA_m_tran1`, `music_notstream_sting`). Tracks: `bhm_jowan_mage`, `bhm_mage_end`, `bhm_jowan_atk`; hit `wall` (category `stinger`, ducks music -15 dB, named by a cutscene).

What plays, by the code and the data together (inferred as a sequence; each step checked):
1. The first bhm area loads: theme `Mage Origin` is loaded (5 frames after the request, once the old music has stopped). Loading is mode 9: no track yet, `music_state` held.
2. The load ends, mode 0: track `main`, from 0 (no old track, no stinger), fading in over 4 s. The party is placed inside the area's trigger: `music_state` 3 (the tower theme), applied at the next update.
3. Moving between the six areas: the same theme, so the music is not restarted; leaving sets `music_state` 0 and entering the next area's trigger sets its state, so `main` changes piece (tower 3 → caves 2 → Harrowing/Fade 1 → basement 4) at the area change. The change of piece is FMOD's, inside the one event (how FMOD crosses layers: not read).
4. A fight (GM_COMBAT): `combat` in the same state, started at main's position + 400 ms snapped to the next marker within 15 s, after `from_main`'s hit; back to explore: `main` the same way with `to_main`.
5. A conversation or cutscene from explore: `main` stays. From combat: `main`, no marker sync, no stinger.
6. Cutscene switch events and VO lines: `bhm_jowan_mage_swt` → `bhm_jowan_mage` (plays once, no return: silence after it until a mode change asks for a track; but a conversation's end forces `main` only when the track returns, so here it does not); `bhm_jowan_atk_swt` (`,1`) → `bhm_jowan_atk`, back to `main` when it ends; `bhm_intro_switch` and `bhm500cs_harrowing_switch` name nothing in `Mage Origin` → no change (the intro's music is its own project `bhm_intro.fev`, category `cutscene`, which ducks music -60 dB). `bhm100_jowan/VO/36641_m` `#music:bhm_mage_end` → `bhm_mage_end` at once, no return.
7. The party dies: `death` (the data's `Death`) after `to_death`, in the area's state, not looped.

#### Not read

- Who calls the theme's pause (slot 5) and whether music pauses with the game (`OnGamePause` ducks music -9 dB: data).
- How FMOD changes piece inside an event when `music_state` moves (crossfade, wait for the end of the playing sound, or a cut): FMOD's, and the instance fields "loopmode" and the u32 after it are not decoded.
- `_stingerprecache`'s parameter `param02` and what the load mode 2 does; `stingers` vs the theme's own `stingers/hit*`.
- The controller's +0x95 (a pregame menu index) and the credits' `chargen` choice; who starts the `_EndOfGameCredits`.
- 0x005BD220's jump table (which mode plays `OnCutscene`, `OnDeath`, `OnGameLoading`): question 12.
- That 0x007CF2E0's object is the conversation's line timer (its callers 0x0068DCA8, 0x00691E23 are in the dialogue code).
- The step of the area-load machine 0x006C8EF0 that holds the call at 0x006C937E, and when exactly the party enters the trigger relative to it.
- FMOD property indices 0x22 and 0x24 by FMOD 4's enum (inferred REVERBWETLEVEL and FADEIN).
- Awakening's and the add-ins' `music.fev`, the enciphered add-in areas.

### The listener, the interface's sounds, the ducking events and sound_settings (PARAGON_AUDIO questions 1, 11, 12) (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (subagent au2) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py`, a linear disassembly, `fmod_event.dll`'s exports, ghilana's tables, `audio_categories.fev`'s strings, `gui.fsb`'s sample headers and FFDec's decompile of `inventory.gfx` (patch003's). Not seen in play. "(checked)" = read in the disassembly or the data; "(inferred)" = not.

Names used below:
- **The sound manager**: the singleton from getter 0x005B9020 (global 0x00C323B4), as in `lib-agent-2-scg-sounds.md`.
- **The renderer**: `EclipsePrivate::CContentRenderer_FMOD` (vftable 0x00AFF20C, 26 slots, constructor 0x005BE410), at manager+0x90, made by 0x005BDD40 from the manager's init 0x005BD660. (checked)
- **FMOD's objects**: `FMOD::System*` at global 0x00C32508, `FMOD::EventSystem*` at 0x00C3250C (from `FMOD_EventSystem_Create` at 0x005C5522 and `getSystemObject`, in the renderer's init 0x005C54B0). (checked)
- `EventSystem` vtable slots, read from `fmod_event.dll`'s C wrappers: +0x00 init, +0x1C getSystemObject, +0x24 load, +0x2C getProject, +0x38 getCategory, +0x44 getGroup, +0x48 getEvent, +0x5C setReverbProperties, +0x64 getReverbPreset, +0x74 setReverbAmbientProperties, +0x7C set3DNumListeners, **+0x84 set3DListenerAttributes**. `EventCategory`: +0x18 stopAllEvents, +0x1C setVolume, +0x20 getVolume, +0x24 setPitch, +0x2C setPaused, +0x34 setMute. (checked)

#### 1. The listener (Q1)

##### The FMOD call
- **Only the renderer calls `set3DListenerAttributes`**, listener 0, in two of its slots (checked; no other `+0x84` call through the event system):
  - **Slot 6, 0x005BE790 `SetListenerPosition(const vec4 *p)`**: stores p at renderer+0x10, then `set3DListenerAttributes(0, &p, NULL, NULL, NULL)`.
  - **Slot 7, 0x005BE7F0 `SetListenerOrientation(const vec *fwd, const vec *up)`**: only when |fwd·up| ≤ 0.001 (0x00AE7110 = 0.001, 0x00AFF304 = −0.001; else nothing), normalizes both (0x00451A30), stores them at renderer+0x20 / +0x30, then `set3DListenerAttributes(0, NULL, NULL, &fwd, &up)`.
  - Both do nothing until the byte 0x00C32510 (FMOD up) is set and the event system exists.
- **Velocity is never passed** (always NULL), so the listener has no velocity and no Doppler from its motion (checked as code; the FMOD meaning of NULL, "leave unchanged", inferred from FMOD's API).
- **Init flags**: `EventSystem::init(maxchannels = internal_soundsettings row 1 (384), flags 0x88, ...)` at 0x005C5852. 0x88 has no `FMOD_INIT_3D_RIGHTHANDED` (0x2 in FMOD Ex 4.x), so FMOD works left-handed (checked flags; bit meanings from FMOD Ex's header, inferred).

##### The sound manager's forwarders
- 0x005B6390 **`SetListenerPosition(cam, player)`** (`ret 8`, checked):
  ```
  p = cam;                                  // player == NULL: the camera itself
  if (player) p = player + (cam - player) * b;   // b = manager+0x38 = sound_settings row 17 = 0.17
  if (p != manager+0x00) { manager+0x00 = p; return 1; }   // only stored here
  ```
- 0x005B6440 **`FlushListenerPosition()`**: if manager+0x00 differs from the renderer's last (+0x10), renderer slot 6 with it (0x004D4EF0 is "vectors differ"). (checked)
- 0x005B64C0 **`SetListenerOrientation(const quat *q)`**: `fwd = rotate(q, (0, 1, 0))`, `up = rotate(q, (0, 0, −1))` (0x004CB6D0 is rotate-by-quaternion, q = x, y, z, w; −1.0 at 0x00AE2E00), then 0x005B64A0 → renderer slot 7, every call. (checked)
  - **The up vector is the camera's −Z.** With FMOD left-handed and the engine's world right-handed (Z up), a flipped up keeps left and right correct: FMOD's right = up × forward, which for up −Z and forward +Y is +X (inferred; the reason is not written anywhere, the vectors are checked).

##### Where it is called: every frame, by mode
**`CClientExoApp::FrameUpdate` 0x005D7560** calls **0x005D6610** first thing (at 0x005D75A3), before the game mode controller's update (0x00641F90) and before the sound manager's update (0x005D54B0 at 0x005D7776). So the listener uses the camera of the frame before (checked order; the one-frame lag inferred).

0x005D6610 (checked), with `mode` = the game mode controller's current mode (app+0x114, 0x00640060; values from `GameModes.h`):

| Mode | Position | Orientation |
|---|---|---|
| 9 Loading, 8 Pregame, −1 | (0, 0, 0): `SetListenerPosition(&zero, NULL)` | unchanged |
| 4 Dialog, 5 Cutscene | not computed here: only `FlushListenerPosition` (the cutscene step sets it, below) | not set here |
| 6 FlyCam, 12 PartyPicker | the camera's position | the camera's |
| every other: 0 Explore, 1 Combat, 2 Dead, 3 GUI, 7 Fixed, 10 Movie, 11 CharGen | `player + 0.17·(camera − player)` | the camera's |

- **The camera** is `app+0x114 → +0x74 → +0x58` (0x005D5000): its position at +0x70, its rotation quaternion at +0x90..+0x9C. It is the rendered camera, whatever drives it (explore, tactical, fly), not `CameraControllerExplore`'s focus (checked offsets; "the rendered camera" inferred from FrameUpdate reading the same +0x70/+0x90 for the view right after, 0x005D75C1).
- **The player** is the controlled creature (0x005D5CC0, `[[app+0xD0]+0x5C]`): its position +0x120 (the feet) **plus 1.8 m in z** (a constant, the double 1.8 at 0x00B004D8; not the creature's height). No controlled creature: the camera alone. (checked)
- **So in explore**, close in: the listener is 17% of the way from a point 1.8 m above the leader's feet to the camera: about 0.6 m behind the head at the close zoom (3.8 m), about 3.6 m toward the camera in the tactical view (21.3 m). Facing is always the camera's. (formula checked; the distances worked from `dev/CAMERA.md`)
- **Then the update writes it**: position only when changed (0x005B6440); orientation every frame.

##### Cutscenes and conversations
- **The cutscene step 0x007DD4C0** (the player's per-frame step; `lib-agent-2-cutscenes.md`) does, when it has a camera (+0x1D8), each step (checked, 0x007DD6C5..0x007DD75E):
  1. `manager.SetListenerPosition(camPos, NULL)`: the cut camera's own position, no balance toward any creature. camPos is 0x007CDD30: the world position of the camera object whose id is at +0x1C0, or the camera's own (+0x154) when that is −1.
  2. `manager.SetListenerOrientation(camRot)`: 0x007CDEA0, the same object's rotation.
  3. Only when the player's +0x94 is 5: `FlushListenerPosition` and `manager.Update(5, dt)` (0x005BD880). What +0x94 = 5 means was not read (states 3 playing, 6 failed, 7 are read elsewhere).
- In Dialog (4) and Cutscene (5) the frame's own code only flushes, so **the listener follows the cut's camera**, which a conversation's line cuts drive as well (that conversations' cuts run through this step: inferred from `lib-agent-2-cutscenes.md`). A conversation with no cut playing leaves the listener where it last was (inferred).

#### 2. The interface's sounds (Q11)

- **The movies call an FSCommand.** Every movie's `ExternalCommands` class (FFDec, `inventory.gfx`; the same pool strings are in all 57 movies checked) has:
  ```
  static function PlayGUISound(a_nSoundType, a_sEventName) {
     if (a_sEventName == undefined || a_sEventName == "") a_sEventName = SoundEvents.ET_CLICK;   // "Click"
     fscommand("PlayGUISound", a_nSoundType + " " + a_sEventName);
  }
  ```
  `StopGUISound` is the same with `"StopGUISound"`. `SoundEvents.ST_*` are the rows (ST_BUTTON 1, ST_ERROR 3, ST_CHECKBOX_CLICK 87, ...; ST_CRAFT_RUNE 20001 and ST_OPEN_CRAFTING_RUNE 20000 have no row), and `ET_CLICK = "Click"` is the column. Buttons send `PlayGUISound(this.m_nSoundType, "Click")` on press or release (`Behaviour/PushButton.as`). (checked)
- **The exe's handlers** are registered by `GuiCommands::RegisterFSCommand` (0x005201C0) at 0x00522235 and 0x0052226C (checked):
  - `PlayGUISound` → **0x0051BB60** `(owner, args)`: the row is the integer before the first space, the column the rest; then 0x00522C30(row, column).
  - `StopGUISound` → **0x0051BC50**, the same parse; then 0x00522DC0(row, column).
- **0x00522C30 `PlayGUISound(row, column)`**, `ret 8`, on the singleton from 0x00510CA0 (checked):
  1. A negative row does nothing.
  2. The column name is trimmed of `" \t"` (0x004C3310, the string 0x00ADEE0C) and hashed as GDA columns are.
  3. Table 152 (`sound_gui`), `GetString(row, column)` (the table's vtable +0x20). If that is empty, row 0's (`default`, empty) instead, so nothing.
  4. A non-empty string is the FMOD event path as is: `manager.PlaySound(path, NULL, 0, 0)` (0x005BC7F0).
- **The event path** is the `Click` column verbatim: `glo_gui/gui/<event>` (project `glo_gui.fev`, group `gui`). There is no prefix, suffix or fallback beyond row 0. 33 of the 126 rows are empty and play nothing (5, 6, 9, 12, 13, 15, 16, 46, 51, 55, 85, 104, 115, 116, and row 0). (checked)
- **It is 2-D:**
  - The position is NULL, and the manager's PlaySound skips the 3-D position when it is NULL (0x005BC886..0x005BC88D). (checked)
  - All 101 samples of `gui.fsb` have FMOD's 2-D mode bit (0x2000) and none the 3-D bit. (checked)
  - The event's own 2-D flag in `glo_gui.fev` was not read.
- **0x00522DC0 `StopGUISound`**: the same lookup, then `manager.StopSound(path)` (0x005BA100: every live instance whose full path matches). (checked as calls)
- **The engine plays rows itself** through 0x00522C30 with column "Click" (string 0x00B05DAC). Row numbers are checked; the callers' roles are inferred from the rows' labels:

  | Row | Label | Caller |
  |---|---|---|
  | 2 | open book | 0x00A5589A |
  | 17 / 79 | unequip / uninstall rune | 0x006BC0FE |
  | 18 | container pickup | 0x006A7150 |
  | 43 | select character | 0x00636BBB |
  | 64 | party picker error | 0x007671ED |
  | 69 | load | 0x006D1EA6 |
  | 70 | save | 0x006D1CCD |
  | 71 | quick save | 0x006D1D62 |
  | 77 | repair | 0x00752D09 |
  | a variable | | 0x006BE41A |

#### 3. The ducking events (Q12)

All of them are started with `manager.PlaySound(path, NULL, 0, 0)`, so they are 2-D, and stopped with `StopSound(path)` (checked). What they do is their user properties, parsed by 0x005B9750 when an instance is made (checked):
- **`#duck:<category>,<attack ms>,<hold ms>,<release ms>,<dB>[,<pitch>]`**, one record per property:
  - attack, hold and release are ×0.001 (0x00AE7118), so seconds;
  - dB becomes gain = 10^(dB/20) (0x005B6110; 20.0 at 0x00AFEBA8; clamped to 0 at ≤ 0.001 and to 1 at ≥ 1);
  - the sixth field is stored raw at +0x20 (the duck's pitch).
- **The duck's envelope** (0x005B6270, per frame per instance; t is reset at each change of state; checked):
  - attack: `gain = 1 − (1 − G)·t/A`, `pitch = P·t/A`;
  - then hold: it holds G **while the event plays**, and counts the hold time only after the event has stopped (flag +0x44, set by the stop hook 0x005B9250 → 0x005B6230);
  - release: `gain = G + (1 − G)·t/R`, `pitch = P·(1 − t/R)`; done at gain 1, pitch 0;
  - starting the event again (0x005B9170 → 0x005B6240) restarts the attack from t = 0.
- **Ducks on a category combine** (renderer slot 12, 0x005C3A80, `SetCategory(name, gain*, volume*, pitch*)`, checked):
  - the smallest gain wins;
  - the pitch of largest magnitude wins;
  - it applies to the category's children too;
  - an empty name sets every category.

  How the duck gain then reaches FMOD's category (volume × duck, then reset each frame) was not read (inferred).
- **`#pause:<category>[,...]`**: the start hook (0x005B9170 → 0x005B7540(cat, 1)) pauses the category: `EventCategory::setPaused(true)` through renderer slot 13 (0x005C1A70). For `ambient`, `vfxamb_combat` and names starting with `music`, it also pauses the manager's live instances of that category. The stop hook unpauses (0x005B930F, (cat, 0)). The numbers after the name are not used on this path (checked calls; "not used" inferred).

##### Who starts and stops each
**The sound manager's state is the game mode.**
- `manager.Update(mode, dt)` (0x005BD880) is called every frame by 0x005D54B0 (from FrameUpdate) with the game mode controller's mode. It sets +0x60 = old, +0x5C = new, then runs 0x005BD220.
- 0x005BD220 works from two jump tables on `state − 2`: entry at 0x005BD618, exit at 0x005BD638 (checked):

| Event | Started | Stopped |
|---|---|---|
| `audio_categories/ducking/OnDeath` | entering mode 2, Dead | leaving mode 2 |
| `.../OnCutscene` | entering 4 Dialog (unless coming from 5), or 5 Cutscene (unless coming from 4) | leaving 4 (unless going to 5), or 5 (unless going to 4) |
| `.../OnGameLoading` | entering 9 Loading | leaving 9 |
| `.../OnExploreMusic` | the music's track is `main`, it plays and its gain is above 0 (stops OnCombatMusic) | the track changes to another, or stops, or the music option is ≤ −60 dB |
| `.../OnCombatMusic` | the track is `combat`, it plays and its gain is above 0 (stops OnExploreMusic) | the same |
| `.../OnGamePause` | `CClientExoApp::SetPaused(true)` (0x005D6740) in modes 0, 1, 2, 4, 5 | `SetPaused(false)`, in any mode |
| `.../OnMovie` | `ScriptBackend::ForScript_PlayMovie` (0x006DC680, after `SetGameMode(10)`), and a cut's `ActionPlayMovie` start (0x007CE7CB) unless the player's +0x94 is 5 | the movie queue empties (0x006DC0BA), or the cut's movie ends (0x007CE8D1) |

- **Dialog and cutscene share one OnCutscene**: a switch between them neither restarts nor stops it. The cutscene step also calls `Update(5)` (§1), and that rule keeps the duck steady if the two alternate (inferred).
- **Music ducks**: 0x005BCCD0, on the music object at manager+0x120, runs every frame from 0x005BD220 (even with no change of mode). It acts only when the track name or the playing flag changes (kept at the object's +0 and +0x93), and compares exact names `main` (0x00AFEBD8) and `combat` (0x00AFED3C). Gain is the object's +0x84, linear. The options' volume function (0x005BC2F0) also stops both when music is set to ≤ −60 dB (0x00AFEE58 = −60.0). (checked)
- **Pause in the GUI mode** (mode 3): `SetPaused(true)` plays no OnGamePause. It pauses the FMOD categories `VOambient`, `VOconversation` and `VOnarration` instead (0x005B7540(cat, 1)), which `SetPaused(false)` unpauses in every mode (checked). Pausing in modes 6–12 does neither.
- **Each mode change** also sets category `gui`'s duck to gain 1, pitch 0 (0x005BD52E). It then runs the `/CategoryDucking/` events (made by 0x005BCF60 from `audio_categories`, kept in the map at manager+0xF4): each one is started while its count is > 0 and stopped at 0. The count is meant to track the sounds playing in that category: inferred, the increments were not read. (checked as code)
- **The name `OnGameLoading`**: the exe asks for `OnGameLoading`, and the project has `OnGameloading` (lower-case l). Whether FMOD's lookup ignores case was not read.

##### What each does (from `audio_categories.fev`'s strings, checked)
Format: category attack/hold/release ms, dB.
- **OnCutscene**: ambient 500/0/1000 0; vfxambient 2000/0/2000 −12; VOambient 500/0/500 −12; amb_oneshots 500/0/500 −6; vfx 500/0/500 −16; dog_ambient 500/0/500 −60; music 1000/0/1000 −3; CRambient_ss 33/0/33 −9; rat_ambient 33/0/33 −60.
- **OnDeath**: no user property, so no duck or pause by this mechanism. Its own content, if any, was not read.
- **OnCombatMusic**: ambient 1000/66/2000 −4; amb_oneshots 1000/66/2000 −4; vfxamb_combat 500/0/1000 −60.
- **OnExploreMusic**: ambient 1000/66/2000 0; vfxambient 1000/66/2000 0; amb_oneshots 1000/66/2000 −4.
- **OnGamePause**:
  - #pause: movement, footsteps, VOambient_ss, vfx, VOambient, dog_main (each "33,0,33,−6");
  - #duck: vfxambient −9, ambient −6, unducked −6, music −9, placeables −9, amb_oneshots −6, CRambient_ss −6 (all 33/0/33).
- **OnMovie**: no user property found between its name and the next event's.
- **OnGameloading**: movement, footsteps, vfx, vfxambient, ambient, unducked, amb_oneshots, placeables: 200/0/200 −60.
- **OnFadeCutscene** (the exe names it nowhere: unused): footsteps 33/0/33 0; ambient 66/0/333 −6; vfxambient 500/0/500 −6; VOambient 66/0/333 −12; amb_oneshots 66/0/333 −6; vfx 300/0/333 −10.
- **CustomDucking/TestDuck** (footsteps 1,1,1, −20 dB, pitch −0.2) and **TestPause** (#pause:footsteps): tests, which the exe does not name.
- **CategoryDucking/combat**: ambient 33/0/1000 −2; hespith_duck 1000/0/1000 −60.

##### Stealth's −12 dB (row 0)
- **`SetStealthEnabled` (758, handler 0x0066BE50, the case 0x2F6 at 0x0066BEC8) → `CCreature` 0x006A2400(enabled)**:
  - It runs when the flag (creature+0x7F0) changes, or always during Loading (manager state 9).
  - On: dB = manager+0x28 = **row 0 (−12)**. Off: dB = 0.
  - That dB goes to the creature's four sound emitters (+0x948, +0x94C, +0x95C, +0x960) through 0x005B9450 (`instance.SetVolume(10^(dB/20))`, content vtable +0x4C). (checked)
- **Which sounds**: the four emitters are the instances the creature's animation-event handler (0x006B6290) plays footsteps and movement on:
  - +0x948/+0x94C at most one every 560 ms (0x230, timestamps +0x940/+0x944);
  - +0x95C/+0x960 alternately, at most one every 260 ms (0x104, +0x964, toggle +0x968).

  So **stealth quiets that creature's own footsteps and movement foley by 12 dB, not the categories**, and any creature's, not only the player's. Matching emitters to footsteps and movement is inferred: the 560/260 ms match rows 3 and 4's footsteps/movement values, and row 0's comment says so. Whether the −12 dB survives a new event name on the emitter (0x006AA940 → 0x005BA530) was not read.
- Turning stealth on also posts a message with the tag `'GAME' 'HTSG'` when 0x00838210 on creature+0x4F8 is true (unread).

#### 4. Every sound_settings.gda row (table 273) and what the exe does with it

**Only 0x005B5F20 reads table 273** (from the manager's init 0x005BD660; no other `push 0x111` table read exists; checked). It reads rows 0, 1, 9, 14, 15, 17 and 20, column `Value` (hash 0x3FC13DE1), into a settings block at manager+0x28. The block is handed to the renderer's init (0x005C54B0) and copied, 11 dwords, by the event resolver (0x005BA656) and start (0x005BACC2). All other rows are never read. (checked)

| Row | Label (Value) | The exe |
|---|---|---|
| 0 | Stealth Ducking (−12) | manager+0x28: the stealth volume on a creature's footstep and movement emitters (§3). Checked. |
| 1 | Max ambient Soundset VO playback (5) | manager+0x34 (`ftol`). In the resolver (0x005BA66C..0x005BA691): when it is ≥ 0, the content's slot +0xAC says yes (inferred: the event is in VOambient, VOambient_ss or CRambient_ss), and the manager's +0x58 (inferred: the count playing) is ≥ the limit, the new sound is not started. Checked compare; the two meanings inferred. |
| 2 | occlusion blending speed (1.2) | not read |
| 3 | min time between footsteps (560) | not read; but **560 ms is hard-coded** in the creature's animation-event handler (0x006B65E5, `lea eax,[edi-0x230]`) |
| 4 | min time between movement (260) | not read; **260 ms hard-coded** the same way (0x006B666E, `-0x104`) |
| 5 | force minimum fadeout (34) | not read |
| 6 | fade out for sounds stopped by animation (400) | not read |
| 7 | max time between markers (15000) | not read ("NOT IMPLEMENTED YET", and true) |
| 8 | fade out for max playback hack (100) | not read |
| 9 | Use Fmod profiler (0) | manager+0x30 = 1 when > 0. Where it is used was not found. |
| 10 | delay switching music themes (5) | not read |
| 11 | occlusion test frequency (100) | not read |
| 12 | audio volume, update on min. distance (0.25) | not read |
| 13 | audio volume, update on min. time (0.06) | not read |
| 14 | Sound radius multiplier for conversations (1.8) | manager+0x2C. In the start 0x005BAC80, below. Checked. |
| 15 | High quality resampling (0) | manager+0x31 = 1 when > 0. Init: `setSoftwareFormat(48000, PCM16, 0, 0, resampler 2)` when set (0x005C578A..0x005C57A4); a "SigmaTel" driver gets (48000, 5, 0, 0, 1) whatever the row. Checked; resampler numbers per FMOD Ex, inferred. |
| 16 | Reverb blend time (0.33) | not read |
| 17 | Camera / player listener balance (0.17) | manager+0x38: the listener's `player + (camera − player)·0.17` (§1). Checked. |
| 18 | Max ambient VO playback (1) | not read |
| 19 | Reduce reverb memory usage (0) | not read |
| 20 | force 7.1 to 5.1 (1) | manager+0x3C = 1 when > 0. Init: a 7.1 driver (speaker mode 6) is set to 5.1 (5) (0x005C5636..0x005C5645). Checked. |
| 21 | (empty) | not read |

**Row 14 in the start** (0x005BAC80):
- The conditions:
  - the start's second argument is set;
  - the manager's state is 4 Dialog or 5 Cutscene;
  - the event's category has no `VO` in it (0x00AFEC10);
  - it is not `CRambient_ss`, `dog_main` or `dog_ambient`;
  - its multiplier +0x34 is still 1.0.
- Then: multiplier = 1.8, and 0x005B6AF0 calls the content's vtable +0x70 with `instance+0x10 × 1.8`. That this is the 3-D distance (rolloff) is inferred from the row's comment.
- In both cases (rolloff applied or not, once past the mode and VO checks) the event's parameter `cutscene` is set to 1.0 (0x005B66B0 with 0x00AFEC68).
- Creatures' emitters start with this argument 1 (0x006B6590). Script `PlaySound` passes its own fourth argument, 0. (checked)

**The same block's other fields come from `internal_soundsettings.gda` (table 274)**, read by 0x005B5F20 too (checked):

| Row | Label (Value) | Field | Init |
|---|---|---|---|
| 0 | Real Channels (64) | +0x40 | `setSoftwareChannels` |
| 1 | Virtual Channels (384) | +0x44 | `EventSystem::init`'s maxchannels |
| 5 | max ADPCM codecs (60) | +0x4C | `setAdvancedSettings` |
| 6 | max MP3 codecs (12) | +0x50 | `setAdvancedSettings` |
| 10 | stream buffer size (131072) | +0x48 | `setStreamBufferSize(size, RAWBYTES)` when ≥ 0x2000 |

Rows 2–4 and 7–9 ("NOT IMPLEMENTED") are not read.

#### Not read
- Whether FMOD's event lookup ignores case (`OnGameLoading` against `OnGameloading`), and how 0x005BA530 walks a path.
- How a category's duck gain and pitch reach FMOD each frame (volume × duck, the reset), and the `/CategoryDucking/` counts' increments.
- `#pause`'s numbers (stored or ignored); `OnDeath`'s and `OnMovie`'s own sound content in the `.fev`.
- The cutscene player's +0x94 = 5 meaning, and which conversations run their lines through the cutscene step.
- 0x005D54B0's area call 0x00682020 with the listener position (inferred: the reverb at the listener).
- Row 9's use (the profiler); content vtable +0x70's exact FMOD property (row 14).
- 0x0076C0B8/0x0076CFD5: something in the 0x76Cxxx camera code pauses and unpauses category `ambient` (unread).
- `glo_gui.fev`'s events' own 2-D/3-D flags; the engine-side GUI sound callers' contexts beyond their row numbers.
- Whether the stealth volume survives the emitter's event being replaced.

### Ambience: sound objects, projects, audio triggers (PARAGON_AUDIO.md questions 5, 6, 7) (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (subagent au4, with two forks: au4q6 for Q6, au4q7 for Q7) in the GOG exe (`C:\project\DAOLib\game\DAOrigins.exe`, 1.05, base 0x400000), `fmod_event.dll` (FMOD Ex EventSystem 4.24.24), the tables through ghilana and the `.fev` files on disk, for DAOParagonEngine #30. Not seen in play. Each claim is marked "(checked)", read in the disassembly or the data, or "(inferred)". Builds on `%TEMP%\lib-agent-2-scg-sounds.md`; the music and the listener are `%TEMP%\lib-agent-2-audio-music.md` and `%TEMP%\lib-agent-2-audio-listener.md`.

#### In short

- **Q5, sound objects.** An area's `SoundList` entry is a `CCSoundObject`; a sounds region's is a `CCSoundObjectPannable`. Its FMOD event is made when the area's load ends (step 8, after POSTLOADEXIT) and played on its first AI tick while `Active`; there is no range test, distance is FMOD's. With `MaxDistanceMult` > 0, the event's own properties are changed: volume ×10^(Volume/20) (Volume ≤ 0 only), pitch + Pitch, min and max distance ×MaxDistanceMult, cone angles and FadeIn/FadeOut replaced, cone outside volume ×10^(ConeVolume/20). `Priority` never reaches FMOD. Occlusion is the engine's own ray (no FMOD geometry): placeables' `OcclusionFactor`, static geometry's `LowOcclusionFactor`/100 by material, combined as 1 − Π(1 − f), smoothed at 1.2 per second, given as `Event::set3DOcclusion(direct, 0)`.
- **Q6, projects.** At start `audio_categories.fev`, then every `sound_projects_to_load.gda` row (re-synced when the sources change). Everything else is loaded on demand by name through a reference-counted map: an area's projects are the first path segments of its sound objects' `Sound` paths (no area-name rule; `tower_level_1.fev` is Ostagar's Tower of Ishal, not the mage origin's); a conversation loads the project named for its resref, a cutscene its actions' sound and VO projects, a creature its sound set's. The music theme loads `music.fev` itself. Nothing unloads by area; a project goes with its last holder. FMOD's files are found loose in folders only, by bare name, in the resource manager's order.
- **Q7, triggers.** `.are` trigger Type 3 is `CCTriggerAudioReverb`, Type 4 `CCTriggerAudioVolume` (a reverb region that also owns sounds), Type 5 `CCTriggerAudioMusicVolume` (not a script region, as `Area.h` says). They act only for the controlled creature. Reverb: the most recently entered occupied region's `master_reverb` preset, else the area's; `Priority` is never read. Music volume: `music_state` set to `EnterState`/`ExitState` after the delay. Save fields 16108 (reverb preset), 16111 (a region's sounds), 16112 (Type), 16113..16116 (music states and delays); 16109 is unused.

#### Cross-checks between the three readings

- The pannable sound's event is made by the same function as the plain one (0x007C2750 calls 0x007C23D0 first; checked), so a sounds region's paths load their projects exactly as an area's `SoundList` does (answers Q6's "Not read" on this).
- The flag the sound object passes beside its project (Q5 step 2) is LoadProject's `precache` (Q6 section 2): `global_amb_*` projects are loaded without precaching their samples, every other area project with. (checked)
- Q7 reads area+0x414 as `MusicPreset` (inferred there); the music note reads `MusicPreset` into area+0x440 (0x006832D1) and that is what the end of the area load passes to the music (0x0067C160 → 0x005BB510(area+0x440, 0), checked here too). What area+0x414 holds (passed to the manager's 0x005B6B80 at 0x006881C9) is not read.
- `src/dao/core/Area.h` needs two corrections for a port: Type 5 is the music-volume trigger, and the music-volume region's `EnterStateDelay`/`ExitStateDelay` are milliseconds in the file (seconds once loaded).

#### Q5. An area's sound objects

Read 2026-10-18 by lib-agent-2 (subagent au4) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py` and capstone, the tables through ghilana, FMOD's version from `fmod_event.dll`'s resource (FMOD Ex EventSystem 4.24.24). Not seen in play. "(checked)" is read in the disassembly or the data; "(inferred)" is not. Builds on `%TEMP%\lib-agent-2-scg-sounds.md` (the sound commands and the sound object's fields), which this corrects in one place (below, "The instance list").

##### The classes and where they come from

- An area's `SoundList` entry is a `CCSoundObject` (0x4B0 bytes; base constructor 0x007C1F10, vftable 0x00B2208C), made by the area's sound-list load 0x0067DA40 (called from the area's object load at 0x0068869A): from the in-memory save's `SavegameAreaSounds` (16019, `push 0x3E93`) when the area has a saved struct, else from the `.are`'s `SoundList` (0x00B0DAC0). Each is loaded (0x007C2BE0), then added to the area (0x007C3110: the area's object list +0x694 through 0x006F1340). (checked as calls)
- A sounds region's (trigger Type 4) `SoundList` entry is a `CCSoundObjectPannable` (0x4E0 bytes, constructor 0x007C2B20, vftable 0x00B223E4), owned by the region (+0x4B8) and placed at the region's position (Q7's section). It differs from the plain class in three slots only (checked by diffing the two vtables): +0x44 (a cast answering `this`), +0x160 the tick (0x007C2EE0) and +0x1C0 the event's making (0x007C2750).
- The constructor sets `Occludable` (+0x4A3) to 1 before any load. (checked, 0x007C1FBB)

##### The fields, as loaded (0x007C21C0, checked)

The load reads, with these defaults when the field is missing: `Active` (int → byte +0x4A0, through `SetActive` 0x007C2190), `Sound` (string → +0x444), `Volume` (float, 0.0 → +0x47C), `Pitch` (float, 0.0 → +0x480), `FadeIn` (int, 0 → +0x484), `FadeOut` (int, 0 → +0x488), `MaxDistanceMult` (float, **0.0** → +0x48C), `ConeInside` (+0x490), `ConeOutside` (+0x494), `ConeVolume` (+0x498, floats, 0.0), `Priority` (int → +0x49C), `Occludable` (→ +0x4A3). Label strings 0x00B0F110, 0x00B222B0..0x00B22250.

Example (`bhm100ar_tower_level_1.are`, checked): `global_amb_glo/interior_ambience/3d_areaeffect/amb_door_opclose`, Volume -8, Pitch 0, FadeIn 0, FadeOut 34, MaxDistanceMult 0.851, cones 360/360/1, Priority 230, Occludable 0, Active 1; the `global_amb_wal/3d_placeables/...` ones Volume 0 or -4, FadeOut 100, MaxDistanceMult 0.813, Priority 221.

##### How each field reaches FMOD: the event's making (vtable +0x1C0, 0x007C23D0, checked)

1. Nothing is made if `Sound` is empty. Otherwise a sound-event instance (0xC8 bytes, constructor 0x005BA240) goes to +0x440.
2. The project name, `Sound` up to its first `/`, is lowercased and tested for the prefix `global_amb_` (the global wstring 0x00C38BE8, initialised from 0x00B225C0). The result is a flag passed to the name's resolve `0x005BA530(name, 1, flag, 0)`: 0 for the `global_amb_*` projects, 1 for any other. The resolve with its second argument set hands the project name and the flag to the manager's 0x005BA220 → 0x005B9620 (the project's load or reference: Q6's). If the resolve fails the instance is deleted and the object stays without one. (checked; the flag's meaning is Q6's)
3. `Occludable` goes to the instance's +0xBD (the occlusion gate below).
4. The object's position (+0x120) and orientation (+0x130) are set again through its own setters (0x00695540, 0x00694CE0), which pass them to the instance (slots +0x7C 0x007C2010 → 0x005B67D0 and +0x80 0x007C2040 → 0x005B67F0). (checked)
5. **Only when `MaxDistanceMult` > 0** (a missing field, or 0, skips the whole step and leaves the event as the designer made it), the event's own properties are read (0x005B6A60 with "this instance" false: `Event::getPropertyByIndex` over every property, 0x005BE8E0) into a 0x50-byte block, changed, and written back (0x005B6AB0 caches the block on the instance at +0x74 and applies it, 0x005B6530 → the content's slot +0x94, 0x005BEC30: `Event::setPropertyByIndex` with "this instance" true for each changed value):

| Field | FMOD property (index) | What is done | |
|---|---|---|---|
| `Volume` | VOLUME (1) | ×10^(Volume/20), **only when Volume ≤ 0** (a positive value is ignored); a factor below 0.001 (−60 dB) becomes 0 | checked (20.0 at 0x00AFEBA8, 10.0 at 0x00AE7D48, 0.001 at 0x00AE7110, pow 0x008E8C50) |
| `Pitch` | PITCH (3) | **added** to the event's pitch (FMOD's raw pitch units) | checked |
| `FadeIn`, `FadeOut` | FADEIN (36), FADEOUT (37), ms | **replace** the event's. The setter stops the FMOD event immediately before changing either when it differs (0x005BED92, 0x005BEDD8), harmless before the start | checked |
| `MaxDistanceMult` | 3D_MINDISTANCE (17) and 3D_MAXDISTANCE (18) | **both** multiplied. The setter writes the min distance only when > 0. For an event with custom rolloff (mode bit 0x04000000) it writes max ÷ the event's own max distance instead (0x005BECE3) | checked |
| `ConeInside`, `ConeOutside` | 3D_CONEINSIDEANGLE (20), 3D_CONEOUTSIDEANGLE (21), degrees | replace | checked |
| `ConeVolume` | 3D_CONEOUTSIDEVOLUME (22) | ×10^(ConeVolume/20) when ConeVolume ≤ 0, as Volume; the data's 1 leaves it | checked |
| `Priority` | none | loaded and saved (0x007C2AE1) but **never given to FMOD**; no other reader in the class | checked within the class; no outside reader found (inferred) |
| `Occludable` | none directly | gates the engine's occlusion (below) | checked |
| `Active` | none | starts and stops the event (below) | checked |

- The property indices are FMOD 4.x's `FMOD_EVENT_PROPERTY` order; 14 (MODE, tested for FMOD_3D 0x10), 16 (3D_ROLLOFF, tested for 0x00100000 log rolloff) and 19 (3D_POSITION, tested for head-relative 0x40000) confirm the order in the exe's own tests, and the fields' pairings above confirm 1, 3, 17, 18, 20–22, 36, 37. The block's other entries (+0x0C priority 11, +0x10 max playbacks 12, +0x34 index 25, +0x38 index 27, +0x3C index 34, +0x40 index 38, +0x4C index 43) are read and written back unchanged by a sound object. (checked; names past 22 other than 36/37 inferred)
- The block is cached on the instance (+0x74) and re-applied every time the event is started (0x005BAC80 calls 0x005B6530), so the changes hold across stops and restarts. (checked)
- **The pannable class** (a sounds region's) runs the same step, then reads the block again and sets 3D_CONEOUTSIDEVOLUME to 1.0 and index 43 to 0 (0x007C2750): its cone never attenuates. (checked; index 43's name not confirmed)
- **Positions** go to FMOD unconverted: `Event::set3DAttributes(position)` gets the engine's x, y, z as they are (content slot +0x64, 0x005C0880). An orientation is passed as the cone's direction (slot +0x68, 0x005BF660) when its length is within 0.9999..1.001, normalised; otherwise (0, −1, 0). Slot +0x6C (0x005BF740) is the velocity, not the position. (checked; the listener's own frame is Q1's)
- **Dialog and cutscene modes:** an event started while the game mode is 4 (Dialog) or 5 (Cutscene) whose category does not contain `VO` and is not `CRambient_ss`, `dog_main` or `dog_ambient` gets its max distance ×`sound_settings` row 14 (1.8, manager +0x2C) (0x005BAD4E..0x005BADC7 → 0x005B6AF0, content slot +0x70); the manager's update puts it back to ×1 once the mode is neither (0x005BD970). A sound object playing on into a conversation keeps ×1; one restarted during it gets ×1.8 until the mode ends. (checked; the "cutscene" category call 0x005B66B0 beside it not read)

##### When it starts and stops

- **The event exists only while the object is in the world.** Vtable +0x140 (0x007C2090, `SetInWorld(bool)`, name inferred) stores +0x4A2; true makes the event (+0x1C0) if there is none; false stops it (non-immediate) and deletes the instance at once. (checked)
- **An area's sounds enter the world at the end of its load.** The area's 0x0067C160(bool) walks the area's object list +0x694, calls +0x140(bool) on each sound object (`GetSoundObjectById` 0x005D59B0), then with true starts the area's music (manager +0x120 → 0x005BB510(area+0x440, 0)) and with false sets the music parameter to 0 (0x005B6CD0(0, 0)). True is called at the area-load machine's step 8 (0x006C937E), right after `AREALOAD_POSTLOADEXIT`/`AREALOADSAVE_POSTLOADEXIT` (39/43) is queued and run and after 0x0067C3B0(1) makes the area current; false from 0x006C5E40 (a loading-state step 0x006C8420 calls it on the area being left). (checked as calls; area+0x440's meaning not read)
- **Play** happens on the object's tick, not at load: vtable +0x160 (0x007C2150), `AIUpdate(dt)` by its use (inferred), is called for every object by the AI master's walk (0x0064A2D0, from 0x00641FC0) and by 0x00692C60 over a vector at +0x408 of an area. It calls `Play` (0x007C20F0) when `Active` (+0x4A0) is set, the object is not playing (+0x4A1), it is in the world (+0x4A2), and the game mode is not 9 (Loading; the manager's +0x5C is `GameModeController::GetGameMode`, 0x00640060, passed to the manager's update 0x005BD880 from 0x005D5529). (checked)
- **No range test**: a sound object plays as soon as its area is current and it is active, however far the listener is. Distance is FMOD's (min and max distance, the event's rolloff, virtual voices: `internal_soundsettings` rows 0 and 1, 64 real and 384 virtual channels). (checked: no distance test in +0x160 or `Play`; the pannable's tick has one, below)
- `Play` (0x007C20F0): sets playing, gives the instance the reverb preset of the object's position (the area's 0x0067DCB0: the containing reverb or sounds region with the smallest x-y rectangle, else the area's own; Q7) through 0x005B69A0 (stored on the instance +0x54 and the content +0x70; the content routes the event's reverb send to the renderer's reverb instance holding it, 0x005BE6A0: `Event::setReverbProperties` with Room −10000 except for that instance, read in outline), then starts with 0x005BAC80(0, 0). (checked)
- **Fades:** a start fades in over the event's FADEIN (the object's `FadeIn` when MaxDistanceMult > 0), FMOD's own; `Stop` (0x007C1FF0) is the instance's 0x005B9330(0) → the content's slot +0x40 (0x005BF530) → `Event::stop(immediate = false)`, so FMOD fades out over FADEOUT. Leaving the world stops non-immediately and then deletes the instance at once (0x007C20A2..0x007C20B3), which likely cuts the fade (inferred).
- **Active and `ActivateSoundByTag`**: `SetActive` (0x007C2190) plays or stops when the value changes; the script command walks the controlled creature's area's +0x694 for sound objects of that tag (`lib-agent-2-scg-sounds.md`). `Active` is saved, so a sound turned off stays off. `StopSoundObject` stops without clearing `Active`, so the next tick restarts it. (checked)
- **The pannable's tick** (0x007C2EE0, the sounds region's sounds): runs the base tick, then, when not loading, moves its event: nothing while the listener is farther from the region's position than the event's max distance (instance +0x10) plus the region's radius (+0x530, the farthest corner + 4 m); **at the listener** when the region's id is in the area's +0x368 vector (0x008663A0 is a vector find; inferred: the regions the listener stands in); otherwise at the nearest point of the region's outline (0x00798DE0) at the listener's height, facing the listener. That re-placement waits for a timer (+0x4D0 ≥ 0.06 s) or a listener move above 4 m, and needs a move above 0.25 m since the last placement; the timer is then re-armed to rand()%100 × 0.0003 − 0.005 s, and after a placement to 0.06 s. (checked; constants 0x00B0E1F0 0.06, 0x00ADEF1C 4.0, 0x00AFF3E8 0.25, 0x00B225B8, 0x00B225B0, 0x00B225A8)

##### Occlusion

FMOD geometry is not used: `System::createGeometry` is not imported, only `Event::set3DOcclusion` (thunk 0x0090914E). The engine casts rays itself. (checked)

- **Who is tested** (0x005B7220, the manager's, checked): occlusion must be on (manager +0x55, set at 0x0040430F to the negation of an options bool at [0x00C32938]+0x6B4: inferred the `DisableSoundOcclusion` setting, whose string is at 0x00B14C90), the instance's +0xBD (`Occludable` for a sound object) set, the game mode not 12 (PartyPicker), and the event 3-D (+0x18, from its MODE). At the start (0x005BAC80 → (instance, 1)) the test is skipped when the listener is beyond 1.25 × the event's max distance (0x00AFEBD0); each frame (the manager's update 0x005BD880 → (instance, 0)) it runs for every live instance with no distance test. An event FMOD reports inaudible (`Event::getInfo` audibility ≤ 0) is not cast for unless its direct occlusion is exactly 1.0 (it is flagged to snap on its next test, +0xC1). Within 0.05 m (0x00AF70B0) of the listener the occlusion is 0. A per-cast chance rand()%100 ≤ [0x00BF981C] = 100 always passes (`sound_settings` row 11's 100 %, hard-coded).
- **The ray** (0x00A2C700 on the manager's +0x130): from the listener (manager +0x90's position) to the event's position, every hit collected (0x00648BD0, flags 0x1024: Placeables 0x4, StaticGeometry 0x20, and 0x1000, trees by `dev/COLLISION.md`). A cache of the last 8 (from, to) pairs and their result answers a repeated query. For each hit in order, a factor f:
  - a placeable (hit flags bit 0x4): the placeable's +0x674, its `placeable_types` `OcclusionFactor` (read at 0x0075292E, column hash 0x913E3094); (checked)
  - bit 0x1000: hit+0x30 / 45; (checked; hit+0x30's meaning not read)
  - static geometry (bit 0x20): `sound_geometry_mat_types` (table 156) `LowOcclusionFactor` (hash 0xAE2B65F5) at row hit+0x38, × 0.01 × hit+0x30 / 45 (0x00AE7FF0, 0x00B39A50); but a static hit within 0.5 m (0x00AE7104) of an earlier static hit's distance gives f = 1.0, full. (checked as arithmetic; that hit+0x38 is the surface's sound material and that two close hits are a wall's two faces inferred)
  - the total: occ ← occ + (1 − occ) · f per hit, stopping once occ ≥ 1, clamped to 1. It is the direct occlusion; the reverb occlusion output is set to 0 at the top and never written again, so it is always 0. (checked)
- **The table** (checked): Unknown 0, Wood 40, Sand 20, Leaves 20, Grass 10, Stone 100, Dirt 40, Puddle 30, Water 40, Mud 40, Ice 50, Snow 30, Hollow_wood 30, Metal 100, Gravel 40, Carpet 50.
- **Smoothing** (0x005B6EB0, each frame with the frame time): the applied direct occlusion (+0x2C) moves toward the target (+0x24) by 1.2 × dt (0x00AFEBC0; `sound_settings` row 2 says 1.2/s and "not implemented", but the constant is the same), without overshoot; the reverb occlusion (+0x28 → +0x30, always 0 from the ray) is applied at once; then `Event::set3DOcclusion(direct, reverb)` (content slot +0x58, 0x005C08D0). The first test after a start or a skipped test snaps (dt −1.0). (checked)

##### The instance list (a correction)

`lib-agent-2-scg-sounds.md` inferred that sound objects' events are not in the manager's list. They are: every start (0x005BAC80) pushes the instance into the manager's vector at +0xA8 (begin +0xAC, end +0xB0; 0x005BAE4D, the push 0x0076B540 not read). So `StopSound(path)` matches a sound object's event too when its full path is given, and the per-frame update (occlusion, the dialog distance scale) covers them. (checked as the call)

##### Not read (Q5)

- hit+0x30 and hit+0x38 of the ray's hit record at their source (the physics wrapper), and so the exact static-geometry formula's inputs.
- Whether the manager frees a sound object's finished instance (0x005BDB0E deletes an instance whose +0xBE byte is set and which no longer plays; whether a sound object's instance ever has +0xBE is not read), which would leave +0x440 dangling.
- 0x005B66B0 (the "cutscene" category call at a start in Dialog or Cutscene mode), the area's +0x408 vector ticked by 0x00692C60, the area's +0x368 vector, area+0x440 passed to the music start.
- Index 43 of the property block (the pannable's 0), and the names of indices 25, 27, 34, 38.
- The values of `Pitch`, `MaxDistanceMult` and the rest across every area (ghilana's query does not reach into `SoundList`); only the tower's were read.

#### Q6. Which FMOD projects load, and when

Read 2026-10-18 by lib-agent-2's subagent (au4q6) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py`, in `fmod_event.dll` 4.24.24 (its exports), and in the data through ghilana and the files on disk. Not seen in play. "(checked)" means read in the disassembly or the data. "(inferred)" means not read.

##### Summary

- **Nothing loads a project by an area's name.** An area's projects are the first path segments of its sound objects' `Sound` fields (the `.are` SoundList and the sound regions' lists). Each sound object, as its event instance is made, loads `<segment>.fev` through a reference-counted project map. The instance's destructor releases it. `tower_level_1.fev` belongs to `pre410ar_tower_level_1` (Ostagar's Tower of Ishal), not to the mage origin: `bhm100ar_tower_level_1`'s sounds name `global_amb_glo`, `global_amb_wal` and `bhm_extra` (checked, below).
- At start the FMOD init loads `audio_categories.fev` directly. Then a sync loads every `Label` of `sound_projects_to_load.gda` (table 157) into the map. The sync runs again whenever the sources change (campaign loaded, module dropped). It unloads rows that are gone, and reloads a project whose file now resolves to a different copy (checked).
- These also load and release projects on demand, all through the same map (checked):
  - a conversation: the project named for the conversation's resref;
  - an ambient (bark) conversation: its `ConversationVOBank`;
  - a cutscene's speak-line action: its VO bank;
  - a cutscene's sound action: its event's first segment;
  - a creature's sound set: the sound-set name.
- The music theme loads its own project (`music` from `music\Mage Origin`) straight through `EventSystem::load`, outside the map (checked).
- Every `.fev`/`.fsb` FMOD opens goes through the game's own file callbacks. Those resolve the bare file name to the first *folder* source holding it, in the resource manager's priority order, so the language's VO folder, the campaign's and the add-ins' audio folders are found that way (checked).

##### 1. FMOD's setup and file access

- **Init** `0x005C5430` (the renderer `EclipsePrivate::CContentRenderer_FMOD`, vftable 0x00AFF20C, slot +4 `0x005C54B0` calls it; the renderer is made by `0x005BDD40` from the sound manager's init `0x005BD660`, stored at manager+0x90) (checked):
  - `FMOD_EventSystem_Create` → global `0x00C3250C` (the `EventSystem*`); the low-level `System*` → `0x00C32508`.
  - `EventSystem::setPluginPath(NULL)` (+0x10), `EventSystem::init` (+0x00).
  - `System::setFileSystem(open 0x005C1070, close 0x005C02D0, read 0x005BE2D0, seek 0x005BE390, blockalign -1)` (call at 0x005C59E4).
  - Then `0x005C5280`: `EventSystem::load("audio_categories.fev", NULL, &out)` (+0x24, string 0x00AFF8D4). Each category goes through `getNumCategories` (+0x40) / `getCategoryByIndex` (+0x3C) to `0x005C4980`.
- **The EventSystem's vtable** was rebuilt from `fmod_event.dll`'s exports (`EventSystemI` vtable at 0x1003558C) (checked): +0x00 init, +0x0C setMediaPath, +0x10 setPluginPath, +0x24 load, +0x28 unload, +0x2C getProject, +0x38 getCategory, +0x3C getCategoryByIndex, +0x40 getNumCategories, +0x48 getEvent, +0x5C setReverbProperties, +0x64 getReverbPreset, +0x84 set3DListenerAttributes, +0x94 preloadFSB.
  - `EventProjectI`: +0x00 release, +0x08 getGroup, +0x0C getGroupByIndex, +0x10 getNumGroups, +0x30 getMemoryInfo.
  - `EventGroupI`: +0x04 loadEventData, +0x08 freeEventData.
- **setMediaPath, preloadFSB and EventSystem::unload are never called.** No call through +0x0C, +0x94 or +0x28 follows a load of `[0x00C3250C]` anywhere in the FMOD code (0x005BE000–0x005C6000, where all 31 references to the global are). So the media path stays empty: FMOD asks for `<project>.fev` and its banks `<bank>.fsb` by bare name (checked as code; the bank naming is FMOD's, inferred).
- **The open callback** `0x005C1070` (`__stdcall (name, unicode, &filesize, &handle, &userdata)`, `ret 0x14`) (checked):
  1. The narrow name becomes a `std::wstring`.
  2. `ECResMan::Manager()` (0x004C7990) → `0x004C5950(&name)`. That lowercases the name and walks the manager's sources in order (its map at +4). It takes only sources that are going (+0x69 set, +0x68 clear) whose kind (slot +0x20) is 0 or 1, and whose slot +8 has the file, then replaces the name with that source's path plus the name.
  3. The kinds: `DirectorySource` answers 0/1 (`0x004CF1B0`), `RIMSource` 2 (`0x004CCAE0`), `ErfSource` 3 (`0x004CE240`). **So FMOD's files are found loose in folders only, never inside an ERF or RIM** (checked).
  4. `ECFileAccess::Aliases::Path` (0x004CA780) turns the alias into a path, and a 0x48-byte file object opens it (0x004C9170).
  5. A failure returns 0x17 (`FMOD_ERR_FILE_NOTFOUND`). On success, `filesize` comes from 0x004C8A30 and userdata is 0xDEAFBEEF.
- **Where the copies come from:** the folder sources and their priorities are `dev/FILES.md`'s and `Install.cpp`'s (AUDIO_VO: 100006 for the language's `packages\core\audio\vo\<lang>\`, AUDIO_SOUND: 100008, the campaign's voice 50006 and sound 50007, an add-in's `audio\sound` base −1 and `audio\vo\<lang>` −2). An add-in's `module\` folders are mounted only while it extends the loaded campaign (FILES.md). The exe's strings for these folders are at 0x00ADF5A4 (`\audio\vo\`), 0x00ADF5FC (`\audio\sound\`), 0x00ADF6F4 and 0x00ADF738 (`packages\core\audio\...`), used by the source set-up `0x0040F060` (checked as references). A lower priority number wins, as for every resource (FILES.md; for audio, inferred from the walk order, since the map's key order was not read).
- **Project name vs file name** (checked over all 9,020 `.fev` on disk): the project name in each FEV1 equals its file name but for case in four files (`conv_TEMPLATE_PROJECT_NAME`, `DA_bed110cs_tainted_mirror`, `DA_epi_coronation_mix`, `Wynne_transform`). The lookup lowercases the file name. Whether `getEvent`'s project match is case-sensitive is FMOD's (not read).

##### 2. The project map: LoadProject and UnloadProject

The sound manager (singleton getter 0x005B9020) keeps the renderer at +0x90 and the project map's owner at +0x94.

- **LoadProject** `0x005BA220` (manager, `__thiscall (const std::wstring &name, bool precache)`, `ret 8`) → `0x005B9620` on [manager+0x94] (checked):
  1. An empty name: false.
  2. `name == "music"` (0x00AFEC1C) while manager+0x56 is 0: false.
  3. The map has the name (find 0x00A5E9F0): the bank's count (+0x20) goes up by 1; true.
  4. Otherwise the renderer's slot +0x24 (`0x005BE4B0`) makes a `CContentBank_FMOD` (0x50 bytes, vftable 0x00AFF278; base 0x005BDFD0 sets the count to 1). Its +4 is the name.
  5. Slot +4 (`0x005C2050`) is `EventSystem::load(name + ".fev", NULL, &bank+0x4C)`. On failure the bank is deleted: false.
  6. Then map[name] = bank (0x00706A30). With `precache`, slot +0x0C (`0x005C1240`, arg 1) runs: for every top-level group (`getNumGroups`, `getGroupByIndex(i, false)`), `EventGroup::loadEventData(2, 0)`.
  7. A precache failure logs "Failed to precache fmod project %s" (0x00AFF448) and returns false, with the bank left in the map (checked).
  8. 2 is `FMOD_EVENT_RESOURCE_SAMPLES` in FMOD's header (inferred from the header's enum; the slot +0x0C argument picks 2 or 0, and LoadProject always passes 1, so 2).
- **UnloadProject** `0x005B8630` (manager, `__thiscall (const std::wstring &name, bool force)`, `ret 8`) → `0x005B82A0` (checked):
  - Not in the map: false.
  - The count is above 1 and not `force`: the count goes down by 1.
  - Otherwise it leaves the map (0x005B7C50) and the bank is deleted. The bank's destructor `0x005C0940` → `0x005BF950` calls `EventProject::release` (+0x00).
  - No caller passes `force` = 1 (every call site pushes 0, below) (checked).
- The bank's other slots: +0x10 `0x005BF970` is `getMemoryInfo` in KB; +0x08 `0x005C2160` and +0x14 `0x005C2290` find a group by name (`getGroup`) (checked as calls; their callers not read).

##### 3. At start, and whenever the sources change: `sound_projects_to_load.gda`

- **The table** (ghilana, checked): table 157, "Sound projects list", one core file `sound_projects_to_load.gda` (`packages/core/data/2da.erf`). Its columns are ID, Label, PrecacheWaveData (u8), Comment, AddInUID, ExtendedModule. Its rows:

| ID | Label | PrecacheWaveData | Comment |
|---|---|---|---|
| 0 | glo_fly_bf | 1 | |
| 1 | glo_fly_fs | 1 | |
| 2 | glo_fly_mv | 1 | |
| 3 | glo_fly_plc | 1 | placeables |
| 4 | glo_fly_wep | 1 | |
| 5 | glo_gui | 1 | |
| 6 | master_reverb | 0 | reverb definitions |
| 7 | vfx_spells | 1 | |
| 8 | glo_amb_creat | 1 | |
| 9 | glo_dog | 1 | |
| 10 | c_dragona | 1 | dragon sounds |
| 11 | slo_blows | 1 | slow motion sounds |
| 12 | (empty) | 0 | |

  - Add-ins add worksheets: `sound_projects_to_load_gw` (dao_prc_nrx_1) and `_sh` (dao_prc_cp_2), both in enciphered archives and unread. `_gxa` belongs to Awakening's campaign.
  - Not in the table: `music`, `music_cutscene`, `audio_categories`, `global_amb_*` (below).
- **The sync** `0x005BB040` (manager, no arguments) (checked):
  1. It gets table 157 (`push 0x9D` → `GdaStore::FindTable` 0x004DF580).
  2. It walks the manager's vector at +0x98 (begin +0x9C, end +0xA0; 0x38-byte records: the label `std::wstring` at +0, the resolved path at +0x1C) of projects loaded from the table. It keeps a record only if its label is still some row's `Label` (hash 0xB49CA166) **and** `label + ".fev"` still resolves (`0x004C5950`, section 1) to the path it stored.
  3. Otherwise the project is released (`UnloadProject(label, 0)`, 0x005B8630) and the record erased (0x005B81E0).
  4. Then, for every row with a non-empty `Label` not already in the vector, it reads `PrecacheWaveData` (hash 0x0C64DDBA, the table's slot +0x4C). If `LoadProject(label, PrecacheWaveData)` succeeds, it appends {label, resolved path} (0x005B5A60).
  5. A row whose file is not found fails and is tried again at the next sync (inferred from the code's shape).
- `AddInUID` and `ExtendedModule` are **not read** by the exe: neither column's hash (0x3174176D, 0x534ABBA1) nor name is anywhere in the code (checked). A row counts only through the merged table, that is, while its worksheet's source is mounted.
- **When the sync runs** (its five callers, checked as calls; the occasions are inferred from what precedes each):
  - `0x00404260` (ID 160): the application's start, right after the sound manager's init `0x005BD660`.
  - `0x005FCE80`: the campaign's package sources set up (FILES.md's 0x005FCE80), after `0x004E0BF0` (inferred: the data tables merged again).
  - `0x005FC140` (ID 11001, a `CCModule` method): the same pair.
  - `0x006C76F0`: the loading states for a new game or a save, after `0x005FCE80`.
  - `0x006C8800` (ID 15092): after `CCModule::DropAreaList` and `CClientExoApp::GetModule`, the module left.

##### 4. An area's projects: its sound objects

- **The make**: `0x007C23D0` (ID 20325, the sound object's slot that makes its event instance, `dev`'s +0x1C0 in the sounds note) (checked):
  1. A 0xC8-byte instance (0x005BA240) goes to +0x440.
  2. The project is `Sound` (+0x444) up to its first `/`. **precache** is 1 unless that project starts with `global_amb_` (the `std::wstring` at 0x00C38BE8, set from 0x00B225C0 by 0x00AD2F00; `find(..., 0) == 0`).
  3. Then `resolve(Sound, loadProject = 1, precache, 0)` (`0x005BA530`). On failure the instance is destroyed and +0x440 cleared, so the sound is silent. On success `Occludable` (+0x4A3) goes to the instance's +0xBD.
- **The resolve** `0x005BA530` (instance, `__thiscall (const std::wstring &name, bool loadProject, bool precache, int mode)`) (checked):
  - With `loadProject`: the text before the first `/` (0x00AEBF7C) goes to +0x38, and `LoadProject(it, precache)`'s answer goes to the instance's +0xBC.
  - Then the event comes from the `CContent_FMOD` at +0x70, slot +4 (`0x005C3390` → `0x005C1C20`), that is, `EventSystem::getEvent(name, mode)` (+0x48).
  - **No other caller passes `loadProject` = 1**: `ForScript_PlaySound` 0x005BC7F0, 0x005BC940, 0x005BCF60, 0x006AA940, 0x006B6290, 0x007DA670, 0x007DC0D0, 0x007E80E0, 0x007EA220 and 0x00A5DA60 all push 0 there.
  - So a script's `PlaySound` of an event whose project nothing has loaded finds no event and is silent (inferred from FMOD's getEvent needing the project loaded).
- **The release**: the instance's destructor `0x005BA350` calls `UnloadProject(+0x38, 0)` when +0xBC is set (0x005BA4A3–0x005BA4B9) (checked). So a project stays while any sound object naming it has an instance. It goes when the last one's instance is freed: the object leaves the world (its slot +0x140 false frees +0x440, the sounds note) or is destroyed (inferred: on leaving an area list the areas are deleted, `dev/LOAD_EVENTS.md`, so their sound objects with them; fast load's kept lists keep them).
- **Inactive sounds load too.** The instance is made by the object's being in the world, not by `Active`. That is why `bhm100ar_tower_level_1` has `bhm_extra/mage_fire/sfx_smfrburst*` sound objects placed with `Active` 0, and why a script's later `PlaySound`/`ActivateSoundByTag` finds `bhm_extra` loaded (inferred).
- **The data** (checked: all 297 `.are` the game takes, extracted with ghilana, their `project/...` strings):
  - `bhm100ar_tower_level_1`: SoundList `global_amb_glo/...` ×3, `global_amb_wal/...` ×3, `bhm_extra/mage_fire/...` ×2 (inactive). Its Type 4 sound regions name `global_amb_glo/...`. MusicPreset is `music\Mage Origin`, and nothing names `tower_level_1`.
  - `tower_level_1..4` are `pre410ar_tower_level_1` .. `pre440ar_tower_level_4`'s, the Tower of Ishal.
  - Across all areas: `global_amb_glo` in 250 areas, `global_amb_wal` 88, `global_amb_orz` 66, `global_amb_fade` 25, `generic_ambience` 20, `glo_fly_plc` 10, then the areas' own (`cir_extra`, `bdc_proving`, `kings_camp`, `korcari_wilds`, `elven_ruins`, Awakening's `*_area_extra`, ...); 36 projects in all.
  - **`generic_ambience.fev` is in no source of the install**, so the 20 areas that name it (`ran*` highway encounters, `den500ar_arl_exterior`, `bec200ar_estate_ext`, `shl000ar_offer`) get those sounds silently dropped (checked: no such file; the drop follows from section 2).

##### 5. Conversations, barks and cutscenes, on demand

- **A conversation** (`GameModeDialog`, IDs 13950–13952 beside `ShowResponses` 13949) (checked):
  - Its start `0x006913E0` takes the conversation's resref (its +0, copied by 0x0068CAE0; the same field the ambient manager logs as "RESREF: %ls"), cuts it at the first `.`, keeps it at +0x1EC, and calls `LoadProject(+0x1EC, false)` (0x006918B8).
  - Its end `0x006910D0` calls `UnloadProject(+0x1EC, 0)` (0x006912BF) and deletes its cutscene at +0x208 (0x007E10A0).
  - **The project is named for the conversation, not for its `ConversationVOBank`** (12004; `0x0068CFD0` reads it), whose name the line's voice path `<vobank>/VO/<strref>` uses. A conversation whose VO bank differs from its own name relies on the bank's project being loaded some other way (inferred; how many such conversations exist was not counted).
  - The VO `.fev` is found in the language's folder by name: `modules/single player/audio/vo/<lang>/vo/bhm100_jowan.fev` (checked on disk).
- **An ambient conversation** (`CAmbientDialogManager`, `.\cambientdialogmanager.cpp` 0x00B15254) (checked):
  - `0x006FE4A0` loads `LoadProject(ConversationVOBank, false)` once per entry (flag +0x2D), then plays the line through `0x007F2B70`.
  - `0x006FF020` (four callers) releases it.
- **A cutscene** (checked):
  - The project loader `0x007D8140` (ID 20909) is the cutscene's: `LoadProject(name, false)`, and on success the name is pushed onto the cutscene's vector at +0x1A0 (0x1C-byte `std::wstring`s, begin +0x1A4, end +0x1A8).
  - The cutscene's teardown `0x007E10A0` (ID 21039; called from the conversation's start and end and nine places in all) calls `UnloadProject(each, 0)`.
  - Two actions load through it:
    - The sound action `0x007DA670` (ID 20943): its `CutsceneActionSoundName` (5670), up to the first `/`, before resolving the event with `loadProject` 0.
    - The speak-line action `0x007DC0D0`: its `CutsceneActionSpeakLineVobank` (5563), **unless** `CutsceneActionSpeakLineNoVOIngame` (5565) is set.
  - So a cutscene's own sound project (`bhm500cs_harrowing`, `bhm_intro`, ...) and its lines' VO banks load as its actions are built, and go when the cutscene is torn down (the timing of action creation against the cutscene's start was not read).
- **A pin past the owner's end**: `0x005BAF30` (instance, `(bool)`) takes, once, the project of the instance's own event path (0x005B6860, to the first `/`) with `LoadProject(..., false)` into +0xBC, so the destructor releases it (checked). Only the cutscene's sound handlers `0x007D91A0` and `0x007D97A0`/`0x007D9760` pass true, and only when the cutscene's +0x1B4 byte is 0. PlaySound and the others pass false. Inferred: a cutscene sound that outlives its cutscene keeps its project loaded until it ends.
- **A creature's sound set** (checked):
  - `CCCreature::ForScript_SetSoundSet` `0x006BD0E0` (also called by the creature's loads, `0x006BDC70` ×4, `0x006C04C0`, `0x007C0B60` ×2, and others) releases the old name at +0x96C (when +0x980 says it has one), stores the new one, and calls `LoadProject(name, false)`.
  - The creature's destructor `0x006BAFE0` releases it (0x006BB3E4).
  - So each sound set's own `.fev` (the `ss_*` barks) is loaded per creature that has it, counted (inferred: the sound set name is a project name, as it is passed straight to LoadProject).

##### 6. Music

- The music theme (`CMusicThemeContent_FMOD`, vftable 0x00AFF86C) (checked):
  - Its slot +4 (`0x005C0E20`) takes the preset, turns `\` (0x00ADE368) into `/`, and keeps the text before the first `/` at +0x4C: `music` for `music\Mage Origin`.
  - Its update (slot +0x0C, `0x005C4B10`), in state 1, calls `0x005C27A0`. That function asks `EventSystem::getProject(+0x4C)` and resolves `+0x4C + ".fev"` through `0x004C5950`. If the project is loaded but its file now resolves elsewhere than the path kept at +4, it releases the project and loads it again. If it is not loaded, it calls `EventSystem::load` itself (the call at 0x005C2920).
  - Its destructor `0x005C38F0` releases the project (`getProject` then `release`).
- So `music.fev` is held by the theme, outside the map. LoadProject refuses the name `music` while manager+0x56 is 0, which keeps a second owner from taking it through the map (inferred; what sets +0x56 was not read).
- `music_cutscene` is in no table. Inferred: it loads through a cutscene sound action naming `music_cutscene/...`, by section 5's path (no cutscene was read for it).

##### 7. Unloading on an area change

There is no "unload the area's projects" step. Every project but the table's, `audio_categories` and the music theme's is counted, and goes when its last holder goes (checked as code; the holders' lifetimes are inferred):
- an area's sound objects, freed with the area (a dropped area list; `dev/LOAD_EVENTS.md`);
- creatures, with their sound sets;
- a conversation, at its end;
- a cutscene, at its teardown.

The table's projects stay loaded for the session, re-synced at the source changes in section 3. Fast load (`perf/fastload`) keeps area lists alive, so their sound objects, and with them their projects, stay loaded (inferred).

##### 8. The add-ins' projects

- An add-in adds nothing of its own to how projects load (checked: no code reads `AddInUID`/`ExtendedModule`). Its projects come in by the same three routes:
  - its `sound_projects_to_load_*` worksheet's rows, while its `core\data` is mounted, at the next sync;
  - its areas' sound objects' path segments (Awakening's `ltl_area_extra`, `vgk_area_extra`, ... in `Addins/dao_prc_ep_1/module/audio/sound/`);
  - its conversations and cutscenes.
- The files are found because the add-in's `audio\sound` and `audio\vo\<lang>` folders are sources, at its base −1 and −2 (FILES.md).
- On disk, add-ins' `module/audio/sound` `.fev` files: dao_prc_ep_1 49, dao_prc_str 25, dao_prc_gib 17, dao_prc_lel 11, dao_prc_drk 2, cp_1, cp_2 and nrx_1 one each (checked). The offer `dao_prc_ocp_1` has `offer_prc_kcc.fev`, which one area names (`shl000ar_offer`'s).

##### Not read (Q6)

- What sets manager+0x56 (the `music` guard), and whether it is the options' "music disabled".
- The order of the resource manager's source map that `0x004C5950` walks (assumed priority order, as FILES.md has for other resources).
- What FMOD does when two projects of the same name are loaded (the map and the theme both loading `music`), and whether `getEvent`'s project match ignores case.
- The ambient dialog's `0x006FF020` callers and when an entry ends; the cutscene's +0x1B4 byte; when cutscene actions are built relative to the cutscene's start.
- How many conversations have a `ConversationVOBank` different from their resref (each would play no voice unless its bank's project is loaded otherwise).
- `0x005C4260` (renderer slot +0x40, which enumerates a project's events) and `0x005C1F80`/`0x005C3530`/`0x005C35B0` (a parameter on an object at renderer+0xA8, likely the music system's): read only far enough to see they load no project.
- The sound manager's +0x94: the scripts' sounds note gives it as "the soundset variation bags" from PlaySoundSet's use. Here it is the object holding the project map (LoadProject and UnloadProject run on it). Whether one object holds both was not read.

#### Q7. The audio triggers

Read 2026-10-18 by lib-agent-2 (subagent au4q7) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py` and capstone; data through ghilana (`bhm100ar_tower_level_1.are`) and `master_reverb.fev` from disk. Not seen in play. "(checked)" is read in the disassembly or the data; "(inferred)" is not.

##### The classes and which Type makes which

- The factory is `0x0067A690(type)` (cdecl), a jump table at 0x0067A838 over 0..5 (checked):

  | Type | Class | Size | Constructor | vftable |
  |---|---|---|---|---|
  | 0, and anything above 5 | `CCTrigger` | 0x510 | 0x0079B0B0 | 0x00B1FEF4 |
  | 1 | `CCTriggerAreaTransition` | 0x530 | 0x0079B2C0 | 0x00B2083C |
  | 2 | `CCTriggerTrap` | 0x520 | 0x0079B230 | 0x00B20524 |
  | 3 | `CCTriggerAudioReverb` | 0x530 | 0x0079B340 | 0x00B2020C |
  | 4 | `CCTriggerAudioVolume` (the "region of sounds") | 0x540 | 0x0079B550 | 0x00B20E6C |
  | 5 | `CCTriggerAudioMusicVolume` | 0x530 | 0x0079B440 | 0x00B20B54 |

- Vtable slot 115 (+0x1CC) answers the same number back (0x004CE240 → 3, 0x00A11120 → 4, 0x0046E640 → 5, AT 1, trap 2, base 0) and is what the save writes as `SavegameAreaTriggerType` (16112) and the restore feeds to the factory (0x00687C26). (checked)
- `CCTriggerAudioVolume` derives from `CCTriggerAudioReverb` (`--bases`; its constructor calls 0x0079B340 first), so a sounds region is also a reverb region with its own `ReverbPreset`, and its enter and leave are the reverb trigger's (slots +0x1E4/+0x1E8 are the same functions). `CCTriggerAudioMusicVolume` derives from `CCTrigger` directly. (checked)
- Casts (vtable slots returning `this`, 0x0079AF30, else null 0x004137B0): +0x1D4 answers reverb and volume triggers; +0x1D8 answers the volume trigger only; +0x1DC answers the music-volume trigger only (the cast `SetMusicVolumeStateByTag` uses). (checked)
- **Correction for `src/dao/core/Area.h`:** its comment calls Type 5 "a region running a script as it is entered and left". In the exe Type 5 is the music-volume trigger. Its `.are` struct carries `ScriptResRefID` and `ScriptDelay` beside `EnterState`/`ExitState` (the tower's has `""` and 0), but no trigger load reads `ScriptDelay`: the string is not in the exe. (checked: factory, labels, `--strings ScriptDelay` finds nothing)

##### Where the area keeps them

Both the `.are` load (0x00687DA2) and the restore from the in-memory save (0x00687C39) do the same after the factory (checked):
1. Vtable +0x1C0 loads the object: from a `.are` struct, a template (`.utt`) named by `TemplateResRef` is loaded first through +0x1C4 (base 0x0079A1D0), then the instance's own fields through slot 23 (+0x5C) over it, mode 2 (checked as calls; that an instance struct missing a field resets it to the reader's default is inferred from the readers' defaults: "" and 0).
2. Vtable +0xF8, add to the area (sends SPAWN once, `dev/LOAD_EVENTS.md`).
3. If +0x1D4 answers (reverb or sounds region): its id goes into the area's vector at +0x348 (begin +0x34C, end +0x350).
4. If +0x1D8 answers (sounds region): into the area's vector at +0x358.
5. If +0x1D0 answers: into +0x338. No audio trigger answers +0x1D0.

The music-volume trigger goes in no list of its own: `SetMusicVolumeStateByTag` walks the area's object vector +0x694 (`lib-agent-2-scg-sounds.md`).

##### Fields read, offsets, and the save

| Class | `.are`/`.utt` field | Object offset | Save field (GFF4) | Notes |
|---|---|---|---|---|
| Reverb (and Volume) | `ReverbPreset` (string) | +0x510 (wstring) | 16108 `SavegameAreaTriggerReverbPreset` | load 0x007999B0, save 0x0079A450, restore 0x0079A9D0 (checked) |
| Reverb (and Volume) | `Priority` (int) | +0x52C | not saved: 16109 `SavegameAreaTriggerPriority` is never written or read (no `push 0x3EED` in the exe) | **read and never used**: no reader of +0x52C in any trigger or area code (checked by a scan for `+ 0x52c]`; inferred that no virtual getter hides it) |
| Volume | `SoundList` (list of structs), each struct's `ObjectId` (default -1) and the sound's own fields | sound ids at +0x538 (count +0x53C) | 16111 `SavegameAreaTriggerSounds`: a list, each sound's own save struct (its vtable +0x54); restored by its field 0x17 (object id) | load 0x0079A000, save 0x00799B90, restore 0x0079A620 (checked) |
| Volume | (none) | +0x530 the region's radius, +0x534 = 4.0 (0x00ADEF1C) | not saved | +0x530 = max distance from the trigger's position (+0x120) to its outline's corners, plus +0x534, computed when the object loads (slot +0x1C0 override 0x0079A490) (checked) |
| MusicVolume | `EnterState` (int) | +0x510 | 16113 `...MusicVolumeEnterState` | load 0x00799AD0, save 0x00799FA0, restore 0x0079AAA0 (checked) |
| MusicVolume | `ExitState` (int) | +0x514 | 16114 `...MusicVolumeExitState` | |
| MusicVolume | `EnterStateDelay` (int, **milliseconds**) | +0x518 (float, seconds: × 0.001 from 0x00AE7118) | 16115 `...EnterStateDelay`, a float in seconds | |
| MusicVolume | `ExitStateDelay` (int, ms) | +0x51C (float, s) | 16116 `...ExitStateDelay`, float s | |
| MusicVolume | (none) | +0x520 byte: the controlled creature is inside | not saved | set by enter, cleared by leave (checked) |
| all | `Type` | slot +0x1CC | 16112 `SavegameAreaTriggerType` | written by the base save 0x00799E10 (checked as `push 0x3EF0` there) |

16110 is `SavegameAreaTriggerLoadScreen` (the area transition's), not audio. The names are master's `dev/gff4_fields.csv`; the pushes of 0x3EEC..0x3EF4 are at the addresses above (checked).

##### Enter and leave: who calls them

- A creature keeps the set of triggers it stands in at +0xBA8. `0x006AE100` (creature, called from 0x006B3028 and 0x006B31C2, two position updates: inferred) diffs that set against the triggers at its new position: each trigger left gets vtable +0x17C(0, ...), each entered +0x17C(1, ...). (checked)
- `CCTrigger` +0x17C is 0x00799850: it first raises the script event (0x00698850: `EVENT_TYPE_ENTER` 8 or `EVENT_TYPE_EXIT` 9 to the trigger's script, through the disabled-set gate 0x00698490, and not while the area's +0x234 is set) and then calls slot +0x1E4 (enter) or +0x1E8 (leave) with one byte: **whether the creature is the controlled creature** (`0x006A7D30`: its id equals the app's controlled id, 0x005D5CA0). (checked)
- So any creature's crossing reaches the audio slots, but all three audio triggers act only when that byte is true (each starts `cmp byte [esp+4], 0; je return`). Followers, enemies and the camera do nothing to reverb or music. (checked)
- Whether switching the controlled party member re-runs enter/leave for the new leader is not read.

##### CCTriggerAudioReverb (and the sounds region): reverb on enter and leave

- **Enter** (0x0079B3D0, checked): if the creature is the controlled one, the manager's `0x005B7790(&ReverbPreset, 0.0, -1.0)` sets the preset, then the trigger's id is appended to the area's list at +0x12C (a `std::list`, 0x00698C30 push_back).
- **Leave** (0x0079AB70, checked): if the controlled creature, the trigger's id is removed from the area's +0x12C list (0x00745230, every node of that id). Then:
  - if the list still has ids, the **last one appended** (`back()`, 0x00469870 / 0x0079A190) is looked up as a trigger (0x005D5AA0) and its `ReverbPreset` is set with (0.0, -1.0);
  - if the list is empty, the area's own `ReverbPreset` (area +0x424) is set, through `CCArea 0x0067A080(-1.0)` → manager 0x005B7790(area+0x424, 0.0, -1.0);
  - if the trigger has no area, the empty name `""` is set.
- **So overlapping reverb regions are a stack by entry order: the most recently entered region that is still occupied wins. `Priority` plays no part** (checked: no reader). The tower's reverb region has `Priority` 128 and every sounds region none.
- **The area's default:** when an area is made current, `0x006881C9..0x006881E4` sets the music theme from area +0x414 (`MusicPreset`, 0x005B6B80: Q4's) and then the area's reverb from +0x424 with 0.0 (checked as calls; that +0x414/+0x424 are `MusicPreset`/`ReverbPreset` is inferred from their use, the field loads not read). The tower's area `ReverbPreset` is `""`.
- **A sound's reverb at a point** (for sound objects, `CCArea 0x0067DCB0`, checked): of the area's reverb-capable triggers (+0x348 vector, cast +0x1D4) whose shape contains the point (vtable +0x180), the one with the **smallest bounding rectangle in x-y** (0x00799010's min/max, area = Δx·Δy, below an initial 9,999,999.0 from 0x00B0DACC) gives its `ReverbPreset`; none, the area's +0x424. This is a different rule from the listener's stack above.

**How a preset name reaches FMOD** (checked unless marked):
- The manager's `0x005B7790(name, fade, time)` calls the renderer (manager +0x90, `CContentRenderer_FMOD`, vftable 0x00AFF20C) slot +0x38 = 0x005C35F0; on success it calls 0x005B69D0 on every live sound-event instance (manager +0xAC..+0xB0; inferred: re-applies the event's reverb) and stores the name at manager +0x74.
- The renderer keeps a global vector of reverb-instance objects at 0x00C32554..0x00C32558 (needs at least two; index 0 unused here). It sets the new name on **instance 1** (`0x005C30A0(1, name, fade, time)`, time < 0 becoming 0.33 s from 0x00AFF1C8). If instances 2 and 3 already hold the same name, it swaps the old name onto that instance from the three names at renderer +0x48 (stride 0x1C). (the swap's purpose inferred: no two instances on one preset)
- The instance object (`0x005C23E0`, its index at +0x1A0, 1..3) starts from FMOD's "off" preset built on the stack (31 dwords, `FMOD_REVERB_PROPERTIES`: Instance 0, Environment -1, EnvSize 7.5, EnvDiffusion 1.0, Room -10000, RoomHF -10000, ...). Then:
  - the name `force_generic` (0x00AFF040) takes a fixed generic-like preset (Room -1000, RoomHF -100, ..., flags 0x3F; its constants at 0x00AFF3E0..0x00AFF760, not decoded);
  - any other non-empty name is looked up by the FMOD event system (`[0x00C3250C]`) vtable +0x64 `(name as narrow string, &props, 0)`: **`EventSystem::getReverbPreset` by its shape (inferred)**, so a name the loaded projects lack leaves the "off" preset;
  - an empty name leaves "off".
- `props.Instance` = the index − 1, so instance 1 is FMOD reverb instance 0. With fade 0 the properties are set at once through event-system vtable +0x5C(&props) (`EventSystem::setReverbProperties` by its shape, inferred); otherwise the old properties (+0x11C) are kept as the start (+0xA0) and the target (+0x24) is faded to in an update (+0x198 = 1.0; the fade's update not read).
- **Every trigger and area call passes fade 0.0, so the change is immediate**; the 0.33 s default never applies to them. (checked)
- **The names are `master_reverb.fev`'s presets** (checked in the file): `Padded cell`, `Generic`, `Room`, `Bathroom`, `Living room`, `Stone room`, `Auditorium`, `Concert hall`, `Cave`, `Arena`, `Hangar`, `Carpetted hallway`, `Hallway`, `Stone corridor`, `Alley`, `Forest`, `City`, `Mountains`, `Quarry`, `Plain`, `Parking lot`, `Sewer pipe`, `Underwater`, `Drugged`, `Dizzy`, `Psychotic`, `DA custom small room`, `DA custom medium room`, `DA custom large room`, `DA custom tiled room`, `DA custom cathedral`, `DA custom large chamber`, `DA custom large hall`, `DA custom stone corridor`, `DA custom forest`, `DA custom open space`, `DA custom stone room`, `DA_parking_lot`. The tower uses `Concert hall`, `DA custom large chamber`, `DA custom stone room`, `DA custom stone corridor` (checked). That the project is loaded at start is Q6's question.

##### CCTriggerAudioVolume: the region of sounds

- **Load** (slot 23, 0x0079A5D0, checked): the reverb load (`ReverbPreset`, `Priority`), then 0x0079A000. For each struct of `SoundList` (0x00B0DAC0):
  - a `CCSoundObjectPannable` (0x4E0 bytes, constructor 0x007C2B20, vftable 0x00B223E4) is made with the trigger as its owner (+0x4B8) and the struct's `ObjectId` (0x00B0DAB4, default -1) as its id;
  - it loads the sound's fields (0x007C2880, Q5's fields);
  - 0x00695540(&trigger+0x120, 0) puts it at the trigger's position (inferred: SetPosition; the data agrees, every listed sound has its region's position);
  - its id is stored in the trigger's +0x538 vector. A sound that fails to load is deleted.
- **Add to area** (slot 62, 0x0079B7D0): the base add, then each sound's 0x007C3110(area), which adds the sound to the area (its list +0x694, inferred from 0x006F1340). **Remove** (slot 63, 0x0079AD70): the base remove, then each sound's vtable +0xFC. (checked)
- **Enter and leave start and stop nothing**: they are the reverb trigger's, the preset alone (checked). The region's sounds play as sound objects do (`Active`, the tick: Q5). The pannable's tick (0x007C2EE0, slot override; partly read) does this:
  1. it runs the base sound tick 0x007C2150 and does nothing while the manager's +0x5C is 9;
  2. it takes the listener's position (manager 0x005B5E70) and acts only when (distance from the listener to the trigger's position) − the event instance's +0x10 < the trigger's +0x530 (radius + 4 m) (checked);
  3. when the area's set at +0x368 holds the trigger (inferred: the triggers the listener or controlled creature is in), the event is placed **at the listener** (0x005B67D0) (checked as the call);
  4. otherwise, on a timer (+0x4D0, re-armed with a random value from rand()%100 and the doubles at 0x00B225B0/0x00B225B8), it is placed at the **nearest point of the region's outline** (0x00798DE0: inside test 0x004D4EB0, else the closest point on each edge) and given a direction (0x005B67F0) (inferred from the geometry; the constants not decoded).
  So a region's ambience surrounds the listener inside it and comes from the region's edge outside it. The detail belongs with Q5.
- **Tower example** (`bhm100ar_tower_level_1.are`, checked): eight Type 4 regions, each with presets `DA custom large chamber`, `DA custom stone room` or `DA custom stone corridor`. Each has the same four sounds: `global_amb_glo/exterior_air_wind/amb_ext_subair1_lp` (Volume -2, Priority 61), and `global_amb_glo/interior_ambience/amb_int_ambcrk_sgl`, `amb_int_castle_lp`, `amb_int_crksub_sgl` (Volume -6, Priority 230). All have FadeIn 0, FadeOut 34, MaxDistanceMult 1, cones 360/360/1, Occludable 1, Active 1, Tag the event's last part.

##### CCTriggerAudioMusicVolume: the music state

- **Enter** (slot +0x1E4, 0x00799380, checked): if the controlled creature, manager `0x005B6E80((float)EnterState, EnterStateDelay s)`, then +0x520 = 1.
- **Leave** (slot +0x1E8, 0x00799A80, checked): if the controlled creature and the game mode (app +0x114 → 0x00640060) is not 9 (`GameModeLoading`, `src/dao/GameModes.h`), manager `0x005B6E80((float)ExitState, ExitStateDelay s)`, then +0x520 = 0. While loading, the exit state is not applied (and +0x520 stays set).
- `0x005B6E80` → the current music theme (manager +0x120) `0x005B6CD0`: stores the state at theme +0x8C and the delay at +0x88 (checked). It is the same store `SetMusicIntensity` uses, so the last writer wins.
- **The delay and the parameter** (theme update 0x005BB630, checked): while the manager's state +0x5C is not 9 and the delay is ≥ 0, the delay counts down by the frame's time. When it reaches 0 or below it is set to -1 and `0x005B6DF0(state)` calls the theme content's vtable +0x1C, `CMusicThemeContent_FMOD` slot 7 (0x005C0150): it stores the value at +0x124 and, unless the theme's state +0x128 is 9 or 2, sets it on its three FMOD events (+0x2C, +0x30, +0x34) through 0x005C0110: `Event::getParameter("music_state")` then `EventParameter::setValue(state)` (imports at 0x00ADA66C and 0x00ADA650, checked). A delay of 0 applies at the next update.
- **The parameter's name is `music_state`** (0x00AFF3F8), not `@music_state`: the `@` that string searches show at 0x00AFF3F7 is the last byte of the float 7.5 at 0x00AFF3F4. (checked) What each value of `music_state` selects in `music.fev` is Q4's.
- **Tower example** (checked): one Type 5 region over the whole level, EnterState 3, ExitState 3, both delays 0, ScriptResRefID "", ScriptDelay 0.
- `SetMusicVolumeStateByTag` (0x0067C380 → 0x00799350, checked) stores the new state as `EnterState` (+0x510). If +0x520 is set, it calls enter (+0x1E4) at once with true, so the new state applies with `EnterStateDelay`.

##### Not read (Q7)

- What the creature's trigger-set update (0x006AE100) is called from beyond the two sites 0x006B3028/0x006B31C2, and whether a change of controlled creature re-runs enter/leave for the triggers the new leader stands in.
- Whether the area's +0x12C list (the listener's reverb stack) is cleared on area change or load, and what the area's +0x234 (no script ENTER/EXIT while set) is exactly.
- The reverb renderer's fade update (instance +0x198, +0xA0 → +0x24) and who passes a non-zero fade; the decoded values of the `force_generic` preset; the exact identity of event-system vtable +0x5C/+0x64 (named by argument shape only).
- 0x005B69D0, which every live event instance gets after a preset change.
- The pannable sound's tick in full: the event instance's +0x10, the area's +0x368 set, the timer's constants, and 0x005B67F0 (Q5's to finish).
- Whether a template (`.utt`) of an audio trigger exists in the data. The tower's audio triggers have no `TemplateResRef`, and the template-then-instance order is read only in the base load.
- Who loads `master_reverb.fev` and when (Q6).

### VO as the engine plays it: PARAGON_AUDIO.md questions 2 and 3 (read 2026-10-18 by lib-agent-2, au1)

Read in the GOG exe (`game\DAOrigins.exe`, static disassembly only, no Ghidra, game not run) and in the install's `.fev`/`.fsb`. FMOD is 4.24.24 (both DLLs' version resource). "Checked" means read in the disassembly or the data; "inferred" means not. FMOD enum values used below (callback type 10 = SOUNDDEF_SELECTINDEX, event property indices, FMOD_MODE bits, pitch units) are from FMOD Ex 4.x's headers as remembered, not read from the DLL; each is noted where it carries a claim, and in every case the exe's own use of it agrees.

#### 1. The template VO event: the line's index picks the wave

##### The path (checked)

- `CContent_FMOD` (vtable 0x00AFF30C, 45 slots) wraps one `FMOD::Event*` at +0xA8.
- The content wrapper's Load is 0x005BA530 (`__thiscall (name, flag, ?, arg)`, `ret 0x10`). It first asks the content (vtable slot 1, 0x005C3390) for an event of the exact name. Failing that, if the name holds `/vo/`, it tries the four templates in table 0x00BF9820: VOconversation, VOnarration, VOambient, VOambient_ss.
  - For each template it reads its `all_lines_list` user property and splits it on `;`, each token cut to 15 characters. The index counts from 0.
  - On the first template that holds the line's last path part (`36451_m`):
    - it stops any playing sound of that name (0x005BA100, the address library's `ScriptBackend::ForScript_StopSound`);
    - it calls the content's slot +0xA4 (0x005BEFA0) with (index, line name). That slot stores the index at content +0xC8 and the name at +0xAC. Slot +0xA8 (0x005BEFC0) returns both.
- Slot 1 resets +0xC8 to -1 for every event it loads.
- **The index's use (checked).** Slot 0x005BF0B0 registers `Event::setCallback(0x005BF000, content)` (the only call of the import, thunk 0x009090CA).
  - The callback is `__stdcall (event, type, param1, param2, userdata)`, `ret 0x14`. It acts only when type == 10 and userdata is set. It calls slot +0xA8 and writes the index to `*param2`.
  - Type 10 is FMOD's `FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_SELECTINDEX` (inferred, from the enum as remembered; FMOD calls it for a sound definition whose play mode is "programmer selected", and `*param2` is the entry to play).
  - The sound definition's name (param1) is ignored, so every programmer-selected definition in the event gets the same index.
- **So it is neither a programmer sound nor a parameter.** It is the sound definition's entry, chosen by index. The template event's sound definition lists the waves in the same order as `all_lines_list`.

##### Verified on `bhm100_jowan.fev` and across every VO project (checked)

The FEV1 sound-definition table, as read from the file:
- the count of definitions (u32);
- then per definition: the name (u32 length, then text with its NUL), the property-set index (u32), the entry count (u32);
- then per entry: type (u32, 0 = a wave), weight (u32, 100), the file name (`36451_m.wav`), the bank name (`bhm100_jowan`), the **index of the sample in the `.fsb`** (u32), and a length in ms (u32).

The property sets come before the definitions. There are two in a VO project: set 0 has play mode 5 and set 1 has play mode 3. The play-mode values are inferred (5 = programmer selected, consistent with the callback above).

`bhm100_jowan`:

| Sound definition | Set | Entries | Notes |
|---|---|---|---|
| `/VOconversation` | 0 | 204 | the same 204 names, in order, as its event's `all_lines_list` |
| `/VOambient` | 0 | 8 | the same as its list |
| `/VOnarration`, `/VOambient_ss` | 0 | 0 | lists are `do not delete me` |
| `/36451_m`, `/36641_m` | 1 | 1 each | the lines with their own events (`#duck`, `#music`) |

- The `.fsb` has 214 samples (8 + 204 + 2). Each entry's bank index names the sample whose FSB4 header name equals the entry's file name.
- **All 1,378 VO projects** (en-us, campaign and core): every one of the 1,667 real `all_lines_list` values equals its template's sound-definition entries in order. All 5,512 template definitions use set 0. 4,191 lists are the placeholder.
- Five entries elsewhere name a different sample by index: in `ss_genlock`, `ss_hurlock` and an `arl300cr_demon` `_P` entry. The `ss_*` projects are sound sets with other entry types; not chased.

**For a port:** line `L` of template `T` is entry `all_lines_list(T).index(L)` of `T`'s sound definition. That entry's bank index is the FSB sample, which is also findable by name, `<L>.wav`. A line with its own event (not in any list) plays its own one-entry definition (set 1).

#### 2. VO is 3-D at its speaker, 1.8 m up, and never occluded

##### The event's mode, from the `.fev` (field offsets inferred; values coherent across all projects)

An event header is: the name, a 16-byte GUID, then from there:

| Offset | Field |
|---|---|
| +0 | volume (f32) |
| +0x14 | max playbacks |
| +0x1C | FMOD_MODE (u32) |
| +0x20 | min distance (f32) |
| +0x24 | max distance (f32) |

Tally over the VO projects:

| Template | Volume | Mode | Min–max | Count |
|---|---|---|---|---|
| VOconversation | 0.501 (−6 dB) | 0x04080010 | 0–30 m | 1,399 (one is 0.708) |
| VOambient | 0.501 | 0x04080010 | 0–30 m | 1,399 (one 0–75) |
| VOambient_ss | 0.501 | 0x04080010 | 0–50 m | 1,393 (7 at 0–30) |
| VOnarration | 0.501 | 0x00180008 | 1–10000 | 1,400 |

- 0x04080010 is `FMOD_3D | FMOD_3D_WORLDRELATIVE | FMOD_3D_CUSTOMROLLOFF`: 3-D, rolled off by the event's `(distance)` parameter curve, whose envelopes follow in the file (not parsed).
- 0x00180008 is `FMOD_2D | WORLDRELATIVE | LOGROLLOFF`.
- A line's own event (`36451_m`, `36641_m`) is like VOconversation: 3-D, 0–30 m, max playbacks 2.
- The exe reads the same properties through `getPropertyByIndex` (slot 36, 0x005BE8E0) into an info struct:

| Property index | Info struct field |
|---|---|
| 14 (mode) | +0x14 is3D = mode & 0x10 |
| 16 (rolloff) | +0x18 rolloff kind |
| 17 | +0x1C min distance |
| 18 | +0x20 max distance |
| 19 | +0x24 head-relative = value 0x40000 |
| 36, 37 | fade in, fade out |
| 3 | pitch |

  Load copies max distance to content +0x10, is3D to +0x18 and head-relative to +0x1C (checked). The property-index meanings are inferred from the enum, consistent with the mode bits tested.

##### Where the voice is placed (checked)

- The line's voice is played by the speaker's `AnimBase` (the model wrapper; RTTI `AnimBase*`, functions 0x00A5Cxxx–0x00A5Fxxx). The call is `PlayVO` 0x00A5DA60: `__thiscall (face, ?, event, faceSync, keepPrevious, ?)`, `ret 0x18`. Its callers are ShowLine 0x0068E916, 0x0069FF54, and the ambient manager 0x006FE71B.
- AnimBase holds:
  - +0x1AC: the VO content;
  - +0x1B0: a sound emitter (0x60 bytes, made by 0x005B8AA0, created once);
  - +0x1A8: a byte flag. When it is set, the emitter is placed at the listener (0x005B5E70) instead. Who sets it was not read.
- **Position:** the emitter is placed at the object's position + **(0, 0, 1.8)** (constant 0x00AFEBA0; z is up). This is a fixed height, not a head bone. It is done at PlayVO (0x00A5DC05) and again by AnimBase's position setters 0x00A5CD50 and 0x00A5CDF0 whenever the object's position changes (callers include 0x00752007 and 0x007F2A7A). They store it into emitter +0x30 and mark the emitter dirty (+0x1D). The orientation is passed too (0x00A5CCC0). The voice therefore follows its speaker.
- The emitter is pushed to FMOD by the content's `SetPosition` slot 25 (0x005C0880 → `Event::set3DAttributes(pos, 0, 0)`). The content's slots 26 and 27 set orientation and velocity. The step that copies the dirty emitter into the content each frame was not read (inferred).
- Positions are given to 2-D events too; FMOD ignores them.
- **Occlusion:** Load clears content +0xBD (occludable) when the event's category is VOnarration or VOconversation (0x005BAB98–0x005BABBC). The occlusion test 0x005B7220 needs +0xBD set and a 3-D event. It ray-tests only within 1.25 × max distance, and only for a random share of tries set by `[0x00BF981C]` (100%). Conversation and narration voice is never occluded; VOambient lines can be.
- **Fades:** every event's fade-out is raised to at least 34 ms at load (slot 1: property 37 < 0x22 → 0x22; `sound_settings` row 5's "force minimum fadeout", hard-coded). A 3-D world-relative event's fade-in is raised to at least 100 ms (0x005BAB36–0x005BAB61).
- **Ambient limit:** a VO content in CRambient_ss, VOambient_ss or VOambient is refused when the count of such playing (renderer +0x58) has reached `sound_settings` row 1 (5). The test is the content's slot +0xAC (0x005C32D0); row 1 sits at renderer +0x34.

##### `sound_settings.gda` row 14, the "conversation" ×1.8 (checked)

- The settings loader is 0x005B5F20: table 273 into the renderer (0x005B9020's object). The renderer's constructor sets the same defaults (0x005B8E50).

| Renderer offset | Row | Value |
|---|---|---|
| +0x28 | 0 (stealth ducking) | −12 |
| +0x2C | 14 | 1.8 |
| +0x30 | 9 | |
| +0x31 | 15 | |
| +0x34 | 1 | int |
| +0x38 | 17 (listener balance) | 0.17 |
| +0x3C | 20 | |

  Table 274 (`internal_soundsettings`) rows 0, 1, 10, 5 and 6 go to +0x40, +0x44, +0x48, +0x4C and +0x50.
- **Where it is applied:** the content's Play is 0x005BAC80 (`(startArg, cutsceneScale)`, `ret 8`). When `cutsceneScale` is 1, all of the following must hold:
  - the renderer's game mode (+0x5C) is 4 (dialogue) or 5 (cutscene);
  - the event's **category name does not contain "VO"**;
  - its radius scale (+0x34) is still 1.0;
  - its category is not CRambient_ss, dog_main or dog_ambient.

  Then it sets +0x34 = row 14 and calls 0x005B6AF0. That calls slot 28 (0x005C0790), which only for an event whose rolloff has `FMOD_3D_CUSTOMROLLOFF` (0x04000000) sets the instance's property 18 to (1.8 × the max distance read at load) / the template's property 18.
- In every case under that mode test, it also sets the event's parameter `cutscene` to 1.0, if the event has one (slot 31, `EventParameter::setValue`).
- **Callers passing 1:**
  - the cutscene's sound action `EC::Cutscene::Actor::ActionSound` (vtable 0x00B231F0, slot 1, 0x007D3100);
  - 0x006AA940 and 0x006B6290;
  - the script backend's PlaySound wrappers 0x005BC7F0 and 0x005BC940, with their caller's value.
- **Callers passing 0:** PlayVO (0x00A5DCFB) and the cut's speak-line update (0x007CF9E9).
- **So row 14 never touches VO.** It widens non-VO sounds started in a cutscene or a conversation's cut. The comment "for sounds started in cutscenes" is accurate; "Sound radius multiplier for conversations" is the label. Whether property 18 is the max distance for custom-rolloff events in FMOD 4.24 is inferred.

#### 3. A conversation line without a cut: how long it shows

ShowLine is 0x0068E250 (`GameModeDialog`; dialog object fields below). The update is 0x0068DDD0 (`(dt)`, `ret 4`).

##### ShowLine, non-cut branch (checked)

1. **The previous voice is stopped.** If the dialog's `+0x20C` (the previous line had a cut) is clear, the previous line's speaker (+0x16C) has its VO stopped: AnimBase `StopVO` 0x00A5D360 (deleteEmitter = 1, resetFace = the skip flag +0x2C9). StopVO stops FaceFX, deletes the content (+0x1AC = 0) and the emitter. Then +0x16C = the new speaker, +0x174 = the line, +0x2C8 = 0.
2. **Facing and the camera.** Speaker and listener are faced (0x0069EAA0) and their animation controllers resumed. The dialogue camera is set (0x0068D4B0).
3. **The voice.** The name comes from 0x007F2B70 (`<vobank>/VO/<strref>_m|_f`). Unless the line's `NoVOIngame` (12216, read by 0x0068D8A0) is set, `PlayVO(face, …, event, faceSync, keepPrevious = 0, 0)` is called.
   - PlayVO stops the old VO and any sound of the same name first.
   - It loads the content and places the emitter.
   - When FaceFX has the face animation, it plays through the face (with the content when faceSync); otherwise it plays the content directly (Play(0, 0)).
   - faceSync = (`g_eclipseGame` +0x254 == 0.0). The meaning of that float was not read.
4. **What the movie is given.** hasVO = AnimBase +0x1AC ≠ 0 (0x00A5C9D0), i.e. a VO event was found and loaded. The text and hasVO go to the conversation GUI object (dialog +0x84, 0x0068CBD0: two data slots).
5. **The line timer, dialog +0x2C4:**
   - **NoVOIngame:** T = 1.0 (`[0x00C01814]`).
   - **otherwise:** T = AnimBase 0x00A5D4D0. That is the FaceFX animation's length when the face is playing it, else the event's sound length (content slot 13, 0x005C1D60: the playing channel's `Sound::getLength` in ms, starting the event paused to read it if needed) / 1000. With no VO it is 0.
   - At the end: **timer = max(0, T − 0.3)** (`[0x00C01810]` = 0.3, 0x0068EC02). +0x309 (skipped-this-line) and +0x234 are cleared.
   - The cut branch sets T = the player's +8 (or 1.0 with PlayUntilVo and NoVOIngame) and takes the same − 0.3, which matches PLOTS.md's 0.7.
6. **Animations.** The line's animation (12213, 0x007F0050) plays on the speaker with (anim, 1.0, −2.0, 1). +0x2C8 = RevertAnim (12217, 0x007F00A0).
7. A dialog with `+0x20D` set takes a short path that only sets the text and keeps the old timer. It is likely the slideshow (inferred).

##### The update, non-cut branch (checked)

- **Every frame**, while +0x2BC is clear:
  - the input cooldown +0x258 −= dt;
  - a reply queued at +0x254 is taken (0x0068D8F0) and +0x254 reset.
- **Lone continue.** The kept replies are dialog +0x1A4 (from ShowResponses). When there is exactly one, and its id is −1 or its text is empty, and the speaker's AnimBase holds a VO content and it has **finished** (0x00A5D440: the content no longer playing and the face animation not playing), the continue is queued (0x0068D020(0)). The cooldown +0x258 = 1.0 s is then set.
- **Real replies.** Otherwise, once the speaker's VO has finished, the movie's `Conversation.OnNPCLineFinished` is invoked (0x0068CC60). It is called on every such frame; the movie's handling was not read.
- **The timer** (tail, 0x0068E1C7): while +0x2BC is clear and timer ≥ 0, timer −= dt. When it reaches ≤ 0, `OfferReplies` (0x007F1A40) is called and timer = −1.

##### Answers to Q3

- **A voiced line** offers its replies 0.3 s before its voice ends (the timer). If the only reply is the empty "continue", the line goes on by itself when its voice (and face) has ended, as after a cut. The checks run each frame; the reply is taken on the next update.
- **A line without voice** (no event found, or the speaker has none) has timer 0, so its replies are offered on the next frame. With no VO content, the lone continue is **never** taken by itself: the line stays until the player clicks. Nothing derives a duration from the text length on this path.
- **NoVOIngame:** no voice is played, the replies come after 0.7 s, and there is no auto-continue (no content).
- **After an auto-continue**, the player's reply selection (0x0068D050) and skip are ignored for 1 s (+0x258 > 0).
- **The cut path's lone continue** (0x0068DED1) additionally needs dialog +0x1B4 clear. +0x1B4 is ShowResponses' flag (the NPC line's NoVOIngame, 0x00690B9B), which is cleared when the cut player's PlayUntilVo (+0x11C) is 0. So after a cut the continue is taken by itself unless the line is NoVOIngame with PlayUntilVo set.
- **The subtitles option** (`[0x00C32938]+0x78`) is not read anywhere on dialogue mode's non-cut path in the exe. Only cutscene mode's text (0x00692xxx) and the ambient manager read it. The movie gets the text with HasVO and decides (`conversation.gfx` not read).
- **The fast path** only orders the replies (ShowResponses, as PLOTS.md has it). It plays no part in timing or the auto-continue.
- **Skip** (`HandleMessage_SkipDialogMessage` 0x0068DC10) is ignored while +0x2BC is set or the cooldown runs. Otherwise:
  - It sets +0x2C9 = 1. The next ShowLine's StopVO then resets the face.
  - The first skip of a line sets +0x309 and counts in +0x30C.
  - With a cut player playing (state 3) it seeks to the next skip marker (0x007CF2E0) and returns. Failing that, it sets the player's +0x1B4.
  - If the replies have not been offered (timer ≥ 0), it offers them now (timer −1).
  - If the first kept reply is the empty continue, it takes it at once (0x0068D8F0(0)); otherwise +0x2C9 is cleared again.
  - **The skip does not stop the voice itself.** The voice stops when the next NPC line's ShowLine stops the previous speaker's VO. So skipping a voiced line with a lone continue cuts the voice at once (the next line starts). Skipping one with real replies shows them while the voice plays on until a reply leads to the next line (inferred from the two reads).

#### 4. `#duck` on a VO line, and `#music` (0x005B9750, checked)

The user properties are read at Load (0x005BAB72), each property value in turn (0x005B6720). Spaces are removed and fields split on `,`.

- **`#duck:<category>,<attack ms>,<hold ms>,<release ms>,<dB>[,<pitch>]`** makes a 0x48-byte record, kept in the content's list at +0x78:

| Record offset | Field |
|---|---|
| +0 | the category name |
| +0x24 | attack, s |
| +0x28 | hold, s |
| +0x2C | release, s |
| +0x1C | gain = 10^(dB/20) (0x005B6110; ≤ 0.001 becomes 0, clamped to ≤ 1) |
| +0x20 | the optional fifth value, raw |

  A record with a negative time or a gain outside 0..1 is dropped (0x005B9B69 path).
- **Its envelope** (0x005B6270, state +0x40, elapsed +0x34, output gain +0x38, output pitch +0x3C). It is started when the content plays (Play → 0x005B9170 → 0x005B6240: state 1, elapsed 0 unless already attacking).

| State | What happens |
|---|---|
| 1 attack | gain = 1 − (1 − g)·t/attack, pitch = p·t/attack; when t > attack, state 2 with gain g, pitch p |
| 2 hold | waits for the release flag (+0x44), set when the content stops or ends (0x005B9250 → 0x005B6230, from stop 0x005B9381 and 0x005BDA0C); then counts the hold; when t ≥ hold, state 3 |
| 3 release | gain = g + (1 − g)·t/release, pitch = p·(1 − t/attack). The attack time is used here; it looks like a bug, checked as written. When t > release, state 4 with gain 1 and pitch 0 |

  The ramps are linear in amplitude, not in dB.
- **Applied per frame** (0x005B90E0 from the renderer's update 0x005BD5C8) through the FMOD renderer's slot 12 (0x005C3A80). It writes the gain into the named category's node (node +0x20) and into each sub-category (+0x2C children). Where several ducks meet, the **lowest gain** wins, and for the pitch (node +0x28) the **largest |value|**.
- Each event's volume each update (content slot 40, 0x005C3D10) = category node +0x24 (the option volume) × node +0x20 (the duck) × the event's own +0xD4, through `Event::setVolume`. Its pitch = the event's template pitch (+0xD0) + node +0x28 + its own +0xD8, through `Event::setPitch(…, 0)`; units 0 = raw, inferred.
- So the fifth `#duck` field is a **pitch offset** given to the ducked category.
- **The example:** `#duck:music,500,0,2000,-6` on `bhm100_jowan/VO/36451_m` takes the music category to ×0.501 over 0.5 s as the line starts. It holds while the line plays, then 0 s after it ends returns to ×1 over 2 s.
- **`#pause:<category>`**: the first field only. It is added once to the content's list at +0x88.
- **`#music:<track>[,<n>[,<ms>]]`**:
  - +0x98 = track. +0xB4 = 1 only when `n` > 0 and the track is neither `combat` nor `main`. +0xB8 = ms / 1000.
  - At Play, a non-empty track is handed to the music system (renderer +0x120, 0x005B70C0). With delay ≤ 0 it calls the theme machine 0x005B6D40(track, +0xB4 flag, 0) at once. Otherwise it stores the track, delay and flag at music +0x5C/+0x7C/+0x78 for later (the delayed start is inferred).
  - `#music:bhm_mage_end` therefore changes the music as the line starts, flag 0.
  - What the flag means is question 4's.
- Also at Load: `UseMaxPlaybacksFix` == 1 sets content +0xBF, and `time` (int) goes to content +0x20. Their uses were not read.

#### Not read

- The FEV1 event header's field layout (the offsets above are fitted to the data) and the `(distance)` parameter's rolloff envelopes for VO (30 m custom curve): for the project reader.
- FMOD 4.24's enums, as noted at the top.
- Who sets AnimBase +0x1A8 (the voice at the listener). The emitter → content copy each frame (emitter +0x1D dirty).
- `g_eclipseGame`+0x254 (PlayVO's faceSync condition).
- `conversation.gfx`'s handling of HasVO, the subtitles option and `OnNPCLineFinished`.
- Whether dialog +0x1A4 (kept replies) is cleared before a new line's replies are offered. A stale list would let the lone-continue test see the old one; not seen in play.
- The `+0x20D` short path of ShowLine (slideshow?). The wait loop at 0x0068E705 (0x00619060 on 0x00C33B20).
- Category node +0x24's reset each frame, and the renderer update order.
- Nothing here was seen in play.

### Sounds from animation events, effects and sound sets (PARAGON_AUDIO questions 8, 9, 10) (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by a lib-agent-2 subagent (prefix au5) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, a full-text disassembly (`%TEMP%\lib-agent-2-b2b-b-text.txt`), scratch scripts `%TEMP%\lib-agent-2-au5-*.py`; the tables through ghilana (the copies the game takes); 3,161 readable `.evt` files extracted to `%TEMP%\lib-agent-2-au5-evt` (1,004 more are in enciphered add-ins or other campaigns); the `.fev` projects' strings from disk; the scripts in `toolset\extract\scripts\`. No Ghidra, no game. **[C]** = read in the disassembly or the data; **[I]** = inferred. Addresses are GOG's. Column names are GDA column hashes (CRC32 of the lowercased UTF-16LE name), matched with `tools\gda.py`'s `column_hash`. Not seen in play.

#### Summary

- **Q8.** Every animation event reaches `CCCreature::OnAnimationEvent` 0x006C0BF0, which first calls **0x006B6290, the creature's animation-event sound player**, for every event id. That function switches on the id and does the following:
  1. It builds a lookup key: a kind, a table, a row and a column.
  2. The sound manager's 0x005B8C60 turns the key into one or two FMOD event paths from the `sound_*` tables.
  3. It plays each path on one of two emitters that follow the creature: the body (+0x870) or the main-hand weapon (+0x8D0).

  The ground's surface comes **only from the walk grid's per-cell byte** (`AreaGridSoundData` 3114). That byte is copied to creature+0xB6C: the low nibble is the surface row 0–15 and the high nibble the vegetation row. The creature's columns come from **apr_base** (`Sound_Footstep_Type`, `Sound_Monster_Size`, `Sound_Unarmed`, `Sound_Default_Armor`, `Disabled_*_Sounds`), the weapons' `InvSoundType`, and the armour's `SoundMatType`.
- **Q9.** A `vfx_base` row's `SoundImpact`/`SoundDuration`/`SoundCessation` columns are FMOD event paths. Each plays when the effect *enters* that phase, and only if the effect's tree has the phase's node:
  - impact and cessation are one-shot 3-D sounds at the effect's position;
  - duration is one kept instance whose position is updated every tick and which is stopped at the next phase change or when the effect is destroyed.

  `*Volume` is in dB (10^(v/20), clamped to [0.001, 1]). `*Pitch` goes straight to the FMOD event's pitch, and 0 means unchanged. Nothing is played in GM_LOADING.
- **Q10.** **The engine raises no combat bark itself.** The barks are the scripts' (`sys_soundset_h.nss` `SSPlaySituationalSound` from the rules and creature cores). The engine's only own `PlaySoundSet` callers are the party picker's select (with a 0.1 s cooldown), the GUI's test message, and a GUI-sent `PlaySoundSetEntryMessage`.
  - A bark is a talk-table line's VO in the creature's sound set's own project `ss_<name>`: through the `VOambient_ss` template for companions and the PC, or a per-line event in category `CRambient_ss` for monsters.
  - A new event in category VOambient, VOambient_ss or CRambient_ss is refused while ≥ `sound_settings` row 1 `Value` (5) such events play. The count is redone every frame.

#### Q8. Animation events

##### 8.1 The dispatch [C]

`CCCreature::OnAnimationEvent(event*, bool soundOnly)` 0x006C0BF0 is CCCreature's vtable slot 0xB8; CCObject's slot is 0x00694EE0 and CCPlaceable's is 0x0074C360.
- The event struct: +0 the id (`AnimationEventID`, the events.gda row), +0x1C the string (`AnimationEventString`, wchar*), and +0x18 another string (inferred: the clip's name).
- If `soundOnly` is set, only 0x006B6290 runs.
- Otherwise these run in order:
  1. a switch on the id (non-sound handlers);
  2. **0x006B6290(event)**, the sounds;
  3. 0x006A05B0 (a sub-object at +0xA30, not read);
  4. if the switch did not handle the id, the base 0x00694EE0, which sends ids 9, 10, 11, 13, 16, 33 and 38 to vtable slots.

Native (non-sound) handlers in 0x006C0BF0, for completeness [C]:

| id | handler |
|---|---|
| 1 | 0x006A6230 |
| 2 | 0x006AB040 |
| 3–6 | 0x006AAC00 (equip/unequip, `dev`'s sheathe notes) |
| 7 | 0x006AF430 (ability impact) |
| 8 | sets +0xCF4 from the string |
| 21 | 0x006A5B70 |
| 28 | 0x006AAC20 |
| 29 | 0x006A5C10 |
| 30 | 0x006A62F0 |
| 31 | 0x006AABB0 |
| 32 | 0x006BFC80 |
| 34 and 37 | 0x0069E5B0 |
| 35 | 0x006A0DF0 |
| 36 | 0x006A0E10 |
| 39 and 40 | 0x006A05C0 |

**events.gda's `IsAudio` and `EventName` columns are never read by the exe** (no immediate of either hash) [C]. The ids are hard-coded.

##### 8.2 What each id plays (0x006B6290) [C]

First switch (id − 1, jump tables 0x006B6834/0x006B67FC): the key builder.

| id | events.gda | key builder | table (m2da id), row, column | emitter |
|---|---|---|---|---|
| 1 | AttackImpact | 0x006A04A0 | weapon hit (8.6) | weapon |
| 3 | EquipR | 0x0069E970(…,1) | `sound_weapon_actions` (146), row = right weapon material, column **`Sheath`** | weapon |
| 4 | EquipL | 0x0069E970(…,0) | as 3, left weapon material | weapon |
| 5 | UnequipR | 0x0069E9A0(…,1) | 146, right, column **`Unsheath`** | weapon |
| 6 | UnequipL | 0x0069E9A0(…,0) | 146, left, `Unsheath` | weapon |
| 7 | AbilityCast | 0x006A5B00 | only if the string holds `|PlayHit`: a weapon hit on the target (8.6) | weapon |
| 9 | ProjectileLoad | 0x0069E8E0 | 146, hand by `|Left`, column `LoadProjectile` | weapon |
| 10 | ProjectileFire | 0x0069E910 | 146, hand by `|Left`, column `Fire` | weapon |
| 14 | FootDown | 0x006AAAA0 | footsteps or scuffs (8.3) | the foot instances on the body |
| 17 | MovementSound | 0x0069E7F0 | foley (8.4) | the foley instances on the body |
| 18 | Bodyfall | 0x006A5AD0 | two events (8.5) | body |
| 19 | Swish | 0x0069E940 | 146, hand by `|Left`, column `Swish` | weapon |
| 20 | Playsound | (direct) | **the event's string is the FMOD path** | body |
| 28 | Decapitate | (direct, same case as 20) | the string as the path (empty in both clips that have it, so nothing) | body |
| 2, 8, 11–13, 15, 16, 21–27, 29–40 | | none | nothing; **21 WeaponBlocked and 23/24 MvmtLock play nothing despite `IsAudio` 1** | |

The pieces:
- **"The hand"** (0x0069E7B0) is right unless the string contains `|Left` (0x0069E690 searches for `|Left`, string 0x00B0F2EC). Right takes creature+0x938, left +0x93C. Equip and unequip pass the hand explicitly.
- **The lookup** (sound manager 0x005B8C60(id, out vector, key)) switches on the key's kind [C]:

  | kind | event | lookup |
  |---|---|---|
  | 1 | bodyfall | table 148 `sound_bodyfall_creat_size`[row][column], then table 147 `sound_bodyfall_foley`[row][hash(label of `sound_item_materials` row k+0xC)] |
  | 2 | foley | table 151 `sound_movement_foley`[k+8][k+4] |
  | 3 | weapon action | table 146 `sound_weapon_actions`[k+8][k+4] |
  | 4 | hit | table k+4 [k+0xC][hash(label of `sound_item_materials` row k+8)] |
  | 5 | footstep | k+0xC bit 0 set: table 149 `sound_movement_scuffing`[row][`ScuffingSound`]; else table 150 `sound_movement_footsteps`[row][k+8] |

  - A cell that is missing or empty adds no path.
  - "hash(label)" is 0x005B6F60: table 153 `sound_item_materials`'s `Label` for the row (`Unknown` when empty), trimmed and hashed (0x004C1150 → 0x004CA9A0). So a label names a column (`Armor-Chain` → the `Armor-Chain` column). The case-insensitive match is inferred from GDA's lowercased hashes.
- **Playing**, second switch (0x006B6850/0x006B6860):
  - Ids 1, 3–7, 9, 10, 19 and 21 play through the manager's 0x005BC940(path, emitter, params) on **emitter +0x8D0**.
  - Other ids up to 21, and 18, play on **emitter +0x870**.
  - Id 20's string plays on +0x870 with params flag 4.
  - For the controlled creature (0x006A7D30: its id equals `[[app]+0xD0]+0x5C`), params flags are 0xC: the start's second argument is 1, and instance+0xBD is set to 0. For others the params are null and +0xBD keeps its default of 1.
  - +0xBD is tested by the manager's 0x005B7220, a distance/occlusion pass against the listener [I]. So the controlled creature's own sounds skip it [I].
  - 0x005BC940 does this: a new 0xC8 instance; resolve (0x005BA530); 0x005BAF30(0); attach to the emitter (0x005B9520); then volume (flag 1), pitch (flag 2), +0xBD (flag 8) and a named parameter (flag 0x10); then start (0x005BAC80).
- **The emitters** are updated every tick by 0x006AA4D0, from CCCreature's tick (vtable slot 0x160, 0x006BD4D0 → 0x006AD8F0) [C]:
  - **+0x870 (body)**: the creature's position, z raised by `vtable+0x1B0(0) × 0.5` (inferred: half its height), or 0.9 m without appearance data.
  - **+0x8D0 (weapon)**: the world position of the model's wield slot 3 (the main-hand item, `AnimBaseWield` slot table at +0x224; 0x00A66B40), else the body's.
  - Every 0.3–0.5 s (+0xA04) a non-controlled creature's emitters take the area's reverb for their place (0x0067DCB0 → 0x005B76B0).
- **The creature's sound materials**, set by 0x006A0910(keepFootstepAndSize) from apr_base (table 2) at row `[+0x7B0]+0x58` [C]:
  - +0x950 = hash(`Sound_Footstep_Type`), +0x934 = hash(`Sound_Monster_Size`). These are only set when the argument is 0.
  - **+0x938/+0x93C (right/left weapon material)**: apr_base `Sound_Unarmed` when `Disabled_Weapon_Sounds` is set or the hand is empty, else the item's bitm_base `InvSoundType`. Longsword and greatsword give 1 Sword, dagger 4, mace and maul 3, axes 2, staff 7, bows 8, crossbow 9, shields 5/6, creature weapons 0.
  - **+0x930 (armour material)**, in this order:
    1. apr_base `Sound_Default_Armor`;
    2. unless `Disabled_Armor_Sounds`: if the slot-0 material index (appearance+0x148[0]) is > 0, `materialtypes` (table 89) `Sound_MatType` for it;
    3. then, if > 0, the slot-0 item's variation table (bitm_base `Variation_Worksheet`, row = the item's variation +0x138[0]) `SoundMatType` (0x007485D0).
    4. Clamped: anything outside 10–13, 16–18 and 23–25 becomes **16 (Flesh)**.
  - Slot 0 is inferred to be the chest.
  - Example: apr 1/2/3/15 (humanoids) are `Humanoid_Large` (elves `Humanoid_Small`), `Medium`, `Sound_Unarmed` 19 BareHands, `Sound_Default_Armor` 16.

##### 8.3 Footsteps and scuffs (id 14) [C]

- **Gate 0x007C4A80**: it refuses when all of these hold:
  - the creature's size hash +0x934 is `Medium` or `Small`;
  - its model's state `[+0x154]+0x150` equals 3 (not identified; inferred an LOD or culled state);
  - the game mode is not 4 (dialog) or 5 (cutscene).

  The same gate applies to foley (17) and vegetation.
- **Skipped** for the controlled creature when its appearance row is 12 and walk/run is 0 (as read; the purpose is not known).
- +0x958 = the movement state (+0xB20). Scuff = the string contains `|Scuff` (0x00B0FC1C).
- **Key**: kind 5, row = **creature+0xB6C & 0xF**, column = +0x950.
  - The path is `sound_movement_footsteps[surface][Sound_Footstep_Type]`, e.g. `glo_fly_fs/humanoid/large/stone`.
  - Or `sound_movement_scuffing[surface].ScuffingSound`, e.g. `glo_fly_fs/scuff/stone`.
- **Play** (second switch, case 14):
  - **Spider** (+0x950 == hash `Spider`, 0x9378FA6A): a new instance each step, attached to the body emitter, with the parameter `walk/run` = +0x954. No rate limit.
  - **Everyone else**: one persistent instance per foot (made in the constructor and attached to the body emitter):
    - **left** (string has `|Left`): instance +0x948, last-time +0x940;
    - **right**: instance +0x94C, last-time +0x944.
  - A step plays only if `GetTickCount() − 560 > last` for that foot (0x230: sound_settings row 3's 560, hard-coded; the row says "not implemented"). It is wall-clock time, not game time.
  - 0x006AA940(instance, path, &handle) runs these steps:
    1. it sets instance+0xBD = 1 (0 for the controlled creature);
    2. it stops the instance;
    3. it re-resolves the event only if the path changed;
    4. it sets **`walk/run` = creature+0x954**;
    5. it starts the instance.

  The handle is then handed to the model with the event's +0x18 string (0x00A60260). Inferred: so the sound stops with the clip.
- **`walk/run`** (+0x954) is ramped in 0x006AD8F0(dt) by the movement state +0xB20 [C]:
  - 2: += dt, clamped to 1 (inferred: running);
  - 1 or 4: −= dt, clamped to 0;
  - 0 or 3: set to 0 at once.

  So it is a 0..1 parameter that takes a second to change. glo_fly_fs.fev names `walk/run` 54 times and glo_fly_mv.fev 8 times.
- **The surface byte** creature+0xB6C [C]:
  - It is written in **CCCreature vtable slot 0x7C** (0x006B5EF0, inferred SetPosition), at 0x006B608F, from the world's grid query 0x00A46870 (`[app]+0x118`, the position and the area id).
  - In game modes 4 and 5 it is also refreshed every tick from the model's animated position (0x006AA5EB).
  - The query returns `AreaGridSoundData` (3114; `dev/COLLISION.md`).
  - Low nibble → `sound_movement_footsteps`/`scuffing`/`bodyfall*` row (0 Unknown, 1 Wood, 2 Sand, 3 Leaves, 4 Grass, 5 Stone, 6 Dirt, 7 Puddle, 8 Water, 9 Mud, 10 Ice, 11 Snow, 12 Hollow_wood, 14 Gravel, 15 Carpet; there is no 13).
  - High nibble → `sound_vegetation_type` row (0x006A05A0).
  - A cell byte of −1 would read as 15 Carpet / vegetation 15. Inferred: creatures do not stand on such cells.
- **`MMHNodeSoundMaterial` (6330)** is read only by the model loaders (push 0x18BA at 0x00595701, 0x0059EFA7, 0x005A9ECF, 0x005AB6CA), not by footsteps [C]. Its use is presumably occlusion with `sound_geometry_mat_types` (question 5) [I].
- **`TerrainSoundData` (7076)** is never pushed as a field id in the code [C]. Inferred: unused.
- **Data fault [C, strings]**: glo_fly_fs.fev has no `water` event. Rows 7 Puddle and 8 Water (`…/water`) name events that don't exist, so stepping in water is silent (inferred). `carpet` exists only for humanoids and scuffs. The `Shale` column is empty everywhere.

##### 8.4 Foley, movement sound (id 17) [C]

- Gate 0x007C4A80 (8.3).
- **Key**: kind 2, row = **+0x930** (armour material), column = **hash(the event's string)**: `Small` 2,095 events, `Medium` 334, `Unknown` 83, empty 8 (the clips' survey).
- The path is `sound_movement_foley[armour][string]`, e.g. `glo_fly_mv/chain_armor/smll_move`. Flesh, Fur and Scale rows are empty; golems have their own.
- **Play**: at most one every 260 ms (0x104: row 4, hard-coded) on `GetTickCount` with +0x964. The instances alternate between +0x95C and +0x960 (toggle +0x968), through 0x006AA940 (so `walk/run` is set too).
- **Vegetation** is not an animation event [C]. Every tick, 0x006A5510 runs these steps:
  1. the gate;
  2. if the squared distance moved since the last rustle (+0x9F0) is greater than +0xA00 (0.8 + 0.004·(rand%100), so about 0.9–1.1 m);
  3. and the grid's high nibble is > 0;
  4. it plays `sound_vegetation_type` (155)[veg][`Sound_Monster_Size`] as a one-shot at the creature's position (PlaySound 0x005BC7F0);
  5. it resets +0x9F0 and draws a new threshold.

  Rows: 1 Shrubs, 2 High Grass, 3 Flower, 4 Wheat.

##### 8.5 Bodyfall (id 18) [C]

Key kind 1: row = the surface (0x006A0590), column = +0x934 (`Sound_Monster_Size`), material = +0x930. **Two events** on the body emitter:
1. `sound_bodyfall_creat_size[surface][size]`, e.g. `glo_fly_bf/bf_master/bf_creature_size/bf_creat_med/stone`. The columns are Unknown, Small, Medium, Big and Huge.
2. `sound_bodyfall_foley[surface][label(armour)]`, e.g. `…/bodyfall_foley_sweeteners/chain_armor_foley`. The columns are Unknown and Armor-Leather/Cloth/Chain/Metal, so Flesh, Fur and the others give only the first event.

##### 8.6 Weapon hits (id 1, and id 7 with `|PlayHit`) [C]

- **AttackImpact** (0x006A04A0): the attack record for the hand (creature+0xB54, 0x00697220(0 right / 1 left)).
  - Its result +0 equal to 0 (invalid), 3 (miss) or 8 (engine-only) plays nothing.
  - The target object (record+4) decides the lookup:
    - **a creature**: table 154 `sound_weapon_hits`, row = the attacker's weapon material for the hand, column = label(**the target's +0x930**). E.g. Sword on chain → `glo_fly_wep/weapon_impacts/sword/impact_armor/chain_arm`. There is no `Steel_Golem` column, so hits on steel golems are silent [C, data].
    - **a placeable** (vtable +0x24): table 93 `placeable_types`, row = the placeable's appearance (+0x4F0), column = label(the attacker's weapon material), e.g. `Sword` → `…/sword/impact_materials/wood`.
  - With no record, the key is left kind 4 with table 0 [I: nothing plays].
- **AbilityCast with `|PlayHit`** (0x006A5B00; string 0x00B0F2F8): the target is the creature's current target (0x006A0180 → +0xB54).
  - If it is another creature, its +0x930 is used; else 0 (`Unknown` column).
  - Table 154, row = the caster's weapon material (hand by `|Left`).
  - 69 + 12 events in the clips carry `|PlayHit`.

##### 8.7 Weapon actions: data notes [C]

- `sound_weapon_actions`' columns are swapped in the data for melee weapons: **`Sheath` (played on EquipR/L, the draw) holds `…/movement/unsheath`** and `Unsheath` holds `…/sheath`. So the draw sounds right.
- Bows and crossbows have them unswapped, so a bow draw plays `bow/movement/sheath`.
- The Staff row holds the crossbow's events.
- Swish rows: Sword, Axe, Blunt, Dagger, Bow (arrow), CrossBow, BareHands, Claws, Shields. The Staff row is empty.

##### 8.8 The clips' events (survey of 3,161 readable .evt) [C]

| id | count | strings |
|---|---|---|
| 14 | 7,528 | `''`, `|Left`, `|Scuff`, `|Left|Scuff` (repeated `|Left` prefixes appear; only the substring matters) |
| 17 | 2,520 | `Small`, `Medium`, `Unknown` |
| 16 | 2,397 | effects' emitter names |
| 20 | 1,192 | FMOD paths: `glo_fly_wep/.../swish`, `glo_dog/dog/ss/...`, `vfx_spells/vfx_new/abilities/ability_swing_med`, `glo_fly_fs/dog/dirt`, `glo_fly_mv/broodmother/...` |
| 7 | 282 | |
| 19 | 221 | |
| 18 | 158 | |
| 1 | 126 | |

- 27 `PlaceableSound` (65) belongs to placeables: CCPlaceable's handler 0x0074C360 looks up `placeable_types[appearance][the string]`, e.g. `ClosedToOpened` → `glo_fly_plc/da_doors/door_open/small_open` [C, the lookup; its play not followed].

##### 8.9 Event paths, as Paragon should build them

```
surface = gridSoundByte(pos) & 0xF ; veg = gridSoundByte(pos) >> 4
apr     = apr_base[appearance]
foot    = apr.Sound_Footstep_Type ; size = apr.Sound_Monster_Size
armour  = clamp16(materialtypes/variation SoundMatType or apr.Sound_Default_Armor)
wmatR/L = item ? bitm_base[item].InvSoundType : apr.Sound_Unarmed   (Disabled_Weapon_Sounds → Sound_Unarmed)
hand    = id in {3,5} → R ; {4,6} → L ; else ("|Left" in s ? L : R)
14: s has "|Scuff" ? sound_movement_scuffing[surface].ScuffingSound : sound_movement_footsteps[surface][foot]
    one per foot per 560 ms wall-clock; param walk/run ∈ [0,1]; body emitter
17: sound_movement_foley[armour][s]          one per 260 ms, alternating two instances
18: sound_bodyfall_creat_size[surface][size] + sound_bodyfall_foley[surface][item_materials[armour].Label]
19/9/10: sound_weapon_actions[wmat(hand)][Swish|LoadProjectile|Fire]   weapon emitter
3,4 / 5,6: sound_weapon_actions[wmat(hand)][Sheath] / [Unsheath]
1: result ∉ {0,3,8}: creature → sound_weapon_hits[wmat(hand)][item_materials[target.armour].Label]
                     placeable → placeable_types[plc.appearance][item_materials[wmat(hand)].Label]
7 with "|PlayHit": sound_weapon_hits[wmat(hand)][item_materials[target.armour or 0].Label]
20: s itself                                  body emitter
vegetation (per tick, ≥ ~1 m moved): sound_vegetation_type[veg][size], one-shot at the position
```

Stealth's −12 dB on the foot and foley instances is in `%TEMP%\lib-agent-2-audio-listener.md` (row 0).

#### Q9. Effects' sounds [C]

- **The row** (`vfx_base`, loader 0x007E9E00; cached at 0x00C11978):
  - `SoundImpact` +0x6C, `SoundImpactVolume` +0x88, `SoundImpactPitch` +0x8C;
  - `SoundDuration` +0x48, `SoundDurationVolume` +0x64, `SoundDurationPitch` +0x68;
  - `SoundCessation` +0x90, `SoundCessationVolume` +0xAC, `SoundCessationPitch` +0xB0.

  Volume and pitch are read (as floats) only when the name is not empty.
- **When**: `CCVisualEffect` SetPhase 0x007EAB50(phase) maps the phase to its animation (table 0x00B23D94) and asks the tree for it (0x00A5D920). **Only if the tree has it** does it call the phase sound 0x007EA220(phase), set +0x444 and play the clip. So a phase the tree lacks is skipped without its sound (the advance 0x007EAC30 moves on).
- **0x007EA220(phase)** runs these steps:
  1. It returns when the phase is unchanged or the **game mode is 9 (loading)**.
  2. It stops the duration instance +0x458 if there is one (0x005B9330(0)).
  3. The position is the effect's +0x120. If the effect has a model and x and y are both 0, it uses the model's animated position (0x00A5D9B0).
  4. By phase:
     - **1 cast**: no sound.
     - **2 impact**: if `SoundImpact` is not empty, a **one-shot** PlaySound (manager 0x005BC7F0(path, &pos, 0, params{flags 3, volume, pitch})), 3-D at the position. It is not stopped later.
     - **3 duration**: one instance kept at +0x458, made and resolved once. It gets its volume (0x005B9450) and pitch (0x005B6980), then `set3DPosition` (0x005B67D0), then start(0,0). Looping is the FMOD event's own (no loop in the exe).
     - **4 cessation**: like impact, with `SoundCessation`.
- **Following**: the effect's vtable slot 0x160 (0x007E9B80, the per-object tick slot, as the creature's and sound object's) moves the duration instance to the effect's current position, or the model's animated position, every tick.
- **Stop** (0x0079E3E0 → 0x007EAB50(4)) goes to cessation. That stops the duration instance and plays the cessation one-shot. If the tree has no cessation clip, the effect is destroyed (0x0079DF10). The destructor 0x007EA140 frees +0x458 (0x005BA350, inferred stopping it). The impact one-shot runs on.
- **Units**:
  - Volume 0x005B9450 → 0x005B6110: linear = 10^(dB/20); 0 below 0.001; clamped to 1; to the FMOD event's `setVolume` (vtable +0x4C). So the columns are dB, 0 = unchanged.
  - Pitch goes straight to the event's vtable +0x50 (setPitch); 0 = unchanged [I: FMOD's offset units].
  - All 993 rows have 0 for every volume and pitch (one row −60, row 0 INVALID), so in practice: play at the project's own settings.
- **Data**: 601 rows have an impact sound, 251 a duration and 231 a cessation.
  - 1,061 paths are `vfx_spells/vfx_new/<school>/<name>_imp_n|_dur_n|_ces_n`, e.g. the wisp 3006 `vfx_spells/vfx_new/monster/fxc_wisp_c_{imp,dur,ces}_n`, all present in vfx_spells.fev.
  - 17 are `glo_fly_plc/placeable_vfx/…`.
  - **5 are `x_spells/vfx_new/mind/fxa_dispel_imp_c_n`** (90185, 90186, 90188, 90191, 90192): a typo for `vfx_spells`, which won't resolve (inferred silent).
- **Spells, then**:
  - The conjure effect (`ConjureVFX`, a D-type effect) gives its impact sound when the conjure starts, its duration loop through the conjure, and its cessation at the end.
  - The scripts' impact effects (`VFX_Impact0/1`, F-type) give their impact sound.
  - Lingering D effects give impact + loop + cessation. See `dev/EFFECTS.md`.

#### Q10. Sound sets

- **The creature's set** is creature+0x96C, a name such as `ss_alistair` or `ss_pc_mhuman_wise`. It comes from the template's `SoundSet` field (string 0x00B10728, read in the creature's load at 0x006C08DF [C]; the call into SetSoundSet from there is not followed) and from `SetSoundSet` 182 (0x00664350 → `CCCreature::ForScript_SetSoundSet` 0x006BD0E0) [C]:
  1. It stores the name.
  2. It releases the previous one (0x005B8630).
  3. It loads the project of that name through the renderer (manager+0x94 → 0x005B9620) [I: `ss_<name>.fev`/`.fsb` under `audio/vo/<lang>/vo/`; 217 core en-us sets].
  4. It reads the project's `VO` user property `Disable2DAProbabilities` (0x00B10670); if it is 1, creature+0x969 = 1, so every bark plays (probability 1.0).
  5. It reads `sse_<name>` (0x00B102EC) `StrID` rows to pre-fetch `<strref>_m.fxe` faces into +0x9A4 [I].
- **playersoundsets.gda**: 35 rows, `Label`, `StrIDName`, `Gender` (1 m, 2 f), `Race` (1 dwarf, 2 elf, 3 human), `SoundSet` = `ss_pc_{m|f}{human|elf|dwarf}_{wise|cocky|exp|mystic|sultry|suave|smart|violent}`. Character creation passes the chosen one to `SetSoundSet` (`CHARGEN_SELECT_SOUNDSET`, the chargen notes).
- **PlaySoundSet** (reuse `%TEMP%\lib-agent-2-scg-sounds.md`): `sst_<set>.Count[entry]`, a shuffle bag over `sse_<set>` rows entry·100+i, `StrID`; probability as there. **`ss_types`** gives these per entry:
  - `EntryIdx` = entry·100;
  - `Probability` (e.g. 1 COMBAT ATTACK 0.2, 15 ATTACK GRUNT 0.5, 16 PAIN GRUNT 0.5, 18 DEATH 1.0, 21 ENEMY KILLED 0.1);
  - `BRestricted` (3, 4, 5, 6, 17: the scripts' once-until-reset flags);
  - `BUseProbablityModifier`.

  The line plays through 0x0069FE50(StrID, force) [C, calls]:
  - the path comes from the VO builder 0x007F2B70 with the creature's set name as the bank: **`<ss_set>/VO/<strref>_m|_f`**, the same rule as conversation lines;
  - it is played with lip sync on the creature's voice object (0x00A5DA60).
- **How it resolves (data, strings)**:
  - **Companion and PC sets** have no per-line events: the line plays through the **`VOambient_ss` template**, whose `all_lines_list` holds `179932_m;179933_m;…` (ss_alistair). Its category is VOambient_ss.
  - **Monster sets** (ss_bear and others; ss_abomination, ss_arcane_horror, ss_ashwraith, ss_bronto, ss_broodmother, ss_deepstalker, ss_deer have the category) have **per-line events** (`280341_m`…) in category **`CRambient_ss`**.
- **The cap (sound_settings row 1)** [C]:
  - The manager reads table 273 `sound_settings` column `Value` (not Default/Xbox/Ps3) at 0x005B5F3E. Row 1 is truncated to an int into settings+0xC (manager+0x34): **5**.
  - Each frame the manager's update (0x005BD880) recounts its live instances whose category is `VOambient`, `VOambient_ss` or `CRambient_ss` into manager+0x58.
  - When an event is resolved (0x005BA530), if the limit is ≥ 0, `CContent_FMOD` slot 0xAC (0x005C32D0) answers whether the event is ambient VO. That needs:
    - a global flag [0x00C32510] (not read);
    - the path containing `/VO/`;
    - its category being one of the three.
  - If it is, and the count is ≥ the limit, **the event is refused** (silently).
  - Row 18 ("Max ambient VO playback") is not on this path (not read).
- **Who barks** [C, scripts]:
  - **The engine itself raises no combat bark.** Its `ForScript_PlaySoundSet` (0x006B5740) callers are:
    - `GUI::DragonAgeGUISystem::HandleMessage_TestSoundSetMessage` 0x0060BA20 (forced, 1.0);
    - `CCCreature::HandleMessage_PlaySoundSetEntryMessage` 0x006B7590 (1.0; the message is made by GUI factories 0x00607E20/0x0060AD80, so it is sent from the Flash UI);
    - the party picker's select/deselect (0x00766040/0x007661B0 under `GameModePartyPicker::HandleMessage_SelectPartyMemberUnderCursorMessage` 0x00768AE0), with an animation and a 0.1 s cooldown;
    - the script command 479.
  - The scripts' `SSPlaySituationalSound` (`_Systems/_Includes/sys_soundset_h.nss`) plays only in GM_COMBAT or GM_EXPLORE, for a valid, active, living creature (dying allowed):

    | Raised from | Situation | Combat entry | Explore entry |
    |---|---|---|---|
    | `rules_core` COMMAND_PENDING, an attack accepted (:188) | BATTLECRY | 1/12 TAUNT, 6/12 ATTACK, else BATTLE CRY | WARCRY or BATTLE CRY |
    | DAMAGED, damage ≥ 5 (:461) | GOT_DAMAGED | PAIN GRUNT | |
    | COMMAND_COMPLETE (:751) | COMMAND_COMPLETE | health < 0.15 NEAR DEATH, < 0.25 HEAL ME, mana/stamina < 0.15 MANA LOW (mages) or STAMINA LOW | HEAL ME |
    | PERCEPTION_APPEAR (:944, :1111) | ENEMY_SIGHTED | none | ENEMIES SIGHTED by the enemy's race: darkspawn, animal, beast, undead, demon, dragon, other |
    | spell interrupted (:443) | SPELL_INTERRUPTED | SPELL FAILED | |
    | `combat_h` :1174, a melee hit that is not a deathblow | ATTACK_IMPACT | ATTACK GRUNT | |
    | `combat_h` :1323, the last hostile gone | END_OF_COMBAT (a random member) | CHEER PARTY | |
    | `creature_core` :178, DYING | DYING | DEATH (bosses forced at 1.0) | DEATH |
    | `creature_core` :221 (the killer) | ENEMY_KILLED | ENEMY KILLED | |
    | `player_core` :440 | PARTY_MEMBER_SLAIN | MONSTER SLEW PARTY MEMBER (a hostile killer) | |
    | ON_SELECT (:865) | SELECTED | SELECT NEUTRAL | SELECT by approval |
    | event 94 PLAYER_COMMAND_ADDED (:881) | ORDER_RECEIVED | START TASK | START TASK |

    - ORDER_RECEIVED uses the command table's `SoundsetProbability`, is not played while stealthy, and gives no move barks in explore.
    - Restricted entries set a per-creature flag (locals by entry/32) that the heal/damage resets clear.
    - Others: `utility_h` :669 YES, `ability_core` :219 CANNOT DO, `player_core` :42 EXPLORE HEAL ME, `ai_main_h_2` :3669 ENEMIES SIGHTED OTHER, and area scripts.
- **The categories**: the options' voice volume sets VOambient_ss and CRambient_ss (PARAGON_AUDIO §3). The ducking events move CRambient_ss (OnCutscene −9 dB, OnGamePause −6 dB) and pause VOambient_ss in OnGamePause (`%TEMP%\lib-agent-2-audio-listener.md`).

#### Not read

- The model state `[+0x154]+0x150` == 3 in the gate 0x007C4A80, and why appearance 12 skips the controlled creature's footsteps.
- 0x00A60260 (the footstep/foley handle kept by the clip name) and whether those sounds stop with the clip.
- The meaning of instance+0xBD and the start's second argument in 0x005BAC80, beyond the distance/occlusion test in 0x005B7220.
- Attack result 8, 0x006A0180/0x006972A0 (the "current target" of the ability-cast hit), and the slot-0 item being the chest.
- The spider's per-step instance after a failed start (the +0x890 list).
- Whether the template's `SoundSet` reaches `SetSoundSet` at load, 0x005B9620's project load in full, and the `sse_` face pre-fetch (+0x9A4).
- The global flag [0x00C32510] in the ambient-VO test, and `sound_settings` row 18.
- `MMHNodeSoundMaterial`'s consumer (presumably occlusion, question 5).
- CCPlaceable's 27 handler past the lookup; 0x006A05B0.
- Events from enciphered add-ins' clips (1,004 not surveyed).
- None of this was seen in play.
