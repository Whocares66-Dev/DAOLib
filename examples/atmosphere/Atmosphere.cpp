// Example: the area's sky and sunlight, read and set (dao/Atmosphere.h).
//
// How far can an area be taken from day to night without leaving it? In
// any area, once the game is in explore mode, the plugin goes through a
// list of looks, ten seconds each, round and round. A look is three
// things: the sky (the sun's disc, the air, the clouds, the fog, the
// moon), the level's sunlight (the light on the ground and the creatures,
// and the way their shadows fall) and the picture's grade (the
// framebuffer's colour effect, which Ostagar's scripts use). The list has
// the area's own; the sunlight dimmed, turned blue, turned round and put
// out; the game's night, battle and lightning tables; the grade's hue,
// saturation and brightness; and a night made on the spot, the night
// table's sky over moonlight, with the steps of a dusk on the way to it.
// Each look is announced in the log as it is set, `state N: <name>`, with
// what then read back, so a screenshot can be put beside each line.
//
// The plugin keeps every area's own sky and sunlight as it comes to it,
// and each one it has seen is a look in every other area: an area under
// another's sky and sun. The game's day and night Redcliffe are two areas,
// so with a file named trip.txt beside the DLL, Redcliffe village
// (arl100ar_redcliffe_village) is left after its first look for the night
// village (arl101ar_redcliffe_night) and come back to after a round there:
// the day village is then seen under the night village's sky and sun, and
// the night village under the day's. (The night village is the siege's:
// its fight starts some ten seconds in and ends explore mode, and with it
// the looks, until it is over.)
//
// Seen in Redcliffe 2026-10-10 (DAOLib #62): every look there in the next
// frame; the sky and the far haze follow the sky alone, the ground, the
// creatures and their shadows the sunlight alone, and the grade everything
// but the interface, its hue in degrees. Trees, grass and buildings keep
// their level's light through all of it. Each area has a grade of its own
// (the day village's saturation 0.85, the night's 0.82), and with it put
// back after each look the area's own is the load's again.
//
// What it does goes to example_atmosphere.log.
#include "dao/Atmosphere.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Framebuffer.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using dao::atmosphere::Preset;
using dao::atmosphere::Sky;
using dao::atmosphere::Sunlight;
using dao::atmosphere::WithAir;
using dao::atmosphere::WithClouds;
using dao::atmosphere::WithFog;
using Clock = std::chrono::steady_clock;

constexpr auto kStateTime = std::chrono::seconds(10);
constexpr std::wstring_view kDayVillage = L"arl100ar_redcliffe_village";
constexpr std::wstring_view kNightVillage = L"arl101ar_redcliffe_night";
// The framebuffer's colour effect and its values, as Ostagar's scripts name
// them (pre100_atmosphere_h.nss); the game's own rows leave brightness,
// saturation and contrast at 1 and hue at 0.
constexpr std::wstring_view kColourEffect = L"da_HSVMatrix";
// The night village's sunlight, as its level has it: the colour a made night borrows.
constexpr std::array<float, 4> kMoonlight{0.27F, 0.41F, 0.79F, 1.0F};

// The picture's grade: the framebuffer's colour effect, as the game has it or as a look wants
// it. An area comes with its own, which is on.
struct Grade
{
    bool on = true;
    float hue = 0; // degrees round the colour wheel
    float saturation = 1;
    float contrast = 1;
    float brightness = 1;
};

struct Look
{
    Sky sky;
    Sunlight sun;
    Grade grade;
};

struct State
{
    std::string name;
    Look look;
};

std::map<std::wstring, Look> g_seen; // each area's own sky and sunlight, by the area's tag
std::wstring g_area;                 // the area the states are for
std::vector<State> g_states;
std::size_t g_next = 0;
Clock::time_point g_due;
int g_rounds = 0;       // whole rounds made in this area
bool g_trip = false;    // trip.txt is there
bool g_tripped = false; // the day village has been left for the night's

std::string Values(const Sky &sky)
{
    std::string text;
    for (const dao::core::SkyField &field : dao::core::kSkyFields)
    {
        text += fmt::format("{}{} {:.4g}", text.empty() ? "" : ", ", field.name, sky.*field.member);
    }
    return text;
}

std::string Values(const Sunlight &sun)
{
    return fmt::format("colour {:.3g} {:.3g} {:.3g} {:.3g}, on creatures {:.3g} {:.3g} {:.3g} {:.3g}, direction "
                       "{:.3g} {:.3g} {:.3g} {:.3g}",
                       sun.color[0], sun.color[1], sun.color[2], sun.color[3], sun.characterColor[0],
                       sun.characterColor[1], sun.characterColor[2], sun.characterColor[3], sun.direction[0],
                       sun.direction[1], sun.direction[2], sun.direction[3]);
}

// What of `now` is not as `wanted`, for the log: nothing, if the sky took every value.
std::string Differences(const Sky &wanted, const Sky &now)
{
    std::string text;
    for (const dao::core::SkyField &field : dao::core::kSkyFields)
    {
        if (wanted.*field.member != now.*field.member)
        {
            text += fmt::format("{}{} {:.4g} for {:.4g}", text.empty() ? "" : ", ", field.name, now.*field.member,
                                wanted.*field.member);
        }
    }
    return text;
}

std::string Values(const Grade &grade)
{
    return fmt::format("{}, hue {:.4g}, saturation {:.4g}, contrast {:.4g}, brightness {:.4g}", grade.on ? "on" : "off",
                       grade.hue, grade.saturation, grade.contrast, grade.brightness);
}

// The grade as the game has it now; one left as it is made where the game has no such effect.
Grade CurrentGrade()
{
    namespace fb = dao::framebuffer;
    Grade grade;
    grade.on = fb::Enabled(kColourEffect).value_or(grade.on);
    grade.hue = fb::Value(kColourEffect, L"Hue").value_or(grade.hue);
    grade.saturation = fb::Value(kColourEffect, L"Saturation").value_or(grade.saturation);
    grade.contrast = fb::Value(kColourEffect, L"Contrast").value_or(grade.contrast);
    grade.brightness = fb::Value(kColourEffect, L"Brightness").value_or(grade.brightness);
    return grade;
}

void Set(const Grade &grade)
{
    namespace fb = dao::framebuffer;
    fb::SetValue(kColourEffect, L"Hue", grade.hue);
    fb::SetValue(kColourEffect, L"Saturation", grade.saturation);
    fb::SetValue(kColourEffect, L"Contrast", grade.contrast);
    fb::SetValue(kColourEffect, L"Brightness", grade.brightness);
    fb::SetEnabled(kColourEffect, grade.on);
}

void Set(const Look &look)
{
    Set(look.grade);
    const bool sky = dao::atmosphere::SetSky(look.sky);
    const bool sun = dao::atmosphere::SetSunlight(look.sun);
    const auto now = dao::atmosphere::CurrentSky();
    const std::string differences = now ? Differences(look.sky, *now) : "nothing to read";
    spdlog::info("  sky {}, sunlight {}; sky read back: {}; sunlight read back: {}", sky ? "taken" : "not taken",
                 sun ? "taken" : "not taken", differences.empty() ? "as set" : differences,
                 dao::atmosphere::CurrentSunlight() == look.sun ? "as set" : "not as set");
}

Sky Table(const Sky &sky, Preset preset)
{
    return WithFog(WithClouds(WithAir(sky, preset), preset), preset);
}

Sunlight Scaled(Sunlight sun, float by)
{
    for (auto *colour : {&sun.color, &sun.characterColor})
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            (*colour)[i] *= by;
        }
    }
    return sun;
}

Sunlight Coloured(Sunlight sun, const std::array<float, 4> &colour)
{
    sun.color = colour;
    sun.characterColor = colour;
    return sun;
}

// A look some share of the way from one to another, with the grade of the one it starts from.
Look Between(const Look &from, const Look &to, float share)
{
    return {dao::core::Blend(from.sky, to.sky, share), dao::core::Blend(from.sun, to.sun, share), from.grade};
}

std::vector<State> StatesFor(const std::wstring &area, const Look &own)
{
    const auto with = [&own](auto change) {
        Look look = own;
        change(look);
        return look;
    };
    const auto graded = [&with](float hue, float saturation, float brightness) {
        return with([=](Look &look) { look.grade = {true, hue, saturation, look.grade.contrast, brightness}; });
    };
    // A night for an area that has none: the night table's sky, and the area's sun where it
    // stands, as moonlight.
    const Look night{Table(own.sky, Preset::Night), Coloured(own.sun, kMoonlight), own.grade};
    std::vector<State> states{
        {"the area's own", own},
        {"sunlight a fifth", with([](Look &look) { look.sun = Scaled(look.sun, 0.2F); })},
        {"sunlight blue", with([](Look &look) { look.sun = Coloured(look.sun, {0.1F, 0.2F, 1.0F, 1.0F}); })},
        {"sunlight from the other side", with([](Look &look) {
             look.sun.direction[0] = -look.sun.direction[0];
             look.sun.direction[1] = -look.sun.direction[1];
         })},
        {"sunlight out", with([](Look &look) { look.sun = Scaled(look.sun, 0); })},
        {"night table", with([](Look &look) { look.sky = Table(look.sky, Preset::Night); })},
        {"battle table", with([](Look &look) { look.sky = Table(look.sky, Preset::Battle); })},
        {"lightning table", with([](Look &look) { look.sky = Table(look.sky, Preset::Lightning); })},
        {"grade hue 180", graded(180, 1, 1)},
        {"grade saturation 0.3", graded(0, 0.3F, 1)},
        {"grade brightness 0.5", graded(0, 1, 0.5F)},
        {"a night made here: a quarter of the way", Between(own, night, 0.25F)},
        {"a night made here: half way", Between(own, night, 0.5F)},
        {"a night made here: three quarters of the way", Between(own, night, 0.75F)},
        {"a night made here", night},
        // The trees, the grass and the buildings keep the day's light, which neither the sky
        // nor the sunlight is: the grade takes some of its warmth out.
        {"a night made here, graded", {night.sky, night.sun, {true, 0, 0.55F, own.grade.contrast, 0.8F}}},
    };
    for (const auto &[tag, other] : g_seen)
    {
        if (tag != area)
        {
            const std::string of = dao::Utf8(tag);
            states.push_back({"the sky of " + of, with([&other](Look &look) { look.sky = other.sky; })});
            states.push_back({"the sunlight of " + of, with([&other](Look &look) { look.sun = other.sun; })});
            states.push_back({"half way to the sky and sunlight of " + of, Between(own, other, 0.5F)});
            states.push_back({"the sky and sunlight of " + of, {other.sky, other.sun, own.grade}});
        }
    }
    return states;
}

// The area the controlled creature is in, or "" where there is none to ask.
std::wstring Area()
{
    const dao::ObjectId controlled = dao::script::api::GetMainControlled();
    if (controlled == dao::kInvalidObject)
    {
        return {};
    }
    const dao::ObjectId area = dao::script::api::GetArea(controlled);
    return area != dao::kInvalidObject ? dao::script::api::GetTag(area) : std::wstring();
}

void Arrive(const std::wstring &area, const Look &own)
{
    g_seen.insert_or_assign(area, own);
    g_area = area;
    g_states = StatesFor(area, own);
    g_next = 0;
    g_rounds = 0;
    g_due = Clock::now();
    spdlog::info("area {}: its own sky: {}", dao::Utf8(area), Values(own.sky));
    spdlog::info("area {}: its sunlight: {}", dao::Utf8(area), Values(own.sun));
    spdlog::info("area {}: its grade: {}", dao::Utf8(area), Values(own.grade));
    std::string effects;
    for (const std::wstring &name : dao::framebuffer::Effects())
    {
        effects += fmt::format("{}{} ({})", effects.empty() ? "" : ", ", dao::Utf8(name),
                               dao::framebuffer::Enabled(name).value_or(false) ? "on" : "off");
    }
    spdlog::info("area {}: the framebuffer's effects: {}", dao::Utf8(area), effects);
    if (const auto now = dao::atmosphere::CurrentSky(); now && *now != own.sky)
    {
        spdlog::info("area {}: the sky drawn is not its own: {}", dao::Utf8(area), Differences(own.sky, *now));
    }
}

// Redcliffe's two villages: the day's left for the night's once its own look has been seen,
// and the night's for the day's after a round. True if the party is on its way.
bool Trip()
{
    if (!g_trip)
    {
        return false;
    }
    std::wstring_view to;
    std::wstring_view at;
    if (g_area == kDayVillage && !g_tripped && g_rounds == 0 && g_next == 1)
    {
        to = kNightVillage;
        at = L"arl101wp_party_start";
    }
    else if (g_area == kNightVillage && g_rounds == 1 && g_next == 0)
    {
        to = kDayVillage;
        at = L"arl100wp_start";
    }
    else
    {
        return false;
    }
    g_tripped = true; // asked once each way, whatever comes of it
    g_rounds = 2;
    // The area's own again before leaving it, so nothing of this look is carried along.
    Set(g_states.front().look.grade);
    dao::atmosphere::ResetSky();
    dao::atmosphere::ResetSunlight();
    const auto result = dao::script::api::DoAreaTransition(to, at);
    spdlog::info("trip: to {} at {}: DoAreaTransition answered {}", dao::Utf8(to), dao::Utf8(at), result);
    return true;
}

void Frame()
{
    if (dao::CClientExoApp::CurrentMode() != dao::GameMode::Explore)
    {
        return;
    }
    const std::wstring area = Area();
    const auto sky = dao::atmosphere::AreaOwnSky();
    const auto sun = dao::atmosphere::AreaOwnSunlight();
    if (area.empty() || !sky || !sun)
    {
        return;
    }
    if (area != g_area)
    {
        Arrive(area, {*sky, *sun, CurrentGrade()});
    }
    if (Clock::now() < g_due || Trip())
    {
        return;
    }
    const State &state = g_states[g_next];
    spdlog::info("state {}: {}", g_next, state.name);
    Set(state.look);
    g_due = Clock::now() + kStateTime;
    g_next = (g_next + 1) % g_states.size();
    if (g_next == 0)
    {
        ++g_rounds;
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_atmosphere");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    std::error_code ignored;
    g_trip = std::filesystem::exists(std::filesystem::path(info->pluginPath).parent_path() / "trip.txt", ignored);
    spdlog::info("a state every {} s in explore mode; the trip between Redcliffe's villages is {}", kStateTime.count(),
                 g_trip ? "on (trip.txt)" : "off (no trip.txt beside the DLL)");
    dao::OnFrame(&Frame);
    return true;
}
