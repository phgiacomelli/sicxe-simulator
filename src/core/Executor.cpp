#include "Executor.hpp"

#include "execution/ArithmeticOperations.hpp"
#include "execution/ControlOperations.hpp"
#include "execution/DataOperations.hpp"

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
            return executeControlOperation(instruction, operand, postFetchPc, memory, registers);

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
            return executeDataOperation(instruction, operand, postFetchPc, memory, registers);

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
            return executeArithmeticOperation(instruction, operand, postFetchPc, memory, registers);
    }

    return ExecuteFault{ExecuteFaultCode::UnsupportedOpcode, instruction.instructionAddress};
}
