#include "dao/core/GfxMovie.h"
#include "gfx_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using dao::core::gfx::AppendDoAction;
using dao::core::gfx::Call;
using dao::core::gfx::InsertAfter;
using dao::core::gfx::Statements;
using dao::core::gfx::Unpack;

namespace
{

using namespace dao::test::gfx; // NOLINT(google-build-using-namespace): the builder's words

// The controls screen's script in small: a pool, a branch over the whole
// definition, and inside two functions the list's calls.
struct Screen
{
    Bytes movie;
    std::size_t tagLength = 0;   // where the script tag's 32-bit length is
    std::size_t branch = 0;      // where the branch's offset is
    std::size_t outerSize = 0;   // where the outer function's size is
    std::size_t innerSize = 0;   // and the inner's
    std::size_t afterAnchor = 0; // where calls are put
    std::size_t loopBranch = 0;  // a branch back, after the list
};

Screen MakeScreen()
{
    const Bytes pool = Pool({"AddKeyBinding", "TogglePause", "HideMainInterface", "IgnoreKeyBinding", "AddHeader"});
    Bytes list;
    Add(list, Function("AddHeader", Simple(0x17)));
    Add(list, PooledCall(1, 0));
    const std::size_t listAnchor = list.size() + PooledCall(2, 0).size();
    Add(list, PooledCall(2, 0));
    Add(list, PooledCall(1, 3));
    // A loop at the list's end: back to its first call.
    const std::size_t listLoop = list.size() + 3;
    Add(list, Branch(0x9D, -static_cast<int>(list.size() + 5 - Function("AddHeader", Simple(0x17)).size())));

    const Bytes inner = Function2(list);
    Bytes outerBody = Simple(0x4C);
    const std::size_t innerAt = outerBody.size();
    Add(outerBody, inner);
    Add(outerBody, Simple(0x4F));
    const Bytes outer = Function2(outerBody);

    Bytes script;
    Put(script, 7, 2); // the sprite the script is for
    Add(script, pool);
    Add(script, Branch(0x9D, static_cast<int>(outer.size())));
    const std::size_t outerAt = script.size();
    Add(script, outer);
    Add(script, Simple(0x17));

    Bytes tags = ShortTag(9, {std::byte{1}, std::byte{2}, std::byte{3}});
    const std::size_t tagAt = tags.size();
    Add(tags, LongTag(59, script));

    Screen screen;
    screen.movie = Movie(tags);
    const std::size_t scriptAt = kFirstTag + tagAt + 6;
    screen.tagLength = kFirstTag + tagAt + 2;
    screen.branch = scriptAt + 2 + pool.size() + 3;
    // A function2's record with no name and no parameters: the code and its
    // length (3), the name's 0, the parameter count (2), a register count
    // and flags (3), then the size.
    constexpr std::size_t kSizeIn = 3 + 1 + 2 + 3;
    screen.outerSize = scriptAt + outerAt + kSizeIn;
    const std::size_t outerBodyAt = scriptAt + outerAt + kSizeIn + 2;
    screen.innerSize = outerBodyAt + innerAt + kSizeIn;
    const std::size_t listAt = outerBodyAt + innerAt + kSizeIn + 2;
    screen.afterAnchor = listAt + listAnchor;
    screen.loopBranch = listAt + listLoop;
    return screen;
}

const Call kAnchor{"AddKeyBinding", "HideMainInterface"};

} // namespace

TEST_CASE("statements are a push of the argument, 1 and the name, a call and a pop", "[gfx]")
{
    const std::vector<Call> calls = {{"AddHeader", "Mods"}, {"AddKeyBinding", "MyAction"}};
    const Bytes code = Statements(calls);

    Bytes expected;
    Bytes first{std::byte{0}};
    PutText(first, "Mods");
    first.push_back(std::byte{7});
    Put(first, 1, 4);
    first.push_back(std::byte{0});
    PutText(first, "AddHeader");
    Add(expected, Record(0x96, first));
    Add(expected, Simple(0x3D));
    Add(expected, Simple(0x17));
    Bytes second{std::byte{0}};
    PutText(second, "MyAction");
    second.push_back(std::byte{7});
    Put(second, 1, 4);
    second.push_back(std::byte{0});
    PutText(second, "AddKeyBinding");
    Add(expected, Record(0x96, second));
    Add(expected, Simple(0x3D));
    Add(expected, Simple(0x17));
    CHECK(code == expected);
    CHECK(Statements({}).empty());
}

TEST_CASE("calls go in after the anchor and every size around them grows", "[gfx]")
{
    const Screen screen = MakeScreen();
    const std::vector<Call> calls = {{"AddHeader", "Mods"}, {"AddKeyBinding", "MyAction"}};
    const Bytes code = Statements(calls);
    const auto added = static_cast<std::uint32_t>(code.size());

    const auto patched = InsertAfter(screen.movie, kAnchor, code);
    REQUIRE(patched);
    REQUIRE(patched->size() == screen.movie.size() + added);

    // The code is where it was asked for.
    const Bytes &before = screen.movie;
    CHECK(Bytes(patched->begin() + static_cast<std::ptrdiff_t>(screen.afterAnchor),
                patched->begin() + static_cast<std::ptrdiff_t>(screen.afterAnchor + added)) == code);

    CHECK(Get(*patched, 4, 4) == Get(before, 4, 4) + added);
    CHECK(Get(*patched, screen.tagLength, 4) == Get(before, screen.tagLength, 4) + added);
    CHECK(Get(*patched, screen.outerSize, 2) == Get(before, screen.outerSize, 2) + added);
    CHECK(Get(*patched, screen.innerSize, 2) == Get(before, screen.innerSize, 2) + added);
    // The branch over the definition jumps further; the loop's, back across
    // the new calls, further back.
    CHECK(Get(*patched, screen.branch, 2) == Get(before, screen.branch, 2) + added);
    const auto loopBefore = static_cast<std::int16_t>(Get(before, screen.loopBranch, 2));
    const auto loopAfter = static_cast<std::int16_t>(Get(*patched, screen.loopBranch + added, 2));
    CHECK(loopAfter == loopBefore - static_cast<int>(added));

    // Field by field: undo the six changes and take the code out, and the
    // movie is the one that went in.
    Bytes undone = *patched;
    const auto put = [&undone](std::size_t at, std::uint32_t value, std::size_t width) {
        for (std::size_t i = 0; i < width; ++i)
        {
            undone[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
        }
    };
    put(4, Get(before, 4, 4), 4);
    put(screen.tagLength, Get(before, screen.tagLength, 4), 4);
    put(screen.outerSize, Get(before, screen.outerSize, 2), 2);
    put(screen.innerSize, Get(before, screen.innerSize, 2), 2);
    put(screen.branch, Get(before, screen.branch, 2), 2);
    put(screen.loopBranch + added, Get(before, screen.loopBranch, 2), 2);
    undone.erase(undone.begin() + static_cast<std::ptrdiff_t>(screen.afterAnchor),
                 undone.begin() + static_cast<std::ptrdiff_t>(screen.afterAnchor + added));
    CHECK(undone == before);
}

TEST_CASE("a second insertion goes after the first's anchor again", "[gfx]")
{
    const Screen screen = MakeScreen();
    const std::vector<Call> one = {{"AddKeyBinding", "First"}};
    const std::vector<Call> two = {{"AddKeyBinding", "Second"}};
    const auto first = InsertAfter(screen.movie, kAnchor, Statements(one));
    REQUIRE(first);
    // What was put in can be an anchor itself: the patched movie still reads.
    const auto second = InsertAfter(*first, {"AddKeyBinding", "First"}, Statements(two));
    REQUIRE(second);
    const auto both = InsertAfter(screen.movie, kAnchor, Statements(std::vector<Call>{one[0], two[0]}));
    REQUIRE(both);
    CHECK(*second == *both);
}

TEST_CASE("a statement that is not in the movie leaves nothing to patch", "[gfx]")
{
    const Screen screen = MakeScreen();
    const Bytes code = Statements(std::vector<Call>{{"AddKeyBinding", "MyAction"}});
    CHECK_FALSE(InsertAfter(screen.movie, {"AddKeyBinding", "NoSuchAction"}, code));
    CHECK_FALSE(InsertAfter(screen.movie, {"NoSuchFunction", "HideMainInterface"}, code));
    // The same words the other way round are another statement.
    CHECK_FALSE(InsertAfter(screen.movie, {"HideMainInterface", "AddKeyBinding"}, code));
}

TEST_CASE("sizes that would not fit their fields refuse the patch", "[gfx]")
{
    const Screen screen = MakeScreen();
    // A function's size is 16 bits, and so is a branch's reach.
    const Bytes huge(0x10000, std::byte{0x17});
    CHECK_FALSE(InsertAfter(screen.movie, kAnchor, huge));
    const Bytes large(0x7F00, std::byte{0x17});
    CHECK(InsertAfter(screen.movie, kAnchor, large));
    const Bytes tooFarBack(0x8000, std::byte{0x17});
    CHECK_FALSE(InsertAfter(screen.movie, kAnchor, tooFarBack));
}

TEST_CASE("script in a short tag, or in a sprite, is handled or refused", "[gfx]")
{
    const Bytes pool = Pool({"AddKeyBinding", "HideMainInterface"});
    Bytes script = pool;
    Add(script, PooledCall(1, 0));
    const Bytes code = Statements(std::vector<Call>{{"AddKeyBinding", "MyAction"}});

    // A short DoAction has no room to grow in.
    CHECK_FALSE(InsertAfter(Movie(ShortTag(12, script)), kAnchor, code));

    // A long one, in a sprite: both tags' lengths grow.
    Bytes sprite;
    Put(sprite, 3, 2);
    Put(sprite, 1, 2);
    Add(sprite, LongTag(12, script));
    Put(sprite, 0, 2);
    const Bytes movie = Movie(LongTag(39, sprite));
    const auto patched = InsertAfter(movie, kAnchor, code);
    REQUIRE(patched);
    const auto added = static_cast<std::uint32_t>(code.size());
    CHECK(Get(*patched, kFirstTag + 2, 4) == Get(movie, kFirstTag + 2, 4) + added);
    CHECK(Get(*patched, kFirstTag + 6 + 4 + 2, 4) == Get(movie, kFirstTag + 6 + 4 + 2, 4) + added);
    CHECK(Get(*patched, 4, 4) == movie.size() + added);
}

TEST_CASE("bytes that are no movie, or one cut short, are refused", "[gfx]")
{
    const Screen screen = MakeScreen();
    const Bytes code = Statements(std::vector<Call>{{"AddKeyBinding", "MyAction"}});
    CHECK_FALSE(InsertAfter({}, kAnchor, code));
    Bytes wrong = screen.movie;
    wrong[0] = std::byte{'X'};
    CHECK_FALSE(InsertAfter(wrong, kAnchor, code));
    // Cut anywhere, it is refused or patched, and never read past its end.
    for (std::size_t length = 0; length < screen.movie.size(); ++length)
    {
        const auto patched = InsertAfter(std::span(screen.movie).first(length), kAnchor, code);
        CHECK((!patched || patched->size() == length + code.size()));
    }
}

TEST_CASE("an uncompressed movie unpacks to itself, to the length it gives", "[gfx]")
{
    const Screen screen = MakeScreen();
    const auto same = Unpack(screen.movie);
    REQUIRE(same);
    CHECK(*same == screen.movie);

    // What is past the header's length is dropped.
    Bytes padded = screen.movie;
    padded.insert(padded.end(), 10, std::byte{0xEE});
    const auto trimmed = Unpack(padded);
    REQUIRE(trimmed);
    CHECK(*trimmed == screen.movie);

    // Shorter than it says, it is no movie.
    CHECK_FALSE(Unpack(std::span(screen.movie).first(screen.movie.size() - 1)));
    CHECK_FALSE(Unpack({}));
    Bytes wrong = screen.movie;
    wrong[0] = std::byte{'Z'};
    CHECK_FALSE(Unpack(wrong));
}

TEST_CASE("a compressed movie unpacks to its header and what the stream inflates to", "[gfx]")
{
    // "CFX", version 8, the length 8 + 12, then zlib.compress(b"abcabcabcabc", 9).
    Bytes file;
    PutText(file, "CFX");
    file.back() = std::byte{8};
    Put(file, 20, 4);
    for (const int b : {0x78, 0xDA, 0x4B, 0x4C, 0x4A, 0x4E, 0x84, 0x21, 0x00, 0x1D, 0xE0, 0x04, 0x99})
    {
        file.push_back(static_cast<std::byte>(b));
    }
    const auto movie = Unpack(file);
    REQUIRE(movie);
    REQUIRE(movie->size() == 20);
    CHECK(std::string(reinterpret_cast<const char *>(movie->data()), 3) == "GFX");
    CHECK(std::to_integer<int>((*movie)[3]) == 8);
    CHECK(Get(*movie, 4, 4) == 20);
    CHECK(std::string(reinterpret_cast<const char *>(movie->data()) + 8, 12) == "abcabcabcabc");

    // The stream gives more than the header says: the rest is dropped, as
    // the game's own movies need.
    Bytes shorter = file;
    shorter[4] = std::byte{14};
    const auto cut = Unpack(shorter);
    REQUIRE(cut);
    CHECK(cut->size() == 14);

    // It gives less: no movie.
    Bytes longer = file;
    longer[4] = std::byte{21};
    CHECK_FALSE(Unpack(longer));

    // A Flash file's signature likewise.
    Bytes flash = file;
    flash[1] = std::byte{'W'};
    flash[2] = std::byte{'S'};
    const auto fws = Unpack(flash);
    REQUIRE(fws);
    CHECK(std::string(reinterpret_cast<const char *>(fws->data()), 3) == "FWS");
}

TEST_CASE("a DoAction of a plugin's own goes on the first frame, after the frame's script", "[gfx]")
{
    // The movie: its own script with a pool, a frame, a second frame.
    Bytes own = Pool({"Init", "a"});
    Add(own, PooledCall(1, 0));
    own.push_back(std::byte{0});
    Bytes tags = LongTag(12, own);
    Add(tags, ShortTag(1, {}));
    Add(tags, ShortTag(1, {}));
    const Bytes movie = Movie(tags);

    // The plugin's, compiled with a pool of its own and no End action.
    Bytes mine = Pool({"Mark", "b"});
    Add(mine, PooledCall(1, 0));
    const auto out = AppendDoAction(movie, mine);
    REQUIRE(out.has_value());

    // Its tag sits between the movie's script and the first ShowFrame, the
    // End action added, and the header's length has grown by it.
    const std::size_t ownEnd = kFirstTag + 6 + own.size();
    CHECK(Get(*out, ownEnd, 2) == (12U << 6U | 0x3FU));
    CHECK(Get(*out, ownEnd + 2, 4) == mine.size() + 1);
    CHECK(Bytes(out->begin() + static_cast<std::ptrdiff_t>(ownEnd + 6),
                out->begin() + static_cast<std::ptrdiff_t>(ownEnd + 6 + mine.size())) == mine);
    CHECK((*out)[ownEnd + 6 + mine.size()] == std::byte{0});
    const std::size_t showFrame = ownEnd + 6 + mine.size() + 1;
    CHECK(Get(*out, showFrame, 2) == (1U << 6U));
    CHECK(Get(*out, 4, 4) == out->size());
    CHECK(out->size() == movie.size() + 6 + mine.size() + 1);

    // A body that ends in End already gets no second.
    Bytes closed = mine;
    closed.push_back(std::byte{0});
    const auto again = AppendDoAction(movie, closed);
    REQUIRE(again.has_value());
    CHECK(again->size() == out->size());
}

TEST_CASE("a movie with no frame, or none at all, takes no DoAction", "[gfx]")
{
    CHECK_FALSE(AppendDoAction(Movie(LongTag(12, Bytes{std::byte{0}})), Bytes{std::byte{0}}).has_value());
    CHECK_FALSE(AppendDoAction(Bytes{}, Bytes{std::byte{0}}).has_value());
}
