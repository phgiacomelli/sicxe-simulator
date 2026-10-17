#pragma once

#include "AssemblyDiagnostic.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace assembler {

struct AssemblySymbol {
    std::string name;
    std::uint32_t address{};
    SourcePosition definition;

    bool operator==(const AssemblySymbol&) const = default;
};

struct AssemblyListingLine {
    std::size_t line{};
    std::string source;
    std::optional<std::uint32_t> address;
    std::vector<std::uint8_t> bytes;
    std::size_t reservedBytes{};

    bool operator==(const AssemblyListingLine&) const = default;
};

struct AssemblySuccess {
    std::uint32_t loadAddress{};
    std::uint32_t initialPc{};
    std::vector<std::uint8_t> bytes;
    std::vector<AssemblyListingLine> listing;
    std::vector<AssemblySymbol> symbols;

    bool operator==(const AssemblySuccess&) const = default;
};

struct AssemblyFailure {
    std::vector<AssemblyDiagnostic> diagnostics;

    bool operator==(const AssemblyFailure&) const = default;
};

// Failure carries diagnostics only: no partially assembled bytes are exposed.
using AssemblyResult = std::variant<AssemblySuccess, AssemblyFailure>;

} // namespace assembler
