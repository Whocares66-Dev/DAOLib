# Aurora lineage: what Neverwinter Nights' community work can tell us

Dragon Age: Origins' engine, Eclipse, descends from BioWare's Aurora engine (Neverwinter Nights, 2002), as did Obsidian's Electron (NWN2) and BioWare's Odyssey (KotOR). NWN's engine has been studied for twenty years, and a lot of that work is public. This page is what carries over, the evidence for it, and the caveats. Checked 2026-09-30.

## The evidence it carries over

The exe keeps Aurora's names and shapes in its core:

- **Class names in NWN's style**, from the RTTI (`tools\daoexe.py --rtti`):
  - `CClientExoApp`, `CExoGFF`, `CResGFF`, `CResARE`, `CResIFO`, `CTlkTable`, `CNetLayer`, `CRules`, `CGameObject`, `CGameEffect`, `CScriptEvent`, `CAIEvent`
  - the script machine's `CVirtualMachineCommands`, `CClientVirtualMachineCommands` and `CVirtualMachineCmdImplementer`
  - client-side objects with NWN's `CNWC*` pattern shortened to `CC*`: `CCCreature`, `CCArea`, `CCItem`, `CCPlaceable`, `CCModule`, `CCParty`
- **The script machine.** Scripts call the engine through 827 numbered commands, each implemented as a function of two stack arguments (`ret 8`), as NWN's `ExecuteCommandXxx(int nCommandId, int nParameters)` are. Each pops its arguments off the VM's stack through a global VM object (`0x00C1D080`). The command list is declared, with numbers, in `script.ldf`, as NWN's is in `nwscript.nss`.
- **Objects by id.** `CClientExoApp` at `+0xCC` holds an object array. Looking an object up is a call with the id and an out-pointer, then a virtual call on the result (`0x005D5BE0`), the shape of NWN's `CGameObjectArray::GetGameObject(id, &object)` and its `As...()` casts. NWN's invalid object id, `0x7F000000`, appears 67 times in the code.
- **A queue of actions per object.** DAO's `AddCommand` and the queue insert at `0x0073CD10` do what NWN's action queue (`CNWSObject::AddAction`) does, with commands in place of actions.
- **The formats.** GFF, ERF, TLK, 2DA (GDA in DAO), ARE and IFO resources, in newer versions.

## What does not carry over

Eclipse was substantially re-architected. It has no NWN counterpart for its message passing (`Messages::`, `MemberFunctionMessageHandler<...>`), its `SmartData` attributes, its `GameMode*` classes, its task-based threading (`Multithreading::`, `Eclipse::UpdateTask`), or its Scaleform UI. Layouts and offsets never carry over: a DAO class shares a name and a role with its NWN ancestor, not its fields. So NWN is a map of what to look for and what to call it, and every fact is still read from the DAO exe.

## Where the work is

- **NWNX:EE** (<https://github.com/nwnxee/unified>): headers for NWN:EE's server, generated from its symbols, under `NWNXLib/API`: `CNWSCreature`, `CNWSObject`, `CVirtualMachine`, `CGameObjectArray`, `CExoString`, `CExoArrayList`, `CNWSVirtualMachineCommands`. Use it for the names and the likely members of the Aurora-era classes above, and for how its plugins hook the VM and the action queue, which is the NWNX pattern this project follows.
- **NWNX2 for Linux** (<https://github.com/NWNX/nwnx2-linux>): the older, 1.69-era API headers, closer in time to the code DAO grew from.
- **xoreos** (<https://xoreos.org/>, <https://github.com/xoreos/xoreos>, cloned to `C:\Modding\xoreos`): an open reimplementation of the Aurora family, Dragon Age: Origins included. It loads DAO campaigns and areas, and documents and reads DAO's resource formats. For DAO it has its own script interpreter and the full table of 863 engine functions from `script.ldf` (`src/engines/dragonage/script/function_tables.h`), of which 103 are implemented as of its September 2026 master: math, strings, local variables, events and a few object queries. The rest are stubs, so the game's own scripts get little done. It is a reimplementation, so none of it says what the original engine does inside. Use it for the file formats and resource handling, not for the engine's internals. Its names for GFF4's numeric field labels, 2,494 of them (`src/aurora/gff4fields.h`), are how `tools/gff4.py` reads a save game or a template by name (`dev/SAVES.md`); the table is read from the clone, not copied here. Its `src/engines/dragonage/types.h` has the scripts' object types, which agree with `script.ldf`'s `OBJECT_TYPE_*` and, one bit off, with the engine's type byte (`src/dao/core/ObjectType.h`).
- **NWN2 and KotOR.** Electron (NWN2, with NWNX4) and Odyssey (KotOR, with a large reverse-engineering community) are Aurora's other descendants. They are later than NWN and so sometimes closer to Eclipse, but they are further afield.

## Matched so far against NWN:EE's headers (2026-09-30, `C:\Modding\unified`)

| NWN:EE | DAO | How it was matched |
|---|---|---|
| `CVirtualMachine::StackPop*`, `StackPush*`: the whole typed stack, 8 pops and 6 pushes | `0x00A2F3C0`-`0x00A2F6D0`, `0x00A2D650`-`0x00A2D980` | voted across 665 script commands, each command's VM calls against its `script.ldf` argument and return types (`src/dao/Addresses.h` has every one with its votes). They run in NWN's declaration order: integer, float, vector, string, engine structure, object, with DAO's own resource and array types slotted in. |
| the VM itself, `g_pVirtualMachine` | `0x00C1D080` | `this` for all of the above, in every command |
| `CGameObjectArray::GetGameObject(OBJECT_ID, CGameObject**)` returning `uint8_t` | `0x00A58190` | the same arguments and a byte result, on the application's array at `+0xCC` |
| `CNWSObjectActionNode` (action id, twelve parameters, 16-bit group id) | the command queued at `0x0073CD10` (type, sequence number in 16 bits) | the same role, and a 16-bit id beside the type |
| `CGameObject`'s virtuals: destructor, `SetId`, `ResetUpdateTimes`, then one `As*()` cast per object class, null but in the class it names | `CGameObject`'s vtable (`0x00B0DFA4`): destructor, a one-argument slot 1 that sets the id, then 19 casts in slots 2-20, each `xor eax, eax; ret` | the shape, then read slot by slot: which class answers each cast with `this` (`tools/object_casts.py`, `dev/object_casts.csv`). The classes differ, client ones (`CCCreature`) where NWN's server has `CNWSCreature`, and two casts go to bases inside the object, `SEffectsHolder` and `ItemPossessor`. |
| `CVirtualMachineCmdImplementer`'s virtuals: destructor, `InitializeCommands`, `ExecuteCommand` | `CVirtualMachineCommands`' slots 0-2 | slot 1 allocates the command table and registers every command; slot 2 runs one. Its errors are NWN's numbers (-638, -639, -642). |
| `CNWSCombatRound`, NWN's 6-second rounds | none | no round class in DAO's RTTI: Eclipse dropped rounds for continuous combat, which a turn-based mode would have to put back |

Each is in `src/dao/Addresses.h` with its evidence.

## How to use it

1. When a DAO class has an NWN name, read the NWN:EE header for it first. It says what the class is for and what members to expect.
2. Look for those members in the DAO exe (Ghidra), and record each as its own finding, with its DAO offset and evidence, in a `src/dao/` header.
3. Never copy a layout. NWN:EE is also 64-bit and fifteen years newer, so even its sizes differ.
