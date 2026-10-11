#include "dao/core/OptionsMovie.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace dao::core::options_movie
{

namespace
{

constexpr std::int32_t kRow = 35;           // pixels from one row to the next
constexpr std::int32_t kLastRow = 403;      // the lowest a row can start, the screen's buttons below
constexpr std::int32_t kControlHeight = 30; // what a checkbox takes of its row
// A choice: the slider, right of the column's checkboxes, in two rows.
constexpr std::int32_t kChoiceRight = 40;
constexpr std::int32_t kDropdownRight = 4; // a dropdown, as the tab's own stand
constexpr std::int32_t kChoiceHeight = (static_cast<std::int32_t>(kChoiceRows) * kRow) - (kRow - kControlHeight);
constexpr std::uint16_t kDepthStep = 6;    // as the designer's own placements are apart
constexpr std::int32_t kSetUpRegister = 2; // in SetupOptionsMenuInitFunctions: where the set-up functions are kept

// A heading: the bar, in its two rows, and the text in the bar.
constexpr std::int32_t kBarHeight = 48;
constexpr std::int32_t kBarDown = 6;                               // from its first row's top
constexpr std::array<std::int32_t, 2> kBarLeftOfColumn = {15, 37}; // of the column's checkboxes
constexpr std::int32_t kTextRight = 24;
constexpr std::int32_t kTextDown = 13;
constexpr std::int32_t kColumnReach = 45; // either side of a column's checkboxes, what is placed is the column's

constexpr std::string_view kLastLeft = "OptLootShimmer_mc";
constexpr std::string_view kLastRight = "OptShowTutorials_mc";
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
gfx::Bytes HeadingSetUp(const Slot &slot, std::string_view title)
{
    using namespace gfx;
    const Bytes setText =
        Join({Push({Text(title), Integer(1), Register(1), Text("SetText")}), Do(Op::CallMethod), Do(Op::Pop)});
    return Join({Push({Register(kSetUpRegister), Text(HeadingInstance(slot) + "_init")}), Function(setText),
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

} // namespace

std::string ControlInstance(std::string_view option)
{
    return "Opt" + std::string(option) + "_mc";
}

std::string HeadingInstance(const Slot &slot)
{
    return "DAOLibSectionAt" + std::to_string(slot.x) + "x" + std::to_string(slot.y) + "_mc";
}

std::optional<std::vector<Slot>> FreeSlots(std::span<const std::byte> movie)
{
    const auto left = gfx::FindInstance(movie, kLastLeft);
    const auto right = gfx::FindInstance(movie, kLastRight);
    if (!left || !right || left->sprite != right->sprite)
    {
        return std::nullopt;
    }
    const std::vector<gfx::Instance> placed = gfx::InstancesOf(movie, left->sprite);
    const auto heading = FindHeading(movie);
    const auto slider = gfx::FindInstance(movie, kSlider);
    const auto picker = gfx::FindInstance(movie, kPicker);
    std::vector<Slot> slots;
    std::int32_t column = 0;
    for (const auto &last : {*left, *right})
    {
        for (std::int32_t y = last.y + kRow; y <= kLastRow; y += kRow)
        {
            // Taken by whatever of the column's reaches into the row: a
            // bar is higher than a row, and lies in two.
            const bool taken = std::ranges::any_of(placed, [&](const gfx::Instance &instance) {
                std::int32_t height = kControlHeight;
                if (heading && instance.character == heading->bar)
                {
                    height = kBarHeight;
                }
                else if ((slider && instance.character == slider->character) ||
                         (picker && instance.character == picker->character))
                {
                    height = kChoiceHeight;
                }
                return instance.x >= last.x - kColumnReach && instance.x <= last.x + kColumnReach &&
                       instance.y < y + kRow && instance.y + height > y;
            });
            if (!taken)
            {
                slots.push_back({last.x, y, column});
            }
        }
        ++column;
    }
    return slots;
}

namespace
{

// The first of `rows` free slots one under the other in one column; the
// slots' count if there is no such run.
std::size_t FirstRun(const std::vector<Slot> &slots, std::size_t rows)
{
    for (std::size_t i = 0; i + rows <= slots.size(); ++i)
    {
        bool together = true;
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

} // namespace

std::optional<gfx::Bytes> AddSections(std::span<const std::byte> movie, std::span<const Section> sections,
                                      std::size_t &shown)
{
    shown = 0;
    auto slots = FreeSlots(movie);
    const auto tab = gfx::FindInstance(movie, kLastLeft);
    const auto heading = FindHeading(movie);
    const auto slider = SymbolOf(movie, kSlider);
    const auto picker = SymbolOf(movie, kPicker);
    if (!slots || !tab || !heading)
    {
        return std::nullopt;
    }
    // The symbol a choice is, if the movie has it.
    const auto symbol = [&](const Choice &choice) { return choice.dropdown ? picker : slider; };

    std::vector<gfx::Placement> placements; // in the order placed; GiveDepths has their depths
    gfx::Bytes setUps;
    const auto place = [&](std::string name, std::uint16_t character, std::int32_t x, std::int32_t y) {
        placements.push_back({std::move(name), character, 0, x, y});
    };
    for (const Section &section : sections)
    {
        // The first rows free one under the other in one column, enough
        // for the heading and every checkbox.
        const std::size_t rows = kHeadingRows + section.toggles.size() + (kChoiceRows * section.choices.size());
        const std::size_t first = FirstRun(*slots, rows);
        const bool choicesFit =
            std::ranges::all_of(section.choices, [&](const Choice &choice) { return symbol(choice).has_value(); });
        if ((section.toggles.empty() && section.choices.empty()) || !choicesFit || first == slots->size())
        {
            break;
        }
        const Slot top = (*slots)[first];
        const std::int32_t barX = top.x - kBarLeftOfColumn[static_cast<std::size_t>(top.column)];
        const std::int32_t barY = top.y + kBarDown;
        place({}, heading->bar, barX, barY);
        place(HeadingInstance(top), heading->text, barX + kTextRight, barY + kTextDown);
        const gfx::Bytes headingSetUp = HeadingSetUp(top, section.title);
        setUps.insert(setUps.end(), headingSetUp.begin(), headingSetUp.end());
        for (std::size_t i = 0; i < section.toggles.size(); ++i)
        {
            // A checkbox is what the game's own are.
            const Slot &row = (*slots)[first + kHeadingRows + i];
            place(ControlInstance(section.toggles[i].option), tab->character, row.x, row.y);
            const gfx::Bytes setUp = ToggleSetUp(section.toggles[i]);
            setUps.insert(setUps.end(), setUp.begin(), setUp.end());
        }
        for (std::size_t i = 0; i < section.choices.size(); ++i)
        {
            // A choice is the game's slider with named stops or its
            // dropdown, in two rows.
            const Choice &choice = section.choices[i];
            const Slot &row = (*slots)[first + kHeadingRows + section.toggles.size() + (kChoiceRows * i)];
            const std::int32_t right = choice.dropdown ? kDropdownRight : kChoiceRight;
            place(ControlInstance(choice.option), symbol(choice).value_or(0), row.x + right, row.y);
            const gfx::Bytes setUp = choice.dropdown ? DropdownSetUp(choice) : SliderSetUp(choice);
            setUps.insert(setUps.end(), setUp.begin(), setUp.end());
        }
        slots->erase(slots->begin() + static_cast<std::ptrdiff_t>(first),
                     slots->begin() + static_cast<std::ptrdiff_t>(first + rows));
        ++shown;
    }
    if (shown == 0)
    {
        return std::nullopt;
    }
    GiveDepths(placements, tab->deepest, picker);

    auto placed = gfx::PlaceInSprite(movie, tab->sprite, placements);
    auto patched =
        placed ? gfx::InsertBefore(*placed, {kSetUpRegister, std::string(kFirstSetUp)}, setUps) : std::nullopt;
    if (!patched)
    {
        shown = 0;
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
