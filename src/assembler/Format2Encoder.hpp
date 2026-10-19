#pragma once

#include "InstructionOperand.hpp"

#include <array>
#include <variant>

namespace assembler {
using Format2Bytes = std::array<std::uint8_t, 2>;
using Format2Result = std::variant<Format2Bytes, InstructionDiagnostic>;
Format2Result encodeFormat2(const InstructionSpec& spec, const InstructionOperand& operand);
} // namespace assembler
