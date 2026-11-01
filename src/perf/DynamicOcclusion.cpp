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

// The switch is the options screen's, on the game thread; the hook reads it on
// the update thread, which is the same one: the frame callback runs just before
// the game's update, and Umbra's camera is set within it.
std::atomic<bool> g_on{true};
std::atomic<bool> g_frustumOnly{false};
std::atomic<bool> g_areaLoaded{false}; // set by the load's end, taken by the next frame

using SetParametersFn = void(__fastcall *)(void *camera, int edx, int width, int height, unsigned properties, float a,
                                           float b);
dao::Detour<SetParametersFn> g_setParameters;

// The update thread's wait for the render thread, summed: a frame is timed less it, so
// the frame's own work is compared, not the interval a vsync or a frame cap holds every
// frame to whichever way runs (#286).
using WaitFn = void(__fastcall *)(void *graphics, int edx);
dao::Detour<WaitFn> g_wait;
std::atomic<std::int64_t> g_waitTicks{0};

void __fastcall Wait(void *graphics, int edx)
{
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    QueryPerformanceCounter(&start);
    g_wait.Original()(graphics, edx);
    QueryPerformanceCounter(&end);
    g_waitTicks.fetch_add(end.QuadPart - start.QuadPart, std::memory_order_relaxed);
}

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
        const std::int64_t waited = g_waitTicks.exchange(0, std::memory_order_relaxed);
        const double ms = last_ == 0 ? 0.0 : static_cast<double>(now.QuadPart - last_ - waited) * msPerTick_;
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
        if (std::ranges::any_of(around_, [&](double each) { return each > around * kOutlier; }) ||
            std::ranges::any_of(probe_, [&](double each) { return each > around * kOutlier; }))
        {
            return;
        }
        estimate_ += kWeight * (std::log(probe / around) - estimate_);
        if (estimate_ < std::log(kFaster))
        {
            frustumOnly_ = !frustumOnly_;
            g_frustumOnly.store(frustumOnly_, std::memory_order_relaxed);
            estimate_ = -estimate_;
            spdlog::debug("dynamic occlusion: {} (the other way was {:.0f}% slower)",
                          frustumOnly_ ? "view-frustum culling alone" : "occlusion culling",
                          (std::exp(estimate_) - 1.0) * 100.0);
        }
    }

    void End()
    {
        position_ = -1;
        g_frustumOnly.store(frustumOnly_, std::memory_order_relaxed);
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
    return setParameters != nullptr && g_setParameters.AttachTo(setParameters, &SetParameters) &&
           g_wait.Attach(dao::addr::kIGraphicsInternal_WaitForRenderThread, &Wait);
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
        g_setParameters.Detach();
        g_wait.Detach();
        spdlog::warn("dynamic occlusion: Umbra's camera or the wait for the render thread could not be reached, so "
                     "it is not in");
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
