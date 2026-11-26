#pragma once

#include <cstdint>
#include <span>

namespace linker {

enum class RelocationWidth : std::uint8_t {
    Address = 5,
    Word = 6,
};

constexpr std::uint32_t relocationMask(RelocationWidth width) noexcept {
    return width == RelocationWidth::Address ? 0xFFFFFU : 0xFFFFFFU;
}

// Both fields occupy three big-endian bytes. M05 excludes the high nibble.
constexpr std::uint32_t readRelocationField(std::span<const std::uint8_t, 3> bytes,
                                            RelocationWidth width) noexcept {
    const auto encoded = (static_cast<std::uint32_t>(bytes[0]) << 16U) |
                         (static_cast<std::uint32_t>(bytes[1]) << 8U) | bytes[2];
    return encoded & relocationMask(width);
}

// Stores the low bits of value and preserves the high nibble for M05.
constexpr void writeRelocationField(std::span<std::uint8_t, 3> bytes,
                                    RelocationWidth width,
                                    std::uint32_t value) noexcept {
    const auto firstByte = value >> 16U;
    bytes[0] = static_cast<std::uint8_t>(
        width == RelocationWidth::Address ? (bytes[0] & 0xF0U) | (firstByte & 0x0FU) : firstByte);
    bytes[1] = static_cast<std::uint8_t>(value >> 8U);
    bytes[2] = static_cast<std::uint8_t>(value);
}

} // namespace linker
