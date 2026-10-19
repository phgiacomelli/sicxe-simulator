#include "InstructionCatalog.hpp"

#include <array>

namespace assembler {
namespace {
using F = InstructionFormats;
using P = OperandPattern;
constexpr std::array Specs{
    InstructionSpec{"ADD", 0x18, F::Format34, P::Memory},
    InstructionSpec{"ADDR", 0x90, F::Format2, P::TwoRegisters},
    InstructionSpec{"AND", 0x40, F::Format34, P::Memory},
    InstructionSpec{"CLEAR", 0xB4, F::Format2, P::OneRegister},
    InstructionSpec{"COMP", 0x28, F::Format34, P::Memory},
    InstructionSpec{"COMPR", 0xA0, F::Format2, P::TwoRegisters},
    InstructionSpec{"DIV", 0x24, F::Format34, P::Memory},
    InstructionSpec{"DIVR", 0x9C, F::Format2, P::TwoRegisters},
    InstructionSpec{"J", 0x3C, F::Format34, P::Memory},
    InstructionSpec{"JEQ", 0x30, F::Format34, P::Memory},
    InstructionSpec{"JGT", 0x34, F::Format34, P::Memory},
    InstructionSpec{"JLT", 0x38, F::Format34, P::Memory},
    InstructionSpec{"JSUB", 0x48, F::Format34, P::Memory},
    InstructionSpec{"LDA", 0x00, F::Format34, P::Memory},
    InstructionSpec{"LDB", 0x68, F::Format34, P::Memory},
    InstructionSpec{"LDCH", 0x50, F::Format34, P::Memory},
    InstructionSpec{"LDL", 0x08, F::Format34, P::Memory},
    InstructionSpec{"LDS", 0x6C, F::Format34, P::Memory},
    InstructionSpec{"LDT", 0x74, F::Format34, P::Memory},
    InstructionSpec{"LDX", 0x04, F::Format34, P::Memory},
    InstructionSpec{"MUL", 0x20, F::Format34, P::Memory},
    InstructionSpec{"MULR", 0x98, F::Format2, P::TwoRegisters},
    InstructionSpec{"OR", 0x44, F::Format34, P::Memory},
    InstructionSpec{"RMO", 0xAC, F::Format2, P::TwoRegisters},
    InstructionSpec{"RSUB", 0x4C, F::Format3, P::None},
    InstructionSpec{"SHIFTL", 0xA4, F::Format2, P::RegisterAndCount},
    InstructionSpec{"SHIFTR", 0xA8, F::Format2, P::RegisterAndCount},
    InstructionSpec{"STA", 0x0C, F::Format34, P::Memory},
    InstructionSpec{"STB", 0x78, F::Format34, P::Memory},
    InstructionSpec{"STCH", 0x54, F::Format34, P::Memory},
    InstructionSpec{"STL", 0x14, F::Format34, P::Memory},
    InstructionSpec{"STS", 0x7C, F::Format34, P::Memory},
    InstructionSpec{"STT", 0x84, F::Format34, P::Memory},
    InstructionSpec{"STX", 0x10, F::Format34, P::Memory},
    InstructionSpec{"SUB", 0x1C, F::Format34, P::Memory},
    InstructionSpec{"SUBR", 0x94, F::Format2, P::TwoRegisters},
    InstructionSpec{"TIX", 0x2C, F::Format34, P::Memory},
    InstructionSpec{"TIXR", 0xB8, F::Format2, P::OneRegister},
};

bool matches(std::string_view input, std::string_view key) {
    if (input.size() != key.size()) {
        return false;
    }
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto c = input[i];
        if ((c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c) != key[i]) {
            return false;
        }
    }
    return true;
}
} // namespace

std::span<const InstructionSpec> instructionCatalog() {
    return Specs;
}

InstructionLookupResult findInstruction(std::string_view mnemonic, SourcePosition position) {
    const auto original = mnemonic;
    const bool extended = mnemonic.starts_with('+');
    if (extended) {
        mnemonic.remove_prefix(1);
    }
    for (const auto& spec : Specs) {
        if (matches(mnemonic, spec.mnemonic)) {
            if (extended && !spec.supportsExtended()) {
                return InstructionDiagnostic{InstructionDiagnosticCode::InvalidExtendedFormat,
                                             {std::string{original}, position},
                                             "Esta instrucao nao admite formato 4."};
            }
            return std::cref(spec);
        }
    }
    return InstructionDiagnostic{InstructionDiagnosticCode::UnknownInstruction,
                                 {std::string{original}, position},
                                 "Instrucao fora do recorte da ISA."};
}

InstructionLookupResult findInstruction(const SourceIdentifier& operation) {
    auto result = findInstruction(operation.key, operation.source.position);
    if (auto* error = std::get_if<InstructionDiagnostic>(&result)) {
        error->source = operation.source;
    }
    return result;
}

AssemblyDiagnostic toAssemblyDiagnostic(const InstructionDiagnostic& diagnostic) {
    auto code = AssemblyDiagnosticCode::InvalidOperation;
    switch (diagnostic.code) {
        case InstructionDiagnosticCode::MissingOperand:
        case InstructionDiagnosticCode::MissingValue:
            code = AssemblyDiagnosticCode::EmptyOperand;
            break;
        case InstructionDiagnosticCode::UnexpectedOperand:
        case InstructionDiagnosticCode::WrongOperandCount:
        case InstructionDiagnosticCode::InvalidRegister:
        case InstructionDiagnosticCode::InvalidIndexing:
        case InstructionDiagnosticCode::InvalidFormat2Operand:
            code = AssemblyDiagnosticCode::UnexpectedField;
            break;
        case InstructionDiagnosticCode::InvalidShiftCount:
            code = AssemblyDiagnosticCode::ValueOutOfRange;
            break;
        case InstructionDiagnosticCode::UnsupportedOperandSyntax:
            code = AssemblyDiagnosticCode::UnsupportedExpression;
            break;
        case InstructionDiagnosticCode::InvalidValue:
            code = AssemblyDiagnosticCode::InvalidInteger;
            break;
        case InstructionDiagnosticCode::NegativeImmediate:
            code = AssemblyDiagnosticCode::NegativeValueNotAllowed;
            break;
        case InstructionDiagnosticCode::UnknownInstruction:
        case InstructionDiagnosticCode::InvalidExtendedFormat:
        case InstructionDiagnosticCode::InvalidSourceLine:
        case InstructionDiagnosticCode::InstructionMismatch:
            break;
    }
    return {code, diagnostic.source.position, diagnostic.message};
}
} // namespace assembler
