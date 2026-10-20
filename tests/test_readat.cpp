// ReadAt (dao/core/ReadAt.h): a read at an offset of one handle, which many
// threads may share.
#include "dao/core/ReadAt.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace
{

// A file of the test's own, gone when the test is, named so no two copies of the tests share one.
struct TempFile
{
    std::filesystem::path path;

    explicit TempFile(const std::vector<std::uint8_t> &bytes)
    {
        std::random_device random;
        path = std::filesystem::temp_directory_path() /
               ("dao_readat" + std::to_string(random()) + "_" + std::to_string(random()) + ".bin");
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;
};

// Every byte says where it is, so a read of another's place shows.
std::vector<std::uint8_t> Pattern(std::size_t size)
{
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t i = 0; i < size; ++i)
    {
        bytes[i] = static_cast<std::uint8_t>(((i * 31) + (i / 251)) & 0xFF);
    }
    return bytes;
}

struct Handle
{
    HANDLE value;
    explicit Handle(const std::filesystem::path &path)
        : value(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr))
    {
    }
    ~Handle()
    {
        if (value != INVALID_HANDLE_VALUE)
        {
            CloseHandle(value);
        }
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};

} // namespace

TEST_CASE("ReadAt reads the bytes at an offset, and past the end the bytes there are", "[readat]")
{
    const auto bytes = Pattern(10000);
    const TempFile file(bytes);
    const Handle handle(file.path);
    REQUIRE(handle.value != INVALID_HANDLE_VALUE);

    std::vector<std::uint8_t> got(100);
    CHECK(dao::core::ReadAt(handle.value, got.data(), 100, 5000) == 100);
    CHECK(std::equal(got.begin(), got.end(), bytes.begin() + 5000));

    got.assign(100, 0);
    CHECK(dao::core::ReadAt(handle.value, got.data(), 100, 9950) == 50);
    CHECK(std::equal(got.begin(), got.begin() + 50, bytes.begin() + 9950));

    CHECK(dao::core::ReadAt(handle.value, got.data(), 100, 10000) == 0);
    CHECK(dao::core::ReadAt(handle.value, got.data(), 100, 20000) == 0);
    // The handle's own position was never used: the first bytes are still the first.
    CHECK(dao::core::ReadAt(handle.value, got.data(), 100, 0) == 100);
    CHECK(std::equal(got.begin(), got.end(), bytes.begin()));
}

TEST_CASE("sixteen threads read different offsets of one handle, each its own bytes", "[readat]")
{
    constexpr std::uint32_t kSize = 1U << 20;
    constexpr std::uint32_t kChunk = 4096;
    const auto bytes = Pattern(kSize);
    const TempFile file(bytes);
    const Handle handle(file.path);
    REQUIRE(handle.value != INVALID_HANDLE_VALUE);

    std::atomic<int> wrong = 0;
    std::vector<std::thread> threads;
    threads.reserve(16);
    for (std::uint32_t t = 0; t < 16; ++t)
    {
        threads.emplace_back([&, t] {
            std::vector<std::uint8_t> got(kChunk);
            for (std::uint32_t round = 0; round < 200; ++round)
            {
                const std::uint32_t offset = ((t * 65537U) + (round * 4099U)) % (kSize - kChunk);
                if (dao::core::ReadAt(handle.value, got.data(), kChunk, offset) != kChunk ||
                    !std::equal(got.begin(), got.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset)))
                {
                    ++wrong;
                }
            }
        });
    }
    for (auto &thread : threads)
    {
        thread.join();
    }
    CHECK(wrong == 0);
}
