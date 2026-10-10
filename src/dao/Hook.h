// Replace one of the game's functions with your own, and call the original from it, using
// dao::Detour.
#pragma once

#include "dao/Module.h"

#include <Windows.h>

#include <detours/detours.h>

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
        if (attached_)
        {
            return false;
        }
        original_ = reinterpret_cast<Fn>(Module::Address(id));
        if (original_ == nullptr)
        {
            return false;
        }
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

} // namespace dao
