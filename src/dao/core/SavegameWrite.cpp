#include "dao/core/Savegame.h"

#include "dao/core/SavegameLabels.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

// Notes on the writing, 2026-10-21 (DAOLib #221).
//
// The writer is the reader (Savegame.cpp) run the other way over Gff4's Editor: the same labels
// (SavegameLabels.h), the same members, in the same order, each Put the mirror of a Take. It edits a copy of
// the original file, so what the reader does not model rides along.
//
// What it rests on:
//   checked    the loaders read a field at its label's offset with the raw width, no type check (dev/SAVES.md,
//              "The rules every loader follows"), so a value is written only into a field of the member's width
//              (and a float only into a float), whatever the sign; the file's own templates give the types, 78
//              of them, the same in every save seen (tools/gff4.py --templates), and a struct's template decides
//              which fields it has, so a member the template lacks is not written (a reader finds an absent
//              label and the engine gives it the template's value);
//   checked    the shapes of the file the engine itself writes (its writers, such as the journal's at 0x006ED770,
//              and the saves they made, read with tools/gff4.py): a list that is empty may be null
//              (0xFFFFFFFF) or have a count of 0; an empty string is a null offset or a count of 1; a generic
//              list's element is the struct's template index and the struct flag, then the struct's offset;
//              a talk-table string's text is 0 where it has none; and every list, string and reference of a
//              blank struct is null, which is how Editor::New makes one;
//   inferred   a new struct's other bytes are 0 and its object id is -1 (the engine's default for an object
//              id, dev/SAVES.md); the engine's writer writes every field of the template, here the members of
//              this model, so a field the reader does not model keeps the blank's 0 in a new struct.
//
// The engine's command writer (0x00698150 the object slot, Command::Write 0x007439C0, WriteActionQueue 0x00745110,
// read by lib-agent-2, checked) agrees with the model: 16722, 16723, 16724 and 16725 always, 16726 only if the
// template has the field (as here, a plain member the template lacks is skipped), 16720 only for a current command
// (an Opt child), 16730 one SAC1 each with 16731 to 16738. It writes both lists empty for an area kept and not
// saved, and writes a sub-action of type 4, 14, 15 or 52 with flag 0 of its data's flags array (saved 0x4103) cleared
// and put back after (0x00743D37), the objects intact, so a move sets itself up again on load; the writer here writes
// the flags as the Save has them.
//
// The engine's whole writer (read 2026-10-21 by lib-agent-2, checked; the tables are in SAVES.md): every object
// goes through CCObject slot 21 (0x00697EE0: ObjectID, Position, Orientation, looping animation, effects,
// Active, Immortal, Plot, TemplateResRef, variables, EventScript, Name, Tag, commands) and then its kind's
// writer (creature 0x006B2470, placeable 0x0074D600, item 0x00754020, trigger 0x00799E10, store 0x007EB510); each
// step is "if the template has the field, a typed setter", and the setter does nothing for a field of another
// type, which is the rule used here (a member is written only into the width the template gives it). Where the
// engine's value is not the model's: Active is saved as active and not deactivating, a Tag equal to the
// TemplateResRef is saved as "" (the loader maps it back), trigger geometry is each point minus the trigger's
// Position (the reader's `geometry` is as saved), a sound's WOrientation is always 1.0, a container's items
// carry their cell as Position (index, +0x600, 0) or (slot, set, 0) when equipped, a pool member's Backpack
// is empty, and a store's buy lists are appended again at each load (a vanilla fault: they double each round
// trip, and this writer writes the lists as the Save has them). A field the engine never writes is 0xFF in the
// template it fills; a blank here is 0 in a scalar (a caller sets every field the engine writes for the kind,
// since an unset one would otherwise read as 0, not as the engine's -1 of 16456 and the like).
//
// Changes are found by reading the original struct, never by comparing two Saves: a scalar is written over
// its slot whatever it was (the same bytes if the value is the same), a string, a list and a talk-table string
// only if what the original reads as differs. So an unchanged Save adds nothing, and a file is identical.
//
// A list's elements are paired with the original's by `source`, the offset of the struct Read found them at,
// or, for the lists whose elements hold nothing the model leaves out (the conversations' lines, the travel
// paths, the minimised headers, the new items, a tactics list's custom lists), by place. A struct that is
// carried over is the original, edited in place if the list kept its order and size, else a copy of it in the
// list written afresh; the original is then dead, so nothing in the copy is shared with anything alive. A
// struct without a source, or whose source was used already, is a blank of the template.
//
// Not written: the add-ins list (16006) and the story so far (16008), which Read does not model; a field the
// template lacks; a struct held in a generic field of its own (none in a save); and the removal of a child
// struct or of a field.

namespace dao::core::savegame
{

namespace
{

using gff4::Editor;
using gff4::File;
using gff4::Struct;
using gff4::Type;

// A struct to write into, and the original struct to compare with (none for a blank one, which holds nothing).
struct Node
{
    Struct edit;
    std::optional<Struct> old;
};

constexpr std::string_view kNoField = "the struct's template has no such field";
constexpr std::string_view kWrongType = "the field's type is not the member's";
constexpr std::string_view kNoList = "the struct's template has no such list";
constexpr std::string_view kNoChild = "the struct's template has no such struct field";

std::size_t WidthOf(std::uint16_t type)
{
    switch (static_cast<Type>(type))
    {
    case Type::U8:
    case Type::S8:
        return 1;
    case Type::U16:
    case Type::S16:
        return 2;
    case Type::U32:
    case Type::S32:
        return 4;
    default:
        return 0;
    }
}

constexpr std::uint16_t Id(Type t)
{
    return static_cast<std::uint16_t>(t);
}

template <class T>
concept HasSource = requires(const T &t) { t.source; };

class Writer
{
  public:
    explicit Writer(const File &file) : file_(file), ed_(file), protos_(file.Templates().size())
    {
    }

    std::size_t Size() const
    {
        return ed_.Size();
    }
    std::vector<std::byte> Result() const
    {
        return ed_.Bytes();
    }

    std::vector<Unwritten> Dropped() const
    {
        std::vector<savegame::Unwritten> out;
        out.reserve(unwritten_.size());
        for (const auto &[key, count] : unwritten_)
        {
            out.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key), count});
        }
        return out;
    }

    // -- Scalars. A plain member is written if the field is there, an Opt that is set must be.

    template <class T>
        requires std::is_arithmetic_v<T>
    void Put(const Node &n, std::uint32_t label, T value)
    {
        Set(n, label, value);
    }
    void Put(const Node &n, std::uint32_t label, const std::wstring &value)
    {
        Set(n, label, value);
    }
    void Put(const Node &n, std::uint32_t label, const Vec3 &value)
    {
        Set(n, label, value);
    }
    void Put(const Node &n, std::uint32_t label, const Quat &value)
    {
        Set(n, label, value);
    }
    template <class T> void Put(const Node &n, std::uint32_t label, const std::optional<T> &value)
    {
        if (value && !Set(n, label, *value))
        {
            Report(n, label, Why(n, label));
        }
    }

    // A talk-table string: the reference, and the text of its own if there is one.
    void PutTlk(const Node &n, std::uint32_t label, const Opt<std::uint32_t> &strref, const Opt<std::wstring> &text)
    {
        if (strref && !SetTlk(n, label, *strref, text))
        {
            Report(n, label, Why(n, label));
        }
    }
    void PutTlk(const Node &n, std::uint32_t label, std::uint32_t strref, const std::wstring &text)
    {
        SetTlk(n, label, strref, text);
    }

    // -- Lists of plain values

    template <class T>
        requires std::is_integral_v<T> && (sizeof(T) == 4)
    void Numbers(const Node &n, std::uint32_t label, const std::vector<T> &values)
    {
        std::vector<std::uint32_t> bits(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            bits[i] = static_cast<std::uint32_t>(values[i]);
        }
        if (!SetNumbers(n, label, bits) && !values.empty())
        {
            Report(n, label, Why(n, label, kNoList));
        }
    }
    template <class T>
        requires std::is_integral_v<T> && (sizeof(T) == 4)
    void Numbers(const Node &n, std::uint32_t label, const std::optional<std::vector<T>> &values)
    {
        if (values)
        {
            Numbers(n, label, *values);
        }
    }

    void Bytes(const Node &n, std::uint32_t label, const std::vector<std::uint8_t> &values)
    {
        if (!SetRaw(n, label, Type::U8, std::as_bytes(std::span(values)), values.size(), false) && !values.empty())
        {
            Report(n, label, Why(n, label, kNoList));
        }
    }
    void Bytes(const Node &n, std::uint32_t label, const std::optional<std::vector<std::uint8_t>> &values)
    {
        if (values)
        {
            Bytes(n, label, *values);
        }
    }

    void Floats(const Node &n, std::uint32_t label, const std::vector<float> &values)
    {
        if (!SetRaw(n, label, Type::F32, std::as_bytes(std::span(values)), values.size(), false) && !values.empty())
        {
            Report(n, label, Why(n, label, kNoList));
        }
    }

    // The 16-byte elements a vector list keeps: three floats and a fourth that is junk, or four.
    void Vectors(const Node &n, std::uint32_t label, const std::vector<Vec3> &values)
    {
        std::vector<float> flat;
        flat.reserve(values.size() * 4);
        for (const Vec3 &v : values)
        {
            flat.insert(flat.end(), {v.x, v.y, v.z, 0.0F});
        }
        // By bits: a float compared as a float is not equal to itself when it is a NaN, and a list the file
        // makes no sense of reads as whatever words lie where its slot points.
        const bool same = n.old && SameBits(Decoded(*n.old, label, Type::Vector3), Decoded(flat));
        if (!SetRaw(n, label, Type::Vector3, std::as_bytes(std::span(flat)), values.size(), same) && !values.empty())
        {
            Report(n, label, Why(n, label, kNoList));
        }
    }
    void Quaternions(const Node &n, std::uint32_t label, const std::vector<Quat> &values)
    {
        std::vector<float> flat;
        flat.reserve(values.size() * 4);
        for (const Quat &q : values)
        {
            flat.insert(flat.end(), {q.x, q.y, q.z, q.w});
        }
        if (!SetRaw(n, label, Type::Quaternion, std::as_bytes(std::span(flat)), values.size(), false) &&
            !values.empty())
        {
            Report(n, label, Why(n, label, kNoList));
        }
    }

    void Strings(const Node &n, std::uint32_t label, const std::vector<std::wstring> &values)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || f->type != Id(Type::String) || (f->flags & (gff4::kList | gff4::kStruct)) != gff4::kList)
        {
            if (!values.empty())
            {
                Report(n, label, Why(n, label, kNoList));
            }
            return;
        }
        if ((n.old ? n.old->Strings(label) : std::vector<std::wstring>()) != values)
        {
            ed_.SetStrings(n.edit, label, values);
        }
    }

    // -- Lists of structs

    // Writes a list of structs. `write(node, value)` fills one; `labelOf(value, old)` is, for a generic list,
    // the template's label the element takes (given the original struct it might be, or null), or none if that
    // struct is not of the element's kind. Elements pair with the original's by `source` if the type has one,
    // else by place.
    template <class T, class LabelOf, class WriteOne>
    void List(const Node &n, std::uint32_t label, const std::vector<T> &values, LabelOf labelOf, WriteOne write)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (!IsStructList(f))
        {
            if (!values.empty())
            {
                Report(n, label, Why(n, label, kNoList));
            }
            return;
        }
        Plan plan;
        plan.old = n.old ? n.old->Structs(label) : std::vector<Struct>();
        plan.pair.assign(values.size(), -1);
        plan.labels.resize(values.size());
        Pair(plan, values, labelOf);
        bool same = values.size() == plan.old.size();
        for (std::size_t i = 0; same && i < values.size(); ++i)
        {
            same = std::cmp_equal(plan.pair[i], i);
        }
        if (same)
        {
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                write(Node{plan.old[i], plan.old[i]}, values[i]);
            }
            return;
        }
        const auto placed = Rebuild(n, label, *f, plan);
        for (const Placed &p : placed)
        {
            write(p.node, values[p.value]);
        }
    }

    // -- Child structs

    // The struct a field holds, in place or by reference; if it is a null reference, a blank one when `make`.
    std::optional<Node> Child(const Node &n, std::uint32_t label, bool make)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || (f->flags & gff4::kStruct) == 0 || (f->flags & gff4::kList) != 0 ||
            f->type >= file_.Templates().size())
        {
            return std::nullopt;
        }
        if ((f->flags & gff4::kReference) == 0)
        {
            Node node{Struct(file_, f->type, n.edit.Offset() + f->offset), std::nullopt};
            if (n.old)
            {
                node.old = Struct(file_, f->type, n.old->Offset() + f->offset);
            }
            return node;
        }
        if (n.old)
        {
            if (const auto child = n.old->Child(label))
            {
                return Node{*child, *child};
            }
        }
        if (!make)
        {
            return std::nullopt;
        }
        const auto blank = NewBlank(f->type);
        if (!blank || !ed_.SetChild(n.edit, label, &*blank))
        {
            return std::nullopt;
        }
        return Node{*blank, std::nullopt};
    }

    // A child the Save has as a member that may be absent: written if set.
    template <class T, class WriteOne>
    void PutChild(const Node &n, std::uint32_t label, const std::optional<T> &value, WriteOne write)
    {
        if (!value)
        {
            return;
        }
        if (const auto child = Child(n, label, true))
        {
            write(*child, *value);
            return;
        }
        Report(n, label, Why(n, label, kNoChild));
    }

    // A child the Save has as a plain member: written where the file has it, else made only if something is
    // written into it (a save that lacks it reads as the member's defaults, which are not worth a struct).
    template <class T, class WriteOne> void PutPlain(const Node &n, std::uint32_t label, const T &value, WriteOne write)
    {
        if (const auto child = Child(n, label, false))
        {
            write(*child, value);
            return;
        }
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr ||
            (f->flags & (gff4::kStruct | gff4::kList | gff4::kReference)) != (gff4::kStruct | gff4::kReference) ||
            f->type >= file_.Templates().size())
        {
            return;
        }
        const std::size_t mark = ed_.Size();
        const auto blank = NewBlank(f->type);
        if (!blank)
        {
            return;
        }
        const std::size_t end = blank->Offset() + file_.Templates()[f->type].size;
        const std::vector<std::byte> pristine = Slice(blank->Offset(), file_.Templates()[f->type].size);
        write(Node{*blank, std::nullopt}, value);
        if (ed_.Size() == end && Slice(blank->Offset(), pristine.size()) == pristine)
        {
            ed_.Truncate(mark);
            for (auto &proto : protos_)
            {
                if (proto && proto->Offset() >= mark)
                {
                    proto.reset();
                }
            }
            return;
        }
        ed_.SetChild(n.edit, label, &*blank);
    }

    static bool Has(const Node &n, std::uint32_t label)
    {
        return n.edit.Shape().Find(label) != nullptr;
    }

    void Report(const Node &n, std::uint32_t label, std::string_view reason)
    {
        ++unwritten_[{n.edit.Label(), label, std::string(reason)}];
    }

  private:
    using Field = gff4::Field;

    static std::string_view Why(const Node &n, std::uint32_t label, std::string_view missing = kNoField)
    {
        return n.edit.Shape().Find(label) == nullptr ? missing : kWrongType;
    }

    std::vector<std::byte> Slice(std::size_t at, std::size_t size) const
    {
        const auto &bytes = ed_.Bytes();
        return {bytes.begin() + static_cast<std::ptrdiff_t>(at),
                bytes.begin() + static_cast<std::ptrdiff_t>(at + size)};
    }

    template <class T>
        requires std::is_arithmetic_v<T>
    bool Set(const Node &n, std::uint32_t label, T value)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || (f->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) != 0)
        {
            return false;
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            const auto v = static_cast<float>(value);
            return f->type == Id(Type::F32) && ed_.SetFloats(n.edit, label, std::span<const float>(&v, 1));
        }
        else
        {
            return WidthOf(f->type) == sizeof(T) && ed_.SetNumber(n.edit, label, static_cast<std::int64_t>(value));
        }
    }

    bool Set(const Node &n, std::uint32_t label, const std::wstring &value)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || f->type != Id(Type::String) || (f->flags & (gff4::kList | gff4::kStruct)) != 0)
        {
            return false;
        }
        const auto now = n.old ? n.old->String(label) : std::optional<std::wstring>(std::wstring());
        return (now && *now == value) || ed_.SetString(n.edit, label, value);
    }

    bool Set(const Node &n, std::uint32_t label, const Vec3 &value)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || f->type != Id(Type::Vector3) ||
            (f->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) != 0)
        {
            return false;
        }
        const float v[3] = {value.x, value.y, value.z};
        return ed_.SetFloats(n.edit, label, v);
    }

    bool Set(const Node &n, std::uint32_t label, const Quat &value)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || f->type != Id(Type::Quaternion) ||
            (f->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) != 0)
        {
            return false;
        }
        const float v[4] = {value.x, value.y, value.z, value.w};
        return ed_.SetFloats(n.edit, label, v);
    }

    bool SetTlk(const Node &n, std::uint32_t label, std::uint32_t strref, const std::optional<std::wstring_view> &text)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || f->type != Id(Type::TlkString) ||
            (f->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) != 0)
        {
            return false;
        }
        if (n.old)
        {
            const auto now = n.old->Tlk(label);
            const std::optional<std::wstring> nowText = n.old->TlkText(label);
            const bool sameText = !text || nowText.value_or(std::wstring()) == *text;
            if (now && now->strref == strref && sameText)
            {
                return true;
            }
        }
        return ed_.SetTlk(n.edit, label, strref, text);
    }
    bool SetTlk(const Node &n, std::uint32_t label, std::uint32_t strref, const Opt<std::wstring> &text)
    {
        return SetTlk(n, label, strref,
                      text ? std::optional<std::wstring_view>(*text) : std::optional<std::wstring_view>());
    }
    bool SetTlk(const Node &n, std::uint32_t label, std::uint32_t strref, const std::wstring &text)
    {
        return SetTlk(n, label, strref, std::optional<std::wstring_view>(text));
    }

    bool SetNumbers(const Node &n, std::uint32_t label, const std::vector<std::uint32_t> &bits)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || (f->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) != gff4::kList ||
            (f->type != Id(Type::U32) && f->type != Id(Type::S32)))
        {
            return false;
        }
        if ((n.old ? n.old->Numbers(label) : std::vector<std::uint32_t>()) == bits)
        {
            return true;
        }
        return ed_.SetList(n.edit, label, std::as_bytes(std::span(bits)), static_cast<std::uint32_t>(bits.size()),
                           gff4::Place::Append);
    }

    // A list of fixed-size elements as bytes, unless the original reads the same (`same`, or its bytes are).
    bool SetRaw(const Node &n, std::uint32_t label, Type element, std::span<const std::byte> bytes, std::size_t count,
                bool same)
    {
        const Field *f = n.edit.Shape().Find(label);
        if (f == nullptr || f->type != Id(element) ||
            (f->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) != gff4::kList)
        {
            return false;
        }
        if (!same && n.old)
        {
            const auto now = n.old->ListBytes(label, element);
            same = element != Type::Vector3 && now.size() == bytes.size() &&
                   (bytes.empty() || std::memcmp(now.data(), bytes.data(), bytes.size()) == 0);
        }
        if (!same && !n.old && count == 0)
        {
            same = true;
        }
        return same || ed_.SetList(n.edit, label, bytes, static_cast<std::uint32_t>(count), gff4::Place::Append);
    }

    // A vector list as the reader reads it: the three or four floats of each element, the fourth of a
    // Vector3's dropped.
    static std::vector<float> Decoded(const Struct &s, std::uint32_t label, Type element)
    {
        const auto raw = s.ListBytes(label, element);
        std::vector<float> out(raw.size() / sizeof(float));
        if (!out.empty())
        {
            std::memcpy(out.data(), raw.data(), out.size() * sizeof(float));
        }
        return Decoded(out);
    }
    static bool SameBits(const std::vector<float> &a, const std::vector<float> &b)
    {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
    }
    static std::vector<float> Decoded(std::vector<float> flat)
    {
        for (std::size_t i = 3; i < flat.size(); i += 4)
        {
            flat[i] = 0.0F;
        }
        return flat;
    }

    // What is paired with what in a list: the original's structs, for each value the one it is (or -1), and the
    // template's label a new element takes.
    struct Plan
    {
        std::vector<Struct> old;
        std::vector<std::ptrdiff_t> pair;
        std::vector<std::string_view> labels;
    };
    // An element and the value it holds.
    struct Placed
    {
        Node node;
        std::size_t value;
    };

    template <class T, class LabelOf> void Pair(Plan &plan, const std::vector<T> &values, LabelOf labelOf) const
    {
        const std::vector<Struct> &old = plan.old;
        std::vector<bool> used(old.size(), false);
        std::unordered_map<std::size_t, std::size_t> bySource;
        if constexpr (HasSource<T>)
        {
            for (std::size_t i = 0; i < old.size(); ++i)
            {
                bySource.emplace(old[i].Offset(), i);
            }
        }
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            plan.labels[i] = labelOf(values[i], nullptr).value_or(std::string_view());
            std::size_t at = old.size();
            if constexpr (HasSource<T>)
            {
                const auto it = values[i].source == kNoSource ? bySource.end() : bySource.find(values[i].source);
                at = it == bySource.end() ? old.size() : it->second;
            }
            else
            {
                at = i < old.size() ? i : old.size();
            }
            if (at < old.size() && !used[at] && labelOf(values[i], &old[at]))
            {
                plan.pair[i] = static_cast<std::ptrdiff_t>(at);
                used[at] = true;
            }
        }
    }

    // Whether a field is a list of structs the writer can lay out: held in place, by reference, or generic.
    bool IsStructList(const Field *f) const
    {
        if (f == nullptr)
        {
            return false;
        }
        const auto kind = static_cast<std::uint16_t>(f->flags & (gff4::kList | gff4::kStruct | gff4::kReference));
        const bool generic = f->type == Id(Type::Generic) && kind == (gff4::kList | gff4::kReference);
        const bool typed =
            (kind == (gff4::kList | gff4::kStruct) || kind == (gff4::kList | gff4::kStruct | gff4::kReference)) &&
            f->type < file_.Templates().size();
        return generic || typed;
    }

    // A prototype to copy for a list held in place, else a struct of its own.
    std::optional<Struct> BlankFor(bool inPlace, std::size_t index)
    {
        if (inPlace)
        {
            return Proto(index);
        }
        return NewBlank(index);
    }

    // Writes the list afresh: a copy of each original struct that is carried over, a blank for each new one, and
    // the field pointed at the new list. Gives the structs to write the values into; an element of a kind the
    // file has no template for is left out.
    std::vector<Placed> Rebuild(const Node &n, std::uint32_t label, const Field &f, const Plan &plan)
    {
        const bool inPlace = (f.flags & gff4::kReference) == 0;
        const bool generic = f.type == Id(Type::Generic);
        std::vector<Struct> elements;
        std::vector<std::optional<Struct>> before;
        std::vector<std::size_t> values;
        for (std::size_t i = 0; i < plan.pair.size(); ++i)
        {
            const std::ptrdiff_t at = plan.pair[i];
            if (at >= 0)
            {
                elements.push_back(plan.old[static_cast<std::size_t>(at)]);
                before.emplace_back(plan.old[static_cast<std::size_t>(at)]);
                values.push_back(i);
                continue;
            }
            const std::optional<std::size_t> index =
                generic ? IndexOfKind(plan.labels[i]) : std::optional<std::size_t>(f.type);
            const auto blank = index ? BlankFor(inPlace, *index) : std::nullopt;
            if (!blank)
            {
                Report(n, label, "the file has no template for the element");
                continue;
            }
            elements.push_back(*blank);
            before.emplace_back(std::nullopt);
            values.push_back(i);
        }
        std::vector<Placed> placed;
        if (inPlace)
        {
            const auto copies = ed_.CopyList(n.edit, label, elements);
            if (!copies)
            {
                Report(n, label, "the list could not be copied");
                return placed;
            }
            elements = *copies;
        }
        else if (!ed_.SetStructs(n.edit, label, elements))
        {
            Report(n, label, "the list could not be written");
            return placed;
        }
        for (std::size_t k = 0; k < elements.size(); ++k)
        {
            placed.push_back({Node{elements[k], before[k]}, values[k]});
        }
        return placed;
    }

    // The template a new element of a generic list takes. A plain event of a file with no EBAS takes the ESCR
    // (its script left null: the plain loader reads the five base fields and never the payload).
    std::optional<std::size_t> IndexOfKind(std::string_view label) const
    {
        const auto index = IndexOf(label);
        return index || label != "EBAS" ? index : IndexOf("ESCR");
    }

    std::optional<std::size_t> IndexOf(std::string_view label) const
    {
        const auto &templates = file_.Templates();
        for (std::size_t i = 0; i < templates.size(); ++i)
        {
            if (templates[i].label == label)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    // A blank struct whose object id is -1, the engine's default.
    std::optional<Struct> NewBlank(std::size_t index)
    {
        auto blank = ed_.New(index);
        if (blank)
        {
            const Field *id = blank->Shape().Find(L::kObjectId);
            if (id != nullptr && WidthOf(id->type) == 4 &&
                (id->flags & (gff4::kList | gff4::kStruct | gff4::kReference)) == 0)
            {
                ed_.SetNumber(*blank, L::kObjectId, -1);
            }
        }
        return blank;
    }

    // A blank to copy from, made once.
    const Struct &Proto(std::size_t index)
    {
        if (!protos_[index])
        {
            protos_[index] = NewBlank(index);
        }
        return *protos_[index];
    }

    const File &file_;
    Editor ed_;
    std::vector<std::optional<Struct>> protos_;
    std::map<std::tuple<std::string, std::uint32_t, std::string>, std::size_t> unwritten_;
};

// A typed list's elements are all of one template.
constexpr auto kTyped = [](const auto &, const Struct *) -> std::optional<std::string_view> {
    return std::string_view();
};

template <class T, class WriteOne>
void List(Writer &w, const Node &n, std::uint32_t label, const std::vector<T> &values, WriteOne write)
{
    w.List(n, label, values, kTyped, write);
}

// -- The writers, one for each struct the reader fills, in its order

void Write(Writer &w, const Node &n, const DataArrays &v);
void Write(Writer &w, const Node &n, const Effect &v);
void Write(Writer &w, const Node &n, const Command &v);
void Write(Writer &w, const Node &n, const Subaction &v);
void Write(Writer &w, const Node &n, const ActionQueue &v);
void Write(Writer &w, const Node &n, const Item &v);
void Write(Writer &w, const Node &n, const Stat &v);
void Write(Writer &w, const Node &n, const Quickslot &v);
void Write(Writer &w, const Node &n, const Stats &v);
void Write(Writer &w, const Node &n, const Appearance &v);
void Write(Writer &w, const Node &n, const ClassRank &v);
void Write(Writer &w, const Node &n, const TacticEntry &v);
void Write(Writer &w, const Node &n, const Tactics &v);
void Write(Writer &w, const Node &n, const Creature &v);
void Write(Writer &w, const Node &n, const Placeable &v);
void Write(Writer &w, const Node &n, const Sound &v);
void Write(Writer &w, const Node &n, const Trigger &v);
void Write(Writer &w, const Node &n, const AreaOfEffect &v);
void Write(Writer &w, const Node &n, const Waypoint &v);
void Write(Writer &w, const Node &n, const Store &v);
void Write(Writer &w, const Node &n, const Area &v);
void Write(Writer &w, const Node &n, const Quest &v);

template <class T> void Items(Writer &w, const Node &n, std::uint32_t label, const std::vector<T> &values)
{
    List(w, n, label, values, [&w](const Node &c, const T &v) { Write(w, c, v); });
}
template <class T>
void Items(Writer &w, const Node &n, std::uint32_t label, const std::optional<std::vector<T>> &values)
{
    if (values)
    {
        Items(w, n, label, *values);
    }
}

std::string_view VariableLabel(const Variable &v)
{
    switch (v.value.index())
    {
    case 0:
        return "SVI1";
    case 1:
        return "SVF1";
    case 2:
        return "SVS1";
    case 3:
        return "SVO1";
    default:
        return "SVR1";
    }
}

// A ScriptVarTable: a generic list of the five kinds, each the template of its value's type.
void Variables(Writer &w, const Node &n, const std::vector<Variable> &values)
{
    w.List(
        n, L::kVariables, values,
        [](const Variable &v, const Struct *old) -> std::optional<std::string_view> {
            const std::string_view want = VariableLabel(v);
            if (old != nullptr && old->Label() != want)
            {
                return std::nullopt;
            }
            return want;
        },
        [&w](const Node &c, const Variable &v) {
            w.Put(c, L::kVarName, v.name);
            struct Value
            {
                Writer &w;
                const Node &c;
                void operator()(std::int32_t x) const
                {
                    w.Put(c, L::kVarValue, x);
                }
                void operator()(float x) const
                {
                    w.Put(c, L::kVarValue, x);
                }
                void operator()(const std::wstring &x) const
                {
                    w.Put(c, L::kVarValue, x);
                }
                void operator()(const ObjectId &x) const
                {
                    w.Put(c, L::kVarValue, x.id);
                }
                void operator()(const ResRef &x) const
                {
                    w.Put(c, L::kVarValue, x.name);
                }
            };
            std::visit(Value{w, c}, v.value);
        });
}

void Write(Writer &w, const Node &n, const DataArrays &v)
{
    w.Numbers(n, L::kArrInt, v.ints);
    w.Floats(n, L::kArrFloat, v.floats);
    w.Bytes(n, L::kArrBool, v.bools);
    w.Numbers(n, L::kArrOid, v.objects);
    w.Strings(n, L::kArrString, v.strings);
    w.Vectors(n, L::kArrVector, v.vectors);
    w.Quaternions(n, L::kArrQuaternion, v.quaternions);
}

void Write(Writer &w, const Node &n, const Effect &v)
{
    w.Put(n, L::kEffId, v.id);
    w.Put(n, L::kEffType, v.type);
    w.Put(n, L::kEffDurationType, v.durationType);
    w.Put(n, L::kEffDuration, v.duration);
    w.Put(n, L::kEffTimeIndex, v.timeIndex);
    w.Put(n, L::kEffAnimation, v.animation);
    w.Put(n, L::kEffPriority, v.priority);
    w.Put(n, L::kEffCreator, v.creator);
    w.Put(n, L::kEffAbility, v.abilityId);
    w.PutPlain(n, L::kEffEngineData, v.engineData, [&w](const Node &c, const DataArrays &d) { Write(w, c, d); });
    w.Put(n, L::kEffResource2, v.resource2);
    w.Put(n, L::kEffFlags, v.flags);
    w.PutPlain(n, L::kEffDataarray, v.data, [&w](const Node &c, const DataArrays &d) { Write(w, c, d); });
}

void Write(Writer &w, const Node &n, const Command &v)
{
    w.Put(n, L::kCmdCommandId, v.commandId);
    w.Put(n, L::kCmdId, v.id);
    w.Put(n, L::kCmdStatic, v.isStatic);
    w.PutPlain(n, L::kCmdData, v.data, [&w](const Node &c, const DataArrays &d) { Write(w, c, d); });
    w.Put(n, L::kCmdPlayerIssued, v.playerIssued);
}

void Write(Writer &w, const Node &n, const Subaction &v)
{
    w.Put(n, L::kSubId, v.id);
    w.Put(n, L::kSubCore, v.coreSubaction);
    w.Put(n, L::kSubInterruptable, v.coreInterruptable);
    w.Put(n, L::kSubTimeIndex, v.timeIndex);
    w.Put(n, L::kSubLastTimeIndex, v.lastTimeIndex);
    w.Put(n, L::kSubLength, v.length);
    w.Put(n, L::kSubStartTime, v.startTime);
    w.PutPlain(n, L::kSubData, v.data, [&w](const Node &c, const DataArrays &d) { Write(w, c, d); });
}

void Write(Writer &w, const Node &n, const ActionQueue &v)
{
    w.PutChild(n, L::kCurrentCommand, v.current, [&w](const Node &c, const Command &d) { Write(w, c, d); });
    Items(w, n, L::kSubactionList, v.subactions);
}

// The property lists are written only where they are read, inside ItemAbilityID's presence.
void Write(Writer &w, const Node &n, const Item &v)
{
    w.Put(n, L::kTemplateResRef, v.templateResRef);
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kItemCost, v.cost);
    w.Put(n, L::kItemStackSize, v.stackSize);
    if (Writer::Has(n, L::kItemAbilityId))
    {
        w.Put(n, L::kItemAbilityId, v.abilityId);
        w.Numbers(n, L::kItemProperties, v.properties);
        w.Numbers(n, L::kItemPowers, v.propertyPowers);
        w.Numbers(n, L::kItemEffectIds, v.propertyEffectIds);
        w.Numbers(n, L::kItemVfxIds, v.propertyVfxIds);
    }
    else
    {
        w.Put(n, L::kItemAbilityId, v.abilityId);
        if (!v.properties.empty() || !v.propertyPowers.empty() || !v.propertyEffectIds.empty() ||
            !v.propertyVfxIds.empty())
        {
            w.Report(n, L::kItemProperties, "the item has no ability field, so its property lists are not read");
        }
    }
    w.Strings(n, L::kItemSubitems, v.subitemResRefs);
    w.Put(n, L::kItemRecipe, v.craftingRecipeId);
    w.Put(n, L::kItemBaseCost, v.baseCost);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kItemDroppable, v.droppable);
    w.Put(n, L::kItemDamaged, v.damaged);
    w.Put(n, L::kItemIrremovable, v.irremovable);
    w.Put(n, L::kItemIndestructible, v.indestructible);
    w.Put(n, L::kItemMaterial, v.materialType);
    w.Put(n, L::kItemStealable, v.stealable);
    w.Put(n, L::kItemInfinite, v.infinite);
    w.Put(n, L::kItemVfxPropertyId, v.currentVfxPropertyId);
    w.Put(n, L::kItemVfxPropertyPower, v.currentVfxPropertyPower);
    w.Put(n, L::kItemUnnamed, v.field16237);
    w.Put(n, L::kObjectPlot, v.plot);
    Items(w, n, L::kEffectList, v.effects);
    Variables(w, n, v.variables);
}

std::string_view StatLabel(const Stat &v)
{
    switch (v.kind)
    {
    case Stat::Kind::Attribute:
        return "ATTR";
    case Stat::Kind::Depletable:
        return "DPLT";
    default:
        return "SMPL";
    }
}

// A stat list is generic: the element's template is its kind (the engine goes by its row, not the label).
void StatList(Writer &w, const Node &n, std::uint32_t label, const std::vector<Stat> &values)
{
    w.List(
        n, label, values,
        [](const Stat &v, const Struct *old) -> std::optional<std::string_view> {
            const std::string_view want = StatLabel(v);
            return old != nullptr && old->Label() != want ? std::nullopt : std::optional<std::string_view>(want);
        },
        [&w](const Node &c, const Stat &v) { Write(w, c, v); });
}

void Write(Writer &w, const Node &n, const Stat &v)
{
    w.Put(n, L::kStatIndex, v.index);
    w.Put(n, L::kStatBase, v.base);
    w.Put(n, L::kStatModifier, v.modifier);
    w.Put(n, L::kStatCurrent, v.current);
    w.Put(n, L::kStatComRegen, v.combatRegen);
    w.Put(n, L::kStatRegen, v.regen);
}

void Write(Writer &w, const Node &n, const Quickslot &v)
{
    w.Put(n, L::kQsAbility, v.ability);
    w.Put(n, L::kQsItemTag, v.itemTag);
    w.Put(n, L::kQsTemplate, v.templateResRef);
    w.Put(n, L::kQsNumber, v.number);
}

void Write(Writer &w, const Node &n, const Stats &v)
{
    w.Numbers(n, L::kSpellList, v.spells);
    w.Numbers(n, L::kTalentList, v.talents);
    w.Numbers(n, L::kSkillList, v.skills);
    StatList(w, n, L::kStatList, v.stats);
    StatList(w, n, L::kHeroicStatList, v.heroicStats);
    StatList(w, n, L::kHeroicPartyStatList, v.heroicPartyStats);
    w.Numbers(n, L::kModalAbilities, v.modalAbilities);
    constexpr std::uint32_t slots[] = {L::kQuickslots, L::kQuickslots1, L::kQuickslots2, L::kQuickslots3,
                                       L::kQuickslots4};
    for (std::size_t i = 0; i < v.quickslots.size(); ++i)
    {
        Items(w, n, slots[i], v.quickslots[i]);
    }
    w.Put(n, L::kCurrentQbar, v.currentQbar);
    w.Put(n, L::kLockQbar, v.lockQbar);
    w.Put(n, L::kAutoLevelUp, v.autoLevelUp);
    w.Put(n, L::kUnnamed16339, v.field16339);
    w.Put(n, L::kUnnamed16340, v.field16340);
}

void Write(Writer &w, const Node &n, const Appearance &v)
{
    w.Put(n, L::kAppType, v.type);
    w.Put(n, L::kAppGender, v.gender);
    w.Put(n, L::kAppGore, v.gore);
    w.Put(n, L::kAppDecapitated, v.decapitated);
    w.Bytes(n, L::kAppHeraldry, v.heraldryVariation);
    w.Put(n, L::kAppOriginalType, v.originalType);
    w.Put(n, L::kAppMorphName, v.morphName);
}

void Write(Writer &w, const Node &n, const ClassRank &v)
{
    w.Put(n, L::kClassId, v.classId);
    w.Put(n, L::kClassRank, v.rank);
}

void Write(Writer &w, const Node &n, const TacticEntry &v)
{
    w.Put(n, L::kTeTargetObject, v.targetObject);
    w.Put(n, L::kTeConditionObject, v.conditionObject);
    w.Put(n, L::kTeEnabled, v.enabled);
    w.Put(n, L::kTeTarget, v.target);
    w.Put(n, L::kTeCondition, v.condition);
    w.Put(n, L::kTeCommand, v.command);
    w.Put(n, L::kTeParam, v.commandParam);
    w.Put(n, L::kTeItemTag, v.commandItemTag);
    w.Put(n, L::kTeItemResRef, v.commandItemResRef);
}

void Write(Writer &w, const Node &n, const Tactics &v)
{
    w.Put(n, L::kTacticsEnabled, v.enabled);
    Items(w, n, L::kTacticsList, v.entries);
    w.Put(n, L::kTacticsDirty, v.dirty);
    w.Put(n, L::kTacticsPresetType, v.presetType);
    w.Put(n, L::kTacticsPresetIndex, v.presetIndex);
    w.Bytes(n, L::kTacticsPresetList, v.presetList);
    List(w, n, L::kTacticsCustomList, v.custom,
         [&w](const Node &c, const std::vector<TacticEntry> &entries) { Items(w, c, L::kTacticsList, entries); });
}

void Headers(Writer &w, const Node &n, std::uint32_t label, const std::vector<std::int16_t> &ids)
{
    List(w, n, label, ids, [&w](const Node &c, std::int16_t id) { w.Put(c, L::kHeaderId, id); });
}

void Write(Writer &w, const Node &n, const Portrait &p)
{
    w.Put(n, L::kPortraitPitch, p.pitch);
    w.Put(n, L::kPortraitYaw, p.yaw);
    w.Put(n, L::kPortraitTint, p.tint);
    w.Put(n, L::kPortraitExpression, p.expression);
    w.Put(n, L::kPortraitDistance, p.distance);
    w.Put(n, L::kPortraitPositionH, p.positionH);
    w.Put(n, L::kPortraitPositionV, p.positionV);
}

void Write(Writer &w, const Node &n, const Creature &v)
{
    w.Put(n, L::kTemplateResRef, v.templateResRef);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kOrientation, v.orientation);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kObjectActive, v.active);
    w.Put(n, L::kPartyMembers, v.partyMembers);
    w.Put(n, L::kNonPartyMembers, v.nonPartyMembers);
    w.Put(n, L::kPartySubstate, v.partySubstate);
    w.Put(n, L::kPartyLocked, v.partyLocked);
    w.Put(n, L::kPartyFollow, v.partyFollow);
    w.PutChild(n, L::kCreatureStats, v.stats, [&w](const Node &c, const Stats &s) { Write(w, c, s); });
    Items(w, n, L::kBackpack, v.backpack);
    Items(w, n, L::kPlotItems, v.plotItems);
    w.Put(n, L::kMoney, v.money);
    w.Put(n, L::kEquipmentActiveSet, v.activeSet);
    Items(w, n, L::kEquipmentItems, v.equipment);
    w.Put(n, L::kObjectImmortal, v.immortal);
    w.Put(n, L::kObjectEventScript, v.eventScript);
    w.Put(n, L::kMaxItems, v.maxItems);
    w.Numbers(n, L::kCraftingRecipes, v.craftingRecipes);
    w.Put(n, L::kObjectPlot, v.plot);
    w.Put(n, L::kObjectTreasureGroup, v.treasureGroup);
    w.PutTlk(n, L::kObjectName, v.nameStrref, v.nameText);
    w.Put(n, L::kLoopingAnimation, v.loopingAnimation);
    w.Put(n, L::kInteractionRadius, v.interactionRadius);
    w.Put(n, L::kImportance, v.importance);
    w.PutChild(n, L::kAppearance, v.appearance, [&w](const Node &c, const Appearance &a) { Write(w, c, a); });
    if (v.portrait)
    {
        Write(w, n, *v.portrait);
    }
    w.Put(n, L::kGroupId, v.groupId);
    w.Put(n, L::kTeamId, v.teamId);
    w.Put(n, L::kStealth, v.stealth);
    w.Put(n, L::kIsPlotGiver, v.plotGiver);
    w.Put(n, L::kCanLevelUp, v.canLevelUp);
    w.Put(n, L::kTrackable, v.trackable);
    w.Put(n, L::kControllable, v.controllable);
    w.Put(n, L::kInteractive, v.interactive);
    w.Put(n, L::kRace, v.race);
    w.Put(n, L::kCanChangeEquipment, v.canChangeEquipment);
    Items(w, n, L::kClassRankList, v.classRanks);
    w.Put(n, L::kIsGhost, v.isGhost);
    w.Put(n, L::kShowAsAllyOnMap, v.showAsAllyOnMap);
    w.Put(n, L::kIsStatue, v.isStatue);
    Headers(w, n, L::kMinimizedSkills, v.minimizedSkillHeaders);
    Headers(w, n, L::kMinimizedTalents, v.minimizedTalentHeaders);
    w.Put(n, L::kItemsScaled, v.itemsScaled);
    w.Put(n, L::kHeartbeat, v.heartbeatInterval);
    w.Put(n, L::kRoamRadius, v.roamRadius);
    w.Put(n, L::kRoamCenter, v.roamCenter);
    w.Put(n, L::kPoolName, v.poolName);
    w.Put(n, L::kPoolAvailable, v.poolAvailable);
    w.Put(n, L::kNoPermDeath, v.noPermDeath);
    w.Put(n, L::kTimeSinceDeath, v.timeSinceDeath);
    w.Put(n, L::kBodybagId, v.bodybagId);
    w.Put(n, L::kCreatureRank, v.rank);
    Items(w, n, L::kEffectList, v.effects);
    Items(w, n, L::kCommandList, v.commands);
    w.PutChild(n, L::kCurrentActionQueue, v.actionQueue, [&w](const Node &c, const ActionQueue &q) { Write(w, c, q); });
    w.Put(n, L::kTacticsHasTable, v.tacticsHasTable);
    w.PutChild(n, L::kTacticsTable, v.tactics, [&w](const Node &c, const Tactics &t) { Write(w, c, t); });
    w.Bytes(n, L::kPlayerMorph, v.playerMorph);
    w.Put(n, L::kPlayerSoundSet, v.playerSoundSet);
    w.Put(n, L::kDefaultSoundSet, v.defaultSoundSet);
    Variables(w, n, v.variables);
}

void Write(Writer &w, const Node &n, const Placeable &v)
{
    w.Put(n, L::kTemplateResRef, v.templateResRef);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kOrientation, v.orientation);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kPlaceableState, v.state);
    w.Put(n, L::kPlaceableUseable, v.useable);
    w.Put(n, L::kObjectActive, v.active);
    w.Put(n, L::kObjectImmortal, v.immortal);
    w.Put(n, L::kObjectEventScript, v.eventScript);
    Items(w, n, L::kItems, v.items);
    w.Put(n, L::kMaxItems, v.maxItems);
    w.Put(n, L::kObjectPlot, v.plot);
    w.Put(n, L::kObjectHealth, v.health);
    w.Put(n, L::kObjectMaxHealth, v.maxHealth);
    w.Put(n, L::kObjectRank, v.rank);
    w.Put(n, L::kObjectTreasureGroup, v.treasureGroup);
    w.PutTlk(n, L::kObjectName, v.nameStrref, v.nameText);
    w.Put(n, L::kLootableAppearance, v.lootableCreatureAppearanceType);
    w.Put(n, L::kObjectPickLock, v.pickLock);
    w.Put(n, L::kTrapDetected, v.trapDetected);
    w.Put(n, L::kDetectCheck, v.detectCheck);
    w.Put(n, L::kDisarmCheck, v.disarmCheck);
    w.Put(n, L::kInteractionRadius, v.interactionRadius);
    w.Put(n, L::kTeamId, v.teamId);
    w.Put(n, L::kIsPlotGiver, v.plotGiver);
    w.Put(n, L::kIsBodybag, v.isBodybag);
    w.Put(n, L::kLootableObject, v.lootableObjectId);
    Items(w, n, L::kEffectList, v.effects);
    Items(w, n, L::kCommandList, v.commands);
    w.PutChild(n, L::kCurrentActionQueue, v.actionQueue, [&w](const Node &c, const ActionQueue &q) { Write(w, c, q); });
    Variables(w, n, v.variables);
}

void Write(Writer &w, const Node &n, const Sound &v)
{
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kSoundTag, v.soundTag);
    w.Put(n, L::kSoundActive, v.active);
    w.Put(n, L::kSoundName, v.name);
    w.Put(n, L::kSoundX, v.soundPosition.x);
    w.Put(n, L::kSoundY, v.soundPosition.y);
    w.Put(n, L::kSoundZ, v.soundPosition.z);
    w.Put(n, L::kSoundOx, v.soundOrientation.x);
    w.Put(n, L::kSoundOy, v.soundOrientation.y);
    w.Put(n, L::kSoundOz, v.soundOrientation.z);
    w.Put(n, L::kSoundOw, v.soundOrientation.w);
    w.Put(n, L::kSoundVolume, v.volume);
    w.Put(n, L::kSoundPitch, v.pitch);
    w.Put(n, L::kSoundFadeIn, v.fadeIn);
    w.Put(n, L::kSoundFadeOut, v.fadeOut);
    w.Put(n, L::kSoundMaxDistance, v.maxDistanceMult);
    w.Put(n, L::kSoundConeInside, v.coneInside);
    w.Put(n, L::kSoundConeOutside, v.coneOutside);
    w.Put(n, L::kSoundConeVolume, v.coneVolume);
    w.Put(n, L::kSoundPriority, v.priority);
    w.Put(n, L::kSoundOccludable, v.occludable);
}

// The template of a trigger: the label Read kept, else that of its type (SavegameAreaTriggerType).
std::string_view TriggerLabel(const Trigger &v)
{
    if (!v.label.empty())
    {
        return v.label;
    }
    switch (v.KindOf())
    {
    case Trigger::Kind::Transition:
        return "TRAT";
    case Trigger::Kind::Trap:
        return "TRTP";
    case Trigger::Kind::AudioReverb:
        return "TRAR";
    case Trigger::Kind::AudioVolume:
        return "TRAV";
    case Trigger::Kind::MusicVolume:
        return "TRAM";
    default:
        return "TRGR";
    }
}

void Write(Writer &w, const Node &n, const Trigger &v)
{
    w.Put(n, L::kTemplateResRef, v.templateResRef);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kOrientation, v.orientation);
    w.Put(n, L::kObjectId, v.id);
    w.Vectors(n, L::kTrigGeometry, v.geometry);
    w.Put(n, L::kTrigType, v.type);
    w.Put(n, L::kObjectActive, v.active);
    w.Put(n, L::kObjectEventScript, v.eventScript);
    Items(w, n, L::kCommandList, v.commands);
    w.PutChild(n, L::kCurrentActionQueue, v.actionQueue, [&w](const Node &c, const ActionQueue &q) { Write(w, c, q); });
    Variables(w, n, v.variables);
    w.Put(n, L::kTrigDetectable, v.detectable);
    w.Put(n, L::kTrigDisarmable, v.disarmable);
    w.Put(n, L::kTrigDetectCheck, v.detectCheck);
    w.Put(n, L::kTrigDisarmCheck, v.disarmCheck);
    w.Put(n, L::kTrigLastDisarmed, v.lastDisarmed);
    w.Put(n, L::kTrigLoadScreen, v.loadScreen);
    w.Put(n, L::kTrigReverb, v.reverbPreset);
    Items(w, n, L::kTrigSounds, v.sounds);
    w.Put(n, L::kTrigMusicEnter, v.musicEnterState);
    w.Put(n, L::kTrigMusicExit, v.musicExitState);
    w.Put(n, L::kTrigMusicEnterDelay, v.musicEnterDelay);
    w.Put(n, L::kTrigMusicExitDelay, v.musicExitDelay);
}

// An absent Flags reads as the AbilityID [exe 0x007E5060], so the member is not news where the field is absent.
void Write(Writer &w, const Node &n, const AreaOfEffect &v)
{
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kOrientation, v.orientation);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kObjectEventScript, v.eventScript);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kAoeId, v.aoeId);
    w.Put(n, L::kAoeShape, v.shape);
    w.Put(n, L::kAoeRadius, v.radius);
    w.Put(n, L::kAoeWidth, v.width);
    w.Put(n, L::kAoeLength, v.length);
    w.Put(n, L::kAoeCreator, v.creator);
    w.Put(n, L::kAoeDuration, v.duration);
    w.Put(n, L::kAoeDurationType, v.durationType);
    w.Put(n, L::kAoeLinked, v.linked);
    w.Put(n, L::kAoeAbility, v.abilityId);
    if (Writer::Has(n, L::kAoeFlags) || v.flags != v.abilityId)
    {
        w.Put(n, L::kAoeFlags, v.flags);
    }
    w.Put(n, L::kAoeStationary, v.stationary);
}

void Write(Writer &w, const Node &n, const Waypoint &v)
{
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kOrientation, v.orientation);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kObjectActive, v.active);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kWpMapNote, v.mapNote);
    w.Put(n, L::kWpEnabled, v.mapNoteEnabled);
    w.Put(n, L::kWpText, v.mapNoteText);
    w.Put(n, L::kWpType, v.mapNoteType);
    w.PutTlk(n, L::kWpLocText, v.mapNoteTextStrref, v.mapNoteTextOwn);
}

void Write(Writer &w, const Node &n, const Store &v)
{
    w.Put(n, L::kTemplateResRef, v.templateResRef);
    w.Put(n, L::kPosition, v.position);
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kStoreMarkDown, v.markDown);
    w.Put(n, L::kStoreMarkUp, v.markUp);
    w.Put(n, L::kStoreGold, v.gold);
    w.Put(n, L::kStoreMaxBuy, v.maxBuyPrice);
    w.Numbers(n, L::kStoreWillNotBuy, v.willNotBuy);
    w.Numbers(n, L::kStoreWillOnlyBuy, v.willOnlyBuy);
    Items(w, n, L::kStoreItemList, v.items);
    w.Put(n, L::kObjectEventScript, v.eventScript);
    w.Put(n, L::kObjectTag, v.tag);
    w.Put(n, L::kMaxItems, v.maxItems);
}

void Triggers(Writer &w, const Node &n, const std::vector<Trigger> &values)
{
    w.List(
        n, L::kAreaTriggers, values,
        [](const Trigger &v, const Struct *old) -> std::optional<std::string_view> {
            const std::string_view want = TriggerLabel(v);
            return old != nullptr && old->Label() != want ? std::nullopt : std::optional<std::string_view>(want);
        },
        [&w](const Node &c, const Trigger &v) { Write(w, c, v); });
}

void Write(Writer &w, const Node &n, const Area &v)
{
    w.Put(n, L::kTag, v.tag);
    w.Put(n, L::kObjectId, v.id);
    Items(w, n, L::kAreaPlaceables, v.placeables);
    Items(w, n, L::kAreaCreatures, v.creatures);
    Triggers(w, n, v.triggers);
    Items(w, n, L::kAreaAoes, v.aoes);
    Items(w, n, L::kAreaWaypoints, v.waypoints);
    w.Bytes(n, L::kAreaMap, v.map.bits);
    Items(w, n, L::kAreaStores, v.stores);
    w.Numbers(n, L::kAreaRoomsViewed, v.roomsViewed);
    Items(w, n, L::kAreaSounds, v.sounds);
    w.Put(n, L::kAreaMinImportance, v.minCreatureImportance);
    Items(w, n, L::kEffectList, v.effects);
    Variables(w, n, v.variables);
}

// -- The campaign

void Write(Writer &w, const Node &n, const Quest &v)
{
    w.PutTlk(n, L::kQTitle, v.titleStrref, v.titleText);
    w.PutTlk(n, L::kQText, v.textStrref, v.text);
    w.Put(n, L::kQParent, v.parentPlot);
    w.Put(n, L::kJResRef, v.resRef);
    w.Put(n, L::kQAreaTag, v.areaTag);
    w.Put(n, L::kQCompleted, v.completed);
    w.Put(n, L::kQGroup, v.group);
    w.Put(n, L::kQUpdated, v.updated);
    w.Put(n, L::kQOffer, v.offerId);
}

void Write(Writer &w, const Node &n, const Journal &v)
{
    Items(w, n, L::kJActive, v.active);
    Items(w, n, L::kJComplete, v.completed);
    Items(w, n, L::kJOrphans, v.orphans);
    w.Put(n, L::kJResRef, v.activeResRef);
    w.PutTlk(n, L::kJStoryText, v.storyTextStrref, v.storyText);
    List(w, n, L::kJPlotDestinations, v.plotDestinations, [&w](const Node &c, const PlotDestination &d) {
        w.Put(c, L::kPdTag, d.tag);
        w.Strings(c, L::kPdGuids, d.guids);
    });
    List(w, n, L::kJConversations, v.conversations, [&w](const Node &c, const std::vector<ConversationLine> &lines) {
        List(w, c, L::kConvLines, lines, [&w](const Node &l, const ConversationLine &line) {
            w.Put(l, L::kClSpeaker, line.speaker);
            w.Put(l, L::kClText, line.text);
            w.Put(l, L::kClReply, line.reply);
        });
    });
    List(w, n, L::kJGroups, v.groups, [&w](const Node &c, const JournalGroup &g) {
        w.Put(c, L::kJgResRef, g.resRef);
        w.Put(c, L::kJgCurrent, g.openInCurrent);
        w.Put(c, L::kJgCompleted, g.openInCompleted);
        w.Put(c, L::kJgPriority, g.priority);
    });
}

void TravelPath(Writer &w, const Node &n, std::uint32_t label, const std::vector<std::pair<float, float>> &points)
{
    List(w, n, label, points, [&w](const Node &c, const std::pair<float, float> &p) {
        w.Put(c, L::kWmPosX, p.first);
        w.Put(c, L::kWmPosY, p.second);
    });
}

void Write(Writer &w, const Node &n, const WorldMap &v)
{
    w.Put(n, L::kWmPrimary, v.primaryMap);
    w.Put(n, L::kWmSecondary, v.secondaryMap);
    List(w, n, L::kWmMaps, v.maps, [&w](const Node &c, const WorldMapMap &m) {
        w.Put(c, L::kObjectId, m.id);
        w.Put(c, L::kWmTag, m.tag);
        w.Put(c, L::kWmPlayerLoc, m.playerLocation);
        List(w, c, L::kWmPins, m.pins, [&w](const Node &p, const WorldMapPin &pin) {
            w.Put(p, L::kObjectId, pin.id);
            w.Put(p, L::kWmPinTag, pin.tag);
            w.Put(p, L::kWmPinState, pin.state);
            w.Put(p, L::kWmPinRecent, pin.recentlyActivated);
            w.Put(p, L::kWmPinPrevious, pin.activatedPreviously);
            w.Put(p, L::kWmPinLast, pin.lastState);
            w.PutTlk(p, L::kWmPinName, pin.nameStrref, pin.nameText);
        });
        TravelPath(w, c, L::kWmPathBefore, m.travelPathBefore);
        TravelPath(w, c, L::kWmPathAfter, m.travelPathAfter);
    });
    w.Put(n, L::kWmGui, v.guiStatus);
    w.Put(n, L::kWmLastPin, v.lastPinClicked);
}

// The plot actions' list may hold two templates (PTAI with a count, PTAJ without): an element keeps its own.
void Write(Writer &w, const Node &n, const PlotActions &v)
{
    w.Put(n, L::kPaEnabled, v.enabled);
    w.Put(n, L::kPaCurrentSet, v.currentSet);
    w.List(
        n, L::kPaList, v.list,
        [](const PlotAction &, const Struct *old) -> std::optional<std::string_view> {
            return old != nullptr ? old->Label() : std::string_view("PTAI");
        },
        [&w](const Node &c, const PlotAction &a) {
            w.Put(c, L::kPaId, a.id);
            w.Put(c, L::kPaState, a.state);
            w.Put(c, L::kPaCount, a.count);
            w.Put(c, L::kPaUpdated, a.updated);
        });
}

void Write(Writer &w, const Node &n, const Campaign &v)
{
    w.Put(n, L::kObjectId, v.id);
    w.Put(n, L::kCampaignResource, v.resource);
    List(w, n, L::kGroupList, v.groups, [&w](const Node &c, const Group &g) {
        w.Put(c, L::kGroupId, g.id);
        w.Numbers(c, L::kGroupHostiles, g.hostiles);
    });
    w.Put(n, L::kTimeBeforeDecay, v.creatureTimeBeforeDecay);
    w.PutPlain(n, L::kJournal, v.journal, [&w](const Node &c, const Journal &j) { Write(w, c, j); });
    List(w, n, L::kAmbientDialogs, v.ambientDialogs, [&w](const Node &c, const AmbientDialog &d) {
        w.Put(c, L::kAmbOwner, d.owner);
        w.Put(c, L::kAmbSpeaker, d.speaker);
        w.Put(c, L::kAmbResRef, d.resRef);
        w.Put(c, L::kAmbLine, d.line);
    });
    w.PutPlain(n, L::kWorldmap, v.worldMap, [&w](const Node &c, const WorldMap &m) { Write(w, c, m); });
    w.PutPlain(n, L::kPlotActions, v.plotActions, [&w](const Node &c, const PlotActions &p) { Write(w, c, p); });
    Variables(w, n, v.variables);
    w.Put(n, L::kEntryAreaList, v.entryAreaList);
    w.Put(n, L::kEntryArea, v.entryArea);
    w.Put(n, L::kEntryPosition, v.entryPosition);
    w.Put(n, L::kEntryOrientation, v.entryOrientation);
}

// -- The party and the game's state

void Write(Writer &w, const Node &n, const Party &v)
{
    w.Put(n, L::kObjectId, v.id);
    Items(w, n, L::kPoolMembers, v.pool);
    Items(w, n, L::kBackpack, v.backpack);
    Items(w, n, L::kPlotItems, v.plotItems);
    w.Put(n, L::kMoney, v.money);
    w.Put(n, L::kMaxItems, v.maxItems);
    w.Numbers(n, L::kCraftingRecipes, v.craftingRecipes);
    w.Put(n, L::kSelected, v.selectedCharacter);
    w.Put(n, L::kPickerGui, v.pickerGuiStatus);
    List(w, n, L::kApprovals, v.approvals, [&w](const Node &c, const Approval &a) {
        w.Put(c, L::kApprovalId, a.id);
        w.Put(c, L::kApprovalLevel, a.level);
        w.PutTlk(c, L::kApprovalDesc, a.descStrref, a.descText);
    });
    w.Put(n, L::kLeader, v.leader);
    List(w, n, L::kItemStorage, v.itemStorage, [&w](const Node &c, const StoredItem &s) {
        w.Put(c, L::kStItem, s.item);
        w.Put(c, L::kStOwner, s.owner);
        w.Put(c, L::kStSlot, s.slot);
        w.Put(c, L::kStWeaponset, s.weaponSet);
    });
    List(w, n, L::kNewItems, v.newItems, [&w](const Node &c, std::uint32_t id) { w.Put(c, L::kNewItemId, id); });
    w.Put(n, L::kAutoLevelDefault, v.autoLevelDefault);
    w.Put(n, L::kQuickbarLocked, v.quickbarLocked);
    w.Put(n, L::kHoldPositions, v.holdPositions);
    w.Put(n, L::kRunInDriveMode, v.runInDriveMode);
    w.Put(n, L::kBackpackSort, v.backpackSort);
    w.Put(n, L::kQbarExpansion, v.qbarExpansion);
    w.PutPlain(n, L::kPlotManager, v, [&w](const Node &c, const Party &p) {
        List(w, c, L::kPlotList, p.plots, [&w](const Node &e, const PlotEntry &plot) {
            w.Put(e, L::kPlotGuid, plot.guid);
            for (std::size_t i = 0; i < plot.flags.size(); ++i)
            {
                w.Put(e, L::kPlotFlags1 + static_cast<std::uint32_t>(i), plot.flags[i]);
            }
        });
        w.Strings(c, L::kUnreadCodex, p.unreadCodex);
    });
    w.Numbers(n, L::kSeenLines, v.seenLines);
}

// The template of a queued event, or nothing if a struct of the file's cannot stay one: the label Read kept,
// else ESCR for one with a script event, else EBAS. The engine goes by SavegameEventID and not the template
// (0x0079C0C0), and a plain event's loader reads the five base fields alone (0x0079BF60), so a plain event
// is an EBAS or an ESCR the same. A script event whose payload was taken out cannot stay the file's struct,
// which still holds the SCE1 (a child is never removed): it is written afresh, as an EBAS.
std::optional<std::string_view> EventLabel(const QueuedEvent &e, const Struct *old)
{
    if (old == nullptr)
    {
        return e.script ? "ESCR" : "EBAS";
    }
    const std::string_view fresh = e.script ? "ESCR" : "EBAS";
    const std::string_view want = e.label.empty() ? fresh : std::string_view(e.label);
    if (old->Label() != want || (!e.script && old->Child(L::kEventScript)))
    {
        return std::nullopt;
    }
    return want;
}

void Write(Writer &w, const Node &n, const ScriptEvent &v)
{
    w.Put(n, L::kSeType, v.type);
    w.Put(n, L::kSeCreator, v.creator);
    w.Put(n, L::kSeTarget, v.target);
    w.PutPlain(n, L::kSeData, v.data, [&w](const Node &c, const DataArrays &d) { Write(w, c, d); });
    w.Put(n, L::kSeScriptName, v.scriptName);
    w.Strings(n, L::kSeResources, v.resources);
}

void Write(Writer &w, const Node &n, const GameState &v)
{
    w.Put(n, L::kEffectStartingId, v.effectStartingId);
    w.PutPlain(n, L::kAiMaster, v.eventQueue, [&w](const Node &c, const std::vector<QueuedEvent> &events) {
        w.List(c, L::kEventQueue, events, EventLabel, [&w](const Node &e, const QueuedEvent &event) {
            w.Put(e, L::kEventDay, event.day);
            w.Put(e, L::kEventTime, event.time);
            w.Put(e, L::kEventCaller, event.caller);
            w.Put(e, L::kEventTarget, event.target);
            w.Put(e, L::kEventId, event.id);
            w.PutChild(e, L::kEventScript, event.script,
                       [&w](const Node &s, const ScriptEvent &script) { Write(w, s, script); });
        });
    });
    w.PutPlain(n, L::kWorldTimer, v, [&w](const Node &c, const GameState &s) {
        w.Put(c, L::kWorldDay, s.worldDay);
        w.Put(c, L::kWorldTime, s.worldTimeMs);
    });
}

void Write(Writer &w, const Node &top, const Save &v)
{
    w.PutPlain(top, L::kCampaign, v.campaign, [&w](const Node &c, const Campaign &x) { Write(w, c, x); });
    w.List(
        top, L::kAreaList, v.areas,
        [](const Area &, const Struct *old) -> std::optional<std::string_view> {
            return old != nullptr && old->Label() != "ARE1" ? std::nullopt : std::optional<std::string_view>("ARE1");
        },
        [&w](const Node &c, const Area &a) { Write(w, c, a); });
    w.PutPlain(top, L::kPlayerchar, v, [&w](const Node &c, const Save &s) {
        w.PutChild(c, L::kPlayerChar, s.hero, [&w](const Node &h, const Creature &hero) { Write(w, h, hero); });
        w.Put(c, L::kMapZoom, s.mapZoom);
        w.Put(c, L::kMapLegend, s.mapLegend);
        w.Put(c, L::kTimePlayed, s.timePlayedSeconds);
    });
    w.PutPlain(top, L::kPartyList, v.party, [&w](const Node &c, const Party &x) { Write(w, c, x); });
    w.PutPlain(top, L::kVersion, v.version, [&w](const Node &c, const Version &x) {
        w.Put(c, L::kCheatUsed, x.cheatUsed);
        w.Put(c, L::kBuildNumber, x.buildNumber);
        w.Put(c, L::kSaveVersion, x.saveVersion);
    });
    List(w, top, L::kWorldDatabase, v.idPools, [&w](const Node &c, const IdPool &p) {
        w.Put(c, L::kWdGroup, p.group);
        w.Put(c, L::kWdLastId, p.lastId);
    });
    w.PutPlain(top, L::kGameState, v.state, [&w](const Node &c, const GameState &x) { Write(w, c, x); });
}

} // namespace

std::optional<Written> Write(std::span<const std::byte> original, const Save &save)
{
    const auto file = gff4::File::Parse(original);
    if (!file || file->Type() != "SAVE")
    {
        return std::nullopt;
    }
    const auto root = file->Root();
    if (!root)
    {
        return std::nullopt;
    }
    Writer w(*file);
    Write(w, Node{*root, *root}, save);
    return Written{w.Result(), w.Dropped(), save.DuplicateIds()};
}

} // namespace dao::core::savegame
