// Character creation's choices (dao/CharGen.h).
//
// Notes on character creation: read 2026-10-25 (DAOLib #259) in chargen.gfx (the movie's
// ActionScript, its classes under scripts\__Packages, cited by file and line below) and the GOG
// exe, with dev/CHARGEN.md, after the Eluvian server's chargen tool, which sent the movie's
// messages and checked race, class and background against table 97 itself. The screen's handlers
// check nothing: each GUICharGen message raises the module's creation event (dev/CHARGEN.md) and
// sys_chargen.nss guards next to nothing, so every rule here is the movie's.
// - Genders (RaceGenderScene.as 277-302): table 92's rows from the first, Playable 1.
// - Races (303-328): table 5's rows from the LAST, PlayerRace 1.
// - Classes (329-354): table 3's rows, PlayerClass 1 and BaseClass 0, disabled until a race is
//   chosen (348); then FilterClassesByRace (48-121): in the campaign, a class is enabled when some
//   row of table 97 has 1 in both the race's Label column and the class's CharGenLabel column
//   (97-99); in Awakening's modules (DAO_PRC_EP_1, DAO_PRC_GIB, DAO_PRC_STR, by
//   GetCurrentModuleName, 50-60) when table 20035's row of the class's place plus one has 1 in the
//   race's column (62-84).
// - Backgrounds (355-392): table 97's rows, one button per distinct description of any race
//   (table 208's value in the race's column, 371-373), named by table 210; enabled
//   (FilterBackgroundsByRaceClass, 122-154) when the row has 1 in the race's and the class's
//   columns and the race's description is the button's (134-138). A class chosen with one
//   background open presses it (443-469). A race chosen clears the class and the background
//   (437-440).
// - Voices (AppearanceSoundSetScene.as 279-316): table 212's rows from the second, of the hero's
//   gender and race, named by StrIDName or "?" and the Label, the first the default (308-311).
// - The messages (CharGen.as 285-305): "GUICharGen.CharGenSelect<Gender|Race|Class|Background>
//   <hero> <old> <new> <update>", the choices' row ids, 0 for none; the voice's
//   "GUICharGen.CharGenSelectAppearanceAndSoundSet <hero> 0 <sound set>", as the appearance page's
//   Next sends it (AppearanceSoundSetScene.as 564, 575), after "GUI.SetSoundSet <sound set>" (40-45).
// - The finish (CharGen.as 161-186; dev/CHARGEN.md, "CharGenEnd"): Quick Play
//   (AppearanceSoundSetScene's play button) and the last page's Play (SpellsTalentsScene.as
//   209-225) prompt for the difficulty; its OK sends "ClientOptions.SetClientOptionMessage
//   GameOptions.DifficultyLevel <picked>" and "ClientOptions.CommitOptions", then CompleteChargen
//   and "GUI.HideFullScreenGUI". CompleteChargen sends the voice again for Quick Play (303-306),
//   then the name (AppearanceSoundSetScene.as 332-336: GUICharGen.TempCharName set through the
//   movie's ExternalCommands.SetValueUnicode, an FSCommand no message reaches, then
//   "GUICharGen.CharGenSelectName <hero>"), and sets CharGen.nExitPoint and CharGen.bUserExit. The
//   movie's FinishScene, run as the screen closes, sends "GUICharGen.CharGenEnd <CreatureID> 0
//   <exit point>" only with bUserExit set (187-229); a hide with it unset closed the screen with no
//   CharGenEnd and the game crashed under DragonAgeGUISystem at 0x0072BB11 (Eluvian, 2026-10-18),
//   and the movie's CharGen.CreatureID was 0 there, so it is set to the hero too.
// - The rules: a name is not empty (IsValidName, 77-84) and at most MAX_NAME_LENGTH, 10 (21,
//   147); it starts as table 1001's DefaultNameMale or DefaultNameFemale in the row race * 1000 +
//   class * 100 + background (CreatureUtils.as 7-27). The points pages' Next is greyed while
//   points are left (AttributesScene.as 248-258, AbilityScene.as 57-67: properties 34, 35 and 36),
//   so the full finish wants none. The voice resent is the one chosen, or the first offered, as
//   the appearance page sets it when none is (308-311); a gender chosen clears it
//   (RaceGenderScene.as 426). The difficulties are table 211's rows (OptionsUtils.as 491-506).
// The choices are the screen's own, not the hero's: the movie keeps its gender, race, class and
// background (RaceGenderScene's m_n*, the gender male and the rest none at its start,
// ResetChoices 240-244) and greys by them, while the hero starts with properties of its own. So
// the library keeps the choices it made, from a new hero's, and sends them as the movie's old and
// new values.
//
// Seen in play 2026-10-25 (examples/chargen), a new game's creation screen: genders, races, classes,
// backgrounds and voices as the movie offers them; a class before a race refused Disabled, race 99
// NotOffered, a dwarf mage Disabled, an elf mage's one background (Magi) pressed for it, a noble
// elf rogue Disabled, a voice not offered NotOffered; the hero then read gender 2, race 2, class 3
// and background 3 (city), a female city elf rogue; dexterity raised by one through levelup's
// RaiseAttribute, base 14 to 15, and a skill taken through levelup's Take with Mode::CharGen,
// held a frame later, the points spent.
// The finish, seen in play 2026-10-27 on new games: before a background, refused Disabled; names of
// 11 letters and of none BadName; a dwarf noble warrior's full finish with its points unspent
// PointsLeft, and difficulty 99 NotOffered; with every point spent through levelup, the full
// finish at difficulty 2 closed the screen into the origin's cutscene, and in
// bdn120ar_royal_palace the hero read as Brosca, the name given, race 1, gender 1, class 1,
// background 5, attributes 15 15 11 11 11 16 and the skills and talents taken, the difficulty
// option 2. A female elf mage's quick finish, no name
// or voice given, went into the Harrowing (bhm500ar_tower_harrowing): the hero read as Neria, the
// origin's default, race 2, gender 2, class 2, background 4, its points spent by the game, with
// spells 200254, 10001 and 11106. No difficulty popup was shown or needed: the picker's OK is
// what Finish sends.
#include "dao/CharGen.h"

#include "dao/Data.h"
#include "dao/Gda.h"
#include "dao/Gui.h"
#include "dao/LevelUp.h"
#include "dao/Messages.h"
#include "dao/Tlk.h"
#include "dao/core/GdaHash.h"
#include "dao/generated/ScriptApi.h"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>

namespace dao::chargen
{

namespace
{

namespace api = script::api;

constexpr std::int32_t kGenders = 92;
constexpr std::int32_t kRaces = 5;
constexpr std::int32_t kClasses = 3;
constexpr std::int32_t kBackgrounds = 97;
constexpr std::int32_t kBackgroundNames = 210;
constexpr std::int32_t kSoundSets = 212;
constexpr std::int32_t kClassesFilter = 20035;
constexpr std::int32_t kBackgroundDefaults = 1001;
constexpr std::int32_t kDifficulties = 211;
constexpr std::size_t kMaxNameLength = 10;

// The choices made on the screen, as the movie keeps them, for the hero they were made for.
struct Chosen
{
    ObjectId hero = kInvalidObject;
    std::int32_t gender = 1; // the movie's default, MALE
    std::int32_t race = 0;
    std::int32_t characterClass = 0;
    std::int32_t background = 0;
    std::wstring voice; // none until one is chosen, as the movie's SelectedSoundSet
    std::wstring name;  // none until one is given: the field's default
};
constexpr std::array<std::wstring_view, 3> kExpansionModules{L"DAO_PRC_EP_1", L"DAO_PRC_GIB", L"DAO_PRC_STR"};

std::optional<std::int32_t> Int(std::int32_t table, std::int32_t row, std::string_view column)
{
    return gda::GetInt(table, row, core::GdaColumnHash(column));
}

std::wstring Text(std::int32_t table, std::int32_t row, std::string_view column)
{
    return gda::GetString(table, row, core::GdaColumnHash(column)).value_or(L"");
}

// The tables' labels, as the movie's column names: ASCII.
std::string Narrow(const std::wstring &text)
{
    std::string narrow;
    narrow.reserve(text.size());
    for (const wchar_t letter : text)
    {
        narrow.push_back(static_cast<char>(letter));
    }
    return narrow;
}

std::wstring StrRefText(std::int32_t strref)
{
    return strref > 0 ? tlk::String(static_cast<std::uint32_t>(strref)) : std::wstring();
}

// The rows of a table by their index, as the movie walks them, each as its id.
std::vector<std::int32_t> Rows(std::int32_t table)
{
    std::vector<std::int32_t> rows;
    const std::int32_t count = api::GetM2DARows(table);
    rows.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
    for (std::int32_t index = 0; index < count; ++index)
    {
        rows.push_back(api::GetM2DARowIdFromRowIndex(table, index));
    }
    return rows;
}

Chosen &ChosenFor(ObjectId hero)
{
    static Chosen chosen;
    if (chosen.hero != hero)
    {
        chosen = Chosen{};
        chosen.hero = hero;
    }
    return chosen;
}

std::string RaceColumn(std::int32_t race)
{
    return Narrow(Text(kRaces, race, "Label"));
}

std::string ClassColumn(std::int32_t characterClass)
{
    return Narrow(Text(kClasses, characterClass, "CharGenLabel"));
}

bool Expansion()
{
    const std::wstring module = api::GetTag(api::GetModule());
    return std::ranges::any_of(kExpansionModules, [&](std::wstring_view name) { return module == name; });
}

// The classes the screen lists: table 3's PlayerClass 1 and BaseClass 0, in order.
std::vector<std::int32_t> ClassRows()
{
    std::vector<std::int32_t> rows;
    for (const std::int32_t row : Rows(kClasses))
    {
        if (Int(kClasses, row, "PlayerClass").value_or(0) == 1 && Int(kClasses, row, "BaseClass").value_or(-1) == 0)
        {
            rows.push_back(row);
        }
    }
    return rows;
}

bool ClassEnabled(std::int32_t race, std::int32_t characterClass, std::size_t place)
{
    if (race == 0)
    {
        return false;
    }
    const std::string raceColumn = RaceColumn(race);
    if (Expansion())
    {
        return Int(kClassesFilter, static_cast<std::int32_t>(place) + 1, raceColumn).value_or(0) == 1;
    }
    const std::string classColumn = ClassColumn(characterClass);
    return std::ranges::any_of(Rows(kBackgrounds), [&](std::int32_t row) {
        return Int(kBackgrounds, row, raceColumn).value_or(0) == 1 &&
               Int(kBackgrounds, row, classColumn).value_or(0) == 1;
    });
}

bool BackgroundEnabled(std::int32_t race, std::int32_t characterClass, std::int32_t background)
{
    if (race == 0 || characterClass == 0)
    {
        return false;
    }
    const std::string raceColumn = RaceColumn(race);
    return Int(kBackgrounds, background, raceColumn).value_or(0) == 1 &&
           Int(kBackgrounds, background, ClassColumn(characterClass)).value_or(0) == 1;
}

void Send(std::wstring_view message, ObjectId hero, std::int32_t before, std::int32_t after)
{
    messages::SendText(std::format(L"GUICharGen.{} {} {} {} 1", message, hero, before, after));
}

const Choice *Find(const std::vector<Choice> &choices, std::int32_t id)
{
    const auto found = std::ranges::find_if(choices, [&](const Choice &choice) { return choice.id == id; });
    return found != choices.end() ? &*found : nullptr;
}

// The name the screen's field starts with, the origin's default for the gender.
std::wstring DefaultName(const Chosen &chosen)
{
    const std::int32_t row = (chosen.race * 1000) + (chosen.characterClass * 100) + chosen.background;
    return StrRefText(
        Int(kBackgroundDefaults, row, chosen.gender == kMale ? "DefaultNameMale" : "DefaultNameFemale").value_or(0));
}

bool PointsLeft(ObjectId hero)
{
    const auto points = levelup::PointsOf(hero);
    return points && (points->attributes > 0 || points->skills > 0 || points->talentsOrSpells > 0);
}

// The name as the movie sends it: written to GUICharGen.TempCharName by its own command, then the
// message that reads it.
bool SendName(gui::Movie &movie, ObjectId hero, const std::wstring &name)
{
    const std::array<gui::Value, 2> args{gui::Value::String("GUICharGen.TempCharName"),
                                         gui::Value::WString(name.c_str())};
    if (!movie.Invoke("ExternalCommands.SetValueUnicode", args))
    {
        return false;
    }
    messages::SendText(std::format(L"GUICharGen.CharGenSelectName {}", hero));
    return true;
}

Refusal Check(const std::vector<Choice> &choices, std::int32_t id)
{
    const Choice *choice = Find(choices, id);
    if (choice == nullptr)
    {
        return Refusal::NotOffered;
    }
    return choice->enabled ? Refusal::None : Refusal::Disabled;
}

} // namespace

std::optional<ObjectId> Hero()
{
    const auto full = gui::CurrentFullScreen();
    if (!full || full->name != L"CharGen")
    {
        return std::nullopt;
    }
    const auto id = data::Number(L"CharGenCreature.ID");
    return id ? std::optional<ObjectId>(static_cast<ObjectId>(*id)) : std::nullopt;
}

std::vector<Choice> Genders()
{
    std::vector<Choice> genders;
    const auto hero = Hero();
    for (const std::int32_t row : Rows(kGenders))
    {
        if (Int(kGenders, row, "Playable").value_or(0) == 1)
        {
            genders.push_back({row, StrRefText(Int(kGenders, row, "Name").value_or(0)), true,
                               hero && ChosenFor(*hero).gender == row});
        }
    }
    return genders;
}

std::vector<Choice> Races()
{
    std::vector<Choice> races;
    const auto hero = Hero();
    auto rows = Rows(kRaces);
    std::ranges::reverse(rows);
    for (const std::int32_t row : rows)
    {
        if (Int(kRaces, row, "PlayerRace").value_or(0) == 1)
        {
            races.push_back(
                {row, StrRefText(Int(kRaces, row, "Name").value_or(0)), true, hero && ChosenFor(*hero).race == row});
        }
    }
    return races;
}

std::vector<Choice> Classes()
{
    std::vector<Choice> classes;
    const auto hero = Hero();
    const std::int32_t race = hero ? ChosenFor(*hero).race : 0;
    const auto rows = ClassRows();
    for (std::size_t place = 0; place < rows.size(); ++place)
    {
        const std::int32_t row = rows[place];
        classes.push_back({row, StrRefText(Int(kClasses, row, "NameStrRef").value_or(0)),
                           ClassEnabled(race, row, place), hero && ChosenFor(*hero).characterClass == row});
    }
    return classes;
}

std::vector<Choice> Backgrounds()
{
    std::vector<Choice> backgrounds;
    const auto hero = Hero();
    const Chosen chosen = hero ? ChosenFor(*hero) : Chosen{};
    const auto races = Races();
    for (const std::int32_t row : Rows(kBackgrounds))
    {
        // One per row a playable race may take (the game's rows' descriptions are one a row), named
        // as the movie names its button, by the first race in its order that takes it (366-380).
        const auto first = std::ranges::find_if(
            races, [&](const Choice &any) { return Int(kBackgrounds, row, RaceColumn(any.id)).value_or(0) == 1; });
        if (first == races.end())
        {
            continue;
        }
        const std::int32_t name = Int(kBackgroundNames, row, RaceColumn(first->id)).value_or(0);
        backgrounds.push_back({row, StrRefText(name), BackgroundEnabled(chosen.race, chosen.characterClass, row),
                               chosen.background == row});
    }
    return backgrounds;
}

std::vector<Voice> Voices()
{
    std::vector<Voice> voices;
    const auto hero = Hero();
    if (!hero)
    {
        return voices;
    }
    const std::int32_t gender = ChosenFor(*hero).gender;
    const std::int32_t race = ChosenFor(*hero).race;
    const auto rows = Rows(kSoundSets);
    for (std::size_t index = 1; index < rows.size(); ++index)
    {
        const std::int32_t row = rows[index];
        if (Int(kSoundSets, row, "Gender").value_or(0) != gender || Int(kSoundSets, row, "Race").value_or(0) != race)
        {
            continue;
        }
        std::wstring name = StrRefText(Int(kSoundSets, row, "StrIDName").value_or(0));
        if (name.empty())
        {
            name = L"?" + Text(kSoundSets, row, "Label");
        }
        voices.push_back({std::move(name), Text(kSoundSets, row, "SoundSet")});
    }
    return voices;
}

Refusal SelectGender(std::int32_t gender)
{
    const auto hero = Hero();
    if (!hero)
    {
        return Refusal::NoScreen;
    }
    if (const Refusal refused = Check(Genders(), gender); refused != Refusal::None)
    {
        return refused;
    }
    Chosen &chosen = ChosenFor(*hero);
    Send(L"CharGenSelectGender", *hero, chosen.gender, gender);
    chosen.gender = gender;
    chosen.voice.clear();
    return Refusal::None;
}

Refusal SelectRace(std::int32_t race)
{
    const auto hero = Hero();
    if (!hero)
    {
        return Refusal::NoScreen;
    }
    if (const Refusal refused = Check(Races(), race); refused != Refusal::None)
    {
        return refused;
    }
    // As the screen's race button (RaceGenderScene.as 437-440): the class and background cleared.
    Chosen &chosen = ChosenFor(*hero);
    if (chosen.background != 0)
    {
        Send(L"CharGenSelectBackground", *hero, chosen.background, 0);
        chosen.background = 0;
    }
    if (chosen.characterClass != 0)
    {
        Send(L"CharGenSelectClass", *hero, chosen.characterClass, 0);
        chosen.characterClass = 0;
    }
    Send(L"CharGenSelectRace", *hero, chosen.race, race);
    chosen.race = race;
    return Refusal::None;
}

Refusal SelectClass(std::int32_t characterClass)
{
    const auto hero = Hero();
    if (!hero)
    {
        return Refusal::NoScreen;
    }
    if (const Refusal refused = Check(Classes(), characterClass); refused != Refusal::None)
    {
        return refused;
    }
    // As the screen's class button (443-469): the background cleared, then pressed if one is open.
    Chosen &chosen = ChosenFor(*hero);
    if (chosen.background != 0)
    {
        Send(L"CharGenSelectBackground", *hero, chosen.background, 0);
        chosen.background = 0;
    }
    Send(L"CharGenSelectClass", *hero, chosen.characterClass, characterClass);
    chosen.characterClass = characterClass;
    std::vector<std::int32_t> open;
    for (const Choice &background : Backgrounds())
    {
        if (background.enabled)
        {
            open.push_back(background.id);
        }
    }
    if (open.size() == 1)
    {
        Send(L"CharGenSelectBackground", *hero, 0, open.front());
        chosen.background = open.front();
    }
    return Refusal::None;
}

Refusal SelectBackground(std::int32_t background)
{
    const auto hero = Hero();
    if (!hero)
    {
        return Refusal::NoScreen;
    }
    if (const Refusal refused = Check(Backgrounds(), background); refused != Refusal::None)
    {
        return refused;
    }
    Chosen &chosen = ChosenFor(*hero);
    Send(L"CharGenSelectBackground", *hero, chosen.background, background);
    chosen.background = background;
    return Refusal::None;
}

Refusal SelectVoice(const std::wstring &soundSet)
{
    const auto hero = Hero();
    if (!hero)
    {
        return Refusal::NoScreen;
    }
    const auto voices = Voices();
    if (std::ranges::none_of(voices, [&](const Voice &voice) { return voice.soundSet == soundSet; }))
    {
        return Refusal::NotOffered;
    }
    messages::SendText(std::format(L"GUI.SetSoundSet {}", soundSet));
    messages::SendText(std::format(L"GUICharGen.CharGenSelectAppearanceAndSoundSet {} 0 {}", *hero, soundSet));
    ChosenFor(*hero).voice = soundSet;
    return Refusal::None;
}

Refusal SetName(const std::wstring &name)
{
    const auto hero = Hero();
    if (!hero)
    {
        return Refusal::NoScreen;
    }
    if (name.empty() || name.size() > kMaxNameLength)
    {
        return Refusal::BadName;
    }
    ChosenFor(*hero).name = name;
    return Refusal::None;
}

Refusal Finish(Exit exit, std::optional<std::int32_t> difficulty)
{
    const auto hero = Hero();
    auto movie = gui::ScreenMovie(L"CharGen");
    if (!hero || !movie)
    {
        return Refusal::NoScreen;
    }
    const Chosen chosen = ChosenFor(*hero);
    if (chosen.background == 0)
    {
        return Refusal::Disabled;
    }
    if (exit == Exit::Full && PointsLeft(*hero))
    {
        return Refusal::PointsLeft;
    }
    if (difficulty && (*difficulty < 0 || *difficulty >= api::GetM2DARows(kDifficulties)))
    {
        return Refusal::NotOffered;
    }
    const std::wstring name = chosen.name.empty() ? DefaultName(chosen) : chosen.name;
    if (name.empty())
    {
        return Refusal::BadName;
    }
    // The movie's own order: the difficulty, then CompleteChargen, then the hide.
    if (difficulty)
    {
        messages::SendText(
            std::format(L"ClientOptions.SetClientOptionMessage GameOptions.DifficultyLevel {}", *difficulty));
        messages::SendText(L"ClientOptions.CommitOptions");
    }
    if (exit == Exit::Quick)
    {
        std::wstring voice = chosen.voice;
        if (const auto voices = Voices(); voice.empty() && !voices.empty())
        {
            voice = voices.front().soundSet;
        }
        messages::SendText(std::format(L"GUICharGen.CharGenSelectAppearanceAndSoundSet {} 0 {}", *hero, voice));
    }
    if (!SendName(*movie, *hero, name) ||
        !movie->SetVariable("CharGen.CreatureID", gui::Value::Number(static_cast<double>(*hero))) ||
        !movie->SetVariable("CharGen.nExitPoint", gui::Value::Number(static_cast<double>(exit))) ||
        !movie->SetVariable("CharGen.bUserExit", gui::Value::Bool(true)))
    {
        return Refusal::NoScreen;
    }
    messages::SendText(L"GUI.HideFullScreenGUI");
    return Refusal::None;
}

} // namespace dao::chargen
