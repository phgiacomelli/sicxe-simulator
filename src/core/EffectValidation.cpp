#include "EffectValidation.hpp"

#include <variant>

namespace {

bool isKnownRegister(RegisterId id) {
    return registerIdFromCode(static_cast<std::uint8_t>(id)) == id;
}

bool isKnownConditionCode(ConditionCode conditionCode) {
    switch (conditionCode) {
        case ConditionCode::Less:
        case ConditionCode::Equal:
        case ConditionCode::Greater:
            return true;
    }
    return false;
}

bool isValidPc(std::uint32_t pc, const Memory& memory) {
    return pc <= WordMask && pc < memory.size();
}

std::optional<CycleFault> validateMemoryWrite(const MemoryWrite& write, const Memory& memory) {
    std::uint32_t address = 0;
    std::uint64_t width = 0;
    if (const auto* byteWrite = std::get_if<ByteWrite>(&write)) {
        address = byteWrite->address;
        width = 1;
    } else {
        address = std::get<WordWrite>(write).address;
        width = 3;
    }

    if (static_cast<std::uint64_t>(address) + width > memory.size()) {
        return CycleFault{CycleFaultCode::MemoryWriteOutOfRange, address};
    }
    return std::nullopt;
}

} // namespace

std::optional<CycleFault> validateEffect(const ExecutionEffect& effect, const Memory& memory) {
    if (effect.registerWrite && !isKnownRegister(effect.registerWrite->target)) {
        return CycleFault{CycleFaultCode::InvalidRegisterWrite,
                          static_cast<std::int64_t>(effect.registerWrite->target)};
    }

    if (effect.conditionCode && !isKnownConditionCode(*effect.conditionCode)) {
        return CycleFault{CycleFaultCode::InvalidConditionCode,
                          static_cast<std::int64_t>(*effect.conditionCode)};
    }

    // O Executor normaliza escritas em PC para nextPc. Uma escrita em SW e uma atualização de CC
    // alteram os mesmos bits.
    if (effect.registerWrite) {
        const bool hasUnnormalizedPcWrite = effect.registerWrite->target == RegisterId::PC;
        const bool conflictsWithConditionCode =
            effect.registerWrite->target == RegisterId::SW && effect.conditionCode.has_value();
        if (hasUnnormalizedPcWrite || conflictsWithConditionCode) {
            return CycleFault{CycleFaultCode::ConflictingEffect};
        }
    }

    if (effect.memoryWrite) {
        if (const auto fault = validateMemoryWrite(*effect.memoryWrite, memory)) {
            return fault;
        }
    }

    if (!isValidPc(effect.nextPc, memory)) {
        return CycleFault{CycleFaultCode::NextPcOutOfRange, effect.nextPc};
    }

    return std::nullopt;
}
