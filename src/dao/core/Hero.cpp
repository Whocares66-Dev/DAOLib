// A saved hero (.chr), read 2026-11-06 in the GOG exe by a lib-agent reader (DAOLib #326, for
// DAOParagonEngine's character creation) and checked against three heroes the game wrote:
// - WriteCharFile (0x007BE490, GUIPaperdoll's, from GUICharGen's CharGenSaveInfo handler
//   0x007371C0, which the review screen's Save button sends) writes GFF4 "CHAR V0.1" (pushed at
//   0x007BE51D) with the writer dao/core/Gff4Writer.h lays out. Its templates, in the order it
//   adds them (0x007BE51D..0x007BEA1C): mod, mat, mat4, a second mat4 holding a texture's fields
//   (the .mop's TEX), MOP, ABLT, APPR, and TOP the root.
// - It fills them (0x007BEA21..0x007BFBF2) in the order Write follows: the head (the name
//   "Head_Morph", the parts, the tints, the morph targets, the material values, vectors and
//   textures), then the appearance (gender, race, class, background, the name, the abilities, the
//   attributes from a std::map by id, the seven portrait floats), then the preset's name.
// - ReadCharFile (0x007BA7F0) checks only "GFF " and "V4.0" and reads the appearance by label, a
//   field it lacks 0 or empty; the head and the preset's name are read by 0x007B4210 when the hero
//   is picked. LoadSeededCharacters (0x007BB090) lists *.chr under Pre-release Characters\ (the
//   alias SEEDEDCHARACTERS) and the folders in it, in the order FindFirstFileW gives (0x004C97C0).
// The game wrote the same bytes when the same hero was saved twice.
#include "dao/core/Hero.h"

#include "dao/core/Gff4Writer.h"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <system_error>

namespace dao::core::hero
{

namespace
{

using gff4::kList;
using gff4::Type;

// dev/gff4_fields.csv names them: the head's are the .mop's (dao/core/HeadPreset.cpp).
constexpr std::uint32_t kHead = 0x3D0F4;       // CharMOP
constexpr std::uint32_t kAppearance = 0x3D0F5; // CharApp
constexpr std::uint32_t kPresetName = 0x3D0FD; // CharHeadName
constexpr std::uint32_t kGender = 0x3D0F6;
constexpr std::uint32_t kRace = 0x3D0F7;
constexpr std::uint32_t kClass = 0x3D0F8;
constexpr std::uint32_t kBackground = 0x3D0F9;
constexpr std::uint32_t kAttributes = 0x3D0FA;
constexpr std::uint32_t kAbilities = 0x3D0FB;
constexpr std::uint32_t kName = 0x3D0FC;
constexpr std::uint32_t kAttributeId = 0x3D0FE;
constexpr std::uint32_t kAttributePoints = 0x3D0FF;
constexpr std::uint32_t kPortrait = 0x3D100; // unnamed in the field table

constexpr std::uint32_t kParts = 23000;
constexpr std::uint32_t kTints = 23001;
constexpr std::uint32_t kMorphName = 23008;
constexpr std::uint32_t kNodeName = 23009;
constexpr std::uint32_t kParamName = 23010;
constexpr std::uint32_t kParamIndex = 23011;
constexpr std::uint32_t kParamValue = 23012;
constexpr std::uint32_t kParamVector = 23013;
constexpr std::uint32_t kValues = 23014;
constexpr std::uint32_t kVectors = 23015;
constexpr std::uint32_t kModelName = 23016;
constexpr std::uint32_t kModelValue = 23017;
constexpr std::uint32_t kModels = 23018;
constexpr std::uint32_t kTexNodeName = 23019;
constexpr std::uint32_t kTexParamName = 23020;
constexpr std::uint32_t kTexName = 23021;
constexpr std::uint32_t kTextures = 23022;

constexpr wchar_t kHeadName[] = L"Head_Morph"; // the literal the game writes (0x00B1BB38)

void WriteHead(gff4::Writer &out, const gff4::StructAt &head, const preset::Preset &preset)
{
    out.String(head, kMorphName, kHeadName);
    out.Strings(head, kParts, preset.parts);
    out.Strings(head, kTints, preset.tints);
    const auto models = out.Structs(head, kModels, preset.models.size());
    for (std::size_t i = 0; i < models.size(); ++i)
    {
        out.String(models[i], kModelName, preset.models[i].model);
        out.F32(models[i], kModelValue, preset.models[i].weight);
    }
    const auto values = out.Structs(head, kValues, preset.values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        out.String(values[i], kNodeName, preset.values[i].node);
        out.String(values[i], kParamName, preset.values[i].parameter);
        out.S32(values[i], kParamIndex, preset.values[i].index);
        out.F32(values[i], kParamValue, preset.values[i].value);
    }
    const auto vectors = out.Structs(head, kVectors, preset.vectors.size());
    for (std::size_t i = 0; i < vectors.size(); ++i)
    {
        out.String(vectors[i], kNodeName, preset.vectors[i].node);
        out.String(vectors[i], kParamName, preset.vectors[i].parameter);
        out.S32(vectors[i], kParamIndex, preset.vectors[i].index);
        out.Vector4(vectors[i], kParamVector, preset.vectors[i].value);
    }
    const auto textures = out.Structs(head, kTextures, preset.textures.size());
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        out.String(textures[i], kTexNodeName, preset.textures[i].node);
        out.String(textures[i], kTexParamName, preset.textures[i].parameter);
        out.String(textures[i], kTexName, preset.textures[i].texture);
    }
}

void WriteAppearance(gff4::Writer &out, const gff4::StructAt &appearance, const Hero &hero)
{
    out.U8(appearance, kGender, hero.gender);
    out.S32(appearance, kRace, hero.race);
    out.S32(appearance, kClass, hero.characterClass);
    out.S32(appearance, kBackground, hero.background);
    out.String(appearance, kName, hero.name);
    out.Numbers(appearance, kAbilities, hero.abilities);
    std::vector<Attribute> attributes = hero.attributes;
    std::ranges::stable_sort(attributes, {}, &Attribute::id);
    const auto written = out.Structs(appearance, kAttributes, attributes.size());
    for (std::size_t i = 0; i < written.size(); ++i)
    {
        out.U32(written[i], kAttributeId, attributes[i].id);
        out.S32(written[i], kAttributePoints, attributes[i].points);
    }
    out.Floats(appearance, kPortrait, hero.portrait);
}

std::wstring Text(const gff4::Struct &in, std::uint32_t label)
{
    return in.String(label).value_or(std::wstring{});
}

} // namespace

std::optional<Hero> HeroOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "CHAR" || !root)
    {
        return std::nullopt;
    }
    Hero hero;
    if (const auto head = root->Child(kHead))
    {
        hero.head = preset::PresetIn(*head);
    }
    hero.preset = Text(*root, kPresetName);
    if (const auto appearance = root->Child(kAppearance))
    {
        hero.gender = appearance->Get<std::uint8_t>(kGender).value_or(0);
        hero.race = appearance->Get<std::int32_t>(kRace).value_or(0);
        hero.characterClass = appearance->Get<std::int32_t>(kClass).value_or(0);
        hero.background = appearance->Get<std::int32_t>(kBackground).value_or(0);
        hero.name = Text(*appearance, kName);
        for (const gff4::Struct &attribute : appearance->Structs(kAttributes))
        {
            hero.attributes.push_back({.id = attribute.Get<std::uint32_t>(kAttributeId).value_or(0),
                                       .points = attribute.Get<std::int32_t>(kAttributePoints).value_or(0)});
        }
        for (const std::uint32_t ability : appearance->Numbers(kAbilities))
        {
            hero.abilities.push_back(static_cast<std::int32_t>(ability));
        }
        const auto floats = appearance->ListBytes(kPortrait, Type::F32);
        for (std::size_t i = 0; i < hero.portrait.size() && (i + 1) * sizeof(float) <= floats.size(); ++i)
        {
            std::memcpy(&hero.portrait.at(i), floats.data() + (i * sizeof(float)), sizeof(float));
        }
    }
    return hero;
}

std::vector<std::byte> Write(const Hero &hero)
{
    gff4::Writer out("CHAR", "V0.1");
    const auto model = out.AddTemplate("mod ", {{kModelName, Type::String}, {kModelValue, Type::F32}});
    const auto value = out.AddTemplate(
        "mat ",
        {{kNodeName, Type::String}, {kParamName, Type::String}, {kParamIndex, Type::S32}, {kParamValue, Type::F32}});
    const auto vector = out.AddTemplate("mat4", {{kNodeName, Type::String},
                                                 {kParamName, Type::String},
                                                 {kParamIndex, Type::S32},
                                                 {kParamVector, Type::Vector4}});
    const auto texture = out.AddTemplate(
        "mat4", {{kTexNodeName, Type::String}, {kTexParamName, Type::String}, {kTexName, Type::String}});
    const auto head = out.AddTemplate("MOP ", {{kMorphName, Type::String},
                                               {kParts, Type::String, kList},
                                               {kTints, Type::String, kList},
                                               {kModels, model, kList},
                                               {kValues, value, kList},
                                               {kVectors, vector, kList},
                                               {kTextures, texture, kList}});
    const auto attribute = out.AddTemplate("ABLT", {{kAttributeId, Type::U32}, {kAttributePoints, Type::S32}});
    const auto appearance = out.AddTemplate("APPR", {{kGender, Type::U8},
                                                     {kRace, Type::S32},
                                                     {kClass, Type::S32},
                                                     {kBackground, Type::S32},
                                                     {kName, Type::String},
                                                     {kAbilities, Type::S32, kList},
                                                     {kPortrait, Type::F32, kList},
                                                     {kAttributes, attribute, kList}});
    const gff4::StructAt root =
        out.Root("TOP ", {{kHead, head, 0}, {kAppearance, appearance, 0}, {kPresetName, Type::String}});
    WriteHead(out, out.Child(root, kHead), hero.head);
    WriteAppearance(out, out.Child(root, kAppearance), hero);
    out.String(root, kPresetName, hero.preset);
    return out.Save();
}

std::vector<std::filesystem::path> HeroesIn(const std::filesystem::path &folder)
{
    std::vector<std::filesystem::path> found;
    std::error_code error;
    for (auto it = std::filesystem::recursive_directory_iterator(folder, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
    {
        std::wstring extension = it->path().extension().wstring();
        std::ranges::transform(extension, extension.begin(), [](wchar_t c) { return std::towlower(c); });
        if (it->is_regular_file(error) && extension == L".chr")
        {
            found.push_back(it->path());
        }
    }
    return found;
}

} // namespace dao::core::hero
