// Gives a plugin its own text in the player's language. The text comes from talk tables the
// plugin ships next to its DLL (name_en-us.tlk, name_de-de.tlk and so on), looked up by string
// ID. To read the game's own text, use dao/Tlk.h.
//
//   static const auto text = dao::strings::Table::Load(*info, L"turnbased");
//   const std::wstring tooFar = text.String(2, L"Too far");
//
// `python tools/tlk.py --write strings.csv turnbased_en-us.tlk` makes a talk table from a CSV
// file of id,text rows. The toolset can write talk tables too.
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

// Returns the game's language setting, such as "en-us" or "de-de", or "en-us" if none is set. It
// reads DragonAge.ini on every call.
std::wstring Locale();

// One talk table of a plugin's strings.
class Table
{
  public:
    // Loads <folder>\<name>_<locale>.tlk for the game's language, or <name>_en-us.tlk if there is
    // no file for that language.
    // @param name the start of the file name, such as L"turnbased"
    // @return an empty table, whose String always returns the fallback, if neither file can be
    // read
    static Table Load(const std::filesystem::path &folder, std::wstring_view name);
    // Loads the table, as above, from the folder of the plugin's DLL.
    static Table Load(const LoaderInfo &info, std::wstring_view name);

    // Returns the string with the given ID.
    // @return `fallback` if the table has no such string or no file was read
    std::wstring String(std::uint32_t id, std::wstring_view fallback = {}) const;
    // Returns the path of the file that was read, or nullopt if none was.
    const std::optional<std::filesystem::path> &File() const
    {
        return file_;
    }

  private:
    std::optional<core::TalkTableFile> table_;
    std::optional<std::filesystem::path> file_;
};

} // namespace dao::strings
