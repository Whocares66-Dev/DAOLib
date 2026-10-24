// dao/core/Gff4Walk.h over a file laid out by hand: every kind of value handed to the visitor in
// its template's order, and the walk stopped at its depth (DAOLib #106).
#include "dao/core/Gff4.h"
#include "dao/core/Gff4Walk.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gff4 = dao::core::gff4;
using dao::test::Builder;

namespace
{

// The walk's events as short tokens: "{TOP", "#1", "i-3", "u...", "f1", "sabc", "t42", "[3", "]",
// "}", "none", "...".
class Events : public gff4::Visitor
{
  public:
    std::vector<std::string> out;

    void BeginStruct(std::string_view label) override
    {
        out.push_back("{" + std::string(label));
    }
    void EndStruct() override
    {
        out.emplace_back("}");
    }
    void Field(std::uint32_t label) override
    {
        out.push_back("#" + std::to_string(label));
    }
    void BeginList(std::size_t count) override
    {
        out.push_back("[" + std::to_string(count));
    }
    void EndList() override
    {
        out.emplace_back("]");
    }
    void Value(const gff4::Leaf &leaf) override
    {
        if (const auto *whole = std::get_if<std::int64_t>(&leaf))
        {
            out.push_back("i" + std::to_string(*whole));
        }
        else if (const auto *large = std::get_if<std::uint64_t>(&leaf))
        {
            out.push_back("u" + std::to_string(*large));
        }
        else if (const auto *real = std::get_if<double>(&leaf))
        {
            out.push_back("f" + std::to_string(static_cast<int>(*real)));
        }
        else if (const auto *text = std::get_if<std::wstring>(&leaf))
        {
            std::string narrow = "s";
            for (const wchar_t c : *text)
            {
                narrow += static_cast<char>(c);
            }
            out.push_back(narrow);
        }
        else if (const auto *strref = std::get_if<gff4::StrRef>(&leaf))
        {
            out.push_back("t" + std::to_string(strref->id));
        }
        else
        {
            out.emplace_back("none");
        }
    }
    void TooDeep() override
    {
        out.emplace_back("...");
    }
};

constexpr std::uint16_t kKid = 1;

// A TOP of every kind of field the walk tells apart, two KIDs in a list and one in place.
std::vector<std::byte> Sample()
{
    Builder b = dao::test::Start("TEST", {{"TOP",
                                           0x30,
                                           {{1, 6, 0, 0x00},                              // U64, past INT64_MAX
                                            {2, 1, 0, 0x08},                              // S8
                                            {3, 14, 0, 0x0C},                             // String
                                            {4, 17, 0, 0x10},                             // TlkString
                                            {5, 10, 0, 0x18},                             // Vector3
                                            {6, kKid, gff4::kList | gff4::kStruct, 0x24}, // KIDs
                                            {7, 4, gff4::kList, 0x28},                    // U32s
                                            {8, kKid, gff4::kStruct, 0x2C}}},             // a KID in place
                                          {"KID", 4, {{9, 4, 0, 0}}}});
    const std::size_t top = dao::test::Struct(b, 0x30);
    b.U32(top, 0xFFFFFFFF);
    b.U32(top + 4, 0xFFFFFFFF);
    b.U8(top + 8, 0xFD);
    b.U32(top + 0x0C, b.String(u"abc"));
    b.U32(top + 0x10, 42);
    b.U32(top + 0x14, 0);
    dao::test::Floats(b, top + 0x18, {1, 2, 3});
    b.U32(top + 0x24, b.Numbers({7, 8}));
    b.U32(top + 0x28, b.Numbers({5, 6}));
    b.U32(top + 0x2C, 9);
    return b.bytes;
}

std::vector<std::string> Walked(const std::vector<std::byte> &bytes, int depth)
{
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto root = file->Root();
    REQUIRE(root);
    Events events;
    gff4::Walk(*root, events, depth);
    return events.out;
}

} // namespace

TEST_CASE("the walk hands over every value in its template's order, structs depth first", "[gff4]")
{
    const std::vector<std::string> expected = {
        "{TOP", "#1",  "u" + std::to_string(std::numeric_limits<std::uint64_t>::max()),
        "#2",   "i-3", "#3",
        "sabc", "#4",  "t42",
        "#5",   "[3",  "f1",
        "f2",   "f3",  "]",
        "#6",   "[2",  "{KID",
        "#9",   "i7",  "}",
        "{KID", "#9",  "i8",
        "}",    "]",   "#7",
        "[2",   "i5",  "i6",
        "]",    "#8",  "{KID",
        "#9",   "i9",  "}",
        "}"};
    CHECK(Walked(Sample(), 64) == expected);
}

TEST_CASE("the walk stops at its depth, a struct below it no more than a mark", "[gff4]")
{
    const std::vector<std::string> one = Walked(Sample(), 1);
    // The KIDs are walked to their fields, and their fields' values are past the depth.
    const std::vector<std::string> kid = {"{KID", "#9", "...", "}"};
    CHECK_FALSE(std::ranges::search(one, kid).empty());
    const std::vector<std::string> none = Walked(Sample(), 0);
    CHECK(none.size() == 1 + (2 * 8) + 1);
    CHECK(none[1] == "#1");
    CHECK(none[2] == "...");
    CHECK(Walked(Sample(), -1) == std::vector<std::string>{"..."});
}
