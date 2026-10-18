// A compiled script (.ncs) read into its instructions, over bytes, as the
// game's virtual machine reads them: each instruction's place, opcode, type
// byte and operands, and the whole written out one instruction a line.
//
//   const auto script = dao::core::ncs::ScriptOf(bytes);
//   for (const auto &i : script->code) ... // i.opcode == ncs::Op::Jsr: i.Target()
//   std::puts(dao::core::ncs::Disassemble(*script).c_str());
//
// The format is Neverwinter Nights' NCS V1.0 with Dragon Age's additions:
// opcodes 0x2E to 0x36 (waits, an array element copied in and out, a full
// state stored, a script ended), arrays and resources among the type bytes.
// dev/NCS.md has every opcode, its operands, its handler in the exe and what
// it does on the stack; the names are the toolset compiler's.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::ncs
{

enum class Op : std::uint8_t
{
    CpDownSp = 0x01,
    RsAdd = 0x02,
    CpTopSp = 0x03,
    Const = 0x04,
    Action = 0x05,
    LogAnd = 0x06,
    LogOr = 0x07,
    IncOr = 0x08,
    ExcOr = 0x09,
    BoolAnd = 0x0A,
    Equal = 0x0B,
    NEqual = 0x0C,
    Geq = 0x0D,
    Gt = 0x0E,
    Lt = 0x0F,
    Leq = 0x10,
    ShLeft = 0x11,
    ShRight = 0x12,
    UShRight = 0x13,
    Add = 0x14,
    Sub = 0x15,
    Mul = 0x16,
    Div = 0x17,
    Mod = 0x18,
    Neg = 0x19,
    Comp = 0x1A,
    MovSp = 0x1B,
    StoreStateAll = 0x1C,
    Jmp = 0x1D,
    Jsr = 0x1E,
    Jz = 0x1F,
    Retn = 0x20,
    Destruct = 0x21,
    NotI = 0x22,
    DecISp = 0x23,
    IncISp = 0x24,
    Jnz = 0x25,
    CpDownBp = 0x26,
    CpTopBp = 0x27,
    DecIBp = 0x28,
    IncIBp = 0x29,
    SaveBp = 0x2A,
    RestoreBp = 0x2B,
    StoreState = 0x2C,
    Nop = 0x2D,
    WaitFor = 0x2E,
    WaitUntil = 0x2F,
    CpSpToAl = 0x30,
    CpBpToAl = 0x31,
    CpAlToPSp = 0x32,
    CpAlToPBp = 0x33,
    StoreFullState = 0x34,
    AbortScript = 0x35,
    StoreWaitUntilTimer = 0x36,
};

// Returns an opcode's name as the toolset's compiler writes it ("CPDOWNSP"); empty for none.
std::string_view Name(Op op);

// Returns a type byte's name ("int", "string, string", "location", "effect[]"); empty for none.
std::string_view TypeName(std::uint8_t type);

// The type bytes of a single value: int, float, string, object, resource.
inline constexpr std::uint8_t kInt = 0x03;
inline constexpr std::uint8_t kFloat = 0x04;
inline constexpr std::uint8_t kString = 0x05;
inline constexpr std::uint8_t kObject = 0x06;
inline constexpr std::uint8_t kResource = 0x60;
// An EQUAL or NEQUAL of two structures, its size after the type byte.
inline constexpr std::uint8_t kStructures = 0x24;

// One instruction, its operands as its opcode and type byte have them, all others 0.
struct Instruction
{
    std::uint32_t offset = 0; // in the file; the code starts at 13, where the VM starts
    Op opcode = Op::Nop;
    // The operands' types; for STORE_STATE and STORE_STATEALL, how many bytes on the code a
    // stored state resumes at.
    std::uint8_t type = 0;
    std::uint32_t length = 0; // in bytes, opcode and type byte included
    // The first operand: a stack offset in bytes (the copies, MOVSP, the increments), a jump's
    // distance from this instruction (JMP, JSR, JZ, JNZ), CONST's int or object id (0
    // OBJECT_SELF, 1 OBJECT_INVALID), ACTION's command, STORE_STATE's first stack size,
    // DESTRUCT's bytes to remove, or an EQUAL of structures' size.
    std::int32_t value = 0;
    // The second: a copy's size in bytes, ACTION's argument count, STORE_STATE's second stack
    // size, or DESTRUCT's offset of the part to keep.
    std::int32_t second = 0;
    std::int32_t third = 0; // DESTRUCT's size of the part to keep
    float real = 0;         // CONST's float
    std::string text;       // CONST's string or resource, its bytes as the file has them

    // Returns where a jump goes: its offset plus its distance (JMP, JSR, JZ, JNZ).
    std::uint32_t Target() const
    {
        return offset + static_cast<std::uint32_t>(value);
    }
};

struct Script
{
    std::uint8_t major = 1; // the version, "V1.0"
    std::uint8_t minor = 0;
    std::vector<Instruction> code;
    // Whether every byte decoded; false where an instruction was unknown or ran past the end,
    // which is then at `stopped`.
    bool whole = true;
    std::uint32_t stopped = 0;
};

// Returns a compiled script's instructions, or nothing for bytes that are not one.
std::optional<Script> ScriptOf(std::span<const std::byte> file);

// Returns a script's instructions one a line: the offset in hex, the name and type, the
// operands, and where a jump goes.
std::string Disassemble(const Script &script);

} // namespace dao::core::ncs
