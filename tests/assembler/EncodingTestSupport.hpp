#pragma once

#include "assembler/InstructionCatalog.hpp"
#include "assembler/LocatedLine.hpp"
#include "assembler/SourceParser.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace assembler::encoding_test {

// Build a located instruction without requiring PassOne success, so invalid
// operands can exercise the encoder's public boundary too.
inline LocatedLine located(std::string_view statement, std::uint32_t address = 0) {
    const auto parsed = SourceParser::parse(" " + std::string{statement});
    EXPECT_TRUE(parsed.succeeded()) << statement;
    const auto source = parsed.lines.at(0);
    std::string key = source.statement->operation.key;
    const bool extended = key.starts_with('+');
    if (extended)
        key.erase(0, 1);
    const auto lookup = findInstruction(key);
    const auto* found = std::get_if<std::reference_wrapper<const InstructionSpec>>(&lookup);
    const bool format2 = found && found->get().formats == InstructionFormats::Format2;
    const auto format = format2    ? LocatedInstructionFormat::Format2
                        : extended ? LocatedInstructionFormat::Format4
                                   : LocatedInstructionFormat::Format3;
    return {source,
            LocatedLineCategory::Instruction,
            address,
            static_cast<std::size_t>(format),
            key,
            extended,
            found ? std::optional<std::uint8_t>{found->get().opcode} : std::nullopt,
            format,
            std::nullopt};
}

} // namespace assembler::encoding_test
