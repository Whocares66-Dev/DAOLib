// Reads and sets values in the data tree, the engine's store of named objects and their
// attributes (SmartData), addressed by dotted paths such as `GUI.SelectedChar`. The Flash UI's
// screens are bound to this tree, so you can read what a screen shows, for example the selected
// character or a list's items, and set a value the way a screen would.
//
//   const auto selected = dao::data::Text(L"GUI.SelectedChar");
//   const auto labels = dao::data::Number(L"Map.ShowAllLabels");
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::data
{

// An attribute in the data tree. The engine owns it: get one from Find, and never create or free
// one yourself.
class Attribute
{
  public:
    Attribute() = delete;
    Attribute(const Attribute &) = delete;
    Attribute &operator=(const Attribute &) = delete;
    ~Attribute() = delete;

    // Returns the value as text, whatever the attribute's type. The UI's GetValue call returns the
    // same text.
    std::wstring Text() const;
    // Returns the value as a number, whatever the attribute's type. The UI's GetNumber call
    // returns the same number.
    double Number() const;

    // The type of value an attribute holds.
    enum class Type
    {
        Other, // anything else: a string, an object, a list or a pointer
        Bool,
        Int,
        UnsignedInt,
        Short,
        Byte,
        Float,
    };
    // Returns the type of value the attribute holds, worked out from its class.
    Type Kind() const;

    // Sets a Bool attribute. If the value changes, the engine notifies the attribute's observers,
    // including any movies bound to it.
    // @return false, with nothing changed, if the attribute is not a Bool
    //
    // Call each Set on the game thread, for example from a frame callback. That is where the
    // engine sets its own attributes.
    bool Set(bool value);
    // Sets an Int, Short or Byte attribute, narrowing the value to the attribute's size.
    // @return false, with nothing changed, for any other type
    bool Set(std::int32_t value);
    // Sets an UnsignedInt attribute.
    // @return false, with nothing changed, for any other type
    bool Set(std::uint32_t value);
    // Sets a Float attribute.
    // @return false, with nothing changed, for any other type
    bool Set(float value);

    // Returns how many members the attribute has: a list's items or an object's members. Returns
    // 0 for a plain value. The UI's GetNumMembers call returns the same count.
    std::size_t MemberCount() const;
    // Returns the names of the attribute's members: a list's indices as text, or an object's
    // member names. Returns an empty vector for a plain value. The UI's ListMemberNames call
    // returns the same names.
    std::vector<std::wstring> MemberNames() const;

    // Sets the attribute from text, the same way the Flash UI's SetValueUnicode call does. A
    // string attribute takes the text as it is, and a number attribute parses it.
    // This is how a screen's text field reaches the game, for example the save screen's
    // SaveLoad.SaveName (see dao/Saves.h). Call it on the game thread.
    void SetText(std::wstring_view value);
};

// Finds the attribute at a dotted path, such as `GUI.SelectedChar`.
// @return the attribute, or null if there is nothing at the path
Attribute *Find(std::wstring_view path);

// Returns the value at a path as text, or nullopt if there is nothing at the path.
std::optional<std::wstring> Text(std::wstring_view path);
// Returns the value at a path as a number, or nullopt if there is nothing at the path.
std::optional<double> Number(std::wstring_view path);

// Returns how many members the list or object at a path has, or nullopt if there is nothing at
// the path.
std::optional<std::size_t> MemberCount(std::wstring_view path);
// Returns the member names of the list or object at a path. A list's member names are its
// indices. Returns an empty vector if there is nothing at the path.
std::vector<std::wstring> MemberNames(std::wstring_view path);
// Returns the full path of each member of the list or object at a path: the path followed by
// ".<name>". Each member can be read the same way, so you can walk a list of objects as a screen
// does. Only paths that Find can reach are returned. Returns an empty vector if there is nothing
// at the path.
//
//   for (const std::wstring &chain : dao::data::Members(L"CharGenCreature.Stats.TalentChains"))
//       for (const std::wstring &ability : dao::data::Members(chain))
//           dao::data::Number(ability + L".DisplayState");
std::vector<std::wstring> Members(std::wstring_view path);

} // namespace dao::data
