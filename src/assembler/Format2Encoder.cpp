#include "Format2Encoder.hpp"

namespace assembler {
Format2Result encodeFormat2(const InstructionSpec& spec, const InstructionOperand& operand) {
    const auto reject = [](InstructionDiagnosticCode code,
                           const SourceField& source) -> Format2Result {
        return InstructionDiagnostic{
            code, source, "Operando ou descritor invalido para formato 2."};
    };
    const auto found = findInstruction(operand.operation.text, operand.operation.position);
    if (const auto* failure = std::get_if<InstructionDiagnostic>(&found)) {
        return *failure;
    }
    if (std::get<std::reference_wrapper<const InstructionSpec>>(found).get() != spec) {
        return reject(InstructionDiagnosticCode::InstructionMismatch, operand.operation);
    }
    if (spec.formats != InstructionFormats::Format2 || operand.form != InstructionForm::Format2) {
        return reject(InstructionDiagnosticCode::InvalidFormat2Operand, operand.operation);
    }
    const RegisterToken* first = nullptr;
    const RegisterToken* second = nullptr;
    std::uint8_t low = 0;
    if (const auto* single = std::get_if<OneRegisterOperand>(&operand.data);
        single && spec.operands == OperandPattern::OneRegister) {
        first = &single->first;
    } else if (const auto* pair = std::get_if<TwoRegisterOperand>(&operand.data);
               pair && spec.operands == OperandPattern::TwoRegisters) {
        first = &pair->first;
        second = &pair->second;
        low = static_cast<std::uint8_t>(second->reg);
    } else if (const auto* shift = std::get_if<RegisterCountOperand>(&operand.data);
               shift && spec.operands == OperandPattern::RegisterAndCount) {
        first = &shift->first;
        if (shift->count < 1 || shift->count > 16) {
            return reject(InstructionDiagnosticCode::InvalidShiftCount, shift->countSource);
        }
        low = static_cast<std::uint8_t>(shift->count - 1);
    } else {
        return reject(InstructionDiagnosticCode::InvalidFormat2Operand, operand.operation);
    }
    if (!isInstructionRegister(first->reg)) {
        return reject(InstructionDiagnosticCode::InvalidRegister, first->source);
    }
    if (second && !isInstructionRegister(second->reg)) {
        return reject(InstructionDiagnosticCode::InvalidRegister, second->source);
    }
    return Format2Bytes{
        spec.opcode, static_cast<std::uint8_t>((static_cast<std::uint8_t>(first->reg) << 4) | low)};
}
} // namespace assembler
