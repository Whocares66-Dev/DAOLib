#include "dao/core/GfxMovie.h"

#include "dao/core/Inflate.h"

#include <algorithm>
#include <utility>

namespace dao::core::gfx
{

namespace
{

constexpr std::size_t kHeader = 8;                       // signature, version, length
constexpr std::size_t kMostInflated = 64U * 1024 * 1024; // no movie of the game's is near it

constexpr std::uint32_t kGfx = 'G' | 'F' << 8 | 'X' << 16;
constexpr std::uint32_t kCfx = 'C' | 'F' << 8 | 'X' << 16;
constexpr std::uint32_t kFws = 'F' | 'W' << 8 | 'S' << 16;
constexpr std::uint32_t kCws = 'C' | 'W' << 8 | 'S' << 16;

constexpr unsigned kEnd = 0;
constexpr unsigned kShowFrame = 1;
constexpr unsigned kDoAction = 12;
constexpr unsigned kPlaceObject2 = 26;
constexpr unsigned kDefineSprite = 39;
constexpr unsigned kExportAssets = 56;
constexpr unsigned kDoInitAction = 59;

constexpr std::uint8_t kCallFunction = 0x3D;
constexpr std::uint8_t kPop = 0x17;
constexpr std::uint8_t kConstantPool = 0x88;
constexpr std::uint8_t kDefineFunction2 = 0x8E;
constexpr std::uint8_t kTry = 0x8F;
constexpr std::uint8_t kWith = 0x94;
constexpr std::uint8_t kPush = 0x96;
constexpr std::uint8_t kJump = 0x99;
constexpr std::uint8_t kDefineFunction = 0x9B;
constexpr std::uint8_t kIf = 0x9D;

// The movie's bytes, read little-endian and never past the end: a read
// that would be says so in `good`.
struct Reader
{
    std::span<const std::byte> data;
    bool good = true;

    std::uint32_t Read(std::size_t at, std::size_t width)
    {
        if (at > data.size() || width > data.size() - at)
        {
            good = false;
            return 0;
        }
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < width; ++i)
        {
            value |= std::to_integer<std::uint32_t>(data[at + i]) << (8 * i);
        }
        return value;
    }

    std::uint8_t U8(std::size_t at)
    {
        return static_cast<std::uint8_t>(Read(at, 1));
    }
    std::uint16_t U16(std::size_t at)
    {
        return static_cast<std::uint16_t>(Read(at, 2));
    }
    std::uint32_t U32(std::size_t at)
    {
        return Read(at, 4);
    }

    // The text from `at` to its 0, which must come before `end`; `at` is
    // left after the 0.
    std::string_view Text(std::size_t &at, std::size_t end)
    {
        const std::size_t limit = std::min(end, data.size());
        for (std::size_t i = at; i < limit; ++i)
        {
            if (data[i] == std::byte{0})
            {
                const std::string_view text(reinterpret_cast<const char *>(data.data()) + at, i - at);
                at = i + 1;
                return text;
            }
        }
        good = false;
        return {};
    }
};

// Bit fields, most significant bit first, as a rectangle and a matrix are
// packed.
struct BitReader
{
    Reader &bytes;
    std::size_t at = 0;
    int bit = 0;

    std::uint32_t Unsigned(int count)
    {
        std::uint32_t value = 0;
        for (int i = 0; i < count; ++i)
        {
            const std::uint32_t byte = bytes.U8(at);
            value = value << 1U | ((byte >> (7 - bit)) & 1U);
            if (++bit == 8)
            {
                bit = 0;
                ++at;
            }
        }
        return value;
    }

    std::int32_t Signed(int count)
    {
        const std::uint32_t value = Unsigned(count);
        if (count > 0 && count < 32 && (value & (1U << (count - 1))) != 0)
        {
            return static_cast<std::int32_t>(value) - (std::int32_t{1} << count);
        }
        return static_cast<std::int32_t>(value);
    }

    // The next whole byte.
    std::size_t End() const
    {
        return bit == 0 ? at : at + 1;
    }
};

// Where the header ends and the tags begin: after the rectangle, whose
// first five bits say how many each of its four numbers has, and the rate
// and frame count.
std::size_t FirstTag(Reader &bytes)
{
    const std::size_t bits = bytes.U8(kHeader) >> 3U;
    return kHeader + (5 + 4 * bits + 7) / 8 + 4;
}

// An uncompressed movie's tags, or nothing for bytes that are no such
// movie.
std::optional<std::size_t> Tags(Reader &bytes)
{
    const std::uint32_t signature = bytes.Read(0, 3);
    const std::size_t first = FirstTag(bytes);
    if (!bytes.good || (signature != kGfx && signature != kFws) || first > bytes.data.size())
    {
        return std::nullopt;
    }
    return first;
}

struct Tag
{
    unsigned code = 0;
    std::size_t body = 0;
    std::size_t length = 0;
    std::size_t longLength = 0; // where its 32-bit length is, 0 for the short form
};

Tag ReadTag(Reader &bytes, std::size_t at)
{
    const std::uint16_t word = bytes.U16(at);
    Tag tag;
    tag.code = word >> 6U;
    tag.length = word & 0x3FU;
    tag.body = at + 2;
    if (tag.length == 0x3F)
    {
        tag.longLength = at + 2;
        tag.length = bytes.U32(at + 2);
        tag.body = at + 6;
    }
    return tag;
}

struct Action
{
    std::uint8_t code = 0;
    std::size_t body = 0; // its record's bytes
    std::size_t next = 0; // the action after it (for a function, its body)
};

Action ReadAction(Reader &bytes, std::size_t at)
{
    Action action;
    action.code = bytes.U8(at);
    action.body = at + 1;
    action.next = at + 1;
    if (action.code >= 0x80)
    {
        action.body = at + 3;
        action.next = action.body + bytes.U16(at + 1);
    }
    return action;
}

// A function's record: its name, its parameters (for 0x8E a register count
// and flags before them, and a register with each), then its body's size.
struct FunctionRecord
{
    std::size_t sizeAt = 0;
    std::size_t size = 0;
};

FunctionRecord ReadFunction(Reader &bytes, const Action &action)
{
    std::size_t at = action.body;
    bytes.Text(at, action.next);
    const std::size_t parameters = bytes.U16(at);
    at += 2;
    if (action.code == kDefineFunction2)
    {
        at += 3;
    }
    for (std::size_t i = 0; i < parameters && bytes.good; ++i)
    {
        if (action.code == kDefineFunction2)
        {
            at += 1;
        }
        bytes.Text(at, action.next);
    }
    FunctionRecord function;
    function.sizeAt = at;
    function.size = bytes.U16(at);
    if (at + 2 != action.next)
    {
        bytes.good = false;
    }
    return function;
}

// What a push holds that a statement is known by: its strings (written out
// or from the pool), its integers and its registers. Anything else is kept
// as a value that equals none.
struct Pushed
{
    enum class Kind
    {
        Text,
        Integer,
        Register,
        Other,
    };
    Kind kind = Kind::Other;
    std::string_view text;
    std::int32_t number = 0;
};

// The values of a push, or nothing for one that is not understood.
std::optional<std::vector<Pushed>> ReadPush(Reader &bytes, const Action &action,
                                            const std::vector<std::string_view> &pool)
{
    std::vector<Pushed> values;
    std::size_t at = action.body;
    const auto pooled = [&](std::size_t which) { return which < pool.size() ? pool[which] : std::string_view(); };
    while (at < action.next && bytes.good)
    {
        const std::uint8_t type = bytes.U8(at++);
        Pushed value;
        switch (type)
        {
        case 0: // a string
            value = {Pushed::Kind::Text, bytes.Text(at, action.next), 0};
            break;
        case 1: // a float
            at += 4;
            break;
        case 2: // null
        case 3: // undefined
            break;
        case 4: // a register
            value = {Pushed::Kind::Register, {}, bytes.U8(at)};
            at += 1;
            break;
        case 5: // a boolean
            at += 1;
            break;
        case 6: // a double
            at += 8;
            break;
        case 7: // an integer
            value = {Pushed::Kind::Integer, {}, static_cast<std::int32_t>(bytes.U32(at))};
            at += 4;
            break;
        case 8: // a constant, by a byte
            value = {Pushed::Kind::Text, pooled(bytes.U8(at)), 0};
            at += 1;
            break;
        case 9: // a constant, by a word
            value = {Pushed::Kind::Text, pooled(bytes.U16(at)), 0};
            at += 2;
            break;
        default:
            return std::nullopt;
        }
        values.push_back(value);
    }
    if (!bytes.good || at != action.next)
    {
        return std::nullopt;
    }
    return values;
}

bool IsText(const Pushed &value, std::string_view text)
{
    return value.kind == Pushed::Kind::Text && value.text == text;
}

// The statement a place is known by.
struct Anchor
{
    const Call *after = nullptr;       // straight after this call, or
    const MemberSet *before = nullptr; // straight before this member being set
};

// Where code goes for a push at `at` that the anchor names, if it does.
std::optional<std::size_t> PlaceAt(Reader &bytes, const Anchor &anchor, std::size_t at, const Action &action,
                                   std::size_t end, const std::vector<Pushed> &values)
{
    if (anchor.after != nullptr)
    {
        // The argument, the count 1, the function's name; then the call
        // and the answer dropped.
        if (values.size() == 3 && IsText(values[0], anchor.after->argument) &&
            values[1].kind == Pushed::Kind::Integer && values[1].number == 1 &&
            IsText(values[2], anchor.after->function) && action.next + 2 <= end &&
            bytes.U8(action.next) == kCallFunction && bytes.U8(action.next + 1) == kPop)
        {
            return action.next + 2;
        }
    }
    else if (anchor.before != nullptr)
    {
        if (values.size() >= 2 && values[0].kind == Pushed::Kind::Register && values[0].number == anchor.before->reg &&
            IsText(values[1], anchor.before->member))
        {
            return at;
        }
    }
    return std::nullopt;
}

// What is around the place: every field that says how long something
// holding it is, and the runs of actions it is in, outermost last.
struct Place
{
    std::size_t at = 0;
    std::vector<std::size_t> tagLengths;
    std::vector<std::size_t> functionSizes;
    std::vector<std::pair<std::size_t, std::size_t>> runs;
};

enum class Search
{
    Absent,
    Found,
    Bad,
};

Search InActions(Reader &bytes, std::size_t begin, std::size_t end, const std::vector<std::string_view> &outer,
                 const Anchor &anchor, Place &place)
{
    std::vector<std::string_view> own;
    const std::vector<std::string_view> *pool = &outer;
    std::size_t at = begin;
    while (at < end)
    {
        const Action action = ReadAction(bytes, at);
        if (!bytes.good || action.next > end)
        {
            return Search::Bad;
        }
        std::size_t next = action.next;
        if (action.code == kConstantPool)
        {
            std::size_t text = action.body + 2;
            own.clear();
            for (std::size_t i = bytes.U16(action.body); i > 0 && bytes.good; --i)
            {
                own.push_back(bytes.Text(text, action.next));
            }
            pool = &own;
        }
        else if (action.code == kPush)
        {
            if (const auto values = ReadPush(bytes, action, *pool))
            {
                if (const auto where = PlaceAt(bytes, anchor, at, action, end, *values))
                {
                    place.at = *where;
                    place.runs.emplace_back(begin, end);
                    return Search::Found;
                }
            }
        }
        else if (action.code == kDefineFunction || action.code == kDefineFunction2)
        {
            const FunctionRecord function = ReadFunction(bytes, action);
            if (!bytes.good || function.size > end - action.next)
            {
                return Search::Bad;
            }
            next = action.next + function.size;
            const Search inside = InActions(bytes, action.next, next, *pool, anchor, place);
            if (inside == Search::Found)
            {
                place.functionSizes.push_back(function.sizeAt);
                place.runs.emplace_back(begin, end);
            }
            if (inside != Search::Absent)
            {
                return inside;
            }
        }
        if (!bytes.good)
        {
            return Search::Bad;
        }
        at = next;
    }
    return Search::Absent;
}

Search InTags(Reader &bytes, std::size_t begin, std::size_t end, const Anchor &anchor, Place &place)
{
    std::size_t at = begin;
    while (at < end)
    {
        const Tag tag = ReadTag(bytes, at);
        if (!bytes.good || tag.body > end || tag.length > end - tag.body)
        {
            return Search::Bad;
        }
        if (tag.code == kEnd)
        {
            break;
        }
        const std::size_t tagEnd = tag.body + tag.length;
        Search inside = Search::Absent;
        if (tag.code == kDoAction)
        {
            inside = InActions(bytes, tag.body, tagEnd, {}, anchor, place);
        }
        else if (tag.code == kDoInitAction && tag.length >= 2)
        {
            inside = InActions(bytes, tag.body + 2, tagEnd, {}, anchor, place);
        }
        else if (tag.code == kDefineSprite && tag.length >= 4)
        {
            inside = InTags(bytes, tag.body + 4, tagEnd, anchor, place);
        }
        if (inside == Search::Found)
        {
            // A short tag has no room to grow in; the game's script tags
            // are all long.
            if (tag.longLength == 0)
            {
                return Search::Bad;
            }
            place.tagLengths.push_back(tag.longLength);
        }
        if (inside != Search::Absent)
        {
            return inside;
        }
        at = tagEnd;
    }
    return Search::Absent;
}

// A branch in a run that jumps across the place: where its offset is, and
// which way it has to change.
struct Branch
{
    std::size_t offsetAt = 0;
    bool forward = true;
};

// The branches across the place in one run, not looking into the functions
// in it (a branch stays in its own function). False for a `with` or a
// `try` around the place, whose sizes are not handled.
bool BranchesAcross(Reader &bytes, std::size_t begin, std::size_t end, std::size_t place, std::vector<Branch> &out)
{
    std::size_t at = begin;
    while (at < end)
    {
        const Action action = ReadAction(bytes, at);
        if (!bytes.good || action.next > end)
        {
            return false;
        }
        std::size_t next = action.next;
        if (action.code == kDefineFunction || action.code == kDefineFunction2)
        {
            next = action.next + ReadFunction(bytes, action).size;
        }
        else if (action.code == kJump || action.code == kIf)
        {
            const auto offset = static_cast<std::int16_t>(bytes.U16(action.body));
            const std::int64_t target = static_cast<std::int64_t>(action.next) + offset;
            const auto here = static_cast<std::int64_t>(place);
            if (static_cast<std::int64_t>(action.next) <= here && here < target)
            {
                out.push_back({action.body, true});
            }
            else if (target <= here && here < static_cast<std::int64_t>(action.next))
            {
                out.push_back({action.body, false});
            }
        }
        else if (action.code == kWith)
        {
            const std::size_t size = bytes.U16(action.body);
            if (action.next <= place && place < action.next + size)
            {
                return false;
            }
        }
        else if (action.code == kTry)
        {
            // Flags, then the sizes of the try, the catch and the finally.
            const std::size_t size =
                std::size_t{bytes.U16(action.body + 1)} + bytes.U16(action.body + 3) + bytes.U16(action.body + 5);
            if (action.next <= place && place < action.next + size)
            {
                return false;
            }
        }
        if (!bytes.good || next > end)
        {
            return false;
        }
        at = next;
    }
    return true;
}

void Write(Bytes &out, std::size_t at, std::uint32_t value, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
    {
        out[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
    }
}

void Append(Bytes &out, std::uint32_t value, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
    {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
    }
}

// Text and the 0 that ends it; a 0 in the text ends it early.
void AppendText(Bytes &out, std::string_view text)
{
    for (const char c : text)
    {
        if (c == '\0')
        {
            break;
        }
        out.push_back(static_cast<std::byte>(c));
    }
    out.push_back(std::byte{0});
}

std::optional<Bytes> Insert(std::span<const std::byte> movie, const Anchor &anchor, std::span<const std::byte> code)
{
    Reader bytes{movie};
    const auto first = Tags(bytes);
    if (!first)
    {
        return std::nullopt;
    }

    Place place;
    if (InTags(bytes, *first, movie.size(), anchor, place) != Search::Found)
    {
        return std::nullopt;
    }
    std::vector<Branch> branches;
    for (const auto &[begin, end] : place.runs)
    {
        if (!BranchesAcross(bytes, begin, end, place.at, branches))
        {
            return std::nullopt;
        }
    }

    // Every field is checked before any is written.
    const std::size_t added = code.size();
    for (const std::size_t at : place.functionSizes)
    {
        if (bytes.U16(at) + added > 0xFFFF)
        {
            return std::nullopt;
        }
    }
    for (const Branch &branch : branches)
    {
        const auto offset = static_cast<std::int16_t>(bytes.U16(branch.offsetAt));
        const std::int64_t moved =
            branch.forward ? offset + static_cast<std::int64_t>(added) : offset - static_cast<std::int64_t>(added);
        if (moved > INT16_MAX || moved < INT16_MIN)
        {
            return std::nullopt;
        }
    }
    if (added > UINT32_MAX - bytes.U32(4) || !bytes.good)
    {
        return std::nullopt;
    }

    Bytes out(movie.begin(), movie.end());
    out.insert(out.begin() + static_cast<std::ptrdiff_t>(place.at), code.begin(), code.end());
    // A field after the place has moved with what was put in.
    const auto moved = [&](std::size_t at) { return at >= place.at ? at + added : at; };
    Write(out, 4, bytes.U32(4) + static_cast<std::uint32_t>(added), 4);
    for (const std::size_t at : place.tagLengths)
    {
        Write(out, moved(at), bytes.U32(at) + static_cast<std::uint32_t>(added), 4);
    }
    for (const std::size_t at : place.functionSizes)
    {
        Write(out, moved(at), bytes.U16(at) + static_cast<std::uint32_t>(added), 2);
    }
    for (const Branch &branch : branches)
    {
        const auto offset = static_cast<std::int16_t>(bytes.U16(branch.offsetAt));
        const auto changed = static_cast<std::int16_t>(branch.forward ? offset + static_cast<std::int32_t>(added)
                                                                      : offset - static_cast<std::int32_t>(added));
        Write(out, moved(branch.offsetAt), static_cast<std::uint16_t>(changed), 2);
    }
    return out;
}

// What a PlaceObject2 says: flags, a depth, then by the flags a character,
// a matrix, a colour transform, a ratio and a name.
struct Placed
{
    std::uint16_t depth = 0;
    std::optional<std::uint16_t> character;
    std::int32_t x = 0; // in twips
    std::int32_t y = 0;
    std::string_view name;
};

Placed ReadPlaced(Reader &bytes, const Tag &tag)
{
    Placed placed;
    const std::uint8_t flags = bytes.U8(tag.body);
    placed.depth = bytes.U16(tag.body + 1);
    std::size_t at = tag.body + 3;
    if ((flags & 0x02U) != 0)
    {
        placed.character = bytes.U16(at);
        at += 2;
    }
    if ((flags & 0x04U) != 0)
    {
        BitReader bits{bytes, at};
        if (bits.Unsigned(1) != 0) // a scale
        {
            const int count = static_cast<int>(bits.Unsigned(5));
            bits.Signed(count);
            bits.Signed(count);
        }
        if (bits.Unsigned(1) != 0) // a rotation
        {
            const int count = static_cast<int>(bits.Unsigned(5));
            bits.Signed(count);
            bits.Signed(count);
        }
        const int count = static_cast<int>(bits.Unsigned(5));
        placed.x = bits.Signed(count);
        placed.y = bits.Signed(count);
        at = bits.End();
    }
    if ((flags & 0x08U) != 0)
    {
        BitReader bits{bytes, at};
        const bool adds = bits.Unsigned(1) != 0;
        const bool multiplies = bits.Unsigned(1) != 0;
        const int count = static_cast<int>(bits.Unsigned(4));
        for (int i = 0; i < (adds ? 4 : 0) + (multiplies ? 4 : 0); ++i)
        {
            bits.Signed(count);
        }
        at = bits.End();
    }
    if ((flags & 0x10U) != 0)
    {
        at += 2;
    }
    if ((flags & 0x20U) != 0)
    {
        placed.name = bytes.Text(at, tag.body + tag.length);
    }
    return placed;
}

// Bit fields written, most significant bit first.
struct BitWriter
{
    Bytes &out;
    int bit = 0;

    void Put(std::uint32_t value, int count)
    {
        for (int i = count - 1; i >= 0; --i)
        {
            if (bit == 0)
            {
                out.push_back(std::byte{0});
            }
            if (((value >> i) & 1U) != 0)
            {
                out.back() |= static_cast<std::byte>(0x80U >> bit);
            }
            bit = (bit + 1) % 8;
        }
    }
};

// How many bits a signed number needs.
int BitsFor(std::int32_t value)
{
    int count = 1;
    while (count < 31 && (value < -(std::int32_t{1} << (count - 1)) || value >= (std::int32_t{1} << (count - 1))))
    {
        ++count;
    }
    return count;
}

// A PlaceObject2 of a character at a depth and a place, with a name.
Bytes PlaceTag(const Placement &placement)
{
    Bytes body;
    body.push_back(std::byte{0x26}); // a name, a matrix, a character
    Append(body, placement.depth, 2);
    Append(body, placement.character, 2);
    const std::int32_t x = placement.x * 20; // twips
    const std::int32_t y = placement.y * 20;
    const int count = std::max(BitsFor(x), BitsFor(y));
    BitWriter bits{body};
    bits.Put(0, 1); // no scale
    bits.Put(0, 1); // no rotation
    bits.Put(static_cast<std::uint32_t>(count), 5);
    bits.Put(static_cast<std::uint32_t>(x), count);
    bits.Put(static_cast<std::uint32_t>(y), count);
    AppendText(body, placement.name);

    Bytes tag;
    if (body.size() < 0x3F)
    {
        Append(tag, kPlaceObject2 << 6U | static_cast<std::uint32_t>(body.size()), 2);
    }
    else
    {
        Append(tag, kPlaceObject2 << 6U | 0x3FU, 2);
        Append(tag, static_cast<std::uint32_t>(body.size()), 4);
    }
    tag.insert(tag.end(), body.begin(), body.end());
    return tag;
}

} // namespace

std::optional<Bytes> Unpack(std::span<const std::byte> file)
{
    Reader bytes{file};
    const std::uint32_t signature = bytes.Read(0, 3);
    const std::uint32_t length = bytes.U32(4);
    if (!bytes.good || length < kHeader)
    {
        return std::nullopt;
    }
    if (signature == kGfx || signature == kFws)
    {
        if (file.size() < length)
        {
            return std::nullopt;
        }
        return Bytes(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(length));
    }
    if (signature != kCfx && signature != kCws)
    {
        return std::nullopt;
    }
    // The game's own inflate to more than their header says, what is past
    // the End tag being of no use; so to the header's length, no further.
    auto inflated = InflateZlib(file.subspan(kHeader), kMostInflated);
    if (!inflated || inflated->size() < length - kHeader)
    {
        return std::nullopt;
    }
    Bytes movie(file.begin(), file.begin() + kHeader);
    movie[0] = static_cast<std::byte>(signature == kCfx ? 'G' : 'F');
    movie.insert(movie.end(), inflated->begin(), inflated->begin() + static_cast<std::ptrdiff_t>(length - kHeader));
    return movie;
}

Value Text(std::string_view text)
{
    return {Value::Kind::Text, std::string(text), 0};
}

Value Integer(std::int32_t number)
{
    return {Value::Kind::Integer, {}, number};
}

Value Register(std::int32_t number)
{
    return {Value::Kind::Register, {}, number};
}

Bytes Push(std::initializer_list<Value> values)
{
    Bytes body;
    for (const Value &value : values)
    {
        switch (value.kind)
        {
        case Value::Kind::Text:
            body.push_back(std::byte{0});
            AppendText(body, value.text);
            break;
        case Value::Kind::Integer:
            body.push_back(std::byte{7});
            Append(body, static_cast<std::uint32_t>(value.number), 4);
            break;
        case Value::Kind::Register:
            body.push_back(std::byte{4});
            body.push_back(static_cast<std::byte>(value.number));
            break;
        }
    }
    Bytes code{static_cast<std::byte>(kPush)};
    Append(code, static_cast<std::uint32_t>(body.size()), 2);
    code.insert(code.end(), body.begin(), body.end());
    return code;
}

Bytes Do(Op op)
{
    return {static_cast<std::byte>(op)};
}

Bytes Function(std::span<const std::byte> body)
{
    // No name, no parameters, two registers, and the flags the game's own
    // set-up functions have: `this` in register 1, no `arguments`, no
    // `super`.
    Bytes record;
    AppendText(record, "");
    Append(record, 0, 2);
    record.push_back(std::byte{2});
    Append(record, 0x0029, 2);
    Append(record, static_cast<std::uint32_t>(body.size()), 2);

    Bytes code{static_cast<std::byte>(kDefineFunction2)};
    Append(code, static_cast<std::uint32_t>(record.size()), 2);
    code.insert(code.end(), record.begin(), record.end());
    code.insert(code.end(), body.begin(), body.end());
    return code;
}

Bytes Join(std::initializer_list<Bytes> pieces)
{
    Bytes code;
    for (const Bytes &piece : pieces)
    {
        code.insert(code.end(), piece.begin(), piece.end());
    }
    return code;
}

Bytes Statements(std::span<const Call> calls)
{
    Bytes code;
    for (const Call &call : calls)
    {
        const Bytes statement =
            Join({Push({Text(call.argument), Integer(1), Text(call.function)}), Do(Op::CallFunction), Do(Op::Pop)});
        code.insert(code.end(), statement.begin(), statement.end());
    }
    return code;
}

std::optional<Bytes> InsertAfter(std::span<const std::byte> movie, const Call &after, std::span<const std::byte> code)
{
    Anchor anchor;
    anchor.after = &after;
    return Insert(movie, anchor, code);
}

std::optional<Bytes> InsertBefore(std::span<const std::byte> movie, const MemberSet &before,
                                  std::span<const std::byte> code)
{
    Anchor anchor;
    anchor.before = &before;
    return Insert(movie, anchor, code);
}

std::optional<Instance> FindInstance(std::span<const std::byte> movie, std::string_view name)
{
    Reader bytes{movie};
    const auto first = Tags(bytes);
    if (!first)
    {
        return std::nullopt;
    }
    std::size_t at = *first;
    while (at < movie.size())
    {
        const Tag tag = ReadTag(bytes, at);
        if (!bytes.good || tag.length > movie.size() - tag.body || tag.code == kEnd)
        {
            break;
        }
        if (tag.code == kDefineSprite && tag.length >= 4)
        {
            std::optional<Instance> found;
            std::uint16_t deepest = 0;
            std::size_t inner = tag.body + 4;
            const std::size_t end = tag.body + tag.length;
            while (inner < end)
            {
                const Tag placed = ReadTag(bytes, inner);
                if (!bytes.good || placed.length > end - placed.body || placed.code == kEnd)
                {
                    break;
                }
                if (placed.code == kPlaceObject2 && placed.length >= 3)
                {
                    const Placed what = ReadPlaced(bytes, placed);
                    deepest = std::max(deepest, what.depth);
                    if (!found && bytes.good && what.name == name && !name.empty() && what.character)
                    {
                        found = Instance{bytes.U16(tag.body), *what.character, what.depth, 0, what.x / 20, what.y / 20};
                    }
                    bytes.good = true; // a placement not understood is not this one
                }
                inner = placed.body + placed.length;
            }
            if (found)
            {
                found->deepest = deepest;
                return found;
            }
        }
        at = tag.body + tag.length;
    }
    return std::nullopt;
}

std::optional<std::uint16_t> ExportedCharacter(std::span<const std::byte> movie, std::string_view name)
{
    Reader bytes{movie};
    const auto first = Tags(bytes);
    if (!first)
    {
        return std::nullopt;
    }
    std::size_t at = *first;
    while (at < movie.size())
    {
        const Tag tag = ReadTag(bytes, at);
        if (!bytes.good || tag.length > movie.size() - tag.body || tag.code == kEnd)
        {
            break;
        }
        if (tag.code == kExportAssets)
        {
            const std::size_t end = tag.body + tag.length;
            std::size_t entry = tag.body + 2;
            for (std::size_t i = bytes.U16(tag.body); i > 0 && bytes.good && entry + 2 <= end; --i)
            {
                const std::uint16_t character = bytes.U16(entry);
                entry += 2;
                if (bytes.Text(entry, end) == name && bytes.good)
                {
                    return character;
                }
            }
            bytes.good = true;
        }
        at = tag.body + tag.length;
    }
    return std::nullopt;
}

std::optional<Bytes> PlaceInSprite(std::span<const std::byte> movie, std::uint16_t sprite,
                                   std::span<const Placement> placements)
{
    Reader bytes{movie};
    const auto first = Tags(bytes);
    if (!first)
    {
        return std::nullopt;
    }
    std::size_t at = *first;
    while (at < movie.size())
    {
        const Tag tag = ReadTag(bytes, at);
        if (!bytes.good || tag.length > movie.size() - tag.body || tag.code == kEnd)
        {
            break;
        }
        if (tag.code == kDefineSprite && tag.length >= 4 && bytes.U16(tag.body) == sprite)
        {
            // Before the sprite's first ShowFrame: on its first frame, over
            // what it places already.
            const std::size_t end = tag.body + tag.length;
            std::size_t inner = tag.body + 4;
            while (inner < end)
            {
                const Tag placed = ReadTag(bytes, inner);
                if (!bytes.good || placed.length > end - placed.body || placed.code == kEnd)
                {
                    return std::nullopt;
                }
                if (placed.code == kShowFrame)
                {
                    break;
                }
                inner = placed.body + placed.length;
            }
            if (inner >= end || tag.longLength == 0)
            {
                return std::nullopt;
            }
            Bytes tags;
            for (const Placement &placement : placements)
            {
                const Bytes one = PlaceTag(placement);
                tags.insert(tags.end(), one.begin(), one.end());
            }
            if (tags.size() > UINT32_MAX - bytes.U32(4))
            {
                return std::nullopt;
            }
            Bytes out(movie.begin(), movie.end());
            out.insert(out.begin() + static_cast<std::ptrdiff_t>(inner), tags.begin(), tags.end());
            Write(out, 4, bytes.U32(4) + static_cast<std::uint32_t>(tags.size()), 4);
            Write(out, tag.longLength, static_cast<std::uint32_t>(tag.length + tags.size()), 4);
            return out;
        }
        at = tag.body + tag.length;
    }
    return std::nullopt;
}

} // namespace dao::core::gfx
