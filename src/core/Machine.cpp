#include "Machine.hpp"

#include "EffectValidation.hpp"

#include <stdexcept>
#include <utility>

Machine::Machine(std::size_t memorySize) : memory_(memorySize) {
}

LoadResult Machine::loadProgram(std::span<const std::uint8_t> bytes,
                                std::uint32_t loadAddress,
                                std::uint32_t initialPc) {
    const std::size_t memorySize = memory_.size();

    // Toda validação ocorre antes da primeira alteração de estado.
    if (bytes.empty()) {
        return LoadFault{LoadFaultCode::EmptyProgram};
    }
    if (loadAddress >= memorySize) {
        return LoadFault{LoadFaultCode::LoadAddressOutOfRange, loadAddress};
    }
    if (bytes.size() > memorySize - loadAddress) {
        const std::uint64_t end = static_cast<std::uint64_t>(loadAddress) + bytes.size();
        return LoadFault{LoadFaultCode::ProgramExceedsMemory, static_cast<std::int64_t>(end)};
    }
    if (initialPc > WordMask || initialPc >= memorySize) {
        return LoadFault{LoadFaultCode::InitialPcOutOfRange, initialPc};
    }

    reset();
    for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
        memory_.writeByte(loadAddress + offset, bytes[offset]);
    }
    registers_.setPC(initialPc);

    return LoadSuccess{loadAddress, bytes.size(), initialPc};
}

void Machine::reset() {
    memory_ = Memory{memory_.size()};
    registers_.reset();
    lastFault_.reset();
}

StepResult Machine::step() {
    // 1. Captura do PC da instrução.
    const std::uint32_t instructionPc = registers_.getPC();

    // 2. Decodificação: somente leitura de memória.
    const DecodeResult decoded = decoder_.decode(memory_, instructionPc);
    if (const auto* decodeFault = std::get_if<DecodeFault>(&decoded)) {
        return fail(MachineFault{instructionPc, *decodeFault});
    }
    const auto& instruction = std::get<DecodedInstruction>(decoded);

    // 3. PC pós-fetch em tipo largo, sem wrap.
    const auto postFetchPc = computePostFetchPc(instruction.instructionAddress, instruction.length);
    if (!postFetchPc) {
        const std::int64_t end = instruction.instructionAddress + instruction.length;
        const CycleFault cycleFault{CycleFaultCode::PostFetchPcOutOfRange, end};
        return fail(MachineFault{instructionPc, cycleFault});
    }
    if (*postFetchPc >= memory_.size()) {
        const CycleFault cycleFault{CycleFaultCode::NextPcOutOfRange, *postFetchPc};
        return fail(MachineFault{instructionPc, cycleFault});
    }

    // Após o fetch, um registrador PC lido pela instrução já aponta para a próxima instrução,
    // como no cálculo PC-relative. O registrador real só é alterado no commit.
    RegisterFile executionRegisters = registers_;
    executionRegisters.setPC(*postFetchPc);

    // 4. Resolução do operando.
    const ResolveResult resolved =
        addressResolver_.resolve(instruction, *postFetchPc, memory_, executionRegisters);
    if (const auto* resolveFault = std::get_if<ResolveFault>(&resolved)) {
        return fail(MachineFault{instructionPc, *resolveFault});
    }
    const auto& operand = std::get<ResolvedOperand>(resolved);

    // 5. Execução: produz um efeito sem alterar a máquina.
    const ExecuteResult executed =
        executor_.execute(instruction, operand, *postFetchPc, memory_, executionRegisters);
    if (const auto* executeFault = std::get_if<ExecuteFault>(&executed)) {
        return fail(MachineFault{instructionPc, *executeFault});
    }
    const auto& effect = std::get<ExecutionEffect>(executed);

    // 6. Validação de todos os destinos do efeito.
    if (const auto cycleFault = validateEffect(effect, memory_)) {
        return fail(MachineFault{instructionPc, *cycleFault});
    }

    // 7. Commit: a partir daqui nada pode falhar.
    const std::uint32_t nextPc = commit(effect);
    lastFault_.reset();

    return StepSuccess{instruction, effect, nextPc};
}

MachineSnapshot Machine::snapshot() const {
    MachineSnapshot result;

    result.registers.a = registers_.getA();
    result.registers.x = registers_.getX();
    result.registers.l = registers_.getL();
    result.registers.b = registers_.getB();
    result.registers.s = registers_.getS();
    result.registers.t = registers_.getT();
    result.registers.f = registers_.getF();
    result.registers.pc = registers_.getPC();
    result.registers.sw = registers_.getSW();
    try {
        result.registers.conditionCode = registers_.getConditionCode();
    } catch (const std::logic_error&) {
        // Codificação reservada: o CC fica ausente, mas SW continua disponível.
    }

    result.memory.reserve(memory_.size());
    for (std::size_t address = 0; address < memory_.size(); ++address) {
        result.memory.push_back(memory_.readByte(address));
    }

    result.lastFault = lastFault_;
    return result;
}

StepResult Machine::fail(MachineFault fault) {
    lastFault_ = fault;
    return fault;
}

std::uint32_t Machine::commit(const ExecutionEffect& effect) {
    if (effect.memoryWrite) {
        if (const auto* byteWrite = std::get_if<ByteWrite>(&*effect.memoryWrite)) {
            memory_.writeByte(byteWrite->address, byteWrite->value);
        } else {
            const auto& wordWrite = std::get<WordWrite>(*effect.memoryWrite);
            memory_.writeWord(wordWrite.address, wordWrite.value);
        }
    }

    if (effect.registerWrite) {
        registers_.write(effect.registerWrite->target, effect.registerWrite->value);
    }
    if (effect.conditionCode) {
        registers_.setConditionCode(*effect.conditionCode);
    }

    registers_.setPC(effect.nextPc);
    return effect.nextPc;
}
