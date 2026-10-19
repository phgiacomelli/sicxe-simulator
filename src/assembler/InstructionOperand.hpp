#pragma once

#include "InstructionCatalog.hpp"

#include <variant>

namespace assembler {

enum class InstructionRegister : std::uint8_t {
    A = 0,
    X = 1,
    L = 2,
    B = 3,
    S = 4,
    T = 5,
    PC = 8,
    SW = 9
};
bool isInstructionRegister(InstructionRegister reg);

struct RegisterToken {
    InstructionRegister reg;
    SourceField source;
};
struct NoInstructionOperand {};
struct OneRegisterOperand {
    RegisterToken first;
};
struct TwoRegisterOperand {
    RegisterToken first;
    RegisterToken second;
};
struct RegisterCountOperand {
    RegisterToken first;
    std::uint8_t count; // Source count 1..16, not the encoded nibble.
    SourceField countSource;
};
enum class InstructionAddressing { Simple, Immediate, Indirect };
enum class InstructionValueKind { Integer, Symbol };
struct MemoryInstructionOperand {
    InstructionAddressing mode;
    bool indexed;
    InstructionValueKind kind;
    SourceField value; // Owned, unchanged token for package 3/6; no resolved address.
};
using InstructionOperandData = std::variant<NoInstructionOperand,
                                            OneRegisterOperand,
                                            TwoRegisterOperand,
                                            RegisterCountOperand,
                                            MemoryInstructionOperand>;
struct InstructionOperand {
    InstructionForm form;
    SourceField operation;
    InstructionOperandData data;

    std::uint8_t size() const {
        return static_cast<std::uint8_t>(form);
    }
};
using InstructionOperandResult = std::variant<InstructionOperand, InstructionDiagnostic>;
InstructionOperandResult parseInstructionOperand(const InstructionSpec& spec,
                                                 const SourceLine& parsedLine);

} // namespace assembler
