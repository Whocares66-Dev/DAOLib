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
// Which copy of a resource the game takes is not said: that is the resource
// manager's in the exe, unread (DAOLib #106, step 4).
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

// One copy of a resource.
struct Copy
{
    std::uint32_t source = 0;
    std::uint32_t archive = kLoose; // a place in Archives(), or kLoose for a file of its own
    std::uint32_t entry = 0;        // its entry in the archive, or a place among the loose files
    std::uint32_t size = 0;
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
    static std::unique_ptr<Index> Build(const std::filesystem::path &game, const std::filesystem::path &documents);

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
    // Returns every resource by its name in lower case (UTF-8), with its copies in the order of
    // their sources.
    const std::map<std::string, std::vector<Copy>, std::less<>> &Resources() const
    {
        return resources_;
    }
    // Returns a resource's copies, by its name in any case; null for none.
    const std::vector<Copy> *Find(std::string_view name) const;
    // Returns the place of the source of an id, in any case.
    std::optional<std::uint32_t> SourceOf(std::string_view id) const;
    // Returns where a copy is: its archive's path, or its own file's, relative to its base folder.
    const std::string &Where(const Copy &copy) const;

    // Returns true if a copy's bytes can be read: not in an enciphered or compressed archive.
    bool Readable(const Copy &copy) const;
    // Returns a copy's bytes.
    // @return nullopt if it is not Readable, or its file has changed or gone since the index
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

    std::filesystem::path game_;
    std::filesystem::path documents_;
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
