// Notes on an install, read on the GOG install 2026-10-02 and 2026-10-12
// (Eluvian's dev/GHILANA.md, "The game on disk"): packages\core\data holds the
// core's archives (2da.erf, designercreatures.erf, modelmeshdata.erf at 425
// MiB), packages\core\patch its patches, packages\core\textures\high and
// medium the textures at each quality (so the core has a texture twice by
// design), modules\single player the campaign with nearly 10,000 .rim files,
// Addins\<uid> and Offers\<uid> the Ultimate Edition's add-ins (their 377
// .erf files ERF V2.2, enciphered), bin_ship\daolib\plugins\<name>\override
// DAOLib's plugins' files (DAOLib #43): the loader serves a plugin's
// override\ only for a plugin it finds, <name>\<name>.dll, and reads the
// folder DAOLIB_PLUGINS names in place of the game's, not as well as it
// (src/loader/Loader.cpp, #67, #143). Documents holds Settings\AddIns.xml
// and offers.xml, AddIns\ (add-ins a player installed) and
// packages\core\override. 10,612 archives, 539,436 copies of 235,278
// resources; the index of them read in 4 to 5 seconds on sixteen threads in a
// release build, a cache of the system's warm. Since DAOLib #144 (2026-10-16,
// the game running beside it): 2.0 to 2.3 s, and 1.1 to 1.4 s with the
// archives' directories kept (Options::cache), now that a path's place in its
// base is lexical and opens no file. An add-in's manifest.xml is left out,
// which describes it and is no resource.
//
// FMOD's audio (DAOLib #191, 2026-10-18): 9,052 banks (.fsb) and 9,020
// projects (.fev), every one a loose file in an audio\sound\ or
// audio\vo\<language>\ folder, so indexed as any loose file, at its folder's
// priority above (AUDIO_VO 100006, AUDIO_SOUND 100008, the campaign's 50006
// and 50007, a package's +7 and +8, an add-in's -2 and -1), another
// language's voice marked so. The game hands FMOD its own file system
// (System::setFileSystem, called at 0x005C59E4): the open callback
// (0x005C1070) asks the resource manager for the name (0x004C5950, which
// walks its sources in their order for the first that has it) and opens the
// file found through its alias (0x004CA780). So a bank is the resource
// manager's, in the order every other file is: read in the exe, not seen in
// play; that FMOD passes a bank's name alone, as a .fev names its banks, is
// inferred. They cost the index little: 18,072 copies more of 539,436, in
// folders the walk lists already for the files beside them; their paths in
// loose_ are about a megabyte. So no lazy form.
//
// Written for Eluvian's ghilana 2026-10-12 and moved here (DAOLib #106).
//
// The order (DAOLib #106, step 4; dev/FILES.md, where the evidence is): each
// copy's place is the folder the resource manager takes it from, and that
// folder's priority. The core's: the player's override 0, the install's 1,
// PATCH: 100000, LOCALE: 100001, ENV: 100002 (its own files; a level's
// env\<level>\ 100007, and only while the level is loaded), TEXTURES:
// 100003, DATA:'s RIMs 100004, DATA: 100005, AUDIO_VO: 100006,
// AUDIO_SOUND: 100008. A DAOLib plugin's override\ 2. The loaded campaign's
// (its CAMP_ aliases, 0x005FD051 on): override 50000, patch 50002, locale
// 50003, data 50004, toolset 50005, voice 50006, sound 50007, its own folder
// 50008. An add-in's: a base of 9000 - 7i for the i-th the game takes
// (AddInOrder.h), 10000 more in the game's folder than in Documents, and
// within it data and env at the base, sound -1, voice -2, textures -3,
// locale -4, patch -5, override -6, for its core\ and, while it extends the
// loaded campaign, its module\ (0x005EF460; the names pushed before each
// call). An offer's 200000 - 7i, all its folders.
//
// A campaign's packages (DAOLib #113, read 2026-10-13): loading a campaign
// (0x005FCE80) reads its .cif, the campaign's name and ".cif" by the resource
// manager, and for the i-th name of its CampaignCIFPackagesList (17124) adds
// that package's folders under PACKAGES:<name>\ at 70000 + 100i and more:
// override 0, textures' patch 1, patch 2, locale 3, env 4, textures 5, data 6,
// voice 7, sound 8 (0x00011173 and the offsets from it). So a package is
// behind the campaign's folders (50000) and ahead of the core's (100000).
// Awakening's lists core_ep1, and so do the add-ins that extend it; the main
// campaign's lists none (Eluvian's session read every .cif of the GOG
// install, 2026-10-13). Leaving a campaign takes them away again
// (0x005FC140).
#include "dao/core/Install.h"

#include "dao/core/AddInOrder.h"
#include "dao/core/Gff4.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <execution>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <thread>
#include <unordered_map>

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

// Every path here was found by walking under its base, so its place in it is lexical. Not
// fs::relative, which makes both canonical and so opens each file: for the GOG install's ten
// thousand archives that was half of Build (DAOLib #144).
std::string Relative(const fs::path &path, const fs::path &base)
{
    const fs::path relative = path.lexically_relative(base);
    return Utf8(relative.empty() ? path : relative);
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

// A base as the keys and ids name it.
std::string BaseName(Base base)
{
    switch (base)
    {
    case Base::Game:
        return "game";
    case Base::Documents:
        return "documents";
    case Base::Plugins:
        return "plugins";
    }
    return "game";
}

} // namespace
std::unique_ptr<Index> Index::Build(const fs::path &game, const fs::path &documents, const Options &options)
{
    const auto start = std::chrono::steady_clock::now();
    auto index = std::make_unique<Index>();
    index->game_ = game;
    index->documents_ = documents;
    index->plugins_ = options.plugins.empty() ? game / L"bin_ship" / L"daolib" / L"plugins" : options.plugins;
    index->options_ = options;
    index->options_.language = Lower(options.language); // as the folders' and files' names are compared
    index->options_.textures = Lower(options.textures);
    if (!documents.empty())
    {
        index->addIns_ = ReadAddIns(ReadText(documents / L"Settings" / L"AddIns.xml"), "AddInItem");
        index->offers_ = ReadAddIns(ReadText(documents / L"Settings" / L"offers.xml"), "OfferItem");
    }
    index->Walk();
    const KeptByPath kept = LoadKept(options.cache);
    auto reads = index->ReadDirectories(kept);
    index->KeepDirectories(reads, kept.size()); // before AddArchive takes the names
    for (std::size_t i = 0; i < index->pending_.size(); ++i)
    {
        if (!reads[i].header)
        {
            ++index->stats_.unopened;
            continue;
        }
        ++(reads[i].kept ? index->stats_.kept : index->stats_.read);
        index->AddArchive(index->pending_[i], reads[i]);
    }
    index->GatherResources();
    index->ReadPackages();
    index->Order();
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
        id += "(" + BaseName(base) + ")";
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
    // The walk is under the base, so a path's place in it is the path with the base's text taken
    // off: the same as lexically_relative, which was a third of the walk (DAOLib #207).
    std::string prefix = Utf8(base);
    if (!prefix.empty() && prefix.back() != '/')
    {
        prefix.push_back('/');
    }
    const auto placeIn = [&](const fs::path &path, const std::string &full) {
        return full.starts_with(prefix) ? full.substr(prefix.size()) : Relative(path, base);
    };
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
        const std::string full = Utf8(it->path());
        const std::string name = Lower(full.substr(full.rfind('/') + 1));
        const std::string extension = Extension(name);
        if (extension == "erf" || extension == "rim")
        {
            // The listing's size and time, which the walk has read already (MSVC's
            // directory_entry keeps them from the folder's listing).
            Pending pending{source, it->path(), placeIn(it->path(), full)};
            std::error_code sized;
            std::error_code timed;
            const auto size = it->file_size(sized);
            const auto written = it->last_write_time(timed);
            pending.stamped = !sized && !timed;
            pending.size = size;
            pending.written = written.time_since_epoch().count();
            pending_.push_back(std::move(pending));
            continue;
        }
        if (name == "manifest.xml")
        {
            continue;
        }
        std::error_code sizeError;
        const auto size = it->file_size(sizeError);
        loose_.push_back(placeIn(it->path(), full));
        gathered_.emplace_back(name, Copy{source, kLoose, static_cast<std::uint32_t>(loose_.size() - 1),
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
    for (const auto &[name, folder] : Folders(game_ / L"packages"))
    {
        if (Lower(name) != "core")
        {
            Collect(AddSource("package:" + Lower(name), "package", Base::Game, folder), folder, {});
        }
    }
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
    for (const auto &[name, folder] : Folders(plugins_))
    {
        const fs::path override = folder / L"override";
        std::error_code error;
        if (fs::is_regular_file(folder / (folder.filename().wstring() + L".dll"), error) &&
            fs::is_directory(override, error))
        {
            Collect(AddSource("plugin:" + Lower(name), "plugin", Base::Plugins, override), override, {});
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

Index::DirectoryRead Index::ReadDirectory(const Pending &pending, const KeptByPath &kept)
{
    DirectoryRead read;
    std::error_code placed;
    const fs::path absolute = fs::absolute(pending.path, placed);
    if (pending.stamped && !placed)
    {
        read.key = Utf8(absolute);
        read.size = pending.size;
        read.written = pending.written;
        const auto found = kept.find(read.key);
        if (found != kept.end() && found->second.size == read.size && found->second.written == read.written)
        {
            read.header = found->second.header;
            read.files = found->second.files;
            read.kept = true;
            return read;
        }
    }
    const auto archive = erf::Archive::Open(pending.path);
    if (!archive)
    {
        return read;
    }
    read.header = archive->Head();
    read.files.reserve(archive->Entries().size());
    for (const auto &entry : archive->Entries())
    {
        read.files.emplace_back(Lower(Utf8(fs::path(entry.name))), entry.size);
    }
    return read;
}

std::vector<Index::DirectoryRead> Index::ReadDirectories(const KeptByPath &kept) const
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
        readers.emplace_back([this, &reads, &next, &kept] {
            for (std::size_t i = next++; i < pending_.size(); i = next++)
            {
                reads[i] = ReadDirectory(pending_[i], kept);
            }
        });
    }
    readers.clear(); // each joined
    return reads;
}

void Index::KeepDirectories(const std::vector<DirectoryRead> &reads, std::size_t keptBefore) const
{
    if (options_.cache.empty())
    {
        return;
    }
    // Written again when an archive was read, or one kept has gone. Counted before any names are
    // copied, so that a cache nothing changed in costs nothing (DAOLib #207).
    std::size_t holding = 0;
    bool changed = false;
    for (const DirectoryRead &read : reads)
    {
        if (read.header && !read.key.empty())
        {
            ++holding;
            changed = changed || !read.kept;
        }
    }
    if (!changed && holding == keptBefore)
    {
        return;
    }
    std::vector<KeptDirectory> keep;
    keep.reserve(holding);
    for (const DirectoryRead &read : reads)
    {
        if (read.header && !read.key.empty())
        {
            keep.push_back({read.key, read.size, read.written, *read.header, read.files});
        }
    }
    SaveKept(options_.cache, keep);
}

void Index::AddArchive(const Pending &pending, DirectoryRead &read)
{
    Source &source = sources_[pending.source];
    ArchiveRecord record;
    record.source = pending.source;
    record.path = pending.relative;
    record.header = *read.header;
    record.files = read.files.size();
    const auto index = static_cast<std::uint32_t>(archives_.size());
    const bool readable = record.header.Encryption() == 0 && record.header.Compression() == 0;
    // An archive the index cannot read takes its copies' bytes from plain files where the program
    // has them (DAOLib #211): its folder under archiveCopies, listed once, matched by name in any
    // case as the game's names are.
    std::unordered_map<std::string, std::string> plain;
    if (!readable && !options_.archiveCopies.empty())
    {
        const std::string sourceFolder = Lower(source.folder);
        const std::string name = sourceFolder.substr(sourceFolder.find_last_of('/') + 1);
        const std::string inside =
            record.path.starts_with(source.folder + "/") ? record.path.substr(source.folder.size() + 1) : record.path;
        record.copies = options_.archiveCopies / fs::path(std::u8string(name.begin(), name.end())) /
                        fs::path(std::u8string(inside.begin(), inside.end()));
        std::error_code missing;
        for (const auto &file : fs::directory_iterator(record.copies, missing))
        {
            const std::u8string own = file.path().filename().u8string();
            plain.emplace(Lower(std::string(own.begin(), own.end())), std::string(own.begin(), own.end()));
        }
        if (plain.empty())
        {
            record.copies.clear();
        }
        else
        {
            record.copied.resize(read.files.size());
        }
    }
    archives_.push_back(std::move(record));
    ++source.archives;
    for (std::size_t e = 0; e < read.files.size(); ++e)
    {
        auto &[name, size] = read.files[e];
        if (name.empty())
        {
            continue;
        }
        bool covered = false;
        if (!plain.empty())
        {
            if (const auto found = plain.find(Lower(name)); found != plain.end())
            {
                archives_.back().copied[e] = found->second;
                covered = true;
            }
        }
        gathered_.emplace_back(std::move(name),
                               Copy{pending.source, index, static_cast<std::uint32_t>(e), std::min(size, kLargest)});
        ++source.files;
        source.unreadable += readable || covered ? 0 : 1;
        ++stats_.copies;
    }
}

// Into the map in one pass: sorted by name, each name's copies in the walk's order, and each name
// put in at the map's end. Half a million copies one at a time into the map took longer than
// reading every archive's directory (DAOLib #144). The sort is of views of the names with the
// copy's place in the walk, on every processor, and the names are moved into the map after
// (DAOLib #207).
void Index::GatherResources()
{
    struct Ref
    {
        std::string_view name;
        std::size_t walked;
    };
    std::vector<Ref> refs;
    refs.reserve(gathered_.size());
    for (std::size_t i = 0; i < gathered_.size(); ++i)
    {
        refs.push_back({gathered_[i].first, i});
    }
    std::sort(std::execution::par, refs.begin(), refs.end(), [](const Ref &a, const Ref &b) {
        const int order = a.name.compare(b.name);
        return order != 0 ? order < 0 : a.walked < b.walked;
    });
    for (std::size_t at = 0; at < refs.size();)
    {
        std::size_t end = at + 1;
        while (end < refs.size() && refs[end].name == refs[at].name)
        {
            ++end;
        }
        std::vector<Copy> copies;
        copies.reserve(end - at);
        for (std::size_t i = at; i < end; ++i)
        {
            copies.push_back(gathered_[refs[i].walked].second);
        }
        resources_.emplace_hint(resources_.end(), std::move(gathered_[refs[at].walked].first), std::move(copies));
        at = end;
    }
    gathered_.clear();
    gathered_.shrink_to_fit();
}

// The loaded campaign's .cif is its name with no spaces and ".cif" (singleplayer.cif,
// dao_prc_ep_1.cif); any copy of it that reads gives its packages.
void Index::ReadPackages()
{
    std::string name;
    for (const char c : Lower(options_.campaign))
    {
        if (c != ' ')
        {
            name.push_back(c);
        }
    }
    const auto copies = resources_.find(name + ".cif");
    if (name.empty() || copies == resources_.end())
    {
        return;
    }
    constexpr std::uint32_t kPackagesList = 17124; // CampaignCIFPackagesList
    for (const Copy &copy : copies->second)
    {
        const auto bytes = Readable(copy) ? Read(copy) : std::nullopt;
        const auto file = bytes ? gff4::File::Parse(*bytes) : std::nullopt;
        const auto root = file ? file->Root() : std::nullopt;
        if (!root)
        {
            continue;
        }
        for (const std::wstring &package : root->Strings(kPackagesList))
        {
            std::string text;
            for (const wchar_t c : package)
            {
                text.push_back(static_cast<char>(c < 0x80 ? c : '?'));
            }
            packages_.push_back(Lower(text));
        }
        return;
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

std::optional<fs::path> Index::FileOf(const Copy &copy) const
{
    if (copy.archive != kLoose)
    {
        return std::nullopt;
    }
    const std::string &relative = loose_[copy.entry];
    return Folder(sources_[copy.source].base) / fs::path(std::u8string(relative.begin(), relative.end()));
}

bool Index::Readable(const Copy &copy) const
{
    if (copy.archive == kLoose)
    {
        return true;
    }
    const ArchiveRecord &archive = archives_[copy.archive];
    const auto &header = archive.header;
    return (header.Encryption() == 0 && header.Compression() == 0) ||
           (copy.entry < archive.copied.size() && !archive.copied[copy.entry].empty());
}

// A table parsed again for each cell asked cost Paragon 1,850 parses at 0.8 ms in Ostagar's camp,
// and a worksheet merged at its first use 0.2 to 0.7 s; its shim kept both, and they are kept here
// for every caller (DAOLib #207, dev/PARAGON_LOAD_TIME.md items 3 and 5).
std::shared_ptr<const GdaFile> Index::Table(std::string_view name) const
{
    const std::string key = Lower(std::string(name));
    {
        const std::scoped_lock held(tablesLock_);
        if (const auto found = tables_.find(key); found != tables_.end())
        {
            return found->second;
        }
    }
    std::shared_ptr<const GdaFile> made;
    if (const auto *copies = Find(key); copies != nullptr && !copies->empty())
    {
        if (const auto bytes = Read(copies->front()))
        {
            if (auto table = GdaFile::Parse(*bytes))
            {
                made = std::make_shared<const GdaFile>(std::move(*table));
            }
        }
    }
    // Two threads may both parse one table; the first kept is the one every caller gets.
    const std::scoped_lock held(tablesLock_);
    return tables_.emplace(key, std::move(made)).first->second;
}

namespace
{

// A worksheet's files merged: each the game's copy of a .gda the worksheet takes, read in the order
// the game lists them.
m2da::Merged MergedSheet(const Index &index, const m2da::Worksheet &sheet)
{
    std::vector<NamedCopy> files;
    for (const auto &[name, copies] : index.Resources())
    {
        if (name.ends_with(".gda") && !copies.empty() && m2da::Takes(sheet, std::wstring(name.begin(), name.end() - 4)))
        {
            files.push_back({name, copies.front()});
        }
    }
    index.SortFiles(files);
    std::vector<GdaFile> tables;
    for (const NamedCopy &file : files)
    {
        const auto bytes = index.Read(file.copy);
        if (auto table = bytes ? GdaFile::Parse(*bytes) : std::nullopt)
        {
            tables.push_back(std::move(*table));
        }
    }
    std::vector<const GdaFile *> parts;
    parts.reserve(tables.size());
    for (const GdaFile &table : tables)
    {
        parts.push_back(&table);
    }
    return m2da::Merge(parts);
}

} // namespace

const m2da::Merged *Index::Worksheet(std::int64_t id) const
{
    const std::scoped_lock held(worksheetsLock_);
    if (const auto found = worksheets_.find(id); found != worksheets_.end())
    {
        return found->second.get();
    }
    std::unique_ptr<const m2da::Merged> made;
    if (const auto base = Table("m2da_base.gda"))
    {
        const std::vector<m2da::Worksheet> sheets = m2da::Worksheets(*base, true);
        if (const auto sheet = std::ranges::find(sheets, id, &m2da::Worksheet::id); sheet != sheets.end())
        {
            made = std::make_unique<const m2da::Merged>(MergedSheet(*this, *sheet));
        }
    }
    return worksheets_.emplace(id, std::move(made)).first->second.get();
}

bool Index::Whole(std::string_view uid) const
{
    const std::string id = "addin:" + Lower(std::string(uid));
    const auto found = std::ranges::find_if(sources_, [&](const Source &s) { return Lower(s.id) == id; });
    return found != sources_.end() && found->settings && found->settings->enabled.value_or(false) &&
           found->unreadable == 0;
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
    if (const ArchiveRecord &record = archives_[copy.archive];
        copy.entry < record.copied.size() && !record.copied[copy.entry].empty())
    {
        // A plain copy of an archive the index cannot read (DAOLib #211): the file whole.
        const std::string &file = record.copied[copy.entry];
        std::ifstream in(record.copies / fs::path(std::u8string(file.begin(), file.end())), std::ios::binary);
        std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        if (!in.eof() && in.fail())
        {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(chars.size());
        std::memcpy(bytes.data(), chars.data(), chars.size());
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

namespace
{

// Where the game's loader puts a folder: its priority, or why it takes none.
struct Place
{
    std::int32_t priority = 0;
    Unused unused = Unused::No;
};

constexpr std::int32_t kPlayersOverride = 0;
constexpr std::int32_t kGameOverride = 1;
constexpr std::int32_t kPluginFolder = 2;
constexpr std::int32_t kAddInBase = 9000;     // the first add-in the game takes, from Documents
constexpr std::int32_t kAddInStep = 7;        // less for each after it
constexpr std::int32_t kInGameFolder = 10000; // more for an add-in in the game's folder
constexpr std::int32_t kOfferBase = 200000;
constexpr std::int32_t kPackageBase = 70000; // the loaded campaign's first package
constexpr std::int32_t kPackageStep = 100;   // more for each after it

std::vector<std::string_view> Segments(std::string_view path)
{
    std::vector<std::string_view> out;
    while (!path.empty())
    {
        const auto slash = path.find('/');
        out.push_back(path.substr(0, slash));
        path = slash == std::string_view::npos ? std::string_view() : path.substr(slash + 1);
    }
    return out;
}

// A file of the core's, by its path in packages\core.
Place CorePlace(std::span<const std::string_view> path, const Options &options)
{
    if (path.size() < 2)
    {
        return {0, Unused::Folder};
    }
    const std::string_view top = path[0];
    if (top == "patch")
    {
        return {100000};
    }
    if (top == "locale")
    {
        return {100001, path.size() > 2 && path[1] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "env")
    {
        return path.size() == 2 ? Place{100002} : Place{100007, Unused::Level};
    }
    if (top == "textures")
    {
        return {100003, path.size() > 2 && path[1] == options.textures ? Unused::No : Unused::Textures};
    }
    if (top == "data")
    {
        // DATA:'s own RIMs come before its folders and ERFs (0x004031B8).
        return path.size() == 2 && Extension(std::string(path[1])) == "rim" ? Place{100004} : Place{100005};
    }
    if (top == "audio" && path[1] == "vo")
    {
        return {100006, path.size() > 3 && path[2] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "audio" && path[1] == "sound")
    {
        return {100008};
    }
    return {0, Unused::Folder};
}

// A file of the loaded campaign's, by its path in the campaign's folder.
Place CampaignPlace(std::span<const std::string_view> path, const Options &options)
{
    if (path.size() < 2)
    {
        return {50008};
    }
    const std::string_view top = path[0];
    if (top == "override")
    {
        return {50000};
    }
    if (top == "patch")
    {
        return {50002};
    }
    if (top == "locale")
    {
        return {50003, path.size() > 2 && path[1] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "data")
    {
        return {50004};
    }
    if (top == "toolset")
    {
        return {50005};
    }
    if (top == "audio" && path[1] == "vo")
    {
        return {50006, path.size() > 3 && path[2] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "audio" && path[1] == "sound")
    {
        return {50007};
    }
    if (top == "env")
    {
        return {50008};
    }
    return {50008, Unused::Folder};
}

// A file of a package's, by its path in packages\<name>, the package's base given.
Place PackagePlace(std::span<const std::string_view> path, std::int32_t base, const Options &options)
{
    if (path.size() < 2)
    {
        return {base, Unused::Folder};
    }
    const std::string_view top = path[0];
    if (top == "override")
    {
        return {base};
    }
    if (top == "patch")
    {
        return {base + 2};
    }
    if (top == "locale")
    {
        return {base + 3, path.size() > 2 && path[1] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "env")
    {
        return path.size() == 2 ? Place{base + 4} : Place{base + 4, Unused::Level};
    }
    if (top == "textures" && path.size() > 2)
    {
        if (path[1] == "patch_" + options.textures)
        {
            return {base + 1};
        }
        return {base + 5, path[1] == options.textures ? Unused::No : Unused::Textures};
    }
    if (top == "data")
    {
        return {base + 6};
    }
    if (top == "audio" && path[1] == "vo")
    {
        return {base + 7, path.size() > 3 && path[2] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "audio" && path[1] == "sound")
    {
        return {base + 8};
    }
    return {base, Unused::Folder};
}

// A file of an add-in's core\ or module\, by its path in that folder, the add-in's base given.
Place AddInPlace(std::span<const std::string_view> path, std::int32_t base, const Options &options)
{
    if (path.size() < 2)
    {
        return {base};
    }
    const std::string_view top = path[0];
    if (top == "data" || top == "env")
    {
        return {base};
    }
    if (top == "audio" && path[1] == "sound")
    {
        return {base - 1};
    }
    if (top == "audio" && path[1] == "vo")
    {
        return {base - 2, path.size() > 3 && path[2] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "textures")
    {
        return {base - 3, path.size() > 2 && path[1] == options.textures ? Unused::No : Unused::Textures};
    }
    if (top == "locale")
    {
        return {base - 4, path.size() > 2 && path[1] == options.language ? Unused::No : Unused::Language};
    }
    if (top == "patch")
    {
        return {base - 5};
    }
    if (top == "override")
    {
        return {base - 6};
    }
    return {base, Unused::Folder};
}

// The places of the items the game takes, by their UID in lower case.
std::map<std::string, std::int32_t, std::less<>> Slots(const std::vector<AddIn> &items, bool enabledOnly)
{
    std::map<std::string, std::int32_t, std::less<>> slots;
    const std::vector<std::size_t> order = AddInOrder(items, enabledOnly);
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        slots.emplace(Lower(items[order[i]].uid), static_cast<std::int32_t>(i));
    }
    return slots;
}

// A copy's place, and whether the order it rests on was seen.
struct Placed
{
    Place place;
    bool settled = true;
};

// Places each copy as the game's loader places its folder.
class Placer
{
  public:
    explicit Placer(const Index &index)
        : index_(index), addInSlots_(Slots(index.AddIns(), true)), offerSlots_(Slots(index.Offers(), false)),
          campaign_(Lower(index.Set().campaign))
    {
        std::map<std::string, bool, std::less<>> offerFolders;
        for (const Source &source : index.Sources())
        {
            const std::string uid = UidOf(source);
            if (source.kind == "addin" && (!addInFolders_.contains(uid) || source.base == Base::Documents))
            {
                addInFolders_[uid] = source.base;
            }
            else if (source.kind == "offer")
            {
                offerFolders[uid] = true;
            }
        }
        // What was not seen: whether a disabled add-in, or one with no folder, takes a place in
        // the game's count. Where neither is, the order is the one seen in play.
        addInsSettled_ = std::ranges::all_of(index.AddIns(), [this](const AddIn &item) {
            return item.enabled.value_or(false) && addInFolders_.contains(Lower(item.uid));
        });
        offersSettled_ = std::ranges::all_of(
            index.Offers(), [&offerFolders](const AddIn &item) { return offerFolders.contains(Lower(item.uid)); });
    }

    Placed Of(const Copy &copy) const
    {
        const Source &source = index_.Sources()[copy.source];
        std::string path = Lower(index_.Where(copy));
        const std::string folder = Lower(source.folder) + "/";
        if (path.starts_with(folder))
        {
            path.erase(0, folder.size());
        }
        const std::vector<std::string_view> parts = Segments(path);
        const Options &options = index_.Set();
        if (source.kind == "override")
        {
            return {{source.base == Base::Documents ? kPlayersOverride : kGameOverride}};
        }
        if (source.kind == "plugin")
        {
            return {{kPluginFolder}};
        }
        if (source.kind == "core")
        {
            return {CorePlace(parts, options)};
        }
        if (source.kind == "package")
        {
            const auto &loaded = index_.Packages();
            const auto slot = std::ranges::find(loaded, UidOf(source));
            if (slot == loaded.end())
            {
                Place found = PackagePlace(parts, kPackageBase, options);
                found.unused = Unused::Campaign;
                return {found};
            }
            const auto i = static_cast<std::int32_t>(slot - loaded.begin());
            return {PackagePlace(parts, kPackageBase + (kPackageStep * i), options)};
        }
        if (source.kind == "campaign" || source.kind == "module")
        {
            Place found = CampaignPlace(parts, options);
            if (found.unused == Unused::No && UidOf(source) != campaign_)
            {
                found.unused = Unused::Campaign;
            }
            return {found};
        }
        if (source.kind == "offer")
        {
            const auto slot = offerSlots_.find(UidOf(source));
            const Place found = slot == offerSlots_.end() ? Place{kOfferBase, Unused::Disabled}
                                                          : Place{kOfferBase - (kAddInStep * slot->second)};
            return {found, offersSettled_};
        }
        return {AddInCopy(source, parts), addInsSettled_};
    }

  private:
    static std::string UidOf(const Source &source)
    {
        return Lower(Utf8(fs::path(source.folder).filename()));
    }

    // A copy of an add-in's, by its path in the add-in's folder.
    Place AddInCopy(const Source &source, std::span<const std::string_view> parts) const
    {
        const std::string uid = UidOf(source);
        const auto slot = addInSlots_.find(uid);
        if (slot == addInSlots_.end())
        {
            return {kAddInBase, Unused::Disabled};
        }
        const bool inGame = source.base == Base::Game;
        const std::int32_t base = kAddInBase - (kAddInStep * slot->second) + (inGame ? kInGameFolder : 0);
        if (inGame && addInFolders_.at(uid) == Base::Documents)
        {
            return {base, Unused::Folder}; // the player's copy in Documents is taken instead
        }
        if (parts.size() < 2)
        {
            return {base};
        }
        const std::span<const std::string_view> rest = parts.subspan(1);
        const Options &options = index_.Set();
        if (parts[0] == "core")
        {
            return AddInPlace(rest, base, options);
        }
        if (parts[0] != "module")
        {
            return {base, Unused::Folder};
        }
        const std::string extends = source.settings ? Lower(source.settings->extends) : std::string();
        if (!campaign_.empty() && uid == campaign_ && extends.empty())
        {
            return CampaignPlace(rest, options); // the campaign is this add-in's own
        }
        Place found = AddInPlace(rest, base, options);
        if (found.unused == Unused::No && (campaign_.empty() || extends != campaign_))
        {
            found.unused = Unused::Campaign;
        }
        return found;
    }

    const Index &index_;
    std::map<std::string, std::int32_t, std::less<>> addInSlots_;
    std::map<std::string, std::int32_t, std::less<>> offerSlots_;
    std::map<std::string, Base, std::less<>> addInFolders_; // an add-in's folder, Documents' over the game's
    std::string campaign_;
    bool addInsSettled_ = true;
    bool offersSettled_ = true;
};

// Whether a file is a talk table of another language than the game's. The game asks for a talk
// table by a name it makes of the table's, the language's and ".tlk", or "_f.tlk" for the female
// player's (0x004E5862 to 0x004E5AFF, its literals "_", "_f" and ".tlk"): core_en-us.tlk,
// core_en-us_f.tlk. Every talk table of the GOG install is so named, ten languages of each
// (2026-10-13), so one of another language is never the game's.
bool IsOtherLanguage(std::string_view name, std::string_view language)
{
    constexpr std::string_view kTalkTable = ".tlk";
    constexpr std::string_view kFemale = "_f";
    if (!name.ends_with(kTalkTable))
    {
        return false;
    }
    name.remove_suffix(kTalkTable.size());
    if (name.ends_with(kFemale))
    {
        name.remove_suffix(kFemale.size());
    }
    const auto underscore = name.rfind('_');
    if (underscore == std::string_view::npos)
    {
        return false;
    }
    const std::string_view code = name.substr(underscore + 1);
    // A language's code: two letters, a dash, two letters (en-us), or the game's "pseudo".
    const bool isCode =
        code == "pseudo" || (code.size() == 5 && code[2] == '-' &&
                             std::ranges::all_of(code.substr(0, 2), [](char c) { return c >= 'a' && c <= 'z'; }) &&
                             std::ranges::all_of(code.substr(3), [](char c) { return c >= 'a' && c <= 'z'; }));
    return isCode && code != language;
}

// What the resource manager sorts a source by after its priority: its kind (its slot 8: a loose
// file's folder 0, a RIM 2, an ERF 3) and its name, the later first.
struct Key
{
    std::uint8_t kind;
    std::string name;
};

Key KeyOf(const Index &index, const Copy &copy)
{
    std::string where = Lower(index.Where(copy));
    if (copy.archive == kLoose)
    {
        const auto slash = where.rfind('/');
        where.resize(slash == std::string::npos ? 0 : slash);
        return {0, std::move(where)};
    }
    const std::uint8_t kind = Extension(where) == "rim" ? 2 : 3;
    return {kind, std::move(where)};
}

// A key that is another's name, so that a copy of an archive borrows the archive's.
struct KeyView
{
    std::uint8_t kind;
    std::string_view name;
};

// The places of copies in the order the game meets them: those it has first, then as the
// resource manager sorts its sources (priority, kind, name, the later first), then, of one
// source's files, by their names (`names`, where given): a source lists its own files by name,
// whether an archive or a folder (seen in play 2026-10-13, dev/FILES.md).
std::vector<std::size_t> GameOrder(std::span<const Copy> copies, std::span<const KeyView> keys,
                                   std::span<const std::string> names)
{
    std::vector<std::size_t> order(copies.size());
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::ranges::stable_sort(order, [&copies, &keys, &names](std::size_t a, std::size_t b) {
        const bool hasA = copies[a].unused == Unused::No;
        const bool hasB = copies[b].unused == Unused::No;
        if (hasA != hasB)
        {
            return hasA;
        }
        if (copies[a].priority != copies[b].priority)
        {
            return copies[a].priority < copies[b].priority;
        }
        if (keys[a].kind != keys[b].kind)
        {
            return keys[a].kind < keys[b].kind;
        }
        if (keys[a].name != keys[b].name)
        {
            return keys[a].name > keys[b].name;
        }
        return !names.empty() && names[a] < names[b];
    });
    return order;
}

std::vector<std::size_t> GameOrder(const Index &index, std::span<const Copy> copies, std::span<const std::string> names)
{
    std::vector<Key> owned;
    owned.reserve(copies.size());
    std::vector<KeyView> keys;
    keys.reserve(copies.size());
    for (const Copy &copy : copies)
    {
        owned.push_back(KeyOf(index, copy));
        keys.push_back({owned.back().kind, owned.back().name});
    }
    return GameOrder(copies, keys, names);
}

template <class T> void Permute(std::vector<T> &items, const std::vector<std::size_t> &order)
{
    std::vector<T> sorted;
    sorted.reserve(items.size());
    for (const std::size_t i : order)
    {
        sorted.push_back(std::move(items[i]));
    }
    items = std::move(sorted);
}

// A resource's copies sorted into the order the game meets them.
void Sort(const Index &index, std::vector<Copy> &copies)
{
    Permute(copies, GameOrder(index, copies, {}));
}

} // namespace

void Index::SortCopies(std::vector<Copy> &copies) const
{
    Sort(*this, copies);
}

void Index::SortFiles(std::vector<NamedCopy> &files) const
{
    std::vector<Copy> copies;
    std::vector<std::string> names;
    copies.reserve(files.size());
    names.reserve(files.size());
    for (const NamedCopy &file : files)
    {
        copies.push_back(file.copy);
        names.push_back(Lower(file.name));
    }
    Permute(files, GameOrder(*this, copies, names));
}

std::string Index::EngineSource(const Copy &copy) const
{
    // The base first: the game's and the player's override folders, and an add-in's folder in
    // the game's and in Documents, have the same path within their bases.
    return BaseName(sources_[copy.source].base) + ":" + KeyOf(*this, copy).name;
}

// A copy's place and key depend on the archive it is in, not on its entry: worked out once for
// each archive, and for each loose file by its own path (DAOLib #207).
void Index::Order()
{
    const Placer placer(*this);
    struct Whole
    {
        Placed placed;
        Key key;
    };
    std::vector<Whole> ofArchive;
    ofArchive.reserve(archives_.size());
    for (std::uint32_t a = 0; a < archives_.size(); ++a)
    {
        const Copy any{archives_[a].source, a, 0, 0};
        ofArchive.push_back({placer.Of(any), KeyOf(*this, any)});
    }
    std::vector<Key> looseKeys;
    std::vector<KeyView> keys;
    for (auto &[name, copies] : resources_)
    {
        const bool otherLanguage = IsOtherLanguage(name, options_.language);
        looseKeys.clear();
        keys.clear();
        for (const Copy &copy : copies)
        {
            if (copy.archive == kLoose)
            {
                looseKeys.push_back(KeyOf(*this, copy));
            }
        }
        std::size_t loose = 0;
        for (Copy &copy : copies)
        {
            const Placed placed = copy.archive == kLoose ? placer.Of(copy) : ofArchive[copy.archive].placed;
            copy.priority = placed.place.priority;
            copy.unused = placed.place.unused == Unused::No && otherLanguage ? Unused::Language : placed.place.unused;
            copy.settled = placed.settled;
            const Key &key = copy.archive == kLoose ? looseKeys[loose++] : ofArchive[copy.archive].key;
            keys.push_back({key.kind, key.name});
        }
        if (copies.size() > 1)
        {
            Permute(copies, GameOrder(copies, keys, {}));
        }
    }
}

} // namespace dao::core::install
