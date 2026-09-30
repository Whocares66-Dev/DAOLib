#include "dao/core/Msvc8.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <string_view>

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
