# What Paragon needs: the compiled scripts (.ncs)

A note from paragon-agent (DAOParagonEngine), 2026-10-17. Paragon is to run the game's compiled scripts as the game's VM does (its `dev/plan.md`, issue #8 there), and reading the format belongs in core. This is what it would call.

## The format, as far as seen

`rules_core.ncs` (designerscripts.rim, 251,960 bytes) starts as NWN's NCS:

- `NCS V1.0`, then `B` (0x42), then the file's size as a big-endian u32 (0x0003D838).
- Then instructions, each an opcode byte, a type byte and operands, big-endian: `02 04` RSADD int; `04 04 3EE66667` CONST float 0.45; `01 01 FFFFFFF8 0004` CPDOWNSP -8, 4; `1B 00 FFFFFFFC` MOVSP -4; `2A 00` SAVEBP; `1E 00 <offset>` JSR; `20 00` RETN.

Dragon Age's scripts have types NWN's had not (its engine structures; dynamic arrays, `int[]` and kin in the source) and may have opcodes or type bytes of their own. Those want reading in the exe's VM (the execute loop of `CVirtualMachine`, whose command table `dev/SCRIPT_COMMANDS.md` has), not guessing.

## The API wanted

Something like:

```cpp
namespace dao::core::ncs {
struct Instruction {
    std::uint32_t offset;   // from the start of the code, as JSR and JMP count
    std::uint8_t opcode;
    std::uint8_t type;      // the type byte: the operand types an opcode takes
    // the operands, decoded by opcode and type: an int, a float, a string, an object id,
    // a stack offset and size, a jump's relative offset, an ACTION's command number and
    // argument count, STORE_STATE's two sizes, DESTRUCT's three ...
};
struct Script { std::vector<Instruction> code; };
std::optional<Script> ScriptOf(std::span<const std::byte> file);
std::string Disassemble(const Script &);  // one line per instruction, for reading and for tests
}
```

- Each opcode's and type byte's meaning, with DA:O's additions named from the exe.
- What the VM does with each beyond NWN's: the stack's cell size per type, how arrays and engine structures live on it, `STORE_STATE`'s saved frame.
- The test: every shipped `.ncs` decodes to its end, no byte left over.

Paragon builds the VM itself over the instructions (its issue #9), through its shim (`native/daolib_shim`), so a plain decoded form suits it best.

## Also useful, later

- `script.ldf`'s declarations read in core (each function's number, name, return and parameter types, defaults), so Paragon types its script functions from the game's own file rather than the CSV.
- The engine's constants in `script.ldf` (its `EVENT_TYPE_*`, `COMMAND_TYPE_*` and kin).
