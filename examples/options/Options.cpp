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
// A second section, "DAOLib example choice", has a choice of four named
// values, the game's own slider with named stops: it goes under the right
// column, since the left is full. It starts at "Normal"; the arrows and
// the slider change it, the name beside it follows, and each change is
// logged. Not yet seen in play.
// What it does goes to example_options.log.
#include "dao/Options.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>

namespace
{

constexpr const wchar_t *kTitle = L"DAOLib example";
constexpr const wchar_t *kChoiceTitle = L"DAOLib example choice";

std::atomic<bool> g_plain{false};
std::atomic<bool> g_startsOn{true};
std::atomic<std::int32_t> g_speed{1};

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
    static dao::options::SectionState was = dao::options::SectionState::Unknown;
    static dao::options::SectionState choiceWas = dao::options::SectionState::Unknown;
    const dao::options::SectionState now = dao::options::StateOf(kTitle);
    if (now != was)
    {
        spdlog::info("the section: {}", Name(now));
        was = now;
    }
    const dao::options::SectionState choiceNow = dao::options::StateOf(kChoiceTitle);
    if (choiceNow != choiceWas)
    {
        spdlog::info("the choice's section: {}", Name(choiceNow));
        choiceWas = choiceNow;
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
    // A choice of one value is refused.
    dao::options::Choice single = speed;
    single.name = L"ExampleSingle";
    single.values = {L"Only"};
    const bool choiceAdded = dao::options::AddSection(kChoiceTitle, {}, {speed});
    const bool singleRefused = !dao::options::AddSection(L"Refused choice", {}, {single});
    spdlog::info("the choice added: {}; one value refused: {}", choiceAdded, singleRefused);
    dao::OnFrame(&Frame);
    return true;
}
