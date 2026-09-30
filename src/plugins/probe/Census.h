// The probe's census: a call counter on every script command and message
// handler the tables in dev/ name (874 functions, 2026-09-29), cheap
// enough to leave on while playing.
//
// Each function is detoured to a ten-byte stub of its own, `mov eax, index;
// jmp CountAndContinue`, and CountAndContinue is two instructions: add one
// to counts[index], then jump to the function's trampoline. Nothing on the
// stack moves and only eax is touched, which every x86 calling convention
// leaves free at a function's entry, so one routine serves functions of
// every signature: __stdcall script commands, __thiscall handlers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace probe
{

enum class Kind : std::uint8_t
{
    Command,
    Handler,
};

struct Target
{
    std::uint32_t address; // preferred-base virtual address
    Kind kind;
    const char *label; // every name at the address, " | " between
};

// The generated list (tools/probe_targets.py).
std::span<const Target> Targets();

struct CensusResult
{
    std::size_t attached = 0;
    std::size_t skipped = 0; // an address the probe already hooks another way
    std::size_t failed = 0;  // Detours refused: too short, or not code it can relocate
};

// Detours every target except those at the given addresses, one transaction
// each so a refusal costs one target and not all. At plugin load only.
CensusResult AttachCensus(std::span<const std::uint32_t> except);

// Calls counted so far at each target, by its index in Targets(); zero for a
// target not attached. Read without locking: each count is one aligned
// word, so a read sees some recent value, never a torn one.
std::uint32_t CallCount(std::size_t index);

bool IsAttached(std::size_t index);

} // namespace probe
