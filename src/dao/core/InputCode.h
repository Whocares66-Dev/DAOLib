// The engine's word for an input, and the text a plugin's own action is
// bound to: the part of dao/Input.h that needs no game.
//
// An input is one 32-bit word. Read 2026-10-01 from the game's own default
// keys (0x006E9510) and the two functions that make the word (0x00880570
// for a key, 0x00880590 for a mouse button): a code in the low bits, the
// modifiers held with it above (0x100000 Shift, 0x200000 Control, 0x400000
// Alt: the three the engine tests to write a key's name, "Shift + ", "Alt
// + ", "Control + ", 0x0088E560), and the
// device at the top (0x10000000 the keyboard, 0x20000000 the mouse). The
// keyboard's codes are DirectInput's scan codes: the game's defaults give
// pause 0x39, quicksave 0x3F, the journal 0x24 and select all 0x200000 with
// 0x1E, where keymap.gda gives Space, F5, J and Control+A.
//
// A binding names what a pressed key sends as text (dao/Addresses.h,
// Messages::FromBindingText). A plugin's action is bound to a text no
// message has, "DAOLibAction <name>", which dao::input catches before the
// engine looks for a message of that name.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dao::core
{

// A key, by DirectInput's scan code.
enum class Key : std::uint32_t
{
    None = 0x00,
    Escape = 0x01,
    Digit1 = 0x02,
    Digit2 = 0x03,
    Digit3 = 0x04,
    Digit4 = 0x05,
    Digit5 = 0x06,
    Digit6 = 0x07,
    Digit7 = 0x08,
    Digit8 = 0x09,
    Digit9 = 0x0A,
    Digit0 = 0x0B,
    Minus = 0x0C,
    Equals = 0x0D,
    Backspace = 0x0E,
    Tab = 0x0F,
    Q = 0x10,
    W = 0x11,
    E = 0x12,
    R = 0x13,
    T = 0x14,
    Y = 0x15,
    U = 0x16,
    I = 0x17,
    O = 0x18,
    P = 0x19,
    LeftBracket = 0x1A,
    RightBracket = 0x1B,
    Enter = 0x1C,
    A = 0x1E,
    S = 0x1F,
    D = 0x20,
    F = 0x21,
    G = 0x22,
    H = 0x23,
    J = 0x24,
    K = 0x25,
    L = 0x26,
    Semicolon = 0x27,
    Apostrophe = 0x28,
    Grave = 0x29,
    Backslash = 0x2B,
    Z = 0x2C,
    X = 0x2D,
    C = 0x2E,
    V = 0x2F,
    B = 0x30,
    N = 0x31,
    M = 0x32,
    Comma = 0x33,
    Period = 0x34,
    Slash = 0x35,
    Space = 0x39,
    F1 = 0x3B,
    F2 = 0x3C,
    F3 = 0x3D,
    F4 = 0x3E,
    F5 = 0x3F,
    F6 = 0x40,
    F7 = 0x41,
    F8 = 0x42,
    F9 = 0x43,
    F10 = 0x44,
    F11 = 0x57,
    F12 = 0x58,
    Home = 0xC7,
    Up = 0xC8,
    PageUp = 0xC9,
    Left = 0xCB,
    Right = 0xCD,
    End = 0xCF,
    Down = 0xD0,
    PageDown = 0xD1,
    Insert = 0xD2,
    Delete = 0xD3,
};

// Held with the key; or them together.
inline constexpr std::uint32_t kShift = 0x00100000;
inline constexpr std::uint32_t kControl = 0x00200000;
inline constexpr std::uint32_t kAlt = 0x00400000;

inline constexpr std::uint32_t kKeyboard = 0x10000000;
inline constexpr std::uint32_t kMouse = 0x20000000;

// What a binding fires on: the game's own press-once actions (the
// screenshot, the console) are registered with this and the keyboard's bit,
// and no key (0x006E30D0).
inline constexpr std::uint32_t kOnPress = 0x00020000;

// The word for a key with its modifiers, as 0x00880570 makes it.
constexpr std::uint32_t KeyboardInput(Key key, std::uint32_t modifiers = 0)
{
    return static_cast<std::uint32_t>(key) | (modifiers & (kShift | kControl | kAlt)) | kKeyboard;
}

inline constexpr std::wstring_view kActionWord = L"DAOLibAction";

// An action's name is a key in KeyBindings.ini (<name>_0, <name>_1) and a
// name in the engine's data tree, so letters and digits only, a letter
// first, as the game's own are ("TogglePause").
constexpr bool ValidActionName(std::wstring_view name)
{
    if (name.empty() || name.size() > 48)
    {
        return false;
    }
    const auto letter = [](wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'); };
    if (!letter(name.front()))
    {
        return false;
    }
    return std::ranges::all_of(name, [&](wchar_t c) { return letter(c) || (c >= L'0' && c <= L'9'); });
}

// The text an action is bound to: "DAOLibAction <name>".
inline std::wstring ActionText(std::wstring_view name)
{
    std::wstring text(kActionWord);
    text += L' ';
    text += name;
    return text;
}

// The action a binding's text names, or nothing for any other text (the
// game's own, "UserTogglePauseMessage").
constexpr std::optional<std::wstring_view> ActionOfText(std::wstring_view text)
{
    if (text.size() <= kActionWord.size() + 1 || !text.starts_with(kActionWord) || text[kActionWord.size()] != L' ')
    {
        return std::nullopt;
    }
    const std::wstring_view name = text.substr(kActionWord.size() + 1);
    return ValidActionName(name) ? std::optional<std::wstring_view>(name) : std::nullopt;
}

} // namespace dao::core
