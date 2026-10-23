#include "dao/Render.h"

#include "dao/Hook.h"
#include "dao/Resources.h"

#include <Windows.h>
#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

// Notes on the draws (DAOLib #246, 2026-10-23): the game draws through one IDirect3DDevice9 made
// by IDirect3D9::CreateDevice (slot 16) on the Direct3D9 object Direct3DCreate9 returns; its frames
// end at the device's Present (slot 17) or, during an area's arrival, at the implicit swap chain's
// (slot 3; dao/Addresses.h at kD3DSwapChain_Present). The device is not made pure, so its Get*
// methods answer: the shaders' functions and constants, the textures and the render states. These
// are d3d9.dll's own functions, detoured where the vtables point (as the probe's colour-curve watch
// and DAOElgaraShaders' capture did before), so a second plugin detouring them as well chains.

namespace dao::render
{

namespace
{

using CreateFn = IDirect3D9 *(__stdcall *)(UINT);
using CreateDeviceFn = HRESULT(__stdcall *)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *,
                                            IDirect3DDevice9 **);
using PresentFn = HRESULT(__stdcall *)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
using ChainPresentFn = HRESULT(__stdcall *)(IDirect3DSwapChain9 *, const RECT *, const RECT *, HWND, const RGNDATA *,
                                            DWORD);
using DrawFn = HRESULT(__stdcall *)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT);
using DrawIndexedFn = HRESULT(__stdcall *)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
using DrawUpFn = HRESULT(__stdcall *)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, const void *, UINT);
using DrawIndexedUpFn = HRESULT(__stdcall *)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void *,
                                             D3DFORMAT, const void *, UINT);

constexpr std::size_t kCreateDeviceSlot = 16;  // IDirect3D9::CreateDevice
constexpr std::size_t kPresentSlot = 17;       // IDirect3DDevice9::Present
constexpr std::size_t kDrawSlot = 81;          // IDirect3DDevice9::DrawPrimitive
constexpr std::size_t kDrawIndexedSlot = 82;   // IDirect3DDevice9::DrawIndexedPrimitive
constexpr std::size_t kDrawUpSlot = 83;        // IDirect3DDevice9::DrawPrimitiveUP
constexpr std::size_t kDrawIndexedUpSlot = 84; // IDirect3DDevice9::DrawIndexedPrimitiveUP
constexpr std::size_t kChainPresentSlot = 3;   // IDirect3DSwapChain9::Present
constexpr DWORD kVertexSamplers = D3DVERTEXTEXTURESAMPLER0;

Detour<CreateFn> g_create;
Detour<CreateDeviceFn> g_createDevice;
Detour<PresentFn> g_present;
Detour<ChainPresentFn> g_chainPresent;
Detour<DrawFn> g_draw;
Detour<DrawIndexedFn> g_drawIndexed;
Detour<DrawUpFn> g_drawUp;
Detour<DrawIndexedUpFn> g_drawIndexedUp;

template <class Fn> Fn Method(void *object, std::size_t slot)
{
    return reinterpret_cast<Fn>((*static_cast<void ***>(object))[slot]);
}

// A watch as the library keeps it.
struct Record
{
    DrawEvent draw;
    PresentEvent present;
    std::vector<std::uint32_t> states;
    std::uint64_t first = 0; // the first frame handed
    std::uint64_t last = 0;  // the frame after the last, 0 for no end
    bool armed = false;
};

std::mutex g_lock;            // the records and the names
std::deque<Record> g_records; // a deque: a record does not move when another is added
std::atomic<int> g_armed{0};  // records armed: the draws' one test
std::atomic<std::uint64_t> g_frame{0};
std::uint32_t g_index = 0; // the draw's place in its frame; the render thread's alone
std::unordered_map<IDirect3DBaseTexture9 *, std::wstring> g_names; // textures made from a .dds

bool Active(const Record &record, std::uint64_t frame)
{
    return record.armed && frame >= record.first && (record.last == 0 || frame < record.last);
}

// The watches active for a frame, and the render states they ask for between them.
struct Handing
{
    std::vector<const Record *> records;
    std::vector<std::uint32_t> states;
};

Handing HandingFor(std::uint64_t frame)
{
    Handing handing;
    const std::lock_guard lock(g_lock);
    for (const Record &record : g_records)
    {
        if (Active(record, frame))
        {
            handing.records.push_back(&record);
            for (const std::uint32_t state : record.states)
            {
                if (std::ranges::find(handing.states, state) == handing.states.end())
                {
                    handing.states.push_back(state);
                }
            }
        }
    }
    return handing;
}

// What a draw's views point into, kept until the next draw on the render thread.
struct Scratch
{
    std::vector<std::byte> vertexCode;
    std::vector<std::byte> pixelCode;
    std::optional<core::ctab::Table> vertexTable;
    std::optional<core::ctab::Table> pixelTable;
    std::deque<std::wstring> names;
};

thread_local Scratch t_scratch;

std::wstring_view NameOf(IDirect3DBaseTexture9 *texture)
{
    const std::lock_guard lock(g_lock);
    const auto found = g_names.find(texture);
    if (found == g_names.end())
    {
        return {};
    }
    return t_scratch.names.emplace_back(found->second);
}

template <class ShaderT> void ReadCode(ShaderT *shader, std::vector<std::byte> &code)
{
    code.clear();
    UINT size = 0;
    if (shader == nullptr || FAILED(shader->GetFunction(nullptr, &size)) || size == 0)
    {
        return;
    }
    code.resize(size);
    if (FAILED(shader->GetFunction(code.data(), &size)))
    {
        code.clear();
    }
}

// A constant's registers as the device holds them, for the vertex shader or the pixel shader.
Value ValueOf(IDirect3DDevice9 *device, const core::ctab::Constant &constant, bool vertex)
{
    Value value;
    value.constant = &constant;
    const UINT index = constant.index;
    const UINT count = constant.count;
    switch (constant.set)
    {
    case core::ctab::RegisterSet::Float4:
        value.floats.resize(std::size_t{count} * 4);
        vertex ? device->GetVertexShaderConstantF(index, value.floats.data(), count)
               : device->GetPixelShaderConstantF(index, value.floats.data(), count);
        break;
    case core::ctab::RegisterSet::Int4:
        value.ints.resize(std::size_t{count} * 4);
        vertex ? device->GetVertexShaderConstantI(index, value.ints.data(), count)
               : device->GetPixelShaderConstantI(index, value.ints.data(), count);
        break;
    case core::ctab::RegisterSet::Bool: {
        std::vector<BOOL> bools(count);
        vertex ? device->GetVertexShaderConstantB(index, bools.data(), count)
               : device->GetPixelShaderConstantB(index, bools.data(), count);
        value.bools.assign(bools.begin(), bools.end());
        break;
    }
    case core::ctab::RegisterSet::Sampler:
        break;
    }
    return value;
}

void ReadSamplers(IDirect3DDevice9 *device, const core::ctab::Constant &constant, bool vertex, Shader &shader)
{
    for (std::uint32_t reg = constant.index; reg < std::uint32_t{constant.index} + constant.count; ++reg)
    {
        IDirect3DBaseTexture9 *texture = nullptr;
        if (FAILED(device->GetTexture(vertex ? kVertexSamplers + reg : reg, &texture)) || texture == nullptr)
        {
            continue;
        }
        shader.textures.push_back({reg, texture, NameOf(texture)});
        texture->Release(); // the device still holds it while the draw is handed
    }
}

void ReadShader(IDirect3DDevice9 *device, bool vertex, Shader &shader)
{
    std::vector<std::byte> &code = vertex ? t_scratch.vertexCode : t_scratch.pixelCode;
    std::optional<core::ctab::Table> &table = vertex ? t_scratch.vertexTable : t_scratch.pixelTable;
    if (vertex)
    {
        IDirect3DVertexShader9 *bound = nullptr;
        device->GetVertexShader(&bound);
        ReadCode(bound, code);
        if (bound != nullptr)
        {
            bound->Release();
        }
    }
    else
    {
        IDirect3DPixelShader9 *bound = nullptr;
        device->GetPixelShader(&bound);
        ReadCode(bound, code);
        if (bound != nullptr)
        {
            bound->Release();
        }
    }
    shader.bytecode = code;
    table = code.empty() ? std::nullopt : core::ctab::Read(code);
    if (!table)
    {
        return;
    }
    shader.table = &*table;
    shader.values.reserve(table->constants.size());
    for (const core::ctab::Constant &constant : table->constants)
    {
        shader.values.push_back(ValueOf(device, constant, vertex));
        if (constant.set == core::ctab::RegisterSet::Sampler)
        {
            ReadSamplers(device, constant, vertex, shader);
        }
    }
}

// Reads the device's state for a draw and hands it to the active watches. The draw's own arguments
// are filled by the caller.
void Hand(Draw &draw)
{
    const std::uint64_t frame = g_frame.load();
    const Handing handing = HandingFor(frame);
    if (handing.records.empty())
    {
        return;
    }
    t_scratch.names.clear();
    draw.frame = frame;
    draw.index = g_index;
    ReadShader(draw.device, true, draw.vertex);
    ReadShader(draw.device, false, draw.pixel);
    for (const std::uint32_t state : handing.states)
    {
        DWORD value = 0;
        draw.device->GetRenderState(static_cast<D3DRENDERSTATETYPE>(state), &value);
        draw.states.push_back({state, value});
    }
    for (const Record *record : handing.records)
    {
        record->draw(draw);
    }
}

// The frame ends: its watches are told, those whose frames are done disarm, and the count moves on.
void EndFrame()
{
    const std::uint64_t frame = g_frame.load();
    std::vector<const Record *> told;
    if (g_armed.load() > 0)
    {
        const std::lock_guard lock(g_lock);
        for (Record &record : g_records)
        {
            if (!Active(record, frame))
            {
                continue;
            }
            if (record.present)
            {
                told.push_back(&record);
            }
            if (record.last != 0 && frame + 1 >= record.last)
            {
                record.armed = false;
                --g_armed;
            }
        }
    }
    for (const Record *record : told)
    {
        record->present(frame);
    }
    g_index = 0;
    ++g_frame;
}

HRESULT __stdcall DrawPrimitive(IDirect3DDevice9 *device, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    if (g_armed.load() > 0)
    {
        Draw draw;
        draw.device = device;
        draw.call = Call::Primitive;
        draw.type = type;
        draw.start = start;
        draw.primitives = count;
        Hand(draw);
    }
    ++g_index;
    return g_draw.Original()(device, type, start, count);
}

HRESULT __stdcall DrawIndexedPrimitive(IDirect3DDevice9 *device, D3DPRIMITIVETYPE type, INT base, UINT minIndex,
                                       UINT vertices, UINT start, UINT count)
{
    if (g_armed.load() > 0)
    {
        Draw draw;
        draw.device = device;
        draw.call = Call::IndexedPrimitive;
        draw.type = type;
        draw.baseVertex = base;
        draw.minIndex = minIndex;
        draw.vertices = vertices;
        draw.start = start;
        draw.primitives = count;
        Hand(draw);
    }
    ++g_index;
    return g_drawIndexed.Original()(device, type, base, minIndex, vertices, start, count);
}

HRESULT __stdcall DrawPrimitiveUp(IDirect3DDevice9 *device, D3DPRIMITIVETYPE type, UINT count, const void *data,
                                  UINT stride)
{
    if (g_armed.load() > 0)
    {
        Draw draw;
        draw.device = device;
        draw.call = Call::PrimitiveUp;
        draw.type = type;
        draw.primitives = count;
        Hand(draw);
    }
    ++g_index;
    return g_drawUp.Original()(device, type, count, data, stride);
}

HRESULT __stdcall DrawIndexedPrimitiveUp(IDirect3DDevice9 *device, D3DPRIMITIVETYPE type, UINT minIndex, UINT vertices,
                                         UINT count, const void *indices, D3DFORMAT format, const void *data,
                                         UINT stride)
{
    if (g_armed.load() > 0)
    {
        Draw draw;
        draw.device = device;
        draw.call = Call::IndexedPrimitiveUp;
        draw.type = type;
        draw.minIndex = minIndex;
        draw.vertices = vertices;
        draw.primitives = count;
        Hand(draw);
    }
    ++g_index;
    return g_drawIndexedUp.Original()(device, type, minIndex, vertices, count, indices, format, data, stride);
}

HRESULT __stdcall Present(IDirect3DDevice9 *device, const RECT *source, const RECT *dest, HWND window,
                          const RGNDATA *dirty)
{
    EndFrame();
    return g_present.Original()(device, source, dest, window, dirty);
}

HRESULT __stdcall ChainPresent(IDirect3DSwapChain9 *chain, const RECT *source, const RECT *dest, HWND window,
                               const RGNDATA *dirty, DWORD flags)
{
    EndFrame();
    return g_chainPresent.Original()(chain, source, dest, window, dirty, flags);
}

void AttachDevice(IDirect3DDevice9 *device)
{
    const bool drawn =
        g_draw.AttachTo(Method<DrawFn>(device, kDrawSlot), &DrawPrimitive) &&
        g_drawIndexed.AttachTo(Method<DrawIndexedFn>(device, kDrawIndexedSlot), &DrawIndexedPrimitive) &&
        g_drawUp.AttachTo(Method<DrawUpFn>(device, kDrawUpSlot), &DrawPrimitiveUp) &&
        g_drawIndexedUp.AttachTo(Method<DrawIndexedUpFn>(device, kDrawIndexedUpSlot), &DrawIndexedPrimitiveUp);
    bool presented = g_present.AttachTo(Method<PresentFn>(device, kPresentSlot), &Present);
    IDirect3DSwapChain9 *chain = nullptr;
    if (SUCCEEDED(device->GetSwapChain(0, &chain)) && chain != nullptr)
    {
        presented =
            g_chainPresent.AttachTo(Method<ChainPresentFn>(chain, kChainPresentSlot), &ChainPresent) && presented;
        chain->Release();
    }
    spdlog::info("render: {} on the device's draws, {} on its presents", drawn ? "detours" : "no detours",
                 presented ? "detours" : "not all detours");
}

HRESULT __stdcall CreateDevice(IDirect3D9 *direct3d, UINT adapter, D3DDEVTYPE type, HWND window, DWORD behaviour,
                               D3DPRESENT_PARAMETERS *parameters, IDirect3DDevice9 **device)
{
    const HRESULT result = g_createDevice.Original()(direct3d, adapter, type, window, behaviour, parameters, device);
    if (SUCCEEDED(result) && device != nullptr && *device != nullptr && !g_draw.IsAttached())
    {
        AttachDevice(*device);
    }
    return result;
}

IDirect3D9 *__stdcall Create(UINT version)
{
    IDirect3D9 *direct3d = g_create.Original()(version);
    if (direct3d != nullptr && !g_createDevice.IsAttached() &&
        !g_createDevice.AttachTo(Method<CreateDeviceFn>(direct3d, kCreateDeviceSlot), &CreateDevice))
    {
        spdlog::warn("render: no detour on CreateDevice");
    }
    return direct3d;
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        resources::OnTexture(
            [](const resources::Texture &texture) {
                const std::lock_guard lock(g_lock);
                g_names[texture.texture] = std::wstring(texture.name);
            },
            [](const resources::Texture &texture) {
                const std::lock_guard lock(g_lock);
                g_names.erase(texture.texture);
            });
        const HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
        if (d3d9 == nullptr ||
            !g_create.AttachTo(reinterpret_cast<CreateFn>(GetProcAddress(d3d9, "Direct3DCreate9")), &Create))
        {
            spdlog::warn("render: no detour on Direct3DCreate9");
        }
    });
}

} // namespace

void Watcher::Arm(std::uint32_t frames, std::vector<std::uint32_t> states) const
{
    const std::lock_guard lock(g_lock);
    Record &record = g_records.at(id_);
    record.states = std::move(states);
    record.first = g_frame.load() + 1;
    record.last = frames == 0 ? 0 : record.first + frames;
    if (!record.armed)
    {
        record.armed = true;
        ++g_armed;
    }
}

void Watcher::Disarm() const
{
    const std::lock_guard lock(g_lock);
    Record &record = g_records.at(id_);
    if (record.armed)
    {
        record.armed = false;
        --g_armed;
    }
}

bool Watcher::Armed() const
{
    const std::lock_guard lock(g_lock);
    return g_records.at(id_).armed;
}

Watcher Watch(DrawEvent draw, PresentEvent present)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_records.push_back({std::move(draw), std::move(present), {}, 0, 0, false});
    return Watcher(g_records.size() - 1);
}

std::uint64_t Frame()
{
    return g_frame.load();
}

} // namespace dao::render
