#include "dao/Strings.h"

#include "dao/Log.h"

#include <Windows.h>

#include <array>
#include <fstream>
#include <iterator>
#include <vector>

namespace dao::strings
{

namespace
{

std::optional<core::TalkTableFile> Read(const std::filesystem::path &file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return std::nullopt;
    }
    const std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return core::TalkTableFile::Parse(std::span(reinterpret_cast<const std::byte *>(raw.data()), raw.size()));
}

} // namespace

std::wstring Locale()
{
    // The settings sit beside the logs: Documents\BioWare\Dragon Age.
    const auto settings = LogDirectory().parent_path() / L"Settings" / L"DragonAge.ini";
    std::array<wchar_t, 32> locale{};
    GetPrivateProfileStringW(L"GameOptions", L"Locale", L"en-us", locale.data(), static_cast<DWORD>(locale.size()),
                             settings.c_str());
    return locale[0] != L'\0' ? std::wstring(locale.data()) : std::wstring(L"en-us");
}

Table Table::Load(const std::filesystem::path &folder, std::wstring_view name)
{
    Table table;
    for (const std::wstring &locale : {Locale(), std::wstring(L"en-us")})
    {
        const auto file = folder / (std::wstring(name) + L"_" + locale + L".tlk");
        auto parsed = Read(file);
        if (!parsed)
        {
            continue;
        }
        table.file_ = file;
        table.table_ = std::move(parsed);
        break;
    }
    return table;
}

Table Table::Load(const LoaderInfo &info, std::wstring_view name)
{
    const std::filesystem::path plugin = info.pluginPath != nullptr ? info.pluginPath : L"";
    return Load(plugin.parent_path(), name);
}

std::wstring Table::String(std::uint32_t id, std::wstring_view fallback) const
{
    auto text = table_ ? table_->String(id) : std::nullopt;
    return text ? std::move(*text) : std::wstring(fallback);
}

} // namespace dao::strings
