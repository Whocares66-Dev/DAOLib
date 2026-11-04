// dao::VtableHook's write of a vtable entry. A class's vtable is in its module's read-only data,
// so the page is made writable for the one pointer and its protection put back; the pointer is
// exchanged with an interlocked compare, so a call on another thread reads the old entry or the
// new one, never half of each (DAOLib #319).
#include "dao/Hook.h"

namespace dao::detail
{

bool SwapSlot(void **slot, void *expected, void *desired) noexcept
{
    void *const page = static_cast<void *>(slot);
    DWORD was = 0;
    if (slot == nullptr || VirtualProtect(page, sizeof(void *), PAGE_READWRITE, &was) == FALSE)
    {
        return false;
    }
    const bool swapped = InterlockedCompareExchangePointer(slot, desired, expected) == expected;
    VirtualProtect(page, sizeof(void *), was, &was);
    return swapped;
}

} // namespace dao::detail
