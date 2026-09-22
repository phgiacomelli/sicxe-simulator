#ifndef SICXE_DATA_OPERATIONS_HPP
#define SICXE_DATA_OPERATIONS_HPP

#include "../ExecutionEffect.hpp"
#include "../Instruction.hpp"
#include "../Memory.hpp"
#include "../Operand.hpp"
#include "../RegisterFile.hpp"

#include <cstdint>

ExecuteResult executeDataOperation(const DecodedInstruction& instruction,
                                   const ResolvedOperand& operand,
                                   std::uint32_t postFetchPc,
                                   const Memory& memory,
                                   const RegisterFile& registers);

#endif