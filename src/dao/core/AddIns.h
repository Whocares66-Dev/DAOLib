// The add-ins and offers the game knows of, as its settings list them:
// Documents\BioWare\Dragon Age\Settings\AddIns.xml and offers.xml, read as
// text.
//
//   const auto addIns = dao::core::ReadAddIns(xml, "AddInItem");
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core
{

struct AddIn
{
    std::string uid;     // "DAO_PRC_CP_1"; in lower case it is its folder's name
    std::string name;    // "PRC_CP1_Cailan"
    std::string title;   // its Title's DefaultText, its English name; empty if it has none
    std::string extends; // the module it extends: "Single Player", "DAO_PRC_EP_1", or none
    std::optional<int> priority;
    std::optional<bool> enabled; // an offer has none
    std::optional<int> state;
    bool bioware = false;
};

// Returns the items of an AddIns.xml or offers.xml, in the file's order; one with no UID is
// left out.
// @param tag "AddInItem" for AddIns.xml, "OfferItem" for offers.xml
std::vector<AddIn> ReadAddIns(std::string_view xml, std::string_view tag);

} // namespace dao::core
