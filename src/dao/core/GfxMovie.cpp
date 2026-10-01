#include "dao/core/GfxMovie.h"

#include "dao/core/Inflate.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace dao::core::gfx
{

namespace
{

constexpr std::size_t kHeader = 8;                       // signature, version, length
constexpr std::size_t kMostInflated = 64U * 1024 * 1024; // no movie of the game's is near it

constexpr unsigned kEnd = 0;
constexpr unsigned kDoAction = 12;
constexpr unsigned kDefineSprite = 39;
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
struct Bytes
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

// Where the header ends and the tags begin: after the rectangle, whose
// first five bits say how many each of its four numbers has, and the rate
// and frame count.
std::size_t FirstTag(Bytes &bytes)
{
    const std::size_t bits = bytes.U8(kHeader) >> 3U;
    return kHeader + (5 + 4 * bits + 7) / 8 + 4;
}

struct Tag
{
    unsigned code = 0;
    std::size_t body = 0;
    std::size_t length = 0;
    std::size_t longLength = 0; // where its 32-bit length is, 0 for the short form
};

Tag ReadTag(Bytes &bytes, std::size_t at)
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

Action ReadAction(Bytes &bytes, std::size_t at)
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
struct Function
{
    std::size_t sizeAt = 0;
    std::size_t size = 0;
};

Function ReadFunction(Bytes &bytes, const Action &action)
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
    Function function;
    function.sizeAt = at;
    function.size = bytes.U16(at);
    if (at + 2 != action.next)
    {
        bytes.good = false;
    }
    return function;
}

// Whether a push is the three values of `call`: its argument, the count 1,
// its function's name.
bool PushIs(Bytes &bytes, const Action &action, const std::vector<std::string_view> &pool, const Call &call)
{
    std::size_t at = action.body;
    int index = 0;
    const auto pooled = [&](std::size_t which) { return which < pool.size() ? pool[which] : std::string_view(); };
    while (at < action.next && bytes.good)
    {
        const std::uint8_t type = bytes.U8(at++);
        std::string_view text;
        bool isText = false;
        std::int64_t number = -1;
        switch (type)
        {
        case 0: // a string
            text = bytes.Text(at, action.next);
            isText = true;
            break;
        case 1: // a float
            at += 4;
            break;
        case 2: // null
        case 3: // undefined
            break;
        case 4: // a register
        case 5: // a boolean
            at += 1;
            break;
        case 6: // a double
            at += 8;
            break;
        case 7: // an integer
            number = static_cast<std::int32_t>(bytes.U32(at));
            at += 4;
            break;
        case 8: // a constant, by a byte
            text = pooled(bytes.U8(at));
            isText = true;
            at += 1;
            break;
        case 9: // a constant, by a word
            text = pooled(bytes.U16(at));
            isText = true;
            at += 2;
            break;
        default:
            return false;
        }
        const bool fits = index == 0   ? isText && text == call.argument
                          : index == 1 ? number == 1
                          : index == 2 ? isText && text == call.function
                                       : false;
        if (!fits)
        {
            return false;
        }
        ++index;
    }
    return bytes.good && index == 3 && at == action.next;
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

Search InActions(Bytes &bytes, std::size_t begin, std::size_t end, const std::vector<std::string_view> &outer,
                 const Call &after, Place &place)
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
            if (PushIs(bytes, action, *pool, after) && action.next + 2 <= end &&
                bytes.U8(action.next) == kCallFunction && bytes.U8(action.next + 1) == kPop)
            {
                place.at = action.next + 2;
                place.runs.emplace_back(begin, end);
                return Search::Found;
            }
        }
        else if (action.code == kDefineFunction || action.code == kDefineFunction2)
        {
            const Function function = ReadFunction(bytes, action);
            if (!bytes.good || function.size > end - action.next)
            {
                return Search::Bad;
            }
            next = action.next + function.size;
            const Search inside = InActions(bytes, action.next, next, *pool, after, place);
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

Search InTags(Bytes &bytes, std::size_t begin, std::size_t end, const Call &after, Place &place)
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
            inside = InActions(bytes, tag.body, tagEnd, {}, after, place);
        }
        else if (tag.code == kDoInitAction && tag.length >= 2)
        {
            inside = InActions(bytes, tag.body + 2, tagEnd, {}, after, place);
        }
        else if (tag.code == kDefineSprite && tag.length >= 4)
        {
            inside = InTags(bytes, tag.body + 4, tagEnd, after, place);
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
bool BranchesAcross(Bytes &bytes, std::size_t begin, std::size_t end, std::size_t place, std::vector<Branch> &out)
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

void Write(std::vector<std::byte> &out, std::size_t at, std::uint32_t value, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
    {
        out[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
    }
}

void Append(std::vector<std::byte> &out, std::uint32_t value, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
    {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
    }
}

void AppendText(std::vector<std::byte> &out, std::string_view text)
{
    out.push_back(std::byte{0}); // a push's string
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

} // namespace

std::optional<std::vector<std::byte>> Unpack(std::span<const std::byte> file)
{
    Bytes bytes{file};
    const std::uint32_t signature = bytes.Read(0, 3);
    const std::uint32_t length = bytes.U32(4);
    constexpr std::uint32_t kGfx = 'G' | 'F' << 8 | 'X' << 16;
    constexpr std::uint32_t kCfx = 'C' | 'F' << 8 | 'X' << 16;
    constexpr std::uint32_t kFws = 'F' | 'W' << 8 | 'S' << 16;
    constexpr std::uint32_t kCws = 'C' | 'W' << 8 | 'S' << 16;
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
        return std::vector<std::byte>(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(length));
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
    std::vector<std::byte> movie(file.begin(), file.begin() + kHeader);
    movie[0] = static_cast<std::byte>(signature == kCfx ? 'G' : 'F');
    movie.insert(movie.end(), inflated->begin(), inflated->begin() + static_cast<std::ptrdiff_t>(length - kHeader));
    return movie;
}

std::vector<std::byte> Statements(std::span<const Call> calls)
{
    std::vector<std::byte> code;
    for (const Call &call : calls)
    {
        std::vector<std::byte> values;
        AppendText(values, call.argument);
        values.push_back(std::byte{7}); // an integer: the count of arguments
        Append(values, 1, 4);
        AppendText(values, call.function);

        code.push_back(static_cast<std::byte>(kPush));
        Append(code, static_cast<std::uint32_t>(values.size()), 2);
        code.insert(code.end(), values.begin(), values.end());
        code.push_back(static_cast<std::byte>(kCallFunction));
        code.push_back(static_cast<std::byte>(kPop));
    }
    return code;
}

std::optional<std::vector<std::byte>> InsertAfter(std::span<const std::byte> movie, const Call &after,
                                                  std::span<const std::byte> code)
{
    Bytes bytes{movie};
    const std::uint32_t signature = bytes.Read(0, 3);
    constexpr std::uint32_t kGfx = 'G' | 'F' << 8 | 'X' << 16;
    constexpr std::uint32_t kFws = 'F' | 'W' << 8 | 'S' << 16;
    const std::size_t first = FirstTag(bytes);
    if (!bytes.good || (signature != kGfx && signature != kFws) || first > movie.size())
    {
        return std::nullopt;
    }

    Place place;
    if (InTags(bytes, first, movie.size(), after, place) != Search::Found)
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

    std::vector<std::byte> out(movie.begin(), movie.end());
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

} // namespace dao::core::gfx
