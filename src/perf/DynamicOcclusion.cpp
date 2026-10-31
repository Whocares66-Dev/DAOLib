#include "perf/DynamicOcclusion.h"

#include "dao/Addresses.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "perf/Options.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace perf
{

namespace
{

constexpr const wchar_t *kSection = L"DynamicOcclusion";
constexpr const wchar_t *kKey = L"On";

// The choice, made once after each area's load and kept until the next: the
// first kSettle frames left out, then kMeasured frames timed with occlusion
// culling and kMeasured with view-frustum culling alone (after kSettle more);
// the second way is kept if its frames are kFaster shorter. A frame longer
// than kLongMs is a hitch and starts the check again, kMostTries times at the
// most; past that, occlusion culling stays.
constexpr int kSettle = 60;
constexpr std::size_t kMeasured = 60;
constexpr double kFaster = 0.95;
constexpr double kLongMs = 100.0;
constexpr int kMostTries = 5;

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

// The choice between the two ways, from the frames' lengths, on the game thread.
class Chooser
{
  public:
    // An area has loaded: check again, from occlusion culling.
    void AreaLoaded()
    {
        g_frustumOnly.store(false, std::memory_order_relaxed);
        tries_ = 0;
        Begin();
    }

    void Frame()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double ms = last_ == 0 ? 0.0 : static_cast<double>(now.QuadPart - last_) * msPerTick_;
        last_ = now.QuadPart;
        if (!g_on.load(std::memory_order_relaxed))
        {
            g_frustumOnly.store(false, std::memory_order_relaxed);
            stage_ = Stage::Done;
            return;
        }
        if (stage_ == Stage::Done)
        {
            return;
        }
        if (ms <= 0.0 || ms > kLongMs)
        {
            g_frustumOnly.store(false, std::memory_order_relaxed);
            if (++tries_ < kMostTries)
            {
                Begin();
            }
            else
            {
                stage_ = Stage::Done;
                spdlog::debug("dynamic occlusion: no steady frames to measure; occlusion culling kept");
            }
            return;
        }
        if (settle_ > 0)
        {
            --settle_;
            return;
        }
        sum_ += ms;
        if (++measured_ < kMeasured)
        {
            return;
        }
        const double mean = sum_ / kMeasured;
        sum_ = 0.0;
        measured_ = 0;
        if (stage_ == Stage::Occlusion)
        {
            occlusionMs_ = mean;
            g_frustumOnly.store(true, std::memory_order_relaxed);
            stage_ = Stage::Frustum;
            settle_ = kSettle;
            return;
        }
        const bool frustumOnly = mean < occlusionMs_ * kFaster;
        g_frustumOnly.store(frustumOnly, std::memory_order_relaxed);
        stage_ = Stage::Done;
        spdlog::info("dynamic occlusion: {} for this area ({:.2f} ms a frame with occlusion culling, {:.2f} without)",
                     frustumOnly ? "view-frustum culling alone" : "occlusion culling", occlusionMs_, mean);
    }

  private:
    enum class Stage
    {
        Done,      // the choice holds until the next load
        Occlusion, // timing occlusion culling
        Frustum,   // timing view-frustum culling alone
    };

    void Begin()
    {
        stage_ = Stage::Occlusion;
        settle_ = kSettle;
        sum_ = 0.0;
        measured_ = 0;
    }

    double msPerTick_ = [] {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return 1000.0 / static_cast<double>(frequency.QuadPart);
    }();
    std::int64_t last_ = 0;
    Stage stage_ = Stage::Done;
    int tries_ = 0;
    int settle_ = 0;
    double sum_ = 0.0;
    std::size_t measured_ = 0;
    double occlusionMs_ = 0.0;
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
