#pragma once

#include "assembler/InstructionOperand.hpp"
#include "assembler/SourceParser.hpp"
#include "core/Instruction.hpp"

#include <array>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace assembler::test {
struct ExpectedInstruction {
    std::string_view name;
    std::uint8_t opcode;
    Opcode vmOpcode;
    InstructionFormats formats;
    OperandPattern operands;
};
using F = InstructionFormats;
using P = OperandPattern;
// Independent expected architectural table; never generated from the production catalog.
inline constexpr std::array ExpectedIsa{
    ExpectedInstruction{"ADD", 0x18, Opcode::ADD, F::Format34, P::Memory},
    ExpectedInstruction{"ADDR", 0x90, Opcode::ADDR, F::Format2, P::TwoRegisters},
    ExpectedInstruction{"AND", 0x40, Opcode::AND, F::Format34, P::Memory},
    ExpectedInstruction{"CLEAR", 0xB4, Opcode::CLEAR, F::Format2, P::OneRegister},
    ExpectedInstruction{"COMP", 0x28, Opcode::COMP, F::Format34, P::Memory},
    ExpectedInstruction{"COMPR", 0xA0, Opcode::COMPR, F::Format2, P::TwoRegisters},
    ExpectedInstruction{"DIV", 0x24, Opcode::DIV, F::Format34, P::Memory},
    ExpectedInstruction{"DIVR", 0x9C, Opcode::DIVR, F::Format2, P::TwoRegisters},
    ExpectedInstruction{"J", 0x3C, Opcode::J, F::Format34, P::Memory},
    ExpectedInstruction{"JEQ", 0x30, Opcode::JEQ, F::Format34, P::Memory},
    ExpectedInstruction{"JGT", 0x34, Opcode::JGT, F::Format34, P::Memory},
    ExpectedInstruction{"JLT", 0x38, Opcode::JLT, F::Format34, P::Memory},
    ExpectedInstruction{"JSUB", 0x48, Opcode::JSUB, F::Format34, P::Memory},
    ExpectedInstruction{"LDA", 0x00, Opcode::LDA, F::Format34, P::Memory},
    ExpectedInstruction{"LDB", 0x68, Opcode::LDB, F::Format34, P::Memory},
    ExpectedInstruction{"LDCH", 0x50, Opcode::LDCH, F::Format34, P::Memory},
    ExpectedInstruction{"LDL", 0x08, Opcode::LDL, F::Format34, P::Memory},
    ExpectedInstruction{"LDS", 0x6C, Opcode::LDS, F::Format34, P::Memory},
    ExpectedInstruction{"LDT", 0x74, Opcode::LDT, F::Format34, P::Memory},
    ExpectedInstruction{"LDX", 0x04, Opcode::LDX, F::Format34, P::Memory},
    ExpectedInstruction{"MUL", 0x20, Opcode::MUL, F::Format34, P::Memory},
    ExpectedInstruction{"MULR", 0x98, Opcode::MULR, F::Format2, P::TwoRegisters},
    ExpectedInstruction{"OR", 0x44, Opcode::OR, F::Format34, P::Memory},
    ExpectedInstruction{"RMO", 0xAC, Opcode::RMO, F::Format2, P::TwoRegisters},
    ExpectedInstruction{"RSUB", 0x4C, Opcode::RSUB, F::Format3, P::None},
    ExpectedInstruction{"SHIFTL", 0xA4, Opcode::SHIFTL, F::Format2, P::RegisterAndCount},
    ExpectedInstruction{"SHIFTR", 0xA8, Opcode::SHIFTR, F::Format2, P::RegisterAndCount},
    ExpectedInstruction{"STA", 0x0C, Opcode::STA, F::Format34, P::Memory},
    ExpectedInstruction{"STB", 0x78, Opcode::STB, F::Format34, P::Memory},
    ExpectedInstruction{"STCH", 0x54, Opcode::STCH, F::Format34, P::Memory},
    ExpectedInstruction{"STL", 0x14, Opcode::STL, F::Format34, P::Memory},
    ExpectedInstruction{"STS", 0x7C, Opcode::STS, F::Format34, P::Memory},
    ExpectedInstruction{"STT", 0x84, Opcode::STT, F::Format34, P::Memory},
    ExpectedInstruction{"STX", 0x10, Opcode::STX, F::Format34, P::Memory},
    ExpectedInstruction{"SUB", 0x1C, Opcode::SUB, F::Format34, P::Memory},
    ExpectedInstruction{"SUBR", 0x94, Opcode::SUBR, F::Format2, P::TwoRegisters},
    ExpectedInstruction{"TIX", 0x2C, Opcode::TIX, F::Format34, P::Memory},
    ExpectedInstruction{"TIXR", 0xB8, Opcode::TIXR, F::Format2, P::OneRegister},
};

inline const InstructionSpec& spec(std::string_view name) {
    const auto result = findInstruction(name);
    EXPECT_TRUE(std::holds_alternative<std::reference_wrapper<const InstructionSpec>>(result));
    return std::get<std::reference_wrapper<const InstructionSpec>>(result).get();
}
inline SourceLine line(std::string_view statement) {
    const auto parsed = SourceParser::parse("\n\t" + std::string{statement});
    EXPECT_TRUE(parsed.succeeded());
    return parsed.lines.at(1);
}
inline InstructionOperandResult parse(std::string_view statement) {
    const auto source = line(statement);
    const auto& operation = source.statement.value().operation;
    const auto found = findInstruction(operation);
    if (const auto* failure = std::get_if<InstructionDiagnostic>(&found)) {
        return *failure;
    }
    return parseInstructionOperand(
        std::get<std::reference_wrapper<const InstructionSpec>>(found).get(), source);
}
inline InstructionOperand operand(std::string_view statement) {
    const auto result = parse(statement);
    EXPECT_TRUE(std::holds_alternative<InstructionOperand>(result)) << statement;
    return std::get<InstructionOperand>(result);
}
} // namespace assembler::test
