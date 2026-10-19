#pragma once

#include <cstdint>

namespace assembler {

inline constexpr std::uint32_t AddressSpaceBytes = 0x100000U;
inline constexpr std::uint32_t MaxAddressableByte = AddressSpaceBytes - 1U;

} // namespace assembler
