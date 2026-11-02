// The game's profile, Settings\Profile.dap, read: the build that wrote it, the profile last used, and each
// account's achievements (dev/SAVES.md, "Achievements and the profile"). Read only: nothing here writes a
// profile or grants an achievement.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::profile
{

// Field labels of the file's PSAV type, as dev/gff4_fields.csv names them (Saveprofile*).
inline constexpr std::uint32_t kBuild = 26000;
inline constexpr std::uint32_t kFirstBuild = 26001;
inline constexpr std::uint32_t kLastUsed = 26002;
inline constexpr std::uint32_t kAccounts = 26003;
inline constexpr std::uint32_t kAccountName = 26004;
inline constexpr std::uint32_t kLocalData = 26005;
inline constexpr std::uint32_t kAchievements = 26006;
inline constexpr std::uint32_t kAchievementIndex = 26007;
inline constexpr std::uint32_t kAchievementNew = 26008;
inline constexpr std::uint32_t kAchievementOnline = 26009;
inline constexpr std::uint32_t kAchievementCount = 26010;
inline constexpr std::uint32_t kAchievementDate = 26011;

// One achievement's record in the file.
struct Achievement
{
    std::int32_t index = 0; // the ID column of the game's Achievements table
    bool isNew = false;
    bool online = false;
    std::int32_t count = 0;
    std::wstring date; // as the file has it, day/month/year
};

// One profile of the file's list.
struct Account
{
    std::wstring name;
    std::vector<Achievement> achievements;
    std::vector<std::wstring> unlocked; // the names the account's local achievement data holds with a count over 0
};

struct Profile
{
    std::int32_t build = 0;
    std::int32_t firstBuild = 0;
    std::wstring lastUsed;
    std::vector<Account> accounts;

    // Returns the account named by `lastUsed`, else the first, else null.
    const Account *Current() const;
};

// The profile in `bytes`; nothing if they are not a GFF4 file of type PSAV. A field the file lacks is left empty or 0.
std::optional<Profile> Read(std::span<const std::byte> bytes);

} // namespace dao::core::profile
