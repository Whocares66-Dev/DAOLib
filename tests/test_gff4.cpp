#include "dao/core/Conversation.h"
#include "dao/core/Gff4.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

using dao::core::ConversationFile;
using dao::core::ConversationLine;
namespace gff4 = dao::core::gff4;

using dao::test::Builder;

namespace
{

// A conversation file made by hand, as the game's are laid out
// (tools/gff4.py --templates on one): a CONV top struct, NTRY and RPLY
// lines in a generic list of references, a PLOT condition in place.

struct FieldSpec
{
    std::uint32_t label;
    std::uint16_t type;
    std::uint16_t flags;
    std::uint32_t offset;
};

constexpr std::uint16_t kPlotTemplate = 3;

const std::vector<FieldSpec> kLineFields = {
    {12201, 17, 0, 0x00},                        // text
    {12202, 14, 0, 0x08},                        // speaker
    {12203, 14, 0, 0x0C},                        // listener
    {12205, 5, 0, 0x14},                         // icon
    {12208, kPlotTemplate, gff4::kStruct, 0x24}, // condition, in place
    {12400, 4, gff4::kList, 0x54},               // children
    {12215, 0, 0, 0x62},                         // fast path
};

struct LineSpec
{
    std::uint32_t text;
    std::u16string speaker;
    std::u16string listener;
    std::int32_t icon;
    std::u16string plot;
    std::int32_t flag;
    std::u16string script;
    std::int32_t parameter;
    std::vector<std::uint32_t> children;
    bool fastPath;
};

std::vector<std::byte> MakeConversation(const std::vector<LineSpec> &lines, const std::vector<bool> &replies)
{
    Builder b;
    b.Text(0, "GFF V4.0PC  CONVV0.1");
    const std::vector<std::vector<FieldSpec>> templates = {
        {{12000, 4, gff4::kList, 0x00}, {12002, 0xFFFF, gff4::kList | gff4::kReference, 0x04}, {12004, 14, 0, 0x1C}},
        kLineFields,
        kLineFields,
        {{12303, 14, 0, 0x00}, {12304, 5, 0, 0x04}, {12300, 14, 0, 0x08}, {12301, 5, 0, 0x0C}, {12302, 0, 0, 0x10}},
    };
    const char *labels[] = {"CONV", "NTRY", "RPLY", "PLOT"};
    const std::uint32_t sizes[] = {0x20, 0x68, 0x68, 0x14};
    b.U32(20, static_cast<std::uint32_t>(templates.size()));
    std::size_t fields = 28 + (16 * templates.size());
    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        const std::size_t at = 28 + (16 * i);
        b.Text(at, labels[i]);
        b.U32(at + 4, static_cast<std::uint32_t>(templates[i].size()));
        b.U32(at + 8, static_cast<std::uint32_t>(fields));
        b.U32(at + 12, sizes[i]);
        for (const FieldSpec &f : templates[i])
        {
            b.U32(fields, f.label);
            b.U32(fields + 4, f.type | (std::uint32_t{f.flags} << 16));
            b.U32(fields + 8, f.offset);
            fields += 12;
        }
    }
    b.data = fields;
    b.U32(24, static_cast<std::uint32_t>(b.data));

    // The top struct, then the lines, then what they point at.
    const std::size_t root = b.data;
    const std::size_t first = root + 0x20;
    b.U8(first + (0x68 * lines.size()) - 1, 0);
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const LineSpec &l = lines[i];
        const std::size_t at = first + (0x68 * i);
        b.U32(at, l.text);
        b.U32(at + 4, 0);
        b.U32(at + 0x08, b.String(l.speaker));
        b.U32(at + 0x0C, b.String(l.listener));
        b.U32(at + 0x14, static_cast<std::uint32_t>(l.icon));
        b.U32(at + 0x24, b.String(l.script));
        b.U32(at + 0x28, static_cast<std::uint32_t>(l.parameter));
        b.U32(at + 0x2C, b.String(l.plot));
        b.U32(at + 0x30, static_cast<std::uint32_t>(l.flag));
        b.U8(at + 0x34, 0xFF);
        b.U32(at + 0x54, b.Numbers(l.children));
        b.U8(at + 0x62, l.fastPath ? 1 : 0);
    }
    // The generic list: a count, then per line its template (flagged a
    // struct) and its offset.
    const std::size_t list = b.bytes.size();
    b.U32(list, static_cast<std::uint32_t>(lines.size()));
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const std::uint32_t shape = replies[i] ? 2 : 1;
        b.U32(list + 4 + (8 * i), shape | (std::uint32_t{gff4::kStruct} << 16));
        b.U32(list + 8 + (8 * i), static_cast<std::uint32_t>(first + (0x68 * i) - b.data));
    }
    b.U32(root + 0x04, static_cast<std::uint32_t>(list - b.data));
    b.U32(root + 0x00, b.Numbers({0}));
    b.U32(root + 0x1C, b.String(u"vo_test"));
    return b.bytes;
}

std::vector<std::byte> TwoLines()
{
    return MakeConversation(
        {
            {1234, u"OWNER", u"PLAYER", 0, u"C8BD51CF", 7, u"", 0, {1}, false},
            {5678, u"PLAYER", u"OWNER", 2, u"", -1, u"gen_cond", 3, {}, true},
        },
        {false, true});
}

} // namespace

TEST_CASE("a conversation's lines read as the file holds them")
{
    const auto bytes = TwoLines();
    const auto conversation = ConversationFile::Parse(bytes);
    REQUIRE(conversation);
    CHECK(conversation->LineCount() == 2);
    CHECK(conversation->Starts() == std::vector<std::uint32_t>{0});
    CHECK(conversation->VoiceBank() == L"vo_test");

    const auto npc = conversation->Line(0);
    REQUIRE(npc);
    CHECK(npc->kind == ConversationLine::Kind::Npc);
    CHECK(npc->text == 1234);
    CHECK(npc->speaker == L"OWNER");
    CHECK(npc->listener == L"PLAYER");
    CHECK(npc->condition.plot == L"C8BD51CF");
    CHECK(npc->condition.flag == 7);
    CHECK(npc->condition.test == 0xFF);
    CHECK_FALSE(npc->condition.Empty());
    CHECK(npc->children == std::vector<std::uint32_t>{1});
    CHECK_FALSE(npc->fastPath);

    const auto reply = conversation->Line(1);
    REQUIRE(reply);
    CHECK(reply->kind == ConversationLine::Kind::Player);
    CHECK(reply->text == 5678);
    CHECK(reply->icon == 2);
    CHECK(reply->condition.script == L"gen_cond");
    CHECK(reply->condition.parameter == 3);
    CHECK(reply->children.empty());
    CHECK(reply->fastPath);
    // The action field is not in this file: an empty check, not a failure.
    CHECK(reply->action.Empty());

    CHECK_FALSE(conversation->Line(2));
}

TEST_CASE("a file that is not a conversation is refused, and a broken one reads as nothing")
{
    auto bytes = TwoLines();
    SECTION("too short")
    {
        CHECK_FALSE(gff4::File::Parse(std::span(bytes).first(20)));
    }
    SECTION("not GFF4")
    {
        bytes[0] = std::byte{'X'};
        CHECK_FALSE(gff4::File::Parse(bytes));
    }
    SECTION("another type")
    {
        std::memcpy(bytes.data() + 12, "SAVE", 4);
        CHECK(gff4::File::Parse(bytes));
        CHECK_FALSE(ConversationFile::Parse(bytes));
    }
    SECTION("the line list's offset past the end")
    {
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const std::size_t root = file->DataOffset();
        const std::uint32_t wild = 0x7FFFFFF0;
        std::memcpy(bytes.data() + root + 4, &wild, 4);
        const auto conversation = ConversationFile::Parse(bytes);
        REQUIRE(conversation);
        CHECK(conversation->LineCount() == 0);
        CHECK_FALSE(conversation->Line(0));
    }
    SECTION("cut short in the data")
    {
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto cut = std::span(bytes).first(file->DataOffset() + 0x30);
        const auto conversation = ConversationFile::Parse(cut);
        REQUIRE(conversation);
        CHECK(conversation->LineCount() == 0);
    }
}

namespace
{

// A file of two templates, ROOT with the fields given and ITEM of one U32,
// the root struct 0x40 bytes at the data's start, for the tests to write
// damaged slots into.
struct Damaged
{
    Builder b;
    std::size_t root = 0;
};

Damaged MakeDamaged(const std::vector<FieldSpec> &rootFields)
{
    Damaged d;
    Builder &b = d.b;
    b.Text(0, "GFF V4.0PC  TESTV0.1");
    b.U32(20, 2);
    std::size_t fields = 28 + (16 * 2);
    const std::vector<FieldSpec> itemFields = {{1, 4, 0, 0}};
    const std::vector<FieldSpec> *shapes[] = {&rootFields, &itemFields};
    const char *labels[] = {"ROOT", "ITEM"};
    const std::uint32_t sizes[] = {0x40, 4};
    for (std::size_t i = 0; i < 2; ++i)
    {
        const std::size_t at = 28 + (16 * i);
        b.Text(at, labels[i]);
        b.U32(at + 4, static_cast<std::uint32_t>(shapes[i]->size()));
        b.U32(at + 8, static_cast<std::uint32_t>(fields));
        b.U32(at + 12, sizes[i]);
        for (const FieldSpec &f : *shapes[i])
        {
            b.U32(fields, f.label);
            b.U32(fields + 4, f.type | (std::uint32_t{f.flags} << 16));
            b.U32(fields + 8, f.offset);
            fields += 12;
        }
    }
    b.data = fields;
    b.U32(24, static_cast<std::uint32_t>(b.data));
    d.root = b.data;
    b.U8(d.root + 0x40 - 1, 0);
    return d;
}

} // namespace

TEST_CASE("a damaged file's counts and template numbers give nothing, never a read past its bytes")
{
    // What the crash log parses is whatever buffer a stack word points at,
    // and a save or a mod's file can be damaged: a count or a template's
    // number past what the file holds must give nothing, not a huge
    // allocation or a read past the templates.
    constexpr std::uint16_t kString = 14;
    constexpr std::uint16_t kU32 = 4;
    constexpr std::uint16_t kMissing = 2; // the file has templates 0 and 1
    Damaged d = MakeDamaged({
        {10, kString, 0, 0x00},                            // a string whose count runs past the file
        {11, kString, 0, 0x04},                            // a string slot of 0xFFFFFFFF: empty
        {12, 1, gff4::kList | gff4::kStruct, 0x08},        // a typed list with a huge count
        {13, kMissing, gff4::kStruct, 0x0C},               // a child of a template the file lacks
        {14, 0xFFFF, gff4::kList, 0x10},                   // a generic list, one element of each
        {15, kMissing, gff4::kList | gff4::kStruct, 0x14}, // a typed list of a template the file lacks
        {16, kU32, gff4::kList, 0x18},                     // a list, which is not a number
        {17, kU32, gff4::kReference, 0x1C},                // a number held by reference
    });
    Builder &b = d.b;
    const std::size_t root = d.root;
    const std::size_t string = b.bytes.size();
    b.U32(string, 0x7FFFFFFF);
    b.U32(string + 4, 0x00410041);
    b.U32(root + 0x00, static_cast<std::uint32_t>(string - b.data));
    b.U32(root + 0x04, 0xFFFFFFFF);
    const std::size_t huge = b.bytes.size();
    b.U32(huge, 0x0FFFFFFF); // a count, and no structs after it
    b.U32(root + 0x08, static_cast<std::uint32_t>(huge - b.data));
    const std::size_t generic = b.bytes.size();
    b.U32(generic, 2);
    b.U32(generic + 4, kMissing | (std::uint32_t{gff4::kStruct} << 16));
    b.U32(generic + 8, 0);
    b.U32(generic + 12, 1 | (std::uint32_t{gff4::kStruct} << 16));
    b.U32(generic + 16, 0x1234);
    b.U32(root + 0x10, static_cast<std::uint32_t>(generic - b.data));
    b.U32(root + 0x14, b.Numbers({1, 0}));
    b.U32(root + 0x18, b.Numbers({5, 6}));
    b.U32(root + 0x1C, b.Numbers({0xCAFE}) + 4); // the value, past its own count
    const auto file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    const auto top = file->Root();
    REQUIRE(top);

    CHECK_FALSE(top->String(10).has_value());
    CHECK(top->String(11) == std::wstring());
    CHECK(top->Structs(12).empty());
    CHECK_FALSE(top->Child(13).has_value());
    REQUIRE(top->Structs(14).size() == 1);
    CHECK(top->Structs(14)[0].Get<std::uint32_t>(1) == 0x1234u);
    CHECK(top->Structs(15).empty());
    CHECK_FALSE(top->Get<std::uint32_t>(16).has_value());
    CHECK(top->Get<std::uint32_t>(17) == 0xCAFEu);
}

TEST_CASE("a field read as the wrong type gives nothing")
{
    const auto bytes = TwoLines();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto lines = file->Root()->Structs(ConversationFile::kLineList);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].Label() == "NTRY");
    CHECK(lines[1].Label() == "RPLY");
    CHECK(lines[1].Get<std::int32_t>(ConversationFile::kLineIcon) == 2);
    CHECK_FALSE(lines[1].Get<std::uint32_t>(ConversationFile::kLineIcon)); // an S32 field
    CHECK_FALSE(lines[1].String(ConversationFile::kLineIcon));
    CHECK_FALSE(lines[1].Get<std::int32_t>(99999));
    CHECK(lines[1].Numbers(ConversationFile::kLineSpeaker).empty());
}

namespace
{

// A campaign's variable as singleplayer.cif lays it out (tools/gff4.py --templates): its name, its
// type and its value, the value a generic field held by reference, whose element names the
// value's own type.
std::vector<std::byte> Variable(std::uint16_t type, bool asString)
{
    Builder b = dao::test::Start(
        "CIF ",
        {{"VARS", 16, {{17001, 14, 0, 0}, {17003, dao::test::kGeneric, gff4::kReference, 4}, {17002, 0, 0, 12}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    b.U32(root, b.String(u"LOG_ACTIVE"));
    b.U8(root + 12, asString ? 12 : 1);
    b.U32(root + 4, type);
    const std::size_t value = dao::test::Struct(b, 4);
    if (asString)
    {
        b.U32(value, b.String(u"plt_main")); // a string's value is an offset to it
    }
    else
    {
        b.U32(value, 42);
    }
    b.U32(root + 8, static_cast<std::uint32_t>(value - b.data));
    return b.bytes;
}

} // namespace

TEST_CASE("a generic field holding one value reads as that value's type, and as no other")
{
    const auto whole = Variable(5, false); // S32
    const auto file = gff4::File::Parse(whole);
    REQUIRE(file);
    CHECK(file->Root()->Get<std::int32_t>(17003) == 42);
    CHECK_FALSE(file->Root()->Get<float>(17003));
    CHECK_FALSE(file->Root()->String(17003));
    CHECK(file->Root()->Get<std::uint8_t>(17002) == 1);

    const auto text = Variable(14, true); // ECString
    const auto other = gff4::File::Parse(text);
    REQUIRE(other);
    CHECK(other->Root()->String(17003) == L"plt_main");
    CHECK_FALSE(other->Root()->Get<std::int32_t>(17003));
}
