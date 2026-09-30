#include "dao/core/CallSite.h"

#include <cstdint>

namespace dao::core
{

namespace
{

// modrm fields.
constexpr unsigned Mod(unsigned modrm)
{
    return modrm >> 6;
}
constexpr unsigned Reg(unsigned modrm)
{
    return (modrm >> 3) & 7;
}
constexpr unsigned Rm(unsigned modrm)
{
    return modrm & 7;
}

// The length of an FF /2 call (opcode, modrm, SIB and displacement) given
// its modrm and SIB bytes, or 0 if the modrm is not a call's.
unsigned CallLength(unsigned modrm, unsigned sib)
{
    if (Reg(modrm) != 2)
    {
        return 0;
    }
    const unsigned mod = Mod(modrm);
    const unsigned rm = Rm(modrm);
    const bool hasSib = mod != 3 && rm == 4;
    // A 32-bit displacement: [reg+disp32], [disp32], or [index*scale+disp32]
    // (a SIB with no base).
    const bool disp32 = mod == 2 || (mod == 0 && (rm == 5 || (hasSib && (sib & 7) == 5)));
    const unsigned displacement = mod == 1 ? 1 : (disp32 ? 4 : 0);
    return 2 + (hasSib ? 1 : 0) + displacement;
}

} // namespace

bool EndsWithCall(const std::array<std::byte, 7> &before)
{
    auto at = [&](std::size_t back) { return static_cast<unsigned>(before[7 - back]); };

    // E8 rel32: the opcode five bytes back.
    if (at(5) == 0xE8)
    {
        return true;
    }
    // FF /2: an FF opcode n bytes back whose modrm (and SIB) make the
    // instruction exactly n bytes long.
    for (std::size_t n = 2; n <= 7; ++n)
    {
        if (at(n) != 0xFF)
        {
            continue;
        }
        const unsigned modrm = at(n - 1);
        const unsigned sib = n >= 3 ? at(n - 2) : 0;
        if (CallLength(modrm, sib) == n)
        {
            return true;
        }
    }
    return false;
}

} // namespace dao::core
