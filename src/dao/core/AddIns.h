// The add-ins and offers the game knows of, as its settings list them:
// Documents\BioWare\Dragon Age\Settings\AddIns.xml and offers.xml, read as
// text.
//
//   const auto addIns = dao::core::ReadAddIns(xml, "AddInItem");
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dao::core
{

struct AddIn
{
    std::string uid;   // "DAO_PRC_CP_1"; in lower case it is its folder's name
    std::string name;  // "PRC_CP1_Cailan"
    std::string title; // its Title's DefaultText, its English name; empty if it has none
    // Its Title's text in each language the game reads one in, for those it has: the language's code
    // ("fr-fr") and the text, in the game's order of the languages.
    std::vector<std::pair<std::string, std::string>> titles;
    std::string extends; // the module it extends: "Single Player", "DAO_PRC_EP_1", or none
    std::optional<int> priority;
    std::optional<bool> enabled; // an offer has none
    std::optional<int> state;
    bool bioware = false;
};

// Returns an add-in's title as the game shows it in a language: its text in the language, or in
// en-us for a language the game has no slot for; empty where the item has none.
// @param locale a language's code, in any case: DragonAge.ini's Locale
std::string TitleIn(const AddIn &item, std::string_view locale);

// Returns the items of an AddIns.xml or offers.xml, in the file's order; one with no UID is
// left out.
// @param tag "AddInItem" for AddIns.xml, "OfferItem" for offers.xml
std::vector<AddIn> ReadAddIns(std::string_view xml, std::string_view tag);

} // namespace dao::core
