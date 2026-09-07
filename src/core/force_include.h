#pragma once

#include <cstddef>
#include <cstdint>

/// Force-included into every translation unit by the build (see src/CMakeLists.txt), so these
/// names are always in scope and no file has to include this or <cstdint> to spell an integer.
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using usize = std::size_t;
using uptr = std::uintptr_t;
