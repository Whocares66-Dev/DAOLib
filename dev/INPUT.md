# Input: from DirectInput to an action

How the engine reads the keyboard and mouse and turns them into actions: the DirectInput backend and its events, the modifier mask, the modes' groups that let an input or a message text through, the bindings of numbered inputs, the actions with their two keys and `KeyBindings.ini`. Read 2026-10-31 in the GOG exe by paragon-agent's helpers (128 Input and Camera functions no note cited, DAOParagonEngine #351), every claim checked in the disassembly by lib-agent-2 with subagents; what did not hold was dropped or corrected. Static reading; nothing played. It builds on `src/dao/Addresses.h`'s input rows (the actions, `SetKey`, `TakeKey`, the mapper, `IsDown`, `AxisPair`, the modes' groups), `src/dao/core/InputCode.h` (the input word), `dev/GUI.md` ("Default bindings", "How a binding becomes the string", and the movies' `EnableGameMessage`) and `dev/CAMERA.md` ("The controls"). [C] read in the disassembly, [I] inferred.

The registrations are tabulated in `dev/input_names.csv` (`id`, `name`: the 76 numbered inputs), `dev/input_modes.csv` (`seq`, `helper`, `what`, `modes`: 319 group operations of `RegisterInputMessages`) and `dev/input_actions.csv` (`seq`, `call`, `action`, `args`: 297 rows, R1–R239 of `RegisterActions` and D1–D58 of the defaults function), from two independent transcriptions of `RegisterInputNames` 0x00895F10, `RegisterInputMessages` 0x006DC900 and `RegisterActions` 0x006E30D0 that agree row for row (paragon-agent, DAOParagonEngine #351, 2026-10-31); the call counts were checked by lib-agent-2. `dev/input_keys.csv` (`word`, `device`, `code`, `ini_name`, `inputmap_id`: the 189 keys `0x00882680` registers, by device then code; `BUTTON_NONE` has no word and no `inputmap_id`) lists the key names (lib-agent-2, 2026-10-31).

## The input word

- **One 32-bit word names an input** [C]: the device in the top nibble, the modifiers in bits 20–22, the code in the low 16 bits (`InputCode.h`).
  - **Devices:** 0x10000000 keyboard (made by 0x00880570), 0x20000000 mouse (0x00880590), 0x30000000 the axes (0x008805B0), 0x40000000 a numbered-input trigger (0x008805D0, ID 24188; 100 callers in `RegisterActions` and the input modes' registration) [C for the ORs and the callers; I for the meaning of kind 4].
  - **Modifiers:** Shift 0x100000, Ctrl 0x200000, Alt 0x400000. 0x008805F0 strips them (`& 0xF00FFFFF`), 0x00880620 keeps only them (`& 0x0FF00000`), 0x00880610 keeps the code (`movzx` of the low word) [C].
- **The modifier mask now** is 0x0087DFA0 [C]: the six scancodes Right/Left Shift (0x36, 0x2A), Right/Left Alt (0xB8, 0x38) and Right/Left Ctrl (0x9D, 0x1D), each tested in the keyboard state (0x0087DE50), give 0x100000, 0x400000 and 0x200000. 0x0087DF60 says whether a scancode is one of the six.

## The DirectInput backend: `ECInput::RawInputWin32`

- **The class** [C]: vtable 0x00B2ED28, under the abstract `ECInput::IRawInput` (0x00B2ED14). Slots: 0 the scalar deleting destructor (0x008804D0), 1 `Poll(sink)` (0x008804F0, `ret 4`), 2 the state query (0x0087E350, `ret 8`), 3 Acquire (0x0087DF20). The constructor 0x0087F890 has one caller (0x0087DCBE).
- **Its set-up** [C]: `DirectInput8Create` with the instance from `PlatformWin32` (the game object's +0xC8, its +0x44); a keyboard and a mouse device, each `SetDataFormat`, `SetCooperativeLevel(hwnd, 6)` (foreground, non-exclusive; the hwnd from `PlatformWin32`'s slot +0x10), and `DIPROP_BUFFERSIZE` 0x400. Any failed step clears the usable flag +0x28 and skips the rest. A lost or unacquired device (0x8007000C, 0x8007001E) is acquired again on the next read. [I for the GUIDs' and formats' names, from the usual order.]
- **Its fields** [C]:
  - +0x04/+0x08 two ints the axis query answers as axes 0 and 1, written by 0x0087DF00 (`PlatformWin32`'s cursor hold, Addresses.h) [I that they are the held cursor's point]; +0x0C/+0x10 the copies the mouse read compares.
  - **+0x14/+0x18 the frame's relative mouse x and y**, summed from the buffered records with `dwOfs` 0 and 4 and zeroed by `Poll` (0x008804FB).
  - +0x1C the `IDirectInput8`, +0x20 the keyboard, +0x24 the mouse; +0x28 usable (starts 1), +0x29 skip the next keyboard read, +0x2A a pending 16-byte event at +0x2C.
  - +0x3C keyboard state valid, +0x3D..+0x13C the 256-byte state (0x80 down).
  - +0x140 a vector of 2-byte elements (reserved 0x14 by 0x0087E9F0) [I: the typed characters, GUI.md "Input routing"]; +0x150 a vector of 16-byte events (reserved 0x14 by 0x0087EA20; 0x0087DE40 returns it).
  - +0x160 a `std::map`, seeded with the mouse codes 0x0C..0x13 and the offsets 0, 4, 8; +0x174 the modifier mask of the last mouse read (starts 0); +0x179 mouse state valid; +0x17C the `DIMOUSESTATE2`, its buttons at +0x188..+0x18F.
- **`Poll(sink)`** [C]: zeroes the frame's movement, reads the keyboard (0x0087FCF0), then the mouse (0x0087FF40, the read `src/fix/MouseRecords` corrects, CRASHES.md), then the immediate states (0x0087DDC0: `GetDeviceState` into +0x3D and +0x17C, setting the valid flags). The events go to the sink it is given, a vector of 16-byte events (0x0087EBD0 pushes), not to a vector of its own.
- **An event is 16 bytes** [C] (made by 0x00880520): the input word (code | modifiers now | device), the state word (device | 0x20000 press or 0x10000 release, `InputCode.h`'s `kOnPress`), then, for a key, DirectInput's `dwTimeStamp` and `dwSequence`; 0 and 0 for a mouse button.
- **The keyboard read** [C]: a peek for the count, then the buffered records. A record for one of the six modifiers only updates the state, then makes the new mask and has the held keys released with the old modifiers and pressed again with the new (0x0087F210, 0x0087F2A0) [I for what those two queue; their calls and arguments are C]. Any other record updates the state and queues one event. A failed read releases every held key (0x0087EE80).
- **The mouse buttons** [C]: codes 0x0C + the button (0..7). The first two are swapped when the option at `ClientOptions`+0x350 is set (0x00838210; CAMERA.md "The mouse buttons"). When the modifier mask has changed since the last read, the held buttons are released with the old mask (0x0087F330) and pressed with the new (0x0087F5E0). A failed read releases them all and clears their bytes (0x0087EF30), with no modifiers.
- **The state query** (slot 2) [C], by the word's device:
  - **Keyboard** (0x0087E080): nothing when the key is not down; else a bool, whether the modifiers held now are exactly the word's. A word with no code answers the modifiers alone. So Shift+F1 is not down while Ctrl+Shift+F1 is held.
  - **Mouse** (0x0087E0E0): nothing unless the mouse state is valid. Codes 0x14 and 0x15 answer the frame's x and y movement (+0x14, +0x18), as ints. Button codes go through the same swap.
  - **Axes** (0x0087DE70): codes 0 and 1 answer +0x04 and +0x08, as ints.
  - **Kind 4 and any other**: nothing.
  - The answer is a tagged value of 8 bytes: the value, then 0 for a bool (0x00897420) or 1 for an int (0x00897430). The devices' state object (0x00C1D1C0, from 0x00402C30) asks each device in turn (0x0087DA50); kind 3 means no device answered (0x008973E0).

## The modes' groups: `InputModes`

`InputModes::Get` (0x00891010) makes the one object, 0xE8 bytes, at 0x00C3B800. Its groups are named by the modes' classes (Addresses.h).

- **A group** (`InputModes::Group` 0x00890C60, `ret 4`, returns the group, which lives in the name map's node at +0x28) [C]:
  - **Inputs:** a set of allowed numbered inputs at +4 (`AllowsInput` 0x0088EC00), a set barred at +0x18, and a flag at +1 that bars every input not allowed (`BarsInput` 0x0088EC40, which calls `AllowsInput` itself).
  - **Message texts:** a set allowed at +0x30 (`AllowsText` 0x0088F1D0; `Allow` 0x0088F810 adds), a set barred at +0x5C (`BarsText` 0x0088F220; 0x0088F860 adds), a flag at +0 that bars every text not allowed, and **a flag at +0x2C that allows every text** (set by 0x0088E690; its one caller sets it on `MapInputs`, 0x006DE6A9–0x006DE6DA) [C: the string pushed at 0x006DE6A9 is 0xB12B84, L"MapInputs"; L"CursorLookMode" at 0xB12B64 is the next group's] (2026-10-31, lib-agent-2).
  - The texts are message texts such as "SelectResponseMessage 3", not typed text (Addresses.h; GUI.md's `EnableGameMessage`).
- **The groups that are on** (read 2026-10-31 by paragon-agent's helpers, checked by lib-agent-2 with subagents) [C unless marked]: three slots, at +0xB0, +0xB4 and +0xB8, and a map of overlays at +0xBC. A group is in a slot or in the map, never both.
  - **The slots:** Activate 0x00890CA0 (ID 24296; `(const int *slot, const std::wstring *name)`, `ret 8`) puts a registered group in a slot unless the map has it, replacing whatever the slot held, silently. Deactivate 0x00890DB0 (ID 24298) empties the slot only if it holds that group.
  - **The map:** Push 0x00890D40 (ID 24297, `ret 4`) adds a registered group unless a slot has it; the refusal is silent and nothing is kept for later. Pop 0x0088F8B0 (ID 24274) removes it. Pushing twice keeps one entry.
  - **The extra load:** the name map holds each group in its node (node+0x28, which `Group` returns), but the overlay map's node+0x28 holds a pointer to the group (Push stores it at 0x00890D9E). So its walk loads once more (0x0088EEAD) than the slots, which hold the pointer itself.
- **The walk** (`AllowsInput` 0x0088EDE0, `Allowed` 0x0088F430, the same code over the input and text tests):
  - It goes through the slots, then the map in key order. Each group answers whether it allows, asked only until one has, and whether it bars.
  - **A bar anywhere answers false at once. Otherwise the answer is whether some group allowed. With no group on, it is false.** The order does not change the answer.
  - The branch that reads +0xE4 cannot be reached.
- **Slot 0, the game mode:**
  - **What sets it:** each mode's Enter (vtable +0x18) activates its own name, and its Leave (+0x1C) deactivates it. `GameModeController::SwitchMode` (0x00641D30) calls the old mode's Leave, then the new mode's Enter (0x00641E8B).
  - **Combat** runs Explore's Enter and Leave around its own, so `GameModeExplore` is in the slot for a moment, then `GameModeCombat`.
  - **Loading** activates nothing, so whatever was there stays, or the slot is empty.
  - **Dialog** is activated not by its Enter but on the conversation's first line: `Conversation::PlayLine` (0x007F210B) → 0x007EED30 → 0x006913E0 (0x00691716), once per conversation, when app+0xD0 (the player, PARTY.md) exists [I for that condition].
  - **CharGen** activates `GameModeCharGen_LevelUp` in place of `GameModeCharGen` when its byte +0x84 is set (0x007195A4). Its Leave deactivates only `GameModeCharGen`, so after a level-up the LevelUp group stays until the next mode's Enter replaces it.
  - The sites, mode by mode, are in activation.md.
- **Slot 1, the GUI's context:**
  - `GuiFSCommand::PushContext` (0x005205A0) pushes the movie's name on a stack (0x00C315BC) and activates it in slot 1.
  - `PopContext` (0x0051FA40) erases that name and everything above it. If the stack is not empty, the new top goes into slot 1; if it is, the popped name is deactivated.
  - `GUIPopup`, `MapInputs`, `ResGraph`, `TutorialActive`, `DisableAll` and `GUIContextCutscene` are named in the exe only by `RegisterInputMessages`, so a movie turns them on by name [I which way, slot 1 or the map].
  - Read 2026-10-31 by paragon-agent's helper, checked by lib-agent-2 [C unless marked]:
  - **Only the movies change it.** The stack (a `std::vector<std::wstring>` at 0x00C315BC, made at 0x00ACF4B0) is touched by `PushContext` and `PopContext` alone. Slot 1 is written only by them, since every other Activate and Deactivate passes slot 0 or 2. No hide, unload or mode change of the engine's pops a context.
  - **The stack holds names only,** one stack for the whole GUI. It does not record which movie or scene pushed a name.
  - **Popping:** `PopContext` finds the first copy of the name and erases it and everything above it. So a name pushed twice is removed by one pop, and popping a lower name also removes the contexts pushed after it. An example is the inventory's `PaperDoll` above "Inventory".
  - **How a movie pops:** the engine invokes `_root.FinishSceneFromGame` when a screen closes (GUI.md "1. Scene start protocol"). The movie's `SceneManager.EndMovie` runs `FinishScene_<label>` and the class's `FinishScene`, which pop what the matching `StartScene` pushed. An example is `Inventory.as`:158–167.
    - The next show invokes `_root.StartSceneFromGame`, which pushes them again.
    - The pairing is the movie's own; all 19 movies that push contexts were not each checked [I].
  - **A hidden HUD keeps its context.** The hide-keep (`HideScreen` 0x00523CB0, the HUD's `ToggleMainInterfaceMessage`; GUI.md) invokes nothing in the movie. So the quickbar's `InGameGUI`, pushed in `QuickbarScene.StartScene` and popped only in its `FinishScene`, stays on the stack while the HUD is hidden [C for the AS and the hide's calls; I that nothing else pops it then].
- **Slot 2, the cursor**, set each frame by the mode's slot 29 (`GameMode::Update` 0x00694640 calls it; GUI, CharGen and PartyPicker call it themselves):
  - **Explore and Combat** (`GameModeExplore::UpdateInputContext` 0x00710120) take the first that holds:
    1. `CursorLookMode`, while the explore camera's drag is on (camera +0x10 == 1, set by `ReadInput` at 0x007EDF43; CAMERA.md "The mouse orbit").
    2. `DragSelectMode` while a box is dragged (0x00C35E75).
    3. Over the GUI with nothing under the cursor: `TargetingModeOverGUI` while aiming an ability (0x00C35F6C non-zero), else `CursorOverGUI`.
    4. While aiming: `TargetingModeOverValidTarget` or `TargetingModeOverInvalidTarget`.
    5. Nothing under the cursor: `CursorOverStaticGeometry` with a ground point (this+0xD4, +0xD0), else `CursorOverNull`.
    6. A creature:
       - the controlled one: `CursorOverPortrait` over the GUI, else `CursorOverPartyMember`;
       - one its slot 0x1D0 calls usable: `CursorOverUsableObject`;
       - a party member: as the controlled one;
       - else by its hostility byte (+0x10 → +0x4E8, +0xC): `CursorOverFriendlyNPC` or `CursorOverHostileNPC` [I that the byte is hostility].
    7. A placeable: `CursorOverUsableObject` when its +0x42C is set, else `CursorOverNull`. Any other type: `CursorOverNull`.
  - **Every other mode** (0x00694730) only activates `CursorOverGUI` over the GUI and deactivates it otherwise (0x006947D0, the only slot-2 Deactivate). **So a cursor group Explore left there (`CursorOverHostileNPC`, `CursorLookMode`, a targeting group) stays on through a dialog, a cutscene or the GUI mode** until the cursor goes over the GUI or Explore runs again.
- **The overlays in the map:**
  - **`Console`:** pushed on '`' in the window procedure (0x00414CF0) and by `OpenConsoleMessage` (0x006401FE). The procedure pushes it when the object from 0x00402CA0 is absent or its byte +0x270 (0x0054F810) is set [I for the byte's meaning]. It is popped on Enter while the console is open (0x00414D6F).
  - **`GUITextInput`:** pushed by 0x005110F0 for a non-empty field name and popped for an empty one; the name pushed is always "GUITextInput". Its caller is `GuiExternal::FilterKeyboardInput` (0x0051C760, a movie's call).
  - **`Effect_DisableInput` and `Effect_DisableMovement`:** 0x0069F200 pops both, then pushes them by the controlled creature's flags (0x00A64740): bit 7 for input, bit 0 for movement. It runs when the controlled creature changes (`CCParty::SetSelection` 0x006368F0) and from `RefreshPoolMemberState` (0x006BBDCF) for the controlled creature [I for which effects set the bits].
  - **`TrainingTutorialMode`:** pushed by `ForScript_SetTrainingMode` with a non-zero mode in Explore or Combat, and by Explore's Enter while the tutorial's mode is set. Popped with 0 and by `~TutorialControl`.
  - **`PostCutsceneInputFilter`:**
    - **On:** Explore's Enter (0x0070E9B4) sets a 2 s timer (0x00C35E78) when the previous mode was 4 or 5 [I that these are Dialog and Cutscene], and pushes the group while the timer is above 0.
    - **Off:** Explore's update (0x0071300F) pops it when the timer runs out, and Leave pops it.
    - **Leave does not reset the timer,** so re-entering Explore within the 2 s pushes it again. Combat runs the same Enter.
  - **A movie's own context:** `GuiFSCommand::ActivateContext` (0x0051C000) pushes and `DeactivateContext` (0x0051C090) pops.
- **Filling them at the start** (`RegisterInputMessages` 0x006DC900) [C], each over an array of mode names and a count, `__cdecl`:
  - 0x006DC880 (ID 15450): allow a message text in each listed mode's group (64 calls).
  - **0x006DC8C0 (ID 15451): allow a numbered input in each listed mode's group** (47 calls), through 0x0088F750 into the +4 set. It is not a "disallow": its first call allows input 43, `CastCursorRay`'s axis pair, in fifteen modes.
  - **It also works on groups one by one** (read 2026-10-31 by paragon-agent's helpers, checked by lib-agent-2) [C]: `Group` by name, then:
    - define a group 0x00890AE0 ×38;
    - allow a text 0x0088F810 ×64;
    - bar a text 0x0088F860 ×24;
    - allow an input 0x0088F750 ×59;
    - bar an input 0x0088F7B0 ×16;
    - allow every text 0x0088E690 ×1.
  - **The flags set:**
    - "bars every text not allowed" (+0) on `DisableAll` (0x006DC9AA), `Console` (0x006E17D2), `GUITextInput` (0x006E1B17) and `GUIPopup` (0x006E1C76);
    - "bars every input not allowed" (+1) on `DisableAll` (0x006DC9E7) and `Console` (0x006E1818).
  - One barred text is built from a narrow string (0xB12264, `"ToggleFullScreenGUIMessage MainMenu"` for `PostCutsceneInputFilter`) [paragon-agent's transcription].

### Where the gates are asked in the frame

Read 2026-10-31 by paragon-agent's helpers, checked by lib-agent-2 with a subagent [C unless marked].

- **A numbered input is gated when it is polled.** `IsDown`, `AxisPair` and the int and float queries (0x008924F0, 0x008925B0), the only four callers of `AllowsInput`, answer false or 0 when barred. `IsDown` has 84 call sites, so each consumer sees the groups on at the time it reads [I that there is no central pass].
- **A message text is gated once, when the key events are mapped:**
  - **When it runs:** `EclipseGame::Update` (0x00412F4C) runs the mapper's dispatch (0x0087D690, through 0x004025B0) unless 0x004028A0 answers true [I for that byte].
  - **The mapping:** the dispatch takes the frame's events. For each one in order, it looks up the word in the map at +0x14 and the word less its modifiers in the map at +0x38, and collects the bound messages through 0x0087C950.
  - **The gate:** for each entry, `Allowed(text)` is asked (0x0087CA32, its one caller). **A barred text is dropped, not kept for later.** An allowed one is made into a message (`FromBindingText` 0x007957D0). A null message, for an unknown name, ends that key's list.
  - **Then the sending:** only once every event of the frame is mapped are the messages sent, in order, to the game mode controller. The sends are synchronous (`Messages::Send` 0x004D20C0 calls the handler at once, through 0x004D8330) and happen in `DispatchInput`, before `FrameUpdate` in the same loop iteration (2026-11-07 by lib-agent-2).
  - **So** a message that changes the mode does not change the gating of keys pressed in the same frame [I, from the order].
  - **While remapping** (mapper +0x5C), each event goes out as one `RawInputMessage` instead, ungated.

## Bindings: `Input`

`Input::Get` (0x00891C30) makes the one object, 0x4C bytes, at 0x00C3B8E4: a vector of 12-byte bindings at +4 and a map at +0x10 from a numbered input to its list of binding indices (Addresses.h).

- **A binding** [C]: the first key at +0, the second at +4, a byte at +8 (1), and +9 whether both keys must be down (a chord; 0x008910A0 makes one key, 0x008910C0 two).
- **`IsDown`** (0x00892340) [C]: false when the modes do not allow the input; else true if any of its bindings is down, a chord needing both keys. A single binding with key 0 is never down. It does not stop at the first.
- **`AxisPair`** (0x00892670) [C]: the first binding only, (0, 0) when the modes bar it or either key is 0; else out[0] = the first key's value and out[1] the second's, as ints. The chord flag is not read.
- **Making them** [C]: `AddBinding` (0x008937D0 → 0x00892210) and `AddBindingPair` (0x008937F0 → 0x008922A0) append a binding and its index to the input's list. 0x00892850 (`ret 0xC`; key, input, flag) and 0x00892DC0 (two keys) bind a keyboard or mouse key **eight times, once per combination of Shift, Alt and Ctrl**, the key's own modifiers dropped; with the flag clear, or for an axis or kind 4, once. 44 and 2 callers in `RegisterActions`. Of these, 8 of the 44 and both of the 2 pass the flag 1 [paragon-agent's transcriptions, two copies agreeing]. So such an input is down whatever modifiers are held, which the state query alone would not allow [I from the two readings].
- **`Bind`** (0x00891170) rebinds an index; its one caller is the action's `SetKey` (0x00893A40).
- **The input mapper** (`InputMapper::Get` 0x0087D320, `__cdecl ()`, 0x60 bytes at 0x00C3B214) [C]: its 0x28-byte entries (Addresses.h) and two maps from a key word to entries, at +0x14 and +0x38. 0x0087D5A0 (ID 24122; entry, key) gives an entry a new key and moves it from the old key's place in the map to the new; an entry whose byte +0x24 is set is keyed without modifiers, in the map at +0x38. Its one caller is the action's `SetKey`. Its constructor ends by building the key-name tables from GDA 267 (`inputmap`, column `strref`; 0x00882680; GUI.md "How a binding becomes the string"). +0x5C is the remapping flag (below).

## Actions and their keys: `GroupedInputMappings`

The one object (`Get` 0x00895DC0, 0xE8 bytes at 0x00C3B9D8; data-tree name `InputMappings`, with `Mappings` at +0x58, `RemappingError` at +0x94, the action being remapped at +0xBC and the actions' vector at +0xD8) holds the actions (Addresses.h; GUI.md).

- **An action, `GroupedInputs`** (0x13C bytes, constructor 0x00894140) [C]: `RemappingPrimary` +0x58 and `RemappingSecondary` +0x68 (bools); `Description` +0x78, `PrimaryMappedKey` +0xA0, `SecondaryMappedKey` +0xC8 (strings); the two keys' mappings at +0xF0 and +0x100 (8 bytes each: a flag, then a numbered input's binding index for 0 or a mapper entry for 1); the key words at +0x110 and +0x114; +0x118 how the mappings combine (0 all, else any); +0x119 hidden from the ini; the name at +0x11C; +0x138 the keys missing from the ini (bit 0 the first, bit 1 the second).
- **`AddMapping`** (0x00894BB0) makes two mapper entries for the same trigger and text, one per key (0x00894490). **`AddInputMapping`** (0x00894C40, 15 callers) makes two empty bindings of a numbered input, one per key (0x008944E0).
  - **`RegisterActions` (0x006E30D0) makes, all inline** (read 2026-10-31 by paragon-agent's helpers, checked by lib-agent-2; [C] for the counts):
    - 53 `AddAction`;
    - 45 `AddMapping`, whose flag byte is 0 in all 45 [transcription];
    - 15 `AddInputMapping`;
    - 60 calls of 0x00892820, which make an input-mapper entry through 0x0087D480 with (word, word, text, flag). 15 pass flag 1, the mouse wheel's and the GUI cursor's [transcription];
    - 44 of 0x00892850 and 2 of 0x00892DC0;
    - 19 `AddBinding` and 1 `AddBindingPair`.
  - **The defaults function 0x006E9510:** the clear (0x00893D70), 56 `SetKey`, each with report 0, then 0x006E2FD0.
- **Whether a key may be set** (0x00893840) [C]: each of the action's mappings is asked whether the key word is a valid key of its device (0x00891100, called at 0x0089390A, which calls the test 0x00880C40 at 0x00891156), or 0; the answers are ANDed (combine 0) or ORed.
- **`SetKey`** (0x00894CC0) **returns a bool** [C]: false, with nothing changed, when the key is refused; else `TakeKey`, the action's own `SetKey`, and, when asked to report, `RemappingError` set to talk string 387764, or to 387765 + " " + the `Description` of the action that lost the key. Then true.
- **Remapping from the controls screen** [C]:
  - `EnableKeyRemappingMessage` (0x008947F0) sets the mapper's +0x5C, keeps the action's name at +0xBC, and sets its `RemappingPrimary` to 1 (the message's +0x20 is 0) [I: `RemappingSecondary` for a non-zero +0x20, 0x00894845 not read].
  - The next raw input (0x00894ED0), unless no action is waiting: the word less its modifiers is refused if it is a mouse button 0x0C or 0x0D, or keyboard 0xDB, 0xDC or 0xDD (DirectInput's Windows and Menu keys [I for the names]). Otherwise the key goes to slot 0 if `RemappingPrimary` is set, else slot 1, through `SetKey` with the report; when that answers true, `DisableKeyRemappingMessage` is sent.
  - `DisableKeyRemappingMessage` (0x00894880) clears both flags, the mapper's +0x5C and the name.

## `KeyBindings.ini` and the defaults

- **Nothing before the actions exist** [C]: `LoadKeyBindings`, `SaveKeyBindings`, `ResetKeys` and 0x006E2FD0 do nothing until the byte 0x00C35A30 is set, at the end of `RegisterActions` (0x006E94E4).
- **The file:** `ClientOptions`+0x550 (the settings folder) + L"KeyBindings.ini", section "Key Bindings", keys `<action>_0` and `<action>_1` [C].
- **The keys' names** (read 2026-10-31 by paragon-agent's helpers, two readers agreeing, checked by lib-agent-2 with a subagent) [C unless marked]: 0x00882680 registers 189: 139 keyboard, 12 mouse, 2 cursor axes and 36 gamepad. They are listed in `input_keys.csv` (the word, the device, the code, the ini name, and the row of `inputmap.gda`, GDA 267, whose `StrRef` is the display text).
  - The maps are on the singleton 0x00C3B5F8: word to name at +0, name to word at +0x2C.
  - `Keyboard::BUTTON_NONE` is only in the name-to-code map 0x00C3B7D4, so it parses to 0.
- **Reading a key** (0x008823C0, `(const std::wstring *text)`, `ret 4`, on the out word):
  - **The pieces:** the text is split on `+`, empty pieces dropped. Each piece is trimmed of spaces and tabs.
  - **The modifiers:** every piece but the last is compared without case (ASCII only) with `shift`, `control` and `alt`, giving 0x100000, 0x200000 and 0x400000. Any other piece is ignored, and `Ctrl` is not accepted.
  - **The key:** the last piece is looked up exactly, with case. A miss gives 0 and drops the modifiers.
  - So `shift + Keyboard::Button_F1` reads, but `keyboard::button_f1` gives 0.
- **The ini's text:**
  - **The reader** (0x004E1BF0) reads into 256 characters; a value that does not fit counts as not read. It turns `(UNASSIGNED)` (exact, with case) into "".
  - **The writer** (0x004E1CD0, `(value, path, section, key)`) writes `(UNASSIGNED)` for "".
  - **A word as text** (0x0088E560): "" for 0, else `Shift + `, `Alt + `, `Control + ` in that order, then the key's name, or `---<ERROR - Unregistered value >---` for an unknown word.
  - The display text (0x00881090, GUI.md) puts Ctrl outermost and reads the talk table instead.
- **`ResetKeys`** (0x006EA9F0, `__cdecl ()`) [C]: clears the keys (0x00893D70), runs the defaults function 0x006E9510 (56 `SetKey` calls), then reads `keymap` (GDA 292): per row, the `Label` and the language's column (`EN-US` when the language has none); the label less its last two characters is the action and its suffix `_0` or `_1` the key; `TeleportPlayer--DEBUG` is skipped; the text is parsed (0x008823C0) and set with `SetKey(name, key, which, 0)`. It ends with 0x006E2FD0 (`PerformPrimaryAction` and `PerformSecondaryAction` in code, GUI.md: `SetKey` 0x00894CC0 twice with key 0 and no report, the primary mouse 0x0D with no modifier, stored at 0x006E3011 and 0x006E3019, and the secondary mouse 0x0D with Shift 0x100000, stored at 0x006E306C and 0x006E3074; read to its end 2026-11-01).
- **Load** (0x00895580) [C]: for each action not hidden, each key present in the ini is parsed, taken from any other action (`TakeKey`) and set; a key missing sets its bit in +0x138.
- **Save** (0x00895980) [C]: writes only the keys whose bit is set in +0x138, then clears the mask. Load sets the bits for keys the ini lacks [C]; that a change made on the controls screen sets them too is [I] (GUI.md has bit 0 as "primary changed or missing").
- **Order at the start** (the game mode controller, 0x00641870, Addresses.h): `RegisterInputMessages`, `RegisterActions`, `ResetKeys` (0x006418B2), `LoadKeyBindings` (0x006418B7). `RegisterInputNames` (0x00895F10, from 0x006418A3) names the 76 numbered inputs 0–0x4B (`AccelerateCharacterLateral` to `Pre_Selection`), from narrow strings. Four are made out of order: the blocks for 0xB313DC, 0xB313C8, 0xB313B0 and 0xB31398 take ids 0x39, 0x36, 0x37 and 0x38. So 54–56 are `GUILeftCursorButton`, `GUIMiddleCursorButton` and `GUIRightCursorButton`, and 57 is `DisableMovement`, though `DisableMovement` is made first [C, read by my subagent and by both transcriptions].

## The console's nested commands: `Console::ExpandNestedCommands` (read 2026-11-07 by lib-agent-3, a second reading independent of Paragon's porter, for DAOParagonEngine #443) [C unless marked]

- `Console::ExpandNestedCommands` 0x00841660 (`ret 4`, the text by reference; it answers the global wstring 0x00C39C78) is called by `ExecuteCommand` 0x008425C0 (0x0084261F), `RunCommandWithoutArguments` 0x00842460 (0x008424B1) and itself (0x008418DC). When `Console::IsNotTraceCommand` 0x00840FB0 answers false for the text, it is returned as it is, less a trailing CR or LF (0x008416E2..0x00841781). Otherwise it scans for `[ ... ]` (the characters at 0x00AF02B8 and 0x00AF02BC), expands the inside by a recursive call, runs it through the console's slot 0x10 when the expansion is not empty, and puts the result in place of the brackets [I for what slot 0x10 does with it].
- **The scan:** i (edi) walks the text for `[`. On one, j (ebx) walks for `]`, **from where j last stopped, not from i**: j starts at 0 once (0x0084178B) and is never set back. On a `]` at j it takes the text before the `[` (from the last end, ebp, to i), the inside, substr(i + 1, j − i − 1) (0x008418AB..0x008418C1), expands and runs it, sets ebp = j + 1 (0x0084197A), then **i = j** (0x008419B6) and i + 1 (0x008419B8). With no `]` left, j reaches the end and i = j ends the scan. The only way out of the scan is i reaching the text's length (0x008419BB).
- **So it never ends when a `]` is found at or before the `[` it closes:**
  - **A second pair, `[a][b]`:** the first pair ends with j on its `]` (2), and i = 3, the second `[`. j is tested where it stands, still the first `]`, which is found at once. i is set back to 2, so the scan reaches the same `[` at 3 and the same `]` at 2, for ever.
  - **A `]` before the first `[`, `a]b[c]`:** j stops on the early `]` (1), and i goes back to 1 and comes again to the `[` at 3, for ever.
  - On each turn the inside, substr(i + 1, j − i − 1), has a negative length, so it runs to the end of the text (`b]` or `c]`). It is expanded, run again if it is not empty, and added to the result, which grows each turn. So the game hangs, running that command again and again, until it runs out of memory [I for how it ends].
- One pair, with no `]` before its `[`, works: `x [y] z` runs `y` once and the scan goes on after the `]`. So does any text with no `[`.

## Not read

- 0x0087F210, 0x0087F2A0 and 0x0087EE80 past their calls; what writes +0x04/+0x08 besides 0x0087DF00; the 2-byte vector's use.
- The secondary branch of the enable message (0x00894845).
- Which movie pushes which context name (the .gfx), which decides slot 1 or the map for the six groups above.
- The effects behind the creature's flag bits 7 and 0, and the refresh flag +0xC3D.
- 0x00890AE0 past its callers.
- The meaning of 0x004028A0's byte.
- The save's telemetry ('GAME', 0x474B4D43) when keys are written.
