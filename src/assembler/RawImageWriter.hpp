#pragma once

#include "AssemblyResult.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace assembler {

using RawImageResult = std::variant<std::vector<std::uint8_t>, AssemblyDiagnostic>;

class RawImageWriter {
public:
    static RawImageResult build(std::uint32_t startAddress,
                                std::size_t length,
                                std::span<const AssemblyListingLine> listing);
};

} // namespace assembler
