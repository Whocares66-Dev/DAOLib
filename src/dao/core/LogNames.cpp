#include "dao/core/LogNames.h"

#include <algorithm>
#include <array>
#include <string>

namespace dao::core
{

bool IsOwnLog(std::string_view file, bool ownsLogs, std::uint32_t pid)
{
    constexpr std::string_view kLog = ".log";
    constexpr std::array<std::string_view, 3> kReports = {"crash-", "hang-", "exit-"};
    if (!file.ends_with(kLog) || file.size() == kLog.size())
    {
        return false;
    }
    const std::string_view stem = file.substr(0, file.size() - kLog.size());
    if (stem.ends_with(".previous") ||
        std::ranges::any_of(kReports, [&](std::string_view report) { return stem.starts_with(report); }))
    {
        return false;
    }
    // A process's own id after the name's last dot, all digits, marks a log of a game that had
    // not the plain names.
    const std::size_t dot = stem.rfind('.');
    const std::string_view last = dot == std::string_view::npos ? std::string_view{} : stem.substr(dot + 1);
    const bool numbered = !last.empty() && std::ranges::all_of(last, [](char c) { return c >= '0' && c <= '9'; });
    if (!numbered)
    {
        return ownsLogs;
    }
    return !ownsLogs && last == std::to_string(pid);
}

} // namespace dao::core
