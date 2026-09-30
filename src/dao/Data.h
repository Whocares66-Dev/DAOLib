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
// tree. Reading only, so far; not yet seen in play.
#pragma once

#include <optional>
#include <string>
#include <string_view>

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
};

// The attribute at a path, or null if the tree has none there.
const Attribute *Find(std::wstring_view path);

std::optional<std::wstring> Text(std::wstring_view path);
std::optional<double> Number(std::wstring_view path);

} // namespace dao::data
