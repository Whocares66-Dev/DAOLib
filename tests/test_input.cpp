#include "dao/core/InputCode.h"

#include <catch2/catch_test_macros.hpp>

using dao::core::ActionOfText;
using dao::core::ActionText;
using dao::core::Key;
using dao::core::KeyboardInput;
using dao::core::ValidActionName;

TEST_CASE("a key's input word is the game's own", "[input]")
{
    // The game's defaults (0x006E9510), as its own maker writes them: a code
    // or'd with the modifiers and the keyboard's bit.
    CHECK(KeyboardInput(Key::Space) == 0x10000039);                  // TogglePause
    CHECK(KeyboardInput(Key::F5) == 0x1000003F);                     // QuickSave
    CHECK(KeyboardInput(Key::A, dao::core::kControl) == 0x1020001E); // ToggleFullPartySelect's second key
    CHECK(KeyboardInput(Key::F1, dao::core::kShift) == 0x1010003B);  // SelectPartyMember1Inclusive
    CHECK(KeyboardInput(Key::PageDown) == 0x100000D1);               // ZoomIn's second key
    CHECK(KeyboardInput(Key::A, dao::core::kShift | dao::core::kControl) == 0x1030001E);
    CHECK(KeyboardInput(Key::Z, dao::core::kAlt) == 0x1040002C); // HideMainInterface's second key in keymap.gda
    CHECK(KeyboardInput(Key::T, dao::core::kControl) == 0x10200014);
}

TEST_CASE("bits that are no modifier are dropped from a key's word", "[input]")
{
    CHECK(KeyboardInput(Key::J, 0xFFFFFFFF) == 0x10700024);
    CHECK(KeyboardInput(Key::J, dao::core::kMouse) == 0x10000024);
}

TEST_CASE("an action's name is letters and digits, a letter first", "[input]")
{
    CHECK(ValidActionName(L"TogglePause"));
    CHECK(ValidActionName(L"EndTurn2"));
    CHECK_FALSE(ValidActionName(L""));
    CHECK_FALSE(ValidActionName(L"2Fast"));
    CHECK_FALSE(ValidActionName(L"End Turn"));
    CHECK_FALSE(ValidActionName(L"End_Turn")); // the ini's keys end _0 and _1
    CHECK_FALSE(ValidActionName(L"End.Turn")); // a dot parts names in the data tree
    CHECK_FALSE(ValidActionName(std::wstring(49, L'a')));
    CHECK(ValidActionName(std::wstring(48, L'a')));
}

TEST_CASE("an action's text names it and no other text does", "[input]")
{
    CHECK(ActionText(L"EndTurn") == L"DAOLibAction EndTurn");
    CHECK(ActionOfText(ActionText(L"EndTurn")) == L"EndTurn");

    // The game's own bindings, and near misses.
    CHECK_FALSE(ActionOfText(L"UserTogglePauseMessage"));
    CHECK_FALSE(ActionOfText(L"SaveGameMessage -1 2"));
    CHECK_FALSE(ActionOfText(L""));
    CHECK_FALSE(ActionOfText(L"DAOLibAction"));
    CHECK_FALSE(ActionOfText(L"DAOLibAction "));
    CHECK_FALSE(ActionOfText(L"DAOLibActionEndTurn"));
    CHECK_FALSE(ActionOfText(L"DAOLibAction End Turn"));
    CHECK_FALSE(ActionOfText(L"XDAOLibAction EndTurn"));
}
