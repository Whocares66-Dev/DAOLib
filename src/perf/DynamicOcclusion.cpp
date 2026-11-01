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

// Umbra::Camera::setParameters, each frame for the main view (0x00466E9F) and
// at the shadow views' set-up: occlusion culling taken out while it does not pay.
void __fastcall SetParameters(void *camera, int edx, int width, int height, unsigned properties, float a, float b)
{
    if (properties == kFrustumAndOcclusion && g_on.load(std::memory_order_relaxed) &&
        g_frustumOnly.load(std::memory_order_relaxed))
    {
        properties = kFrustumOnly;
    }
    g_setParameters.Original()(camera, edx, width, height, properties, a, b);
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
    const auto setParameters = Imported<SetParametersFn>(dao::addr::kUmbra_Camera_SetParameters_Import);
    return setParameters != nullptr && g_setParameters.AttachTo(setParameters, &SetParameters);
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
