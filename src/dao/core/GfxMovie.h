// A Flash movie changed as bytes: the part of patching one of the game's
// movies that needs no game (dao/Gui.h, OnMovieLoad).
//
// The game's movies are Scaleform's .gfx: a Flash movie (SWF 8, ActionScript
// 2) whose signature is "GFX", or "CFX" when all after the eight-byte
// header is one zlib stream. Read 2026-10-01 from the game's own
// optionsmenu.gfx, with the SWF file format's published layout:
//
// - The header: three signature bytes, the version, the uncompressed
//   length, a rectangle of a bit count and four numbers, the frame rate
//   and the frame count.
// - Tags to an End tag: a 16-bit word of the tag's code and a length, the
//   length 63 standing for a 32-bit length after it. DoAction (12) and
//   DoInitAction (59, after a sprite's id) hold script; DefineSprite (39)
//   holds tags of its own, PlaceObject2 (26) among them, each putting a
//   character at a depth and a place, with a name; ExportAssets (56) names
//   characters for the script.
// - Script is actions: a code byte, and for a code of 0x80 or more a 16-bit
//   length and that many bytes. A constant pool (0x88) names the strings a
//   push (0x96) refers to by number. A function (0x9B, 0x8E) is followed
//   by its body, whose size is the last field of its record; a branch
//   (0x99, 0x9D) holds an offset from the action after it.
//
// The game's movies build their lists in script: the controls screen calls
// AddKeyBinding("TogglePause") once for each action it lists, the name a
// string in the script. So a movie takes a plugin's rows by having calls
// added after one of its own, and every size and branch around them moved.
// Its fixed screens are sprites that place their controls, and take one
// more by a PlaceObject2 added to the sprite.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::gfx
{

using Bytes = std::vector<std::byte>;

// The movie uncompressed, its signature saying so ("GFX", or "FWS" for a
// Flash file) and its length as its header gives it. A movie that is not
// compressed is copied. Nothing for bytes that are no movie, or a
// compressed one that does not inflate to the length it says.
std::optional<Bytes> Unpack(std::span<const std::byte> file);

// --- Script, written

// What a push puts on the stack.
struct Value
{
    enum class Kind
    {
        Text,
        Integer,
        Register,
    };
    Kind kind = Kind::Integer;
    std::string text;
    std::int32_t number = 0;

    bool operator==(const Value &) const = default;
};
Value Text(std::string_view text);
Value Integer(std::int32_t number);
Value Register(std::int32_t number);

// One push of these values, the strings written out in it.
Bytes Push(std::initializer_list<Value> values);

// The actions with no operands that script here needs.
enum class Op : std::uint8_t
{
    Pop = 0x17,
    GetVariable = 0x1C,
    CallFunction = 0x3D,
    SetMember = 0x4F,
    CallMethod = 0x52,
};
Bytes Do(Op op);

// A function with no name and no parameters around `body`, `this` in
// register 1: as the game's compiled script writes its set-up functions
// (DefineFunction2, two registers, flags 0x29).
Bytes Function(std::span<const std::byte> body);

// Pieces of script one after another.
Bytes Join(std::initializer_list<Bytes> pieces);

// One statement of a movie's script: function(argument); with the argument
// a string.
struct Call
{
    std::string function;
    std::string argument;

    bool operator==(const Call &) const = default;
};

// The statements as actions, in order: each pushes its argument, the count
// 1 and the function's name, calls, and drops the answer, as the game's own
// compiled script does.
Bytes Statements(std::span<const Call> calls);

// --- Script, added to

// The uncompressed movie with `code` put straight after the first
// statement `after` in its script, and what holds that place made to fit:
// the header's length, the tags', the sizes of the functions it is in, and
// branches that jump across it. Nothing if the statement is not there, or
// the movie's layout is not understood, or a size would not fit its field.
std::optional<Bytes> InsertAfter(std::span<const std::byte> movie, const Call &after, std::span<const std::byte> code);

// The start of a statement that sets a member of what a register holds:
// the push of that register and the member's name, as in
// r2.OptAutoPause_mc_init = ...;
struct MemberSet
{
    std::int32_t reg = 0;
    std::string member;
};

// The same, with `code` put straight before the first such statement.
std::optional<Bytes> InsertBefore(std::span<const std::byte> movie, const MemberSet &before,
                                  std::span<const std::byte> code);

// --- What a movie shows

// Something a sprite places: where the designer put a control.
struct Instance
{
    std::string name;
    std::uint16_t sprite = 0;    // the sprite that places it
    std::uint16_t character = 0; // what it is an instance of
    std::uint16_t depth = 0;
    std::uint16_t deepest = 0; // the highest depth the sprite uses
    std::int32_t x = 0;        // in pixels
    std::int32_t y = 0;
};

// The first instance of that name in any sprite.
std::optional<Instance> FindInstance(std::span<const std::byte> movie, std::string_view name);

// Everything a sprite places that has a character, named or not, in the
// order its tags have them.
std::vector<Instance> InstancesOf(std::span<const std::byte> movie, std::uint16_t sprite);

// The first sprite that places a character.
std::optional<std::uint16_t> SpritePlacing(std::span<const std::byte> movie, std::uint16_t character);

// The character the movie exports under a name (ExportAssets), as script
// attaches it by.
std::optional<std::uint16_t> ExportedCharacter(std::span<const std::byte> movie, std::string_view name);

struct Placement
{
    std::string name; // empty for none, as a shape has none
    std::uint16_t character = 0;
    std::uint16_t depth = 0;
    std::int32_t x = 0; // in pixels
    std::int32_t y = 0;
};

// The movie with these placed in a sprite, on its first frame, and the
// sprite's tag and the header's length made to fit. Nothing if there is no
// such sprite or its layout is not understood.
std::optional<Bytes> PlaceInSprite(std::span<const std::byte> movie, std::uint16_t sprite,
                                   std::span<const Placement> placements);

} // namespace dao::core::gfx
