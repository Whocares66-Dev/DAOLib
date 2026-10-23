#include "DrawCheck.h"

#include "dao/Frame.h"
#include "dao/Render.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace probe
{

namespace
{

using Clock = std::chrono::steady_clock;
using Micro = std::chrono::microseconds;

constexpr auto kEvery = std::chrono::seconds(10);
constexpr std::uint32_t kAlphaBlendEnable = 27; // D3DRS_ALPHABLENDENABLE
constexpr std::uint32_t kZEnable = 7;           // D3DRS_ZENABLE
constexpr std::size_t kSamples = 8;             // draws logged with what they are, a frame

// What one armed frame showed; the render thread's while the frame is drawn.
struct Tally
{
    std::array<std::size_t, 4> calls{};
    std::size_t tabled = 0;    // draws whose two shaders' tables both read
    std::size_t constants = 0; // values handed, both shaders
    std::size_t textures = 0;
    std::size_t named = 0; // textures with a .dds name
    std::size_t blended = 0;
    std::size_t owned = 0;   // draws that say their material
    std::size_t noded = 0;   // and their node
    std::size_t trees = 0;   // SpeedTree's (material SPT_...), which have no node
    std::size_t sampled = 0; // of those, logged
    bool logged = false;     // one draw laid out
    std::uint64_t frame = 0;
    bool presented = false; // the armed frame is all in
};

std::mutex g_lock;
Tally g_tally;
std::optional<dao::render::Watcher> g_watch;
Clock::time_point g_armed;
Clock::time_point g_lastFrame; // the game thread's
double g_average = 0.0;        // the game's frames' time while none is armed, us

// One draw laid out: its shaders' targets, each constant's name, set, registers and first value.
void LayOut(const dao::render::Draw &draw)
{
    for (const auto *shader : {&draw.vertex, &draw.pixel})
    {
        if (shader->table == nullptr)
        {
            continue;
        }
        spdlog::info("draw check: draw {} of frame {}, {} with {} constants:", draw.index, draw.frame,
                     shader->table->target, shader->values.size());
        for (const dao::render::Value &value : shader->values)
        {
            std::string first;
            if (!value.floats.empty())
            {
                first = fmt::format("({:.3f}, {:.3f}, {:.3f}, {:.3f})", value.floats[0], value.floats[1],
                                    value.floats[2], value.floats[3]);
            }
            else if (!value.bools.empty())
            {
                for (const bool b : value.bools)
                {
                    first += b ? '1' : '0';
                }
            }
            else if (!value.ints.empty())
            {
                first = fmt::format("({}, {}, {}, {})", value.ints[0], value.ints[1], value.ints[2], value.ints[3]);
            }
            spdlog::info("draw check:   {} set {} r{}..{} {}", value.constant->name,
                         static_cast<int>(value.constant->set), value.constant->index,
                         value.constant->index + value.constant->count - 1, first);
        }
        for (const dao::render::Sampled &sampled : shader->textures)
        {
            spdlog::info("draw check:   s{} {}", sampled.sampler,
                         sampled.name.empty() ? std::string("(no file)") : dao::Utf8(sampled.name));
        }
    }
}

void OnDraw(const dao::render::Draw &draw)
{
    const std::scoped_lock hold(g_lock);
    g_tally.calls.at(static_cast<std::size_t>(draw.call))++;
    const bool tabled = draw.vertex.table != nullptr && draw.pixel.table != nullptr;
    g_tally.tabled += tabled ? 1 : 0;
    g_tally.constants += draw.vertex.values.size() + draw.pixel.values.size();
    for (const auto *shader : {&draw.vertex, &draw.pixel})
    {
        g_tally.textures += shader->textures.size();
        for (const dao::render::Sampled &sampled : shader->textures)
        {
            g_tally.named += sampled.name.empty() ? 0 : 1;
        }
    }
    if (!draw.material.empty())
    {
        ++g_tally.owned;
        g_tally.noded += draw.node != 0 ? 1 : 0;
        g_tally.trees += draw.material.starts_with(L"SPT_") ? 1 : 0;
        if (g_tally.sampled < kSamples && draw.index % 200 == 0)
        {
            ++g_tally.sampled;
            spdlog::info("draw check: draw {} is {} {}, material {}", draw.index, dao::Utf8(draw.model),
                         dao::Utf8(draw.mesh), dao::Utf8(draw.material));
        }
    }
    for (const dao::render::State &state : draw.states)
    {
        g_tally.blended += state.state == kAlphaBlendEnable && state.value != 0 ? 1 : 0;
    }
    if (!g_tally.logged && tabled && !draw.pixel.textures.empty())
    {
        g_tally.logged = true;
        LayOut(draw);
    }
}

// The armed frame is all in: the game thread's next frame logs it.
void OnPresent(std::uint64_t frame)
{
    const std::scoped_lock hold(g_lock);
    g_tally.frame = frame;
    g_tally.presented = true;
}

void Log(const Tally &t, Micro span, double average)
{
    const std::size_t draws = t.calls[0] + t.calls[1] + t.calls[2] + t.calls[3];
    spdlog::info("draw check: frame {}: {} draws ({} DrawPrimitive, {} DrawIndexedPrimitive, {} DrawPrimitiveUP, {} "
                 "DrawIndexedPrimitiveUP), {} with both tables read, {} constants, {} textures ({} by name), {} "
                 "blended, {} saying their material ({} trees; {} their node); the game's frame across it {} us, its "
                 "frames' average {:.0f} us",
                 t.frame, draws, t.calls[0], t.calls[1], t.calls[2], t.calls[3], t.tabled, t.constants, t.textures,
                 t.named, t.blended, t.owned, t.trees, t.noded, span.count(), average);
}

// From the game's frame: logs an armed frame once it is in, keeps the frames' average time while none
// is armed, and arms one every kEvery.
void Rearm()
{
    const auto now = Clock::now();
    if (g_lastFrame == Clock::time_point{})
    {
        g_lastFrame = now; // no frame before the first to time it against
        return;
    }
    const auto span = std::chrono::duration_cast<Micro>(now - g_lastFrame);
    g_lastFrame = now;
    std::optional<Tally> done;
    {
        const std::scoped_lock hold(g_lock);
        if (g_tally.presented)
        {
            done = g_tally;
            g_tally = {};
        }
    }
    if (done)
    {
        Log(*done, span, g_average);
    }
    else if (g_watch && !g_watch->Armed())
    {
        g_average = g_average == 0.0 ? static_cast<double>(span.count())
                                     : (0.95 * g_average) + (0.05 * static_cast<double>(span.count()));
    }
    if (g_watch && !g_watch->Armed() && now - g_armed >= kEvery)
    {
        g_watch->Arm(1, {kAlphaBlendEnable, kZEnable});
        g_armed = now;
    }
}

} // namespace

void AttachDrawCheck()
{
    g_watch = dao::render::Watch(&OnDraw, &OnPresent);
    dao::OnFrame(&Rearm);
    spdlog::info("draw check: on, a frame every {} s", kEvery.count());
}

} // namespace probe
