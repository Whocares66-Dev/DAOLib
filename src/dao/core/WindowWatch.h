// The two timing rules of the loader's log of the game window's close requests
// and activations (loader/WindowLog.cpp, DAOLib #199): a WM_CLOSE is told as
// following an SC_CLOSE if one came within a second before it, and a key is
// told as pressed soon after an activation if it came within two seconds of
// one. Here because the windows are plain arithmetic on a clock, which a test
// can check with no game (tests/test_windowwatch.cpp). Times are milliseconds
// on any one clock that does not run back.
#pragma once

#include <cstdint>

namespace dao::core
{

class WindowWatch
{
  public:
    static constexpr std::uint64_t kCloseAfterSysCloseMs = 1000;
    static constexpr std::uint64_t kKeyAfterActivationMs = 2000;

    // An SC_CLOSE arrived.
    void SysClose(std::uint64_t now)
    {
        sysClose_ = now;
        hasSysClose_ = true;
    }

    // Whether a WM_CLOSE arriving now follows an SC_CLOSE of at most a second ago.
    [[nodiscard]] bool CloseFollowsSysClose(std::uint64_t now) const
    {
        return hasSysClose_ && now >= sysClose_ && now - sysClose_ <= kCloseAfterSysCloseMs;
    }

    // The application was activated.
    void Activated(std::uint64_t now)
    {
        activated_ = now;
        hasActivated_ = true;
    }

    // Whether a key arriving now comes at most two seconds after the last activation.
    [[nodiscard]] bool KeySoonAfterActivation(std::uint64_t now) const
    {
        return hasActivated_ && now >= activated_ && now - activated_ <= kKeyAfterActivationMs;
    }

  private:
    std::uint64_t sysClose_ = 0;
    std::uint64_t activated_ = 0;
    bool hasSysClose_ = false;
    bool hasActivated_ = false;
};

} // namespace dao::core
