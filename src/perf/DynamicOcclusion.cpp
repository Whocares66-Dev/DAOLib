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
#include <unordered_map>

namespace perf
{

namespace
{

constexpr const wchar_t *kSection = L"DynamicOcclusion";
constexpr const wchar_t *kKey = L"On";
constexpr std::uint64_t kNewest = 2;              // frames an answer is given for after the GPU's
constexpr std::size_t kMostRemembered = 1U << 17; // answers kept, past which they are dropped

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

// The switch is the options screen's, on the game thread; the hooks read it on
// the update thread, which is the same one: the frame callback runs just before
// the game's update, and the commander's queries within it.
std::atomic<bool> g_on{true};
std::atomic<bool> g_frustumOnly{false};
std::atomic<std::uint64_t> g_frame{0};
std::atomic<bool> g_areaLoaded{false}; // set by the load's end, taken by the next frame

struct Answer
{
    DWORD pixels;
    std::uint64_t frame;
};
std::unordered_map<int, std::uint32_t> g_boxOfKey;       // a query's box this frame, by its key
std::unordered_map<std::uint32_t, Answer> g_answerOfBox; // a box's newest answer from the GPU

using BeginFn = void(__fastcall *)(void *renderer, int edx, int key);
using DrawFn = void(__fastcall *)(void *renderer, int edx, const void *query);
using ResultFn = bool(__fastcall *)(void *renderer, int edx, int key, DWORD *pixels, bool wait);
using SetParametersFn = void(__fastcall *)(void *camera, int edx, int width, int height, unsigned properties, float a,
                                           float b);
using UmbraGetFn = std::uintptr_t(__fastcall *)(const void *query, int edx);
dao::Detour<BeginFn> g_begin;
dao::Detour<DrawFn> g_draw;
dao::Detour<ResultFn> g_result;
dao::Detour<SetParametersFn> g_setParameters;
UmbraGetFn g_getIndex = nullptr;
UmbraGetFn g_getVertices = nullptr;
UmbraGetFn g_getVertexCount = nullptr;

// A function of umbra.dll's through the exe's import slot for it.
template <class Fn> Fn Imported(dao::AddressId slot)
{
    const auto at = dao::Module::Address(slot);
    return at == 0 ? nullptr : *reinterpret_cast<const Fn *>(at);
}

// FNV-1a over the box's vertices, three floats each.
std::uint32_t BoxOf(const void *query)
{
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(g_getVertices(query, 0));
    const auto count = static_cast<std::size_t>(g_getVertexCount(query, 0));
    std::uint32_t hash = 2166136261U;
    for (std::size_t i = 0; bytes != nullptr && i < count * 3 * sizeof(float); ++i)
    {
        hash = (hash ^ bytes[i]) * 16777619U;
    }
    return hash;
}

void Remember(int key, DWORD pixels)
{
    const auto box = g_boxOfKey.find(key);
    if (box != g_boxOfKey.end())
    {
        g_answerOfBox[box->second] = {pixels, g_frame.load(std::memory_order_relaxed)};
    }
}

// Before a query is begun again, the answer to its last one is read without
// waiting: by now, a frame on, the GPU has nearly always given it.
void __fastcall Begin(void *renderer, int edx, int key)
{
    if (g_on.load(std::memory_order_relaxed))
    {
        DWORD pixels = 0;
        if (g_result.Original()(renderer, edx, key, &pixels, false))
        {
            Remember(key, pixels);
        }
        if (g_answerOfBox.size() > kMostRemembered)
        {
            g_answerOfBox.clear();
        }
    }
    g_begin.Original()(renderer, edx, key);
}

void __fastcall Draw(void *renderer, int edx, const void *query)
{
    if (g_on.load(std::memory_order_relaxed))
    {
        g_boxOfKey[static_cast<int>(g_getIndex(query, 0))] = BoxOf(query);
    }
    g_draw.Original()(renderer, edx, query);
}

bool __fastcall Result(void *renderer, int edx, int key, DWORD *pixels, bool wait)
{
    if (!wait || !g_on.load(std::memory_order_relaxed))
    {
        return g_result.Original()(renderer, edx, key, pixels, wait);
    }
    if (g_result.Original()(renderer, edx, key, pixels, false))
    {
        Remember(key, *pixels);
        return true;
    }
    const auto box = g_boxOfKey.find(key);
    if (box != g_boxOfKey.end())
    {
        const auto answer = g_answerOfBox.find(box->second);
        if (answer != g_answerOfBox.end() && answer->second.frame + kNewest >= g_frame.load(std::memory_order_relaxed))
        {
            *pixels = answer->second.pixels;
            return true;
        }
    }
    const bool answered = g_result.Original()(renderer, edx, key, pixels, true);
    if (answered)
    {
        Remember(key, *pixels);
    }
    return answered;
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
    g_getIndex = Imported<UmbraGetFn>(dao::addr::kUmbra_OcclusionQuery_GetIndex_Import);
    g_getVertices = Imported<UmbraGetFn>(dao::addr::kUmbra_OcclusionQuery_GetVertices_Import);
    g_getVertexCount = Imported<UmbraGetFn>(dao::addr::kUmbra_OcclusionQuery_GetVertexCount_Import);
    const auto setParameters = Imported<SetParametersFn>(dao::addr::kUmbra_Camera_SetParameters_Import);
    return g_getIndex != nullptr && g_getVertices != nullptr && g_getVertexCount != nullptr &&
           g_result.Attach(dao::addr::kD3DUmbraRenderer_GetQueryResult, &Result) &&
           g_begin.Attach(dao::addr::kD3DUmbraRenderer_BeginQuery, &Begin) &&
           g_draw.Attach(dao::addr::kD3DUmbraRenderer_DrawQuery, &Draw) &&
           g_setParameters.AttachTo(setParameters, &SetParameters);
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
        g_result.Detach();
        g_begin.Detach();
        g_draw.Detach();
        g_setParameters.Detach();
        spdlog::warn("dynamic occlusion: Umbra's queries or camera could not be reached, so it is not in");
        return std::nullopt;
    }
    dao::OnFrame([] {
        if (!g_on.load(std::memory_order_relaxed))
        {
            return; // off: Umbra as it always was, and no check
        }
        g_frame.fetch_add(1, std::memory_order_relaxed);
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
