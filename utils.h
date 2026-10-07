#pragma once
// Little-endian byte helpers for RCF parsing.

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

template <typename T>
constexpr auto EnumToValue(T value) {
    return static_cast<typename std::underlying_type<T>::type>(value);
}

// Read a little-endian integer of `length` bytes at `offset`.
[[nodiscard]] inline std::uint32_t ReadU32LE(const std::uint8_t* data,
                                             std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

// Append a u32 in little-endian order.
inline void AppendU32LE(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}
