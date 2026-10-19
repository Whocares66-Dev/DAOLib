#include "dao/core/Stage.h"

#include <cstddef>
#include <string_view>

namespace dao::core::stage
{

namespace
{

constexpr std::uint32_t kTag = 1;
constexpr std::uint32_t kPosition = 4;
constexpr std::uint32_t kOrientation = 5;
constexpr std::uint32_t kPlaces = 11000;
constexpr std::uint32_t kCameras = 11001;
constexpr std::uint32_t kPlacesInShot = 11002;
constexpr std::uint32_t kCameraFov = 11003;
constexpr std::uint32_t kPlaceDefaultCamera = 11004;
constexpr std::uint32_t kLookingFrom = 11006;
constexpr std::uint32_t kLookingAtPrimary = 11007;
constexpr std::uint32_t kLookingAtSecondary = 11008;
constexpr std::uint32_t kLookingAtType = 11009;

template <std::size_t N> std::array<float, N> Several(const gff4::Struct &in, std::uint32_t label)
{
    const std::vector<float> floats = in.Floats(label);
    std::array<float, N> out{};
    for (std::size_t i = 0; i < N && i < floats.size(); ++i)
    {
        out[i] = floats[i];
    }
    return out;
}

std::wstring Text(const gff4::Struct &in, std::uint32_t label)
{
    return in.String(label).value_or(L"");
}

Place PlaceOf(const gff4::Struct &in)
{
    Place place{.tag = Text(in, kTag),
                .position = Several<3>(in, kPosition),
                .orientation = Several<4>(in, kOrientation),
                .defaultCamera = Text(in, kPlaceDefaultCamera)};
    return place;
}

Camera CameraOf(const gff4::Struct &in)
{
    return {.tag = Text(in, kTag),
            .position = Several<3>(in, kPosition),
            .orientation = Several<4>(in, kOrientation),
            .placesInShot = in.Strings(kPlacesInShot),
            .fov = in.Get<float>(kCameraFov).value_or(0.0F),
            .lookingFrom = Text(in, kLookingFrom),
            .lookingAtPrimary = Text(in, kLookingAtPrimary),
            .lookingAtSecondary = Text(in, kLookingAtSecondary),
            .lookingAtType = in.Get<std::uint8_t>(kLookingAtType).value_or(0)};
}

} // namespace

Stage StageOf(const gff4::Struct &root)
{
    Stage stage;
    for (const gff4::Struct &place : root.Structs(kPlaces))
    {
        stage.places.push_back(PlaceOf(place));
    }
    for (const gff4::Struct &camera : root.Structs(kCameras))
    {
        stage.cameras.push_back(CameraOf(camera));
    }
    return stage;
}

std::optional<Stage> StageOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "STG" || !root)
    {
        return std::nullopt;
    }
    return StageOf(*root);
}

const Place *FindPlace(const Stage &stage, std::wstring_view tag)
{
    for (const Place &place : stage.places)
    {
        if (place.tag == tag)
        {
            return &place;
        }
    }
    return nullptr;
}

const Camera *FindCamera(const Stage &stage, std::wstring_view tag)
{
    for (const Camera &camera : stage.cameras)
    {
        if (camera.tag == tag)
        {
            return &camera;
        }
    }
    return nullptr;
}

} // namespace dao::core::stage
