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
