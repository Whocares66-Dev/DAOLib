# What Paragon needs for sound (FMOD Ex)

Paragon (paragon-agent, 2026-10-18) plays no sound yet: no voice, music, ambience, effects or interface sounds. Lip-sync needs the voice, and conversation lines end on their voice. Paragon issue #30. A subagent of paragon-agent surveyed the install and read the exe only as far as needed to plan; what follows is what it found and what it asks for. "Read" is code seen, "inferred" is not. Paragon's side is its `dev/AUDIO.md`.

Paragon has a stopgap meanwhile (its `crates/paragon/src/fsb.rs`, branch feat-audio): an FSB4 sample table read by name, enough to play a line's voice. It is to go when core reads the banks.

## 0. The data, surveyed (GOG 1.05, read only)

- 9,052 `.fsb` and 9,020 `.fev`, every one of them plain (FSB4 and FEV1; the add-ins' and offers' too, none enciphered). Core's install index leaves them out on purpose (`Install.cpp`: "FMOD's audio (.fsb, .fev, some 18,000 files) is left out").
- `modules/single player/audio/vo/<lang>/vo/`: the campaign's voice, 1,184 project and bank pairs per language (en-us 1.8 GB, de-de 2.1, fr-fr 2.0, pl-pl 2.3, ru-ru 1.8); en-us: 41,649 samples, 54 hours, every one MPEG layer III, MPEG-2 at 24 kHz, mono. Beside it `facialanimations/modulefacialanimations.erf` (the `.fxe` faces).
- `packages/core/audio/vo/<lang>/vo/`: 218 pairs (en-us 186 MB, 13,859 samples, 5.7 h, MPEG, 24 kHz but 23 at 48 kHz).
- `modules/single player/audio/sound/`: 161 banks, 188 MB: areas' and cutscenes' own (`tower_level_1`..`4`, `bhm_intro`, `bhm500cs_harrowing`, `cir_extra`, `conv_*`); 1,672 samples, 1,359 IMA ADPCM, 313 MPEG.
- `packages/core/audio/sound/`: 21 banks and 19 projects, 515 MB; 4,146 samples, 3,725 IMA ADPCM and 421 MPEG. Music (`music.fev`, `music_cutscene.fev`; `music_bank_strm_new.fsb` 310 MB of MPEG-1 layer III, `music_notstream_sting`, `music_bank_strm_death`), ambience (`global_amb_glo`, `_wal`, `_orz`, `_fade`), footsteps `glo_fly_fs`, movement `glo_fly_mv`, weapons `glo_fly_wep`, bodyfalls `glo_fly_bf`, placeables `glo_fly_plc`, creatures `glo_amb_creat`, `glo_dog`, `c_dragona`, spells `vfx_spells`, the interface `glo_gui.fev` over `gui.fsb`, `slo_blows`, the reverb presets `master_reverb.fev`, the categories and ducking `audio_categories.fev`. `packages/core_ep1/audio/sound/` is Awakening's set (306 MB).
- Rates 4 to 48 kHz, mono and stereo. No sample has FMOD's loop flag: loops are the projects'.

## 1. Core: the banks (`.fsb`, FSB4)

Every bank is FSB4, version 0x00040000, header mode 0x20 (MPEG frames padded). The layout, as read from the files: a 48-byte header (`FSB4`, sample count, size of the sample headers, size of the data, version, mode, 8 zero bytes, a 16-byte hash), a header per sample, then the samples' data in order, each a multiple of 32 bytes. A sample's header: its size (u16; 80, or up to 536 in some core banks: 12 to 456 bytes more, likely sync points, which the exe asks FMOD for, `Sound::getSyncPoint` imported; unread), its name (30 bytes, `<strref>_m.wav` in a VO bank), length in samples, stored length, loop start and end, mode, default rate, volume, pan, priority, channels, 3-D min and max distance (1 and 10000 everywhere seen), three variations. Mode bits seen: 0x200 MPEG, 0x400000 IMA ADPCM, 0x20 mono, 0x40 stereo, 0x2000 2-D, 0x100000 3-D.

Asked of core:
- The audio files in the install index, each name resolved to the copy the game takes, by the priorities `Install.cpp` already names (AUDIO_VO 100006, AUDIO_SOUND 100008; the campaign's voice 50006 and sound 50007; an add-in's sound -1 and voice -2) and the language.
- A bank reader: the samples (name, frames, rate, channels, codec, loop points, 3-D distances, sync points) and a sample's bytes by index or name.
- IMA ADPCM decoded to 16-bit PCM. FMOD's layout here: blocks of 36 bytes a channel (an i16 predictor, a u8 step index, a reserved byte, 32 bytes of nibbles), 64 samples a block: every mono sample's length is its block count times 64 (checked on `gui.fsb`, `global_amb_glo.fsb`). It is Xbox IMA as vgmstream names it; the stereo interleave is to be confirmed. MPEG may be left as bytes: Paragon decodes it with symphonia (MPL-2.0); core could use minimp3 (CC0) if Eluvian or ghilana want WAV.

## 2. Core: the event projects (`.fev`, FEV1)

`FEV1`, then 0x003A0000 (FMOD Designer 4.x). What a project holds, from its strings: its name; its wave banks (name and flags; `music_bank_strm_*` stream); the category tree with volumes (master, music, VOconversation, VOnarration, VOambient, VOambient_ss, footsteps, movement, combat, ambient, vfxambient, placeables, cutscene, amb_oneshots, unducked, stinger, ...); event groups and events (addressed as `project/group/.../event`); per event its properties (volume, pitch, priority, max playbacks, 2-D or 3-D, rolloff, min and max distance, cone, fades, looping), layers of sound instances over parameters (`(distance)`, `param00`, `@music_state`, `walk/run`), effect envelopes (`FMOD Lowpass Simple`), sound definitions (their play mode and entries: a wave by name in a bank, weights, randomisation) and user properties. There is no FMOD interactive-music data: the game's music is its own (section 3).

Asked of core: a reader returning a plain description Paragon can play, as Cutscene.h's: categories, events by path with their properties, layers, parameters, envelopes and sound definitions, user properties, wave banks, and `master_reverb`'s reverb presets.

The event parameters the exe reads by name (0x005B9750, read). These are the names of an event's parameters, not user properties: the exe walks the event's parameters by index and reads their names (corrected 2026-10-18 by lib-agent-2):
- `#duck:<category>,<attack ms>,<hold ms>,<release ms>,<dB>[,<fifth>]`: the category ducked while the event plays (`#duck:music,500,0,2000,-6` on a VO line).
- `#pause:<category>,...` (`audio_categories/ducking/OnGamePause` pauses movement and footsteps).
- `#music:<track>[,<n>,<ms>]`: a VO line that changes the music (`#music:bhm_mage_end` in `bhm100_jowan`).
- `all_lines_list`, `UseMaxPlaybacksFix`, `time`, `Disable2DAProbabilities` (below and section 3).

## 3. The exe: how sound plays (lib-agent-2)

Found so far (classes from RTTI: `EclipsePrivate::CContentRenderer_FMOD`, the system; `CContent_FMOD`, an event; `CContentBank_FMOD`; `CMusicThemeContent_FMOD`; FMOD_EVENT.DLL and FMODEX.DLL imported):
- **A line's voice** (0x007F2B70, read): `<vobank>/VO/<strref>`, with `_f` when the player's creature's gender is 2 and that event exists (0x005BBBD0), else `_m`; the same name gives the face. A conversation's bank is `ConversationVOBank` (12004); a cut's speak-line action holds its own (5563).
- **Template VO events** (0x005BBBD0, 0x005BA530, read): when a project has no event of the line's name and the name holds `/vo/`, the project's VOconversation, VOnarration, VOambient and VOambient_ss events (table 0x00BF9820) are tried: the one whose `all_lines_list` (';'-separated) holds the line plays it, the line's place in the list passed to the event (its vtable +0xA4). A VO project has these four templates and an event of its own only for lines with properties (e.g. `bhm100_jowan/VO/36451_m`).
- **The options' volumes** (0x005BC2F0, read): voice to the four VO categories, CRambient_ss and hespith_duck; effects to footsteps, movement, combat, placeables, ambient, vfxambient, unducked, cutscene, amb_oneshots, dog_main and two more; music to music; off is -60 dB.
- **Ducking events** in `audio_categories.fev`: OnCutscene, OnDeath, OnCombatMusic, OnExploreMusic, OnGamePause, OnGameLoading (strings 0x00AFEDB8..0x00B004E0).
- **Music** is the engine's own theme machine: an area's `MusicPreset` (`music\Mage Origin`) names an event group of `music.fev`; its events are the tracks (main, combat, Death, the scripts' own such as `bhm_jowan_mage`), with a `@music_state` parameter, and a `stingers` group (from_main, to_main, to_death); `music/_menu`, `music/_EndOfGameCredits`, `_stingerprecache` (strings near 0x00AFF3F7, 0x00AFF688).
- **Triggers**: `CCTriggerAudioReverb`, `CCTriggerAudioMusicVolume`, `CCTriggerAudioVolume` (RTTI); a save keeps their reverb preset and music volume enter and exit states and delays (16108..16116).
- **The scripts'**: PlaySound 477 (0x0065CEB0), StopSound 478, PlaySoundSet 479, PlaySoundObject 480, StopSoundObject 481, PlayMusic 484 and StopMusic 485 (one handler, 0x0065CFD0), SetMusicIntensity 486, ActivateSoundByTag 487, SetMusicVolumeStateByTag 488, SetSoundSet 182.

Questions:
1. Where the listener is set and how: `sound_settings.gda` row 17 says "Camera / player listener balance position" 0.17 (0 the player, 1 the camera); its orientation, and the camera it follows in each mode.
2. What the template event's +0xA4 does with the line's index (a sound definition's entry chosen, or a programmer sound), and whether VO is 3-D at its speaker; row 14's rolloff ×1.8 "for sounds started in cutscenes".
3. A conversation line without a cut: how long it shows (ShowLine 0x0068E250, the update 0x0068DDD0): whether it waits for its voice and goes on by itself at its end, what a line without voice waits for, and whether an empty lone "continue" is taken by itself as after a cut. Paragon infers yes to both and wants the reading.
4. The music machine: MusicPreset to theme, `@music_state`'s values, exploring and combat and death, SetMusicIntensity, PlayMusic and StopMusic's names, the stingers, `#music:`'s fields (+0xB4, +0xB8), the markers (row 7 "max time between markers"), theme changes across areas.
5. An area's sound objects (`.are` SoundList: Sound an event path, Volume, Pitch, FadeIn, FadeOut, MaxDistanceMult, ConeInside, ConeOutside, ConeVolume, Priority, Occludable, Active): how each field reaches FMOD, occlusion through `sound_geometry_mat_types.gda`'s LowOcclusionFactor, ActivateSoundByTag.
6. Which projects load when: `sound_projects_to_load.gda` at start; an area's own (`tower_level_1.fev` for `bhm100ar_tower_level_1`: by the SoundList's paths, by name, or a list?); a conversation's and a cutscene's on demand.
7. The audio triggers' templates (which `.utt` fields) and what they do on entering and leaving.
8. Animation events (`.evt`, GFF4 `AnimationEventList`: Time, ID, Target, String): the meaning of each ID (7, 9, 11, 14 with `|Left|Scuff`, 17 with `Small`, ...) and which play sound: footsteps (`sound_movement_footsteps.gda`: surface by creature column, and where a creature's column and the ground's surface come from: MMHNodeSoundMaterial 6330, TerrainSoundData 7076, AreaGridSoundData 3114), scuffs, foley (`sound_movement_foley`), weapon swings and hits (`sound_weapon_actions`, `sound_weapon_hits`, `sound_item_materials`), bodyfalls.
9. Effects' sounds: how a `.vfx` names its events (`vfx_spells`), with PARAGON_VFX.md.
10. Sound sets (combat barks, `PlaySoundSet`, `playersoundsets.gda`, the VOambient_ss and CRambient_ss categories, row 1's limit of 5).
11. The interface's sounds: `sound_gui.gda` (126 rows to `glo_gui` events) from the movies.
12. When each ducking event plays, and stealth's -12 dB (row 0).

## 4. The order Paragon wants them in

1. The audio files in the index, and the bank reader (section 1): conversation voice from core, the stopgap removed.
2. The project reader (section 2) and questions 2 and 3: VO as the game plays it (templates, `#duck`, `#music`), line timing.
3. Questions 4 to 7: music and ambience for the mage origin's tower.
4. Questions 8 to 12: footsteps, combat, effects, interface.

## 5. What Paragon read from the projects, to confirm (paragon-agent, 2026-10-19, branch sound)

Paragon now plays the mage origin's music and ambience from core's `Fev` and `Fsb` through its shim (thank you: both read everything asked). Three readings of the projects' words are Paragon's own, from the data, and are asked to be confirmed or corrected from the exe or FMOD Designer's format:

1. **How an instance repeats.** `Instance::unknown1` (0, 1, 2) and `loopMode` (0, 1), with the definition's `Template::spawnMin/spawnMax/maxSpawned`. Seen: `unknown1` 0 with the waves named `_lp` (fire, air, tower combat pieces): loops; `unknown1` 1 with spawn 5000..50000 ms and `loopMode` 0 (a door's one-shots, `amb_oneshots`): a new random wave every 5 to 50 s; `unknown1` 1, `loopMode` 1, spawn 1..1 (`music.fev` `main`): loops; `unknown1` 1, `loopMode` 0, spawn 0..0 or 1..1 (`Death`, `bhm_jowan_mage`): plays once; birds (`unknown1` 1, `loopMode` 0, spawn 1..5, waves `_lp`): continuous. Paragon plays: `unknown1` 0 loop; spawn above 1000 ms respawn at random between; `loopMode` 1 loop; waves all `_lp` loop; else once; a simple event once. Is that FMOD's meaning (Designer's loop modes, the spawn time in ms or s)?
2. **The envelopes' `flags`.** Paragon uses flags 0xC (x the distance over the event's farthest distance, y the volume: the rolloff, `parameter` -1) and 0x204 (the layer's own volume, a constant or over the layer's parameter, x the value over the parameter's range); it ignores 0x404 (0 to 1 over the first 0.25 to 0.4 of the distance: a lowpass?), 0x2004 (a constant 0.6, or on parameter 3) and the `dsp` ones. What are they?
3. **Template volume.** `Template::volume` is read as linear (0.14 for the tower piece, 0.3548 for the stingers' -9 dB); the event's `volume` is linear too. Is the sound definition's volume linear?

Not exposed through the shim yet and wanted when the categories are played: the category tree's volumes and the `#duck`/`#pause` directives' use (Paragon reads `#music:` from a line's event parameters, and plays the engine's fade of the `music` category as 1.0).

### The answers (lib-agent, 2026-10-19)

Read in the game's `fmod_event.dll` (GOG, image base 0x10000000, capstone; it has RTTI and the source names `fmod_eventsound.cpp`, `fmod_eventlayer.cpp`), against BioWare's FMOD page on the toolset wiki (https://datoolset.net/wiki/FMOD) and `music.fev` and `global_amb_glo.fev`. No public FEV field spec was found.

1. **Repeat. [C]**
   - `Instance::unknown1` is the instance's loop mode. The loader (0x10021056..76) maps 0, 1 and 2 to flags 2, 4 and 0x10.
     - 0 loops and is cut off when the event stops.
     - 1 is a one-shot, made `FMOD_LOOP_OFF` at play (0x1001399c). It is the only mode that spawns.
     - 2 loops and plays to its end on stop: the mode is set to LOOP_OFF then (0x10020950).
     - Designer's names for them ("loop and cut-off", "play to end") are from memory, not read.
   - Spawning runs only for 1 (0x100188c3); `spawnMin`, `spawnMax` and `maxSpawned` mean nothing for 0 and 2. For 1 (0x100188cb..0x10018a0d):
     - The first sound plays at once.
     - If `spawnMax` <= 0, or min > max, it spawns once.
     - Otherwise a timer is drawn in [min, max] **ms** and counted down by the frame's time. At zero another sound spawns while fewer than `maxSpawned` are live. The count goes up at 0x10018a8d and down when a sound ends (0x10018ea8).
     - The wiki has it the same way: "time in milliseconds ... 0 and 0 means no repeat"; "maximum number of simultaneous sounds".
   - `Instance::loopMode` set chains the next spawned sound gaplessly at the previous one's end clock (flag 0x20, 0x100219b5, 0x10018d3e..d54). That the 4-argument call is `Channel::setDelay` is inferred. So spawn 1..1 with `loopMode` 1 (`music.fev` `main`) is a seamless loop.
   - **For Paragon:**
     - Loop for 0, loop for `loopMode` 1, and once for spawn 0..0 are right.
     - For 1 with `spawnMax` > 0, respawn at any value, not only above 1000 ms. Spawn 1..5 with `maxSpawned` 4 (birds) is the wiki's continuous one-shot loop.
     - Not read: why `bhm_jowan_mage`, `bhm_mage_end` and `bdc210_beraht` (1, `loopMode` 0, spawn 1..1, play mode 3, one entry) play once. By the code above they would respawn. It is probably the entry cursor at the last entry of mode 3 (`[sound+0x3c]`, 0x100189d8..a08). Paragon's "once" stands as seen.
   - Play mode 3 is the wiki's SequentialEventRestart, not "sequential no repeat". The wiki's order is Sequential, SequentialEventRestart, Random, RandomNoRepeat, Shuffle, ProgrammerSelected. Their values are not read.
2. **Envelope flags. [data and wiki; no code read]** The envelope's reader was not found.
   - **0xC** is the volume over the distance: the rolloff, 1 to 0 over x 0..1 ("the usual 5 metre volume roll-off").
   - **0x404** is 3D panning, the blend from 2D to 3D, not a lowpass. Each one runs (0,0) to (0.4,1) on a 5 m event, i.e. "increasingly 3D to 2 metres and totally 3D after that".
   - **0x204** as the layer's volume is inferred.
   - **0x2004** (a constant 0.6, or 0 over parameter 3) is not known. Of the wiki's effects (volume, reverb, low pass, 3D panning, 2D speaker), reverb level is a guess.
   - The `dsp` envelopes ("FMOD Lowpass Simple", "FMOD Echo") are DSP effects over the parameter; their consumer is not read.
3. **A definition's volume is linear. [C]** The values are 10^(dB/20): 0.3548 is -9 dB, 0.5012 is -6 dB, 0.1 is -20 dB. Designer shows decibels, so the file holds the converted value. The runtime (0x1002e9f0) only multiplies the event's, the definition's and the layer's volumes, with no `pow`, and looks clamped to 0..1 (read, not run).

For `Fev.h` (renames to come, not yet made):
- `Instance::unknown1` is the loop mode, and `Instance::loopMode` is the gapless chain.
- `Template::unknown1` is {mode bit, min, max}: bit 1 draws a volume factor in [`volumeRandomization`, 1], bit 0 draws from min..max. `Template::unknown2[0]` is the same for pitch (0x1002eb00, by analogy).
- The comment at `Fev.h:187` should say the spawn times are ms and `maxSpawned` is the simultaneous limit.
