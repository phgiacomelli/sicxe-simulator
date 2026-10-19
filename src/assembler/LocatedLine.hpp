#pragma once

#include "SourceLine.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace assembler {

inline constexpr std::uint32_t MaxAddressableByte = 0xFFFFFU;
inline constexpr std::uint32_t AddressSpaceBytes = 0x100000U;

enum class LocatedLineCategory {
    Instruction,
    Data,
    Reservation,
    Directive,
    Comment,
};

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
    SourceLine source{};
    LocatedLineCategory category{LocatedLineCategory::Comment};
    std::uint32_t address{};
    std::size_t sizeBytes{};
    std::string mnemonic;
    bool extended{false};
    std::optional<std::uint8_t> opcode;
    std::optional<LocatedInstructionFormat> format;
    std::optional<LocatedDirectiveKind> directive;

    constexpr bool isComment() const noexcept {
        return category == LocatedLineCategory::Comment;
    }

    constexpr bool isInstruction() const noexcept {
        return category == LocatedLineCategory::Instruction;
    }

    constexpr bool isDirective() const noexcept {
        return category == LocatedLineCategory::Directive;
    }

    constexpr bool emitsOrReservesBytes() const noexcept {
        return sizeBytes > 0;
    }

    constexpr std::optional<std::uint32_t> listingAddress() const noexcept {
        if (category == LocatedLineCategory::Instruction || category == LocatedLineCategory::Data ||
            category == LocatedLineCategory::Reservation ||
            directive == LocatedDirectiveKind::Start) {
            return address;
        }
        return std::nullopt;
    }

    bool operator==(const LocatedLine&) const = default;
};

} // namespace assembler
