// Whether a value found on a thread's stack is a return address: the
// address just after a call instruction. The probe's sampler scans the
// stacks of the game's threads and keeps the values that point into the
// exe's code and pass this check; each is the caller of whatever was
// running, so a sample says not only where a thread is but what called it.
//
// x86 calls come in a few encodings, and the check looks for each ending
// exactly at the address:
//
//   E8 rel32              call near, relative          5 bytes
//   FF /2 modrm           call through a register or   2, 3, 4, 6 or 7 bytes
//                         memory operand, by the modrm and SIB forms
//
// A stack also holds data that happens to point into code and happens to
// follow such bytes; the check cannot rule those out, only make them rare.
#pragma once

#include <array>
#include <cstddef>

namespace dao::core
{

// The seven bytes before an address, the byte at address-7 first. True if a
// call instruction of any of the forms above ends at the address.
bool EndsWithCall(const std::array<std::byte, 7> &before);

} // namespace dao::core
