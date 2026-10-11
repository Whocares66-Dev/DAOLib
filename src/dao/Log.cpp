// One log file per DLL. Each DLL links spdlog statically and so has a
// default logger of its own; OpenLog points it at that DLL's file. The level
// comes from DAOLIB_LOG_LEVEL, info when unset, and the file's first line
// names the level it took, so a log says what it can show.
//
// A line costs its caller no disk write: it is appended to a buffer that a
// writer thread of the log's own writes out every quarter of a second, and
// at once for a warning or an error (DAOLib #31), so logging from the
// game's frame thread does not stall it. A line can reach the file up to a
// quarter of a second late; kLogLatency is what a reader of the log files
// (the crash logger copying their last lines) waits for first. The crash
// logger's own log is written apart.
#include "dao/Log.h"

#include "dao/Module.h"

#include <Windows.h>

#include <ShlObj.h>
#include <share.h>
#include <spdlog/sinks/base_sink.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace dao
{

std::filesystem::path LogDirectory()
{
    PWSTR documents = nullptr;
    std::filesystem::path root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)))
    {
        root = documents;
    }
    CoTaskMemFree(documents);
    if (root.empty())
    {
        root = std::filesystem::temp_directory_path();
    }
    return root / L"BioWare" / L"Dragon Age" / L"daolib";
}

namespace
{

spdlog::level::level_enum LevelFromEnvironment()
{
    std::string value;
    // getenv_s: the checked form MSVC wants, sized in two calls.
    std::size_t needed = 0;
    if (getenv_s(&needed, nullptr, 0, "DAOLIB_LOG_LEVEL") != 0 || needed == 0)
    {
        return spdlog::level::info;
    }
    value.resize(needed);
    if (getenv_s(&needed, value.data(), value.size(), "DAOLIB_LOG_LEVEL") != 0)
    {
        return spdlog::level::info;
    }
    value.resize(needed - 1); // the terminator getenv_s counted
    const auto level = spdlog::level::from_str(value);
    // from_str answers "off" for a word it does not know, which would hide
    // a typo along with every message; a typo reads as the default instead.
    if (level == spdlog::level::off && value != "off")
    {
        return spdlog::level::info;
    }
    return level;
}

// Whether this game writes the plain <name>.log. The first game running
// with DAOLib does; one started while it runs writes <name>.<pid>.log, so
// as not to truncate the running game's logs (a second DAOrigins.exe did,
// in a playtest, 2026-09-30). The owner's process id sits in a small named
// block of shared memory: every DLL of the owning game finds its own id
// there, and a game that finds another's takes over only if that one has
// exited. Each DLL has the library's statics of its own, so each asks.
bool OwnsLogs()
{
    // An instance (dao/Module.h, DAOLib #67) has a log folder of its own and so a block of
    // its own: with the one block, an instance beside the first game took the names of a
    // second copy in a folder nothing else wrote to (in play, 2026-10-11).
    std::wstring block = L"Local\\DAOLib.LogOwner";
    if (const std::string instance = Module::Instance(); !instance.empty())
    {
        block += L'.';
        block.append(instance.begin(), instance.end());
    }
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LONG), block.c_str());
    if (mapping == nullptr)
    {
        return true;
    }
    auto *owner = static_cast<LONG volatile *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG)));
    if (owner == nullptr)
    {
        CloseHandle(mapping);
        return true;
    }
    // The mapping and its view stay open for the game's life: the block
    // lasts as long as a game holds it.
    const auto self = static_cast<LONG>(GetCurrentProcessId());
    for (;;)
    {
        const LONG current = InterlockedCompareExchange(owner, self, 0);
        if (current == 0 || current == self)
        {
            return true;
        }
        DWORD code = 0;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(current));
        const bool alive = process != nullptr && (GetExitCodeProcess(process, &code) != 0) && code == STILL_ACTIVE;
        if (process != nullptr)
        {
            CloseHandle(process);
        }
        if (alive)
        {
            return false;
        }
        if (InterlockedCompareExchange(owner, self, current) == current)
        {
            return true;
        }
    }
}

// A log file written on a thread of its own. A plugin logs from the game's
// frame thread, and a line written and flushed there is a disk write the
// frame waits for, into a folder OneDrive may watch; so a line is formatted
// and appended to a buffer in memory, and a writer thread writes the buffer
// out: every quarter of a second, at once for a warning or an error, and
// when it passes 64 KB. The thread is never joined: at the game's exit
// Windows has ended it before the library's statics go, and joining it
// there could hang the exit; the sink's destructor writes what is left if
// it can take the lock. A crash leaves it running while the crash logger
// works, so the last lines reach the disk. Lines past 32 MB unwritten (a
// disk that stopped) are dropped and counted.
class Writer
{
  public:
    explicit Writer(std::FILE *file) : file_(file)
    {
    }
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    ~Writer()
    {
        std::fclose(file_);
    }

    void Append(std::string_view line, bool urgent)
    {
        {
            const std::lock_guard lock(mutex_);
            if (pending_.size() + line.size() > kMostPending)
            {
                ++dropped_;
                return;
            }
            pending_.append(line);
            urgent_ = urgent_ || urgent || pending_.size() >= kEager;
        }
        if (urgent)
        {
            wake_.notify_one();
        }
    }
    void Urge()
    {
        {
            const std::lock_guard lock(mutex_);
            urgent_ = true;
        }
        wake_.notify_one();
    }
    // The writer thread's loop.
    void Run()
    {
        std::string out;
        for (;;)
        {
            std::size_t dropped = 0;
            {
                std::unique_lock lock(mutex_);
                wake_.wait_for(lock, kLogLatency, [this] { return urgent_; });
                urgent_ = false;
                out.swap(pending_);
                dropped = std::exchange(dropped_, 0);
            }
            Write(out, dropped);
            out.clear();
        }
    }
    // At the DLL's end: what is left, if no one holds the lock.
    void Finish()
    {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (lock.owns_lock())
        {
            Write(pending_, dropped_);
            pending_.clear();
        }
    }

  private:
    static constexpr std::size_t kEager = 64 * 1024;
    static constexpr std::size_t kMostPending = 32 * 1024 * 1024;

    void Write(const std::string &text, std::size_t dropped)
    {
        if (!text.empty())
        {
            std::fwrite(text.data(), 1, text.size(), file_);
        }
        if (dropped != 0)
        {
            std::fprintf(file_, "[log] %zu line(s) dropped: the log could not be written fast enough\n", dropped);
        }
        if (!text.empty() || dropped != 0)
        {
            std::fflush(file_);
        }
    }

    std::FILE *file_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::string pending_;
    std::size_t dropped_ = 0;
    bool urgent_ = false;
};

class BackgroundFileSink final : public spdlog::sinks::base_sink<std::mutex>
{
  public:
    explicit BackgroundFileSink(std::shared_ptr<Writer> writer) : writer_(std::move(writer))
    {
        std::thread([writer = writer_] { writer->Run(); }).detach();
    }
    BackgroundFileSink(const BackgroundFileSink &) = delete;
    BackgroundFileSink &operator=(const BackgroundFileSink &) = delete;
    ~BackgroundFileSink() override
    {
        writer_->Finish();
    }

  protected:
    void sink_it_(const spdlog::details::log_msg &message) override
    {
        spdlog::memory_buf_t line;
        formatter_->format(message, line);
        writer_->Append({line.data(), line.size()}, message.level >= spdlog::level::warn);
    }
    void flush_() override
    {
        writer_->Urge();
    }

  private:
    std::shared_ptr<Writer> writer_;
};

} // namespace

void OpenLog(std::string_view name)
{
    static bool opened = false;
    if (opened)
    {
        return;
    }
    opened = true;
    try
    {
        const auto directory = LogDirectory();
        std::filesystem::create_directories(directory);
        const bool owner = OwnsLogs();
        const std::string stem =
            owner ? std::string(name) : std::string(name) + "." + std::to_string(GetCurrentProcessId());
        const auto file = directory / (stem + ".log");
        if (owner)
        {
            // The last session's log is kept, one generation, as
            // <name>.previous.log: a game that crashed and was started again at
            // once lost the log of the very session that crashed (DAOLib #47).
            // A reader holding the file open without sharing delete keeps the
            // rename from happening; then the log starts over, as before.
            std::error_code kept;
            std::filesystem::rename(file, directory / (stem + ".previous.log"), kept);
        }
        // Shared, as spdlog's own file sink opened it: a log is read while
        // the game writes it, by a player tailing it and by the tools that
        // watch a session (_wfopen_s denies every other opener, which made
        // the logs unreadable while the game ran, DAOLib #31).
        std::FILE *stream = _wfsopen(file.c_str(), L"wb", _SH_DENYNO);
        if (stream == nullptr)
        {
            throw std::runtime_error("cannot open " + file.string());
        }
        auto sink = std::make_shared<BackgroundFileSink>(std::make_shared<Writer>(stream));
        auto logger = std::make_shared<spdlog::logger>(std::string(name), std::move(sink));
        const auto level = LevelFromEnvironment();
        logger->set_level(level);
        // A warning or an error goes out at once (the sink wakes its writer
        // for those); the rest within a quarter of a second.
        logger->flush_on(spdlog::level::warn);
        logger->set_pattern("[%H:%M:%S.%e] [%t] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
        // A plugin has no version of its own here: the line names the plugin and
        // the library it was built with (DAOLib #46).
        spdlog::info("{} (daolib {}), level {}", name, DAO_VERSION_STRING, spdlog::level::to_string_view(level));
    }
    catch (const std::exception &e)
    {
        // No folder to write to is not a reason to stop the game; the
        // default logger stays spdlog's, which goes nowhere in a DLL, and
        // the reason goes where a debugger attached to the game can see it.
        OutputDebugStringA((std::string("daolib: could not open the log: ") + e.what()).c_str());
    }
}

} // namespace dao
