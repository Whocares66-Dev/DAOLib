#include "dao/core/GfxMovie.h"
#include "dao/core/OptionsMovie.h"
#include "gfx_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace dao::test::gfx; // NOLINT(google-build-using-namespace): the builder's words
namespace gfx = dao::core::gfx;

// A PlaceObject2 as the designer's tool writes one: a character, a matrix
// with a scale as well as the place, and a name.
Bytes DesignerPlace(std::uint16_t depth, std::uint16_t character, int x, int y, std::string_view name)
{
    Bytes body{std::byte{0x26}};
    Put(body, depth, 2);
    Put(body, character, 2);
    // Bits, most significant first: a scale of 17 bits a number, no
    // rotation, the place in 15 bits a number.
    std::vector<int> bits;
    const auto put = [&bits](std::uint32_t value, int count) {
        for (int i = count - 1; i >= 0; --i)
        {
            bits.push_back(static_cast<int>((value >> i) & 1U));
        }
    };
    put(1, 1);
    put(17, 5);
    put(0x10000, 17);
    put(0x0FF86, 17);
    put(0, 1);
    put(15, 5);
    put(static_cast<std::uint32_t>(x * 20) & 0x7FFFU, 15);
    put(static_cast<std::uint32_t>(y * 20) & 0x7FFFU, 15);
    while (bits.size() % 8 != 0)
    {
        bits.push_back(0);
    }
    for (std::size_t i = 0; i < bits.size(); i += 8)
    {
        int byte = 0;
        for (std::size_t j = 0; j < 8; ++j)
        {
            byte = byte << 1 | bits[i + j];
        }
        body.push_back(static_cast<std::byte>(byte));
    }
    PutText(body, name);
    return body.size() < 0x3F ? ShortTag(26, body) : LongTag(26, body);
}

Bytes Exports(std::initializer_list<std::pair<std::uint16_t, std::string_view>> names)
{
    Bytes body;
    Put(body, static_cast<std::uint32_t>(names.size()), 2);
    for (const auto &[character, name] : names)
    {
        Put(body, character, 2);
        PutText(body, name);
    }
    return LongTag(56, body);
}

constexpr std::uint16_t kCheckbox = 222;
constexpr std::uint16_t kHeading = 64;
constexpr std::uint16_t kGameTab = 297;
constexpr std::uint16_t kBar = 294;
constexpr std::uint16_t kHeadingText = 224;
constexpr std::uint16_t kSlider = 171;
constexpr std::uint16_t kPicker = 219;
constexpr std::uint16_t kVideoLeft = 314;
constexpr std::uint16_t kVideoRight = 315;
constexpr std::uint16_t kVideoTab = 316;
constexpr std::uint16_t kDetailText = 98;
constexpr std::uint16_t kDetailBar = 310;
constexpr std::uint16_t kGammaPicture = 313;

// The main timeline's placement of the Video tab's "Detail" bar.
Bytes DetailBarAt(int x, int y)
{
    return DesignerPlace(56, kDetailBar, x, y, "");
}

// The options movie in small: the game's own heading (its text clip
// exported, over a bar), the Game tab with the last checkbox of each
// column and two more controls, and the function that makes their set-up
// functions. Without `pickers` it has none of the game's dropdowns.
Bytes OptionsMovie(bool pickers = true)
{
    Bytes tab;
    Put(tab, kGameTab, 2);
    Put(tab, 1, 2);
    Add(tab, DesignerPlace(25, kCheckbox, 7, 158, "OptAutoPause_mc"));
    Add(tab, DesignerPlace(7, kCheckbox, 483, 166, "OptShowTutorials_mc"));
    Add(tab, DesignerPlace(43, kCheckbox, 7, 228, "OptLootShimmer_mc"));
    if (pickers)
    {
        Add(tab, DesignerPlace(59, kPicker, 11, 0, "OptDifficulty_mc"));
        Add(tab, DesignerPlace(69, kPicker, 488, 0, "OptSubtitles_mc"));
    }
    Add(tab, ShortTag(1, {})); // ShowFrame
    Put(tab, 0, 2);

    // r2.OptShowTutorials_mc_init = "made"; r2.OptAutoPause_mc_init = "made";
    Bytes setUps = Pool({"OptShowTutorials_mc_init", "OptAutoPause_mc_init", "made"});
    const Bytes first{std::byte{4}, std::byte{2}, std::byte{8}, std::byte{0}, std::byte{8}, std::byte{2}};
    Add(setUps, Record(0x96, first));
    Add(setUps, Simple(0x4F));
    const Bytes second{std::byte{4}, std::byte{2}, std::byte{8}, std::byte{1}, std::byte{8}, std::byte{2}};
    Add(setUps, Record(0x96, second));
    Add(setUps, Simple(0x4F));
    Bytes script;
    Put(script, 7, 2);
    Add(script, Function("SetupOptionsMenuInitFunctions", setUps));

    // "Gameplay": the bar, and the heading's text over it.
    Bytes gameplay;
    Put(gameplay, 296, 2);
    Put(gameplay, 1, 2);
    Add(gameplay, DesignerPlace(1, kBar, 0, 0, ""));
    Add(gameplay, DesignerPlace(2, kHeadingText, 24, 13, ""));
    Add(gameplay, ShortTag(1, {}));
    Put(gameplay, 0, 2);

    // The Video tab as the game has it: two columns, a clip each, in a
    // clip of the two. The left has its dropdown deepest, a checkbox, the
    // gamma's slider with its picture, a shape at the clip's origin, and a
    // checkbox low under them; the right three sliders with named stops,
    // whose symbol a choice is, and a checkbox.
    Bytes videoLeft;
    Put(videoLeft, kVideoLeft, 2);
    Put(videoLeft, 1, 2);
    Add(videoLeft, DesignerPlace(1, kCheckbox, 3, 120, "OptFullscreen_mc"));
    Add(videoLeft, DesignerPlace(7, kSlider, 44, 153, "OptGamma_mc"));
    Add(videoLeft, DesignerPlace(28, kCheckbox, 3, 302, "OptVSync_mc"));
    Add(videoLeft, DesignerPlace(34, kGammaPicture, 0, 0, ""));
    Add(videoLeft, DesignerPlace(35, kPicker, 9, 21, "OptResolution_mc"));
    Add(videoLeft, ShortTag(1, {}));
    Put(videoLeft, 0, 2);
    Bytes videoRight;
    Put(videoRight, kVideoRight, 2);
    Put(videoRight, 1, 2);
    Add(videoRight, DesignerPlace(1, kSlider, 72, 115, "OptAntialiasing_mc"));
    Add(videoRight, DesignerPlace(22, kCheckbox, 35, 267, "OptFBFX_mc"));
    Add(videoRight, DesignerPlace(28, kSlider, 72, 69, "OptGraphicsDetail_mc"));
    Add(videoRight, DesignerPlace(49, kSlider, 72, 161, "OptTextureQuality_mc"));
    Add(videoRight, ShortTag(1, {}));
    Put(videoRight, 0, 2);
    Bytes video;
    Put(video, kVideoTab, 2);
    Put(video, 1, 2);
    Add(video, DesignerPlace(1, kVideoLeft, 0, 50, ""));
    Add(video, DesignerPlace(37, kVideoRight, 445, 0, ""));
    Add(video, ShortTag(1, {}));
    Put(video, 0, 2);
    // "Detail": the bar and its text, a clip the main timeline places.
    Bytes detail;
    Put(detail, kDetailBar, 2);
    Put(detail, 1, 2);
    Add(detail, DesignerPlace(1, kBar, 0, 0, ""));
    Add(detail, DesignerPlace(2, kDetailText, 24, 13, ""));
    Add(detail, ShortTag(1, {}));
    Put(detail, 0, 2);

    Bytes tags = Exports({{kHeading, "KeyBindingHeader"},
                          {kCheckbox, "checkbox_mc"},
                          {kHeadingText, "txt_Game"},
                          {kDetailText, "txt_Detail"}});
    Add(tags, LongTag(39, gameplay));
    Add(tags, LongTag(39, videoLeft));
    Add(tags, LongTag(39, videoRight));
    Add(tags, LongTag(39, video));
    Add(tags, LongTag(39, detail));
    Add(tags, LongTag(39, tab));
    Add(tags, LongTag(59, script));
    Add(tags, DetailBarAt(524, 131));
    Add(tags, DesignerPlace(62, kVideoTab, 82, 155, ""));
    Add(tags, ShortTag(1, {}));
    return Movie(tags);
}

bool Holds(const Bytes &movie, const Bytes &code)
{
    return !std::ranges::search(movie, code).empty();
}

// How many of the sections went in.
std::size_t Shown(const std::vector<bool> &shown)
{
    return static_cast<std::size_t>(std::ranges::count(shown, true));
}

} // namespace

TEST_CASE("a push and a function are written as the game's compiled script has them", "[gfx]")
{
    Bytes expected{std::byte{4}, std::byte{2}, std::byte{0}};
    PutText(expected, "name");
    expected.push_back(std::byte{7});
    Put(expected, 3, 4);
    CHECK(gfx::Push({gfx::Register(2), gfx::Text("name"), gfx::Integer(3)}) == Record(0x96, expected));

    // A null is its type alone.
    CHECK(gfx::Push({gfx::Null(), gfx::Register(1)}) == Record(0x96, Bytes{std::byte{2}, std::byte{4}, std::byte{1}}));

    CHECK(gfx::Do(gfx::Op::SetMember) == Simple(0x4F));
    CHECK(gfx::Do(gfx::Op::GetMember) == Simple(0x4E));
    CHECK(gfx::Do(gfx::Op::GetVariable) == Simple(0x1C));
    CHECK(gfx::Do(gfx::Op::CallMethod) == Simple(0x52));
    CHECK(gfx::Do(gfx::Op::CallFunction) == Simple(0x3D));
    CHECK(gfx::Do(gfx::Op::Pop) == Simple(0x17));

    // No name, no parameters, two registers, flags 0x29, the body's size.
    const Bytes body = Simple(0x17);
    Bytes record;
    PutText(record, "");
    Put(record, 0, 2);
    record.push_back(std::byte{2});
    Put(record, 0x29, 2);
    Put(record, 1, 2);
    Bytes function = Record(0x8E, record);
    Add(function, body);
    CHECK(gfx::Function(body) == function);

    CHECK(gfx::Join({Simple(1), Simple(2), {}}) == Bytes{std::byte{1}, std::byte{2}});
}

TEST_CASE("an instance is found by its name, with its sprite, character and place", "[gfx]")
{
    const Bytes movie = OptionsMovie();
    const auto pause = gfx::FindInstance(movie, "OptAutoPause_mc");
    REQUIRE(pause);
    CHECK(pause->sprite == kGameTab);
    CHECK(pause->character == kCheckbox);
    CHECK(pause->depth == 25);
    CHECK(pause->deepest == 69);
    CHECK(pause->x == 7);
    CHECK(pause->y == 158);

    const auto subtitles = gfx::FindInstance(movie, "OptSubtitles_mc");
    REQUIRE(subtitles);
    CHECK(subtitles->character == kPicker);
    CHECK(subtitles->x == 488);
    CHECK(subtitles->y == 0);

    CHECK_FALSE(gfx::FindInstance(movie, "OptNoSuch_mc"));
    CHECK_FALSE(gfx::FindInstance(movie, ""));
    CHECK_FALSE(gfx::FindInstance({}, "OptAutoPause_mc"));

    CHECK(gfx::ExportedCharacter(movie, "KeyBindingHeader") == kHeading);
    CHECK(gfx::ExportedCharacter(movie, "checkbox_mc") == kCheckbox);
    CHECK_FALSE(gfx::ExportedCharacter(movie, "KeyBinding"));
    CHECK_FALSE(gfx::ExportedCharacter({}, "KeyBindingHeader"));
}

TEST_CASE("placements go on a sprite's first frame and are found there after", "[gfx]")
{
    const Bytes movie = OptionsMovie();
    const std::vector<gfx::Placement> placements = {{"Mine_mc", kCheckbox, 75, 7, 263},
                                                    {"Far_mc", kHeading, 81, 483, 1201}};
    const auto patched = gfx::PlaceInSprite(movie, kGameTab, placements);
    REQUIRE(patched);
    CHECK(Get(*patched, 4, 4) == patched->size());

    const auto mine = gfx::FindInstance(*patched, "Mine_mc");
    REQUIRE(mine);
    CHECK(mine->sprite == kGameTab);
    CHECK(mine->character == kCheckbox);
    CHECK(mine->depth == 75);
    CHECK(mine->x == 7);
    CHECK(mine->y == 263);
    CHECK(mine->deepest == 81);
    const auto far = gfx::FindInstance(*patched, "Far_mc");
    REQUIRE(far);
    CHECK(far->character == kHeading);
    CHECK(far->x == 483);
    CHECK(far->y == 1201);

    // What was there is there still, and the script after the sprite reads.
    const auto shimmer = gfx::FindInstance(*patched, "OptLootShimmer_mc");
    REQUIRE(shimmer);
    CHECK(shimmer->y == 228);
    CHECK(gfx::InsertBefore(*patched, {2, "OptAutoPause_mc_init"}, gfx::Do(gfx::Op::Pop)));

    CHECK_FALSE(gfx::PlaceInSprite(movie, 9999, placements));
    CHECK_FALSE(gfx::PlaceInSprite({}, kGameTab, placements));
}

TEST_CASE("code goes in before the statement that sets a member", "[gfx]")
{
    const Bytes movie = OptionsMovie();
    const Bytes code = gfx::Join(
        {gfx::Push({gfx::Register(2), gfx::Text("Mine_mc_init"), gfx::Integer(1)}), gfx::Do(gfx::Op::SetMember)});
    const auto patched = gfx::InsertBefore(movie, {2, "OptAutoPause_mc_init"}, code);
    REQUIRE(patched);
    REQUIRE(patched->size() == movie.size() + code.size());

    // Straight before the second of the two statements: the first's
    // setMember, then the code, then the push of the pool's second string.
    const auto at = std::search(patched->begin(), patched->end(), code.begin(), code.end());
    REQUIRE(at != patched->end());
    CHECK(*(at - 1) == std::byte{0x4F});
    const Bytes second{std::byte{0x96}, std::byte{6}, std::byte{0}, std::byte{4},
                       std::byte{2},    std::byte{8}, std::byte{1}};
    CHECK(std::equal(second.begin(), second.end(), at + static_cast<std::ptrdiff_t>(code.size())));

    // The first statement is an anchor too; another register, or a string
    // that is not the member, is not.
    CHECK(gfx::InsertBefore(movie, {2, "OptShowTutorials_mc_init"}, code));
    CHECK_FALSE(gfx::InsertBefore(movie, {1, "OptAutoPause_mc_init"}, code));
    CHECK_FALSE(gfx::InsertBefore(movie, {2, "made"}, code));
}

TEST_CASE("the Game tab's free rows are under its two columns", "[options]")
{
    using dao::core::options_movie::FreeSlots;
    const auto slots = FreeSlots(OptionsMovie());
    REQUIRE(slots);
    // Under OptLootShimmer_mc at (7, 228), a row every 35 to 403; then
    // under OptShowTutorials_mc at (483, 166).
    REQUIRE(slots->size() == 11);
    CHECK((*slots)[0] == dao::core::options_movie::Slot{7, 263, 0});
    CHECK((*slots)[4] == dao::core::options_movie::Slot{7, 403, 0});
    CHECK((*slots)[5] == dao::core::options_movie::Slot{483, 201, 1});
    CHECK((*slots)[10] == dao::core::options_movie::Slot{483, 376, 1});
    CHECK_FALSE(FreeSlots({}));
}

TEST_CASE("a section is the game's heading over its checkboxes, in one column", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    const std::vector<Section> sections = {
        {"Turn-based", {{"DAOLibTurnBasedCombat", "Turn-based combat", "Outside a fight only"}}},
        {"Second", {{"DAOLibA", "A", ""}, {"DAOLibB", "B", ""}}},
    };
    std::vector<bool> shown;
    const auto patched = AddSections(movie, sections, shown);
    REQUIRE(patched);
    CHECK(Shown(shown) == 2);

    // The first: the bar in the left column's first two free rows, 15 left
    // of the checkboxes and 6 down, the heading's text in the bar at (24,
    // 13), and the checkbox in the third row.
    const auto placed = gfx::InstancesOf(*patched, kGameTab);
    const auto bar =
        std::ranges::find_if(placed, [](const gfx::Instance &instance) { return instance.character == kBar; });
    REQUIRE(bar != placed.end());
    CHECK(bar->x == -8);
    CHECK(bar->y == 269);
    CHECK(bar->name.empty());
    const auto heading = gfx::FindInstance(*patched, HeadingInstance({7, 263, 0}));
    REQUIRE(heading);
    CHECK(heading->character == kHeadingText);
    CHECK(heading->x == 16);
    CHECK(heading->y == 282);
    CHECK(heading->depth > bar->depth);
    const auto toggle = gfx::FindInstance(*patched, ControlInstance("DAOLibTurnBasedCombat"));
    REQUIRE(toggle);
    CHECK(toggle->character == kCheckbox);
    CHECK(toggle->x == 7);
    CHECK(toggle->y == 333);

    // The second needs four rows one under the other, and the left column
    // has two left: it goes to the right column, whose bar is 37 left of
    // its checkboxes.
    const auto second = gfx::FindInstance(*patched, HeadingInstance({483, 201, 1}));
    const auto a = gfx::FindInstance(*patched, ControlInstance("DAOLibA"));
    const auto b = gfx::FindInstance(*patched, ControlInstance("DAOLibB"));
    REQUIRE((second && a && b));
    CHECK(second->x == 446 + 24);
    CHECK(second->y == 201 + 6 + 13);
    CHECK(a->x == 483);
    CHECK(a->y == 271);
    CHECK(b->y == 306);

    // The checkbox's set-up, as the game's own are made.
    const Bytes setUp =
        gfx::Join({gfx::Push({gfx::Register(2), gfx::Text("OptDAOLibTurnBasedCombat_mc_init"),
                              gfx::Text("Outside a fight only"), gfx::Text("Turn-based combat"),
                              gfx::Text("DAOLibTurnBasedCombat"), gfx::Integer(3), gfx::Text("OptionsUtils")}),
                   gfx::Do(gfx::Op::GetVariable), gfx::Push({gfx::Text("MakeOptionToggleInit")}),
                   gfx::Do(gfx::Op::CallMethod), gfx::Do(gfx::Op::SetMember)});
    CHECK(Holds(*patched, setUp));
    // The heading's: the text clip's own set-up sets the title.
    const Bytes setText =
        gfx::Join({gfx::Push({gfx::Text("Turn-based"), gfx::Integer(1), gfx::Register(1), gfx::Text("SetText")}),
                   gfx::Do(gfx::Op::CallMethod), gfx::Do(gfx::Op::Pop)});
    const Bytes headingSetUp = gfx::Join({gfx::Push({gfx::Register(2), gfx::Text("DAOLibSectionAt7x263_mc_init")}),
                                          gfx::Function(setText), gfx::Do(gfx::Op::SetMember)});
    CHECK(Holds(*patched, headingSetUp));

    // They are before the game's first, and the patched movie still reads.
    CHECK(gfx::InsertBefore(*patched, {2, "OptAutoPause_mc_init"}, gfx::Do(gfx::Op::Pop)));
    CHECK(Get(*patched, 4, 4) == patched->size());
}

TEST_CASE("a second plugin's sections go in the rows the first left", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    std::vector<bool> shown;
    // Each plugin has its own copy of the library, and each copy patches
    // what the other left.
    const auto first = AddSections(
        movie, std::vector<Section>{{"First", {{"DAOLibOne", "One", ""}, {"DAOLibTwo", "Two", ""}}}}, shown);
    REQUIRE(first);
    CHECK(Shown(shown) == 1);
    // The bar lies in two rows and the checkboxes in two: one row is left
    // on the left, and the right column's six.
    const auto slots = FreeSlots(*first);
    REQUIRE(slots);
    REQUIRE(slots->size() == 7);
    CHECK((*slots)[0] == Slot{7, 403, 0});
    CHECK((*slots)[1] == Slot{483, 201, 1});

    const auto both = AddSections(*first, std::vector<Section>{{"Second", {{"DAOLibThree", "Three", ""}}}}, shown);
    REQUIRE(both);
    CHECK(Shown(shown) == 1);
    // The first plugin's are where they were, the second's in the right
    // column, and the two headings have names of their own.
    const auto one = gfx::FindInstance(*both, ControlInstance("DAOLibOne"));
    const auto three = gfx::FindInstance(*both, ControlInstance("DAOLibThree"));
    const auto firstHeading = gfx::FindInstance(*both, HeadingInstance({7, 263, 0}));
    const auto secondHeading = gfx::FindInstance(*both, HeadingInstance({483, 201, 1}));
    REQUIRE((one && three && firstHeading && secondHeading));
    CHECK(one->y == 333);
    CHECK(three->x == 483);
    CHECK(three->y == 271);
    CHECK(secondHeading->depth > one->depth);
    CHECK(HeadingInstance({7, 263, 0}) != HeadingInstance({483, 201, 1}));
    CHECK(Holds(*both, gfx::Push({gfx::Text("First"), gfx::Integer(1), gfx::Register(1), gfx::Text("SetText")})));
    CHECK(Holds(*both, gfx::Push({gfx::Text("Second"), gfx::Integer(1), gfx::Register(1), gfx::Text("SetText")})));
}

TEST_CASE("everything a sprite places is listed", "[gfx]")
{
    const Bytes movie = OptionsMovie();
    const auto instances = gfx::InstancesOf(movie, kGameTab);
    REQUIRE(instances.size() == 5);
    CHECK(instances[0].name == "OptAutoPause_mc");
    CHECK(instances[4].name == "OptSubtitles_mc");
    CHECK(instances[4].deepest == 69);
    CHECK(gfx::InstancesOf(movie, 9999).empty());
    CHECK(gfx::InstancesOf({}, kGameTab).empty());
}

TEST_CASE("actions are listed under their headings after the game's own", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    // The key list in small: two of the game's rows.
    Bytes script = Pool({"AddKeyBinding", "TogglePause", "HideMainInterface"});
    Add(script, PooledCall(1, 0));
    Add(script, PooledCall(2, 0));
    Add(script, Simple(0x17));
    const Bytes movie = Movie(LongTag(12, script));

    const std::vector<KeySection> sections = {
        {"Turn-based", {"TurnBasedToggle"}}, {"Mods", {"OtherOne", "OtherTwo"}}, {"Nothing", {}}};
    const auto listed = ListKeys(movie, sections);
    REQUIRE(listed);
    const std::vector<gfx::Call> expected = {{"AddHeader", "Turn-based"},
                                             {"AddKeyBinding", "TurnBasedToggle"},
                                             {"AddHeader", "Mods"},
                                             {"AddKeyBinding", "OtherOne"},
                                             {"AddKeyBinding", "OtherTwo"}};
    const Bytes code = gfx::Statements(expected);
    const auto at = std::search(listed->begin(), listed->end(), code.begin(), code.end());
    REQUIRE(at != listed->end());
    // Straight after the game's last row: its call and pop.
    CHECK(*(at - 1) == std::byte{0x17});
    CHECK(*(at - 2) == std::byte{0x3D});
    CHECK(listed->size() == movie.size() + code.size());

    // Another plugin's copy of the library adds to what this one left: a
    // heading that is there takes the rows, a new one is made after the
    // game's rows.
    const std::vector<KeySection> more = {{"Mods", {"ThirdOne"}}, {"Camera mod", {"Orbit"}}};
    const auto both = ListKeys(*listed, more);
    REQUIRE(both);
    const Bytes mods = gfx::Statements(
        std::vector<gfx::Call>{{"AddHeader", "Mods"}, {"AddKeyBinding", "ThirdOne"}, {"AddKeyBinding", "OtherOne"}});
    CHECK(Holds(*both, mods));
    const Bytes camera = gfx::Statements(
        std::vector<gfx::Call>{{"AddHeader", "Camera mod"}, {"AddKeyBinding", "Orbit"}, {"AddHeader", "Turn-based"}});
    CHECK(Holds(*both, camera));
    // One "Mods" heading, not two.
    const Bytes heading = gfx::Statements(std::vector<gfx::Call>{{"AddHeader", "Mods"}});
    const auto firstHeading = std::search(both->begin(), both->end(), heading.begin(), heading.end());
    REQUIRE(firstHeading != both->end());
    CHECK(std::search(firstHeading + 1, both->end(), heading.begin(), heading.end()) == both->end());

    // Nothing to add, or a list that is not the game's.
    CHECK_FALSE(ListKeys(movie, {}));
    CHECK_FALSE(ListKeys(movie, std::vector<KeySection>{{"Nothing", {}}}));
    CHECK_FALSE(ListKeys(Movie(LongTag(12, Simple(0x17))), sections));
}

TEST_CASE("a section goes in the column it asks for, or in the other for want of room", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    // The left column is free and comes first, and the section asks for the right.
    Section right{"Right", {{"DAOLibR", "R", ""}}};
    right.column = 1;
    const Section either{"Either", {{"DAOLibE", "E", ""}}};
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{right, either}, shown);
    REQUIRE(patched);
    CHECK(Shown(shown) == 2);
    const auto inRight = gfx::FindInstance(*patched, ControlInstance("DAOLibR"));
    const auto inLeft = gfx::FindInstance(*patched, ControlInstance("DAOLibE"));
    REQUIRE((inRight && inLeft));
    CHECK(inRight->x == 483);
    CHECK(inLeft->x == 7);

    // The right holds a heading and four: with those in it, a section that
    // asks for the right goes in the left.
    const Section four{"Four",
                       {{"DAOLibF0", "0", ""}, {"DAOLibF1", "1", ""}, {"DAOLibF2", "2", ""}, {"DAOLibF3", "3", ""}}};
    const auto full = AddSections(movie, std::vector<Section>{four, right}, shown);
    REQUIRE(full);
    CHECK(Shown(shown) == 2);
    const auto pushed = gfx::FindInstance(*full, ControlInstance("DAOLibR"));
    REQUIRE(pushed);
    CHECK(pushed->x == 7);
}

TEST_CASE("a section with no room is left out, and those after it", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    // A heading and four checkboxes are six rows: only the right column
    // has them. A heading and three are the left's five. Then nothing is
    // free, and what comes after is left out.
    const Section four{"Four",
                       {{"DAOLibF0", "0", ""}, {"DAOLibF1", "1", ""}, {"DAOLibF2", "2", ""}, {"DAOLibF3", "3", ""}}};
    const Section three{"Three", {{"DAOLibX", "X", ""}, {"DAOLibY", "Y", ""}, {"DAOLibZ", "Z", ""}}};
    const Section small{"Small", {{"DAOLibS", "S", ""}}};
    std::vector<bool> shown(9, true);
    const auto patched = AddSections(movie, std::vector<Section>{four, three, small}, shown);
    REQUIRE(patched);
    CHECK(shown == std::vector<bool>{true, true, false});
    const auto lastOfFour = gfx::FindInstance(*patched, ControlInstance("DAOLibF3"));
    const auto lastOfThree = gfx::FindInstance(*patched, ControlInstance("DAOLibZ"));
    REQUIRE((lastOfFour && lastOfThree));
    CHECK(lastOfFour->x == 483);
    CHECK(lastOfFour->y == 376);
    CHECK(lastOfThree->x == 7);
    CHECK(lastOfThree->y == 403);
    CHECK_FALSE(gfx::FindInstance(*patched, ControlInstance("DAOLibS")));
    const auto none = FreeSlots(*patched);
    REQUIRE(none);
    CHECK(none->empty());

    // A section stops those after it, though they would fit: five
    // checkboxes are seven rows, more than either column.
    Section huge{"Huge", {}};
    for (int i = 0; i < 5; ++i)
    {
        huge.toggles.push_back({"DAOLibHuge" + std::to_string(i), "H", ""});
    }
    CHECK_FALSE(AddSections(movie, std::vector<Section>{huge, small}, shown));
    CHECK(Shown(shown) == 0);
    shown.assign(9, true);
    CHECK_FALSE(AddSections(movie, {}, shown));
    CHECK(Shown(shown) == 0);
    CHECK_FALSE(AddSections(movie, std::vector<Section>{{"Empty", {}}}, shown));
    // A movie with no Game tab, and one whose heading is not the game's.
    CHECK_FALSE(AddSections(Movie(LongTag(12, Simple(0x17))), std::vector<Section>{small}, shown));
    CHECK(Shown(shown) == 0);
}

TEST_CASE("a choice is the game's slider with named stops, under the checkboxes", "[options]")
{
    using dao::core::options_movie::AddSections;
    using dao::core::options_movie::Choice;
    using dao::core::options_movie::ControlInstance;
    using dao::core::options_movie::FreeSlots;
    using dao::core::options_movie::Section;
    const Bytes movie = OptionsMovie();
    Section section{"Turn-based", {{"DAOLibTurnBased", "Turn-based combat", ""}}};
    section.choices.push_back({"DAOLibSpeed", "Speed", "How fast turns play", {"0.5x", "1x", "1.5x", "2x"}});
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{section}, shown);
    REQUIRE(patched);
    CHECK(Shown(shown) == 1);

    // The heading in the left column's first two free rows (263, 298), the
    // checkbox in the third (333), the slider in the fourth (368) and, its
    // 41 being more than a row, the fifth (403), 40 right of the column, as
    // the Video tab's symbol.
    const auto choice = gfx::FindInstance(*patched, ControlInstance("DAOLibSpeed"));
    REQUIRE(choice);
    CHECK(choice->sprite == kGameTab);
    CHECK(choice->character == kSlider);
    CHECK(choice->x == 47);
    CHECK(choice->y == 368);

    // Its set-up: the values' array made, then the game's helper called.
    const Bytes setUp =
        gfx::Join({gfx::Push({gfx::Register(2), gfx::Text("OptDAOLibSpeed_mc_init")}),
                   gfx::Push({gfx::Text("2x"), gfx::Text("1.5x"), gfx::Text("1x"), gfx::Text("0.5x"), gfx::Integer(4)}),
                   gfx::Do(gfx::Op::InitArray),
                   gfx::Push({gfx::Text("How fast turns play"), gfx::Text("Speed"), gfx::Text("DAOLibSpeed"),
                              gfx::Integer(4), gfx::Text("OptionsUtils")}),
                   gfx::Do(gfx::Op::GetVariable), gfx::Push({gfx::Text("MakeSliderPickerInit")}),
                   gfx::Do(gfx::Op::CallMethod), gfx::Do(gfx::Op::SetMember)});
    CHECK(Holds(*patched, setUp));

    // Both of its rows are taken for whoever comes next: the left column is
    // full, and the right's six rows are free still.
    const auto after = FreeSlots(*patched);
    REQUIRE(after);
    CHECK(after->size() == 6);
    CHECK(std::all_of(after->begin(), after->end(), [](const auto &slot) { return slot.column == 1; }));
}

TEST_CASE("a choice can be the game's dropdown, deepest so that its list covers what is under it", "[options]")
{
    using dao::core::options_movie::AddSections;
    using dao::core::options_movie::Choice;
    using dao::core::options_movie::ControlInstance;
    using dao::core::options_movie::FreeSlots;
    using dao::core::options_movie::Section;
    const Bytes movie = OptionsMovie();
    // Two sections with a dropdown each: one over a slider, six rows, which
    // only the right column has; one under a checkbox, the left's five.
    Section graphics{"Graphics", {}};
    graphics.choices.push_back({"DAOLibAntiAliasing", "Anti-aliasing", "Which", {"Default", "SMAA"}, true});
    graphics.choices.push_back({"DAOLibLook", "Look", "", {"Plain", "Vivid"}});
    Section other{"Other", {{"DAOLibBox", "A box", ""}}};
    other.choices.push_back({"DAOLibMode", "Mode", "", {"a", "b", "c"}, true});
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{graphics, other}, shown);
    REQUIRE(patched);
    CHECK(Shown(shown) == 2);

    // The first: the heading at 201 and 236, the dropdown at 271, 4 right
    // of the column as the tab's own stand, the slider under it at 341.
    const auto dropdown = gfx::FindInstance(*patched, ControlInstance("DAOLibAntiAliasing"));
    const auto slider = gfx::FindInstance(*patched, ControlInstance("DAOLibLook"));
    REQUIRE((dropdown && slider));
    CHECK(dropdown->sprite == kGameTab);
    CHECK(dropdown->character == kPicker);
    CHECK(dropdown->x == 487);
    CHECK(dropdown->y == 271);
    CHECK(slider->character == kSlider);
    CHECK(slider->y == 341);
    // The second: its checkbox at 333, its dropdown at 368.
    const auto box = gfx::FindInstance(*patched, ControlInstance("DAOLibBox"));
    const auto mode = gfx::FindInstance(*patched, ControlInstance("DAOLibMode"));
    REQUIRE((box && mode));
    CHECK(mode->x == 11);
    CHECK(mode->y == 368);

    // The dropdowns are deeper than everything else placed, whichever
    // section it is of, and the first, added first, deeper than the second.
    CHECK(dropdown->depth > slider->depth);
    CHECK(dropdown->depth > box->depth);
    CHECK(mode->depth > box->depth);
    CHECK(mode->depth > slider->depth);
    CHECK(dropdown->depth > mode->depth);

    // Its set-up: the movie's own callback, no tooltips for the entries, the
    // values' array, then the game's helper called with the six.
    const Bytes setUp =
        gfx::Join({gfx::Push({gfx::Register(2), gfx::Text("OptDAOLibAntiAliasing_mc_init"), gfx::Text("OptionsMenu")}),
                   gfx::Do(gfx::Op::GetVariable), gfx::Push({gfx::Text("PickEntryCallBack")}),
                   gfx::Do(gfx::Op::GetMember), gfx::Push({gfx::Null()}),
                   gfx::Push({gfx::Text("SMAA"), gfx::Text("Default"), gfx::Integer(2)}), gfx::Do(gfx::Op::InitArray),
                   gfx::Push({gfx::Text("Which"), gfx::Text("Anti-aliasing"), gfx::Text("DAOLibAntiAliasing"),
                              gfx::Integer(6), gfx::Text("OptionsUtils")}),
                   gfx::Do(gfx::Op::GetVariable), gfx::Push({gfx::Text("MakePickerInit")}),
                   gfx::Do(gfx::Op::CallMethod), gfx::Do(gfx::Op::SetMember)});
    CHECK(Holds(*patched, setUp));
    // The slider beside it is still set up as a slider.
    CHECK(Holds(*patched, gfx::Push({gfx::Text("MakeSliderPickerInit")})));
    CHECK(gfx::InsertBefore(*patched, {2, "OptAutoPause_mc_init"}, gfx::Do(gfx::Op::Pop)));

    // A dropdown's two rows are taken for whoever comes next, as a slider's
    // are: nothing is free after these two, and after a heading and a
    // dropdown alone the left column has its last row.
    const auto after = FreeSlots(*patched);
    REQUIRE(after);
    CHECK(after->empty());
    const auto alone =
        AddSections(movie, std::vector<Section>{{"One", {}, {{"DAOLibOnly", "Only", "", {"a", "b"}, true}}}}, shown);
    REQUIRE(alone);
    const auto left = FreeSlots(*alone);
    REQUIRE(left);
    REQUIRE(left->size() == 7);
    CHECK((*left)[0] == dao::core::options_movie::Slot{7, 403, 0});
}

TEST_CASE("a dropdown in a movie with no picker of the game's is left out with its section", "[options]")
{
    using dao::core::options_movie::AddSections;
    using dao::core::options_movie::Section;
    const Bytes movie = OptionsMovie(false);
    std::vector<bool> shown(9, true);
    CHECK_FALSE(
        AddSections(movie, std::vector<Section>{{"One", {}, {{"DAOLibOnly", "Only", "", {"a", "b"}, true}}}}, shown));
    CHECK(Shown(shown) == 0);
    // The same as a slider, or a checkbox alone, goes in.
    CHECK(AddSections(movie, std::vector<Section>{{"One", {}, {{"DAOLibOnly", "Only", "", {"a", "b"}}}}}, shown));
    CHECK(Shown(shown) == 1);
}

TEST_CASE("a section with a choice that does not fit is left out", "[options]")
{
    using dao::core::options_movie::AddSections;
    using dao::core::options_movie::Section;
    // A heading, a checkbox and two sliders are six rows, the sliders' 82
    // in three, which the right column has; a third slider makes it seven,
    // more than either column has.
    Section section{"Big", {{"DAOLibOne", "One", ""}}};
    section.choices.push_back({"DAOLibA", "A", "", {"x", "y"}});
    section.choices.push_back({"DAOLibB", "B", "", {"x", "y"}});
    std::vector<bool> shown;
    const auto fits = AddSections(OptionsMovie(), std::vector<Section>{section}, shown);
    REQUIRE(fits);
    CHECK(Shown(shown) == 1);
    CHECK(gfx::FindInstance(*fits, "OptDAOLibA_mc")->sprite == kGameTab);
    CHECK(gfx::FindInstance(*fits, "OptDAOLibA_mc")->x == 483 + 40);
    section.choices.push_back({"DAOLibC", "C", "", {"x", "y"}});
    CHECK_FALSE(AddSections(OptionsMovie(), std::vector<Section>{section}, shown));
    CHECK(Shown(shown) == 0);
}

TEST_CASE("the Video tab's free rows are its right column's, from the bars' height down", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const auto slots = FreeSlots(OptionsMovie(), Tab::Video);
    REQUIRE(slots);
    // The tab as a section finds it, its own controls all in the left
    // column: none there, and in the right clip a row every 35 from 30
    // above its origin, where a heading's bar is level with "Screen"'s, to
    // the Game tab's last row on the screen. Fourteen: a heading and twelve.
    REQUIRE(slots->size() == 14);
    CHECK(slots->front() == Slot{35, -30, 1});
    CHECK((*slots)[1] == Slot{35, 5, 1});
    CHECK(slots->back() == Slot{35, 425, 1});
    // The Game tab's are as they were, asked for by name or not.
    CHECK(FreeSlots(OptionsMovie(), Tab::Game) == FreeSlots(OptionsMovie()));
    CHECK_FALSE(FreeSlots({}, Tab::Video));
}

TEST_CASE("a section on the Video tab is placed in its column's own clip", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    // A heading, a checkbox and a slider: at the right column's top, though
    // the left is asked for, which has no row.
    Section graphics{"Graphics", {{"DAOLibSmaa", "Extra anti-aliasing", ""}}, {{"DAOLibLook", "Look", "", {"a", "b"}}}};
    graphics.tab = Tab::Video;
    graphics.column = 0;
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{graphics}, shown);
    REQUIRE(patched);
    CHECK(shown == std::vector<bool>{true});

    // The heading's text 24 right and 13 down of its bar, the bar 38 left
    // of the column's checkboxes and 6 down in the first row.
    const Slot top{35, -30, 1};
    CHECK(HeadingInstance(top, Tab::Video) != HeadingInstance(top, Tab::Game));
    const auto heading = gfx::FindInstance(*patched, HeadingInstance(top, Tab::Video));
    const auto box = gfx::FindInstance(*patched, ControlInstance("DAOLibSmaa"));
    const auto slider = gfx::FindInstance(*patched, ControlInstance("DAOLibLook"));
    REQUIRE((heading && box && slider));
    CHECK(heading->sprite == kVideoRight);
    CHECK(heading->character == kHeadingText);
    CHECK(heading->x == 35 - 38 + 24);
    CHECK(heading->y == -30 + 6 + 13);
    CHECK(box->sprite == kVideoRight);
    CHECK(box->character == kCheckbox);
    CHECK(box->x == 35);
    CHECK(box->y == 40);
    // The slider 37 right of the column's checkboxes, as the tab's own.
    CHECK(slider->sprite == kVideoRight);
    CHECK(slider->character == kSlider);
    CHECK(slider->x == 72);
    CHECK(slider->y == 75);
    CHECK(Holds(*patched, gfx::Push({gfx::Text("MakeSliderPickerInit")})));

    // Nine rows of the column are left, under the section, and the Game tab
    // is untouched.
    const auto left = FreeSlots(*patched, Tab::Video);
    REQUIRE(left);
    CHECK(left->size() == 9);
    CHECK(left->front() == Slot{35, 145, 1});
    CHECK(FreeSlots(*patched, Tab::Game) == FreeSlots(movie, Tab::Game));
}

TEST_CASE("a section's sliders stand 41 apart, as the game's own, and take the rows they reach into", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    const auto sliders = [](std::size_t count) {
        Section section{"Look", {}};
        for (std::size_t i = 0; i < count; ++i)
        {
            section.choices.push_back({"DAOLibS" + std::to_string(i), "S", "", {"a", "b"}});
        }
        section.tab = Tab::Video;
        return section;
    };

    // The Video tab's column, fourteen rows, holds a heading, two
    // checkboxes and eight sliders: the first in the row under the
    // checkboxes, each 41 under the last.
    Section full = sliders(8);
    full.toggles = {{"DAOLibOne", "One", ""}, {"DAOLibTwo", "Two", ""}};
    CHECK(RowsOf(full) == 14);
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{full}, shown);
    REQUIRE(patched);
    CHECK(shown == std::vector<bool>{true});
    for (std::int32_t i = 0; i < 8; ++i)
    {
        const auto slider = gfx::FindInstance(*patched, ControlInstance("DAOLibS" + std::to_string(i)));
        REQUIRE(slider);
        CHECK(slider->x == 72);
        CHECK(slider->y == 110 + (41 * i));
    }
    const auto none = FreeSlots(*patched, Tab::Video);
    REQUIRE(none);
    CHECK(none->empty());
    // A ninth has no room.
    full.choices.push_back({"DAOLibNinth", "S", "", {"a", "b"}});
    CHECK(RowsOf(full) == 15);
    CHECK_FALSE(AddSections(movie, std::vector<Section>{full}, shown));

    // Six sliders under a heading end a pixel into their tenth row. That
    // row is the section's, to another plugin's copy of the library reading
    // the movie after as to this one placing a second section now.
    CHECK(RowsOf(sliders(6)) == 10);
    const auto six = AddSections(movie, std::vector<Section>{sliders(6)}, shown);
    REQUIRE(six);
    const auto under = FreeSlots(*six, Tab::Video);
    REQUIRE(under);
    REQUIRE(under->size() == 4);
    CHECK(under->front() == Slot{35, 320, 1});
    Section second{"Second", {{"DAOLibUnder", "Under", ""}}};
    second.tab = Tab::Video;
    const auto both = AddSections(movie, std::vector<Section>{sliders(6), second}, shown);
    REQUIRE(both);
    CHECK(Shown(shown) == 2);
    CHECK(gfx::FindInstance(*both, ControlInstance("DAOLibUnder"))->y == 320 + 70);
}

TEST_CASE("a section on the Video tab has the tab's own controls put in its left column, closer", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    Section graphics{"Graphics", {{"DAOLibSmaa", "Extra anti-aliasing", ""}}};
    graphics.tab = Tab::Video;
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{graphics}, shown);
    REQUIRE(patched);

    // Each of the tab's own where the layout has it, in the left clip: its
    // own at the depths they had, the dropdown still deepest, and the right
    // column's at depths the left had free under the dropdown's.
    struct Where
    {
        std::string_view name;
        int x;
        int y;
    };
    for (const Where &where :
         {Where{"OptResolution_mc", 9, -5}, Where{"OptFullscreen_mc", 3, 58}, Where{"OptGamma_mc", 44, 91},
          Where{"OptVSync_mc", 3, 184}, Where{"OptGraphicsDetail_mc", 44, 280}, Where{"OptAntialiasing_mc", 44, 321},
          Where{"OptTextureQuality_mc", 44, 362}, Where{"OptFBFX_mc", 3, 398}})
    {
        CAPTURE(where.name);
        const auto found = gfx::FindInstance(*patched, where.name);
        REQUIRE(found);
        CHECK(found->sprite == kVideoLeft);
        CHECK(found->x == where.x);
        CHECK(found->y == where.y);
        CHECK(found->depth <= 35);
    }
    CHECK(gfx::FindInstance(*patched, "OptResolution_mc")->depth == 35);
    CHECK(gfx::FindInstance(*patched, "OptVSync_mc")->depth == 28);
    const auto left = gfx::InstancesOf(*patched, kVideoLeft);
    // Nine things, each at a depth of its own: the eight and the gamma's
    // picture, which went up with its slider, 62.
    REQUIRE(left.size() == 9);
    for (const gfx::Instance &one : left)
    {
        CHECK(std::ranges::count(left, one.depth, &gfx::Instance::depth) == 1);
        if (one.character == kGammaPicture)
        {
            CHECK(one.y == -62);
            CHECK(one.depth == 34);
        }
    }
    // The right clip has the section alone, and the "Detail" bar is under
    // Vertical sync on the main timeline: 15 left of the checkbox and 36
    // under it, the left clip being at (82, 205) of the stage.
    for (const gfx::Instance &one : gfx::InstancesOf(*patched, kVideoRight))
    {
        CHECK(one.character != kSlider);
        CHECK(one.name != "OptFBFX_mc");
    }
    CHECK(Holds(*patched, DetailBarAt(70, 425)));
    CHECK_FALSE(Holds(*patched, DetailBarAt(524, 131)));

    // A second plugin's copy of the library finds the tab laid out, leaves
    // it so, and puts its section under the first's.
    Section more{"More", {{"DAOLibMore", "More", ""}}};
    more.tab = Tab::Video;
    const auto twice = AddSections(*patched, std::vector<Section>{more}, shown);
    REQUIRE(twice);
    CHECK(gfx::InstancesOf(*twice, kVideoLeft).size() == 9);
    CHECK(gfx::FindInstance(*twice, "OptFBFX_mc")->y == 398);
    CHECK(gfx::FindInstance(*twice, "OptFBFX_mc")->depth == gfx::FindInstance(*patched, "OptFBFX_mc")->depth);
    const auto second = gfx::FindInstance(*twice, ControlInstance("DAOLibMore"));
    REQUIRE(second);
    CHECK(second->sprite == kVideoRight);
    CHECK(second->y == 145);
}

TEST_CASE("the Video tab is as the game has it with no section for it, or where it is not as read", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    std::vector<bool> shown;
    const auto game = AddSections(movie, std::vector<Section>{{"Game", {{"DAOLibG", "G", ""}}}}, shown);
    REQUIRE(game);
    CHECK(gfx::FindInstance(*game, "OptFBFX_mc")->sprite == kVideoRight);
    CHECK(gfx::FindInstance(*game, "OptVSync_mc")->y == 302);
    CHECK(Holds(*game, DetailBarAt(524, 131)));

    // A tab without one of its controls is left alone, and its section out.
    const auto vsync = gfx::FindInstance(movie, "OptVSync_mc");
    REQUIRE(vsync);
    const auto strange = gfx::RemovePlaced(movie, vsync->sprite, vsync->depth);
    REQUIRE(strange);
    Section graphics{"Graphics", {{"DAOLibSmaa", "Extra anti-aliasing", ""}}};
    graphics.tab = Tab::Video;
    CHECK_FALSE(AddSections(*strange, std::vector<Section>{graphics}, shown));
    CHECK(shown == std::vector<bool>{false});
    CHECK_FALSE(FreeSlots(*strange, Tab::Video));
}

TEST_CASE("a tab's sections are not kept out by another tab's want of room", "[options]")
{
    using namespace dao::core::options_movie; // NOLINT(google-build-using-namespace)
    const Bytes movie = OptionsMovie();
    // Thirteen checkboxes are fifteen rows, more than any column: on the
    // Game tab it keeps the Game section after it out, and not the Video one.
    Section huge{"Huge", {}};
    for (int i = 0; i < 13; ++i)
    {
        huge.toggles.push_back({"DAOLibHuge" + std::to_string(i), "H", ""});
    }
    Section video{"Video", {{"DAOLibV", "V", ""}}};
    video.tab = Tab::Video;
    const Section game{"Game", {{"DAOLibG", "G", ""}}};
    std::vector<bool> shown;
    const auto patched = AddSections(movie, std::vector<Section>{huge, video, game}, shown);
    REQUIRE(patched);
    CHECK(shown == std::vector<bool>{false, true, false});
    CHECK(gfx::FindInstance(*patched, ControlInstance("DAOLibV")));
    CHECK_FALSE(gfx::FindInstance(*patched, ControlInstance("DAOLibG")));

    // And the other way: too much for the Video tab, and the Game section
    // after it goes in. One patch then places in the clips of both tabs.
    huge.tab = Tab::Video;
    const auto both = AddSections(movie, std::vector<Section>{video, huge, game}, shown);
    REQUIRE(both);
    CHECK(shown == std::vector<bool>{true, false, true});
    const auto onVideo = gfx::FindInstance(*both, ControlInstance("DAOLibV"));
    const auto onGame = gfx::FindInstance(*both, ControlInstance("DAOLibG"));
    REQUIRE((onVideo && onGame));
    CHECK(onVideo->sprite == kVideoRight);
    CHECK(onGame->sprite == kGameTab);
    // Each set up, the two tabs' in the one function.
    const auto setUp = [](std::string_view option, std::string_view label) {
        return gfx::Push({gfx::Register(2), gfx::Text(ControlInstance(option) + "_init"), gfx::Text(""),
                          gfx::Text(label), gfx::Text(option), gfx::Integer(3), gfx::Text("OptionsUtils")});
    };
    CHECK(Holds(*both, setUp("DAOLibV", "V")));
    CHECK(Holds(*both, setUp("DAOLibG", "G")));
}
