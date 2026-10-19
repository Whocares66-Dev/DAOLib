#include "dao/core/Effect.h"

#include "dao/core/ModelNodes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace dao::core::effect
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kUint16List = 8;
constexpr std::uint32_t kVector3List = 16;
constexpr std::uint32_t kMaterialLibrary = 6002;
constexpr std::uint32_t kLightColour = 6007;
constexpr std::uint32_t kLightRadius = 6008;
constexpr std::uint32_t kLightIsStatic = 6009;
constexpr std::uint32_t kBirthRate = 6011;
constexpr std::uint32_t kBirthRateRange = 6012;
constexpr std::uint32_t kLife = 6013;
constexpr std::uint32_t kLifeRange = 6014;
constexpr std::uint32_t kScaleRange = 6015;
constexpr std::uint32_t kInitialSpeed = 6016;
constexpr std::uint32_t kInitialSpeedRange = 6017;
constexpr std::uint32_t kAcceleration = 6018;
constexpr std::uint32_t kInitialRotationSpeed = 6019;
constexpr std::uint32_t kInitialRotationSpeedRange = 6020;
constexpr std::uint32_t kRotationalAcceleration = 6021;
constexpr std::uint32_t kInvMovementSpreadUpdateDelay = 6022;
constexpr std::uint32_t kSpawnSpreadX = 6023;
constexpr std::uint32_t kSpawnSpreadY = 6024;
constexpr std::uint32_t kMovementSpreadX = 6025;
constexpr std::uint32_t kMovementSpreadY = 6026;
constexpr std::uint32_t kPerMeter = 6028;
constexpr std::uint32_t kWind = 6030;
constexpr std::uint32_t kGravityMultiplier = 6031;
constexpr std::uint32_t kFollowPath = 6032;
constexpr std::uint32_t kLinkTogether = 6033;
constexpr std::uint32_t kOnlyWhenVisible = 6034;
constexpr std::uint32_t kCollisions = 6035;
constexpr std::uint32_t kInheritVelocity = 6036;
constexpr std::uint32_t kOrientationBehavior = 6037;
constexpr std::uint32_t kParticleInheritance = 6038;
constexpr std::uint32_t kAgeLife = 6040;
constexpr std::uint32_t kAgeScaleX = 6041;
constexpr std::uint32_t kAgeScaleY = 6042;
constexpr std::uint32_t kAgeColour = 6043;
constexpr std::uint32_t kWithinVolume = 6045;
constexpr std::uint32_t kInvertNormals = 6046;
constexpr std::uint32_t kFlipbookFps = 6180;
constexpr std::uint32_t kFlipbookRows = 6181;
constexpr std::uint32_t kFlipbookColumns = 6182;
constexpr std::uint32_t kFlipbookRandomStart = 6183;
constexpr std::uint32_t kTargetName = 6184;
constexpr std::uint32_t kTargetAttraction = 6185;
constexpr std::uint32_t kTargetRadius = 6186;
constexpr std::uint32_t kTracksTarget = 6187;
constexpr std::uint32_t kKillWhenHit = 6188;
constexpr std::uint32_t kFlipbookType = 6189;
constexpr std::uint32_t kDataIsIndexStream = 6168;
constexpr std::uint32_t kEmitType = 6234;
constexpr std::uint32_t kPhysicsEmitter = 6239;
constexpr std::uint32_t kPhysicsObjectSpawn = 6243;
constexpr std::uint32_t kAttachmentType = 6246;
constexpr std::uint32_t kAttachmentName = 6247;
constexpr std::uint32_t kIntensityVariation = 6249;
constexpr std::uint32_t kIntensityPeriod = 6250;
constexpr std::uint32_t kIntensityPeriodDelta = 6251;
constexpr std::uint32_t kAgeColourMultiplier = 6279;
constexpr std::uint32_t kAgeScaleXMultiplier = 6280;
constexpr std::uint32_t kAgeScaleYMultiplier = 6281;
constexpr std::uint32_t kBounciness = 6282;
constexpr std::uint32_t kFriction = 6283;
constexpr std::uint32_t kMeshParticleModel = 6284;
constexpr std::uint32_t kVolumeType = 6285;
constexpr std::uint32_t kVolumeRadius = 6286;
constexpr std::uint32_t kCylinderLength = 6287;
constexpr std::uint32_t kCylinderAxis = 6288;
constexpr std::uint32_t kVolumeBoxMin = 6289;
constexpr std::uint32_t kVolumeBoxMax = 6290;
constexpr std::uint32_t kNormalsAsDirection = 6291;
constexpr std::uint32_t kWorldAxisAcceleration = 6294;
constexpr std::uint32_t kAffectDomain = 6296;
constexpr std::uint32_t kVertexFormat = 6297;
constexpr std::uint32_t kObjectSpaceAcceleration = 6298;
constexpr std::uint32_t kInitialRotation = 6299;
constexpr std::uint32_t kInitialRotationRange = 6300;
constexpr std::uint32_t kMeshUpAxis = 6302;
constexpr std::uint32_t kMeshRollAxis = 6303;
constexpr std::uint32_t kUvDistributionSize = 6308;
constexpr std::uint32_t kIgnoreDistortion = 6309;
constexpr std::uint32_t kSplatWidth = 6310;
constexpr std::uint32_t kSplatHeight = 6311;
constexpr std::uint32_t kSplatSamplesWidth = 6312;
constexpr std::uint32_t kSplatSamplesHeight = 6313;
constexpr std::uint32_t kSplatOrientationRange = 6314;
constexpr std::uint32_t kSplatLife = 6315;
constexpr std::uint32_t kSplatFlipbookType = 6316;
constexpr std::uint32_t kSplatFlipbookFps = 6317;
constexpr std::uint32_t kSplatFlipbookRows = 6318;
constexpr std::uint32_t kSplatFlipbookColumns = 6319;
constexpr std::uint32_t kSplatFlipbookRandomStart = 6320;
constexpr std::uint32_t kCanSplat = 6321;
constexpr std::uint32_t kLod = 6323;
constexpr std::uint32_t kSplatMaterial = 6324;
constexpr std::uint32_t kUserParamName = 6325;
constexpr std::uint32_t kRemoteDecal = 6327;
constexpr std::uint32_t kRemoteFresnelFalloff = 6328;
constexpr std::uint32_t kRemoteInvertFresnel = 6329;
constexpr std::uint32_t kRemoteAlpha = 6331;
constexpr std::uint32_t kRemoteTint = 6332;
constexpr std::uint32_t kPresimulateTime = 6333;
constexpr std::uint32_t kCanBeOccluded = 6339;
constexpr std::uint32_t kVariationTint = 6340;
constexpr std::uint32_t kSplatHoldLastFrame = 6341;
constexpr std::uint32_t kSpawnOnSurface = 6342;
constexpr std::uint32_t kNormalForVelocity = 6343;
constexpr std::uint32_t kSplatColourMultiplier = 6344;
constexpr std::uint32_t kLightVersion = 6345;
// A Part's structs deeper than this are left out: the game's go three deep under a node.
constexpr std::size_t kPartsDeep = 8;

bool SameName(std::wstring_view a, std::wstring_view b)
{
    return std::ranges::equal(a, b, [](wchar_t x, wchar_t y) { return std::towlower(x) == std::towlower(y); });
}

float F(const gff4::Struct &from, std::uint32_t label, float otherwise = 0)
{
    return from.Get<float>(label).value_or(otherwise);
}

std::uint8_t U8(const gff4::Struct &from, std::uint32_t label)
{
    return from.Get<std::uint8_t>(label).value_or(0);
}

bool Flag(const gff4::Struct &from, std::uint32_t label)
{
    return U8(from, label) != 0;
}

std::uint32_t U32(const gff4::Struct &from, std::uint32_t label)
{
    return from.Get<std::uint32_t>(label).value_or(0);
}

std::wstring Text(const gff4::Struct &from, std::uint32_t label)
{
    return from.String(label).value_or(L"");
}

template <std::size_t N> std::array<float, N> Floats(const gff4::Struct &from, std::uint32_t label)
{
    std::array<float, N> out{};
    const std::vector<float> read = from.Floats(label);
    std::copy_n(read.begin(), std::min(N, read.size()), out.begin());
    return out;
}

std::optional<Raw> RawOf(const gff4::Struct &from, const gff4::Field &field)
{
    const auto type = static_cast<gff4::Type>(field.type);
    Raw raw{.label = field.label, .type = type, .value = std::int64_t{0}};
    const auto whole = [&]<class T>() -> std::optional<Raw> {
        if (const auto value = from.Get<T>(field.label))
        {
            raw.value = static_cast<std::int64_t>(*value);
            return raw;
        }
        return std::nullopt;
    };
    switch (type)
    {
    case gff4::Type::U8:
        return whole.operator()<std::uint8_t>();
    case gff4::Type::S8:
        return whole.operator()<std::int8_t>();
    case gff4::Type::U16:
        return whole.operator()<std::uint16_t>();
    case gff4::Type::S16:
        return whole.operator()<std::int16_t>();
    case gff4::Type::U32:
        return whole.operator()<std::uint32_t>();
    case gff4::Type::S32:
        return whole.operator()<std::int32_t>();
    case gff4::Type::U64:
        return whole.operator()<std::uint64_t>();
    case gff4::Type::S64:
        return whole.operator()<std::int64_t>();
    case gff4::Type::F64:
        if (const auto value = from.Get<double>(field.label))
        {
            raw.value = std::vector<float>{static_cast<float>(*value)};
            return raw;
        }
        return std::nullopt;
    case gff4::Type::String:
        if (auto text = from.String(field.label))
        {
            raw.value = std::move(*text);
            return raw;
        }
        return std::nullopt;
    case gff4::Type::TlkString:
        if (const auto tlk = from.Tlk(field.label))
        {
            raw.value = static_cast<std::int64_t>(tlk->strref);
            return raw;
        }
        return std::nullopt;
    default:
        if (std::vector<float> floats = from.Floats(field.label); !floats.empty())
        {
            raw.value = std::move(floats);
            return raw;
        }
        return std::nullopt;
    }
}

// Every field of a struct that is no list nor struct, as the file has it.
std::vector<Raw> FieldsOf(const gff4::Struct &from)
{
    std::vector<Raw> out;
    for (const gff4::Field &field : from.Shape().fields)
    {
        if ((field.flags & (gff4::kList | gff4::kStruct)) != 0 ||
            field.type == static_cast<std::uint16_t>(gff4::Type::Generic))
        {
            continue;
        }
        if (std::optional<Raw> raw = RawOf(from, field))
        {
            out.push_back(std::move(*raw));
        }
    }
    return out;
}

Part PartOf(const gff4::Struct &from, std::size_t deep)
{
    Part part{.label = from.Label(), .fields = FieldsOf(from), .parts = {}};
    if (deep < kPartsDeep)
    {
        for (const gff4::Struct &child : from.Structs(model::kChildren))
        {
            part.parts.push_back(PartOf(child, deep + 1));
        }
    }
    return part;
}

bool IsNode(const gff4::Struct &from)
{
    return from.Shape().Find(model::kChildren) != nullptr && from.Shape().Find(model::kMmhName) != nullptr;
}

std::vector<AgeKey> AgeMapOf(const gff4::Struct &owner)
{
    std::vector<AgeKey> keys;
    for (const gff4::Struct &map : owner.Structs(model::kChildren))
    {
        if (map.Label() != "amap")
        {
            continue;
        }
        for (const gff4::Struct &key : map.Structs(model::kChildren))
        {
            keys.push_back({.lifeElapsed = F(key, kAgeLife),
                            .scaleX = F(key, kAgeScaleX, 1),
                            .scaleY = F(key, kAgeScaleY, 1),
                            .colour = Floats<4>(key, kAgeColour)});
        }
    }
    return keys;
}

// A mesh volume's points and indices, each in a "data" struct under it: the points a Vector3
// list, the indices a U16 list in the struct that is the index stream.
void MeshOf(const gff4::Struct &volume, SpawnVolume &out)
{
    for (const gff4::Struct &data : volume.Structs(model::kChildren))
    {
        if (data.Label() != "data")
        {
            continue;
        }
        if (Flag(data, kDataIsIndexStream))
        {
            const std::span<const std::byte> bytes = data.ListBytes(kUint16List, gff4::Type::U16);
            out.indices.resize(bytes.size() / 2);
            std::memcpy(out.indices.data(), bytes.data(), out.indices.size() * 2);
            continue;
        }
        const std::span<const std::byte> bytes = data.ListBytes(kVector3List, gff4::Type::Vector3);
        for (std::size_t at = 0; at + 16 <= bytes.size(); at += 16)
        {
            Vector point{};
            std::memcpy(point.data(), bytes.data() + at, 12);
            out.points.push_back(point);
        }
    }
}

SpawnVolume SpawnVolumeOf(const gff4::Struct &volume)
{
    SpawnVolume out{.type = U8(volume, kVolumeType),
                    .spawnWithinVolume = Flag(volume, kWithinVolume),
                    .invertNormals = Flag(volume, kInvertNormals),
                    .normalsAsDirection = Flag(volume, kNormalsAsDirection),
                    .radius = F(volume, kVolumeRadius),
                    .cylinderLength = F(volume, kCylinderLength),
                    .cylinderAxis = Floats<3>(volume, kCylinderAxis),
                    .boxMin = Floats<3>(volume, kVolumeBoxMin),
                    .boxMax = Floats<3>(volume, kVolumeBoxMax),
                    .points = {},
                    .indices = {}};
    MeshOf(volume, out);
    return out;
}

Splat SplatOf(const gff4::Struct &splat)
{
    return {.material = Text(splat, kSplatMaterial),
            .width = F(splat, kSplatWidth),
            .height = F(splat, kSplatHeight),
            .samplesWidth = U32(splat, kSplatSamplesWidth),
            .samplesHeight = U32(splat, kSplatSamplesHeight),
            .orientationRange = F(splat, kSplatOrientationRange),
            .life = F(splat, kSplatLife),
            .flipbookType = U8(splat, kSplatFlipbookType),
            .flipbookFramesPerSecond = F(splat, kSplatFlipbookFps),
            .flipbookRows = U8(splat, kSplatFlipbookRows),
            .flipbookColumns = U8(splat, kSplatFlipbookColumns),
            .flipbookRandomStartFrame = Flag(splat, kSplatFlipbookRandomStart),
            .holdLastFrame = Flag(splat, kSplatHoldLastFrame),
            .ageMapColourMultiplier = Floats<4>(splat, kSplatColourMultiplier),
            .ageMap = AgeMapOf(splat)};
}

// What of an emitter is its structs: the age map, the spawn volume, the attachments and the splat.
void EmitterPartsOf(const gff4::Struct &node, Emitter &out)
{
    out.ageMap = AgeMapOf(node);
    for (const gff4::Struct &child : node.Structs(model::kChildren))
    {
        if (child.Label() == "spnv")
        {
            out.spawnVolume = SpawnVolumeOf(child);
        }
        else if (child.Label() == "spla")
        {
            out.splat = SplatOf(child);
        }
        else if (child.Label() == "emas")
        {
            for (const gff4::Struct &attachment : child.Structs(model::kChildren))
            {
                out.attachments.push_back({.type = U32(attachment, kAttachmentType),
                                           .name = Text(attachment, kAttachmentName),
                                           .spawnOnSurface = Flag(attachment, kSpawnOnSurface),
                                           .useNormalForVelocity = Flag(attachment, kNormalForVelocity)});
            }
        }
    }
}

Emitter EmitterOf(const gff4::Struct &node)
{
    Emitter out{
        .material = Text(node, model::kMaterialObject),
        .materialLibrary = Text(node, kMaterialLibrary),
        .type = U8(node, kEmitType),
        .birthRate = F(node, kBirthRate),
        .birthRateRange = F(node, kBirthRateRange),
        .life = F(node, kLife),
        .lifeRange = F(node, kLifeRange),
        .scaleRange = F(node, kScaleRange),
        .initialSpeed = F(node, kInitialSpeed),
        .initialSpeedRange = F(node, kInitialSpeedRange),
        .acceleration = F(node, kAcceleration),
        .initialRotation = F(node, kInitialRotation),
        .initialRotationRange = F(node, kInitialRotationRange),
        .initialRotationSpeed = F(node, kInitialRotationSpeed),
        .initialRotationSpeedRange = F(node, kInitialRotationSpeedRange),
        .rotationalAcceleration = F(node, kRotationalAcceleration),
        .invMovementSpreadUpdateDelay = F(node, kInvMovementSpreadUpdateDelay),
        .spawnSpreadX = F(node, kSpawnSpreadX),
        .spawnSpreadY = F(node, kSpawnSpreadY),
        .movementSpreadX = F(node, kMovementSpreadX),
        .movementSpreadY = F(node, kMovementSpreadY),
        .gravityMultiplier = F(node, kGravityMultiplier),
        .worldAxisAcceleration = Floats<3>(node, kWorldAxisAcceleration),
        .objectSpaceAcceleration = Flag(node, kObjectSpaceAcceleration),
        .birthRateInParticlesPerMeter = Flag(node, kPerMeter),
        .affectedByWind = Flag(node, kWind),
        .followPath = Flag(node, kFollowPath),
        .linkParticlesTogether = Flag(node, kLinkTogether),
        .updateOnlyWhenVisible = Flag(node, kOnlyWhenVisible),
        .enableCollisions = Flag(node, kCollisions),
        .inheritVelocityInsteadOfPos = Flag(node, kInheritVelocity),
        .bounciness = F(node, kBounciness),
        .friction = F(node, kFriction),
        .orientationBehavior = U32(node, kOrientationBehavior),
        .particleInheritance = U32(node, kParticleInheritance),
        .flipbookType = U8(node, kFlipbookType),
        .flipbookFramesPerSecond = F(node, kFlipbookFps),
        .flipbookRows = U8(node, kFlipbookRows),
        .flipbookColumns = U8(node, kFlipbookColumns),
        .flipbookRandomStartFrame = Flag(node, kFlipbookRandomStart),
        .targetName = Text(node, kTargetName),
        .targetAttraction = F(node, kTargetAttraction),
        .targetRadius = F(node, kTargetRadius),
        .spawnDirectionTracksTarget = Flag(node, kTracksTarget),
        .killParticleWhenTargetHit = Flag(node, kKillWhenHit),
        .isPhysicsEmitter = Flag(node, kPhysicsEmitter),
        .isPhysicsObjectSpawnEmitter = Flag(node, kPhysicsObjectSpawn),
        .meshParticleModel = Text(node, kMeshParticleModel),
        .meshParticleUpAxis = Floats<3>(node, kMeshUpAxis),
        .meshParticleRollAxis = Floats<3>(node, kMeshRollAxis),
        .vertexFormat = U8(node, kVertexFormat),
        .uvDistributionSize = U32(node, kUvDistributionSize),
        .ignoreDistortion = Flag(node, kIgnoreDistortion),
        .canParticlesSplat = Flag(node, kCanSplat),
        .lod = U8(node, kLod),
        .userParamName = Text(node, kUserParamName),
        .useVariationTint = Flag(node, kVariationTint),
        .ageMapColourMultiplier = Floats<4>(node, kAgeColourMultiplier),
        .ageMapScaleXMultiplier = F(node, kAgeScaleXMultiplier, 1),
        .ageMapScaleYMultiplier = F(node, kAgeScaleYMultiplier, 1),
        .ageMap = {},
        .spawnVolume = std::nullopt,
        .attachments = {},
        .splat = std::nullopt,
    };
    EmitterPartsOf(node, out);
    return out;
}

Light LightOf(const gff4::Struct &node)
{
    return {.colour = Floats<4>(node, kLightColour),
            .radius = F(node, kLightRadius),
            .isStatic = Flag(node, kLightIsStatic),
            .intensityVariation = F(node, kIntensityVariation),
            .intensityPeriod = F(node, kIntensityPeriod),
            .intensityPeriodDelta = F(node, kIntensityPeriodDelta),
            .affectDomain = U8(node, kAffectDomain),
            .canBeOccluded = Flag(node, kCanBeOccluded),
            .useVariationTint = Flag(node, kVariationTint),
            .version = U8(node, kLightVersion)};
}

// Whether a child struct of a node is read by a member of its Item: its transform and exports
// (model::Node's), its bounds, and an emitter's parts.
bool IsRead(const std::string &kind, const std::string &child)
{
    if (child == "trsl" || child == "rota" || child == "scal" || child == "xprt" || child == "bbox")
    {
        return true;
    }
    return kind == "nemt" && (child == "amap" || child == "spnv" || child == "emas" || child == "spla");
}

Item ItemOf(model::Node node, const gff4::Struct &from)
{
    Item item{.node = std::move(node),
              .emitter = std::nullopt,
              .light = std::nullopt,
              .material = {},
              .bounds = std::nullopt,
              .fields = FieldsOf(from),
              .parts = {}};
    const std::string &kind = item.node.kind;
    if (kind == "nemt")
    {
        item.emitter = EmitterOf(from);
    }
    else if (kind == "nplt")
    {
        item.light = LightOf(from);
    }
    if (kind == "nemg" || kind == "mshh")
    {
        item.material = Text(from, model::kMaterialObject);
    }
    for (const gff4::Struct &child : from.Structs(model::kChildren))
    {
        if (IsNode(child))
        {
            continue;
        }
        if (child.Label() == "bbox")
        {
            item.bounds = std::array{Floats<4>(child, model::kBoxMin), Floats<4>(child, model::kBoxMax)};
        }
        if (!IsRead(kind, child.Label()))
        {
            item.parts.push_back(PartOf(child, 0));
        }
    }
    return item;
}

std::wstring CellText(const m2da::Merged &table, const m2da::Row &row, std::string_view column)
{
    const GdaFile::Cell *cell = m2da::CellOf(table, row, column);
    const auto *text = cell != nullptr ? std::get_if<std::wstring>(cell) : nullptr;
    return text != nullptr ? *text : std::wstring();
}

double CellNumber(const m2da::Merged &table, const m2da::Row &row, std::string_view column)
{
    const GdaFile::Cell *cell = m2da::CellOf(table, row, column);
    if (cell == nullptr)
    {
        return 0;
    }
    if (const auto *whole = std::get_if<std::int64_t>(cell))
    {
        return static_cast<double>(*whole);
    }
    const auto *real = std::get_if<double>(cell);
    return real != nullptr ? *real : 0;
}

Sound SoundOf(const m2da::Merged &table, const m2da::Row &row, const std::string &column)
{
    return {.name = CellText(table, row, column),
            .volume = static_cast<float>(CellNumber(table, row, column + "Volume")),
            .pitch = static_cast<float>(CellNumber(table, row, column + "Pitch"))};
}

std::wstring WithExtension(const std::wstring &stem, const wchar_t *extension)
{
    return stem.empty() ? std::wstring() : stem + extension;
}

} // namespace

std::wstring Sized::File() const
{
    return WithExtension(model, L".mmh");
}

// Read 2026-10-18 by lib-agent-2 in the exe (0x007EA550, dev/EFFECTS.md, "How vfx_base's model is
// chosen").
std::optional<Chosen> Row::ModelFor(float ownerSize) const
{
    if (sizes.empty())
    {
        return std::nullopt;
    }
    std::size_t at = 0;
    if (ownerSize > 0 && sizes[0].size > 0 && sizes.size() > 1 && ownerSize > sizes[0].size)
    {
        at = sizes.size() - 1;
        for (std::size_t i = 1; i < sizes.size(); ++i)
        {
            if (ownerSize <= sizes[i].size)
            {
                const bool upper = sizes[i].size - ownerSize < ownerSize - sizes[i - 1].size;
                at = upper ? i : i - 1;
                break;
            }
        }
    }
    const Sized &taken = sizes[at];
    return Chosen{.model = taken, .scale = ownerSize > 0 && taken.size > 0 ? ownerSize / taken.size : 1.0F};
}

std::wstring Row::TreeFile() const
{
    return WithExtension(tree, L".anb");
}

std::optional<Row> RowOf(const m2da::Merged &effects, std::int64_t id)
{
    const m2da::Row *row = m2da::Taken(effects, id);
    if (row == nullptr)
    {
        return std::nullopt;
    }
    const auto number = [&](std::string_view column) { return CellNumber(effects, *row, column); };
    const auto text = [&](std::string_view column) { return CellText(effects, *row, column); };
    Row out{.id = id,
            .label = text("Label"),
            .type = text("Type"),
            .fixedOrientation = number("FixedOrientation") != 0,
            .alignToGround = number("AlignToGround") != 0,
            .sizes = {},
            .tree = text("BlendTreeName"),
            .crust = number("CrustEffect") != 0,
            .impact = SoundOf(effects, *row, "SoundImpact"),
            .duration = SoundOf(effects, *row, "SoundDuration"),
            .cessation = SoundOf(effects, *row, "SoundCessation"),
            .tint = text("Tint"),
            .loadGlobal = static_cast<std::int64_t>(number("LoadGlobal")),
            .pauseAnimate = number("PauseAnimate") != 0,
            .priority = static_cast<std::int64_t>(number("Priority")),
            .showInPaperDoll = number("ShowInPaperDoll") != 0};
    for (const auto &[model, size] :
         {std::pair{"ModelName1", "Size1"}, std::pair{"ModelName2", "Size2"}, std::pair{"ModelName3", "Size3"}})
    {
        std::wstring name = text(model);
        const std::size_t first = name.find_first_not_of(L" 	");
        name = first == std::wstring::npos ? std::wstring()
                                           : name.substr(first, name.find_last_not_of(L" 	") - first + 1);
        if (name.empty() || (!out.sizes.empty() && out.sizes[0].size <= 0))
        {
            break;
        }
        out.sizes.push_back({.model = std::move(name), .size = static_cast<float>(number(size))});
    }
    return out;
}

const Item *Effect::Find(std::wstring_view node) const
{
    const auto found = std::ranges::find_if(items, [&](const Item &item) { return SameName(item.node.name, node); });
    return found != items.end() ? &*found : nullptr;
}

std::optional<Driven> Effect::Drives(std::wstring_view channel) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        for (const model::Export &exported : items[i].node.exports)
        {
            if (SameName(exported.name, channel))
            {
                return Driven{
                    .item = i, .exported = &exported, .parameter = static_cast<Parameter>(exported.variableType)};
            }
        }
    }
    return std::nullopt;
}

std::optional<Effect> EffectOf(const gff4::File &model)
{
    std::optional<model::Hierarchy> hierarchy = model::HierarchyOf(model);
    const std::optional<gff4::Struct> root = model.Root();
    if (!hierarchy || !root)
    {
        return std::nullopt;
    }
    Effect effect{.name = std::move(hierarchy->name),
                  .presimulateTime = F(*root, kPresimulateTime),
                  .remoteMaterial = std::nullopt,
                  .items = {},
                  .totalExports = U32(*root, model::kTotalExports)};
    if (root->Shape().Find(kRemoteDecal) != nullptr)
    {
        effect.remoteMaterial = RemoteMaterial{.decal = Text(*root, kRemoteDecal),
                                               .fresnelFalloff = F(*root, kRemoteFresnelFalloff),
                                               .invertFresnel = Flag(*root, kRemoteInvertFresnel),
                                               .alpha = F(*root, kRemoteAlpha),
                                               .tint = Floats<4>(*root, kRemoteTint)};
    }
    const std::vector<gff4::Struct> structs = model::detail::NodeStructs(model);
    for (std::size_t i = 0; i < hierarchy->nodes.size() && i < structs.size(); ++i)
    {
        effect.items.push_back(ItemOf(std::move(hierarchy->nodes[i]), structs[i]));
    }
    return effect;
}

} // namespace dao::core::effect
