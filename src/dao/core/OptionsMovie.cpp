#include "dao/core/OptionsMovie.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace dao::core::options_movie
{

namespace
{

constexpr std::int32_t kRow = 35;          // pixels from one row to the next
constexpr std::int32_t kLastRow = 403;     // the lowest a row can start, the screen's buttons below
constexpr std::uint16_t kDepthStep = 6;    // as the designer's own placements are apart
constexpr std::int32_t kSetUpRegister = 2; // in SetupOptionsMenuInitFunctions: where the set-up functions are kept

constexpr std::string_view kLastLeft = "OptLootShimmer_mc";
constexpr std::string_view kLastRight = "OptShowTutorials_mc";
constexpr std::string_view kFirstSetUp = "OptAutoPause_mc_init";
constexpr std::string_view kHeadingSymbol = "KeyBindingHeader";
constexpr std::string_view kHeadingText = "KeyBindingHeader_mc_init";

// r2.<instance>_init = OptionsUtils.MakeOptionToggleInit(option, label,
// tooltip); the arguments pushed last first, as compiled script has them.
gfx::Bytes ToggleSetUp(const Toggle &toggle)
{
    using namespace gfx;
    return Join({Push({Register(kSetUpRegister), Text(ToggleInstance(toggle.option) + "_init"), Text(toggle.tooltip),
                       Text(toggle.label), Text(toggle.option), Integer(3), Text("OptionsUtils")}),
                 Do(Op::GetVariable), Push({Text("MakeOptionToggleInit")}), Do(Op::CallMethod), Do(Op::SetMember)});
}

// r2.<instance>_init = function () {
//     this.KeyBindingHeader_mc_init = function () { this.SetText(title); };
// };
gfx::Bytes HeadingSetUp(const Slot &slot, std::string_view title)
{
    using namespace gfx;
    const Bytes setText =
        Join({Push({Text(title), Integer(1), Register(1), Text("SetText")}), Do(Op::CallMethod), Do(Op::Pop)});
    const Bytes setUp = Join({Push({Register(1), Text(kHeadingText)}), Function(setText), Do(Op::SetMember)});
    return Join(
        {Push({Register(kSetUpRegister), Text(HeadingInstance(slot) + "_init")}), Function(setUp), Do(Op::SetMember)});
}

} // namespace

std::string ToggleInstance(std::string_view option)
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
    std::vector<Slot> slots;
    for (const auto &last : {*left, *right})
    {
        for (std::int32_t y = last.y + kRow; y <= kLastRow; y += kRow)
        {
            const bool taken = std::any_of(placed.begin(), placed.end(), [&](const gfx::Instance &instance) {
                return instance.x == last.x && instance.y == y;
            });
            if (!taken)
            {
                slots.push_back({last.x, y});
            }
        }
    }
    return slots;
}

std::optional<gfx::Bytes> AddSections(std::span<const std::byte> movie, std::span<const Section> sections,
                                      std::size_t &shown)
{
    shown = 0;
    const auto slots = FreeSlots(movie);
    const auto tab = gfx::FindInstance(movie, kLastLeft);
    const auto heading = gfx::ExportedCharacter(movie, kHeadingSymbol);
    if (!slots || !tab || !heading)
    {
        return std::nullopt;
    }

    std::vector<gfx::Placement> placements;
    gfx::Bytes setUps;
    std::size_t slot = 0;
    std::uint16_t depth = tab->deepest;
    const auto place = [&](std::string name, std::uint16_t character) {
        depth = static_cast<std::uint16_t>(depth + kDepthStep);
        placements.push_back({std::move(name), character, depth, (*slots)[slot].x, (*slots)[slot].y});
        ++slot;
    };
    for (const Section &section : sections)
    {
        if (section.toggles.empty() || slot + 1 + section.toggles.size() > slots->size())
        {
            break;
        }
        const Slot at = (*slots)[slot];
        place(HeadingInstance(at), *heading);
        const gfx::Bytes headingSetUp = HeadingSetUp(at, section.title);
        setUps.insert(setUps.end(), headingSetUp.begin(), headingSetUp.end());
        for (const Toggle &toggle : section.toggles)
        {
            // A checkbox is what the game's own are.
            place(ToggleInstance(toggle.option), tab->character);
            const gfx::Bytes setUp = ToggleSetUp(toggle);
            setUps.insert(setUps.end(), setUp.begin(), setUp.end());
        }
        ++shown;
    }
    if (shown == 0)
    {
        return std::nullopt;
    }

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
