#include "dao/core/Profile.h"

#include "dao/core/Gff4.h"

#include <algorithm>
#include <optional>
#include <string>

namespace dao::core::profile
{

namespace
{

// The local achievement data's first record sits at this offset; what is before it is a header not read here.
constexpr std::size_t kRecordsAt = 0x80;

// A flag the file may hold as a byte or as a number.
bool Flag(const gff4::Struct &in, std::uint32_t label)
{
    if (const auto byte = in.Get<std::uint8_t>(label))
    {
        return *byte != 0;
    }
    return in.Get<std::int32_t>(label).value_or(0) != 0;
}

bool IsTextByte(std::byte b)
{
    const auto c = static_cast<unsigned char>(b);
    return c > 0x20 && c < 0x7F && c != ';' && c != '@';
}

// Reads the text up to `stop` from `at`, moving `at` past the stop; false if a byte that is no text, or the end, comes
// first.
bool Field(std::span<const std::byte> blob, std::size_t &at, char stop, std::string &out)
{
    out.clear();
    while (at < blob.size() && IsTextByte(blob[at]))
    {
        out.push_back(static_cast<char>(blob[at]));
        ++at;
    }
    if (out.empty() || at >= blob.size() || static_cast<char>(blob[at]) != stop)
    {
        return false;
    }
    ++at;
    return true;
}

// Whether `text` is all digits, and then whether some are not 0; nothing for text that is no number.
std::optional<bool> CountOverZero(const std::string &text)
{
    if (!std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; }))
    {
        return std::nullopt;
    }
    return std::ranges::any_of(text, [](char c) { return c != '0'; });
}

// The blob's records are "NAME;group;n@" in text from kRecordsAt, then a lone NUL and other bytes (read 2026-11-02 in
// one real file: 200 records, the NUL after the last). Reading stops at the first record that is not whole, so
// whatever follows the records, or a file cut short, gives the records before it. An n that is no number stops it too.
std::vector<std::wstring> Unlocked(std::span<const std::byte> blob)
{
    std::vector<std::wstring> out;
    std::size_t at = kRecordsAt;
    std::string name;
    std::string group;
    std::string count;
    while (at < blob.size() && Field(blob, at, ';', name) && Field(blob, at, ';', group) && Field(blob, at, '@', count))
    {
        const auto over = CountOverZero(count);
        if (!over)
        {
            break;
        }
        if (*over)
        {
            out.emplace_back(name.begin(), name.end());
        }
    }
    return out;
}

Achievement ReadAchievement(const gff4::Struct &in)
{
    Achievement a;
    a.index = in.Get<std::int32_t>(kAchievementIndex).value_or(0);
    a.isNew = Flag(in, kAchievementNew);
    a.online = Flag(in, kAchievementOnline);
    a.count = in.Get<std::int32_t>(kAchievementCount).value_or(0);
    a.date = in.String(kAchievementDate).value_or(L"");
    return a;
}

Account ReadAccount(const gff4::Struct &in)
{
    Account account;
    account.name = in.String(kAccountName).value_or(L"");
    account.unlocked = Unlocked(in.ListBytes(kLocalData, gff4::Type::U8));
    for (const auto &a : in.Structs(kAchievements))
    {
        account.achievements.push_back(ReadAchievement(a));
    }
    return account;
}

} // namespace

// OnlineFeatures::LoadProfile 0x00481D30 hands the file's list to 0x00480440(root, name, allowFallback), read
// 2026-11-02: it walks ProfileList (26003) and compares each AccountName (26004) with the name by a std::wstring
// compare, so exactly and by case. The account that matches is the one loaded. When none matches, and the call
// allows a fallback and the list is not empty, it calls itself once with the first account's name and no fallback
// allowed, and the call is false if that finds none either. The name "default" is not in this function; it is the
// name the one real file read has for its only account, and the caller's choice of name is not read. Current is
// the library's rule: the last used, else "default".
// 0x00480440 (this, root, name, allowFallback): the account of that name, compared exactly, else with the
// flag set it calls itself once with the first account's name. Which name LoadProfile asks for was not read.
const Account *Profile::Current() const
{
    const auto found = std::ranges::find(accounts, lastUsed, &Account::name);
    if (found != accounts.end())
    {
        return &*found;
    }
    return accounts.empty() ? nullptr : &accounts.front();
}

std::optional<Profile> Read(std::span<const std::byte> bytes)
{
    const auto file = gff4::File::Parse(bytes);
    if (!file || file->Type() != "PSAV")
    {
        return std::nullopt;
    }
    const auto root = file->Root();
    if (!root)
    {
        return std::nullopt;
    }
    Profile profile;
    profile.build = root->Get<std::int32_t>(kBuild).value_or(0);
    profile.firstBuild = root->Get<std::int32_t>(kFirstBuild).value_or(0);
    profile.lastUsed = root->String(kLastUsed).value_or(L"");
    for (const auto &account : root->Structs(kAccounts))
    {
        profile.accounts.push_back(ReadAccount(account));
    }
    return profile;
}

} // namespace dao::core::profile
