// Every data table the GOG install has, loose or in an archive of version 2.0, read and written back
// by GdaFile::Bytes (DAOLib #106's third step), byte for byte: G2DA version 0.2, the data tables,
// and 0.1, the conversations' tables. A DAOLib plugin's own tables under bin_ship are left out: not
// the game's. Hidden, as it reads every archive of the install: run it by its tag, dao_core_tests
// "[install-real]".
#include "dao/core/Erf.h"
#include "dao/core/GdaFile.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace archive = dao::core::erf;

namespace
{

std::vector<std::byte> ReadFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::wstring Lower(std::wstring text)
{
    std::ranges::transform(text, text.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return text;
}

} // namespace

TEST_CASE("on the GOG install, every data table is written back as it was", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    std::size_t tables = 0;
    const auto check = [&](const std::wstring &where, const std::vector<std::byte> &bytes) {
        INFO(std::filesystem::path(where).string());
        const auto table = dao::core::GdaFile::Parse(bytes);
        REQUIRE(table);
        const std::vector<std::byte> written = table->Bytes();
        const auto [mine, theirs] = std::ranges::mismatch(written, bytes);
        INFO("first difference at " << (mine - written.begin()) << " of " << written.size() << " and " << bytes.size());
        CHECK((mine == written.end() && theirs == bytes.end()));
        ++tables;
    };
    for (auto item = std::filesystem::recursive_directory_iterator(game); item != std::filesystem::end(item); ++item)
    {
        if (item->is_directory() && Lower(item->path().filename().wstring()) == L"bin_ship")
        {
            item.disable_recursion_pending();
            continue;
        }
        const std::wstring extension = Lower(item->path().extension().wstring());
        if (!item->is_regular_file())
        {
            continue;
        }
        if (extension == L".gda")
        {
            check(item->path().wstring(), ReadFile(item->path()));
            continue;
        }
        if (extension != L".erf" && extension != L".rim")
        {
            continue;
        }
        const auto opened = archive::Archive::Open(item->path());
        if (!opened || opened->Head().version != archive::Version::V20)
        {
            continue;
        }
        for (const archive::Entry &entry : opened->Entries())
        {
            if (Lower(std::filesystem::path(entry.name).extension().wstring()) == L".gda")
            {
                const auto bytes = opened->Read(entry);
                REQUIRE(bytes);
                check(item->path().wstring() + L":" + entry.name, *bytes);
            }
        }
    }
    CHECK(tables > 0);
    WARN(tables << " data tables written back");
}
