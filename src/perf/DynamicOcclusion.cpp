#include "perf/DynamicOcclusion.h"

#include "dao/Addresses.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "perf/Options.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// Notes on dynamic occlusion:
//
// Umbra decides what the camera can see from the level's cells and, for the
// objects it is unsure of, occlusion queries: the GPU is asked how many pixels
// of an object's box would be drawn. That costs its search, an occluder pass
// and the queries; where little is hidden (the Brecilian Forest, 6.3 ms a frame
// with it, 4.4 without) that is more than it saves, and where much is
// (Denerim's market on an RTX 3090, 6.3 ms with it, 8.0 without) far less. On a
// slower GPU it costs more: the 3090 locked to 600 MHz, Denerim's market too ran
// faster without it (9.9 ms against 7.8). What is hidden is never seen either
// way, so the picture is the same: a paused frame in Denerim differed only in
// the HUD's blinking text and the hovered companion. What pays depends on what
// the camera sees, which changes as it turns and the party moves, even within one
// room: in Ostagar the spot the party loads in ran 15% faster without occlusion
// culling and the camp a few metres on 15-25% slower. So the choice is made all
// the time (#286): once a second a few frames are run the other way between
// frames run the way in use, the comparisons are smoothed by exponential decay,
// and the other way is taken when it is clearly faster, with the same margin to
// go back, so the way does not flip to and fro. A frame is timed by the interval
// between frames: whichever of the update thread, the render thread and the GPU
// holds the frame up, the interval shows it; at a vsync or a frame cap both ways
// take the cap's interval, the estimate stays and nothing changes that could be
// seen, and below the cap the difference shows again. Timing the update thread's
// work alone, less its wait for the render thread (the wait is
// kIGraphicsInternal_WaitForRenderThread), hid what occlusion culling saves on
// the GPU (the review of 9f33b51). View-frustum culling alone is Umbra's camera
// given properties 1 for 3.
//
// A choice made once an area, from where the party loaded in (#285), lost 6% in
// Ostagar; one made once a room the party stands in still lost 3%, an outdoor
// room being a cell of terrain whose views differ as much. Probing at once when
// the frames changed, with probes in turn, medians and a dwell after a switch,
// did worse than a probe a second: the frame time of a party running moves 10%
// every second or two, so the bursts never stopped (the forest +13% against
// +22%), and Ostagar's switch back took as long (#286's comments).
//
// #281 also gave a query the GPU had not yet answered the same box's answer from
// the frame before, so as not to wait for it. It was taken out (#285): measured
// with the party running a route, it gained nothing, on the 3090 (Denerim,
// six rounds, 5.78 ms against vanilla's 5.80) or at 600 MHz (Orzammar, 91 fps
// against 92), since the update thread seldom waited on the GPU.
//
// The picture was not the same (#315): the frame's scene depth, which distortion,
// depth of field and skin scattering read, is drawn by a depth pass of the instances
// Umbra sends the commander for its occlusion queries (command 0x90), and without
// occlusion culling it sends none, so at Redcliffe's bridge the stream's ripples
// were drawn across the bridge and the bank (Denerim's market, where paused frames
// were compared, has no water). While occlusion culling is out, a visible instance
// is now sent to the depth pass as Umbra sends it with occlusion culling: one, a
// node or a group, whose Umbra object has property 8 (dao/Addresses.h, counted in
// play; groups since #318, below). A
// first try that sent every visible instance, lights and emitters among them, had
// the game abort in the depth pass's sort (0x0045B3DD) within a minute. Without
// occlusion culling Umbra also sends the depth pass's bracket (0xa0, 0x91, 0xa1)
// before the visible instances, so what was sent waited for the next frame's
// flush, its nodes sometimes gone by then (a pure virtual call); the bracket is
// held and sent once they are all known, before 0xa3. Seen 2026-11-03 at the
// bridge: every frustum-only frame of 24 had a depth pass of the occlusion
// frames' 71 draws and the 12 of the occluded meshes it also draws, the water
// right, and no fault in minutes of play.
//
// That first sent every visible group, and a creature faded by an effect
// (EFFECT_TYPE_ALPHA: Fade Shroud with Combat Magic, stealth) had its face, hair and
// weapon drawn into the depth pass and nothing of it in colour, so the scene behind
// them was hidden and they showed black, each second's probe flashing them (#318).
// Umbra sends a group to the depth pass only while the group's Umbra object has
// property 8, and SyncUmbraProxyVisibility (0x00543B40) takes it away while the
// object's flags (+0x260) have 0x4000: seen 2026-11-04 in Redcliffe with Morrigan
// faded, 16 visible groups with it sent and the two without it (Morrigan and her
// staff) not. A group is now tested as a node is.
//

namespace perf
{

namespace
{

constexpr const wchar_t *kSection = L"DynamicOcclusion";
constexpr const wchar_t *kKey = L"On";

// The choice, made all the time from probes (#286): every kProbeMs of frames, kBlock
// frames are run the other way between kBlock before and kBlock after run the way in
// use, so the view is nearly the same for all three, and the probe's mean frame over
// theirs is one sample of how the other way compares here. A block's first frame after
// a change of way, which the other way's work may still hold up, is left out; a probe
// with a frame over kLongMs, or over kOutlier times its neighbours' mean, is a hitch
// and is dropped. The samples' logarithms are smoothed by exponential decay, each
// weighted kWeight, so a probe's noise barely moves the estimate while a change of view
// does within a few seconds; the other way is taken when the estimate says it is
// kFaster shorter, and the estimate is then turned about, so going back needs the same
// margin the other way and a view near the line does not flip to and fro (the user,
// 2026-10-31: "we don't want jittering"). The first kSettle frames after a load are
// left out, and a load starts again from occlusion culling, as the game does.
constexpr int kSettle = 60;
constexpr int kBlock = 8;
constexpr double kProbeMs = 1000.0;
constexpr double kLongMs = 100.0;
constexpr double kOutlier = 2.5;
constexpr double kWeight = 0.3;
constexpr double kFaster = 0.95;

// Umbra's camera properties: view-frustum culling 1, occlusion culling 2.
constexpr unsigned kFrustumAndOcclusion = 3;
constexpr unsigned kFrustumOnly = 1;

// The switch is set from the options screen and read by the frame callback and
// the camera's hook.
std::atomic<bool> g_on{true};
std::atomic<bool> g_frustumOnly{false};
std::atomic<bool> g_areaLoaded{false}; // set by the load's end, taken by the next frame

using SetParametersFn = void(__fastcall *)(void *camera, int edx, int width, int height, unsigned properties, float a,
                                           float b);
dao::Detour<SetParametersFn> g_setParameters;

// A function of umbra.dll's through the exe's import slot for it.
template <class Fn> Fn Imported(dao::AddressId slot)
{
    const auto at = dao::Module::Address(slot);
    return at == 0 ? nullptr : *reinterpret_cast<const Fn *>(at);
}

// The camera occlusion culling was last taken out of, while it is out of it.
std::atomic<const void *> g_cameraWithoutOcclusion{nullptr};

// Umbra::Camera::setParameters, each frame for the main view (0x00466E9F) and
// at the shadow views' set-up: occlusion culling taken out while it does not pay.
void __fastcall SetParameters(void *camera, int edx, int width, int height, unsigned properties, float a, float b)
{
    if (properties == kFrustumAndOcclusion)
    {
        if (g_on.load(std::memory_order_relaxed) && g_frustumOnly.load(std::memory_order_relaxed))
        {
            properties = kFrustumOnly;
            g_cameraWithoutOcclusion.store(camera, std::memory_order_relaxed);
        }
        else
        {
            const void *expected = camera;
            g_cameraWithoutOcclusion.compare_exchange_strong(expected, nullptr, std::memory_order_relaxed);
        }
    }
    g_setParameters.Original()(camera, edx, width, height, properties, a, b);
}

// Whether the culling running on this thread is of a camera without occlusion
// culling: its commands come to the commander within the camera's resolveVisibility.
thread_local bool t_resolvingWithoutOcclusion = false;

using CommandFn = void(__fastcall *)(void *commander, int edx, int command);
dao::Detour<CommandFn> g_command;

// The depth pass's bracket (0xa0, 0x91, 0xa1) of a view without occlusion culling,
// held until its visible instances are known (Command, below).
thread_local std::array<int, 3> t_held{};
thread_local std::size_t t_heldCount = 0;

// Sends the held bracket, and passes the rest of the view's commands through.
void Release(void *commander, int edx)
{
    t_resolvingWithoutOcclusion = false;
    for (std::size_t i = 0; i < t_heldCount; ++i)
    {
        g_command.Original()(commander, edx, t_held[i]);
    }
    t_heldCount = 0;
}

using ResolveVisibilityFn = void(__fastcall *)(const void *camera, int edx, void *commander, int a, float b);
dao::Detour<ResolveVisibilityFn> g_resolveVisibility;

void __fastcall ResolveVisibility(const void *camera, int edx, void *commander, int a, float b)
{
    t_resolvingWithoutOcclusion = camera == g_cameraWithoutOcclusion.load(std::memory_order_relaxed);
    t_heldCount = 0;
    g_resolveVisibility.Original()(camera, edx, commander, a, b);
    if (t_heldCount != 0)
    {
        Release(commander, 0); // a view that ended with no 0xa3: the bracket is still closed
    }
    t_resolvingWithoutOcclusion = false;
}

// The main view's Umbra commander's commands (dao/Addresses.h).
constexpr int kInstanceVisible = 0x30;
constexpr int kInstanceInDepth = 0x90;
constexpr int kDepthFlush = 0x91;
constexpr int kDepthBegin = 0xa0;
constexpr int kDepthEnd = 0xa1;
constexpr int kVisibleEnd = 0xa3;

// The Umbra object property an instance's depth is drawn for (dao/Addresses.h).
constexpr int kPropertyInDepth = 8;

using GetInstanceFn = const void *(__fastcall *)(const void *commander, int edx);
using GetUserPointerFn = void **(__fastcall *)(const void *instance, int edx);
using TestFn = bool(__fastcall *)(const void *object, int edx, int property);
GetInstanceFn g_getInstance = nullptr;
GetUserPointerFn g_getUserPointer = nullptr;
TestFn g_test = nullptr;

// The Umbra object of an instance's user pointer, the engine's {node, group}: a node's
// is the first field of its record (the node's +0x20, EclipseUmbraScene::AddNode); a
// group's, the first field of its proxy (the object's +0x300), the group being the
// object's +8 (EclipseUmbraScene::AddObject).
const void *UmbraObject(void *const *user)
{
    const auto *node = static_cast<const char *>(user[0]);
    const auto *group = static_cast<const char *>(user[1]);
    if (node == nullptr && group == nullptr)
    {
        return nullptr;
    }
    const char *holder = node != nullptr ? node + 0x20 : group - 8 + 0x300;
    const auto *fields = *reinterpret_cast<const void *const *const *>(holder);
    return fields != nullptr ? fields[0] : nullptr;
}

// Whether occlusion culling would have had the commander's current instance's depth
// drawn: one whose Umbra object has the property, a node or a group alike. A group's
// loses it while its object is faded (EFFECT_TYPE_ALPHA; #318).
bool DrawnInDepth(const void *commander)
{
    void *const *user = g_getUserPointer(g_getInstance(commander, 0), 0);
    if (user == nullptr)
    {
        return false;
    }
    const void *object = UmbraObject(user);
    return object != nullptr && g_test(object, 0, kPropertyInDepth);
}

// Without occlusion culling Umbra asks for no depth pass, and the scene depth that
// distortion and the frame's effects read was left empty (#315). With it, Umbra
// sends the instances to draw in depth within the depth pass's bracket, and the
// visible ones after it; without it, it sends the bracket before it knows what is
// visible. So the bracket is held, each visible instance is given to the depth pass
// as occlusion culling gives it, and the bracket is let go once they are all known:
// the engine then draws the same depth pass, in the same view, as with occlusion
// culling.
void __fastcall Command(void *commander, int edx, int command)
{
    if (t_resolvingWithoutOcclusion)
    {
        if ((command == kDepthBegin || command == kDepthFlush || command == kDepthEnd) && t_heldCount < t_held.size())
        {
            t_held[t_heldCount++] = command;
            return;
        }
        if (command == kInstanceVisible && DrawnInDepth(commander))
        {
            g_command.Original()(commander, edx, kInstanceInDepth);
        }
        else if (command == kVisibleEnd)
        {
            Release(commander, edx);
        }
    }
    g_command.Original()(commander, edx, command);
}

// The way frames run, chosen from probes of the other way, on the game thread.
class Chooser
{
  public:
    // An area has loaded: occlusion culling, and nothing known of the other way.
    void AreaLoaded()
    {
        Reset();
    }

    // Turned off: on again, the choice starts afresh.
    void Off()
    {
        Reset();
        last_ = 0;
    }

    void Frame()
    {
        const double ms = FrameMs();
        if (ms <= 0.0)
        {
            return;
        }
        if (settle_ > 0)
        {
            --settle_;
            return;
        }
        if (position_ < 0)
        {
            sinceProbe_ += ms;
            if (sinceProbe_ >= kProbeMs)
            {
                sinceProbe_ = 0.0;
                position_ = 0;
                around_.clear();
                probe_.clear();
            }
            return;
        }
        Probe(ms);
    }

  private:
    static constexpr int kWindow = 3 * kBlock;

    // Whether frame `position` of a probe's window runs the other way: the middle block.
    static bool OtherWay(int position)
    {
        return position >= kBlock && position < 2 * kBlock;
    }

    double FrameMs()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double ms = last_ == 0 ? 0.0 : static_cast<double>(now.QuadPart - last_) * msPerTick_;
        last_ = now.QuadPart;
        return ms;
    }

    void Reset()
    {
        frustumOnly_ = false;
        g_frustumOnly.store(false, std::memory_order_relaxed);
        estimate_ = 0.0;
        settle_ = kSettle;
        sinceProbe_ = 0.0;
        position_ = -1;
    }

    // `ms` is frame `position_` of the window, which ran the way set at the last call.
    void Probe(double ms)
    {
        if (ms > kLongMs)
        {
            End();
            return;
        }
        // The first frame of a block run after a change of way is left out.
        if (position_ != kBlock && position_ != 2 * kBlock)
        {
            (OtherWay(position_) ? probe_ : around_).push_back(ms);
        }
        if (++position_ < kWindow)
        {
            g_frustumOnly.store(OtherWay(position_) ? !frustumOnly_ : frustumOnly_, std::memory_order_relaxed);
            return;
        }
        const double around = Mean(around_);
        const double probe = Mean(probe_);
        End();
        const double typical = Median(around_);
        if (std::ranges::any_of(around_, [&](double each) { return each > typical * kOutlier; }) ||
            std::ranges::any_of(probe_, [&](double each) { return each > typical * kOutlier; }))
        {
            return;
        }
        estimate_ += kWeight * (std::log(probe / around) - estimate_);
        if (estimate_ < std::log(kFaster))
        {
            frustumOnly_ = !frustumOnly_;
            g_frustumOnly.store(frustumOnly_, std::memory_order_relaxed);
            spdlog::debug("dynamic occlusion: {} (the way left was {:.0f}% slower)",
                          frustumOnly_ ? "view-frustum culling alone" : "occlusion culling",
                          (std::exp(-estimate_) - 1.0) * 100.0);
            estimate_ = -estimate_;
        }
    }

    void End()
    {
        position_ = -1;
        g_frustumOnly.store(frustumOnly_, std::memory_order_relaxed);
    }

    static double Median(std::vector<double> ms)
    {
        const auto middle = ms.begin() + static_cast<std::ptrdiff_t>(ms.size() / 2);
        std::ranges::nth_element(ms, middle);
        return *middle;
    }

    static double Mean(const std::vector<double> &ms)
    {
        double sum = 0.0;
        for (const double each : ms)
        {
            sum += each;
        }
        return sum / static_cast<double>(ms.size());
    }

    double msPerTick_ = [] {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return 1000.0 / static_cast<double>(frequency.QuadPart);
    }();
    std::int64_t last_ = 0;
    bool frustumOnly_ = false; // the way in use
    double estimate_ = 0.0;    // the smoothed log of the other way's frame over this way's
    int settle_ = kSettle;
    double sinceProbe_ = 0.0;
    int position_ = -1; // the frame of a probe's window, or -1 between probes
    std::vector<double> around_;
    std::vector<double> probe_;
};

Chooser g_chooser;

void Set(bool on)
{
    g_on.store(on, std::memory_order_relaxed);
    if (!on)
    {
        g_frustumOnly.store(false, std::memory_order_relaxed); // and on again, occlusion culling until a load's check
    }
    if (WritePrivateProfileStringW(kSection, kKey, on ? L"1" : L"0", SettingsFile().c_str()) == FALSE)
    {
        spdlog::warn("dynamic occlusion: the switch could not be kept in {}; it is {} until the game is closed",
                     SettingsFile().string(), on ? "on" : "off");
    }
    spdlog::info("dynamic occlusion: turned {} in the options screen", on ? "on" : "off");
}

bool LeftOut()
{
    std::array<char, 4> value{};
    const DWORD length =
        GetEnvironmentVariableA("DAOLIB_DYNAMIC_OCCLUSION", value.data(), static_cast<DWORD>(value.size()));
    return length == 1 && value[0] == '0';
}

bool Attach()
{
    // The depth pass's two hooks before the switch's, so occlusion culling is never
    // taken out without them.
    const auto setParameters = Imported<SetParametersFn>(dao::addr::kUmbra_Camera_SetParameters_Import);
    const auto resolveVisibility = Imported<ResolveVisibilityFn>(dao::addr::kUmbra_Camera_ResolveVisibility_Import);
    g_getInstance = Imported<GetInstanceFn>(dao::addr::kUmbra_Commander_GetInstance_Import);
    g_getUserPointer = Imported<GetUserPointerFn>(dao::addr::kUmbra_Commander_Instance_GetUserPointer_Import);
    g_test = Imported<TestFn>(dao::addr::kUmbra_Object_Test_Import);
    if (setParameters == nullptr || resolveVisibility == nullptr || g_getInstance == nullptr ||
        g_getUserPointer == nullptr || g_test == nullptr ||
        !g_command.Attach(dao::addr::kEclipseUmbraCommander_Command, &Command))
    {
        return false;
    }
    if (!g_resolveVisibility.AttachTo(resolveVisibility, &ResolveVisibility) ||
        !g_setParameters.AttachTo(setParameters, &SetParameters))
    {
        g_resolveVisibility.Detach();
        g_command.Detach();
        return false;
    }
    return true;
}

} // namespace

std::optional<dao::options::Toggle> InstallDynamicOcclusion()
{
    if (LeftOut())
    {
        spdlog::info("dynamic occlusion: DAOLIB_DYNAMIC_OCCLUSION=0, so it is not in: Umbra runs as it always did");
        return std::nullopt;
    }
    if (!Attach())
    {
        spdlog::warn("dynamic occlusion: Umbra's camera could not be reached, so it is not in");
        return std::nullopt;
    }
    dao::OnFrame([] {
        if (!g_on.load(std::memory_order_relaxed))
        {
            g_chooser.Off();
            return; // off: Umbra as it always was, and no check
        }
        if (g_areaLoaded.exchange(false, std::memory_order_relaxed))
        {
            g_chooser.AreaLoaded();
        }
        g_chooser.Frame();
    });
    dao::modes::OnAreaLoaded([] { g_areaLoaded.store(true, std::memory_order_relaxed); });
    g_on.store(GetPrivateProfileIntW(kSection, kKey, 1, SettingsFile().c_str()) != 0, std::memory_order_relaxed);
    spdlog::info("dynamic occlusion: {} ({})", g_on.load() ? "on" : "off", SettingsFile().string());
    return dao::options::Toggle{
        .name = L"DAOLibDynamicOcclusion",
        .label = L"Dynamic occlusion culling",
        .tooltip = L"Use occlusion culling only where it improves performance.",
        .get = [] { return g_on.load(std::memory_order_relaxed); },
        .set =
            [](bool wanted) {
                Set(wanted);
                return wanted;
            },
    };
}

} // namespace perf
