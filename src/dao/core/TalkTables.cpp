// Notes on the talk tables' order, read 2026-10-24 for DAOLib #106 and seen in play the same day
// (lib-agent-3's instance of the GOG game, a test add-in's tables in its Documents, each holding
// an id another table holds with a marker of its own, GetTlkTableString asked through Eluvian):
//
// The engine's one CTlkTable (in 0x00C1D08C) keeps a vector of 12-byte entries at +0x28..+0x2C:
// the table, its name, a female byte. GetString (0x004E4990) asks each table front to back and
// takes the first that holds the id (0x004E47F0: the file's list of strings is an open hash
// table, searched from hash(id) % count one slot on at a time until the id, an empty slot of id
// 0xFFFFFFFF, or the start again; every shipped table is laid out so, as TalkTableFile::Write
// lays its own, and a table with no empty slot is searched whole). It reads no gender: the +0x1C that
// GetTlkTableString sets is not read in a lookup, and its gender argument changed nothing in
// play.
//
// A name is loaded by 0x004E5A80: <name>_<language>.tlk, or <name>_en-us.tlk where the resource
// manager has no such file (0x004C5E40), pushed on the vector's end (0x004E5810, 0x004E5570),
// then the same name's _f table with its female byte set; nothing where neither file is found.
// It is called with "Core" at start (0x005D6562), with each add-in's <UID>_c as the add-ins
// are enabled (0x005F4E65 in 0x005F4610), and each offer's (0x0049B230 in 0x0049AA90); then
// as a campaign loads (0x005FCE80): each package its .cif lists (0x005FDE44), each add-in's
// <UID> that extends the campaign and has a module (0x005EFC7B in 0x005EF460), each offer's
// (0x0049A0CC in 0x00499A30), and last the campaign's own, by its name (0x005D51DE in
// 0x005D5080, after 0x005FCE80 in the load's state machine 0x006C76F0). Leaving the campaign
// erases its tables by name (0x004E4BD0 from 0x005D5290 and 0x005FC140). In play, with each
// pair of tables sharing an id: the core's before an add-in's core table; add-ins' core tables
// the higher Priority first (500 before 200 before 1), as AddInOrder takes them; offers' core
// tables after the add-ins' and before every module table; an add-in's module table after
// every core table, before an offer's and before singleplayer's. A table of an add-in that
// extends another campaign was not loaded (Leliana's Song's save had none of singleplayer's).
//
// The gender setter 0x004E5CF0(female) walks the vector by index, and each entry whose female
// byte is the one asked for it erases (0x004E4B10) and inserts at the front (0x004E56E0, at
// place 0), the index going on regardless: those tables come out ahead, reversed. Its callers:
// the load's state machine with the hero's gender (0x006C7B7C), SetPartyLeader's backend
// (0x006361D5, read by lib-agent-2 for #187) and, for a creature's appearance set to a gender,
// 0x006A6915 and 0x006A692B. In play: a male hero's new game had the plain tables reversed (an
// add-in's table before the core's, singleplayer's before an add-in's module table); a female
// hero's save the plain tables in their load order and German's _f tables ahead (core_de-de_f's
// "Magierin" for 238912, and "Magier" with a male hero); and setting the hero's gender male,
// female, then male again reversed the plain tables, left them, and put them back.
#include "dao/core/TalkTables.h"

#include "dao/core/AddInOrder.h"

#include <algorithm>
#include <utility>

namespace dao::core::install
{

namespace
{

// The copy of a file the game reads, or none. A copy of another language's is read where the
// language has no file of the name, as a table falls back to en-us's.
std::optional<Copy> Readable(const Index &index, const std::string &name)
{
    const std::vector<Copy> *copies = index.Find(name);
    if (copies == nullptr || copies->empty())
    {
        return std::nullopt;
    }
    const Copy &first = copies->front();
    if (first.unused != Unused::No && first.unused != Unused::Language)
    {
        return std::nullopt;
    }
    return first;
}

// A name's table file in a language: "core_en-us.tlk", "core_de-de_f.tlk".
std::string FileName(const std::string &name, std::string_view language, bool female)
{
    std::string file = name;
    file.append("_").append(language).append(female ? "_f.tlk" : ".tlk");
    return file;
}

// One name's tables as 0x004E5A80 loads them: its language's or en-us's, then the female one.
void Load(const Index &index, const std::string &name, std::vector<LoadedTable> &tables)
{
    const std::string language = Lower(index.Set().language);
    for (const bool female : {false, true})
    {
        for (const std::string &file : {FileName(name, language, female), FileName(name, "en-us", female)})
        {
            if (const auto copy = Readable(index, file))
            {
                tables.push_back({file, *copy, female});
                break;
            }
        }
    }
}

bool Extends(const AddIn &item, const std::string &campaign)
{
    return !campaign.empty() && Lower(item.extends) == campaign;
}

} // namespace

std::vector<LoadedTable> LoadOrder(const Index &index)
{
    std::vector<LoadedTable> tables;
    const std::vector<std::size_t> addIns = AddInOrder(index.AddIns(), true);
    const std::vector<std::size_t> offers = AddInOrder(index.Offers(), false);
    Load(index, "core", tables);
    for (const std::size_t i : addIns)
    {
        Load(index, Lower(index.AddIns()[i].uid) + "_c", tables);
    }
    for (const std::size_t i : offers)
    {
        Load(index, Lower(index.Offers()[i].uid) + "_c", tables);
    }
    for (const std::string &package : index.Packages())
    {
        Load(index, package, tables);
    }
    const std::string campaign = Lower(index.Set().campaign);
    for (const std::size_t i : addIns)
    {
        if (Extends(index.AddIns()[i], campaign))
        {
            Load(index, Lower(index.AddIns()[i].uid), tables);
        }
    }
    for (const std::size_t i : offers)
    {
        if (Extends(index.Offers()[i], campaign))
        {
            Load(index, Lower(index.Offers()[i].uid), tables);
        }
    }
    if (!campaign.empty())
    {
        Load(index, CampaignFileName(campaign), tables);
    }
    return tables;
}

void SetGender(std::vector<LoadedTable> &tables, bool female)
{
    for (std::size_t i = 0; i < tables.size(); ++i)
    {
        if (tables[i].female == female)
        {
            std::rotate(tables.begin(), tables.begin() + static_cast<std::ptrdiff_t>(i),
                        tables.begin() + static_cast<std::ptrdiff_t>(i) + 1);
        }
    }
}

std::vector<LoadedTable> TalkTables(const Index &index, bool female)
{
    std::vector<LoadedTable> tables = LoadOrder(index);
    SetGender(tables, female);
    return tables;
}

Strings::Strings(const Index &index, std::vector<LoadedTable> tables)
{
    for (LoadedTable &table : tables)
    {
        const auto bytes = index.Readable(table.copy) ? index.Read(table.copy) : std::nullopt;
        auto file = bytes ? TalkTableFile::Parse(*bytes) : std::nullopt;
        if (file)
        {
            tables_.push_back(std::move(table));
            files_.push_back(std::move(*file));
        }
    }
}

std::optional<Strings::Found> Strings::Lookup(std::uint32_t id) const
{
    for (std::size_t i = 0; i < files_.size(); ++i)
    {
        if (auto text = files_[i].String(id))
        {
            return Found{std::move(*text), i};
        }
    }
    return std::nullopt;
}

std::vector<Strings::Found> Strings::All(std::uint32_t id) const
{
    std::vector<Found> found;
    for (std::size_t i = 0; i < files_.size(); ++i)
    {
        if (auto text = files_[i].String(id))
        {
            found.push_back({std::move(*text), i});
        }
    }
    return found;
}

} // namespace dao::core::install
