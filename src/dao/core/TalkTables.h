// The talk tables the game searches for a string id, in its order, and the
// text it finds there, read from an install with no game (DAOLib #106).
//
//   const auto index = dao::core::install::Index::Build(game, documents);
//   const dao::core::install::Strings strings(*index, dao::core::install::TalkTables(*index, false));
//   if (const auto found = strings.Lookup(238912))
//       ... found->text is L"Magi", from strings.Tables()[found->table]
//
// The engine keeps one list of tables and answers an id from the first that
// holds it. Which tables it has and their order depend on the campaign and
// the language the index was built for (install::Options), and on the hero's
// gender, which reorders them: TalkTables is the order once a campaign has
// loaded, LoadOrder and SetGender the steps it is made of.
#pragma once

#include "dao/core/Gff3.h"
#include "dao/core/Install.h"
#include "dao/core/TalkTable.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::install
{

// A talk table the game has, and the copy of its file it reads.
struct LoadedTable
{
    std::string name;    // the file's name in lower case: "core_en-us.tlk", "dao_prc_cp_1_c_en-us_f.tlk"
    Copy copy;           // as Index::Find gives it
    bool female = false; // a _f table: the text a female hero is shown in its place
};

// Returns the talk tables the game loads, in the order it loads them: the core's, each enabled
// add-in's core table and each offer's, in the order the game takes them; then, as the loaded
// campaign loads, its packages', the module tables of the add-ins and offers that extend it,
// and its own. Each name's table is in the language the index was built for, or in en-us where
// it has none, followed by its _f table where there is one.
std::vector<LoadedTable> LoadOrder(const Index &index);

// Reorders the tables as the engine does each time the hero's gender is set (on loading a
// campaign, changing the party's leader, setting the hero's gender): the tables of that
// gender move ahead of the rest, in reverse of their order, so each setting reverses them again.
// @param female true for the _f tables, false for the rest
void SetGender(std::vector<LoadedTable> &tables, bool female);

// Returns the talk tables in the order the game searches them once the loaded campaign has
// loaded with a hero of this gender: LoadOrder, then SetGender once. Where two tables hold an
// id, a later setting of the gender may give the other's text.
std::vector<LoadedTable> TalkTables(const Index &index, bool female);

// Talk tables read, and an id answered from them as the game answers it.
class Strings
{
  public:
    // Reads the tables; one whose copy does not read or parse is left out.
    Strings(const Index &index, std::vector<LoadedTable> tables);

    struct Found
    {
        std::wstring text;
        std::size_t table = 0; // its place in Tables()
    };

    // Returns the text of an id from the first table that holds it, as the game shows it.
    std::optional<Found> Lookup(std::uint32_t id) const;
    // Returns the text of an id from every table that holds it, in the order searched.
    std::vector<Found> All(std::uint32_t id) const;
    // Returns the tables read, in the order searched.
    const std::vector<LoadedTable> &Tables() const
    {
        return tables_;
    }
    // Returns a table's strings.
    const TalkTableFile &Table(std::size_t table) const
    {
        return files_[table];
    }

  private:
    std::vector<LoadedTable> tables_;
    std::vector<TalkTableFile> files_;
};

// Returns the text the game shows for a template's localised string: its own text if it has any,
// the first of them whatever its language, else its id's text from the tables, else nothing.
std::wstring TextOf(const gff3::LocString &text, const Strings &strings);

} // namespace dao::core::install
