#include "Executor.hpp"

#include "execution/ArithmeticOperations.hpp"
#include "execution/ControlOperations.hpp"
#include "execution/DataOperations.hpp"

#include <variant>

namespace {

ExecuteResult normalizeProgramCounterWrite(ExecuteResult result) {
    auto* effect = std::get_if<ExecutionEffect>(&result);
    if (effect == nullptr || !effect->registerWrite ||
        effect->registerWrite->target != RegisterId::PC) {
        return result;
    }

    effect->nextPc = effect->registerWrite->value;
    effect->registerWrite.reset();
    return result;
}

} // namespace

ExecuteResult Executor::execute(const DecodedInstruction& instruction,
                                const ResolvedOperand& operand,
                                std::uint32_t postFetchPc,
                                const Memory& memory,
                                const RegisterFile& registers) const {
    switch (instruction.opcode) {
        case Opcode::J:
        case Opcode::JEQ:
        case Opcode::JGT:
        case Opcode::JLT:
        case Opcode::JSUB:
        case Opcode::RSUB:
            return normalizeProgramCounterWrite(
                executeControlOperation(instruction, operand, postFetchPc, memory, registers));

        case Opcode::CLEAR:
        case Opcode::RMO:
        case Opcode::LDA:
        case Opcode::LDB:
        case Opcode::LDL:
        case Opcode::LDS:
        case Opcode::LDT:
        case Opcode::LDX:
        case Opcode::STA:
        case Opcode::STB:
        case Opcode::STL:
        case Opcode::STS:
        case Opcode::STT:
        case Opcode::STX:
        case Opcode::LDCH:
        case Opcode::STCH:
        case Opcode::AND:
        case Opcode::OR:
            return normalizeProgramCounterWrite(
                executeDataOperation(instruction, operand, postFetchPc, memory, registers));

        case Opcode::ADD:
        case Opcode::ADDR:
        case Opcode::SUB:
        case Opcode::SUBR:
        case Opcode::MUL:
        case Opcode::MULR:
        case Opcode::DIV:
        case Opcode::DIVR:
        case Opcode::COMP:
        case Opcode::COMPR:
        case Opcode::SHIFTL:
        case Opcode::SHIFTR:
        case Opcode::TIX:
        case Opcode::TIXR:
            return normalizeProgramCounterWrite(
                executeArithmeticOperation(instruction, operand, postFetchPc, memory, registers));
    }

    return ExecuteFault{ExecuteFaultCode::UnsupportedOpcode, instruction.instructionAddress};
}
