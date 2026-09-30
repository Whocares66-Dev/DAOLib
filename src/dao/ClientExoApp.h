// CClientExoApp: the client application object, a singleton the engine
// allocates on first use and keeps at dao::addr::kClientExoApp. The name
// comes from the RTTI of the vftables its constructor stores; the lineage
// is BioWare's Aurora engine (Neverwinter Nights' CExoApp).
//
// The first class of the library in CommonLib's shape: an opaque type, the
// fields known so far read at their offsets, and the engine's own functions
// called at their addresses. Nothing here reimplements the engine. Every
// offset and address was read 2026-09-29 from the disassembly of
// CClientExoApp::SetPaused (dao/Addresses.h has the story); none is yet
// verified in play, which is what the probe plugin is for.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"

#include <cstddef>
#include <cstdint>

namespace dao
{

class CClientExoApp
{
  public:
    CClientExoApp() = delete;
    CClientExoApp(const CClientExoApp &) = delete;
    CClientExoApp &operator=(const CClientExoApp &) = delete;
    ~CClientExoApp() = delete;

    // The singleton, or null before the engine has made it (at plugin load,
    // for one) and after it is torn down.
    static CClientExoApp *Get()
    {
        return *At<CClientExoApp *>(addr::kClientExoApp);
    }

    // The paused flag: the byte at +0xC of the flag object at +0xD8, as the
    // engine's own getter (0x00838210, `mov al, [ecx+0xC]`) reads it.
    bool IsPaused() const
    {
        return Field<std::uint8_t>(kPausedFlag + kFlagValue) != 0;
    }

    // Milliseconds spent paused so far this session: the running total
    // SetPaused keeps at +0xFC, not counting a pause still in progress.
    std::uint32_t PausedMilliseconds() const
    {
        return Field<std::uint32_t>(kPausedTotal);
    }

    // The engine's own pause. `second` is what the pause key passes 1 for;
    // its meaning is not yet read.
    void SetPaused(bool paused, bool second = true)
    {
        using Fn = void(__fastcall *)(CClientExoApp *, int, bool, bool);
        reinterpret_cast<Fn>(Module::Address(addr::kCClientExoApp_SetPaused))(this, 0, paused, second);
    }

    static constexpr std::size_t kPausedFlag = 0xD8;
    static constexpr std::size_t kSecondFlag = 0xE8; // set from SetPaused's `second`, by the disassembly
    static constexpr std::size_t kFlagValue = 0xC;
    static constexpr std::size_t kPausedSince = 0xF8;
    static constexpr std::size_t kPausedTotal = 0xFC;
    // A pointer to an object holding the game mode, an int at +0x78 of it.
    // Seen in play 2026-09-29 (dev/TURN_BASED.md): 0 exploring, 3 a menu
    // screen, 4 dialogue, 9 loading, 11 the main menu. What the object is,
    // is not yet read.
    static constexpr std::size_t kModeHolder = 0x114;
    static constexpr std::size_t kModeValue = 0x78;
    static constexpr std::size_t kSize = 0x2F0; // what operator new is asked for

  private:
    template <class T> const T &Field(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

} // namespace dao
