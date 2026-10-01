// A plugin's own text, in the player's language: a talk table of its own
// beside its DLL, chosen by the game's language, read by string id. What
// the game's own text is through dao/Tlk.h, for a plugin's.
//
//   static const auto text = dao::strings::Table::Load(*info, L"turnbased");
//   const std::wstring tooFar = text.String(2, L"Too far");
//
// A plugin ships turnbased_en-us.tlk, turnbased_de-de.tlk and so on beside
// its DLL: GFF4 talk tables as the game's own (dao/core/TalkTable.h).
// `python tools/tlk.py --write strings.csv turnbased_en-us.tlk` makes one
// from a CSV of id,text rows; the toolset writes them too. The player's language is the game's: Locale under
// [GameOptions] in Documents\BioWare\Dragon Age\Settings\DragonAge.ini (en-us on this machine, 2026-09-30), the setting
// that picks the game's own core_<lang>.tlk. A language with no file of the plugin's falls back to en-us; a string with
// no entry, to the fallback given.
#pragma once

#include "dao/Plugin.h"
#include "dao/core/TalkTable.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dao::strings
{

// The game's language, as its options give it ("en-us", "de-de"); en-us if
// the options say none.
std::wstring Locale();

class Table
{
  public:
    // <folder>\<name>_<locale>.tlk, else <name>_en-us.tlk; an empty table if
    // neither is there or readable.
    static Table Load(const std::filesystem::path &folder, std::wstring_view name);
    // The same, from the folder the plugin's DLL is in.
    static Table Load(const LoaderInfo &info, std::wstring_view name);

    std::wstring String(std::uint32_t id, std::wstring_view fallback = {}) const;
    // The file read, if one was.
    const std::optional<std::filesystem::path> &File() const
    {
        return file_;
    }

  private:
    std::optional<core::TalkTableFile> table_;
    std::optional<std::filesystem::path> file_;
};

} // namespace dao::strings
