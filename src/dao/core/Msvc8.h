// The C++ standard library types the engine was built with, as they lie in
// memory: MSVC 2005's std::wstring and std::vector. The game passes these
// between its functions (the script VM's strings, an effect's parameters),
// and a plugin built with a later MSVC cannot use its own std::wstring for
// them: the layout has changed since.
//
// Read 2026-09-30. The VM's string is 0x1C bytes (StackPushString,
// 0x00A2D730, allocates that much and copy-constructs it). Its constructor
// (0x004C0D90) sets +0x18 to 7, +0x14 to 0 and a 16-bit 0 at +4: capacity 7
// wide characters in an inline buffer of 8, length 0, empty. Its destructor
// (0x004C0DB0) frees the pointer at +4 only when the capacity is 8 or more.
// That is MSVC 8's basic_string<wchar_t>: an empty allocator padded to 4
// bytes, then a 16-byte union of the inline buffer and the heap pointer,
// then the length and the capacity.
//
// std::vector: the effect's parameter getters (CGameEffect::GetInteger,
// 0x00A5A220) read the first element's pointer at +4 and the end of the
// elements at +8 of a vector and index from +4: an allocator word, then
// first, last and the end of the storage.
//
// Only the layout is here; making or freeing one goes through the engine's
// own functions (dao/String.h), since the memory is the engine's heap's.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dao::core
{

struct WStringRep
{
    static constexpr std::uint32_t kInlineCapacity = 7; // wide characters, the 8th the terminator

    std::uint32_t allocator = 0;
    union {
        wchar_t buffer[kInlineCapacity + 1];
        const wchar_t *pointer;
    } data{};
    std::uint32_t size = 0;
    std::uint32_t capacity = kInlineCapacity;

    const wchar_t *Characters() const
    {
        return capacity > kInlineCapacity ? data.pointer : data.buffer;
    }

    std::wstring_view View() const
    {
        return {Characters(), size};
    }

    // A string the engine can read from, not free or change: the text in
    // the inline buffer when it fits, else a pointer to it, with the
    // capacity set to say so. For passing text to engine code that copies
    // it (StackPushString, operator=); `text` must outlive that call.
    static WStringRep Borrow(std::wstring_view text)
    {
        WStringRep rep;
        rep.size = static_cast<std::uint32_t>(text.size());
        if (text.size() <= kInlineCapacity)
        {
            text.copy(rep.data.buffer, text.size());
            rep.data.buffer[text.size()] = L'\0';
        }
        else
        {
            rep.data.pointer = text.data();
            rep.capacity = rep.size;
        }
        return rep;
    }
};
static_assert(sizeof(WStringRep) == 0x1C, "the engine allocates 0x1C bytes for a string");
static_assert(offsetof(WStringRep, size) == 0x14 && offsetof(WStringRep, capacity) == 0x18);

template <class T> struct VectorRep
{
    std::uint32_t allocator;
    T *first;
    T *last;
    T *end;

    std::span<const T> View() const
    {
        return first == nullptr ? std::span<const T>{} : std::span<const T>{first, last};
    }
};
static_assert(sizeof(VectorRep<int>) == 0x10);

} // namespace dao::core
