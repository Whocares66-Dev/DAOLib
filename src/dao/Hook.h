// A detour on a function of the game's, through Microsoft Detours: the
// function's first bytes are replaced by a jump to ours, the displaced
// prologue is relocated into a trampoline, and Original() is that
// trampoline, callable as the function was.
//
// Calling conventions matter on x86, where the game is. A free function or
// a static method is __cdecl unless the engine's code says otherwise. A
// member function is __thiscall, with `this` in ecx, which a free function
// cannot declare; write the hook as __fastcall with `this` first and an
// unused int second (edx), and declare Fn the same way:
//
//     using HandleFn = bool(__fastcall*)(Controller* self, int, const Message& m);
//     dao::Detour<HandleFn> g_handle;
//     bool __fastcall Hooked(Controller* self, int edx, const Message& m) { ... }
//     g_handle.Attach(dao::addr::kHandle, &Hooked);
//     ... g_handle.Original()(self, 0, m);
#pragma once

#include "dao/Module.h"

#include <Windows.h>

#include <detours/detours.h>

#include <cstdint>

namespace dao
{

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

    // Attach to the function with an ID from dao/Addresses.h.
    // False, with nothing changed, if the running build has no such function
    // (Module::Address gives zero) or Detours refused: an address that is not
    // code, or one already patched in a way it cannot follow.
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

    // The function as it was, to call from the hook. Null until Attach.
    Fn Original() const
    {
        return original_;
    }

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
