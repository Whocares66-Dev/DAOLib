#include "dao/core/Profile.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <shlobj.h>
#include <string>
#include <vector>
#include <windows.h>

namespace profile = dao::core::profile;
namespace gff4 = dao::core::gff4;
using dao::test::Builder;
using dao::test::TemplateSpec;

namespace
{

constexpr std::uint16_t kU8 = 0;
constexpr std::uint16_t kS32 = 5;
constexpr std::uint16_t kStr = 14;
constexpr std::uint16_t kList = gff4::kList;
constexpr std::uint16_t kListStruct = gff4::kList | gff4::kStruct; // a list of structs held in place

struct TestAccount
{
    std::u16string name;
    std::vector<std::uint8_t> blob;
};

// A blob as the game's: a header of 0x80 bytes, the records, a lone NUL, and bytes after it.
std::vector<std::uint8_t> Blob(const std::string &records, const std::string &after = "")
{
    std::vector<std::uint8_t> out(0x80, 0);
    out[0] = '3';
    out[1] = ';';
    out.insert(out.end(), records.begin(), records.end());
    out.push_back(0);
    out.insert(out.end(), after.begin(), after.end());
    return out;
}

// A profile laid out as the game's (tools/gff4.py --templates on a Profile.dap), cut to the fields read: the first
// account carries two achievements.
std::vector<std::byte> MakeProfile(const std::vector<TestAccount> &accounts, const std::u16string &last,
                                   const char *type = "PSAV")
{
    std::vector<TemplateSpec> templates = {
        {"", 0x10, {{26000, kS32, 0, 0}, {26001, kS32, 0, 4}, {26002, kStr, 0, 8}, {26003, 1, kListStruct, 12}}},
        {"PROF", 0x10, {{26004, kStr, 0, 0}, {26005, kU8, kList, 4}, {26006, 2, kListStruct, 8}}},
        {"ACHV",
         0x14,
         {{26007, kS32, 0, 0}, {26008, kU8, 0, 4}, {26009, kU8, 0, 5}, {26010, kS32, 0, 8}, {26011, kStr, 0, 12}}},
    };
    Builder b = dao::test::Start(type, templates);
    const std::size_t root = dao::test::Struct(b, 0x10);
    b.U32(root + 0, 13263);
    b.U32(root + 4, 12000);
    b.U32(root + 8, b.String(last));
    const std::size_t listAt = b.bytes.size();
    b.U32(listAt, static_cast<std::uint32_t>(accounts.size()));
    b.U8(listAt + 4 + (0x10 * accounts.size()) - 1, 0);
    const std::size_t first = listAt + 4;
    b.U32(root + 12, static_cast<std::uint32_t>(listAt - b.data));
    for (std::size_t i = 0; i < accounts.size(); ++i)
    {
        const std::size_t at = first + (0x10 * i);
        b.U32(at + 0, b.String(accounts[i].name));
        b.U32(at + 4, dao::test::Bytes(b, accounts[i].blob));
        const std::size_t count = i == 0 ? 2 : 0;
        const std::size_t achvAt = b.bytes.size();
        b.U32(achvAt, static_cast<std::uint32_t>(count));
        b.U8(achvAt + 4 + (0x14 * count) - (count == 0 ? 0 : 1), 0);
        b.U32(at + 8, static_cast<std::uint32_t>(achvAt - b.data));
        if (count == 2)
        {
            const std::size_t a0 = achvAt + 4;
            b.U32(a0 + 0, 0);
            b.U8(a0 + 4, 1);
            b.U8(a0 + 5, 0);
            b.U32(a0 + 8, 0);
            b.U32(a0 + 12, b.String(u"30/9/2026"));
            const std::size_t a1 = a0 + 0x14;
            b.U32(a1 + 0, 77);
            b.U8(a1 + 4, 0);
            b.U8(a1 + 5, 1);
            b.U32(a1 + 8, 3);
            b.U32(a1 + 12, b.String(u"17/2/2023"));
        }
    }
    return b.bytes;
}

} // namespace

TEST_CASE("a profile's build, last used account and achievements are read", "[profile]")
{
    const auto bytes = MakeProfile({{u"default", Blob("A;default;0@B;default;1@")}}, u"default");
    const auto p = profile::Read(bytes);
    REQUIRE(p);
    CHECK(p->build == 13263);
    CHECK(p->firstBuild == 12000);
    CHECK(p->lastUsed == L"default");
    REQUIRE(p->accounts.size() == 1);
    const auto &account = p->accounts[0];
    CHECK(account.name == L"default");
    REQUIRE(account.achievements.size() == 2);
    CHECK(account.achievements[0].index == 0);
    CHECK(account.achievements[0].isNew);
    CHECK_FALSE(account.achievements[0].online);
    CHECK(account.achievements[0].count == 0);
    CHECK(account.achievements[0].date == L"30/9/2026");
    CHECK(account.achievements[1].index == 77);
    CHECK_FALSE(account.achievements[1].isNew);
    CHECK(account.achievements[1].online);
    CHECK(account.achievements[1].count == 3);
    CHECK(account.achievements[1].date == L"17/2/2023");
}

TEST_CASE("the unlocked names are the blob's records with a count over 0, up to the lone NUL", "[profile]")
{
    // The record after the NUL looks whole and is not read; nor is the header, which has a "3;" like a record's start.
    const auto bytes = MakeProfile(
        {{u"default", Blob("ONE;default;1@TWO;default;0@THREE;default;10@FOUR;default;00@", "LATE;default;1@")}},
        u"default");
    const auto p = profile::Read(bytes);
    REQUIRE(p);
    CHECK(p->accounts[0].unlocked == std::vector<std::wstring>{L"ONE", L"THREE"});
}

TEST_CASE("a blob that is cut or odd gives the records before the break and never faults", "[profile]")
{
    const auto read = [](const std::vector<std::uint8_t> &blob) {
        const auto p = profile::Read(MakeProfile({{u"default", blob}}, u"default"));
        REQUIRE(p);
        return p->accounts[0].unlocked;
    };
    // Cut inside a record, and with no NUL at all.
    std::vector<std::uint8_t> cut = Blob("A;default;1@B;default;1");
    cut.pop_back();
    CHECK(read(cut) == std::vector<std::wstring>{L"A"});
    // A count that is no number, a record with a missing group, an empty name: each stops the reading.
    CHECK(read(Blob("A;default;1@B;default;x@C;default;1@")) == std::vector<std::wstring>{L"A"});
    CHECK(read(Blob("A;default;1@B;1@C;default;1@")) == std::vector<std::wstring>{L"A"});
    CHECK(read(Blob("A;default;1@;default;1@C;default;1@")) == std::vector<std::wstring>{L"A"});
    // No bytes past the header, a blob shorter than the header, and none at all.
    CHECK(read(std::vector<std::uint8_t>(0x80, 0)).empty());
    CHECK(read(std::vector<std::uint8_t>(10, 0x41)).empty());
    CHECK(read({}).empty());
    // A record that ends the blob exactly is read.
    std::vector<std::uint8_t> exact(0x80, 0);
    const std::string last = "Z;default;5@";
    exact.insert(exact.end(), last.begin(), last.end());
    CHECK(read(exact) == std::vector<std::wstring>{L"Z"});
}

TEST_CASE("the current account is the last used, else the first, else none", "[profile]")
{
    const auto read = [](const std::u16string &last, const std::vector<TestAccount> &accounts) {
        auto p = profile::Read(MakeProfile(accounts, last));
        REQUIRE(p);
        return *p;
    };
    const std::vector<TestAccount> both{{u"default", Blob("")}, {u"other", Blob("")}};
    CHECK(read(u"other", both).Current()->name == L"other");
    CHECK(read(u"gone", both).Current()->name == L"default");
    CHECK(read(u"", both).Current()->name == L"default");
    // The name is matched exactly, as the engine's compare does.
    CHECK(read(u"OTHER", both).Current()->name == L"default");
    CHECK(read(u"gone", {{u"a", Blob("")}, {u"b", Blob("")}}).Current()->name == L"a");
    CHECK(read(u"", {}).Current() == nullptr);
    CHECK(read(u"", {{u"", Blob("")}}).Current() != nullptr);
}

TEST_CASE("a file that is not a PSAV is no profile", "[profile]")
{
    CHECK_FALSE(profile::Read(MakeProfile({}, u"", "SAVE")));
    CHECK_FALSE(profile::Read({}));
    const std::vector<std::byte> junk(100, std::byte{0x41});
    CHECK_FALSE(profile::Read(junk));
}

// The player's own profile, read only. Hidden, as the file is the player's: run it by its exact name.
TEST_CASE("the player's profile, when there, reads with an account, its achievements and unlocked names",
          "[.][profile-real]")
{
    PWSTR documents = nullptr;
    std::filesystem::path file;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents)))
    {
        file = std::filesystem::path(documents) / L"BioWare/Dragon Age/Settings/Profile.dap";
    }
    CoTaskMemFree(documents);
    if (!std::filesystem::is_regular_file(file))
    {
        SKIP("no Documents/BioWare/Dragon Age/Settings/Profile.dap here");
    }
    std::ifstream in(file, std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(raw.size());
    std::memcpy(bytes.data(), raw.data(), raw.size());
    const auto p = profile::Read(bytes);
    REQUIRE(p);
    CHECK(p->build > 0);
    REQUIRE(p->Current() != nullptr);
    const auto &account = *p->Current();
    CHECK_FALSE(account.achievements.empty());
    // Only so many names can be unlocked as the file has achievements.
    CHECK(account.unlocked.size() <= account.achievements.size());
    WARN(p->accounts.size() << " accounts, " << account.achievements.size() << " achievements, "
                            << account.unlocked.size() << " unlocked in the current one");
}
