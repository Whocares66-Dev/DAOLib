// The engine's data tree (Eclipse's SmartData): named objects and their
// attributes by dotted path, what the Flash UI binds to and asks for with
// its GetValue and GetNumber calls.
//
//   const auto selected = dao::data::Text(L"GUI.SelectedChar");
//   const auto labels = dao::data::Number(L"Map.ShowAllLabels");
//
// Read 2026-09-30 from the UI's GetValue and GetNumber handlers
// (dao/Addresses.h). The tree's root is SmartData::TokenManager, a
// singleton made on first use; its Find splits a path on '.' and walks it
// to an attribute, SmartData::IAttribute, or to a shared null attribute
// when there is none. An attribute gives its value as text (slot 12) and
// as a number (slot 13), whatever its type. The objects the exe names as
// roots include CClientExoApp, GUI, Map, Container and Conversation; paths
// seen: GUI.SelectedChar, GUI.SelectedChar.Equipment, Map.ShowAllLabels,
// CClientExoApp.GUIDialogResult. The game's own classes are SmartObjects
// too (the game modes, the application), with their attributes in the
// tree. A plain attribute (a bool, a number) can be set, and setting one
// tells its observers, the movies bound to it among them. Not yet seen in
// play. A list's members are read below (Members).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::data
{

// An attribute in the tree: never made or freed by a plugin.
class Attribute
{
  public:
    Attribute() = delete;
    Attribute(const Attribute &) = delete;
    Attribute &operator=(const Attribute &) = delete;
    ~Attribute() = delete;

    // The value as the UI's GetValue gives it, and as GetNumber does.
    std::wstring Text() const;
    double Number() const;

    // What the attribute holds, by its class; Other for the rest (strings,
    // objects, lists, pointers).
    enum class Type
    {
        Other,
        Bool,
        Int,
        UnsignedInt,
        Short,
        Byte,
        Float,
    };
    Type Kind() const;

    // Sets a plain attribute of that type, as the engine does: stored,
    // and its observers told if it changed. False, and nothing done, for
    // an attribute of another type. From the scripts' thread (a frame
    // callback), where the engine sets its own.
    bool Set(bool value);
    bool Set(std::int32_t value); // an Int, Short or Byte attribute, narrowed to it
    bool Set(std::uint32_t value);
    bool Set(float value);

    // How many members it has, as the UI's GetNumMembers answers (its slot
    // 16): a list's items, an object's members; 0 for a plain value.
    std::size_t MemberCount() const;

    // Sets the attribute from text, as the Flash UI's SetValueUnicode does
    // (0x0051F6E0, which hands the text to the attribute's slot 10): a
    // string attribute takes it as it is, a number reads it. The way a
    // screen's text field reaches the game (the save screen's
    // SaveLoad.SaveName, dao/Saves.h). From the scripts' thread.
    void SetText(std::wstring_view value);
};

// The attribute at a path, or null if the tree has none there.
Attribute *Find(std::wstring_view path);

std::optional<std::wstring> Text(std::wstring_view path);
std::optional<double> Number(std::wstring_view path);

// A list in the tree, as the screens read one: how many it holds, and each
// member's path, the list's own with ".<index>" after it, whose members are
// read the same way.
//
//   for (const std::wstring &chain : dao::data::Members(L"CharGenCreature.Stats.TalentChains"))
//       for (const std::wstring &ability : dao::data::Members(chain))
//           dao::data::Number(ability + L".DisplayState");
//
// Read 2026-10-03. The UI's GetNumMembers (an FSCommand, 0x0051BE40) finds
// the path and answers its attribute's slot 16. A list is an
// ECDynArray<T*> attribute (the level-up screen's chains, a container's
// inventory, a store's stock, a conversation's lines), and every one of
// the 49 such classes looks a member up (its slot 6, 0x007268E0) by the
// path's component read as a number, so Find reaches "<list>.0" and on.
// An object's members go by name and are not listed here. Members gives
// only paths that Find reaches.
//
// The level-up screen's chains (FindAbilityChainsMessage, built by
// 0x0062DCD0): each member an AbilityGUIInfo of AbilityTemplate, Column,
// DisplayState and AbilityInstance. DisplayState is 1 for an ability the
// creature has (0x0071BEC0), 2 for one it can learn now (0x0071BEF0, the
// test behind the scripts' IsAbilityAvailable), 3 for one it cannot yet.
// Nothing for a path with nothing there. Seen in play 2026-10-03 through
// the Eluvian server: the world map's 58 pins read by their index paths,
// with the map down. MemberCount and Members on the level-up chains not
// yet seen.
std::optional<std::size_t> MemberCount(std::wstring_view path);
std::vector<std::wstring> Members(std::wstring_view path);

} // namespace dao::data
