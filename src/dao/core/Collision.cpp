// dao/core/Collision.h: a level's static physics from its .opf, and a
// placeable model's shapes from its .phy. The engine's side of both is
// lib-agent-2's reading of 2026-10-18 in the GOG exe, dev/COLLISION.md; the
// files' side was read 2026-10-18 over every one the game takes:
//
//   All 132 .opf (packages/core/env/; 81 of the 213 levels, the add-ins'
//   among them, have none, only the .arl's binary copy): every one framed as
//   the header says, its counts of records its counts of actors and shapes,
//   every userProperties its actor's record and then one a shape. 65,896
//   actors, none with a body; 98,996 shapes: 62,784 boxes, 24,751 capsules,
//   9,176 triangle meshes, 2,285 spheres, nothing else. No shape is a
//   trigger, none has raycasting or scene queries off, every materialIndex is
//   0. Every actor record is (-1, 0, room, n); a shape record's third word is
//   often 0xBAADF00D, the debug heap's unwritten memory. lak302d's actor
//   records are the .arl's own (lib-agent-2).
//   All 17,763 .phy: 30,875 shapes, MMHShapeType 1 with sphs (214), 2 with
//   boxs (3,619), 3 with caps (20,037), 5 with mshs (7,005); each cobj's
//   parent a "node" (24,202 shapes) or a "mshh" (6,673), its type 1 or 2 and
//   its MMHCollisionGroup 0. The masks built from them: 0x81000002 (19,228),
//   0x82002020, 0x81002020, 0x80010000, 0x81000004 (656), and twelve rarer.
//
// The .opf's poses were checked against a room's model: lak302d's Actor_1,
// tvi_smdrfrme_0.GOB at (0, -72, 2.5), is room 1101's model at its place,
// turned by (0, 0, -0.7071, 0.7071), which is the matrix whose rows are the
// pose's first three numbers and the next three. So the numbers are rows.
#include "dao/core/Collision.h"

#include "dao/core/Bytes.h"
#include "dao/core/XmlTags.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <functional>
#include <map>
#include <string_view>
#include <system_error>
#include <utility>

namespace dao::core::collision
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kMmhName = 6000;
constexpr std::uint32_t kObjType = 6057; // MMHNodeCollisionObjType
constexpr std::uint32_t kShapeType = 6058;
constexpr std::uint32_t kShapeRotation = 6060;
constexpr std::uint32_t kShapePosition = 6061;
constexpr std::uint32_t kMaskAll = 6063;
constexpr std::uint32_t kMaskItems = 6065;
constexpr std::uint32_t kMaskCreatures = 6066;
constexpr std::uint32_t kMaskPlaceables = 6067;
constexpr std::uint32_t kMaskTriggers = 6068;
constexpr std::uint32_t kMaskStaticGeometry = 6069;
constexpr std::uint32_t kMaskNonwalkable = 6070;
constexpr std::uint32_t kBoxDim = 6071;
constexpr std::uint32_t kRadius = 6072;
constexpr std::uint32_t kHeight = 6073;
constexpr std::uint32_t kShapeVolume = 6240;
constexpr std::uint32_t kShapeName = 6241;
constexpr std::uint32_t kAllowEmitterSpawn = 6244;
constexpr std::uint32_t kFadeable = 6252;
constexpr std::uint32_t kMaskWater = 6277;
constexpr std::uint32_t kMaskTerrainWall = 6295;
constexpr std::uint32_t kMaskWalkable = 6305;
constexpr std::uint32_t kTypeStruct = 6998; // MMHShapeTypeStruct
constexpr std::uint32_t kChildren = 6999;

constexpr std::size_t kActorRecordSize = 16;
constexpr std::size_t kShapeRecordSize = 12;

// --- the .opf's XML

std::string_view Trimmed(std::string_view text)
{
    while (!text.empty() && xml::Space(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && xml::Space(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

// Calls take(word) with each of text's words between spaces.
template <class Take> void ForEachWord(std::string_view text, Take &&take)
{
    std::size_t at = 0;
    while (at < text.size())
    {
        while (at < text.size() && xml::Space(text[at]))
        {
            ++at;
        }
        std::size_t end = at;
        while (end < text.size() && !xml::Space(text[end]))
        {
            ++end;
        }
        if (end > at)
        {
            take(text.substr(at, end - at));
        }
        at = end;
    }
}

template <class T> std::optional<T> NumberOf(std::string_view word)
{
    T value{};
    const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), value);
    if (error != std::errc{} || end != word.data() + word.size())
    {
        return std::nullopt;
    }
    return value;
}

template <class T> std::vector<T> NumbersOf(std::string_view text)
{
    std::vector<T> out;
    ForEachWord(text, [&out](std::string_view word) {
        if (const std::optional<T> value = NumberOf<T>(word))
        {
            out.push_back(*value);
        }
    });
    return out;
}

template <class T> T AttributeNumber(std::string_view tag, std::string_view name, T otherwise)
{
    const std::optional<std::string> text = xml::Attribute(tag, name);
    return text ? NumberOf<T>(*text).value_or(otherwise) : otherwise;
}

// An actor's userProperties, "a,b,c": its record's index, then each of its shapes'.
std::vector<std::int32_t> RecordIndices(std::string text)
{
    std::ranges::replace(text, ',', ' ');
    return NumbersOf<std::int32_t>(text);
}

// A pose's twelve numbers: the turn's rows, then the translation. Fewer leave it as it was.
void ReadPose(std::string_view text, Pose &pose)
{
    const std::vector<float> numbers = NumbersOf<float>(text);
    if (numbers.size() < 12)
    {
        return;
    }
    for (std::size_t i = 0; i < 9; ++i)
    {
        pose.rotation[i] = numbers[i];
    }
    pose.translation = {numbers[9], numbers[10], numbers[11]};
}

Kind KindOf(std::string_view element)
{
    if (element == "NxBoxShapeDesc")
    {
        return Kind::Box;
    }
    if (element == "NxCapsuleShapeDesc")
    {
        return Kind::Capsule;
    }
    if (element == "NxSphereShapeDesc")
    {
        return Kind::Sphere;
    }
    if (element == "NxTriangleMeshShapeDesc")
    {
        return Kind::Mesh;
    }
    if (element == "NxConvexShapeDesc")
    {
        return Kind::Convex;
    }
    return Kind::Unknown;
}

// A shape's element, NxBoxShapeDesc and kin, as against NxShapeDesc within it.
bool IsShapeElement(std::string_view element)
{
    return element != "NxShapeDesc" && element.starts_with("Nx") && element.ends_with("ShapeDesc");
}

// The stream's elements in turn, each with the element it is in: the actors, their shapes, the meshes.
class Reader
{
  public:
    Reader(std::string_view text, StaticPhysics &out) : text_(text), out_(out)
    {
    }

    void Run()
    {
        xml::ForEachTag(text_, [this](std::string_view tag) { Take(tag); });
    }

    // Each actor's userProperties: its record's index, then its shapes'.
    const std::vector<std::vector<std::int32_t>> &Records() const
    {
        return records_;
    }

  private:
    // The text after a tag up to the next one.
    std::string_view TextAfter(std::string_view tag) const
    {
        const std::size_t from = static_cast<std::size_t>(tag.data() - text_.data()) + tag.size() + 1;
        if (from >= text_.size())
        {
            return {};
        }
        const std::size_t to = text_.find('<', from);
        return Trimmed(text_.substr(from, to == std::string_view::npos ? std::string_view::npos : to - from));
    }

    std::string_view Parent() const
    {
        return open_.empty() ? std::string_view{} : open_.back();
    }

    void Take(std::string_view tag)
    {
        if (tag.starts_with('/'))
        {
            if (!open_.empty())
            {
                if (open_.back() == "NxActorDesc")
                {
                    actor_ = nullptr;
                }
                else if (IsShapeElement(open_.back()))
                {
                    shape_ = nullptr;
                }
                open_.pop_back();
            }
            return;
        }
        const std::string_view element = xml::ElementOf(tag);
        Open(element, tag);
        if (!tag.ends_with('/'))
        {
            open_.push_back(element);
        }
    }

    void Open(std::string_view element, std::string_view tag)
    {
        const std::string_view parent = Parent();
        if (element == "NxTriangleMeshDesc")
        {
            std::string id = xml::Attribute(tag, "id").value_or("");
            meshIds_[id] = static_cast<std::int32_t>(out_.meshes.size());
            out_.meshes.emplace_back().id = std::move(id);
        }
        else if (parent == "NxSimpleTriangleMesh" && !out_.meshes.empty())
        {
            Mesh(element, tag);
        }
        else if (element == "NxActorDesc")
        {
            OpenActor(tag);
        }
        else if (actor_ != nullptr && parent == "NxActorDesc")
        {
            ActorChild(element, tag);
        }
        else if (shape_ != nullptr && parent == "NxShapeDesc")
        {
            ShapeChild(element, tag);
        }
        else if (shape_ != nullptr && element == "NxShapeDesc")
        {
            shape_->name = xml::Attribute(tag, "name").value_or("");
        }
    }

    void Mesh(std::string_view element, std::string_view tag)
    {
        TriangleMesh &mesh = out_.meshes.back();
        if (element == "points")
        {
            const std::vector<float> numbers = NumbersOf<float>(TextAfter(tag));
            for (std::size_t i = 0; i + 2 < numbers.size(); i += 3)
            {
                mesh.points.push_back({numbers[i], numbers[i + 1], numbers[i + 2]});
            }
        }
        else if (element == "triangles")
        {
            const std::vector<std::uint32_t> numbers = NumbersOf<std::uint32_t>(TextAfter(tag));
            for (std::size_t i = 0; i + 2 < numbers.size(); i += 3)
            {
                mesh.triangles.push_back({numbers[i], numbers[i + 1], numbers[i + 2]});
            }
        }
    }

    void OpenActor(std::string_view tag)
    {
        Actor &actor = out_.actors.emplace_back();
        actor.id = xml::Attribute(tag, "id").value_or("");
        actor.name = xml::Attribute(tag, "name").value_or("");
        actor.hasBody = xml::Attribute(tag, "hasBody").value_or("") == "true";
        records_.push_back(RecordIndices(xml::Attribute(tag, "userProperties").value_or("")));
        actor_ = &actor;
        shape_ = nullptr;
    }

    void ActorChild(std::string_view element, std::string_view tag)
    {
        if (element == "globalPose")
        {
            ReadPose(TextAfter(tag), actor_->globalPose);
        }
        else if (element == "group")
        {
            actor_->group = NumberOf<std::uint32_t>(TextAfter(tag)).value_or(0);
        }
        else if (IsShapeElement(element))
        {
            StaticShape &shape = actor_->shapes.emplace_back();
            shape.kind = KindOf(element);
            if (const std::optional<std::string> dimensions = xml::Attribute(tag, "dimensions"))
            {
                const std::vector<float> numbers = NumbersOf<float>(*dimensions);
                if (numbers.size() == 3)
                {
                    shape.dimensions = {numbers[0], numbers[1], numbers[2]};
                }
            }
            shape.radius = AttributeNumber<float>(tag, "radius", 0.0F);
            shape.height = AttributeNumber<float>(tag, "height", 0.0F);
            if (const std::optional<std::string> mesh = xml::Attribute(tag, "meshData"))
            {
                const auto found = meshIds_.find(*mesh);
                shape.mesh = found == meshIds_.end() ? -1 : found->second;
            }
            shape_ = &shape;
        }
    }

    void ShapeChild(std::string_view element, std::string_view tag)
    {
        if (element == "localPose")
        {
            ReadPose(TextAfter(tag), shape_->localPose);
        }
        else if (element == "group")
        {
            shape_->group = NumberOf<std::uint32_t>(TextAfter(tag)).value_or(0);
        }
        else if (element == "NxGroupsMask")
        {
            shape_->groupsMask = {
                AttributeNumber<std::uint32_t>(tag, "bits0", 0), AttributeNumber<std::uint32_t>(tag, "bits1", 0),
                AttributeNumber<std::uint32_t>(tag, "bits2", 0), AttributeNumber<std::uint32_t>(tag, "bits3", 0)};
        }
    }

    std::string_view text_;
    StaticPhysics &out_;
    std::vector<std::string_view> open_;
    std::map<std::string, std::int32_t, std::less<>> meshIds_;
    std::vector<std::vector<std::int32_t>> records_;
    // Pointers into out_'s vectors: a new actor drops both, a new shape the last shape's.
    Actor *actor_ = nullptr;
    StaticShape *shape_ = nullptr;
};

// Gives each actor and shape its record's index from its userProperties, and each actor its record's room, as
// NXU_notifyActor (0x0082A6F0) points their userData at the records.
void Link(StaticPhysics &physics, const std::vector<std::vector<std::int32_t>> &records)
{
    for (std::size_t a = 0; a < physics.actors.size() && a < records.size(); ++a)
    {
        Actor &actor = physics.actors[a];
        const std::vector<std::int32_t> &indices = records[a];
        if (indices.empty())
        {
            continue;
        }
        actor.record = indices[0];
        if (actor.record >= 0 && static_cast<std::size_t>(actor.record) < physics.actorRecords.size())
        {
            actor.room = physics.actorRecords[static_cast<std::size_t>(actor.record)].room;
        }
        for (std::size_t s = 0; s < actor.shapes.size() && s + 1 < indices.size(); ++s)
        {
            actor.shapes[s].record = indices[s + 1];
        }
    }
}

// --- the .arl's binary stream
//
// NXU's binary form (DAOLib #197), as the files hold it, read 2026-10-19 against the XML of all 132 levels that have
// both (dev/COLLISION.md). The stream is "NXUSTREAM", two zero bytes, the SDK's version and NXU's (u32 each), then
// blocks. A block is a 16-byte head, 0x00ABCDEF, its type, its length from the head's start to its end, and
// 0x01234567, then its own fields, then the blocks it holds. A string is a u32 length, and for a length over 0 that
// many characters and a NUL. The types are the NXU classes' numbers, which move with the SDK's version.

constexpr std::uint32_t kBlockMagic = 0x00ABCDEF;
constexpr std::uint32_t kBlockEnd = 0x01234567;
constexpr std::size_t kBlockHead = 16;
constexpr std::size_t kStreamHead = 19;
constexpr std::size_t kMaxDepth = 8;
constexpr std::uint32_t kMeshFlag16Bit = 2; // NX_MF_16_BIT_INDICES

// The block types of the classes this reads.
struct BlockTypes
{
    std::uint32_t sdk = 0;
    std::uint32_t shapeDesc = 0;
    std::uint32_t box = 0;
    std::uint32_t sphere = 0;
    std::uint32_t capsule = 0;
    std::uint32_t mesh = 0;
    std::uint32_t actor = 0;
    // The same in every version seen.
    std::uint32_t simpleMesh = 7;
    std::uint32_t meshDesc = 9;
    std::uint32_t groupsMask = 17;
};

// SDK 272 (0x110) is the 132 levels with an .opf; 281 (0x119) is 18 of the 26 core levels without one.
constexpr BlockTypes kTypes[] = {
    {.sdk = 272, .shapeDesc = 31, .box = 32, .sphere = 34, .capsule = 35, .mesh = 38, .actor = 41},
    {.sdk = 281, .shapeDesc = 32, .box = 33, .sphere = 35, .capsule = 36, .mesh = 39, .actor = 42},
};

struct Block
{
    std::uint32_t type = 0;
    std::size_t body = 0; // after the head
    std::size_t end = 0;
};

// Reads fields in turn, and fails once at the first that runs past its end.
class Cursor
{
  public:
    Cursor(std::span<const std::byte> bytes, std::size_t at, std::size_t end) : bytes_(bytes.first(end)), at_(at)
    {
    }

    template <class T> T Take()
    {
        const std::optional<T> value = ReadAt<T>(bytes_, at_);
        if (!value)
        {
            failed_ = true;
            return T{};
        }
        at_ += sizeof(T);
        return *value;
    }

    void Skip(std::size_t count)
    {
        if (count > bytes_.size() - std::min(at_, bytes_.size()))
        {
            failed_ = true;
            return;
        }
        at_ += count;
    }

    std::string String()
    {
        const auto length = Take<std::uint32_t>();
        if (failed_ || length == 0)
        {
            return {};
        }
        const std::size_t from = at_;
        Skip(std::size_t{length} + 1);
        return failed_ ? std::string{} : std::string(reinterpret_cast<const char *>(bytes_.data()) + from, length);
    }

    // A pose as the stream has it: the turn's three rows, each followed by its translation's number.
    Pose PoseOf()
    {
        Pose pose;
        for (std::size_t row = 0; row < 3; ++row)
        {
            for (std::size_t column = 0; column < 3; ++column)
            {
                pose.rotation[(row * 3) + column] = Take<float>();
            }
            pose.translation[row] = Take<float>();
        }
        return pose;
    }

    bool Failed() const
    {
        return failed_;
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t at_;
    bool failed_ = false;
};

class BinaryReader
{
  public:
    BinaryReader(std::span<const std::byte> stream, StaticPhysics &out) : stream_(stream), out_(out)
    {
    }

    // Returns false for a stream it cannot read whole.
    bool Run()
    {
        if (stream_.size() < kStreamHead || std::memcmp(stream_.data(), "NXUSTREAM", 9) != 0)
        {
            return false;
        }
        const std::uint32_t sdk = ReadAt<std::uint32_t>(stream_, 11).value_or(0);
        const auto *const found = std::ranges::find(kTypes, sdk, &BlockTypes::sdk);
        if (found == std::end(kTypes))
        {
            return false;
        }
        types_ = *found;
        const std::vector<Block> top = Children(kStreamHead, stream_.size());
        for (const Block &block : top)
        {
            Visit(block, 0);
        }
        return !top.empty() && !failed_;
    }

    // Each actor's userProperties: its record's index, then its shapes'.
    const std::vector<std::vector<std::int32_t>> &Records() const
    {
        return records_;
    }

  private:
    // The blocks in a body, found by their heads: the fields before the first, and between blocks, are skipped. A
    // head whose length does not fit the body is a stream cut short or damaged, which fails the read.
    std::vector<Block> Children(std::size_t from, std::size_t end)
    {
        std::vector<Block> blocks;
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(stream_.data());
        for (std::size_t at = from; at + kBlockHead <= end;)
        {
            const std::uint8_t *hit = std::find(bytes + at, bytes + end - kBlockHead + 1, std::uint8_t{0xEF});
            at = static_cast<std::size_t>(hit - bytes);
            if (at + kBlockHead > end || !IsHead(at))
            {
                ++at;
                continue;
            }
            const std::uint32_t length = ReadAt<std::uint32_t>(stream_, at + 8).value_or(0);
            if (length < kBlockHead || length > end - at)
            {
                failed_ = true;
                return blocks;
            }
            blocks.push_back(Block{.type = ReadAt<std::uint32_t>(stream_, at + 4).value_or(0),
                                   .body = at + kBlockHead,
                                   .end = at + length});
            at += length;
        }
        return blocks;
    }

    // Returns whether the bytes at `at` begin and end a block's head as every head does.
    bool IsHead(std::size_t at) const
    {
        return ReadAt<std::uint32_t>(stream_, at) == kBlockMagic &&
               ReadAt<std::uint32_t>(stream_, at + 12) == kBlockEnd;
    }

    Cursor Body(const Block &block) const
    {
        return {stream_, block.body, block.end};
    }

    // Finds the actors and meshes under the blocks that hold them: the collection and its scene.
    void Visit(const Block &block, std::size_t depth)
    {
        if (block.type == types_.meshDesc)
        {
            ReadMesh(block);
        }
        else if (block.type == types_.actor)
        {
            ReadActor(block);
        }
        else if (depth < kMaxDepth)
        {
            for (const Block &child : Children(block.body, block.end))
            {
                Visit(child, depth + 1);
            }
        }
    }

    void ReadMesh(const Block &block)
    {
        Cursor head = Body(block);
        TriangleMesh &mesh = out_.meshes.emplace_back();
        mesh.id = head.String();
        meshIds_[mesh.id] = static_cast<std::int32_t>(out_.meshes.size() - 1);
        failed_ |= head.Failed();
        for (const Block &child : Children(block.body, block.end))
        {
            if (child.type == types_.simpleMesh)
            {
                failed_ |= !ReadPointsAndTriangles(child, mesh);
            }
        }
    }

    // An NxSimpleTriangleMesh: the points, the triangles' indices, and the flags that say how wide they are.
    bool ReadPointsAndTriangles(const Block &block, TriangleMesh &mesh) const
    {
        Cursor c = Body(block);
        const std::size_t size = block.end - block.body;
        const auto pointCount = c.Take<std::uint32_t>();
        if (c.Failed() || size < 12 || pointCount > (size - 12) / sizeof(Point))
        {
            return false;
        }
        for (std::uint32_t i = 0; i < pointCount; ++i)
        {
            Point point;
            point[0] = c.Take<float>();
            point[1] = c.Take<float>();
            point[2] = c.Take<float>();
            mesh.points.push_back(point);
        }
        const auto triangleCount = c.Take<std::uint32_t>();
        const std::uint32_t flags = ReadAt<std::uint32_t>(stream_, block.end - 4).value_or(0);
        const std::size_t width = (flags & kMeshFlag16Bit) != 0 ? 2 : 4;
        const std::size_t expected =
            12 + (std::size_t{pointCount} * sizeof(Point)) + (std::size_t{triangleCount} * 3 * width);
        if (c.Failed() || size != expected)
        {
            return false;
        }
        for (std::uint32_t i = 0; i < triangleCount; ++i)
        {
            std::array<std::uint32_t, 3> triangle{};
            for (std::uint32_t &index : triangle)
            {
                index = width == 2 ? c.Take<std::uint16_t>() : c.Take<std::uint32_t>();
            }
            mesh.triangles.push_back(triangle);
        }
        return !c.Failed();
    }

    // An NxActorDesc: its fields, then its shapes in order.
    void ReadActor(const Block &block)
    {
        Cursor c = Body(block);
        Actor &actor = out_.actors.emplace_back();
        actor.id = c.String();
        records_.push_back(RecordIndices(c.String()));
        actor.hasBody = c.Take<std::uint32_t>() != 0;
        actor.name = c.String();
        actor.globalPose = c.PoseOf();
        c.Skip(16 + 4 + 4); // a quaternion (0, 0, 0, 1) in every actor, the density, the flags
        actor.group = c.Take<std::uint32_t>();
        failed_ |= c.Failed();
        for (const Block &child : Children(block.body, block.end))
        {
            if (const std::optional<Kind> kind = KindOfBlock(child.type))
            {
                actor.shapes.push_back(ReadShape(child, *kind));
            }
            else
            {
                failed_ = true;
            }
        }
    }

    std::optional<Kind> KindOfBlock(std::uint32_t type) const
    {
        if (type == types_.box)
        {
            return Kind::Box;
        }
        if (type == types_.sphere)
        {
            return Kind::Sphere;
        }
        if (type == types_.capsule)
        {
            return Kind::Capsule;
        }
        if (type == types_.mesh)
        {
            return Kind::Mesh;
        }
        return std::nullopt;
    }

    // A shape block: its own size or mesh, then the NxShapeDesc block it holds.
    StaticShape ReadShape(const Block &block, Kind kind)
    {
        StaticShape shape;
        shape.kind = kind;
        Cursor c = Body(block);
        switch (kind)
        {
        case Kind::Box:
            shape.dimensions = {c.Take<float>(), c.Take<float>(), c.Take<float>()};
            break;
        case Kind::Sphere:
            shape.radius = c.Take<float>();
            break;
        case Kind::Capsule:
            shape.radius = c.Take<float>();
            shape.height = c.Take<float>();
            break;
        default: {
            const auto found = meshIds_.find(c.String());
            shape.mesh = found == meshIds_.end() ? -1 : found->second;
            break;
        }
        }
        failed_ |= c.Failed();
        for (const Block &child : Children(block.body, block.end))
        {
            if (child.type == types_.shapeDesc)
            {
                ReadShapeDesc(child, shape);
            }
        }
        return shape;
    }

    void ReadShapeDesc(const Block &block, StaticShape &shape)
    {
        Cursor c = Body(block);
        c.String(); // userProperties
        shape.name = c.String();
        shape.localPose = c.PoseOf();
        c.Skip(16 + 4); // a quaternion (0, 0, 0, 1) in every shape, the shape flags
        shape.group = c.Take<std::uint32_t>();
        failed_ |= c.Failed();
        for (const Block &child : Children(block.body, block.end))
        {
            if (child.type == types_.groupsMask)
            {
                Cursor mask = Body(child);
                for (std::uint32_t &bits : shape.groupsMask)
                {
                    bits = mask.Take<std::uint32_t>();
                }
                failed_ |= mask.Failed();
            }
        }
    }

    std::span<const std::byte> stream_;
    StaticPhysics &out_;
    BlockTypes types_;
    bool failed_ = false;
    std::map<std::string, std::int32_t, std::less<>> meshIds_;
    std::vector<std::vector<std::int32_t>> records_;
};

// Reads the actors and meshes of a binary stream into out, which keeps its records and nothing of the stream's if
// the stream is not one this reads.
void ReadBinary(std::span<const std::byte> stream, StaticPhysics &out)
{
    StaticPhysics read;
    BinaryReader reader(stream, read);
    if (!reader.Run())
    {
        return;
    }
    out.meshes = std::move(read.meshes);
    out.actors = std::move(read.actors);
    out.binary = true;
    Link(out, reader.Records());
}

// The records after the stream; false if they do not end the bytes exactly.
bool ReadRecords(std::span<const std::byte> bytes, std::size_t at, StaticPhysics &out)
{
    const std::optional<std::uint32_t> actors = ReadAt<std::uint32_t>(bytes, at);
    if (!actors || *actors > (bytes.size() - at - 4) / kActorRecordSize)
    {
        return false;
    }
    at += 4;
    for (std::uint32_t i = 0; i < *actors; ++i, at += kActorRecordSize)
    {
        out.actorRecords.push_back({.first = ReadAt<std::int32_t>(bytes, at).value_or(-1),
                                    .second = ReadAt<std::int32_t>(bytes, at + 4).value_or(0),
                                    .room = ReadAt<std::int32_t>(bytes, at + 8).value_or(-1),
                                    .sound = ReadAt<std::int32_t>(bytes, at + 12).value_or(0)});
    }
    const std::optional<std::uint32_t> shapes = ReadAt<std::uint32_t>(bytes, at);
    if (!shapes || *shapes > (bytes.size() - at - 4) / kShapeRecordSize)
    {
        return false;
    }
    at += 4;
    for (std::uint32_t i = 0; i < *shapes; ++i, at += kShapeRecordSize)
    {
        out.shapeRecords.push_back(
            {.words = {ReadAt<std::uint32_t>(bytes, at).value_or(0), ReadAt<std::uint32_t>(bytes, at + 4).value_or(0),
                       ReadAt<std::uint32_t>(bytes, at + 8).value_or(0)}});
    }
    return at == bytes.size();
}

// --- the .phy

Point PointOf(const std::vector<float> &floats)
{
    return floats.size() >= 3 ? Point{floats[0], floats[1], floats[2]} : Point{};
}

Shape ShapeOf(const gff4::Struct &shap, const std::wstring &node, std::uint32_t objectType)
{
    Shape shape;
    shape.name = shap.String(kShapeName).value_or(L"");
    shape.node = node;
    shape.kind = static_cast<Kind>(shap.Get<std::uint32_t>(kShapeType).value_or(0));
    const std::vector<float> rotation = shap.Floats(kShapeRotation);
    if (rotation.size() == 4)
    {
        shape.rotation = {rotation[0], rotation[1], rotation[2], rotation[3]};
    }
    shape.position = PointOf(shap.Floats(kShapePosition));
    shape.mask = MaskOf(shap);
    shape.fadeable = shap.Get<std::uint8_t>(kFadeable).value_or(0) != 0;
    shape.objectType = objectType;
    // For an emitter attached to the owner (DAOLib #213; dev/EFFECTS.md, the Spirit of Valor's
    // ghost): it spawns on the shapes that allow it, weighted by their volume.
    shape.allowEmitterSpawn = shap.Get<std::uint8_t>(kAllowEmitterSpawn).value_or(0) != 0;
    shape.volume = shap.Get<float>(kShapeVolume).value_or(0.0F);
    if (const std::optional<gff4::Struct> size = shap.Child(kTypeStruct))
    {
        shape.dimensions = PointOf(size->Floats(kBoxDim));
        shape.radius = size->Get<float>(kRadius).value_or(0.0F);
        shape.height = size->Get<float>(kHeight).value_or(0.0F);
    }
    return shape;
}

void Walk(const gff4::Struct &node, const std::wstring &name, std::uint32_t objectType, std::vector<Shape> &out)
{
    for (const gff4::Struct &child : node.Structs(kChildren))
    {
        const std::string &label = child.Label();
        if (label == "shap")
        {
            out.push_back(ShapeOf(child, name, objectType));
            continue;
        }
        const std::optional<std::wstring> own = child.String(kMmhName);
        const std::uint32_t type = label == "cobj" ? child.Get<std::uint32_t>(kObjType).value_or(0) : objectType;
        Walk(child, own ? *own : name, type, out);
    }
}

} // namespace

Pose Compose(const Pose &outer, const Pose &local)
{
    Pose out;
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            float sum = 0;
            for (std::size_t k = 0; k < 3; ++k)
            {
                sum += outer.rotation[(row * 3) + k] * local.rotation[(k * 3) + column];
            }
            out.rotation[(row * 3) + column] = sum;
        }
    }
    out.translation = Apply(outer, local.translation);
    return out;
}

Point Apply(const Pose &pose, const Point &point)
{
    Point out{};
    for (std::size_t row = 0; row < 3; ++row)
    {
        out[row] = (pose.rotation[row * 3] * point[0]) + (pose.rotation[(row * 3) + 1] * point[1]) +
                   (pose.rotation[(row * 3) + 2] * point[2]) + pose.translation[row];
    }
    return out;
}

std::optional<StaticPhysics> StaticPhysicsOf(std::span<const std::byte> bytes)
{
    const std::optional<std::uint32_t> total = ReadAt<std::uint32_t>(bytes, 0);
    const std::optional<std::uint32_t> length = ReadAt<std::uint32_t>(bytes, 4);
    if (!total || !length || *total != bytes.size() || *length > bytes.size() - 8)
    {
        return std::nullopt;
    }
    StaticPhysics out;
    if (!ReadRecords(bytes, 8 + std::size_t{*length}, out))
    {
        return std::nullopt;
    }
    const std::string_view stream(reinterpret_cast<const char *>(bytes.data()) + 8, *length);
    out.xml = Trimmed(stream).starts_with("<NXUSTREAM2>");
    if (out.xml)
    {
        Reader reader(stream, out);
        reader.Run();
        Link(out, reader.Records());
    }
    else
    {
        ReadBinary(bytes.subspan(8, *length), out);
    }
    return out;
}

std::uint32_t MaskOf(const gff4::Struct &shap)
{
    // 0x00589DB0, its flags read from 0x00589F99 to 0x0058A20F in this order: All makes every bit set, None is
    // read and left, Effects and Waypoints are not read.
    const auto flag = [&shap](std::uint32_t label) { return shap.Get<std::uint8_t>(label).value_or(0) != 0; };
    std::uint32_t mask = flag(kMaskAll) ? kAll : 0;
    static constexpr std::pair<std::uint32_t, std::uint32_t> kFlags[] = {
        {kMaskItems, kItems},
        {kMaskCreatures, kCreatures},
        {kMaskPlaceables, kPlaceables},
        {kMaskTriggers, kTriggers},
        {kMaskStaticGeometry, kStaticGeometry},
        {kMaskNonwalkable, kNonwalkable},
        {kMaskWalkable, kWalkable},
        {kMaskWater, kWater},
        {kMaskTerrainWall, kTerrainWall},
    };
    for (const auto &[label, bit] : kFlags)
    {
        if (flag(label))
        {
            mask |= bit;
        }
    }
    if (mask != 0)
    {
        mask |= kAny;
    }
    if (!flag(kFadeable) && (mask & kStaticGeometry) != 0)
    {
        mask |= kNotFadeable;
    }
    return mask;
}

std::vector<Shape> ShapesOf(const gff4::File &phy)
{
    std::vector<Shape> out;
    const std::optional<gff4::Struct> root = phy.Root();
    if (phy.Type() != "PHY" || !root)
    {
        return out;
    }
    Walk(*root, root->String(kMmhName).value_or(L""), 0, out);
    return out;
}

std::uint32_t MaskOf(std::span<const Shape> shapes)
{
    // PhysicsActor 0x0081E6C0, by PhysicsObject slot 20 at model-ready (0x0074F7A0).
    std::uint32_t mask = 0;
    for (const Shape &shape : shapes)
    {
        mask |= shape.mask;
    }
    return mask;
}

} // namespace dao::core::collision
