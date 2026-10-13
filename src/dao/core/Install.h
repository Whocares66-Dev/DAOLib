// A game install on disk: the sources the game takes resources from, every
// archive's directory, and every copy of every resource, read with no game.
// Paths in: which install and which Documents folder is the caller's to say
// (finding them reads the registry, which core does not).
//
//   const auto index = dao::core::install::Index::Build(game, documents);
//   if (const auto *copies = index->Find("abi_base.gda"))
//       const auto bytes = index->Read(copies->front());
//
// A source is named as answers name it:
//
//   core                  <game>\packages\core, but its override folder
//   override:game         <game>\packages\core\override
//   campaign              <game>\modules\single player
//   module:<name>         <game>\modules\<name>, another
//   addin:<uid>           <game>\Addins\<uid>, or <documents>\AddIns\<uid> (a player's)
//   offer:<uid>           <game>\Offers\<uid>
//   plugin:<name>         <game>\bin_ship\daolib\plugins\<name>\override (DAOLib #43)
//   override:documents    <documents>\packages\core\override
//
// Each resource's copies are in the order the game looks for it, the first
// the one it takes, as dev/FILES.md says the resource manager orders its
// sources (read in the exe and seen in play, DAOLib #106, step 4). Some
// depend on what the game is doing, which the caller says (Options): the
// campaign loaded, the language, the texture quality; a level's own files
// count only while the level is loaded, which a tool with no game cannot
// know, so they are marked and put after.
#pragma once

#include "dao/core/AddIns.h"
#include "dao/core/Erf.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dao::core::install
{

enum class Base
{
    Game,
    Documents,
};

struct Source
{
    std::string id;   // "addin:dao_prc_cp_1"
    std::string kind; // "core", "override", "campaign", "module", "addin", "offer", "plugin"
    Base base = Base::Game;
    std::string folder;            // relative to its base folder, UTF-8 with forward slashes
    std::optional<AddIn> settings; // an add-in's or offer's item in AddIns.xml or offers.xml
    std::size_t archives = 0;
    std::size_t loose = 0;
    std::size_t files = 0;      // copies, in its archives and loose
    std::size_t unreadable = 0; // of them, in enciphered or compressed archives
};

struct ArchiveRecord
{
    std::uint32_t source = 0;
    std::string path; // relative to its source's base folder, UTF-8 with forward slashes
    erf::Header header;
    std::size_t files = 0;
};

inline constexpr std::uint32_t kLoose = 0xFFFFFFFF;

// Why the game does not have a copy, while set as the index's Options say.
enum class Unused : std::uint8_t
{
    No,       // the game has it
    Campaign, // another campaign's, or an add-in's for another campaign
    Level,    // a level's own, which the game has only while the level is loaded
    Language, // another language's voice or text
    Textures, // another texture quality's
    Disabled, // an add-in AddIns.xml does not have enabled, or an offer offers.xml does not list
    Folder,   // in no folder the game takes files from (a toolset's, an add-in's copy in the game
              // folder when the player's in Documents has the same one)
};

// One copy of a resource.
struct Copy
{
    std::uint32_t source = 0;
    std::uint32_t archive = kLoose; // a place in Archives(), or kLoose for a file of its own
    std::uint32_t entry = 0;        // its entry in the archive, or a place among the loose files
    std::uint32_t size = 0;
    // The game's priority for its place: of the copies it has, it takes the one of the lowest, and
    // of equal ones a loose file before a RIM before an ERF, then the later name (dev/FILES.md).
    std::int32_t priority = 0;
    Unused unused = Unused::No;
    // False where the order rests on what was not seen: an AddIns.xml with an add-in disabled or
    // with no folder, which may or may not take a place in the game's count.
    bool settled = true;
};

// What the game is doing, which decides some of the places it takes files from.
struct Options
{
    std::string campaign = "Single Player"; // the loaded campaign, as AddIns.xml's ExtendedModuleUID names
                                            // one; empty for none (the main menu)
    std::string language = "en-us";
    std::string textures = "high"; // a folder of packages\core	extures: "high" or "medium"
};

struct Stats
{
    double ms = 0; // how long the index took to build
    std::size_t archives = 0;
    std::size_t copies = 0;
    std::size_t resources = 0;
    std::size_t unopened = 0; // .erf and .rim files that are no archive of version 2.0 or 2.2
};

class Index
{
  public:
    // Returns the index of an install: its sources walked, every archive's directory read on
    // several threads, AddIns.xml and offers.xml read from the Documents folder.
    // @param documents Documents\BioWare\Dragon Age, or an instance's; empty for none
    static std::unique_ptr<Index> Build(const std::filesystem::path &game, const std::filesystem::path &documents,
                                        const Options &options = {});

    const std::filesystem::path &Folder(Base base) const
    {
        return base == Base::Game ? game_ : documents_;
    }
    const Stats &Measured() const
    {
        return stats_;
    }
    // Returns the add-ins AddIns.xml lists, and the offers offers.xml lists, folder or none.
    const std::vector<AddIn> &AddIns() const
    {
        return addIns_;
    }
    const std::vector<AddIn> &Offers() const
    {
        return offers_;
    }
    const std::vector<Source> &Sources() const
    {
        return sources_;
    }
    const std::vector<ArchiveRecord> &Archives() const
    {
        return archives_;
    }
    // Returns what the game was taken to be doing when the index was built.
    const Options &Set() const
    {
        return options_;
    }
    // Returns every resource by its name in lower case (UTF-8), with its copies in the order the
    // game looks for it: those it has first, the one it takes at the front.
    const std::map<std::string, std::vector<Copy>, std::less<>> &Resources() const
    {
        return resources_;
    }
    // Returns a resource's copies, by its name in any case, in the game's order; null for none.
    const std::vector<Copy> *Find(std::string_view name) const;
    // Returns the place of the source of an id, in any case.
    std::optional<std::uint32_t> SourceOf(std::string_view id) const;
    // Returns where a copy is: its archive's path, or its own file's, relative to its base folder.
    const std::string &Where(const Copy &copy) const;

    // Returns true if a copy's bytes can be read: not in an enciphered or compressed archive.
    bool Readable(const Copy &copy) const;
    // Returns a copy's bytes.
    // @return nullopt if it is not Readable, or its file has gone or shrunk since the index. An
    // archive rewritten since may give another file's bytes: build the index again after the
    // install changes.
    std::optional<std::vector<std::byte>> Read(const Copy &copy) const;

  private:
    struct Pending
    {
        std::uint32_t source;
        std::filesystem::path path;
    };
    struct DirectoryRead
    {
        std::optional<erf::Header> header;
        std::vector<std::pair<std::string, std::uint32_t>> files; // name in lower case, size
    };

    void Walk();
    std::uint32_t AddSource(std::string id, std::string kind, Base base, const std::filesystem::path &folder);
    void Collect(std::uint32_t source, const std::filesystem::path &root, const std::filesystem::path &skip);
    std::vector<DirectoryRead> ReadDirectories() const;
    void AddArchive(const Pending &pending, DirectoryRead &read);
    void Order();

    std::filesystem::path game_;
    std::filesystem::path documents_;
    Options options_;
    Stats stats_;
    std::vector<AddIn> addIns_;
    std::vector<AddIn> offers_;
    std::vector<Source> sources_;
    std::vector<ArchiveRecord> archives_;
    std::vector<Pending> pending_;
    std::vector<std::string> loose_;
    std::map<std::string, std::vector<Copy>, std::less<>> resources_;

    // The archives last read from, held open.
    mutable std::mutex openLock_;
    mutable std::vector<std::pair<std::uint32_t, std::shared_ptr<erf::Archive>>> open_;
};

} // namespace dao::core::install
