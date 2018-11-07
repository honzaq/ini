#pragma once
#include <stdint.h>

namespace ini {

uint32_t crc32(const void* data, size_t length, uint32_t previousCrc32 = 0);

} // end of namespace ini
