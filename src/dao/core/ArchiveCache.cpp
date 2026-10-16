// The kept directories' file (dao/core/ArchiveCache.h), DAOLib #144.
//
// Little-endian throughout: "DAOLIBAC", the format's version (u32), the count
// (u32), then each directory: its path (u32 length, UTF-8), its size (u64),
// its last-write time (i64), its header (version, count and flags, u32
// each) and its files, a count (u32) and each name (u32 length, UTF-8) and
// size (u32). A file of another version is no cache: a new DAOLib reads the
// archives again rather than misread an old one. The file is archives.bin in
// the cache folder.
#include "dao/core/ArchiveCache.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <random>

namespace dao::core::install
{

namespace fs = std::filesystem;

namespace
{

constexpr char kMagic[8] = {'D', 'A', 'O', 'L', 'I', 'B', 'A', 'C'};
constexpr std::uint32_t kVersion = 1;
constexpr const wchar_t *kKeptFile = L"archives.bin";

class Writer
{
  public:
    explicit Writer(std::vector<std::byte> &out) : out_(out)
    {
    }
    template <class T> void Number(T value)
    {
        for (std::size_t i = 0; i < sizeof(T); ++i)
        {
            out_.push_back(static_cast<std::byte>((static_cast<std::uint64_t>(value) >> (8 * i)) & 0xFF));
        }
    }
    template <class Length> void Text(const std::string &text)
    {
        Number(static_cast<Length>(text.size()));
        for (const char c : text)
        {
            out_.push_back(static_cast<std::byte>(c));
        }
    }

  private:
    std::vector<std::byte> &out_;
};

class Reader
{
  public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes)
    {
    }
    template <class T> std::optional<T> Number()
    {
        if (bytes_.size() - at_ < sizeof(T))
        {
            return std::nullopt;
        }
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
        {
            value |= static_cast<std::uint64_t>(bytes_[at_ + i]) << (8 * i);
        }
        at_ += sizeof(T);
        return static_cast<T>(value);
    }
    template <class Length> std::optional<std::string> Text()
    {
        const auto length = Number<Length>();
        if (!length || bytes_.size() - at_ < *length)
        {
            return std::nullopt;
        }
        std::string text(*length, '\0');
        std::memcpy(text.data(), bytes_.data() + at_, *length);
        at_ += *length;
        return text;
    }
    bool Ended() const
    {
        return at_ == bytes_.size();
    }
    std::size_t Left() const
    {
        return bytes_.size() - at_;
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t at_ = 0;
};

std::optional<KeptDirectory> ReadOne(Reader &in)
{
    KeptDirectory kept;
    auto path = in.Text<std::uint32_t>();
    const auto size = in.Number<std::uint64_t>();
    const auto written = in.Number<std::int64_t>();
    if (!path || !size || !written)
    {
        return std::nullopt;
    }
    kept.path = std::move(*path);
    kept.size = *size;
    kept.written = *written;
    const auto version = in.Number<std::uint32_t>();
    const auto count = in.Number<std::uint32_t>();
    const auto flags = in.Number<std::uint32_t>();
    const auto files = in.Number<std::uint32_t>();
    // Each file takes eight bytes at the least: more files than that are no count.
    if (!version || *version > static_cast<std::uint32_t>(erf::Version::V22) || !count || !flags || !files ||
        *files > in.Left() / 8)
    {
        return std::nullopt;
    }
    kept.header = erf::Header{static_cast<erf::Version>(*version), *count, *flags};
    kept.files.reserve(*files);
    for (std::uint32_t f = 0; f < *files; ++f)
    {
        auto name = in.Text<std::uint32_t>();
        const auto bytes = in.Number<std::uint32_t>();
        if (!name || !bytes)
        {
            return std::nullopt;
        }
        kept.files.emplace_back(std::move(*name), *bytes);
    }
    return kept;
}

} // namespace

std::vector<std::byte> WriteKept(std::span<const KeptDirectory> directories)
{
    std::vector<std::byte> out;
    Writer w(out);
    for (const char c : kMagic)
    {
        out.push_back(static_cast<std::byte>(c));
    }
    w.Number(kVersion);
    w.Number(static_cast<std::uint32_t>(directories.size()));
    for (const KeptDirectory &kept : directories)
    {
        w.Text<std::uint32_t>(kept.path);
        w.Number(kept.size);
        w.Number(kept.written);
        w.Number(static_cast<std::uint32_t>(kept.header.version));
        w.Number(kept.header.count);
        w.Number(kept.header.flags);
        w.Number(static_cast<std::uint32_t>(kept.files.size()));
        for (const auto &[name, size] : kept.files)
        {
            w.Text<std::uint32_t>(name);
            w.Number(size);
        }
    }
    return out;
}

std::optional<std::vector<KeptDirectory>> ReadKept(std::span<const std::byte> bytes)
{
    if (bytes.size() < sizeof kMagic || std::memcmp(bytes.data(), kMagic, sizeof kMagic) != 0)
    {
        return std::nullopt;
    }
    Reader in(bytes.subspan(sizeof kMagic));
    const auto version = in.Number<std::uint32_t>();
    const auto count = in.Number<std::uint32_t>();
    // Each directory takes 36 bytes at the least.
    if (!version || *version != kVersion || !count || *count > in.Left() / 36)
    {
        return std::nullopt;
    }
    std::vector<KeptDirectory> directories;
    directories.reserve(*count);
    for (std::uint32_t d = 0; d < *count; ++d)
    {
        auto kept = ReadOne(in);
        if (!kept)
        {
            return std::nullopt;
        }
        directories.push_back(std::move(*kept));
    }
    if (!in.Ended())
    {
        return std::nullopt;
    }
    return directories;
}

KeptByPath LoadKept(const fs::path &folder)
{
    KeptByPath kept;
    if (folder.empty())
    {
        return kept;
    }
    std::ifstream in(folder / kKeptFile, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto directories = ReadKept(std::as_bytes(std::span(text)));
    if (!directories)
    {
        return kept;
    }
    kept.reserve(directories->size());
    for (KeptDirectory &directory : *directories)
    {
        std::string key = directory.path;
        kept.emplace(std::move(key), std::move(directory));
    }
    return kept;
}

void SaveKept(const fs::path &folder, std::span<const KeptDirectory> keep)
{
    std::error_code error;
    fs::create_directories(folder, error);
    std::random_device random;
    const fs::path temporary = folder / (std::wstring(kKeptFile) + L"." + std::to_wstring(random()) + L".tmp");
    const std::vector<std::byte> bytes = WriteKept(keep);
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out.good())
        {
            out.close();
            fs::remove(temporary, error);
            return;
        }
    }
    fs::rename(temporary, folder / kKeptFile, error);
    if (error)
    {
        fs::remove(temporary, error);
    }
}

} // namespace dao::core::install
