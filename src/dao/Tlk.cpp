// The talk tables' lookup and the engine's token replacer, called in the engine.
//
// Read 2026-09-30 from the script command GetTlkTableString: the engine's CTlkTable (the object
// in 0x00C1D08C) answers a string reference with its text through GetString, __thiscall
// (strref, std::wstring &out, flag), `ret 0xC`. tools/tlk.py reads the same tables from the files.
//
// The gender, read 2026-10-24 for DAOLib #106 and seen in play: GetString asks the tables front
// to back and reads no gender, nor does the per-table find (0x004E47F0). GetTlkTableString sets
// the table's +0x1C to its gender argument, which no lookup reads: in play its argument changed
// nothing. The female form is the order of the tables: the gender setter 0x004E5CF0 moves the
// tables of one gender to the front, called with the hero's on a campaign's load and from
// SetPartyLeader's backend (0x00636180, with 1 when the new leader's appearance +0x50 is 2,
// GENDER_FEMALE: lib-agent-2's reading for #187). dao/core/TalkTables.h has the whole order.
#include "dao/Tlk.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Module.h"
#include "dao/String.h"

namespace dao::tlk
{

std::wstring String(std::uint32_t strref)
{
    void *table = Global<void *>(addr::kTlkTable);
    if (table == nullptr)
    {
        return {};
    }
    WString text;
    using Fn = void(__fastcall *)(void *, int, std::uint32_t, core::WStringRep *, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kCTlkTable_GetString))(table, 0, strref, text.Engine(), 0);
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
