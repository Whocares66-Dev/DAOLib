// The C++ standard library types the engine was built with, as they lie in
// memory: MSVC 2005's std::wstring, std::vector and std::map. The game passes these
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
// std::map, read 2026-10-02 from the GUI system's map of scenes (its
// SceneIdByName, 0x00523ED0, and AddScene): a word, the head node at +4 and
// the count at +8. A node is its left, parent and right, then the key and
// the value, then its colour and a byte that is set only in the head. The
// head is the end and every missing child: its left is the least node, its
// parent the root. SceneIdByName starts at the head's left and steps
// (0x0067A970) until it is back at the head.
//
// Only the layout is here; making or freeing one goes through the engine's
// own functions (dao/String.h), since the memory is the engine's heap's.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace dao::core
{

// An engine string's inline buffer, in wide characters, the 8th the
// terminator: what a string found in bytes is judged by (StackText.h).
inline constexpr std::uint32_t kWStringInlineCapacity = 7;

// The overlays below are the engine's memory read in place, through pointers
// of the process's own width, so they mean something only in the game's
// 32-bit process. A 64-bit build of core (a tool that reads the game's files,
// DAOLib #106) has no game memory to read: it declares none of them, and a use
// of one there fails to compile instead of reading the wrong offsets.
#if UINTPTR_MAX == UINT32_MAX

struct WStringRep
{
    static constexpr std::uint32_t kInlineCapacity = kWStringInlineCapacity;

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

template <class Key, class Value> struct TreeNodeRep
{
    TreeNodeRep *left;
    TreeNodeRep *parent;
    TreeNodeRep *right;
    Key key;
    Value value;
    char color;
    char isNil; // the head's alone
};
using SceneNodeRep = TreeNodeRep<std::int32_t, void *>; // the GUI system's, as an example
static_assert(offsetof(SceneNodeRep, key) == 0xC);
static_assert(offsetof(SceneNodeRep, value) == 0x10);
static_assert(offsetof(SceneNodeRep, isNil) == 0x15);

template <class Key, class Value> struct TreeRep
{
    using Node = TreeNodeRep<Key, Value>;

    std::uint32_t allocator;
    Node *head;
    std::uint32_t size;

    // Each entry in key order, as the engine's own iterator steps (MSVC 8's
    // _Tree::_Inc), and never more than the count says, so that a broken
    // tree cannot keep the walk going.
    template <class Fn> void ForEach(Fn &&fn) const
    {
        if (head == nullptr)
        {
            return;
        }
        const Node *node = head->left;
        for (std::uint32_t i = 0; i < size && node != nullptr && node->isNil == 0; ++i)
        {
            fn(node->key, node->value);
            node = Next(node);
        }
    }

  private:
    static const Node *Next(const Node *node)
    {
        if (node->right->isNil == 0)
        {
            node = node->right;
            while (node->left->isNil == 0)
            {
                node = node->left;
            }
            return node;
        }
        const Node *parent = node->parent;
        while (parent->isNil == 0 && node == parent->right)
        {
            node = parent;
            parent = parent->parent;
        }
        return parent;
    }
};
static_assert(sizeof(TreeRep<std::int32_t, void *>) == 0xC);

#endif // UINTPTR_MAX == UINT32_MAX

// A std::list of 32-bit values, as MSVC 8 lays it out: an allocator word,
// the head node at +4 (its own value unused) and the count at +8; a node
// is the next node, the previous one, then the value. A creature's cooldown
// manager keeps the records that tick in one (DAOLib #38).
inline constexpr std::uint32_t kListHead = 4;
inline constexpr std::uint32_t kListSize = 8;
inline constexpr std::uint32_t kListNodeValue = 8;

// The values of the list at `list`, front to back, read through `read`
// (an address to its 32-bit word, nothing where it cannot be read): as the
// engine's own iterator steps, from the head's next round to the head, and
// never more than the count says or past a word that will not read, so a
// broken list cannot keep the walk going. Empty for a list that will not
// read.
template <class Read> std::vector<std::uint32_t> ListValues(std::uint32_t list, Read &&read)
{
    std::vector<std::uint32_t> values;
    const std::optional<std::uint32_t> head = read(list + kListHead);
    const std::optional<std::uint32_t> size = read(list + kListSize);
    if (!head || !size || *head == 0)
    {
        return values;
    }
    std::optional<std::uint32_t> node = read(*head);
    for (std::uint32_t i = 0; i < *size && node && *node != 0 && *node != *head; ++i)
    {
        const std::optional<std::uint32_t> value = read(*node + kListNodeValue);
        if (!value)
        {
            break;
        }
        values.push_back(*value);
        node = read(*node);
    }
    return values;
}

} // namespace dao::core
