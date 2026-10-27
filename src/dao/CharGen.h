// Character creation as its screen makes it: what the creation screen offers at each step, each
// as it shows it (to choose, or greyed out), and a choice made as the screen makes it, refused
// where the screen greys it out, and the screen ended as its Play buttons end it. The attributes and
// abilities are dao/LevelUp.h's, with
// levelup::Mode::CharGen: its RaiseAttribute and Take. Lowering an attribute and unpicking an
// ability are left out: the screen allows them only for what its own session spent, which the
// library would have to track beside it. The head and appearance are left to the screen.
//
// A choice raises the module's creation event, whose script makes the change: with the screen
// up the game is paused, and the change is seen on the hero a frame or more after the call.
//
//   for (const auto &race : dao::chargen::Races())
//       if (race.enabled && race.id == dao::chargen::kElf)
//           dao::chargen::SelectRace(race.id);
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::chargen
{

// The scripts' numbers for the screen's choices.
inline constexpr std::int32_t kMale = 1;
inline constexpr std::int32_t kFemale = 2;
inline constexpr std::int32_t kDwarf = 1;
inline constexpr std::int32_t kElf = 2;
inline constexpr std::int32_t kHuman = 3;
inline constexpr std::int32_t kWarrior = 1;
inline constexpr std::int32_t kMage = 2;
inline constexpr std::int32_t kRogue = 3;

// Returns the hero the creation screen makes.
// @return nullopt unless the creation screen is up
//
// Call it, and the rest, on the game thread.
std::optional<ObjectId> Hero();

// A choice the screen offers.
struct Choice
{
    std::int32_t id = 0;  // the row its table gives it, as the scripts number it
    std::wstring name;    // as the screen shows it
    bool enabled = false; // the screen lets it be chosen now
    bool chosen = false;  // the hero has it
};

// Returns the screen's genders, races, classes and backgrounds, in its order.
std::vector<Choice> Genders();
std::vector<Choice> Races();
std::vector<Choice> Classes();
std::vector<Choice> Backgrounds();

// A voice the screen offers the hero's race and gender.
struct Voice
{
    std::wstring name;     // as the screen shows it
    std::wstring soundSet; // the sound set's resource, as SelectVoice takes it
};

// Returns the voices the screen offers the hero's race and gender, its default first.
std::vector<Voice> Voices();

// Why a choice was not made, or None if it was.
enum class Refusal : std::uint8_t
{
    None,
    NoScreen,   // the creation screen is not up
    NotOffered, // the screen offers no such choice
    Disabled,   // the screen greys it out now: a class before a race, a background the race and class cannot take,
                // a finish before a background
    BadName,    // a name the screen's field does not take: empty, or longer than its 10 letters
    PointsLeft, // a full finish with attribute, skill, talent or spell points unspent
};

// Make a choice as the screen's buttons do. A race clears the class and background chosen, and a
// class the background, as the screen does.
Refusal SelectGender(std::int32_t gender);
Refusal SelectRace(std::int32_t race);
Refusal SelectClass(std::int32_t characterClass);
Refusal SelectBackground(std::int32_t background);
Refusal SelectVoice(const std::wstring &soundSet);

// Names the hero, as the appearance page's name field does. Without one, a finish gives the hero
// the name the field starts with, the origin's default.
Refusal SetName(const std::wstring &name);

// How the screen is ended, as its exit points number them.
enum class Exit : std::int32_t
{
    Quick = 1, // the appearance page's Quick Play: the game spends the points
    Full = 2,  // the last page's Play, every point spent
};

// Ends the screen as its Play buttons do: the voice for a quick finish, the name, the difficulty,
// then the screen closed, whose movie then sends the end the origin starts from.
// @param difficulty a row of the difficulty picker, from 0 (Casual); none keeps the game's
//
// The origin starts once the screen has closed, a frame or more later.
Refusal Finish(Exit exit, std::optional<std::int32_t> difficulty = std::nullopt);

} // namespace dao::chargen
