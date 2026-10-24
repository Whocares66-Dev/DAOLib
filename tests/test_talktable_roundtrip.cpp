// Every talk table the GOG install has, read and written back by TalkTableFile::Write (DAOLib
// #106's third step), byte for byte: the header, the list's slots with each id where the game's
// own table has it, and the texts. Hidden, as it reads the install's 1,501 tables: run it by its
// tag, dao_core_tests "[install-real]".
#include "dao/core/TalkTable.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

std::vector<std::byte> ReadFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

} // namespace

TEST_CASE("on the GOG install, every talk table is written back as it was", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    std::size_t tables = 0;
    std::size_t strings = 0;
    for (const auto &item : std::filesystem::recursive_directory_iterator(game))
    {
        std::wstring extension = item.path().extension().wstring();
        std::ranges::transform(extension, extension.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        if (!item.is_regular_file() || extension != L".tlk")
        {
            continue;
        }
        INFO(item.path().string());
        const std::vector<std::byte> bytes = ReadFile(item.path());
        const auto table = dao::core::TalkTableFile::Parse(bytes);
        REQUIRE(table);
        const std::vector<std::byte> written = dao::core::TalkTableFile::Write(table->Strings());
        const auto [mine, theirs] = std::ranges::mismatch(written, bytes);
        INFO("first difference at " << (mine - written.begin()) << " of " << written.size() << " and " << bytes.size());
        CHECK((mine == written.end() && theirs == bytes.end()));
        ++tables;
        strings += table->Size();
    }
    CHECK(tables > 0);
    WARN(tables << " talk tables written back, " << strings << " strings");
}
