// The running DAOrigins.exe, and an ID turned into an address in it.
//
// The game is sold in more than one build (extern/DAOAddressLibrary's
// dev/BUILDS.md), each with its functions and globals somewhere else. The
// library holds IDs (dao/Addresses.h), and here each is looked up in the
// file for the build that is running, beside the loader:
// daolib\addresses\daoaddresses-<link stamp>-<image size>-<entry point>-
// <base>.bin, named by all four of the exe's header fields, since Steam's
// build has GOG's link stamp. A build with no file is one the library does
// not know, and nothing is hooked or read in it. A new build of the game is
// a new file there, with neither the loader nor a plugin rebuilt.
//
// Logs, the probe's call chains and the crash logger write addresses as the
// GOG build's Ghidra listing shows them, whichever build is running, since
// that is the listing the names and the stories in the comments belong to:
// Preferred goes from a running address back to that. It needs the GOG
// build's file as well, which ships beside the others.
#include "dao/Module.h"

#include "dao/core/Instance.h"
#include "dao/core/Pe.h"

#include <Windows.h>

#include <array>
#include <span>
#include <utility>

namespace dao::Module
{

// The loader leaves DAOLIB_INSTANCE as what the process is (loader/Instance.h,
// DAOLib #67): the name it took, in lower case, or unset. Read from the
// process's own environment, not the C runtime's copy of it, which was made
// before the loader wrote the variable.
std::string Instance()
{
    std::array<char, core::kLongestInstanceName + 2> value{};
    const DWORD length = GetEnvironmentVariableA("DAOLIB_INSTANCE", value.data(), static_cast<DWORD>(value.size()));
    return length < value.size() ? core::InstanceOf({value.data(), length}).value_or(std::string()) : std::string();
}

// A plugin's own folder is daolib\plugins\<name>, where its files sit beside
// it (DAOLib #26).
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
        // Beside the loader: for the loader itself, for a plugin in its folder
        // daolib\plugins\<name> (DAOLib #26), and for either when the loader
        // sits beside the exe.
        for (const auto &folder : {own / L"daolib" / L"addresses", own.parent_path().parent_path() / L"addresses",
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
    return {std::wstring_view(buffer.data(), length)};
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

// The files are looked for in daolib\addresses beside the loader, which for a
// plugin in its folder daolib\plugins\<name> is two levels up; the first of
// the candidates (Loaded) that holds the running build's file wins.
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

// `python tools\address_ids.py 0x0066E1B4` says the ID and offset of an
// address read in the GOG listing. The library does not know where a
// function ends, only where the next ID begins; every function the analysis
// found has an ID, so an address in code is in the function named. kFarthest
// bounds how far past an ID is still believed.
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

// The listing's address is what Ghidra, the logs and the last argument of a
// row in dao/Addresses.h write. In another build it is the ID at or below the
// address, and as far past that ID's place in GOG's build.
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
