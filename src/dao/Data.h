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
// play.
#pragma once

#include <cstdint>
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

} // namespace dao::data
