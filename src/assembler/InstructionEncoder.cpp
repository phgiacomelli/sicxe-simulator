#include "InstructionEncoder.hpp"

#include "AddressLimits.hpp"
#include "Format2Encoder.hpp"
#include "InstructionOperand.hpp"

#include <utility>

namespace assembler {
namespace {

InstructionEncodingDiagnostic
failure(InstructionEncodingCode code, SourcePosition position, std::string message) {
    return {code, position, std::move(message), std::monostate{}};
}

InstructionEncodingDiagnostic failure(const InstructionDiagnostic& diagnostic) {
    return {InstructionEncodingCode::InvalidInstruction,
            diagnostic.source.position,
            diagnostic.message,
            diagnostic};
}

InstructionEncodingDiagnostic failure(const OperandDiagnostic& diagnostic) {
    return {InstructionEncodingCode::InvalidOperand,
            diagnostic.position,
            diagnostic.message,
            diagnostic};
}

LocatedInstructionFormat locatedFormat(InstructionForm form) {
    switch (form) {
        case InstructionForm::Format2:
            return LocatedInstructionFormat::Format2;
        case InstructionForm::Format3:
            return LocatedInstructionFormat::Format3;
        case InstructionForm::Format4:
            return LocatedInstructionFormat::Format4;
    }
    // Only forms produced by parseInstructionOperand reach this function.
    return LocatedInstructionFormat::Format3;
}

} // namespace

AssemblyDiagnostic toAssemblyDiagnostic(const InstructionEncodingDiagnostic& diagnostic) {
    if (const auto* instruction = std::get_if<InstructionDiagnostic>(&diagnostic.cause))
        return toAssemblyDiagnostic(*instruction);
    if (const auto* operand = std::get_if<OperandDiagnostic>(&diagnostic.cause))
        return toAssemblyDiagnostic(*operand);
    const bool rangeError = diagnostic.code == InstructionEncodingCode::InvalidBaseState ||
                            diagnostic.code == InstructionEncodingCode::AddressOutOfRange ||
                            diagnostic.code == InstructionEncodingCode::DisplacementOutOfRange;
    return {rangeError ? AssemblyDiagnosticCode::ValueOutOfRange
                       : AssemblyDiagnosticCode::InvalidOperation,
            diagnostic.position,
            diagnostic.message};
}

InstructionEncodingResult
encodeInstruction(const LocatedLine& line, const SymbolTable& symbols, BaseState baseState) {
    if (line.source.kind != SourceLineKind::Statement || !line.source.statement) {
        return failure(InstructionEncodingCode::InvalidSourceLine,
                       {line.source.number, 1},
                       "Codificacao exige linha de fonte sintaticamente valida.");
    }
    const auto& operation = line.source.statement->operation;
    if (line.category != LocatedLineCategory::Instruction) {
        return failure(InstructionEncodingCode::InvalidInstruction,
                       operation.source.position,
                       "Linha localizada nao representa uma instrucao.");
    }
    if (SymbolTable::normalizeName(operation.source.text) != operation.key) {
        return failure(InstructionEncodingCode::LayoutMismatch,
                       operation.source.position,
                       "Chave da operacao diverge da grafia original da fonte.");
    }
    const auto lookup = findInstruction(operation);
    if (const auto* diagnostic = std::get_if<InstructionDiagnostic>(&lookup))
        return failure(*diagnostic);
    const auto& spec = std::get<std::reference_wrapper<const InstructionSpec>>(lookup).get();
    const auto parsed = parseInstructionOperand(spec, line.source);
    if (const auto* diagnostic = std::get_if<InstructionDiagnostic>(&parsed))
        return failure(*diagnostic);
    const auto& operand = std::get<InstructionOperand>(parsed);
    if (line.mnemonic != spec.mnemonic || line.opcode != spec.opcode ||
        line.format != locatedFormat(operand.form) || line.sizeBytes != operand.size() ||
        line.extended != (operand.form == InstructionForm::Format4) || line.directive) {
        return failure(InstructionEncodingCode::LayoutMismatch,
                       operation.source.position,
                       "Instrucao diverge do formato, opcode ou tamanho fixado na passagem 1.");
    }
    if (line.address > MaxAddressableByte || line.sizeBytes > AddressSpaceBytes - line.address) {
        return failure(InstructionEncodingCode::AddressOutOfRange,
                       operation.source.position,
                       "Instrucao excede o espaco enderecavel de 20 bits.");
    }
    if (baseState && *baseState > MaxAddressableByte) {
        return failure(InstructionEncodingCode::InvalidBaseState,
                       operation.source.position,
                       "Estado de BASE fora do espaco enderecavel de 20 bits.");
    }
    if (operand.form == InstructionForm::Format2) {
        const auto result = encodeFormat2(spec, operand);
        if (const auto* diagnostic = std::get_if<InstructionDiagnostic>(&result))
            return failure(*diagnostic);
        const auto& bytes = std::get<Format2Bytes>(result);
        return EncodedInstruction{{bytes.begin(), bytes.end()}};
    }
    if (spec.operands == OperandPattern::None) {
        return EncodedInstruction{{static_cast<std::uint8_t>(spec.opcode | 3U), 0, 0}};
    }
    const auto& memory = std::get<MemoryInstructionOperand>(operand.data);
    const auto parsedValue = parseValue(memory.value);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedValue))
        return failure(*diagnostic);
    const auto& value = std::get<OperandValue>(parsedValue);
    const auto resolved = resolveOperand(value, symbols, Address20BitRange);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&resolved))
        return failure(*diagnostic);
    const auto target = std::get<std::int64_t>(resolved);
    // resolveOperand applies its range to integers only; also bound symbol addresses.
    if (target < 0 || target > MaxAddressableByte) {
        return failure(InstructionEncodingCode::AddressOutOfRange,
                       memory.value.position,
                       "Endereco do simbolo fora do espaco enderecavel de 20 bits.");
    }

    const std::uint8_t ni = memory.mode == InstructionAddressing::Immediate  ? 1
                            : memory.mode == InstructionAddressing::Indirect ? 2
                                                                             : 3;
    const auto first = static_cast<std::uint8_t>(spec.opcode | ni);
    std::uint8_t flags = memory.indexed ? 0x80 : 0;
    if (operand.form == InstructionForm::Format4) {
        const auto address = static_cast<std::uint32_t>(target);
        return EncodedInstruction{{first,
                                   static_cast<std::uint8_t>(flags | 0x10U | (address >> 16)),
                                   static_cast<std::uint8_t>(address >> 8),
                                   static_cast<std::uint8_t>(address)}};
    }

    std::uint32_t field = 0;
    if (std::holds_alternative<IntegerOperand>(value)) {
        const auto direct = validateInteger(value, Address12BitRange);
        if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&direct)) {
            auto error = *diagnostic;
            error.message += " Use '+' para solicitar formato 4 explicitamente.";
            return failure(error);
        }
        field = static_cast<std::uint32_t>(std::get<std::int64_t>(direct));
    } else {
        // Widen before arithmetic: no unsigned subtraction and no 20-bit PC wrap.
        const auto pc = static_cast<std::int64_t>(line.address) + operand.size();
        const auto displacement = target - pc;
        if (displacement >= -2048 && displacement <= 2047) {
            flags |= 0x20;
            field = static_cast<std::uint32_t>(displacement) & 0xFFFU;
        } else if (baseState && target >= *baseState && target - *baseState <= 4095) {
            flags |= 0x40;
            field = static_cast<std::uint32_t>(target - *baseState);
        } else {
            return failure(InstructionEncodingCode::DisplacementOutOfRange,
                           memory.value.position,
                           "Alvo fora do alcance de PC/BASE; use '+' para solicitar formato 4.");
        }
    }
    return EncodedInstruction{
        {first, static_cast<std::uint8_t>(flags | (field >> 8)), static_cast<std::uint8_t>(field)}};
}

} // namespace assembler
