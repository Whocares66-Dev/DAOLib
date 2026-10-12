#include "dao/core/OptionsMovie.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

namespace dao::core::options_movie
{

namespace
{

constexpr std::int32_t kRow = 35;           // pixels from one row to the next
constexpr std::int32_t kControlHeight = 30; // what a checkbox takes of its row
constexpr std::int32_t kChoiceHeight = (static_cast<std::int32_t>(kChoiceRows) * kRow) - (kRow - kControlHeight);
constexpr std::uint16_t kDepthStep = 6;    // as the designer's own placements are apart
constexpr std::int32_t kSetUpRegister = 2; // in SetupOptionsMenuInitFunctions: where the set-up functions are kept

// A heading: the bar, in its two rows, and the text in the bar.
constexpr std::int32_t kBarHeight = 48;
constexpr std::int32_t kBarDown = 6; // from its first row's top
constexpr std::int32_t kTextRight = 24;
constexpr std::int32_t kTextDown = 13;
constexpr std::int32_t kColumnReach = 45; // either side of a column's checkboxes, what is placed is the column's

// A column of a tab: the last of the game's controls in it, a checkbox, under
// which its free rows are, and how what a section places stands to the
// column's checkboxes.
struct ColumnLayout
{
    std::string_view last;
    std::int32_t lastRow = 0;       // the lowest a row can start in the column's clip, the screen's buttons below
    std::int32_t barLeft = 0;       // a heading's bar, left of the checkboxes
    std::int32_t sliderRight = 0;   // a slider, right of them, as the tab's own stand
    std::int32_t dropdownRight = 0; // and a dropdown
};
using TabLayout = std::array<ColumnLayout, 2>;

constexpr TabLayout kGameTab = {{{"OptLootShimmer_mc", 403, 15, 40, 4}, {"OptShowTutorials_mc", 403, 37, 40, 4}}};
// The Video tab as LayOutVideo leaves it: the left column full to its last
// control, no row under it, and the right column a clip with nothing of the
// game's, its rows from kVideoRightTop to the Game tab's last row on the
// screen.
constexpr TabLayout kVideoTab = {{{"OptFBFX_mc", 0, 15, 41, 6}, {"", 448, 38, 37, 4}}};
constexpr std::int32_t kVideoRightX = 35;    // the right column's checkboxes, where the game's own stood
constexpr std::int32_t kVideoRightTop = -30; // its first row: a heading there has its bar level with "Screen"'s

// Where LayOutVideo puts one of the tab's own controls, in the left
// column's clip.
struct Moved
{
    std::string_view name;
    std::int32_t x = 0;
    std::int32_t y = 0;
};
// The left column's own, set as close as the Game tab's: the dropdown where
// the Game tab's first stands under its bar, the checkbox 63 under it, the
// gamma's slider 33 under that, and Vertical sync 8 under the gamma's
// picture, which is 48 to 85 under its slider.
constexpr std::array<Moved, 4> kVideoScreen = {
    {{"OptResolution_mc", 9, -5}, {"OptFullscreen_mc", 3, 58}, {"OptGamma_mc", 44, 91}, {"OptVSync_mc", 3, 184}}};
// The right column's, under them: the first slider 60 under the bar, the
// sliders 41 apart, as the Controls tab's are, and the checkbox 36 under the
// last, in the Game tab's lowest row on the screen, the panel's corner under
// it.
constexpr std::array<Moved, 4> kVideoDetail = {{{"OptGraphicsDetail_mc", 44, 280},
                                                {"OptAntialiasing_mc", 44, 321},
                                                {"OptTextureQuality_mc", 44, 362},
                                                {"OptFBFX_mc", 3, 398}}};
constexpr const Moved &kGamma = kVideoScreen[2];
constexpr const Moved &kVSync = kVideoScreen[3];
constexpr std::string_view kDetailText = "txt_Detail"; // the text of the bar the main timeline places
// The left column's clip on the stage, and a bar under a checkbox of it: a
// row and a pixel lower, as the Game tab's bars stand to the rows over them.
constexpr std::int32_t kVideoLeftX = 82;
constexpr std::int32_t kVideoLeftY = 205;
constexpr std::int32_t kBarUnder = kRow + 1;

constexpr const TabLayout &LayoutOf(Tab tab)
{
    return tab == Tab::Video ? kVideoTab : kGameTab;
}

constexpr std::string_view kFirstSetUp = "OptAutoPause_mc_init";
constexpr std::string_view kHeadingText = "txt_Game";
constexpr std::string_view kSlider = "OptTextureQuality_mc"; // the Video tab's: the symbol a choice is
constexpr std::string_view kPicker = "OptDifficulty_mc";     // the Game tab's: the symbol a dropdown is

// r2.<instance>_init = OptionsUtils.MakeOptionToggleInit(option, label,
// tooltip); the arguments pushed last first, as compiled script has them.
gfx::Bytes ToggleSetUp(const Toggle &toggle)
{
    using namespace gfx;
    return Join({Push({Register(kSetUpRegister), Text(ControlInstance(toggle.option) + "_init"), Text(toggle.tooltip),
                       Text(toggle.label), Text(toggle.option), Integer(3), Text("OptionsUtils")}),
                 Do(Op::GetVariable), Push({Text("MakeOptionToggleInit")}), Do(Op::CallMethod), Do(Op::SetMember)});
}

// new Array(values...): a choice's values pushed last first and then their
// count, as InitArray takes them.
gfx::Bytes ValuesArray(const Choice &choice)
{
    using namespace gfx;
    std::vector<Value> values;
    for (auto it = choice.values.rbegin(); it != choice.values.rend(); ++it)
    {
        values.push_back(Text(*it));
    }
    values.push_back(Integer(static_cast<std::int32_t>(choice.values.size())));
    return Join({Push(values), Do(Op::InitArray)});
}

// A slider's: r2.<instance>_init = OptionsUtils.MakeSliderPickerInit(option,
// label, tooltip, new Array(values...)); the array made first.
gfx::Bytes SliderSetUp(const Choice &choice)
{
    using namespace gfx;
    return Join(
        {Push({Register(kSetUpRegister), Text(ControlInstance(choice.option) + "_init")}), ValuesArray(choice),
         Push({Text(choice.tooltip), Text(choice.label), Text(choice.option), Integer(4), Text("OptionsUtils")}),
         Do(Op::GetVariable), Push({Text("MakeSliderPickerInit")}), Do(Op::CallMethod), Do(Op::SetMember)});
}

// A dropdown's: r2.<instance>_init = OptionsUtils.MakePickerInit(option,
// label, tooltip, new Array(values...), null, OptionsMenu.PickEntryCallBack):
// no tooltips for the entries, and the movie's own callback, which sends the
// entry picked as a slider sends its stop.
gfx::Bytes DropdownSetUp(const Choice &choice)
{
    using namespace gfx;
    return Join(
        {Push({Register(kSetUpRegister), Text(ControlInstance(choice.option) + "_init"), Text("OptionsMenu")}),
         Do(Op::GetVariable), Push({Text("PickEntryCallBack")}), Do(Op::GetMember), Push({Null()}), ValuesArray(choice),
         Push({Text(choice.tooltip), Text(choice.label), Text(choice.option), Integer(6), Text("OptionsUtils")}),
         Do(Op::GetVariable), Push({Text("MakePickerInit")}), Do(Op::CallMethod), Do(Op::SetMember)});
}

// r2.<instance>_init = function () { this.SetText(title); };
gfx::Bytes HeadingSetUp(std::string_view instance, std::string_view title)
{
    using namespace gfx;
    const Bytes setText =
        Join({Push({Text(title), Integer(1), Register(1), Text("SetText")}), Do(Op::CallMethod), Do(Op::Pop)});
    return Join({Push({Register(kSetUpRegister), Text(std::string(instance) + "_init")}), Function(setText),
                 Do(Op::SetMember)});
}

// The game's heading: the text clip it exports, and the bar the same clip
// of the movie places under it.
struct Heading
{
    std::uint16_t bar = 0;
    std::uint16_t text = 0;
};

std::optional<Heading> FindHeading(std::span<const std::byte> movie)
{
    const auto text = gfx::ExportedCharacter(movie, kHeadingText);
    const auto clip = text ? gfx::SpritePlacing(movie, *text) : std::nullopt;
    if (!clip)
    {
        return std::nullopt;
    }
    for (const gfx::Instance &instance : gfx::InstancesOf(movie, *clip))
    {
        if (instance.character != *text)
        {
            return Heading{instance.character, *text};
        }
    }
    return std::nullopt;
}

// The symbol a control like the movie's own `instance` is an instance of.
std::optional<std::uint16_t> SymbolOf(std::span<const std::byte> movie, std::string_view instance)
{
    const auto found = gfx::FindInstance(movie, instance);
    if (!found)
    {
        return std::nullopt;
    }
    return found->character;
}

// What a section is made of, where the movie has it: the game's heading,
// its slider with named stops and its dropdown.
struct Symbols
{
    std::optional<Heading> heading;
    std::optional<std::uint16_t> slider;
    std::optional<std::uint16_t> picker;
};

Symbols SymbolsOf(std::span<const std::byte> movie)
{
    return {FindHeading(movie), SymbolOf(movie, kSlider), SymbolOf(movie, kPicker)};
}

const Moved *PlaceOf(std::span<const Moved> moves, std::string_view name)
{
    const auto found = std::ranges::find(moves, name, &Moved::name);
    return found != moves.end() ? &*found : nullptr;
}

// The Video tab laid out for sections (the user, 2026-10-11, DAOLib #82:
// "reduce the spacing, move detail to the left below screen, then have
// graphics take the whole right column"): the left column's own controls
// set closer, the "Detail" bar and the right column's controls under them,
// and the right column's clip left with nothing of the game's. The movie as
// it is if another copy of the library has laid it out already; nothing if
// the tab is not as read.
std::optional<gfx::Bytes> LayOutVideo(std::span<const std::byte> movie)
{
    const auto vsync = gfx::FindInstance(movie, kVSync.name);
    const auto gamma = gfx::FindInstance(movie, kGamma.name);
    const auto fbfx = gfx::FindInstance(movie, kVideoDetail.back().name);
    const auto text = gfx::ExportedCharacter(movie, kDetailText);
    const auto bar = text ? gfx::SpritePlacing(movie, *text) : std::nullopt;
    if (!vsync || !gamma || !fbfx || !bar)
    {
        return std::nullopt;
    }
    if (fbfx->sprite == vsync->sprite)
    {
        return gfx::Bytes(movie.begin(), movie.end());
    }
    // What the left clip places again: its own higher, each at its depth, and
    // with the gamma's slider its picture, a shape at the clip's origin drawn
    // where it is seen. Then the right clip's, at depths the left has free:
    // from the lowest, so under the dropdown, whose open list covers them.
    std::vector<gfx::Placement> placements;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> taken; // a clip, and a depth of it
    std::vector<std::uint16_t> used;
    for (const gfx::Instance &own : gfx::InstancesOf(movie, vsync->sprite))
    {
        used.push_back(own.depth);
        const Moved *to = PlaceOf(kVideoScreen, own.name);
        const bool picture = own.name.empty() && own.x == 0 && own.y == 0;
        if (to != nullptr || picture)
        {
            taken.emplace_back(own.sprite, own.depth);
            placements.push_back({own.name, own.character, own.depth, to != nullptr ? to->x : own.x,
                                  to != nullptr ? to->y : own.y + kGamma.y - gamma->y});
        }
    }
    std::uint16_t depth = 1;
    for (const gfx::Instance &theirs : gfx::InstancesOf(movie, fbfx->sprite))
    {
        if (const Moved *to = PlaceOf(kVideoDetail, theirs.name))
        {
            while (std::ranges::find(used, depth) != used.end())
            {
                ++depth;
            }
            used.push_back(depth);
            taken.emplace_back(theirs.sprite, theirs.depth);
            placements.push_back({theirs.name, theirs.character, depth, to->x, to->y});
        }
    }
    const auto named = std::ranges::count_if(placements, [](const gfx::Placement &p) { return !p.name.empty(); });
    if (static_cast<std::size_t>(named) != kVideoScreen.size() + kVideoDetail.size())
    {
        return std::nullopt;
    }
    std::optional<gfx::Bytes> out = gfx::Bytes(movie.begin(), movie.end());
    for (const auto &[clip, at] : taken)
    {
        out = out ? gfx::RemovePlaced(*out, clip, at) : std::nullopt;
    }
    out = out ? gfx::PlaceInSprite(*out, vsync->sprite, placements) : std::nullopt;
    return out ? gfx::MoveOnTimeline(*out, *bar, kVideoLeftX + kVSync.x - kVideoTab[0].barLeft,
                                     kVideoLeftY + kVSync.y + kBarUnder)
               : std::nullopt;
}

// The Video tab's columns once it is laid out: the left by its last control,
// and the right, which has none of the game's, as the other clip of the
// tab's, a checkbox's row above its first.
std::optional<std::array<gfx::Instance, 2>> VideoColumns(std::span<const std::byte> movie)
{
    auto left = gfx::FindInstance(movie, kVideoTab[0].last);
    const auto vsync = gfx::FindInstance(movie, kVSync.name);
    const auto tab =
        left && vsync && left->sprite == vsync->sprite ? gfx::SpritePlacing(movie, left->sprite) : std::nullopt;
    if (!tab)
    {
        return std::nullopt;
    }
    for (const gfx::Instance &clip : gfx::InstancesOf(movie, *tab))
    {
        if (clip.character == left->sprite)
        {
            continue;
        }
        gfx::Instance right = *left; // a checkbox, as a section's are
        right.name.clear();
        right.sprite = clip.character;
        right.x = kVideoRightX;
        right.y = kVideoRightTop - kRow;
        right.deepest = 0;
        for (const gfx::Instance &placed : gfx::InstancesOf(movie, right.sprite))
        {
            right.deepest = placed.deepest;
        }
        return std::array{std::move(*left), std::move(right)};
    }
    return std::nullopt;
}

// The last of the game's controls in each column of a tab, with the clip
// each is in; the Video tab's as VideoColumns has them. Nothing if the movie
// has not both.
std::optional<std::array<gfx::Instance, 2>> ColumnsOf(std::span<const std::byte> movie, Tab tab)
{
    if (tab == Tab::Video)
    {
        return VideoColumns(movie);
    }
    const TabLayout &layout = LayoutOf(tab);
    auto left = gfx::FindInstance(movie, layout[0].last);
    auto right = gfx::FindInstance(movie, layout[1].last);
    if (!left || !right)
    {
        return std::nullopt;
    }
    return std::array{std::move(*left), std::move(*right)};
}

// Whether something a clip places reaches into the row at `y` of the column
// whose checkboxes stand at `x`: a bar is higher than a row, and lies in
// two, as a choice does.
bool Taken(const std::vector<gfx::Instance> &placed, const Symbols &symbols, std::int32_t x, std::int32_t y)
{
    return std::ranges::any_of(placed, [&](const gfx::Instance &instance) {
        std::int32_t height = kControlHeight;
        if (symbols.heading && instance.character == symbols.heading->bar)
        {
            height = kBarHeight;
        }
        else if (instance.character == symbols.slider || instance.character == symbols.picker)
        {
            height = kChoiceHeight;
        }
        return instance.x >= x - kColumnReach && instance.x <= x + kColumnReach && instance.y < y + kRow &&
               instance.y + height > y;
    });
}

} // namespace

std::string ControlInstance(std::string_view option)
{
    return "Opt" + std::string(option) + "_mc";
}

std::string HeadingInstance(const Slot &slot, Tab tab)
{
    // The Game tab's as they were before a section had a tab: another
    // plugin's older copy of the library names its headings so.
    const std::string_view on = tab == Tab::Video ? "DAOLibVideoSectionAt" : "DAOLibSectionAt";
    return std::string(on) + std::to_string(slot.x) + "x" + std::to_string(slot.y) + "_mc";
}

std::optional<std::vector<Slot>> FreeSlots(std::span<const std::byte> movie, Tab tab)
{
    // The Video tab's rows are the ones it has once laid out.
    const std::optional<gfx::Bytes> laid = tab == Tab::Video ? LayOutVideo(movie) : std::nullopt;
    if (tab == Tab::Video && !laid)
    {
        return std::nullopt;
    }
    movie = laid ? std::span<const std::byte>(*laid) : movie;
    const auto columns = ColumnsOf(movie, tab);
    if (!columns)
    {
        return std::nullopt;
    }
    const Symbols symbols = SymbolsOf(movie);
    std::vector<Slot> slots;
    for (std::size_t column = 0; column < columns->size(); ++column)
    {
        const gfx::Instance &last = (*columns)[column];
        const std::vector<gfx::Instance> placed = gfx::InstancesOf(movie, last.sprite);
        for (std::int32_t y = last.y + kRow; y <= LayoutOf(tab)[column].lastRow; y += kRow)
        {
            if (!Taken(placed, symbols, last.x, y))
            {
                slots.push_back({last.x, y, static_cast<std::int32_t>(column)});
            }
        }
    }
    return slots;
}

namespace
{

// The first of `rows` free slots one under the other in one column, in
// `column` if it is not kEitherColumn; the slots' count if there is no such
// run.
std::size_t FirstRunIn(const std::vector<Slot> &slots, std::size_t rows, std::int32_t column)
{
    for (std::size_t i = 0; i + rows <= slots.size(); ++i)
    {
        bool together = column == kEitherColumn || slots[i].column == column;
        for (std::size_t j = 1; j < rows && together; ++j)
        {
            const Slot &above = slots[i + j - 1];
            const Slot &below = slots[i + j];
            together = below.column == above.column && below.y == above.y + kRow;
        }
        if (together)
        {
            return i;
        }
    }
    return slots.size();
}

// The rows for a section: in the column it asks for if that has them, in
// the first that has otherwise. A section is better in the other column
// than left out.
std::size_t FirstRun(const std::vector<Slot> &slots, std::size_t rows, std::int32_t column)
{
    const std::size_t wanted = FirstRunIn(slots, rows, column);
    return wanted != slots.size() ? wanted : FirstRunIn(slots, rows, kEitherColumn);
}

// Gives what is placed its depths: above `deepest`, in the order placed, but
// for the dropdowns, which come last, the first placed last of all. A
// dropdown's open list is drawn at the dropdown's depth, so it then covers
// whatever these sections put under it, another dropdown too.
void GiveDepths(std::vector<gfx::Placement> &placements, std::uint16_t deepest, std::optional<std::uint16_t> picker)
{
    std::ranges::reverse(std::ranges::stable_partition(
        placements, [&](const gfx::Placement &placed) { return placed.character != picker; }));
    for (gfx::Placement &placed : placements)
    {
        deepest = static_cast<std::uint16_t>(deepest + kDepthStep);
        placed.depth = deepest;
    }
}

// What the sections place in one clip of the movie.
struct ClipPlacements
{
    std::uint16_t sprite = 0;
    std::uint16_t deepest = 0;              // the highest depth the clip uses as the game has it
    std::vector<gfx::Placement> placements; // in the order placed; GiveDepths has their depths
};

// Everything the sections add to the movie: what is placed, a clip at a
// time, and the set-up functions of all of it.
struct Additions
{
    std::vector<ClipPlacements> clips;
    gfx::Bytes setUps;

    std::vector<gfx::Placement> &In(const gfx::Instance &column)
    {
        auto clip = std::ranges::find(clips, column.sprite, &ClipPlacements::sprite);
        if (clip == clips.end())
        {
            clips.push_back({column.sprite, column.deepest, {}});
            clip = std::prev(clips.end());
        }
        return clip->placements;
    }

    void SetUp(const gfx::Bytes &code)
    {
        setUps.insert(setUps.end(), code.begin(), code.end());
    }
};

// How many rows a section takes: its heading, a row a checkbox and two a
// choice.
std::size_t RowsOf(const Section &section)
{
    return kHeadingRows + section.toggles.size() + (kChoiceRows * section.choices.size());
}

// Places a section in `rows`, free rows one under the other in one column
// of its tab, as many as RowsOf says: the game's bar and heading text, then
// its checkboxes, which are what the column's last control is, then its
// choices. Nothing in a movie without the game's heading.
void PlaceSection(const Section &section, std::span<const Slot> rows, const std::array<gfx::Instance, 2> &columns,
                  const Symbols &symbols, Additions &additions)
{
    if (!symbols.heading)
    {
        return;
    }
    const Heading &gameHeading = *symbols.heading;
    const Slot &top = rows.front();
    const auto side = static_cast<std::size_t>(top.column);
    const ColumnLayout &layout = LayoutOf(section.tab)[side];
    std::vector<gfx::Placement> &placements = additions.In(columns[side]);
    const auto place = [&](std::string name, std::uint16_t character, std::int32_t x, std::int32_t y) {
        placements.push_back({std::move(name), character, 0, x, y});
    };

    const std::int32_t barX = top.x - layout.barLeft;
    const std::int32_t barY = top.y + kBarDown;
    const std::string heading = HeadingInstance(top, section.tab);
    place({}, gameHeading.bar, barX, barY);
    place(heading, gameHeading.text, barX + kTextRight, barY + kTextDown);
    additions.SetUp(HeadingSetUp(heading, section.title));
    for (std::size_t i = 0; i < section.toggles.size(); ++i)
    {
        const Slot &row = rows[kHeadingRows + i];
        place(ControlInstance(section.toggles[i].option), columns[side].character, row.x, row.y);
        additions.SetUp(ToggleSetUp(section.toggles[i]));
    }
    for (std::size_t i = 0; i < section.choices.size(); ++i)
    {
        // A choice is the game's slider with named stops or its dropdown,
        // in two rows.
        const Choice &choice = section.choices[i];
        const Slot &row = rows[kHeadingRows + section.toggles.size() + (kChoiceRows * i)];
        const std::int32_t right = choice.dropdown ? layout.dropdownRight : layout.sliderRight;
        const auto symbol = choice.dropdown ? symbols.picker : symbols.slider;
        place(ControlInstance(choice.option), symbol.value_or(0), row.x + right, row.y);
        additions.SetUp(choice.dropdown ? DropdownSetUp(choice) : SliderSetUp(choice));
    }
}

// Places a tab's sections, in the order given, in the rows the tab has
// free, and says in `shown` which went in. The first left out keeps those
// after it out.
void PlaceTab(std::span<const std::byte> movie, Tab tab, std::span<const Section> sections, const Symbols &symbols,
              Additions &additions, std::vector<bool> &shown)
{
    const auto columns = ColumnsOf(movie, tab);
    auto slots = FreeSlots(movie, tab);
    if (!columns || !slots)
    {
        return;
    }
    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        const Section &section = sections[i];
        if (section.tab != tab)
        {
            continue;
        }
        // The first rows free one under the other in one column, enough
        // for the heading and every control; and each choice a symbol the
        // movie has.
        const std::size_t rows = RowsOf(section);
        const std::size_t first = FirstRun(*slots, rows, section.column);
        const bool choicesFit = std::ranges::all_of(section.choices, [&](const Choice &choice) {
            return (choice.dropdown ? symbols.picker : symbols.slider).has_value();
        });
        if ((section.toggles.empty() && section.choices.empty()) || !choicesFit || first == slots->size())
        {
            return;
        }
        const auto begin = slots->begin() + static_cast<std::ptrdiff_t>(first);
        const auto end = begin + static_cast<std::ptrdiff_t>(rows);
        PlaceSection(section, {begin, end}, *columns, symbols, additions);
        slots->erase(begin, end);
        shown[i] = true;
    }
}

} // namespace

std::optional<gfx::Bytes> AddSections(std::span<const std::byte> movie, std::span<const Section> sections,
                                      std::vector<bool> &shown)
{
    shown.assign(sections.size(), false);
    const Symbols symbols = SymbolsOf(movie);
    if (!symbols.heading)
    {
        return std::nullopt;
    }
    // A section on the Video tab has the tab laid out for it first. Where
    // that cannot be, the tab has no columns and its sections are left out.
    const bool video = std::ranges::any_of(sections, [](const Section &s) { return s.tab == Tab::Video; });
    const std::optional<gfx::Bytes> laid = video ? LayOutVideo(movie) : std::nullopt;
    movie = laid ? std::span<const std::byte>(*laid) : movie;
    Additions additions;
    for (const Tab tab : {Tab::Game, Tab::Video})
    {
        PlaceTab(movie, tab, sections, symbols, additions, shown);
    }
    if (additions.clips.empty())
    {
        return std::nullopt;
    }
    std::optional<gfx::Bytes> patched = gfx::Bytes(movie.begin(), movie.end());
    for (ClipPlacements &clip : additions.clips)
    {
        GiveDepths(clip.placements, clip.deepest, symbols.picker);
        patched = patched ? gfx::PlaceInSprite(*patched, clip.sprite, clip.placements) : std::nullopt;
    }
    patched = patched ? gfx::InsertBefore(*patched, {kSetUpRegister, std::string(kFirstSetUp)}, additions.setUps)
                      : std::nullopt;
    if (!patched)
    {
        shown.assign(sections.size(), false);
    }
    return patched;
}

std::optional<gfx::Bytes> ListKeys(std::span<const std::byte> movie, std::span<const KeySection> sections)
{
    gfx::Bytes current(movie.begin(), movie.end());
    std::vector<gfx::Call> fresh; // sections whose headings are still to make
    bool changed = false;
    for (const KeySection &section : sections)
    {
        std::vector<gfx::Call> rows;
        rows.reserve(section.actions.size());
        for (const std::string &action : section.actions)
        {
            rows.push_back({"AddKeyBinding", action});
        }
        if (rows.empty())
        {
            continue;
        }
        if (auto under = gfx::InsertAfter(current, {"AddHeader", section.title}, gfx::Statements(rows)))
        {
            current = std::move(*under);
            changed = true;
            continue;
        }
        fresh.push_back({"AddHeader", section.title});
        fresh.insert(fresh.end(), rows.begin(), rows.end());
    }
    if (!fresh.empty())
    {
        auto listed = gfx::InsertAfter(current, {"AddKeyBinding", "HideMainInterface"}, gfx::Statements(fresh));
        if (!listed)
        {
            return std::nullopt;
        }
        current = std::move(*listed);
        changed = true;
    }
    if (!changed)
    {
        return std::nullopt;
    }
    return current;
}

} // namespace dao::core::options_movie
