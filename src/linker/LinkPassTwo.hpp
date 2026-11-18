#pragma once

#include "LinkPassOne.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace linker {

// Sparse combined text. A disengaged byte is a hole (for example, a RESB area).
struct LinkModificationResidual {
    std::uint32_t offset{};
    std::uint8_t halfBytes{};
    // Coefficient of the eventual load address: positive adds, negative subtracts.
    std::int64_t loadAddressCoefficient{};
    ObjectPosition position;

    bool operator==(const LinkModificationResidual&) const = default;
};

struct LinkPassTwoIntermediate {
    std::vector<std::optional<std::uint8_t>> text;
    std::uint32_t totalLength{};
    std::uint32_t entryOffset{};
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
    static LinkPassTwoResult run(std::span<const ObjectModule> modules,
                                 const LinkPassOneResult& passOne);
};

LinkPassTwoResult runLinkPassTwo(std::span<const ObjectModule> modules,
                                 const LinkPassOneResult& passOne);

} // namespace linker
