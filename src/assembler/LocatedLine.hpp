#pragma once

#include "SourceLine.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace assembler {

enum class LocatedLineCategory {
    Instruction,
    Data,
    Reservation,
    Directive,
    Comment,
};

using LocatedLineKind = LocatedLineCategory;

enum class LocatedInstructionFormat : std::uint8_t {
    Format2 = 2,
    Format3 = 3,
    Format4 = 4,
};

enum class LocatedDirectiveKind {
    Start,
    End,
    Base,
    NoBase,
    Byte,
    Word,
    Resb,
    Resw,
};

struct LocatedLine {
    SourceLine source;
    LocatedLineCategory category{LocatedLineCategory::Comment};
    std::uint32_t address{};
    std::size_t size{};
    std::string mnemonic;
    bool extended{false};
    std::optional<std::uint8_t> opcode;
    std::optional<LocatedInstructionFormat> instructionFormat;
    std::optional<LocatedDirectiveKind> directive;

    bool hasAddress() const noexcept {
        if (category == LocatedLineCategory::Instruction || category == LocatedLineCategory::Data ||
            category == LocatedLineCategory::Reservation) {
            return true;
        }
        return category == LocatedLineCategory::Directive &&
               directive == LocatedDirectiveKind::Start;
    }

    std::optional<std::uint32_t> listingAddress() const noexcept {
        if (hasAddress()) {
            return address;
        }
        return std::nullopt;
    }

    bool operator==(const LocatedLine&) const = default;
};

} // namespace assembler

