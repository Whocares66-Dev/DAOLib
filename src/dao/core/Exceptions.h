// Names for Windows exception codes, as a crash log prints them:
// EXCEPTION_ACCESS_VIOLATION for 0xC0000005, and so on, with the one C++
// uses for `throw` (0xE06D7363, "msc" in ASCII) among them.
#pragma once

#include <cstdint>
#include <string_view>

namespace dao::core
{

// The name of a code, or an empty view for one this does not know.
std::string_view ExceptionName(std::uint32_t code);

// For an access violation: what the faulting instruction was doing, from the
// first ExceptionInformation word (0 read, 1 write, 8 execute).
std::string_view AccessKind(std::uint32_t operation);

} // namespace dao::core
