// The game's GFF4 layout, written (DAOLib #326). Read 2026-11-06 in the GOG exe by a lib-agent
// reader, from the writer the .chr and the shipped .mop share (0x00A2xxxx), and checked by
// rebuilding every data offset of chargen.rim's hm_cps_p01.mop with these rules, then against three
// .chr files the game wrote in character creation:
// - A template's fields are laid out (0x00A2BB00) in a stable sort by alignment, largest first, and
//   placed in that order, each at its alignment; the struct's size is rounded up to its first
//   field's alignment, which is also the struct's own. The file lists them by label (0x00A2B720).
// - The data block grows as values are written (Alloc 0x00A2B800), new bytes 0xFF: a struct at its
//   alignment; a list as a count followed by its elements, the count placed at a multiple of 4 so
//   that the elements land at their alignment; a string (0x0056B220) as a list of UTF-16 units
//   with its NUL, written once and its offset given again to the same text.
// - The data starts after the header, the templates and their fields, rounded up to the largest
//   alignment any value took (at least 4 once a list is written), padded with 0xFF; nothing
//   follows the last value.
// - The templates are in the order added, with the root's, added last by the game, at index 0
//   (0x0056AE80); a template the same as one before is not added again (0x004DBA20).
#include "dao/core/Gff4Writer.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace dao::core::gff4
{

namespace
{

constexpr std::uint32_t kNone = 0xFFFFFFFF;
constexpr std::size_t kHeaderSize = 28;
constexpr std::size_t kTemplateSize = 16;
constexpr std::size_t kFieldSize = 12;
constexpr std::size_t kReferenceSize = 4; // a list, a reference or a string: an offset

struct Measure
{
    std::uint32_t size = 0;
    std::uint32_t align = 1;
};

// A type's size and alignment as the game's writer gives them (0x004D9370, 0x004D92F4).
Measure MeasureOf(Type type)
{
    switch (type)
    {
    case Type::U8:
    case Type::S8:
        return {1, 1};
    case Type::U16:
    case Type::S16:
        return {2, 2};
    case Type::U32:
    case Type::S32:
    case Type::F32:
    case Type::String:
        return {4, 4};
    case Type::U64:
    case Type::S64:
    case Type::F64:
        return {8, 8};
    case Type::TlkString:
        return {8, 4};
    case Type::Matrix:
        return {64, 16};
    case Type::Vector3:
    case Type::Vector4:
    case Type::Quaternion:
    case Type::Color:
        return {16, 16};
    default: // type 11, which the game's files never hold, among the rest
        return {4, 4};
    }
}

std::size_t RoundUp(std::size_t value, std::size_t align)
{
    return (value + align - 1) / align * align;
}

std::string FourLetters(std::string_view text)
{
    std::string out(text.substr(0, 4));
    out.resize(4, ' ');
    return out;
}

void PutText(std::vector<std::byte> &out, std::size_t at, std::string_view text)
{
    std::memcpy(out.data() + at, text.data(), text.size());
}

void PutU32(std::vector<std::byte> &out, std::size_t at, std::uint32_t value)
{
    std::memcpy(out.data() + at, &value, sizeof value);
}

} // namespace

Writer::Writer(std::string_view type, std::string_view version)
    : type_(FourLetters(type)), version_(FourLetters(version)), shapes_(1)
{
}

Writer::Shape Writer::Lay(std::string_view label, const std::vector<FieldSpec> &fields) const
{
    Shape shape{.label = FourLetters(label), .fields = {}};
    std::vector<Measure> measures;
    for (const FieldSpec &spec : fields)
    {
        shape.fields.push_back({spec});
        if ((spec.flags & (kList | kReference)) != 0)
        {
            measures.push_back({kReferenceSize, kReferenceSize});
        }
        else if ((spec.flags & kStruct) != 0)
        {
            const Shape &held = shapes_.at(spec.type);
            measures.push_back({held.size, held.align});
        }
        else
        {
            measures.push_back(MeasureOf(static_cast<Type>(spec.type)));
        }
    }
    std::vector<std::size_t> order(fields.size());
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::ranges::stable_sort(order,
                             [&](std::size_t a, std::size_t b) { return measures[a].align > measures[b].align; });
    std::size_t offset = 0;
    for (const std::size_t i : order)
    {
        offset = RoundUp(offset, measures[i].align);
        shape.fields[i].offset = static_cast<std::uint32_t>(offset);
        offset += measures[i].size;
    }
    shape.align = order.empty() ? 1 : measures[order.front()].align;
    shape.size = static_cast<std::uint32_t>(RoundUp(offset, shape.align));
    return shape;
}

std::uint16_t Writer::AddTemplate(std::string_view label, const std::vector<FieldSpec> &fields)
{
    Shape shape = Lay(label, fields);
    for (std::size_t i = 1; i < shapes_.size(); ++i)
    {
        const Shape &old = shapes_[i];
        if (old.label == shape.label && old.size == shape.size &&
            std::ranges::equal(old.fields, shape.fields, [](const Laid &a, const Laid &b) {
                return a.spec.label == b.spec.label && a.spec.type == b.spec.type && a.spec.flags == b.spec.flags &&
                       a.offset == b.offset;
            }))
        {
            return static_cast<std::uint16_t>(i);
        }
    }
    shapes_.push_back(std::move(shape));
    return static_cast<std::uint16_t>(shapes_.size() - 1);
}

StructAt Writer::Root(std::string_view label, const std::vector<FieldSpec> &fields)
{
    shapes_.front() = Lay(label, fields);
    return {Alloc(shapes_.front().size, shapes_.front().align, false, 0), 0};
}

const Writer::Laid &Writer::FieldOf(const StructAt &in, std::uint32_t label) const
{
    for (const Laid &field : shapes_.at(in.shape).fields)
    {
        if (field.spec.label == label)
        {
            return field;
        }
    }
    throw std::invalid_argument("gff4::Writer: no such field in the struct's template");
}

StructAt Writer::Child(const StructAt &in, std::uint32_t label) const
{
    const Laid &field = FieldOf(in, label);
    return {in.at + field.offset, field.spec.type};
}

std::size_t Writer::Alloc(std::size_t size, std::size_t align, bool list, std::size_t count)
{
    std::size_t start = 0;
    std::size_t end = 0;
    if (list)
    {
        start = RoundUp(data_.size(), 4);
        while ((start + 4) % align != 0)
        {
            start += 4;
        }
        end = start + 4 + (size * count);
        maxAlign_ = std::max({maxAlign_, align, std::size_t{4}});
    }
    else
    {
        start = RoundUp(data_.size(), align);
        end = start + size;
        maxAlign_ = std::max(maxAlign_, align);
    }
    data_.resize(end, std::byte{0xFF});
    if (list)
    {
        Put(start, static_cast<std::uint32_t>(count));
    }
    return start;
}

template <class T> void Writer::Put(std::size_t at, T value)
{
    std::memcpy(data_.data() + at, &value, sizeof value);
}

void Writer::U8(const StructAt &in, std::uint32_t label, std::uint8_t value)
{
    Put(in.at + FieldOf(in, label).offset, value);
}

void Writer::U32(const StructAt &in, std::uint32_t label, std::uint32_t value)
{
    Put(in.at + FieldOf(in, label).offset, value);
}

void Writer::S32(const StructAt &in, std::uint32_t label, std::int32_t value)
{
    Put(in.at + FieldOf(in, label).offset, value);
}

void Writer::F32(const StructAt &in, std::uint32_t label, float value)
{
    Put(in.at + FieldOf(in, label).offset, value);
}

void Writer::Vector4(const StructAt &in, std::uint32_t label, const std::array<float, 4> &value)
{
    const std::size_t at = in.at + FieldOf(in, label).offset;
    std::memcpy(data_.data() + at, value.data(), sizeof value);
}

std::uint32_t Writer::StoreString(std::wstring_view value)
{
    if (value.empty())
    {
        return kNone;
    }
    if (const auto found = strings_.find(value); found != strings_.end())
    {
        return found->second;
    }
    const std::size_t at = Alloc(2, 2, true, value.size() + 1);
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        Put(at + 4 + (2 * i), static_cast<std::uint16_t>(value[i]));
    }
    Put(at + 4 + (2 * value.size()), std::uint16_t{0});
    strings_.emplace(std::wstring(value), static_cast<std::uint32_t>(at));
    return static_cast<std::uint32_t>(at);
}

void Writer::String(const StructAt &in, std::uint32_t label, std::wstring_view value)
{
    const std::size_t slot = in.at + FieldOf(in, label).offset;
    const std::uint32_t at = StoreString(value);
    Put(slot, at);
}

void Writer::Strings(const StructAt &in, std::uint32_t label, const std::vector<std::wstring> &values)
{
    if (values.empty())
    {
        return;
    }
    const std::size_t list = ScalarList(in, label, values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const std::uint32_t at = StoreString(values[i]);
        Put(list + 4 + (4 * i), at);
    }
}

std::size_t Writer::ScalarList(const StructAt &in, std::uint32_t label, std::size_t count)
{
    const std::size_t slot = in.at + FieldOf(in, label).offset;
    const std::size_t list = Alloc(4, 4, true, count);
    Put(slot, static_cast<std::uint32_t>(list));
    return list;
}

void Writer::Numbers(const StructAt &in, std::uint32_t label, std::span<const std::int32_t> values)
{
    if (values.empty())
    {
        return;
    }
    const std::size_t list = ScalarList(in, label, values.size());
    std::memcpy(data_.data() + list + 4, values.data(), values.size_bytes());
}

void Writer::Floats(const StructAt &in, std::uint32_t label, std::span<const float> values)
{
    if (values.empty())
    {
        return;
    }
    const std::size_t list = ScalarList(in, label, values.size());
    std::memcpy(data_.data() + list + 4, values.data(), values.size_bytes());
}

std::vector<StructAt> Writer::Structs(const StructAt &in, std::uint32_t label, std::size_t count)
{
    std::vector<StructAt> elements;
    if (count == 0)
    {
        return elements;
    }
    const Laid &field = FieldOf(in, label);
    const std::size_t slot = in.at + field.offset;
    const Shape &shape = shapes_.at(field.spec.type);
    const std::size_t list = Alloc(shape.size, shape.align, true, count);
    Put(slot, static_cast<std::uint32_t>(list));
    for (std::size_t i = 0; i < count; ++i)
    {
        elements.push_back({list + 4 + (shape.size * i), field.spec.type});
    }
    return elements;
}

std::vector<std::byte> Writer::Save() const
{
    std::size_t fieldCount = 0;
    for (const Shape &shape : shapes_)
    {
        fieldCount += shape.fields.size();
    }
    const std::size_t fieldsAt = kHeaderSize + (kTemplateSize * shapes_.size());
    const std::size_t headerEnd = fieldsAt + (kFieldSize * fieldCount);
    const std::size_t dataAt = RoundUp(headerEnd, maxAlign_);
    std::vector<std::byte> out(dataAt + data_.size(), std::byte{0xFF});

    PutText(out, 0, "GFF ");
    PutText(out, 4, "V4.0");
    PutText(out, 8, "PC  ");
    PutText(out, 12, type_);
    PutText(out, 16, version_);
    PutU32(out, 20, static_cast<std::uint32_t>(shapes_.size()));
    PutU32(out, 24, static_cast<std::uint32_t>(dataAt));

    std::size_t fieldAt = fieldsAt;
    for (std::size_t i = 0; i < shapes_.size(); ++i)
    {
        const Shape &shape = shapes_[i];
        const std::size_t at = kHeaderSize + (kTemplateSize * i);
        PutText(out, at, shape.label);
        PutU32(out, at + 4, static_cast<std::uint32_t>(shape.fields.size()));
        PutU32(out, at + 8, static_cast<std::uint32_t>(fieldAt));
        PutU32(out, at + 12, shape.size);
        std::vector<Laid> sorted = shape.fields;
        std::ranges::stable_sort(sorted, [](const Laid &a, const Laid &b) { return a.spec.label < b.spec.label; });
        for (const Laid &field : sorted)
        {
            PutU32(out, fieldAt, field.spec.label);
            PutU32(out, fieldAt + 4, field.spec.type | (static_cast<std::uint32_t>(field.spec.flags) << 16));
            PutU32(out, fieldAt + 8, field.offset);
            fieldAt += kFieldSize;
        }
    }
    std::memcpy(out.data() + dataAt, data_.data(), data_.size());
    return out;
}

} // namespace dao::core::gff4
