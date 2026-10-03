#include "dao/core/GdaHash.h"
#include "dao/core/Msvc8.h"
#include "dao/core/ScriptArray.h"
#include "dao/core/VmStack.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

using dao::core::VectorRep;
using dao::core::WStringRep;

TEST_CASE("an empty engine string is what its constructor makes", "[msvc8]")
{
    const WStringRep empty;
    CHECK(empty.size == 0);
    CHECK(empty.capacity == 7);
    CHECK(empty.View().empty());
}

TEST_CASE("a short string borrowed is copied into the inline buffer", "[msvc8]")
{
    const std::wstring text = L"sword";
    const WStringRep rep = WStringRep::Borrow(text);
    CHECK(rep.capacity == 7);
    CHECK(rep.size == 5);
    CHECK(rep.View() == text);
    CHECK(rep.Characters() != text.data());
    CHECK(rep.data.buffer[5] == L'\0');
}

TEST_CASE("seven characters still fit inline, eight do not", "[msvc8]")
{
    const std::wstring seven = L"1234567";
    const WStringRep inline7 = WStringRep::Borrow(seven);
    CHECK(inline7.capacity == 7);
    CHECK(inline7.View() == seven);

    const std::wstring eight = L"12345678";
    const WStringRep pointed = WStringRep::Borrow(eight);
    CHECK(pointed.capacity == 8);
    CHECK(pointed.Characters() == eight.data());
    CHECK(pointed.View() == eight);
}

TEST_CASE("a long string borrowed points at the text", "[msvc8]")
{
    const std::wstring text = L"gen00fl_dog_with_a_long_tag";
    const WStringRep rep = WStringRep::Borrow(text);
    CHECK(rep.Characters() == text.data());
    CHECK(rep.size == text.size());
    CHECK(rep.capacity >= 8);
    CHECK(rep.View() == text);
}

TEST_CASE("an engine vector's elements run from first to last", "[msvc8]")
{
    std::array<int, 3> values{7, 8, 9};
    const VectorRep<int> rep{0, values.data(), values.data() + 3, values.data() + 3};
    CHECK(rep.View().size() == 3);
    CHECK(rep.View()[2] == 9);

    const VectorRep<int> none{0, nullptr, nullptr, nullptr};
    CHECK(none.View().empty());
}

namespace
{

// An engine map of ints, built as MSVC 8 links one: the head is the end and
// every missing child.
struct TestTree
{
    using Node = dao::core::TreeNodeRep<std::int32_t, int>;
    std::array<Node, 6> nodes{}; // the head first

    // 4 at the root; 2 under it with 1 and 3; 6 on its right.
    TestTree()
    {
        Node *head = &nodes[0];
        for (Node &node : nodes)
        {
            node.left = node.right = node.parent = head;
        }
        head->isNil = 1;
        const auto set = [&](std::size_t at, std::int32_t key, Node *parent, Node *left, Node *right) {
            nodes.at(at) = Node{left, parent, right, key, key * 10, 0, 0};
        };
        set(1, 4, head, &nodes[2], &nodes[5]);
        set(2, 2, &nodes[1], &nodes[3], &nodes[4]);
        set(3, 1, &nodes[2], head, head);
        set(4, 3, &nodes[2], head, head);
        set(5, 6, &nodes[1], head, head);
        head->parent = &nodes[1];
        head->left = &nodes[3];
        head->right = &nodes[5];
    }
    TestTree(const TestTree &) = delete;
    TestTree &operator=(const TestTree &) = delete;

    dao::core::TreeRep<std::int32_t, int> Map(std::uint32_t size)
    {
        return {0, &nodes[0], size};
    }
};

} // namespace

TEST_CASE("an engine map is walked in key order", "[msvc8]")
{
    TestTree tree;
    std::vector<std::int32_t> keys;
    tree.Map(5).ForEach([&](std::int32_t key, int value) {
        CHECK(value == key * 10);
        keys.push_back(key);
    });
    CHECK(keys == std::vector<std::int32_t>{1, 2, 3, 4, 6});
}

TEST_CASE("an engine map's walk stops at its count and at an empty map", "[msvc8]")
{
    TestTree tree;
    std::vector<std::int32_t> keys;
    tree.Map(2).ForEach([&](std::int32_t key, int) { keys.push_back(key); });
    CHECK(keys == std::vector<std::int32_t>{1, 2});

    TestTree::Node head{};
    head.left = head.right = head.parent = &head;
    head.isNil = 1;
    bool any = false;
    const dao::core::TreeRep<std::int32_t, int> empty{0, &head, 0};
    empty.ForEach([&](std::int32_t, int) { any = true; });
    const dao::core::TreeRep<std::int32_t, int> none{0, nullptr, 0};
    none.ForEach([&](std::int32_t, int) { any = true; });
    CHECK_FALSE(any);
}

TEST_CASE("a script array's tag says what it holds", "[arrays]")
{
    using dao::core::ArrayStructure;
    using dao::core::IsArrayTag;
    CHECK(IsArrayTag(0x40));
    CHECK(IsArrayTag(0x43));
    CHECK(IsArrayTag(0x44));
    CHECK_FALSE(IsArrayTag(0x45));
    CHECK_FALSE(IsArrayTag(0x4F));
    CHECK(IsArrayTag(0x53));
    CHECK_FALSE(IsArrayTag(0x5A));
    CHECK_FALSE(IsArrayTag(6));       // an object, not an array
    CHECK(ArrayStructure(0x53) == 3); // effect[]
    CHECK(ArrayStructure(0x43) == -1);
}

TEST_CASE("a script array's elements are its payload's vector", "[arrays]")
{
    std::array<std::uint32_t, 2> ids{0x82, 0x6a8};
    VectorRep<std::uint32_t> vector{0, ids.data(), ids.data() + 2, ids.data() + 2};
    std::int32_t references = 1;
    dao::core::ArrayRep::Payload payload{&vector, &references};
    const dao::core::ArrayRep array{0x43, &payload};
    REQUIRE(array.Elements().size() == 2);
    CHECK(array.Elements()[1] == 0x6a8);

    const dao::core::ArrayRep empty{0x43, nullptr};
    CHECK(empty.Elements().empty());
}

TEST_CASE("the VM stack's n-th value counts down from the top", "[vmstack]")
{
    using dao::core::VmStackView;
    using dao::core::VmTag;
    // Bottom to top: an int 7, an object 0x82, a float 1.5 (a command's first argument on top).
    const std::array<std::int32_t, 3> tags{3, 6, 4};
    std::uint32_t oneAndAHalf = 0;
    const float value = 1.5f;
    std::memcpy(&oneAndAHalf, &value, sizeof value);
    const std::array<std::uint32_t, 3> values{7, 0x82, oneAndAHalf};
    const VmStackView stack{3, tags.data(), values.data()};
    CHECK(stack.Tag(0) == static_cast<std::int32_t>(VmTag::Float));
    CHECK(stack.Word(1, static_cast<std::int32_t>(VmTag::Object)) == 0x82u);
    CHECK(stack.Word(2, static_cast<std::int32_t>(VmTag::Int)) == 7u);
    CHECK_FALSE(stack.Word(2, static_cast<std::int32_t>(VmTag::Object)).has_value()); // another type
    CHECK_FALSE(stack.Tag(3).has_value());                                            // past the bottom
    CHECK_FALSE(VmStackView{}.Tag(0).has_value());
}

TEST_CASE("a GDA column is the CRC32 of its lowercased UTF-16 name", "[gda]")
{
    using dao::core::GdaColumnHash;
    // Hashes the engine's ability handler passes, and xoreos's names for them.
    STATIC_REQUIRE(GdaColumnHash("TargetType") == 0x0F642429u);
    CHECK(GdaColumnHash("UseType") == 0xACEFC4C2u);
    CHECK(GdaColumnHash("AutoTarget") == 0x1C7CA631u);
    CHECK(GdaColumnHash("AOE_Type") == 0x2EB7565Cu);
    CHECK(GdaColumnHash("targettype") == GdaColumnHash("TARGETTYPE")); // case does not matter
}
