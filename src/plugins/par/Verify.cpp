// The device verifier of the par plugin (DAOLib #280): what the Direct3D device is given, seen at every draw.
//
// The draws of a frame are replayed by the render thread from the recorded stream, so two ways of recording
// a frame draw the same picture exactly when the device holds the same state at every draw, in the same
// order. This hooks the device's setters and draws (found through a throwaway device of the same driver),
// keeps a shadow of everything set as running hashes per kind of state, and at each draw stores the hashes
// with the draw's own arguments. At Present the frame's list is kept; "dump.txt" beside the DLL, holding a
// label, has the last frame written as <label>.bin in the log folder, for tools/par_diff.py to compare.
#include "dao/Log.h"

#include <Windows.h>
#include <d3d9.h>

#include <detours/detours.h>
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{

std::uint64_t Mix(std::uint64_t k, std::uint64_t v)
{
    std::uint64_t x = (k + 1) * 0x9E3779B97F4A7C15ULL ^ (v + 0x632BE59BD9B4E019ULL);
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

enum Kind : std::size_t
{
    kRenderStates,
    kSamplers,
    kTextures,
    kShaders,
    kVertexConstants,
    kPixelConstants,
    kGeometry, // streams, indices, declaration
    kTargets,  // render targets, depth, viewport, scissor
    kKinds
};

struct Shadow
{
    std::array<std::uint64_t, kKinds> hash{};
    std::array<std::uint32_t, 256> renderState{};
    std::array<std::uint32_t, 20 * 16> sampler{};
    std::array<std::uint64_t, 20> texture{};
    std::array<std::uint32_t, 256 * 4> vertexConstant{};
    std::array<std::uint32_t, 256 * 4> pixelConstant{};
    std::array<std::uint64_t, 16 * 3> stream{};
    std::array<std::uint64_t, 8> single{}; // vs, ps, decl, indices, depth, viewport, scissor
    std::array<std::uint64_t, 4> target{};
};

struct DrawRecord
{
    std::array<std::uint64_t, kKinds> state;
    std::uint64_t draw;
};

// A draw's whole device state, for a dump: written in this order, the arguments of the draw last.
struct Snapshot
{
    std::array<std::uint32_t, 256> renderState;
    std::array<std::uint32_t, 20 * 16> sampler;
    std::array<std::uint64_t, 20> texture;
    std::array<std::uint32_t, 256 * 4> vertexConstant;
    std::array<std::uint32_t, 256 * 4> pixelConstant;
    std::array<std::uint64_t, 16 * 3> stream;
    std::array<std::uint64_t, 8> single;
    std::array<std::uint64_t, 4> target;
    std::uint64_t draw;
    std::uint64_t pad;
};
std::atomic<int> g_capture{0}; // 0 idle, 1 asked for, 2 recording the frame under way
std::vector<Snapshot> g_snapshots;
std::string g_captureLabel;

std::mutex g_mutex; // the render thread owns the shadow; Present and the dump meet here
Shadow g_shadow;
std::vector<DrawRecord> g_current;
std::vector<DrawRecord> g_last;
std::atomic<std::uint64_t> g_frames{0};
std::uint64_t g_lastFrameHash = 0;
std::filesystem::path g_dir;

std::atomic<DWORD> g_renderThread{0}; // the thread that presents: the only one whose device calls are followed

bool Mine()
{
    const DWORD t = g_renderThread.load(std::memory_order_relaxed);
    return t == 0 || t == GetCurrentThreadId();
}

void Put(Kind kind, std::uint64_t slot, std::uint64_t oldValue, std::uint64_t newValue)
{
    g_shadow.hash[kind] ^= Mix(slot, oldValue) ^ Mix(slot, newValue);
}

void SetRenderStateShadow(DWORD state, DWORD value)
{
    if (state < 256)
    {
        Put(kRenderStates, state, g_shadow.renderState[state], value);
        g_shadow.renderState[state] = value;
    }
}

template <std::size_t N>
void SetWords(Kind kind, std::array<std::uint32_t, N> &store, std::size_t first, const std::uint32_t *words,
              std::size_t count)
{
    for (std::size_t i = 0; i < count && first + i < N; ++i)
    {
        Put(kind, first + i, store[first + i], words[i]);
        store[first + i] = words[i];
    }
}

void SetSingle(std::size_t index, const void *value)
{
    const auto v = reinterpret_cast<std::uint64_t>(value);
    Put(kShaders, index, g_shadow.single[index], v);
    g_shadow.single[index] = v;
}

// The hooks. Each calls the real method, which comes first so the shadow follows what the device was given.
using Dev = IDirect3DDevice9;
HRESULT(STDMETHODCALLTYPE *g_setRenderState)(Dev *, D3DRENDERSTATETYPE, DWORD);
HRESULT(STDMETHODCALLTYPE *g_setSamplerState)(Dev *, DWORD, D3DSAMPLERSTATETYPE, DWORD);
HRESULT(STDMETHODCALLTYPE *g_setTexture)(Dev *, DWORD, IDirect3DBaseTexture9 *);
HRESULT(STDMETHODCALLTYPE *g_setVertexShader)(Dev *, IDirect3DVertexShader9 *);
HRESULT(STDMETHODCALLTYPE *g_setPixelShader)(Dev *, IDirect3DPixelShader9 *);
HRESULT(STDMETHODCALLTYPE *g_setVertexShaderConstantF)(Dev *, UINT, const float *, UINT);
HRESULT(STDMETHODCALLTYPE *g_setPixelShaderConstantF)(Dev *, UINT, const float *, UINT);
HRESULT(STDMETHODCALLTYPE *g_setStreamSource)(Dev *, UINT, IDirect3DVertexBuffer9 *, UINT, UINT);
HRESULT(STDMETHODCALLTYPE *g_setIndices)(Dev *, IDirect3DIndexBuffer9 *);
HRESULT(STDMETHODCALLTYPE *g_setVertexDeclaration)(Dev *, IDirect3DVertexDeclaration9 *);
HRESULT(STDMETHODCALLTYPE *g_setRenderTarget)(Dev *, DWORD, IDirect3DSurface9 *);
HRESULT(STDMETHODCALLTYPE *g_setDepthStencilSurface)(Dev *, IDirect3DSurface9 *);
HRESULT(STDMETHODCALLTYPE *g_setViewport)(Dev *, const D3DVIEWPORT9 *);
HRESULT(STDMETHODCALLTYPE *g_setScissorRect)(Dev *, const RECT *);
HRESULT(STDMETHODCALLTYPE *g_drawPrimitive)(Dev *, D3DPRIMITIVETYPE, UINT, UINT);
HRESULT(STDMETHODCALLTYPE *g_drawIndexedPrimitive)(Dev *, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
HRESULT(STDMETHODCALLTYPE *g_present)(Dev *, const RECT *, const RECT *, HWND, const RGNDATA *);
HRESULT(STDMETHODCALLTYPE *g_swapPresent)(IDirect3DSwapChain9 *, const RECT *, const RECT *, HWND, const RGNDATA *, DWORD);

HRESULT STDMETHODCALLTYPE SetRenderStateHook(Dev *d, D3DRENDERSTATETYPE state, DWORD value)
{
    if (!Mine())
    {
        return g_setRenderState(d, state, value);
    }
    SetRenderStateShadow(state, value);
    return g_setRenderState(d, state, value);
}

HRESULT STDMETHODCALLTYPE SetSamplerStateHook(Dev *d, DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value)
{
    if (!Mine())
    {
        return g_setSamplerState(d, sampler, type, value);
    }
    const std::size_t slot = (sampler < 20 ? sampler : 19) * 16 + (type < 16 ? type : 15);
    Put(kSamplers, slot, g_shadow.sampler[slot], value);
    g_shadow.sampler[slot] = value;
    return g_setSamplerState(d, sampler, type, value);
}

HRESULT STDMETHODCALLTYPE SetTextureHook(Dev *d, DWORD stage, IDirect3DBaseTexture9 *texture)
{
    if (!Mine())
    {
        return g_setTexture(d, stage, texture);
    }
    const std::size_t slot = stage < 20 ? stage : 19;
    const auto v = reinterpret_cast<std::uint64_t>(texture);
    Put(kTextures, slot, g_shadow.texture[slot], v);
    g_shadow.texture[slot] = v;
    return g_setTexture(d, stage, texture);
}

HRESULT STDMETHODCALLTYPE SetVertexShaderHook(Dev *d, IDirect3DVertexShader9 *shader)
{
    if (!Mine())
    {
        return g_setVertexShader(d, shader);
    }
    SetSingle(0, shader);
    return g_setVertexShader(d, shader);
}

HRESULT STDMETHODCALLTYPE SetPixelShaderHook(Dev *d, IDirect3DPixelShader9 *shader)
{
    if (!Mine())
    {
        return g_setPixelShader(d, shader);
    }
    SetSingle(1, shader);
    return g_setPixelShader(d, shader);
}

HRESULT STDMETHODCALLTYPE SetVertexShaderConstantFHook(Dev *d, UINT start, const float *data, UINT count)
{
    if (!Mine())
    {
        return g_setVertexShaderConstantF(d, start, data, count);
    }
    SetWords(kVertexConstants, g_shadow.vertexConstant, start * 4ULL, reinterpret_cast<const std::uint32_t *>(data),
             count * 4ULL);
    return g_setVertexShaderConstantF(d, start, data, count);
}

HRESULT STDMETHODCALLTYPE SetPixelShaderConstantFHook(Dev *d, UINT start, const float *data, UINT count)
{
    if (!Mine())
    {
        return g_setPixelShaderConstantF(d, start, data, count);
    }
    SetWords(kPixelConstants, g_shadow.pixelConstant, start * 4ULL, reinterpret_cast<const std::uint32_t *>(data),
             count * 4ULL);
    return g_setPixelShaderConstantF(d, start, data, count);
}

HRESULT STDMETHODCALLTYPE SetStreamSourceHook(Dev *d, UINT stream, IDirect3DVertexBuffer9 *buffer, UINT offset,
                                              UINT stride)
{
    if (!Mine())
    {
        return g_setStreamSource(d, stream, buffer, offset, stride);
    }
    const std::size_t slot = (stream < 16 ? stream : 15) * 3;
    const std::uint64_t values[3] = {reinterpret_cast<std::uint64_t>(buffer), offset, stride};
    for (std::size_t i = 0; i < 3; ++i)
    {
        Put(kGeometry, slot + i, g_shadow.stream[slot + i], values[i]);
        g_shadow.stream[slot + i] = values[i];
    }
    return g_setStreamSource(d, stream, buffer, offset, stride);
}

HRESULT STDMETHODCALLTYPE SetIndicesHook(Dev *d, IDirect3DIndexBuffer9 *indices)
{
    if (!Mine())
    {
        return g_setIndices(d, indices);
    }
    const auto v = reinterpret_cast<std::uint64_t>(indices);
    Put(kGeometry, 100, g_shadow.single[3], v);
    g_shadow.single[3] = v;
    return g_setIndices(d, indices);
}

HRESULT STDMETHODCALLTYPE SetVertexDeclarationHook(Dev *d, IDirect3DVertexDeclaration9 *declaration)
{
    if (!Mine())
    {
        return g_setVertexDeclaration(d, declaration);
    }
    const auto v = reinterpret_cast<std::uint64_t>(declaration);
    Put(kGeometry, 101, g_shadow.single[2], v);
    g_shadow.single[2] = v;
    return g_setVertexDeclaration(d, declaration);
}

HRESULT STDMETHODCALLTYPE SetRenderTargetHook(Dev *d, DWORD index, IDirect3DSurface9 *target)
{
    if (!Mine())
    {
        return g_setRenderTarget(d, index, target);
    }
    const std::size_t slot = index < 4 ? index : 3;
    const auto v = reinterpret_cast<std::uint64_t>(target);
    Put(kTargets, slot, g_shadow.target[slot], v);
    g_shadow.target[slot] = v;
    return g_setRenderTarget(d, index, target);
}

HRESULT STDMETHODCALLTYPE SetDepthStencilSurfaceHook(Dev *d, IDirect3DSurface9 *surface)
{
    if (!Mine())
    {
        return g_setDepthStencilSurface(d, surface);
    }
    const auto v = reinterpret_cast<std::uint64_t>(surface);
    Put(kTargets, 10, g_shadow.single[4], v);
    g_shadow.single[4] = v;
    return g_setDepthStencilSurface(d, surface);
}

HRESULT STDMETHODCALLTYPE SetViewportHook(Dev *d, const D3DVIEWPORT9 *viewport)
{
    if (!Mine())
    {
        return g_setViewport(d, viewport);
    }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 6; ++i)
    {
        v = Mix(i, v ^ reinterpret_cast<const std::uint32_t *>(viewport)[i]);
    }
    Put(kTargets, 11, g_shadow.single[5], v);
    g_shadow.single[5] = v;
    return g_setViewport(d, viewport);
}

HRESULT STDMETHODCALLTYPE SetScissorRectHook(Dev *d, const RECT *rect)
{
    if (!Mine())
    {
        return g_setScissorRect(d, rect);
    }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        v = Mix(i, v ^ reinterpret_cast<const std::uint32_t *>(rect)[i]);
    }
    Put(kTargets, 12, g_shadow.single[6], v);
    g_shadow.single[6] = v;
    return g_setScissorRect(d, rect);
}

void Record(std::uint64_t draw)
{
    g_current.push_back(DrawRecord{g_shadow.hash, draw});
    if (g_capture.load(std::memory_order_relaxed) == 2)
    {
        Snapshot snap;
        snap.renderState = g_shadow.renderState;
        snap.sampler = g_shadow.sampler;
        snap.texture = g_shadow.texture;
        snap.vertexConstant = g_shadow.vertexConstant;
        snap.pixelConstant = g_shadow.pixelConstant;
        snap.stream = g_shadow.stream;
        snap.single = g_shadow.single;
        snap.target = g_shadow.target;
        snap.draw = draw;
        snap.pad = 0;
        g_snapshots.push_back(snap);
    }
}

void WriteCapture()
{
    const auto file = dao::LogDirectory() / (g_captureLabel + ".snap");
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    const auto n = static_cast<std::uint32_t>(g_snapshots.size());
    out.write(reinterpret_cast<const char *>(&n), sizeof n);
    out.write(reinterpret_cast<const char *>(g_snapshots.data()),
              static_cast<std::streamsize>(g_snapshots.size() * sizeof(Snapshot)));
    spdlog::info("verify: {} draws written to {}", n, file.string());
    g_snapshots.clear();
}

HRESULT STDMETHODCALLTYPE DrawPrimitiveHook(Dev *d, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    if (!Mine())
    {
        return g_drawPrimitive(d, type, start, count);
    }
    Record(Mix(Mix(Mix(1, type), start), count));
    return g_drawPrimitive(d, type, start, count);
}

HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveHook(Dev *d, D3DPRIMITIVETYPE type, INT base, UINT minIndex, UINT vertices,
                                                   UINT startIndex, UINT primitives)
{
    if (!Mine())
    {
        return g_drawIndexedPrimitive(d, type, base, minIndex, vertices, startIndex, primitives);
    }
    Record(Mix(Mix(Mix(Mix(Mix(Mix(2, type), static_cast<std::uint32_t>(base)), minIndex), vertices), startIndex),
               primitives));
    return g_drawIndexedPrimitive(d, type, base, minIndex, vertices, startIndex, primitives);
}

void EndFrame()
{
    g_renderThread.store(GetCurrentThreadId());
    if (g_capture.load() == 2)
    {
        WriteCapture();
        g_capture.store(0);
    }
    else if (g_capture.load() == 1)
    {
        g_snapshots.clear();
        g_capture.store(2); // the frame after this boundary is the one kept
    }
    {
        const std::lock_guard lock(g_mutex);
        std::uint64_t frame = 0;
        for (const DrawRecord &rec : g_current)
        {
            for (std::uint64_t h : rec.state)
            {
                frame = Mix(frame, h);
            }
            frame = Mix(frame, rec.draw);
        }
        g_lastFrameHash = frame;
        g_last.swap(g_current);
        g_current.clear();
        g_frames.fetch_add(1, std::memory_order_relaxed);
    }
}

HRESULT STDMETHODCALLTYPE PresentHook(Dev *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    EndFrame();
    return g_present(d, a, b, w, r);
}

HRESULT STDMETHODCALLTYPE SwapPresentHook(IDirect3DSwapChain9 *c, const RECT *a, const RECT *b, HWND w,
                                          const RGNDATA *r, DWORD flags)
{
    EndFrame();
    return g_swapPresent(c, a, b, w, r, flags);
}

std::vector<void *> g_patched; // device methods already detoured: a driver's methods are shared by its devices

template <class F> void Attach(F &original, void *vtableEntry, F hook)
{
    for (void *p : g_patched)
    {
        if (p == vtableEntry)
        {
            return;
        }
    }
    g_patched.push_back(vtableEntry);
    original = reinterpret_cast<F>(vtableEntry);
    DetourAttach(reinterpret_cast<PVOID *>(&original), reinterpret_cast<PVOID>(hook));
}

// Detours the methods of the device's own implementation, whichever d3d9 class its flags chose.
void HookDevice(IDirect3DDevice9 *device)
{
    void **vt = *reinterpret_cast<void ***>(device);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    Attach(g_setRenderState, vt[57], SetRenderStateHook);
    Attach(g_setSamplerState, vt[69], SetSamplerStateHook);
    Attach(g_setTexture, vt[65], SetTextureHook);
    Attach(g_setVertexShader, vt[92], SetVertexShaderHook);
    Attach(g_setPixelShader, vt[107], SetPixelShaderHook);
    Attach(g_setVertexShaderConstantF, vt[94], SetVertexShaderConstantFHook);
    Attach(g_setPixelShaderConstantF, vt[109], SetPixelShaderConstantFHook);
    Attach(g_setStreamSource, vt[100], SetStreamSourceHook);
    Attach(g_setIndices, vt[104], SetIndicesHook);
    Attach(g_setVertexDeclaration, vt[87], SetVertexDeclarationHook);
    Attach(g_setRenderTarget, vt[37], SetRenderTargetHook);
    Attach(g_setDepthStencilSurface, vt[39], SetDepthStencilSurfaceHook);
    Attach(g_setViewport, vt[47], SetViewportHook);
    Attach(g_setScissorRect, vt[75], SetScissorRectHook);
    Attach(g_drawPrimitive, vt[81], DrawPrimitiveHook);
    Attach(g_drawIndexedPrimitive, vt[82], DrawIndexedPrimitiveHook);
    Attach(g_present, vt[17], PresentHook);
    IDirect3DSwapChain9 *chain = nullptr;
    if (SUCCEEDED(device->GetSwapChain(0, &chain)) && chain != nullptr)
    {
        Attach(g_swapPresent, (*reinterpret_cast<void ***>(chain))[3], SwapPresentHook);
        chain->Release();
    }
    const LONG result = DetourTransactionCommit();
    spdlog::info("verify: device {} methods hooked ({} patched in all): {}", static_cast<void *>(device), g_patched.size(),
                 result == NO_ERROR ? "ok" : "failed");
}

// IDirect3D9::CreateDevice (slot 16): the game's own device is hooked as it is made.
HRESULT(STDMETHODCALLTYPE *g_createDevice)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *,
                                           IDirect3DDevice9 **);

HRESULT STDMETHODCALLTYPE CreateDeviceHook(IDirect3D9 *api, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
                                           D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **device)
{
    const HRESULT hr = g_createDevice(api, adapter, type, window, flags, pp, device);
    if (SUCCEEDED(hr) && device != nullptr && *device != nullptr)
    {
        spdlog::info("verify: the game made a device, flags {:#x}", flags);
        HookDevice(*device);
    }
    return hr;
}

void Watch()
{
    std::uint64_t reported = 0;
    for (;;)
    {
        Sleep(500);
        {
            std::ifstream in(g_dir / "dump.txt");
            std::string label;
            if (in >> label)
            {
                in.close();
                std::filesystem::remove(g_dir / "dump.txt");
                g_captureLabel = label;
                g_capture.store(1);
            }
        }
        const auto frames = g_frames.load();
        if (frames != reported && frames % 1 == 0)
        {
            reported = frames;
            static std::uint64_t lastLogged = 0;
            if (GetTickCount64() - lastLogged > 2000)
            {
                lastLogged = GetTickCount64();
                const std::lock_guard lock(g_mutex);
                spdlog::info("verify: frame {}: {} draws, hash {:016x}", frames, g_last.size(), g_lastFrameHash);
            }
        }
    }
}

} // namespace

bool InstallDeviceVerifier(const std::filesystem::path &dir)
{
    g_dir = dir;
    HMODULE d3d = LoadLibraryW(L"d3d9.dll");
    if (d3d == nullptr)
    {
        return false;
    }
    using CreateFn = IDirect3D9 *(WINAPI *)(UINT);
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(d3d, "Direct3DCreate9"));
    IDirect3D9 *api = create != nullptr ? create(D3D_SDK_VERSION) : nullptr;
    if (api == nullptr)
    {
        return false;
    }
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    Attach(g_createDevice, (*reinterpret_cast<void ***>(api))[16], CreateDeviceHook);
    const LONG result = DetourTransactionCommit();
    api->Release();
    spdlog::info("verify: CreateDevice hooked, {}", result == NO_ERROR ? "ok" : "failed");
    std::thread(Watch).detach();
    return result == NO_ERROR;
}
