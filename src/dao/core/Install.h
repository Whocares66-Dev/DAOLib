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
//   package:<name>        <game>\packages\<name>, a package a campaign loads (core_ep1)
//   plugin:<name>         <plugins>\<name>\override, beside <name>.dll (DAOLib #43): <plugins>
//                         is <game>\bin_ship\daolib\plugins, or the folder Options names (#143)
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
#include "dao/core/ArchiveCache.h"
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

// The folder a source's path is relative to: the game's, Documents', or the plugins folder.
enum class Base
{
    Game,
    Documents,
    Plugins,
};

struct Source
{
    std::string id;   // "addin:dao_prc_cp_1"
    std::string kind; // "core", "override", "campaign", "module", "addin", "offer", "plugin", "package"
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
    Campaign, // another campaign's, an add-in's for another campaign, or a package the loaded
              // campaign does not load
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
    std::string textures = "high"; // a folder of packages\core\textures: "high" or "medium"
    // The plugins folder the game was started with (DAOLIB_PLUGINS), read in place of the game's
    // bin_ship\daolib\plugins; empty for the game's.
    std::filesystem::path plugins{};
    // A folder to keep the archives' directories in between builds (archives.bin): an archive
    // whose path, size and last-write time are as kept is not read again. Empty for none.
    std::filesystem::path cache{};
};

// A file by its name, and a copy of it.
struct NamedCopy
{
    std::string name;
    Copy copy;
};

struct Stats
{
    double ms = 0; // how long the index took to build
    std::size_t archives = 0;
    std::size_t copies = 0;
    std::size_t resources = 0;
    std::size_t unopened = 0; // .erf and .rim files that are no archive of version 2.0 or 2.2
    std::size_t read = 0;     // archives whose directories were read from them
    std::size_t kept = 0;     // archives whose directories came from Options::cache
};

class Index
{
  public:
    // Returns the index of an install: its sources walked, every archive's directory read on
    // several threads, AddIns.xml and offers.xml read from the Documents folder.
    // @param documents Documents\BioWare\Dragon Age, or an instance's; empty for none
    static std::unique_ptr<Index> Build(const std::filesystem::path &game, const std::filesystem::path &documents,
                                        const Options &options = {});

    // Returns a base's folder: for Base::Plugins, the plugins folder read.
    const std::filesystem::path &Folder(Base base) const
    {
        switch (base)
        {
        case Base::Documents:
            return documents_;
        case Base::Plugins:
            return plugins_;
        case Base::Game:
            break;
        }
        return game_;
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
    // Returns the packages the loaded campaign loads, in its order, in lower case: its .cif's
    // CampaignCIFPackagesList ("core_ep1" for Awakening); empty for the main campaign or none.
    const std::vector<std::string> &Packages() const
    {
        return packages_;
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
    // Sorts copies, of any resources, into the order the game looks in its sources: those it has
    // first, then by priority, kind (a folder, a RIM, an ERF) and name, the later first. What Find
    // gives one resource's copies in; the order of files of different names the game lists, as a
    // merged table takes its files in (dao/core/M2da.h).
    void SortCopies(std::vector<Copy> &copies) const;
    // Sorts files, each a name and the copy of it the game takes, into the order the game lists
    // its files: by their copies' sources as SortCopies, then, of one source's, by name. The order
    // a merged table takes its files in (dao/core/M2da.h).
    void SortFiles(std::vector<NamedCopy> &files) const;
    // Returns the game's source a copy is read from, as a key: its base ("game:", "documents:" or
    // "plugins:")
    // and its archive's path, or its own file's folder, in lower case.
    std::string EngineSource(const Copy &copy) const;

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
        // Its size and last-write time as the walk listed them, if they read.
        bool stamped = false;
        std::uint64_t size = 0;
        std::int64_t written = 0;
    };
    struct DirectoryRead
    {
        std::optional<erf::Header> header;
        std::vector<std::pair<std::string, std::uint32_t>> files; // name in lower case, size
        bool kept = false;                                        // from Options::cache
        // What the cache keys it by: its absolute path, size and last-write time, if they read.
        std::string key;
        std::uint64_t size = 0;
        std::int64_t written = 0;
    };

    void Walk();
    std::uint32_t AddSource(std::string id, std::string kind, Base base, const std::filesystem::path &folder);
    void Collect(std::uint32_t source, const std::filesystem::path &root, const std::filesystem::path &skip);
    static DirectoryRead ReadDirectory(const Pending &pending, const KeptByPath &kept);
    std::vector<DirectoryRead> ReadDirectories(const KeptByPath &kept) const;
    void KeepDirectories(const std::vector<DirectoryRead> &reads, std::size_t keptBefore) const;
    void AddArchive(const Pending &pending, DirectoryRead &read);
    void GatherResources();
    void ReadPackages();
    void Order();

    std::filesystem::path game_;
    std::filesystem::path documents_;
    std::filesystem::path plugins_;
    Options options_;
    Stats stats_;
    std::vector<AddIn> addIns_;
    std::vector<AddIn> offers_;
    std::vector<Source> sources_;
    std::vector<ArchiveRecord> archives_;
    std::vector<Pending> pending_;
    std::vector<std::string> loose_;
    std::map<std::string, std::vector<Copy>, std::less<>> resources_;
    // Every copy by its name in the walk's order, put into resources_ in one pass after.
    std::vector<std::pair<std::string, Copy>> gathered_;
    std::vector<std::string> packages_;

    // The archives last read from, held open.
    mutable std::mutex openLock_;
    mutable std::vector<std::pair<std::uint32_t, std::shared_ptr<erf::Archive>>> open_;
};

} // namespace dao::core::install
