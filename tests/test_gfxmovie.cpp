#include "dao/core/GfxMovie.h"
#include "gfx_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using dao::core::gfx::AppendDoAction;
using dao::core::gfx::Call;
using dao::core::gfx::FindInstance;
using dao::core::gfx::HastenFadeOut;
using dao::core::gfx::InsertAfter;
using dao::core::gfx::InstancesOf;
using dao::core::gfx::PlaceInSprite;
using dao::core::gfx::Placement;
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

namespace
{

// The codes of a run of tags, in order, to its End.
std::vector<unsigned> TagCodes(const Bytes &movie, std::size_t at)
{
    std::vector<unsigned> codes;
    while (at + 2 <= movie.size())
    {
        const std::uint32_t word = Get(movie, at, 2);
        const unsigned code = word >> 6U;
        std::size_t length = word & 0x3FU;
        at += 2;
        if (length == 0x3F)
        {
            length = Get(movie, at, 4);
            at += 4;
        }
        codes.push_back(code);
        if (code == 0)
        {
            break;
        }
        at += length;
    }
    return codes;
}

// A PlaceObject2 of a named character at depth 1 and (0, 0), as a sprite of
// the game's places its controls.
Bytes Placed(std::uint16_t character, std::string_view name)
{
    Bytes body{std::byte{0x26}};
    Put(body, 1, 2);
    Put(body, character, 2);
    Add(body, PackBits({{0, 1}, {0, 1}, {1, 5}, {0, 1}, {0, 1}}));
    PutText(body, name);
    return ShortTag(26, body);
}

// A sprite of two frames, its tag in the long form: what it places on its
// first frame, then the two ShowFrames.
Bytes TwoFrameSprite(std::uint16_t id, const Bytes &firstFrame)
{
    Bytes sprite;
    Put(sprite, id, 2);
    Put(sprite, 2, 2);
    Add(sprite, firstFrame);
    Add(sprite, ShortTag(1, {}));
    Add(sprite, ShortTag(1, {}));
    Put(sprite, 0, 2);
    return LongTag(39, sprite);
}

// The statement code the tests below put in.
Bytes MyAction()
{
    return Statements(std::vector<Call>{{"AddKeyBinding", "MyAction"}});
}

// The bytes of a movie from `at`, as long as `code`.
Bytes At(const Bytes &movie, std::size_t at, const Bytes &code)
{
    return {movie.begin() + static_cast<std::ptrdiff_t>(at),
            movie.begin() + static_cast<std::ptrdiff_t>(at + code.size())};
}

} // namespace

TEST_CASE("an anchor in a DefineFunction grows that function by the code put in", "[gfx]")
{
    // A function of the older kind (0x9B), named, with a parameter, as some
    // of the game's set-up functions are: code put in it must grow its
    // size, or the statements after it fall out of the function.
    Bytes body = PooledCall(1, 0);
    Add(body, Simple(0x17));
    const Bytes function = Function("SetupKeys", body);
    Bytes script = Pool({"AddKeyBinding", "HideMainInterface"});
    const std::size_t functionAt = script.size();
    Add(script, function);
    Add(script, Simple(0x17));
    const Bytes movie = Movie(LongTag(12, script));
    // The size is the record's last two bytes, straight before the body.
    const std::size_t sizeAt = kFirstTag + 6 + functionAt + function.size() - body.size() - 2;
    REQUIRE(Get(movie, sizeAt, 2) == body.size());

    const Bytes code = MyAction();
    const auto patched = InsertAfter(movie, kAnchor, code);
    REQUIRE(patched);
    CHECK(Get(*patched, sizeAt, 2) == body.size() + code.size());
}

TEST_CASE("a DefineFunction2's parameters are read past, in a register or not", "[gfx]")
{
    // The game's compiled functions are DefineFunction2s with parameters,
    // each a register then a name; a parameter kept in no register has 0
    // there, which must not be taken for its name's end.
    const Bytes body = PooledCall(1, 0);
    Bytes record;
    PutText(record, "SetupKeys");
    Put(record, 2, 2); // parameters
    record.push_back(std::byte{3});
    Put(record, 0, 2); // flags
    record.push_back(std::byte{1});
    PutText(record, "a_sName");
    record.push_back(std::byte{0});
    PutText(record, "a_nIndex");
    Put(record, static_cast<std::uint32_t>(body.size()), 2);
    Bytes function = Record(0x8E, record);
    Add(function, body);
    Bytes script = Pool({"AddKeyBinding", "HideMainInterface"});
    const std::size_t functionAt = script.size();
    Add(script, function);
    const Bytes movie = Movie(LongTag(12, script));
    const std::size_t sizeAt = kFirstTag + 6 + functionAt + function.size() - body.size() - 2;

    const Bytes code = MyAction();
    const auto patched = InsertAfter(movie, kAnchor, code);
    REQUIRE(patched);
    CHECK(Get(*patched, sizeAt, 2) == body.size() + code.size());
}

TEST_CASE("a statement whose strings are pushed by a 16-bit pool index is found", "[gfx]")
{
    // A pool past 256 strings pushes the later ones by a word (type 9); the
    // game's larger movies have such pools.
    constexpr std::uint32_t kStrings = 300;
    Bytes poolBody;
    Put(poolBody, kStrings, 2);
    for (std::uint32_t i = 0; i < kStrings; ++i)
    {
        std::string text = "s" + std::to_string(i);
        if (i == 260)
        {
            text = "AddKeyBinding";
        }
        else if (i == 261)
        {
            text = "HideMainInterface";
        }
        PutText(poolBody, text);
    }
    Bytes push;
    push.push_back(std::byte{9});
    Put(push, 261, 2);
    push.push_back(std::byte{7});
    Put(push, 1, 4);
    push.push_back(std::byte{9});
    Put(push, 260, 2);
    Bytes script = Record(0x88, poolBody);
    Add(script, Record(0x96, push));
    Add(script, Simple(0x3D));
    Add(script, Simple(0x17));
    const std::size_t afterCall = kFirstTag + 6 + script.size();
    Add(script, Simple(0x17));
    const Bytes movie = Movie(LongTag(12, script));

    const Bytes code = MyAction();
    const auto patched = InsertAfter(movie, kAnchor, code);
    REQUIRE(patched);
    CHECK(At(*patched, afterCall, code) == code);
}

TEST_CASE("a function that would grow past 64K refuses the patch", "[gfx]")
{
    // A function's size is 16 bits: with no branch across the place to
    // refuse it first, the size's own check must, or the size wraps and
    // the movie's script falls apart.
    Bytes body(0xFFE0, std::byte{0x17});
    Add(body, PooledCall(1, 0));
    Bytes script = Pool({"AddKeyBinding", "HideMainInterface"});
    Add(script, Function2(body));
    const Bytes movie = Movie(LongTag(12, script));
    CHECK_FALSE(InsertAfter(movie, kAnchor, Bytes(0x40, std::byte{0x17})));
    CHECK(InsertAfter(movie, kAnchor, Bytes(0x10, std::byte{0x17})));
}

TEST_CASE("a DoInitAction's sprite id is not read as script, whatever its bytes", "[gfx]")
{
    // A class's script is a DoInitAction for its sprite, whose id comes
    // first; an id with its low byte past 0x7F, as large movies have, looks
    // like the start of a record if read as an action.
    Bytes script;
    Put(script, 0x00C8, 2);
    Add(script, Pool({"AddKeyBinding", "HideMainInterface"}));
    Add(script, PooledCall(1, 0));
    const std::size_t afterCall = kFirstTag + 6 + script.size();
    const Bytes movie = Movie(LongTag(59, script));

    const Bytes code = MyAction();
    const auto patched = InsertAfter(movie, kAnchor, code);
    REQUIRE(patched);
    CHECK(At(*patched, afterCall, code) == code);
    CHECK(Get(*patched, kFirstTag + 2, 4) == script.size() + code.size());
}

TEST_CASE("placements go on a sprite's first frame, before its first ShowFrame", "[gfx]")
{
    // A tab's sprite stops on its first frame: a control placed on the
    // second would never show.
    const Bytes movie = Movie(TwoFrameSprite(5, Placed(3, "Existing_mc")));
    const std::vector<Placement> mine = {{"Mine_mc", 3, 2, 10, 20}};
    const auto patched = PlaceInSprite(movie, 5, mine);
    REQUIRE(patched);
    constexpr std::size_t kInner = kFirstTag + 6 + 4;
    CHECK(TagCodes(*patched, kInner) == std::vector<unsigned>{26, 26, 1, 1, 0});
    const auto placed = InstancesOf(*patched, 5);
    REQUIRE(placed.size() == 2);
    CHECK(placed[0].name == "Existing_mc");
    CHECK(placed[1].name == "Mine_mc");
}

TEST_CASE("a placement of 63 bytes or more is written in the long form", "[gfx]")
{
    // A tag's short form holds a length up to 62; 63 there means the long
    // form follows. A plugin's control with a long name reaches it: here the
    // body is 63 bytes, a matrix of 3 bytes and a name of 54 characters.
    const Bytes movie = Movie(TwoFrameSprite(5, {}));
    const std::string name = "DAOLibExample_Section_" + std::string(29, 'x') + "_mc";
    REQUIRE(name.size() == 54);
    const std::vector<Placement> mine = {{name, 3, 2, 3, 3}};
    const auto patched = PlaceInSprite(movie, 5, mine);
    REQUIRE(patched);
    constexpr std::size_t kInner = kFirstTag + 6 + 4;
    CHECK(Get(*patched, kInner, 2) == (26U << 6U | 0x3FU));
    CHECK(Get(*patched, kInner + 2, 4) == 63);
    const auto found = FindInstance(*patched, name);
    REQUIRE(found);
    CHECK(found->x == 3);
    CHECK(found->y == 3);
}

TEST_CASE("a placement's colour transform is stepped over to its name", "[gfx]")
{
    // A designer's clip at half alpha: a PlaceObject2 with a colour
    // transform of four multiply terms between its matrix and its name.
    Bytes body{std::byte{0x2E}};
    Put(body, 4, 2);
    Put(body, 9, 2);
    // No scale, no rotation, the place in 8 bits a number: (3, 4) pixels.
    Add(body, PackBits({{0, 1}, {0, 1}, {8, 5}, {60, 8}, {80, 8}}));
    // No add terms, multiply terms of 9 bits: red, green and blue whole,
    // alpha half.
    Add(body, PackBits({{0, 1}, {1, 1}, {9, 4}, {256, 9}, {256, 9}, {256, 9}, {128, 9}}));
    PutText(body, "Faded_mc");
    const Bytes movie = Movie(TwoFrameSprite(5, ShortTag(26, body)));
    const auto found = FindInstance(movie, "Faded_mc");
    REQUIRE(found);
    CHECK(found->character == 9);
    CHECK(found->x == 3);
    CHECK(found->y == 4);
}

namespace
{

// A PlaceObject2 of a character at a depth, whole: no colour transform.
Bytes Places(std::uint16_t depth, std::uint16_t character)
{
    Bytes body{std::byte{0x02}};
    Put(body, depth, 2);
    Put(body, character, 2);
    return ShortTag(26, body);
}

// A PlaceObject2 that sets the alpha of what a depth has: a move with a
// colour transform of four multiply terms, each of `bits` bits.
Bytes Fades(std::uint16_t depth, std::uint32_t alpha, int bits)
{
    Bytes body{std::byte{0x09}};
    Put(body, depth, 2);
    Add(body, PackBits({{0, 1},
                        {1, 1},
                        {static_cast<std::uint32_t>(bits), 4},
                        {256, bits},
                        {256, bits},
                        {256, bits},
                        {alpha, bits}}));
    return ShortTag(26, body);
}

Bytes Frame()
{
    return ShortTag(1, {});
}

Bytes Removes(std::uint16_t depth)
{
    Bytes body;
    Put(body, depth, 2);
    return ShortTag(28, body);
}

// A loading screen's timeline in small: a cover at depth 47 over a picture
// at depth 1, whole on the first frame and given each of `cover` on the
// frames after, while a caption at depth 3 comes up; then the cover taken
// away and the depth given to another thing at half alpha. The terms are
// 10 bits and 12 by turns, so they begin at different bits of their bytes.
Bytes Timeline(std::initializer_list<std::uint32_t> cover)
{
    Bytes tags = Places(1, 4);
    Add(tags, Places(47, 5));
    Add(tags, Places(3, 6));
    Add(tags, Frame());
    std::uint32_t caption = 0;
    int bits = 10;
    for (const std::uint32_t alpha : cover)
    {
        Add(tags, Fades(47, alpha, bits));
        Add(tags, Fades(3, caption += 20, bits));
        Add(tags, Frame());
        bits = bits == 10 ? 12 : 10;
    }
    Add(tags, Removes(47));
    Add(tags, Places(47, 7));
    Add(tags, Fades(47, 128, 10));
    Add(tags, Frame());
    return Movie(tags);
}

} // namespace

TEST_CASE("a fade out sets at each frame what it set so many times further on", "[gfx]")
{
    const Bytes movie = Timeline({224, 192, 160, 128, 96, 64, 32, 0});
    // Nine frames of the cover's life: the second takes the fourth's alpha,
    // the third the seventh's, and the rest are past the end. The caption,
    // the picture and what has the depth after the cover are as they were.
    const auto thrice = HastenFadeOut(movie, 47, 3);
    REQUIRE(thrice);
    CHECK(*thrice == Timeline({160, 64, 0, 0, 0, 0, 0, 0}));
    const auto twice = HastenFadeOut(movie, 47, 2);
    REQUIRE(twice);
    CHECK(*twice == Timeline({192, 128, 64, 0, 0, 0, 0, 0}));
    // So fast that every frame is past the end.
    const auto atOnce = HastenFadeOut(movie, 47, 1000);
    REQUIRE(atOnce);
    CHECK(*atOnce == Timeline({0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE("a fade that stops short of nothing ends where it ended", "[gfx]")
{
    const auto hastened = HastenFadeOut(Timeline({200, 150, 100, 100, 100}), 47, 2);
    REQUIRE(hastened);
    CHECK(*hastened == Timeline({150, 100, 100, 100, 100}));
}

TEST_CASE("frames that set no alpha, or set it whole, are left as they are", "[gfx]")
{
    // The cover held whole for two frames, the second by a colour transform
    // with no multiply terms, which has no alpha to write; then the fade.
    const auto timeline = [](std::uint32_t first, std::uint32_t second, std::uint32_t third) {
        Bytes whole{std::byte{0x09}};
        Put(whole, 47, 2);
        Add(whole, PackBits({{0, 1}, {0, 1}, {0, 4}}));
        Bytes tags = Places(47, 5);
        Add(tags, Frame());
        Add(tags, Frame());
        Add(tags, ShortTag(26, whole));
        Add(tags, Frame());
        for (const std::uint32_t alpha : {first, second, third})
        {
            Add(tags, Fades(47, alpha, 10));
            Add(tags, Frame());
        }
        return Movie(tags);
    };
    const auto hastened = HastenFadeOut(timeline(200, 100, 0), 47, 2);
    REQUIRE(hastened);
    CHECK(*hastened == timeline(0, 0, 0));
}

TEST_CASE("what is no fade out, or not understood, is not hastened", "[gfx]")
{
    const Bytes movie = Timeline({224, 192, 160, 128, 96, 64, 32, 0});
    CHECK_FALSE(HastenFadeOut(movie, 3, 3));  // the caption comes up
    CHECK_FALSE(HastenFadeOut(movie, 1, 3));  // the picture's alpha is never set
    CHECK_FALSE(HastenFadeOut(movie, 9, 3));  // nothing is there
    CHECK_FALSE(HastenFadeOut(movie, 47, 1)); // as fast as it was
    CHECK_FALSE(HastenFadeOut(movie, 47, 0));
    CHECK_FALSE(HastenFadeOut(Timeline({128, 0, 64, 0}), 47, 2));                 // down, and up again
    CHECK_FALSE(HastenFadeOut(Timeline({256, 256, 256}), 47, 2));                 // whole throughout
    CHECK_FALSE(HastenFadeOut(std::span(movie).first(movie.size() - 30), 47, 3)); // cut short in the cover's life

    // Placed by a PlaceObject3, whose flags are two bytes.
    Bytes body{std::byte{0x02}, std::byte{0}};
    Put(body, 47, 2);
    Put(body, 5, 2);
    Bytes tags = ShortTag(70, body);
    Add(tags, Frame());
    Add(tags, Fades(47, 128, 10));
    Add(tags, Frame());
    CHECK_FALSE(HastenFadeOut(Movie(tags), 47, 2));
}
