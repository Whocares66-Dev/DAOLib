// Example: settings of the plugin's own in the game's options screen
// (dao/Options.h).
//
// A section "DAOLib example" goes on the Game tab, under the game's own
// options, with three checkboxes: one that takes every click, one that
// starts ticked, and one that refuses every click and so should draw
// itself unticked again. Every time the game asks a box's value or the
// player clicks one, a line is logged; and once the options screen has
// been opened, what became of the section.
//
// What to look for in play: Options, the Game tab: the heading, a gold bar
// like the game's "Gameplay", and the three boxes under the left column,
// the second ticked. Click each: the
// first and second change and stay changed, the third goes back. Rest the
// mouse on one for its tooltip. Close with OK or Cancel and open again:
// the boxes are as they were left (a change is made at the click). All of
// it seen 2026-10-01 (dao/Options.h).
//
// A second section, "DAOLib example choice", has two choices of named
// values and goes under the right column, since the left is full. The first
// is a dropdown, the game's own as Difficulty is: a button with the value's
// name, "Second" at the start, which opens a list of the three over the
// slider under it; a pick closes the list, puts the name on the button and
// is logged. The second is the game's slider with named stops, of four: it
// starts at "Normal"; the track's ends and a drag change it, the name beside
// it follows, and each change is logged. The slider was seen 2026-10-02
// and the dropdown 2026-10-11 (dao/Options.cpp).
//
// A third section, "DAOLib example video", goes on the Video tab, at the
// top of its right column: a checkbox and a slider of three stops. For it
// the library puts the tab's own "Detail" settings under "Screen" in the
// left column. A fourth, "DAOLib example left out", asks for the Video tab
// too, with more sliders than the column has rows left: it is not on the
// screen, the log has the library's warning for it, and its state is "no
// room".
// What it does goes to example_options.log.
#include "dao/Options.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{

constexpr const wchar_t *kTitle = L"DAOLib example";
constexpr const wchar_t *kChoiceTitle = L"DAOLib example choice";
constexpr const wchar_t *kVideoTitle = L"DAOLib example video";
constexpr const wchar_t *kLeftOutTitle = L"DAOLib example left out";
constexpr std::array kTitles = {kTitle, kChoiceTitle, kVideoTitle, kLeftOutTitle};

std::atomic<bool> g_plain{false};
std::atomic<bool> g_startsOn{true};
std::atomic<bool> g_video{false};
std::atomic<bool> g_leftOut{false};
std::atomic<std::int32_t> g_speed{1};
std::atomic<std::int32_t> g_mode{1};
std::atomic<std::int32_t> g_look{0};

const char *Name(dao::options::SectionState state)
{
    switch (state)
    {
    case dao::options::SectionState::Unknown:
        return "unknown";
    case dao::options::SectionState::Waiting:
        return "waiting for the options screen";
    case dao::options::SectionState::Shown:
        return "shown";
    case dao::options::SectionState::NoRoom:
        return "no room";
    case dao::options::SectionState::Failed:
        return "failed: the options movie was not as read";
    }
    return "?";
}

dao::options::Toggle Kept(const wchar_t *name, const wchar_t *label, const wchar_t *tooltip, std::atomic<bool> &value)
{
    dao::options::Toggle toggle;
    toggle.name = name;
    toggle.label = label;
    toggle.tooltip = tooltip;
    toggle.get = [name, &value] {
        spdlog::info("{} asked: {}", dao::Utf8(name), value.load());
        return value.load();
    };
    toggle.set = [name, &value](bool wanted) {
        spdlog::info("{} clicked: {} -> {}", dao::Utf8(name), value.load(), wanted);
        value = wanted;
        return wanted;
    };
    return toggle;
}

void Frame()
{
    static std::array<dao::options::SectionState, kTitles.size()> was{};
    for (std::size_t i = 0; i < kTitles.size(); ++i)
    {
        const dao::options::SectionState now = dao::options::StateOf(kTitles[i]);
        if (now != was[i])
        {
            spdlog::info("the section \"{}\": {}", dao::Utf8(kTitles[i]), Name(now));
            was[i] = now;
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_options");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::options::Toggle refuses;
    refuses.name = L"ExampleRefuses";
    refuses.label = L"Refuses every click";
    refuses.tooltip = L"The plugin answers no, and the box goes back";
    refuses.get = [] {
        spdlog::info("ExampleRefuses asked: false");
        return false;
    };
    refuses.set = [](bool wanted) {
        spdlog::info("ExampleRefuses clicked: wanted {}, refused", wanted);
        return false;
    };
    const bool added = dao::options::AddSection(
        kTitle, {Kept(L"ExamplePlain", L"Takes every click", L"A setting the plugin keeps", g_plain),
                 Kept(L"ExampleStartsOn", L"Starts ticked", L"On until it is clicked", g_startsOn), refuses});
    // A name with a space is refused, and so is a section with nothing in it.
    dao::options::Toggle spaced = Kept(L"Not A Name", L"refused", L"", g_plain);
    const bool badName = dao::options::AddSection(L"Refused", {spaced});
    const bool empty = dao::options::AddSection(L"Empty", {});
    spdlog::info("added: {}; refused as they should be: {} {}", added, !badName, !empty);

    dao::options::Choice speed;
    speed.name = L"ExampleSpeed";
    speed.label = L"A choice";
    speed.tooltip = L"Four named stops; each change is logged";
    speed.values = {L"Slow", L"Normal", L"Fast", L"Fastest"};
    speed.get = [] {
        spdlog::info("ExampleSpeed asked: {}", g_speed.load());
        return g_speed.load();
    };
    speed.set = [](std::int32_t wanted) {
        spdlog::info("ExampleSpeed changed: {} -> {}", g_speed.load(), wanted);
        g_speed = wanted;
        return wanted;
    };
    dao::options::Choice mode;
    mode.name = L"ExampleMode";
    mode.label = L"A dropdown";
    mode.tooltip = L"Three named values in a list; each pick is logged";
    mode.values = {L"First", L"Second", L"Third"};
    mode.style = dao::options::ChoiceStyle::Dropdown;
    mode.get = [] {
        spdlog::info("ExampleMode asked: {}", g_mode.load());
        return g_mode.load();
    };
    mode.set = [](std::int32_t wanted) {
        spdlog::info("ExampleMode changed: {} -> {}", g_mode.load(), wanted);
        g_mode = wanted;
        return wanted;
    };
    // A choice of one value is refused.
    dao::options::Choice single = speed;
    single.name = L"ExampleSingle";
    single.values = {L"Only"};
    const bool choiceAdded = dao::options::AddSection(kChoiceTitle, {}, {mode, speed});
    const bool singleRefused = !dao::options::AddSection(L"Refused choice", {}, {single});
    spdlog::info("the choice added: {}; one value refused: {}", choiceAdded, singleRefused);

    // On the Video tab: a checkbox and a slider, five of the right column's fourteen rows; and a
    // section after it that the tab has no room for, five sliders being ten rows.
    dao::options::Choice look;
    look.name = L"ExampleLook";
    look.label = L"A choice on Video";
    look.tooltip = L"Three named stops; each change is logged";
    look.values = {L"Plain", L"Warm", L"Cold"};
    look.get = [] {
        spdlog::info("ExampleLook asked: {}", g_look.load());
        return g_look.load();
    };
    look.set = [](std::int32_t wanted) {
        spdlog::info("ExampleLook changed: {} -> {}", g_look.load(), wanted);
        g_look = wanted;
        return wanted;
    };
    const bool videoAdded = dao::options::AddSection(
        kVideoTitle, {Kept(L"ExampleVideo", L"A box on Video", L"A setting on the Video tab", g_video)}, {look},
        dao::options::Column::Either, dao::options::Tab::Video);
    std::vector<dao::options::Choice> tooMany;
    for (const wchar_t *name :
         {L"ExampleLeftOutA", L"ExampleLeftOutB", L"ExampleLeftOutC", L"ExampleLeftOutD", L"ExampleLeftOutE"})
    {
        dao::options::Choice filler;
        filler.name = name;
        filler.label = L"Not on the screen";
        filler.values = {L"Off", L"On"};
        filler.get = [] { return std::int32_t{0}; };
        filler.set = [](std::int32_t wanted) { return wanted; };
        tooMany.push_back(std::move(filler));
    }
    const bool leftOutAdded =
        dao::options::AddSection(kLeftOutTitle, {Kept(L"ExampleLeftOut", L"Not on the screen", L"", g_leftOut)},
                                 std::move(tooMany), dao::options::Column::Either, dao::options::Tab::Video);
    spdlog::info("the Video tab's added: {}; the one it has no room for added: {}", videoAdded, leftOutAdded);
    dao::OnFrame(&Frame);
    return true;
}
