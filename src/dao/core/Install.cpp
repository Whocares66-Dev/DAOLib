// Notes on an install, read on the GOG install 2026-10-02 and 2026-10-12
// (Eluvian's dev/GHILANA.md, "The game on disk"): packages\core\data holds the
// core's archives (2da.erf, designercreatures.erf, modelmeshdata.erf at 425
// MiB), packages\core\patch its patches, packages\core\textures\high and
// medium the textures at each quality (so the core has a texture twice by
// design), modules\single player the campaign with nearly 10,000 .rim files,
// Addins\<uid> and Offers\<uid> the Ultimate Edition's add-ins (their 377
// .erf files ERF V2.2, enciphered), bin_ship\daolib\plugins\<name>\override
// DAOLib's plugins' files (DAOLib #43). Documents holds Settings\AddIns.xml
// and offers.xml, AddIns\ (add-ins a player installed) and
// packages\core\override. 10,612 archives, 539,436 copies of 235,278
// resources; the index of them read in 4 to 5 seconds on sixteen threads in a
// release build, a cache of the system's warm. FMOD's audio (.fsb, .fev, some
// 18,000 files) is left out, and an add-in's manifest.xml, which describes it
// and is no resource.
//
// Written for Eluvian's ghilana 2026-10-12 and moved here (DAOLib #106).
#include "dao/core/Install.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace dao::core::install
{

namespace
{

namespace fs = std::filesystem;

// Archives read at once: the cold read of ten thousand directories is the
// disk's and the system's opening of files, which threads overlap.
constexpr unsigned kMostReaders = 16;
// Archives kept open to read from.
constexpr std::size_t kOpenKept = 8;
// No resource of the game's is near this; a size past it is a damaged directory's.
constexpr std::uint32_t kLargest = 1U << 30;

std::string Lower(std::string text)
{
    for (char &c : text)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

std::string Utf8(const fs::path &path)
{
    const std::u8string text = path.generic_u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}

std::string Relative(const fs::path &path, const fs::path &base)
{
    std::error_code error;
    const fs::path relative = fs::relative(path, base, error);
    return Utf8(error ? path : relative);
}

std::string Extension(const std::string &name)
{
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : name.substr(dot + 1);
}

std::string ReadText(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The folders in a folder, each its name and path, by name.
std::vector<std::pair<std::string, fs::path>> Folders(const fs::path &folder)
{
    std::vector<std::pair<std::string, fs::path>> out;
    std::error_code error;
    for (fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, error), end;
         !error && it != end; it.increment(error))
    {
        std::error_code type;
        if (it->is_directory(type))
        {
            out.emplace_back(Utf8(it->path().filename()), it->path());
        }
    }
    std::ranges::sort(out);
    return out;
}

} // namespace

std::unique_ptr<Index> Index::Build(const fs::path &game, const fs::path &documents)
{
    const auto start = std::chrono::steady_clock::now();
    auto index = std::make_unique<Index>();
    index->game_ = game;
    index->documents_ = documents;
    if (!documents.empty())
    {
        index->addIns_ = ReadAddIns(ReadText(documents / L"Settings" / L"AddIns.xml"), "AddInItem");
        index->offers_ = ReadAddIns(ReadText(documents / L"Settings" / L"offers.xml"), "OfferItem");
    }
    index->Walk();
    auto reads = index->ReadDirectories();
    for (std::size_t i = 0; i < index->pending_.size(); ++i)
    {
        if (!reads[i].header)
        {
            ++index->stats_.unopened;
            continue;
        }
        index->AddArchive(index->pending_[i], reads[i]);
    }
    // Each resource's copies in the sources' order: the walk added the loose
    // files as it met them and the archives' after.
    for (auto &[name, copies] : index->resources_)
    {
        std::ranges::stable_sort(copies, {}, &Copy::source);
    }
    index->pending_.clear();
    index->pending_.shrink_to_fit();
    index->stats_.archives = index->archives_.size();
    index->stats_.resources = index->resources_.size();
    index->stats_.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return index;
}

std::uint32_t Index::AddSource(std::string id, std::string kind, Base base, const fs::path &folder)
{
    if (std::ranges::any_of(sources_, [&id](const Source &s) { return s.id == id; }))
    {
        id += base == Base::Documents ? "(documents)" : "(game)";
    }
    Source source;
    source.id = std::move(id);
    source.kind = std::move(kind);
    source.base = base;
    source.folder = Relative(folder, Folder(base));
    if (source.kind == "addin" || source.kind == "offer")
    {
        const std::string uid = Lower(Utf8(folder.filename()));
        for (const auto &item : source.kind == "addin" ? addIns_ : offers_)
        {
            if (Lower(item.uid) == uid)
            {
                source.settings = item;
            }
        }
    }
    sources_.push_back(std::move(source));
    return static_cast<std::uint32_t>(sources_.size() - 1);
}

void Index::Collect(std::uint32_t source, const fs::path &root, const fs::path &skip)
{
    std::error_code error;
    if (!fs::is_directory(root, error))
    {
        return;
    }
    const fs::path &base = Folder(sources_[source].base);
    const std::string skipped = Lower(Utf8(skip));
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error), end;
         !error && it != end; it.increment(error))
    {
        std::error_code type;
        if (!skip.empty() && it->is_directory(type) && Lower(Utf8(it->path())) == skipped)
        {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(type))
        {
            continue;
        }
        const std::string name = Lower(Utf8(it->path().filename()));
        const std::string extension = Extension(name);
        if (extension == "erf" || extension == "rim")
        {
            pending_.push_back(Pending{source, it->path()});
            continue;
        }
        if (extension == "fsb" || extension == "fev" || name == "manifest.xml")
        {
            continue;
        }
        std::error_code sizeError;
        const auto size = it->file_size(sizeError);
        loose_.push_back(Relative(it->path(), base));
        resources_[name].push_back(Copy{source, kLoose, static_cast<std::uint32_t>(loose_.size() - 1),
                                        static_cast<std::uint32_t>(std::min<std::uintmax_t>(size, kLargest))});
        ++sources_[source].loose;
        ++sources_[source].files;
        ++stats_.copies;
    }
}

void Index::Walk()
{
    const fs::path core = game_ / L"packages" / L"core";
    const fs::path coreOverride = core / L"override";
    Collect(AddSource("core", "core", Base::Game, core), core, coreOverride);
    Collect(AddSource("override:game", "override", Base::Game, coreOverride), coreOverride, {});
    for (const auto &[name, folder] : Folders(game_ / L"modules"))
    {
        const bool campaign = Lower(name) == "single player";
        Collect(AddSource(campaign ? "campaign" : "module:" + Lower(name), campaign ? "campaign" : "module", Base::Game,
                          folder),
                folder, {});
    }
    for (const auto &[name, folder] : Folders(game_ / L"Addins"))
    {
        Collect(AddSource("addin:" + Lower(name), "addin", Base::Game, folder), folder, {});
    }
    for (const auto &[name, folder] : Folders(game_ / L"Offers"))
    {
        Collect(AddSource("offer:" + Lower(name), "offer", Base::Game, folder), folder, {});
    }
    for (const auto &[name, folder] : Folders(game_ / L"bin_ship" / L"daolib" / L"plugins"))
    {
        const fs::path override = folder / L"override";
        std::error_code error;
        if (fs::is_directory(override, error))
        {
            Collect(AddSource("plugin:" + Lower(name), "plugin", Base::Game, override), override, {});
        }
    }
    if (documents_.empty())
    {
        return;
    }
    for (const auto &[name, folder] : Folders(documents_ / L"AddIns"))
    {
        Collect(AddSource("addin:" + Lower(name), "addin", Base::Documents, folder), folder, {});
    }
    const fs::path documentsOverride = documents_ / L"packages" / L"core" / L"override";
    Collect(AddSource("override:documents", "override", Base::Documents, documentsOverride), documentsOverride, {});
}

std::vector<Index::DirectoryRead> Index::ReadDirectories() const
{
    // Each into a slot of its own, put in the index after in the walk's
    // order, so the index is the same whichever thread read what.
    std::vector<DirectoryRead> reads(pending_.size());
    std::atomic<std::size_t> next = 0;
    const unsigned threads = std::clamp(std::thread::hardware_concurrency(), 1U, kMostReaders);
    std::vector<std::jthread> readers;
    readers.reserve(threads);
    for (unsigned t = 0; t < threads; ++t)
    {
        readers.emplace_back([this, &reads, &next] {
            for (std::size_t i = next++; i < pending_.size(); i = next++)
            {
                const auto archive = erf::Archive::Open(pending_[i].path);
                if (!archive)
                {
                    continue;
                }
                reads[i].header = archive->Head();
                reads[i].files.reserve(archive->Entries().size());
                for (const auto &entry : archive->Entries())
                {
                    reads[i].files.emplace_back(Lower(Utf8(fs::path(entry.name))), entry.size);
                }
            }
        });
    }
    readers.clear(); // each joined
    return reads;
}

void Index::AddArchive(const Pending &pending, DirectoryRead &read)
{
    Source &source = sources_[pending.source];
    ArchiveRecord record;
    record.source = pending.source;
    record.path = Relative(pending.path, Folder(source.base));
    record.header = *read.header;
    record.files = read.files.size();
    const auto index = static_cast<std::uint32_t>(archives_.size());
    const bool readable = record.header.Encryption() == 0 && record.header.Compression() == 0;
    archives_.push_back(std::move(record));
    ++source.archives;
    for (std::size_t e = 0; e < read.files.size(); ++e)
    {
        auto &[name, size] = read.files[e];
        if (name.empty())
        {
            continue;
        }
        resources_[std::move(name)].push_back(
            Copy{pending.source, index, static_cast<std::uint32_t>(e), std::min(size, kLargest)});
        ++source.files;
        source.unreadable += readable ? 0 : 1;
        ++stats_.copies;
    }
}

const std::vector<Copy> *Index::Find(std::string_view name) const
{
    const auto it = resources_.find(Lower(std::string(name)));
    return it == resources_.end() ? nullptr : &it->second;
}

std::optional<std::uint32_t> Index::SourceOf(std::string_view id) const
{
    const std::string wanted = Lower(std::string(id));
    for (std::size_t i = 0; i < sources_.size(); ++i)
    {
        if (sources_[i].id == wanted)
        {
            return static_cast<std::uint32_t>(i);
        }
    }
    return std::nullopt;
}

const std::string &Index::Where(const Copy &copy) const
{
    return copy.archive == kLoose ? loose_[copy.entry] : archives_[copy.archive].path;
}

bool Index::Readable(const Copy &copy) const
{
    if (copy.archive == kLoose)
    {
        return true;
    }
    const auto &header = archives_[copy.archive].header;
    return header.Encryption() == 0 && header.Compression() == 0;
}

std::optional<std::vector<std::byte>> Index::Read(const Copy &copy) const
{
    if (!Readable(copy))
    {
        return std::nullopt;
    }
    const fs::path &base = Folder(sources_[copy.source].base);
    if (copy.archive == kLoose)
    {
        const std::string &relative = loose_[copy.entry];
        std::ifstream in(base / fs::path(std::u8string(relative.begin(), relative.end())), std::ios::binary);
        std::vector<std::byte> bytes(copy.size);
        if (!in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        {
            return std::nullopt;
        }
        return bytes;
    }
    std::shared_ptr<erf::Archive> archive;
    {
        const std::scoped_lock guard(openLock_);
        const auto found = std::ranges::find(open_, copy.archive, &decltype(open_)::value_type::first);
        if (found != open_.end())
        {
            archive = found->second;
            std::rotate(open_.begin(), found, found + 1); // the last used first
        }
        else
        {
            const std::string &relative = archives_[copy.archive].path;
            auto opened = erf::Archive::Open(base / fs::path(std::u8string(relative.begin(), relative.end())), true);
            if (!opened)
            {
                return std::nullopt;
            }
            archive = std::make_shared<erf::Archive>(std::move(*opened));
            open_.insert(open_.begin(), {copy.archive, archive});
            if (open_.size() > kOpenKept)
            {
                open_.pop_back();
            }
        }
    }
    if (copy.entry >= archive->Entries().size())
    {
        return std::nullopt;
    }
    return archive->Read(archive->Entries()[copy.entry]);
}

} // namespace dao::core::install
