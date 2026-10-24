// dao/core/Campaign.h over the install's campaign files in game/campaigns
// when they have been copied there (DAOLib #168); the generic field a
// variable's value is in is gff4's, tested in test_gff4.cpp.
#include "dao/core/Campaign.h"
#include "dao/core/Gff4.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

namespace campaign = dao::core::campaign;
namespace gff4 = dao::core::gff4;

namespace
{

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

std::size_t Areas(const campaign::Node &node)
{
    std::size_t count = node.area.empty() ? 0 : 1;
    for (const auto &child : node.children)
    {
        count += Areas(child);
    }
    return count;
}

} // namespace

TEST_CASE("the single player campaign's script, entry, variables and areas", "[campaign][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "campaigns";
    if (!std::filesystem::exists(folder / "singleplayer.cif"))
    {
        SKIP("no game/campaigns (copy modules/single player/singleplayer.cif and the add-ins' .cif there)");
    }
    const auto bytes = FromDisk(folder / "singleplayer.cif");
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = campaign::CampaignOf(*file);
    REQUIRE(read);
    CHECK(read->script == L"sp_module");
    CHECK(read->entryArea == L"default_start_area");
    CHECK(read->entryPosition[0] < -6.0F);
    CHECK(read->entryOrientation[1] == 1.0F);
    CHECK(read->displayName == L"Single Player!!!! yeah!!!");
    REQUIRE(read->variables.size() == 130);
    CHECK(read->variables[0].name == L"LOG_ACTIVE");
    CHECK(read->variables[0].type == 1);
    CHECK(std::get<std::int32_t>(read->variables[0].value) == 0);
    CHECK(read->areas.tag == L"Single Player");
    REQUIRE(read->areas.children.size() == 192);
    const auto &village = read->areas.children[0];
    CHECK(village.tag == L"arl01al_redcliffe_village");
    CHECK(village.area.empty()); // a group
    REQUIRE(village.children.size() == 12);
    CHECK(village.children[0].area == L"arl100ar_redcliffe_village");
    CHECK(village.children[0].archives == std::vector<std::wstring>{L"lak100d", L"al_arl01al_redcliffe_villag"});
    CHECK(Areas(read->areas) > 192);

    std::size_t files = 0;
    std::size_t strings = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto each = FromDisk(entry.path());
        const auto parsed = gff4::File::Parse(each);
        REQUIRE(parsed);
        const auto other = campaign::CampaignOf(*parsed);
        REQUIRE(other);
        ++files;
        for (const auto &variable : other->variables)
        {
            strings += std::holds_alternative<std::wstring>(variable.value) ? 1 : 0;
            // A string variable's value, the string at the generic element's offset (DAOLib #106).
            if (entry.path().filename() == "dao_prc_cp_3.cif" && variable.name == L"VAL_ROCK_NAME")
            {
                CHECK(std::get<std::wstring>(variable.value) == L"hjn197a872skksaeiil");
            }
        }
    }
    CHECK(files == 41);
    CHECK(strings > 0); // the type 12 variables' strings, through the generic field
}
