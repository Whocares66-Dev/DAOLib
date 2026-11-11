// The error core's readers throw when a file they found cannot be read: its open failed, or the read
// came up short (DAOLib, for DAOParagonEngine, 2026-11-07). A file that is not there is no error; the
// readers answer nothing for it.
#pragma once

#include <stdexcept>

namespace dao::core
{

// A file that is there could not be read. what() names the file and the system's error.
class ReadError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

} // namespace dao::core
