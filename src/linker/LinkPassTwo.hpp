#pragma once

#include "LinkPassOne.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace linker {

struct LinkModificationResidual {
    std::uint32_t offset{};
    std::uint8_t halfBytes{};
    // Coefficient of the eventual load address: positive adds, negative subtracts.
    std::int64_t loadAddressCoefficient{};
    ObjectPosition position;
    // M05 keeps the exact signed sum for range checking before any final write.
    // For M06 this is the sum reduced modulo 2^24.
    std::int64_t valueBeforeLoad{};

    bool operator==(const LinkModificationResidual&) const = default;
};

struct LinkPassTwoIntermediate {
    // A disengaged byte is a hole. M05 bytes contain only the low 20 bits.
    std::vector<std::optional<std::uint8_t>> text;
    std::uint32_t totalLength{};
    std::uint32_t entryOffset{};
    // Pending load terms and any M05 values still requiring a range check.
    // A zero coefficient represents a range check only, not a residual M record.
    std::vector<LinkModificationResidual> residuals;

    bool operator==(const LinkPassTwoIntermediate&) const = default;
};

struct LinkPassTwoResult {
    std::optional<LinkPassTwoIntermediate> intermediate;
    std::vector<ObjectDiagnostic> diagnostics;

    constexpr bool succeeded() const noexcept {
        return intermediate.has_value() && diagnostics.empty();
    }

    ObjectFailure failure() const {
        return ObjectFailure{diagnostics};
    }
};

class LinkPassTwo {
public:
    // Use the same unmodified modules, in order, and the corresponding first-pass result.
    static LinkPassTwoResult run(std::span<const ObjectModule> modules,
                                 const LinkPassOneResult& passOne);
};

LinkPassTwoResult runLinkPassTwo(std::span<const ObjectModule> modules,
                                 const LinkPassOneResult& passOne);

} // namespace linker
