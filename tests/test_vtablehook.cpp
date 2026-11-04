// dao::VtableHook over a vtable in a read-only page of the test process: the entry swapped and put
// back, the original callable, an entry another hook has since replaced left as it is, and the
// page's protection kept (DAOLib #319).
#include "dao/Hook.h"

#include <Windows.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>

namespace
{

using MethodFn = int(__stdcall *)(void *self, int value);

int __stdcall Twice(void * /*self*/, int value)
{
    return value * 2;
}

int __stdcall Thrice(void * /*self*/, int value)
{
    return value * 3;
}

dao::VtableHook<MethodFn> g_hook;

int __stdcall Hooked(void *self, int value)
{
    return g_hook.Original()(self, value) + 1;
}

int __stdcall Other(void *self, int value)
{
    return -Twice(self, value);
}

// A vtable of two methods in a page of its own, read-only as a module's vtables are, and an object
// whose first word points at it.
struct Fixture
{
    void **table = nullptr;
    void *object = nullptr;

    Fixture()
    {
        table = static_cast<void **>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (table == nullptr)
        {
            return;
        }
        table[0] = reinterpret_cast<void *>(&Thrice);
        table[1] = reinterpret_cast<void *>(&Twice);
        DWORD was = 0;
        VirtualProtect(static_cast<void *>(table), 4096, PAGE_READONLY, &was);
        object = static_cast<void *>(&table);
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
    ~Fixture()
    {
        if (table != nullptr)
        {
            VirtualFree(static_cast<void *>(table), 0, MEM_RELEASE);
        }
    }

    int Call(std::size_t slot, int value) const
    {
        return reinterpret_cast<MethodFn>(table[slot])(object, value);
    }

    DWORD Protection() const
    {
        MEMORY_BASIC_INFORMATION info{};
        VirtualQuery(static_cast<const void *>(table), &info, sizeof info);
        return info.Protect;
    }
};

} // namespace

TEST_CASE("A vtable hook swaps the entry, calls through to the original and puts it back")
{
    const Fixture fixture;
    REQUIRE(fixture.table != nullptr);
    REQUIRE(g_hook.Attach(fixture.object, 1, &Hooked));
    CHECK(g_hook.IsAttached());
    CHECK(g_hook.Original() == &Twice);
    CHECK(fixture.Call(1, 5) == 11);
    CHECK(fixture.Call(0, 5) == 15); // the other slot is left
    CHECK(fixture.Protection() == PAGE_READONLY);
    CHECK_FALSE(g_hook.Attach(fixture.object, 1, &Hooked)); // already attached
    g_hook.Detach();
    CHECK_FALSE(g_hook.IsAttached());
    CHECK(g_hook.Original() == nullptr);
    CHECK(fixture.Call(1, 5) == 10);
    CHECK(fixture.Protection() == PAGE_READONLY);
}

TEST_CASE("A vtable hook leaves an entry another hook has since replaced")
{
    const Fixture fixture;
    REQUIRE(fixture.table != nullptr);
    REQUIRE(g_hook.Attach(fixture.object, 1, &Hooked));
    REQUIRE(
        dao::detail::SwapSlot(&fixture.table[1], reinterpret_cast<void *>(&Hooked), reinterpret_cast<void *>(&Other)));
    g_hook.Detach();
    CHECK(fixture.table[1] == reinterpret_cast<void *>(&Other));
}

TEST_CASE("A vtable hook refuses a null object, and a swap refuses an entry not as expected")
{
    const Fixture fixture;
    REQUIRE(fixture.table != nullptr);
    CHECK_FALSE(g_hook.Attach(nullptr, 1, &Hooked));
    CHECK_FALSE(
        dao::detail::SwapSlot(&fixture.table[1], reinterpret_cast<void *>(&Thrice), reinterpret_cast<void *>(&Other)));
    CHECK(fixture.table[1] == reinterpret_cast<void *>(&Twice));
}
