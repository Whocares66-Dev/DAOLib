#include "dao/Module.h"

#include "dao/core/Pe.h"

#include <Windows.h>

#include <array>
#include <span>
#include <utility>

namespace dao::Module
{

namespace
{

// The GOG build: the one the Ghidra project, the names and every address
// written in a comment are of (dao/Addresses.h). By all four fields, since
// Steam's build has its link stamp and differs in the rest.
constexpr daoaddr::BuildInfo kListing{
    .linkStamp = 0x4eb19d53, .sizeOfImage = 0x009ec0a2, .entryPointRva = 0x004e3024, .imageBase = 0x00400000};

// How far past an ID's address is still taken to be the same thing, going
// from a running address back to the listing's: no function of the game's is
// this long, and past it the nearest ID below says nothing.
constexpr std::uint32_t kFarthest = 0x10000;

// The folder of the DLL this code is linked into: the loader's, or a
// plugin's.
std::filesystem::path OwnFolder()
{
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&OwnFolder), &module) == FALSE)
    {
        return {};
    }
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring_view(buffer.data(), length)).parent_path();
}

struct Libraries
{
    std::filesystem::path folder;
    std::optional<daoaddr::Library> running; // the build this process is
    std::optional<daoaddr::Library> listing; // GOG's, if that is another
};

// Read once, by each DLL for itself: the files do not change under a
// running game, and Address asks every time.
const Libraries &Loaded()
{
    static const Libraries libraries = [] {
        Libraries out;
        const auto want = Build();
        if (!want)
        {
            return out;
        }
        const std::string name = daoaddr::FileName(*want);
        const auto own = OwnFolder();
        // Beside the loader: for the loader itself, for a plugin in
        // daolib\plugins, and for either when the loader sits beside the exe.
        for (const auto &folder : {own / L"daolib" / L"addresses", own.parent_path() / L"addresses",
                                   Path().parent_path() / L"daolib" / L"addresses"})
        {
            auto library = daoaddr::Library::Load(folder / name);
            if (!library)
            {
                continue;
            }
            // The name says the build; the header inside must say the same.
            if (library->Build() != *want)
            {
                continue;
            }
            out.folder = folder;
            out.running = std::move(library);
            if (*want != kListing)
            {
                out.listing = daoaddr::Library::Load(folder / daoaddr::FileName(kListing));
                if (out.listing && out.listing->Build() != kListing)
                {
                    out.listing.reset();
                }
            }
            break;
        }
        return out;
    }();
    return libraries;
}

// The library whose addresses are the listing's: GOG's.
const daoaddr::Library *Listing()
{
    const Libraries &libraries = Loaded();
    if (!libraries.running)
    {
        return nullptr;
    }
    if (libraries.running->Build() == kListing)
    {
        return &*libraries.running;
    }
    return libraries.listing ? &*libraries.listing : nullptr;
}

} // namespace

std::optional<daoaddr::BuildInfo> Build()
{
    const auto have = Fingerprint();
    if (!have || !have->is32Bit)
    {
        return std::nullopt;
    }
    return daoaddr::BuildInfo{.linkStamp = have->timeDateStamp,
                              .sizeOfImage = have->sizeOfImage,
                              .entryPointRva = have->entryPointRva,
                              .imageBase = have->imageBase};
}

std::uintptr_t Base()
{
    // The handle of the process's executable is its base address.
    return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
}

std::filesystem::path Path()
{
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring_view(buffer.data(), length));
}

std::optional<core::Fingerprint> Fingerprint()
{
    // The headers are mapped at the base, readable, and at least a page
    // long; that page holds the DOS header, the NT headers and the section
    // table of any exe this could be running in.
    constexpr std::size_t kHeaderBytes = 0x1000;
    const auto *bytes = reinterpret_cast<const std::byte *>(Base());
    return core::ReadFingerprint(std::span<const std::byte>(bytes, kHeaderBytes));
}

const daoaddr::Library *Library()
{
    const Libraries &libraries = Loaded();
    return libraries.running ? &*libraries.running : nullptr;
}

std::filesystem::path AddressFolder()
{
    return Loaded().folder;
}

std::uintptr_t Address(AddressId id)
{
    const daoaddr::Library *library = Library();
    const auto rva = library != nullptr ? library->Rva(id) : std::nullopt;
    return rva ? Base() + *rva : 0;
}

std::optional<daoaddr::Entry> Find(AddressId id)
{
    const daoaddr::Library *library = Library();
    return library != nullptr ? library->Find(id) : std::nullopt;
}

namespace
{

// The running build's library read the other way, made on first use.
const daoaddr::Index *RunningIndex()
{
    const daoaddr::Library *library = Library();
    if (library == nullptr)
    {
        return nullptr;
    }
    static const daoaddr::Index index{*library};
    return &index;
}

} // namespace

std::optional<Place> Containing(std::uintptr_t runtime)
{
    const daoaddr::Index *index = RunningIndex();
    if (index == nullptr || runtime < Base())
    {
        return std::nullopt;
    }
    const auto hit = index->At(static_cast<std::uint32_t>(runtime - Base()));
    if (!hit || hit->offset > kFarthest)
    {
        return std::nullopt;
    }
    return Place{.id = hit->id, .offset = hit->offset};
}

std::uint32_t Preferred(std::uintptr_t runtime)
{
    const daoaddr::Library *library = Library();
    // In GOG's build the listing's address is the image's own; so it is
    // taken to be in a build the library does not know, where there is
    // nothing better to say.
    if (library == nullptr || library->Build() == kListing)
    {
        return ImageAddress(runtime);
    }
    const daoaddr::Library *listing = Listing();
    if (listing == nullptr)
    {
        return 0;
    }
    const auto place = Containing(runtime);
    if (!place)
    {
        return 0;
    }
    const auto there = listing->Address(place->id);
    return there ? *there + place->offset : 0;
}

std::uint32_t Preferred(AddressId id)
{
    const daoaddr::Library *listing = Listing();
    return listing != nullptr ? listing->Address(id).value_or(0) : 0;
}

std::optional<CodeRange> Code()
{
    constexpr std::size_t kHeaderBytes = 0x1000;
    const auto *bytes = reinterpret_cast<const std::byte *>(Base());
    const auto text = core::FindSection(std::span<const std::byte>(bytes, kHeaderBytes), ".text");
    if (!text)
    {
        return std::nullopt;
    }
    return CodeRange{.begin = Base() + text->rva, .end = Base() + text->rva + text->virtualSize};
}

} // namespace dao::Module
