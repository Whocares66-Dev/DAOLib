// Notes on the walk: written 2026-10-24 for DAOLib #106's step 6 as the C++ half of
// tools/second_reader.py, which reads each file with tools/gff4.py the same way and compares the
// two, value by value; over 6,288 files of the GOG install and 120 saves they agree. A field is
// read through Struct's own calls, as a caller would: a list of structs by Structs (a reference
// that names none left out), a list of numbers by ListBytes, a generic value by whichever type
// it holds, a TlkString by its id.
#include "dao/core/Gff4Walk.h"

#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace dao::core::gff4
{

namespace
{

std::optional<Leaf> Whole(const Struct &s, std::uint32_t label, Type type)
{
    const auto as = [](auto value) -> std::optional<Leaf> {
        if (!value)
        {
            return std::nullopt;
        }
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(*value)>, std::uint64_t>)
        {
            return *value > static_cast<std::uint64_t>(INT64_MAX) ? Leaf(*value)
                                                                  : Leaf(static_cast<std::int64_t>(*value));
        }
        else
        {
            return Leaf(static_cast<std::int64_t>(*value));
        }
    };
    switch (type)
    {
    case Type::U8:
        return as(s.Get<std::uint8_t>(label));
    case Type::S8:
        return as(s.Get<std::int8_t>(label));
    case Type::U16:
        return as(s.Get<std::uint16_t>(label));
    case Type::S16:
        return as(s.Get<std::int16_t>(label));
    case Type::U32:
        return as(s.Get<std::uint32_t>(label));
    case Type::S32:
        return as(s.Get<std::int32_t>(label));
    case Type::U64:
        return as(s.Get<std::uint64_t>(label));
    case Type::S64:
        return as(s.Get<std::int64_t>(label));
    default:
        return std::nullopt;
    }
}

bool IsFloats(Type type)
{
    return type == Type::Vector3 || type == Type::Vector4 || type == Type::Quaternion || type == Type::Color ||
           type == Type::Matrix;
}

std::size_t FloatCount(Type type)
{
    switch (type)
    {
    case Type::Vector3:
        return 3;
    case Type::Matrix:
        return 16;
    default:
        return 4;
    }
}

// How many bytes an element of a list of the type takes (a Vector3 16, as ListBytes lays it out);
// 0 for a type a list of numbers is not.
std::size_t ElementSize(Type type)
{
    switch (type)
    {
    case Type::U8:
    case Type::S8:
        return 1;
    case Type::U16:
    case Type::S16:
        return 2;
    case Type::U32:
    case Type::S32:
    case Type::F32:
        return 4;
    case Type::U64:
    case Type::S64:
    case Type::F64:
        return 8;
    case Type::Vector3:
    case Type::Vector4:
    case Type::Quaternion:
    case Type::Color:
        return 16;
    case Type::Matrix:
        return 64;
    default:
        return 0;
    }
}

template <class T> T Read(const std::byte *at)
{
    T value{};
    std::memcpy(&value, at, sizeof value);
    return value;
}

// One element of a list of numbers, as a leaf; a vector's floats a list of their own.
void Element(const std::byte *at, Type type, Visitor &visitor)
{
    switch (type)
    {
    case Type::U8:
        visitor.Value(static_cast<std::int64_t>(Read<std::uint8_t>(at)));
        return;
    case Type::S8:
        visitor.Value(static_cast<std::int64_t>(Read<std::int8_t>(at)));
        return;
    case Type::U16:
        visitor.Value(static_cast<std::int64_t>(Read<std::uint16_t>(at)));
        return;
    case Type::S16:
        visitor.Value(static_cast<std::int64_t>(Read<std::int16_t>(at)));
        return;
    case Type::U32:
        visitor.Value(static_cast<std::int64_t>(Read<std::uint32_t>(at)));
        return;
    case Type::S32:
        visitor.Value(static_cast<std::int64_t>(Read<std::int32_t>(at)));
        return;
    case Type::U64: {
        const auto value = Read<std::uint64_t>(at);
        visitor.Value(value > static_cast<std::uint64_t>(INT64_MAX) ? Leaf(value)
                                                                    : Leaf(static_cast<std::int64_t>(value)));
        return;
    }
    case Type::S64:
        visitor.Value(Read<std::int64_t>(at));
        return;
    case Type::F32:
        visitor.Value(static_cast<double>(Read<float>(at)));
        return;
    case Type::F64:
        visitor.Value(Read<double>(at));
        return;
    default: {
        const std::size_t count = FloatCount(type);
        visitor.BeginList(count);
        for (std::size_t k = 0; k < count; ++k)
        {
            visitor.Value(static_cast<double>(Read<float>(at + (4 * k))));
        }
        visitor.EndList();
    }
    }
}

void ListOfNumbers(const Struct &s, const Field &f, Visitor &visitor)
{
    const auto type = static_cast<Type>(f.type);
    const std::size_t size = ElementSize(type);
    if (size == 0)
    {
        visitor.Value(std::monostate{}); // a list of talk-table strings, of which no file has one
        return;
    }
    const std::span<const std::byte> bytes = s.ListBytes(f.label, type);
    const std::size_t count = bytes.size() / size;
    visitor.BeginList(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        Element(bytes.data() + (i * size), type, visitor);
    }
    visitor.EndList();
}

void Floats(const std::vector<float> &floats, Visitor &visitor)
{
    visitor.BeginList(floats.size());
    for (const float f : floats)
    {
        visitor.Value(static_cast<double>(f));
    }
    visitor.EndList();
}

void WalkStruct(const Struct &s, Visitor &visitor, int depth);

// A generic field holding one value: a struct walked, else the value by whichever type it holds.
void Generic(const Struct &s, std::uint32_t label, Visitor &visitor, int depth)
{
    if (const auto child = s.Child(label))
    {
        WalkStruct(*child, visitor, depth - 1);
    }
    else if (const auto text = s.String(label))
    {
        visitor.Value(*text);
    }
    else if (const auto tlk = s.Tlk(label))
    {
        visitor.Value(StrRef{tlk->strref});
    }
    else if (const auto f = s.Get<float>(label))
    {
        visitor.Value(static_cast<double>(*f));
    }
    else if (const auto d = s.Get<double>(label))
    {
        visitor.Value(*d);
    }
    else if (const auto floats = s.Floats(label); !floats.empty())
    {
        Floats(floats, visitor);
    }
    else
    {
        for (const Type type : {Type::U8, Type::S8, Type::U16, Type::S16, Type::U32, Type::S32, Type::U64, Type::S64})
        {
            if (const auto value = Whole(s, label, type))
            {
                visitor.Value(*value);
                return;
            }
        }
        visitor.Value(std::monostate{});
    }
}

void Lists(const Struct &s, const Field &f, Visitor &visitor, int depth)
{
    if ((f.flags & kStruct) != 0 || f.type == static_cast<std::uint16_t>(Type::Generic))
    {
        const std::vector<Struct> items = s.Structs(f.label);
        visitor.BeginList(items.size());
        for (const Struct &item : items)
        {
            WalkStruct(item, visitor, depth - 1);
        }
        visitor.EndList();
    }
    else if (f.type == static_cast<std::uint16_t>(Type::String))
    {
        const std::vector<std::wstring> strings = s.Strings(f.label);
        visitor.BeginList(strings.size());
        for (const std::wstring &text : strings)
        {
            visitor.Value(text);
        }
        visitor.EndList();
    }
    else
    {
        ListOfNumbers(s, f, visitor);
    }
}

void Scalar(const Struct &s, const Field &f, Visitor &visitor)
{
    const auto type = static_cast<Type>(f.type);
    if (type == Type::TlkString)
    {
        const auto tlk = s.Tlk(f.label);
        visitor.Value(tlk ? Leaf(StrRef{tlk->strref}) : Leaf());
    }
    else if (type == Type::String)
    {
        visitor.Value(s.String(f.label).value_or(L""));
    }
    else if (type == Type::F32)
    {
        const auto v = s.Get<float>(f.label);
        visitor.Value(v ? Leaf(static_cast<double>(*v)) : Leaf());
    }
    else if (type == Type::F64)
    {
        const auto v = s.Get<double>(f.label);
        visitor.Value(v ? Leaf(*v) : Leaf());
    }
    else if (IsFloats(type))
    {
        Floats(s.Floats(f.label), visitor);
    }
    else
    {
        visitor.Value(Whole(s, f.label, type).value_or(Leaf()));
    }
}

void FieldOf(const Struct &s, const Field &f, Visitor &visitor, int depth)
{
    if ((f.flags & kList) != 0)
    {
        Lists(s, f, visitor, depth);
    }
    else if ((f.flags & kStruct) != 0)
    {
        if (const auto child = s.Child(f.label))
        {
            WalkStruct(*child, visitor, depth - 1);
        }
        else
        {
            visitor.Value(std::monostate{});
        }
    }
    else if (f.type == static_cast<std::uint16_t>(Type::Generic))
    {
        Generic(s, f.label, visitor, depth);
    }
    else
    {
        Scalar(s, f, visitor);
    }
}

void WalkStruct(const Struct &s, Visitor &visitor, int depth)
{
    if (depth < 0)
    {
        visitor.TooDeep();
        return;
    }
    visitor.BeginStruct(s.Label());
    for (const Field &f : s.Shape().fields)
    {
        visitor.Field(f.label);
        if (depth <= 0)
        {
            visitor.TooDeep();
            continue;
        }
        FieldOf(s, f, visitor, depth);
    }
    visitor.EndStruct();
}

} // namespace

void Walk(const Struct &top, Visitor &visitor, int depth)
{
    WalkStruct(top, visitor, depth);
}

} // namespace dao::core::gff4
