#pragma once

#include "Instruction.hpp"
#include "Memory.hpp"
#include "Operand.hpp"
#include "RegisterFile.hpp"

#include <cstdint>

class AddressResolver {
public:
    AddressResolver() = default;

    ResolveResult resolve(const DecodedInstruction& instruction,
                          std::uint32_t postFetchPc,
                          const Memory& memory,
                          const RegisterFile& registers) const;
};
