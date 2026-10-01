// A Flash movie's script, changed as bytes: the part of patching one of the
// game's movies that needs no game (dao/Gui.h, OnMovieLoad).
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
//   holds tags of its own.
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
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::gfx
{

// The movie uncompressed, its signature saying so ("GFX", or "FWS" for a
// Flash file) and its length as its header gives it. A movie that is not
// compressed is copied. Nothing for bytes that are no movie, or a
// compressed one that does not inflate to the length it says.
std::optional<std::vector<std::byte>> Unpack(std::span<const std::byte> file);

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
std::vector<std::byte> Statements(std::span<const Call> calls);

// The uncompressed movie with `code` put straight after the first
// statement `after` in its script, and what holds that place made to fit:
// the header's length, the tags', the sizes of the functions it is in, and
// branches that jump across it. Nothing if the statement is not there, or
// the movie's layout is not understood, or a size would not fit its field.
std::optional<std::vector<std::byte>> InsertAfter(std::span<const std::byte> movie, const Call &after,
                                                  std::span<const std::byte> code);

} // namespace dao::core::gfx
