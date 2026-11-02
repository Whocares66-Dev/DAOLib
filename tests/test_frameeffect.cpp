#include "dao/core/Erf.h"
#include "dao/core/FrameEffect.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ffx = dao::core::ffx;

namespace
{

// An effect with two targets, a float and a back buffer, and one stage with the given filter body.
std::string Effect(std::string_view body, std::string_view header = R"(id="fx" layer="10" availability="1")")
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<Effect " + std::string(header) +
           ">\n<resources>\n<target id=\"Mask\" width=\"BACKBUFFER_WIDTH/2\" format=\"A8R8G8B8\" shared=\"true\"/>\n"
           "<target id=\"Other\"/>\n<float id=\"Amount\" value=\"1\"/>\n<backbuffer id=\"Depth\" index=\"1\"/>\n"
           "</resources>\n<implementation>\n<stage id=\"s\" event=\"EndFrame\">\n<filter id=\"f\" material=\"M\">\n" +
           std::string(body) + "\n</filter>\n</stage>\n</implementation>\n</Effect>\n";
}

// The instructions of the one filter, or none where the effect was dropped.
std::vector<ffx::Instruction> Lines(std::string_view body)
{
    const auto effects = ffx::Read(Effect(body));
    return effects.empty() ? std::vector<ffx::Instruction>{} : effects[0].stages[0].filters[0].instructions;
}

} // namespace

TEST_CASE("an effect's header, resources, stage and filter are read in file order", "[ffx]")
{
    const auto effects = ffx::Read(Effect("Decal0 = Mask\nfBlur = Amount\nrun semantic Blur"));
    REQUIRE(effects.size() == 1);
    const ffx::Effect &effect = effects[0];
    CHECK(effect.id == "fx");
    CHECK(effect.layer == 10);
    CHECK(effect.available);
    REQUIRE(effect.resources.size() == 4);
    CHECK(effect.resources[0].kind == ffx::ResourceKind::RenderTarget);
    CHECK(effect.resources[0].attributes.size() == 3);
    CHECK(effect.resources[0].attributes[0].name == "width");
    CHECK(effect.resources[0].attributes[0].value == "BACKBUFFER_WIDTH/2");
    CHECK(effect.resources[3].kind == ffx::ResourceKind::BackBuffer);
    REQUIRE(effect.stages.size() == 1);
    CHECK(effect.stages[0].event == ffx::Event::EndFrame);
    const ffx::Filter &filter = effect.stages[0].filters[0];
    CHECK(filter.material == "M");
    REQUIRE(filter.instructions.size() == 3);
    CHECK(std::get<ffx::TextureFromResource>(filter.instructions[0]).parameter == "Decal0");
    CHECK(std::get<ffx::ParameterFromResource>(filter.instructions[1]).kind == ffx::ResourceKind::Float);
    CHECK(std::get<ffx::Run>(filter.instructions[2]).semantic == "Blur");
}

TEST_CASE("element and keyword names match without case", "[ffx]")
{
    // 0x008E687B is __wcsicmp.
    const auto effects = ffx::Read(R"(<EFFECT ID="a"><RESOURCES><TARGET id="T"/></RESOURCES><IMPLEMENTATION>)"
                                   R"(<STAGE id="s" EVENT="endframe"><FILTER id="f">COPY BACKBUFFER TO T</FILTER>)"
                                   "</STAGE></IMPLEMENTATION></EFFECT>");
    REQUIRE(effects.size() == 1);
    CHECK(effects[0].stages[0].event == ffx::Event::EndFrame);
    CHECK(std::get<ffx::Copy>(effects[0].stages[0].filters[0].instructions[0]).fromBackBuffer);
}

TEST_CASE("availability is a number above 0, and an effect without it is not available", "[ffx]")
{
    // 0x0046A16F is setg; the constructor clears the byte.
    CHECK(ffx::Read(Effect("", R"(id="a" availability="2")")).front().available);
    CHECK(!ffx::Read(Effect("", R"(id="a" availability="0")")).front().available);
    CHECK(!ffx::Read(Effect("", R"(id="a" availability="-1")")).front().available);
    const auto bare = ffx::Read(Effect("", R"(id="a" layer="3" minlod="1")"));
    CHECK(!bare.front().available);
    CHECK(bare.front().layer == 3);
    CHECK(bare.front().minLod == 1);
}

TEST_CASE("the id must be the first attribute of an effect, or the file yields nothing", "[ffx]")
{
    CHECK(ffx::Read(Effect("", R"(layer="1" id="a")")).empty());
}

TEST_CASE("an unknown element in Resources drops the effect and stops the file", "[ffx]")
{
    const std::string text =
        R"(<Effect id="a"><Resources><texture id="t"/></Resources></Effect>)" + Effect("", R"(id="b")");
    CHECK(ffx::Read(text).empty());
}

TEST_CASE("a stage with no filter drops the effect", "[ffx]")
{
    CHECK(ffx::Read(R"(<Effect id="a"><implementation><stage id="s" event="start"></stage></implementation>)"
                    "</Effect>")
              .empty());
}

TEST_CASE("an effect read before an error stands, the broken one and the rest are lost", "[ffx]")
{
    const std::string text = Effect("", R"(id="one")") + Effect("bind", R"(id="two")") + Effect("", R"(id="three")");
    const auto effects = ffx::Read(text);
    REQUIRE(effects.size() == 1);
    CHECK(effects[0].id == "one");
}

TEST_CASE("a duplicate resource id is dropped, the first stays", "[ffx]")
{
    // FrameBufferEffect::AddResource, 0x00460280, inserts only when the name is absent.
    const auto effects = ffx::Read(R"(<Effect id="a"><resources><float id="X" value="1"/>)"
                                   R"(<float id="X" value="2"/></resources></Effect>)");
    REQUIRE(effects.size() == 1);
    REQUIRE(effects[0].resources.size() == 1);
    CHECK(effects[0].resources[0].attributes[0].value == "1");
}

TEST_CASE("requires needs effectid as its second attribute", "[ffx]")
{
    const auto effects = ffx::Read(R"(<Effect id="a"><resources><requires id="da_copybackbuffer" )"
                                   R"(effectid="da_copybackbuffer"/></resources></Effect>)");
    REQUIRE(effects.size() == 1);
    REQUIRE(effects[0].requirements.size() == 1);
    CHECK(effects[0].requirements[0].effectId == "da_copybackbuffer");
    CHECK(ffx::Read(R"(<Effect id="a"><resources><requires id="x" other="y" effectid="z"/></resources>)"
                    "</Effect>")
              .empty());
}

TEST_CASE("a stage needs event as its second attribute; an event the engine does not know never runs", "[ffx]")
{
    CHECK(ffx::Read(R"(<Effect id="a"><implementation><stage event="start" id="s"><filter id="f"/></stage>)"
                    "</implementation></Effect>")
              .empty());
    const auto effects = ffx::Read(R"(<Effect id="a"><implementation>)"
                                   R"(<stage id="s" event="user"><filter id="f"/></stage>)"
                                   R"(<stage id="t" event="StaticGeom_Start"><filter id="f"/></stage>)"
                                   R"(<stage id="u" event="stop"><filter id="f"/></stage>)"
                                   "</implementation></Effect>");
    REQUIRE(effects.size() == 1);
    CHECK(effects[0].stages[0].event == ffx::Event::Other);
    CHECK(effects[0].stages[1].event == ffx::Event::StaticGeomStart);
    CHECK(effects[0].stages[2].event == ffx::Event::Stop);
}

TEST_CASE("a filter's material is found as its second or third attribute only", "[ffx]")
{
    const auto effects = ffx::Read(R"(<Effect id="a"><implementation><stage id="s" event="start">)"
                                   R"(<filter id="f" library="L" material="M"/>)"
                                   R"(<filter id="g" a="1" b="2" material="N"/></stage></implementation></Effect>)");
    REQUIRE(effects.size() == 1);
    CHECK(effects[0].stages[0].filters[0].material == "M");
    CHECK(effects[0].stages[0].filters[1].material.empty());
}

TEST_CASE("a cinematics effect is empty, and the file's later effects are lost", "[ffx]")
{
    // 0x0046A0E9 pushes state 11; its end pops to the Effect's state 1, so the next Effect is out
    // of place (0x00469D36, 0x0046A1EF).
    const std::string text =
        Effect("Decal0 = Mask", R"(id="cine" domain="cinematics" layer="7")") + Effect("", R"(id="after")");
    const auto effects = ffx::Read(text);
    REQUIRE(effects.size() == 1);
    CHECK(effects[0].id == "cine");
    CHECK(effects[0].cinematics);
    CHECK(effects[0].layer == 7);
    CHECK(effects[0].resources.empty());
    CHECK(effects[0].stages.empty());
}

TEST_CASE("with cinematics allowed the same file reads whole", "[ffx]")
{
    const std::string text = Effect("Decal0 = Mask", R"(id="cine" domain="cinematics")") + Effect("", R"(id="after")");
    const auto effects = ffx::Read(text, {.cinematicsAllowed = true});
    REQUIRE(effects.size() == 2);
    CHECK(!effects[0].cinematics);
    CHECK(effects[0].stages.size() == 1);
}

TEST_CASE("a domain other than cinematics, and a second effect after a whole one, read", "[ffx]")
{
    const auto effects = ffx::Read(Effect("", R"(id="a" domain="game")") + Effect("", R"(id="b")"));
    REQUIRE(effects.size() == 2);
    CHECK(effects[1].id == "b");
}

TEST_CASE("a number must start with a digit", "[ffx]")
{
    // 0x00460653, 0x004607ED: "-3.5", ".5" and "+1" are no numbers, and a line that is nothing else fails.
    CHECK(ffx::Read(Effect("fBlur = -3.5")).empty());
    CHECK(ffx::Read(Effect("fBlur = .5")).empty());
    const auto instructions = Lines("fBlur = 0.5\nAmount = 2.5\nAmount = -1");
    REQUIRE(instructions.size() == 2); // a resource set to a non-number is accepted and ignored
    CHECK(std::get<ffx::ParameterValue>(instructions[0]).value == 0.5F);
    CHECK(std::get<ffx::SetResource>(instructions[1]).value == 2.5F);
}

TEST_CASE("an assignment's equals signs are skipped, and a target's size is a constant", "[ffx]")
{
    const auto instructions = Lines("fW = Mask.Width\nfH = Depth.height\nx = = Amount\ny   Mask");
    REQUIRE(instructions.size() == 4);
    CHECK(!std::get<ffx::ParameterSize>(instructions[0]).height);
    CHECK(std::get<ffx::ParameterSize>(instructions[1]).height);
    CHECK(std::get<ffx::ParameterFromResource>(instructions[2]).resource == "Amount");
    CHECK(std::get<ffx::TextureFromResource>(instructions[3]).resource == "Mask");
    CHECK(ffx::Read(Effect("a b")).empty());                     // b is no resource, number or member
    CHECK(ffx::Read(Effect("fX = Mask.depth")).empty());         // only width and height
    CHECK(ffx::Read(Effect("Amount = Mask.depth")).size() == 1); // a resource set to a non-number is ignored
}

TEST_CASE("comments are cut at #, blank lines pass, and a line is cut at 255 characters", "[ffx]")
{
    const auto instructions = Lines("# a comment\n\n   \n\tfBlur = 1 # trailing\n    # indented comment");
    REQUIRE(instructions.size() == 1);
    // 0x00469BF6: the buffer holds 255 characters; what follows is not read.
    const std::string padding(250, ' ');
    const auto cut = Lines("bind Mask at 0" + padding + "viewport recalculate");
    REQUIRE(cut.size() == 1);
    CHECK(std::get<ffx::Bind>(cut[0]).viewport == ffx::ViewportMode::Original);
    const auto whole = Lines("bind Mask at 0 viewport recalculate");
    REQUIRE(whole.size() == 1);
    CHECK(std::get<ffx::Bind>(whole[0]).viewport == ffx::ViewportMode::Recalculate);
}

TEST_CASE("bind: a clear after it is ignored", "[ffx]")
{
    const auto instructions = Lines("bind Mask at 0 clear R183G160B143A0");
    REQUIRE(instructions.size() == 1);
    const auto &bind = std::get<ffx::Bind>(instructions[0]);
    CHECK(bind.subject == ffx::Bind::Subject::Resource);
    CHECK(bind.resource == "Mask");
    CHECK(bind.index == 0);
    CHECK(bind.viewport == ffx::ViewportMode::Original);
}

TEST_CASE("bind: viewport counts after at N or a back buffer resource only", "[ffx]")
{
    const auto instructions = Lines("bind BACKBUFFER at 0 viewport recalculate\n"
                                    "bind BACKBUFFER viewport recalculate\n"
                                    "bind Mask viewport stored\n"
                                    "bind Mask at 2 viewport stored\n"
                                    "bind Depth viewport recalculate\n"
                                    "bind NULL at 1");
    REQUIRE(instructions.size() == 6);
    const auto mode = [&](std::size_t i) { return std::get<ffx::Bind>(instructions[i]).viewport; };
    CHECK(mode(0) == ffx::ViewportMode::Recalculate);
    CHECK(std::get<ffx::Bind>(instructions[0]).subject == ffx::Bind::Subject::BackBuffer);
    CHECK(mode(1) == ffx::ViewportMode::Original);
    CHECK(mode(2) == ffx::ViewportMode::Original);
    CHECK(mode(3) == ffx::ViewportMode::Stored);
    CHECK(std::get<ffx::Bind>(instructions[3]).index == 2);
    CHECK(mode(4) == ffx::ViewportMode::Recalculate);
    CHECK(std::get<ffx::Bind>(instructions[4]).index == 1); // the back buffer resource's own
    CHECK(std::get<ffx::Bind>(instructions[5]).subject == ffx::Bind::Subject::Unbind);
    CHECK(std::get<ffx::Bind>(instructions[5]).index == 1);
}

TEST_CASE("bind of a resource that is no surface fails", "[ffx]")
{
    CHECK(ffx::Read(Effect("bind Amount")).empty());
}

TEST_CASE("copy: to is optional, filter picks point or linear, targets only", "[ffx]")
{
    const auto instructions = Lines("copy BACKBUFFER to Mask filter point\ncopy Mask Other filter LINEAR\n"
                                    "copy Mask to Other filter NONE");
    REQUIRE(instructions.size() == 3);
    const auto &first = std::get<ffx::Copy>(instructions[0]);
    CHECK(first.fromBackBuffer);
    CHECK(first.filter == 6);
    CHECK(std::get<ffx::Copy>(instructions[1]).filter == 7);
    CHECK(!std::get<ffx::Copy>(instructions[1]).fromBackBuffer);
    CHECK(std::get<ffx::Copy>(instructions[2]).filter == 5);
    CHECK(ffx::Read(Effect("copy Amount to Mask")).empty());
    CHECK(ffx::Read(Effect("copy Mask to Depth")).empty()); // a target to a back buffer resource is refused
    CHECK(ffx::Read(Effect("copy")).empty());
}

TEST_CASE("run: the semantic needs the filter's material", "[ffx]")
{
    const auto effects = ffx::Read(R"(<Effect id="a"><implementation><stage id="s" event="start">)"
                                   R"(<filter id="f">run semantic X</filter></stage></implementation></Effect>)");
    CHECK(effects.empty());
    const auto instructions = Lines("run\nrun preferred\nrun semantic Pass1");
    REQUIRE(instructions.size() == 3);
    CHECK(std::get<ffx::Run>(instructions[0]).semantic.empty());
    CHECK(std::get<ffx::Run>(instructions[2]).semantic == "Pass1");
    CHECK(ffx::Read(Effect("run other")).empty());
    CHECK(ffx::Read(Effect("run semantic")).empty());
}

TEST_CASE("dump names a target and defaults the path", "[ffx]")
{
    const auto instructions = Lines("dump Mask\ndump Mask out.dds");
    REQUIRE(instructions.size() == 2);
    CHECK(std::get<ffx::Dump>(instructions[0]).path == "Mask.dds");
    CHECK(std::get<ffx::Dump>(instructions[1]).path == "out.dds");
    CHECK(ffx::Read(Effect("dump Nothing")).empty());
}

TEST_CASE("a refused line drops the effect: clear is no word of the parser", "[ffx]")
{
    // da_cleartogreen's `clear R0G255B0A255` is an assignment of an unknown word to a word.
    CHECK(ffx::Read(Effect("clear R0G255B0A255")).empty());
}

TEST_CASE("text outside a filter, and unknown elements outside Resources, are ignored", "[ffx]")
{
    const auto effects = ffx::Read(R"(<Effect id="a"><header><description>copy nothing at all</description></header>)"
                                   "stray text <!-- note --><implementation/></Effect>");
    REQUIRE(effects.size() == 1);
}

TEST_CASE("the game's own effects in postprocesseffects.erf, when there, read as the engine reads them",
          "[ffx][folder]")
{
    const std::filesystem::path path =
        L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins/packages/core/data/postprocesseffects.erf";
    const auto archive = dao::core::erf::Archive::Open(path);
    if (!archive)
    {
        SKIP("no GOG install here");
    }
    std::size_t files = 0;
    for (const auto &entry : archive->Entries())
    {
        const auto bytes = archive->Read(entry);
        REQUIRE(bytes);
        const std::string text(reinterpret_cast<const char *>(bytes->data()), bytes->size());
        // The archive's names are ASCII.
        std::string name;
        for (const wchar_t c : entry.name)
        {
            name.push_back(static_cast<char>(c));
        }
        INFO(name);
        ++files;
        const auto effects = ffx::Read(text);
        const auto allowed = ffx::Read(text, {.cinematicsAllowed = true});
        if (name == "da_cleartogreen.ffx.xml")
        {
            CHECK(effects.empty()); // its `clear` line is refused
            CHECK(allowed.empty());
            continue;
        }
        REQUIRE(effects.size() == 1);
        REQUIRE(allowed.size() == 1);
        CHECK(!effects[0].id.empty());
        // da_bssrdf_shared only declares resources (six targets) and has no implementation.
        if (name == "da_bssrdf_shared.ffx.xml")
        {
            CHECK(allowed[0].stages.empty());
            CHECK(allowed[0].resources.size() == 6);
            CHECK(effects[0].resources.empty());
            continue;
        }
        CHECK(!allowed[0].stages.empty());
        CHECK(effects[0].stages.empty() == effects[0].cinematics);
    }
    CHECK(files == 14);
}
