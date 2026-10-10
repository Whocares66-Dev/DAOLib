// The talk tables' lookup and the engine's token replacer, called in the engine.
//
// Read 2026-09-30 from the script command GetTlkTableString: the engine's CTlkTable (the object
// in 0x00C1D08C) answers a string reference with its text through GetString, __thiscall
// (strref, std::wstring &out, flag), `ret 0xC`, choosing the male or female form by its +0x1C.
// tools/tlk.py reads the same tables from the files. Not yet seen in play.
#include "dao/Tlk.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Module.h"
#include "dao/String.h"

#include <cstddef>

namespace dao::tlk
{

std::wstring String(std::uint32_t strref, bool female)
{
    void *table = Global<void *>(addr::kTlkTable);
    if (table == nullptr)
    {
        return {};
    }
    // The script command sets the gender byte for its call and leaves it;
    // this puts it back.
    auto &gender = *(static_cast<std::byte *>(table) + 0x1C);
    const std::byte before = gender;
    gender = female ? std::byte{1} : std::byte{0};
    WString text;
    using Fn = void(__fastcall *)(void *, int, std::uint32_t, core::WStringRep *, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kCTlkTable_GetString))(table, 0, strref, text.Engine(), 0);
    gender = before;
    return std::wstring(text.View());
}

// Read 2026-10-03 (dao/Addresses.h, ReplaceDesignerTags): the engine's one replacer, which every
// movie asks for its bound text. The hero's names come from the party's object, so before a game
// is loaded the name tokens stay as they are. Seen in play 2026-10-03 through the Eluvian server,
// which passes every conversation line and reply through it: the Quartermaster's lines read
// clean, with no stray markup, and a reply's <FirstName/> came back as the hero's name.
std::wstring Resolve(std::wstring_view text)
{
    // The replacer reads the application for the hero's names.
    if (CClientExoApp::Get() == nullptr)
    {
        return std::wstring(text);
    }
    WString engine(text);
    using Fn = void(__cdecl *)(core::WStringRep *);
    reinterpret_cast<Fn>(Module::Address(addr::kReplaceDesignerTags))(engine.Engine());
    return std::wstring(engine.View());
}

} // namespace dao::tlk
