// Replace one of the game's functions with your own, and call the original from it, using
// dao::Detour; or replace a COM object's method by its vtable slot, using dao::VtableHook.
#pragma once

#include "dao/Module.h"

#include <Windows.h>

#include <detours/detours.h>

#include <cstddef>
#include <cstdint>

// Notes on Detour: Detours replaces the function's first bytes with a jump to the hook
// and relocates the displaced prologue into a trampoline; Original() is that
// trampoline, callable as the function was.

namespace dao
{

// A detour on one of the game's functions, placed with Microsoft Detours.
//
// `Fn` is the function's pointer type, and its calling convention must match the game's:
// - A free function or a static method is __cdecl, unless the game's code says otherwise:
//   a `ret N` at its end means __stdcall.
// - A member function is __thiscall, which a free function cannot declare. Write the hook
//   as __fastcall with `this` first and an unused int second (it receives edx), and
//   declare `Fn` the same way.
//
// With the wrong convention the stack is off when the function returns, so the crash comes
// after the hook, not in it.
//
//     using HandleFn = bool(__fastcall*)(Controller* self, int, const Message& m);
//     dao::Detour<HandleFn> g_handle;
//     bool __fastcall Hooked(Controller* self, int edx, const Message& m) { ... }
//     g_handle.Attach(dao::addr::kHandle, &Hooked);
//     ... g_handle.Original()(self, 0, m);
//
// The destructor removes the detour. Exceptions thrown from a hook are not caught, so guard
// the hook's body yourself (dev/PLUGINS.md).
template <class Fn> class Detour
{
  public:
    Detour() = default;
    Detour(const Detour &) = delete;
    Detour &operator=(const Detour &) = delete;
    ~Detour()
    {
        Detach();
    }

    // Replaces the function with `hook`.
    // @param id the function's ID from dao/Addresses.h
    // @param hook the replacement, of the same type as the function
    // @return true if the detour was placed. False, with nothing changed, if this detour is
    // already attached, the running build has no such function, or Detours rejected the
    // address.
    bool Attach(AddressId id, Fn hook)
    {
        return AttachTo(reinterpret_cast<Fn>(Module::Address(id)), hook);
    }

    // Places the detour on a function by where it is: one that is not the game's own, a
    // system DLL's export or a method read from an object's vtable.
    // @return false if `target` is null, a detour is already placed, or Detours refuses
    bool AttachTo(Fn target, Fn hook)
    {
        if (attached_ || target == nullptr)
        {
            return false;
        }
        original_ = target;
        hook_ = hook;
        if (DetourTransactionBegin() != NO_ERROR)
        {
            return false;
        }
        DetourUpdateThread(GetCurrentThread());
        if (DetourAttach(reinterpret_cast<PVOID *>(&original_), reinterpret_cast<PVOID>(hook_)) != NO_ERROR ||
            DetourTransactionCommit() != NO_ERROR)
        {
            DetourTransactionAbort();
            original_ = nullptr;
            return false;
        }
        attached_ = true;
        return true;
    }

    // Removes the detour and restores the function. Does nothing if it is not attached.
    void Detach()
    {
        if (!attached_)
        {
            return;
        }
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(reinterpret_cast<PVOID *>(&original_), reinterpret_cast<PVOID>(hook_));
        DetourTransactionCommit();
        attached_ = false;
        original_ = nullptr;
    }

    // Returns the original function, for the hook to call. Null until Attach succeeds.
    Fn Original() const
    {
        return original_;
    }

    // Returns true while the detour is in place.
    bool IsAttached() const
    {
        return attached_;
    }

  private:
    Fn original_ = nullptr;
    Fn hook_ = nullptr;
    bool attached_ = false;
};

namespace detail
{

// Swaps the pointer at `slot` from `expected` to `desired`, under VirtualProtect, as one interlocked
// exchange.
// @return false if the slot did not hold `expected` or its page could not be made writable
bool SwapSlot(void **slot, void *expected, void *desired) noexcept;

} // namespace detail

// A hook on one method of a COM object, Direct3D's first: the object's vtable entry for the slot
// is pointed at the hook.
//
// The game calls Direct3D through the vtables, so the hook sees every call. Use it, not Detour,
// for a method of a system object: an overlay (a frame counter, a capture tool) that hooked the
// method's code inline and later writes its bytes back erases a Detour placed after it, but not a
// vtable entry. The vtable is the class's, shared by every object of the class. `Fn` is the
// method's pointer type with the object first, __stdcall for COM.
//
//     using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND,
//                                           const RGNDATA*);
//     dao::VtableHook<PresentFn> g_present;
//     g_present.Attach(device, 17, &Present);
//     ... g_present.Original()(device, source, dest, window, dirty);
//
// The destructor removes the hook.
template <class Fn> class VtableHook
{
  public:
    VtableHook() = default;
    VtableHook(const VtableHook &) = delete;
    VtableHook &operator=(const VtableHook &) = delete;
    ~VtableHook()
    {
        Detach();
    }

    // Points the object's vtable entry for `slot` at `hook`.
    // @return false, with nothing changed, if the hook is already attached, `object` is null, or
    // the entry could not be written
    bool Attach(void *object, std::size_t slot, Fn hook)
    {
        if (attached_ || object == nullptr)
        {
            return false;
        }
        void **entry = *static_cast<void ***>(object) + slot;
        void *was = *entry;
        if (!detail::SwapSlot(entry, was, reinterpret_cast<void *>(hook)))
        {
            return false;
        }
        entry_ = entry;
        original_ = reinterpret_cast<Fn>(was);
        hook_ = hook;
        attached_ = true;
        return true;
    }

    // Puts the entry back as it was. Does nothing if the hook is not attached, and leaves an entry
    // another hook has since replaced, whose own original is this hook.
    void Detach()
    {
        if (!attached_)
        {
            return;
        }
        detail::SwapSlot(entry_, reinterpret_cast<void *>(hook_), reinterpret_cast<void *>(original_));
        attached_ = false;
        entry_ = nullptr;
        original_ = nullptr;
    }

    // Returns the method the entry held, for the hook to call. Null until Attach succeeds.
    Fn Original() const
    {
        return original_;
    }

    // Returns true while the hook is in place.
    bool IsAttached() const
    {
        return attached_;
    }

  private:
    void **entry_ = nullptr;
    Fn original_ = nullptr;
    Fn hook_ = nullptr;
    bool attached_ = false;
};

} // namespace dao
