#include "dao/core/CommandWord.h"
#include "dao/core/FieldPath.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <map>

using dao::core::CommandType;
using dao::core::FieldPath;
using dao::core::SequenceOf;
using dao::core::TypeOf;

namespace
{

// A fake memory: words at addresses; reads outside what is there fail.
struct Memory
{
    std::map<std::uintptr_t, std::uint32_t> words;

    bool operator()(std::uintptr_t address, void *out, std::size_t size) const
    {
        for (std::size_t i = 0; i < size; i += 4)
        {
            const auto it = words.find(address + i);
            if (it == words.end())
            {
                return false;
            }
            std::memcpy(static_cast<std::byte *>(out) + i, &it->second, size - i < 4 ? size - i : 4);
        }
        return true;
    }
};

} // namespace

TEST_CASE("a field one offset from its object")
{
    // CCCreature+0x7EC, what GetPackageAI returns.
    const Memory memory{{{0x1000 + 0x7EC, 42}}};
    constexpr FieldPath<std::int32_t, 1> packageAI{{0x7EC}};
    CHECK(packageAI.Read(0x1000, memory) == 42);
}

TEST_CASE("a field through a pointer")
{
    // CCCreature+0xC60 -> +0x0, what GetCombatState returns.
    const Memory memory{{{0x1000 + 0xC60, 0x5000}, {0x5000, 1}}};
    constexpr FieldPath<std::int32_t, 2> combatState{{0xC60, 0x0}};
    CHECK(combatState.Read(0x1000, memory) == 1);
}

TEST_CASE("a null pointer on the way, or memory that is not there, reads nothing")
{
    constexpr FieldPath<std::int32_t, 2> combatState{{0xC60, 0x0}};
    CHECK_FALSE(combatState.Read(0x1000, Memory{{{0x1000 + 0xC60, 0}}}).has_value());
    CHECK_FALSE(combatState.Read(0x1000, Memory{}).has_value());
    CHECK_FALSE(combatState.Read(0x1000, Memory{{{0x1000 + 0xC60, 0x5000}}}).has_value());
}

TEST_CASE("a narrow field reads its own width")
{
    // CCPlaceable+0x638, a byte (GetPlaceableBaseType); the word holding it
    // has other bytes set above it.
    const Memory memory{{{0x2000 + 0x638, 0xAABBCC07}}};
    constexpr FieldPath<std::uint8_t, 1> baseType{{0x638}};
    CHECK(baseType.Read(0x2000, memory) == 7);
}

TEST_CASE("a command's first word holds its type and its sequence number")
{
    // Words logged in play: an attack, a script's wait, the player's drive.
    CHECK(TypeOf(0x00010033) == CommandType::Attack);
    CHECK(SequenceOf(0x00010033) == 0x33);
    CHECK(TypeOf(0x000d0076) == CommandType::Wait);
    CHECK(TypeOf(0x18210073) == CommandType::Drive);
    CHECK(SequenceOf(0x18210073) == 0x73);
}
