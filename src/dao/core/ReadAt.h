// A read at an offset of a file handle, which shares nothing between readers.
#pragma once

#include <cstdint>

#include <Windows.h>

namespace dao::core
{

// Reads bytes at an offset of a file with ReadFile, the offset in an OVERLAPPED, so the handle's own position is
// neither used nor moved. Safe from any number of threads on one handle.
// @param handle opened for reading; on a handle not opened for overlapped I/O the read is done when this returns
// @return the bytes read: fewer than asked at the file's end, 0 on an error
std::uint32_t ReadAt(HANDLE handle, void *buffer, std::uint32_t bytes, std::uint64_t offset);

} // namespace dao::core
