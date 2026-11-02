#include "dao/core/SubmissionProgram.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace sub = dao::core::sub;

namespace
{

// A program in the file's grammar, groups inside the instruction.
std::string Program(std::string_view groups)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<SubmissionInstruction id = \"Test\">\n" +
           std::string(groups) + "\n</SubmissionInstruction>\n";
}

std::string Group(std::string_view id, std::string_view lines)
{
    return "<SubmissionGroup id=\"" + std::string(id) + "\">\n" + std::string(lines) + "\n</SubmissionGroup>\n";
}

sub::Node NodeOf(std::string_view category, std::uint32_t attributes = 0)
{
    sub::Node node;
    node.categories = sub::CategoryMask(category).value();
    node.attributes = attributes;
    return node;
}

} // namespace

TEST_CASE("the 38 categories are the engine's table in its order", "[sub]")
{
    const auto &names = sub::CategoryNames();
    REQUIRE(names.size() == 38);
    // 0x004F6660, read as code (2026-11-01): the bit of a name is its place in the chain.
    CHECK(sub::CategoryMask("transparent") == 0x1);
    CHECK(sub::CategoryMask("Black") == 0x80000000);
    CHECK(sub::CategoryMask("distortion_diffuse") == 0x100000000);
    CHECK(sub::CategoryMask("LowLOD") == 0x2000000000);
    CHECK(sub::CategoryMask("MULTIPASSHAIR") == 0x40000000);
    CHECK(sub::CategoryMask("3dgui") == 0x800000);
    CHECK(!sub::CategoryMask("nonesuch"));
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        CHECK(sub::CategoryMask(names[i]) == (std::uint64_t{1} << i));
    }
}

TEST_CASE("the attribute names are the engine's bits", "[sub]")
{
    CHECK(sub::AttributeBit("occluder") == 0x1);
    CHECK(sub::AttributeBit("FadeoutObject") == 0x10);
    CHECK(sub::AttributeBit("lodfade") == 0x20);
    CHECK(sub::AttributeBit("hidden") == 0x400);
    CHECK(sub::AttributeBit("ignore_distortion") == 0x40000);
    CHECK(sub::AttributeBit("fadeoutpreemitter") == 0x80000);
    CHECK(!sub::AttributeBit("nonesuch"));
}

TEST_CASE("groups, classes and their includes read in the file's order", "[sub]")
{
    const auto text =
        Program(Group("A", "include class prop, except class transparent\nsort by material\nrender batched") +
                Group("B", "include class sky"));
    const auto program = sub::Read(text);
    REQUIRE(program);
    CHECK(program->id == "Test");
    REQUIRE(program->groups.size() == 2);
    const sub::Group &a = program->groups[0];
    CHECK(a.id == "A");
    REQUIRE(a.classes.size() == 1);
    CHECK(a.classes[0].includes.size() == 1);
    CHECK(a.classes[0].includes[0].kind == sub::DirectiveKind::Class);
    CHECK(a.classes[0].includes[0].categories == 0x10000000);
    CHECK(a.classes[0].mode == sub::RenderMode::Batched);
    CHECK(a.classes[0].HasIncludes());
    REQUIRE(a.exclusions.size() == 1);
    CHECK(a.exclusions[0].categories == 0x1);
    REQUIRE(a.classes[0].OperationsAt(sub::OperationWhen::Before).size() == 1);
    CHECK(a.classes[0].OperationsAt(sub::OperationWhen::Before)[0]->type == sub::OperationType::MaterialSort);
    CHECK(program->groups[1].id == "B");
}

TEST_CASE("includes of one kind merge by their masks, in a class and in the exclusions", "[sub]")
{
    const auto program =
        sub::Read(Program(Group("G", "include class transparent\ninclude class emitter, attribute hidden\n"
                                     "include class vista in Other\nexclude class eye\nexclude class hair, attribute "
                                     "hidden, attribute occluder")));
    REQUIRE(program);
    const sub::Group &g = program->groups[0];
    REQUIRE(g.classes.size() == 2);
    // "include class emitter" is the last keyword's piece again: a class directive and an attribute one.
    REQUIRE(g.classes[0].includes.size() == 2);
    CHECK(g.classes[0].includes[0].categories == (0x1 | 0x40));
    CHECK(g.classes[0].includes[1].kind == sub::DirectiveKind::Attribute);
    CHECK(g.classes[1].name == "Other");
    REQUIRE(g.exclusions.size() == 2);
    CHECK(g.exclusions[0].categories == (0x40000 | 0x80000));
    CHECK(g.exclusions[1].bits == (0x400 | 0x1));
}

TEST_CASE("a name, semantic and multipass directive are not merged", "[sub]")
{
    const auto program =
        sub::Read(Program(Group("G", "include material A, material B, supportsmultipass true, semantic S, "
                                     "shadowcaster 0, shadowcaster 1, invisibleshadowcaster 1")));
    REQUIRE(program);
    const auto &includes = program->groups[0].classes[0].includes;
    REQUIRE(includes.size() == 6);
    CHECK(includes[0].name == "A");
    CHECK(includes[1].name == "B");
    CHECK(includes[2].flag);
    CHECK(includes[3].name == "S");
    CHECK(includes[4].bits == 0x3); // partitions 0 and 1 merged into one
    CHECK(includes[5].kind == sub::DirectiveKind::InvisibleShadowCaster);
}

TEST_CASE("a comment past a hash and the blank lines are nothing", "[sub]")
{
    const auto program =
        sub::Read(Program("<!-- include class sky -->\n" +
                          Group("G", "# include class eye\n   \n\tinclude class sky # not this: class eye\n")));
    REQUIRE(program);
    REQUIRE(program->groups.size() == 1);
    REQUIRE(program->groups[0].classes[0].includes.size() == 1);
    CHECK(program->groups[0].classes[0].includes[0].categories == 0x200000);
}

TEST_CASE("keep, forcedraw, render and select", "[sub]")
{
    const auto program =
        sub::Read(Program(Group("G", "include class sky, keep copy, render pass 1\nselect Second\ninclude all\n"
                                     "keep copy\nkeep original\nforcedraw\nrender with multipass\nrender disabled\n"
                                     "include class eye in Third, render batched")));
    REQUIRE(program);
    const auto &classes = program->groups[0].classes;
    REQUIRE(classes.size() == 3);
    CHECK(classes[0].keepCopy);
    CHECK(classes[0].mode == sub::RenderMode::Pass);
    CHECK(classes[0].pass == 1);
    CHECK(classes[1].name == "Second");
    CHECK(!classes[1].keepCopy); // keep original after keep copy
    CHECK(classes[1].forceDraw);
    CHECK(classes[1].mode == sub::RenderMode::Disabled);
    CHECK(classes[2].name == "Third");
    CHECK(classes[2].mode == sub::RenderMode::Batched);
}

TEST_CASE("operations: sorts, forces, triggers, effects, shadows and speed trees", "[sub]")
{
    const auto program =
        sub::Read(Program(Group("G", "sort by distance farthest\nsort by distance nearest\nforce semantic PSSM, force "
                                     "material Sky.Sky2\nforce material NoDot\n"
                                     "trigger beginframe\ntrigger endframe\neffect da_bssrdf.stage1\nshadows "
                                     "partitiongroup 1\nshadows blurpartition\nshadows blurpartition 1\n"
                                     "shadows bindbackbuffer\nshadows resetinvisiblecasters\nspeedtree "
                                     "renderAllFadedTrees\nrequires shadows enabled\nrequires fbeffects")));
    REQUIRE(program);
    const auto &c = program->groups[0].classes;
    REQUIRE(c.size() == 1);
    const auto &ops = c[0].operations;
    using T = sub::OperationType;
    using W = sub::OperationWhen;
    REQUIRE(ops.size() == 14); // force material NoDot makes none
    CHECK(ops[0].type == T::DistanceSortFarthest);
    CHECK(ops[1].type == T::DistanceSortNearest);
    CHECK((ops[2].type == T::SemanticOverride && ops[2].first == "PSSM" && ops[2].when == W::PerObject));
    // force material A.B: the library and the object are kept; the engine loads the library only [C 0x0046F2D6].
    CHECK((ops[3].type == T::MaterialOverride && ops[3].first == "Sky" && ops[3].second == "Sky2"));
    CHECK((ops[4].type == T::EventTrigger && ops[4].number == 2 && ops[4].when == W::Before));
    CHECK((ops[5].type == T::EventTrigger && ops[5].number == 3 && ops[5].when == W::After));
    CHECK((ops[6].type == T::EffectStage && ops[6].first == "da_bssrdf" && ops[6].second == "stage1"));
    CHECK((ops[7].type == T::StartPartition && ops[7].number == 1 && ops[7].when == W::Before));
    CHECK((ops[8].type == T::StopPartition && ops[8].number == 1 && ops[8].when == W::After));
    CHECK((ops[9].type == T::BlurPartition && ops[9].number == 0));
    CHECK((ops[10].type == T::BlurPartition && ops[10].number == 1));
    CHECK((ops[11].type == T::BindBackbuffer && ops[11].when == W::After));
    CHECK((ops[12].type == T::ResetInvisibleCasters && ops[12].when == W::PerObject));
    CHECK((ops[13].type == T::RenderFadedSpeedTrees && ops[13].when == W::After));
    CHECK(c[0].gate == sub::Gate::FbEffects); // the second requires replaces the first
    CHECK(c[0].OperationsAt(W::After).size() == 4);
    // A class of operations only has no includes, and a group of them is not in the classified list.
    CHECK(!program->groups[0].HasIncludes());
}

TEST_CASE("trigger's events are the engine's numbers, and the words after the first are not read", "[sub]")
{
    CHECK(sub::TriggerEvent("StaticGeom_End") == 6);
    CHECK(sub::TriggerEvent("staticgeomstart") == 5);
    CHECK(sub::TriggerEvent("motiongeometryend") == 8);
    CHECK(sub::TriggerEvent("userdefined") == 10);
    CHECK(sub::TriggerEvent("nonesuch") == -1);
    const auto program = sub::Read(Program(Group("G", "trigger beginframe, staticgeomstart")));
    REQUIRE(program);
    CHECK(program->groups[0].classes[0].operations.size() == 1); // the second piece goes on with no trigger
}

TEST_CASE("a piece with no keyword goes on with the last include, exclude, effect or requires", "[sub]")
{
    const auto program =
        sub::Read(Program(Group("G", "include class sky, class eye, except class hair, attribute hidden, class face")));
    REQUIRE(program);
    CHECK(program->groups[0].classes[0].includes[0].categories == (0x200000 | 0x40000));
    REQUIRE(program->groups[0].exclusions.size() == 2);
    CHECK(program->groups[0].exclusions[0].categories == (0x80000 | 0x1000000));
    CHECK(!sub::Read(Program(Group("G", "class sky")))); // nothing to go on with
}

TEST_CASE("the engine's errors throw the file away", "[sub]")
{
    CHECK(!sub::Read(""));
    CHECK(!sub::Read("<SubmissionInstruction><SubmissionGroup>"));       // tags that do not close
    CHECK(!sub::Read("<SubmissionGroup>include all</SubmissionGroup>")); // a group outside the instruction
    CHECK(!sub::Read(Program("<SubmissionGroup><SubmissionGroup>include all</SubmissionGroup></SubmissionGroup>")));
    CHECK(!sub::Read(Program(Group("G", "include class nonesuch")))); // a class the engine has no name for
    CHECK(!sub::Read(Program(Group("G", "exclude attribute nonesuch"))));
    CHECK(!sub::Read(Program(Group("G", "include class sky eye")))); // a word that is not `in`
    CHECK(!sub::Read(Program(Group("G", "include class"))));         // no value
    CHECK(!sub::Read(Program(Group("G", "include class sky in"))));  // no class name
    CHECK(!sub::Read(Program(Group("G", "exclude"))));
    CHECK(!sub::Read(Program(Group("G", "render"))));
    CHECK(!sub::Read(Program(Group("G", "render pass"))));
    CHECK(!sub::Read(Program(Group("G", "select"))));
    CHECK(!sub::Read(Program(Group("G", "trigger nonesuch"))));
    CHECK(!sub::Read(Program(Group("G", "effect nodot"))));
    CHECK(!sub::Read(Program(Group("G", "requires something"))));
    CHECK(!sub::Read(Program(Group("G", "shadows partitiongroup"))));
    CHECK(!sub::Read(Program(Group("G", "shadows nonesuch"))));
    CHECK(!sub::Read(Program(Group("G", "speedtree nonesuch"))));
    CHECK(!sub::Read(Program(Group("G", "nonesuch"))));
    CHECK(
        !sub::Read(Program(Group("G", "include class sky " + std::string(300, ' ') + "in X " + std::string(10, 'x')))));
    // A word that is no kind is dropped and is not an error.
    CHECK(sub::Read(Program(Group("G", "include nonesuch sky\ndepth\nforce nonesuch x"))));
}

TEST_CASE("a group is a SubmissionGroup or a SubmissionClass, one element under two spellings", "[sub]")
{
    const auto program = sub::Read(Program("<SubmissionClass id=\"A\">include class sky</SubmissionClass>\n" +
                                           Group("B", "include class eye") + "<SubmissionGroup id=\"C\"/>\n"));
    REQUIRE(program);
    REQUIRE(program->groups.size() == 3);
    CHECK(program->groups[0].id == "A");
    CHECK(program->groups[0].classes[0].includes[0].categories == 0x200000);
    CHECK(program->groups[2].classes.empty());
}

TEST_CASE("a line takes at most 16 instructions and 255 characters", "[sub]")
{
    // 0x00472553 stops at the 16th piece: the 17th is not read. [C 0x00472553]
    std::string line = "select a";
    for (char name = 'b'; name <= 'q'; ++name)
    {
        line += std::string(", select ") + name;
    }
    const auto program = sub::Read(Program(Group("G", line)));
    REQUIRE(program);
    CHECK(program->groups[0].classes.size() == 16);
    CHECK(program->groups[0].classes.back().name == "p");
    // 255 characters are taken; 256 make the text an error and the file is thrown away. [C 0x00473FB9]
    const std::string include = "include class sky";
    CHECK(sub::Read(Program(Group("G", include + std::string(255 - include.size(), ' ')))));
    CHECK(!sub::Read(Program(Group("G", include + std::string(256 - include.size(), ' ')))));
    // Blanks before the text are not counted.
    CHECK(sub::Read(Program(Group("G", std::string(40, ' ') + include + std::string(255 - include.size(), ' ')))));
}

TEST_CASE("a failure sticks: whatever follows it cannot save the file", "[sub]")
{
    CHECK(!sub::Read(Program(Group("A", "include class nonesuch") + Group("B", "include class sky"))));
    CHECK(!sub::Read(Program(Group("A", "include class sky") + Group("B", "nonesuch") + Group("C", "include all"))));
    // A group inside a group is the element out of place that does it.
    CHECK(!sub::Read(
        Program("<SubmissionGroup id=\"A\"><SubmissionGroup id=\"B\">include all</SubmissionGroup></SubmissionGroup>" +
                Group("C", "include all"))));
}

TEST_CASE("text outside a group, and elements the builder does not know, are nothing", "[sub]")
{
    const auto program =
        sub::Read(Program("include nonesuch\n<Other>nonesuch</Other>\n" + Group("A", "include class sky")));
    REQUIRE(program);
    CHECK(program->groups.size() == 1);
    // The root names the instruction and has the id; the file may have a byte order mark before it.
    CHECK(sub::Read("\xEF\xBB\xBF" + Program(Group("A", "include all"))));
}

TEST_CASE("a missing or bad file falls back to the one-group program that takes everything", "[sub]")
{
    const sub::Loaded missing = sub::ReadOrFallback(std::nullopt);
    CHECK(missing.fellBack);
    CHECK(missing.program.groups.size() == 1);
    CHECK(sub::ReadOrFallback("").fellBack);
    CHECK(
        sub::ReadOrFallback(
            "<SubmissionInstruction><SubmissionGroup>include class nonesuch</SubmissionGroup></SubmissionInstruction>")
            .fellBack);
    const sub::Loaded good = sub::ReadOrFallback(Program(Group("G", "include class sky")));
    CHECK(!good.fellBack);
    // The fallback takes any node with a material, into its one class, and none without.
    const sub::Program fallback = sub::Fallback();
    CHECK(fallback.Classify(NodeOf("water")) == std::vector<sub::Placement>{{.group = 0, .cls = 0}});
    sub::Node bare;
    bare.hasMaterial = false;
    CHECK(fallback.Classify(bare).empty());
    CHECK(fallback.groups[0].classes[0].mode == sub::RenderMode::Plain);
}

TEST_CASE("a group refuses on any exclusion, and the first class that takes a node ends the search", "[sub]")
{
    const auto program = sub::Read(
        Program(Group("Chars", "include class character, except attribute hidden, class hair\nrender batched") +
                Group("Rest", "include all\nkeep original")));
    REQUIRE(program);
    // A character is claimed by the first group.
    CHECK(program->Classify(NodeOf("character")) == std::vector<sub::Placement>{{.group = 0, .cls = 0}});
    // A hidden one is refused by the first group and falls to the second.
    CHECK(program->Classify(NodeOf("character", sub::attribute::kHidden)) ==
          std::vector<sub::Placement>{{.group = 1, .cls = 0}});
    // Hair is excluded by the first group, whatever its classes.
    CHECK(program->Classify(NodeOf("hair")) == std::vector<sub::Placement>{{.group = 1, .cls = 0}});
}

TEST_CASE("keep copy lets a node go on, and a group that only kept copies does not claim it", "[sub]")
{
    const auto program =
        sub::Read(Program(Group("Shadow", "include shadowcaster 0\nexclude attribute invisiblecaster\nkeep copy") +
                          Group("Lit", "include class prop in First, keep copy\ninclude class prop in Second\n") +
                          Group("Never", "include all")));
    REQUIRE(program);
    sub::Node prop = NodeOf("prop");
    prop.partitions = 0x1;
    // Shadow keeps a copy, so Lit sees it; in Lit both classes take it (the first keeps a copy), and Lit claims it.
    const std::vector<sub::Placement> expected = {
        {.group = 0, .cls = 0}, {.group = 1, .cls = 0}, {.group = 1, .cls = 1}};
    CHECK(program->Classify(prop) == expected);
    // An invisible caster is refused by Shadow and falls to the rest.
    sub::Node invisible = prop;
    invisible.attributes = sub::attribute::kInvisibleCaster;
    CHECK(program->Classify(invisible).front() == sub::Placement{.group = 1, .cls = 0});
    // A node with no material is in no class at all.
    sub::Node bare = prop;
    bare.hasMaterial = false;
    CHECK(program->Classify(bare).empty());
}

TEST_CASE("each kind of directive matches as the engine's does", "[sub]")
{
    sub::Node node = NodeOf("character", sub::attribute::kFadeoutObject);
    node.material = "Armor_Heavy";
    node.semantic = "LerpedTint";
    node.semantics = {"Default", "LerpedTint"};
    node.partitions = 0b10;
    node.castsShadow = false;
    node.supportsMultipass = false;

    auto directive = [](sub::DirectiveKind kind) {
        sub::Directive d;
        d.kind = kind;
        return d;
    };
    sub::Directive d = directive(sub::DirectiveKind::Always);
    CHECK(sub::Matches(d, node));
    d = directive(sub::DirectiveKind::Class);
    d.categories = 0x400 | 0x1;
    CHECK(sub::Matches(d, node));
    d.categories = 0x1;
    CHECK(!sub::Matches(d, node));
    d = directive(sub::DirectiveKind::Attribute);
    d.bits = sub::attribute::kFadeoutObject | sub::attribute::kHidden;
    CHECK(sub::Matches(d, node));
    d.bits = sub::attribute::kHidden;
    CHECK(!sub::Matches(d, node));
    d = directive(sub::DirectiveKind::MaterialName);
    d.name = "armor_heavy";
    CHECK(sub::Matches(d, node));
    d = directive(sub::DirectiveKind::SemanticName);
    d.name = "lerpedtint";
    CHECK(sub::Matches(d, node));
    d = directive(sub::DirectiveKind::IncludesSemantic);
    d.name = "default";
    CHECK(sub::Matches(d, node));
    CHECK(sub::Matches(directive(sub::DirectiveKind::NoShadowCast), node));
    d = directive(sub::DirectiveKind::ShadowCaster);
    d.bits = 0b10;
    CHECK(sub::Matches(d, node));
    d = directive(sub::DirectiveKind::InvisibleShadowCaster);
    d.bits = 0b10;
    CHECK(!sub::Matches(d, node)); // in the partition, but not an invisible caster
    node.attributes |= sub::attribute::kInvisibleCaster;
    CHECK(sub::Matches(d, node));
    d = directive(sub::DirectiveKind::MultiPass);
    d.flag = false;
    CHECK(sub::Matches(d, node));
    d.flag = true;
    CHECK(!sub::Matches(d, node));
    // Shadow partition -1 is every partition.
    const auto all = sub::Read(Program(Group("G", "include shadowcaster -1")));
    REQUIRE(all);
    CHECK(all->groups[0].classes[0].includes[0].bits == 0xFFFFFFFFu);
}

TEST_CASE("the shipped programs in game/subqueuefiles, when there, read whole", "[sub][folder]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "subqueuefiles";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/subqueuefiles in this checkout (ghilana's extract of *.sub.xml writes them)");
    }
    std::size_t files = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (!entry.path().filename().string().ends_with(".sub.xml"))
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        INFO(entry.path().filename().string());
        const auto program = sub::Read(text);
        if (entry.path().filename() == "greenscreen.sub.xml")
        {
            // Its BOM says UTF-16, but its line ends were translated as text (\r\0\r\n\0, 12009 bytes in
            // subqueuefiles.erf), so it is out of step from the first line on: no XML parser takes it. 2026-11-01.
            CHECK(!program);
            ++files;
            continue;
        }
        if (entry.path().filename() == "dadefaultsubxbox.sub.xml")
        {
            // The console's program names attributes the PC exe has none for (immediatelocale, occluder_candidate,
            // insignificant, 0x00472670): its builder errors, and the PC game would fall back. 2026-11-01.
            CHECK(!program);
            ++files;
            continue;
        }
        REQUIRE(program);
        CHECK(!program->groups.empty());
        ++files;
        if (entry.path().filename() != "dadefaultsub.sub.xml")
        {
            continue;
        }
        // dadefaultsub.sub.xml (packages/core/data/subqueuefiles.erf, 2026-11-01): the draw order the
        // frame is made in, from the shadow maps to the primitives.
        CHECK(program->id == "DADefaultSub");
        CHECK(program->groups.front().id == "Shadow0_visible");
        CHECK(program->groups.back().id == "Primitives");
        // The shadow group frames partition 0, keeps a copy and needs shadows on.
        const sub::Group &shadow = program->groups.front();
        REQUIRE(shadow.classes.size() == 1);
        CHECK(shadow.classes[0].keepCopy);
        CHECK(shadow.classes[0].gate == sub::Gate::ShadowsEnabled);
        CHECK(shadow.classes[0].OperationsAt(sub::OperationWhen::Before).size() == 1);
        CHECK(shadow.classes[0].OperationsAt(sub::OperationWhen::After).size() == 1);
        // The faded trees are rendered by the program, in the group of that name.
        bool fadedTrees = false;
        bool blur = false;
        for (const sub::Group &group : program->groups)
        {
            for (const sub::Class &cls : group.classes)
            {
                for (const sub::Operation &operation : cls.operations)
                {
                    fadedTrees = fadedTrees || operation.type == sub::OperationType::RenderFadedSpeedTrees;
                    blur = blur || operation.type == sub::OperationType::BlurPartition;
                }
            }
        }
        CHECK(fadedTrees);
        CHECK(blur);
        // A prop is drawn after characters: MultipassBatched leaves it out, Props takes it.
        sub::Node prop = NodeOf("prop");
        prop.partitions = 0;
        const auto placed = program->Classify(prop);
        REQUIRE(!placed.empty());
        CHECK(program->groups[placed.back().group].id == "Props");
        // A fading character goes to FadeoutGeometry, drawn farthest first.
        sub::Node fading = NodeOf("character", sub::attribute::kFadeoutObject);
        const auto faded = program->Classify(fading);
        REQUIRE(!faded.empty());
        CHECK(program->groups[faded.back().group].id == "FadeoutGeometry");
        // A hidden node of a hidden-excluding group is in none of the geometry groups.
        sub::Node hidden = NodeOf("terrain", sub::attribute::kHidden);
        for (const sub::Placement &placement : program->Classify(hidden))
        {
            CHECK(program->groups[placement.group].id != "Terrain");
        }
    }
    CHECK(files >= 1);
}
