#pragma once

#include "Instruction.hpp"
#include "Memory.hpp"

#include <cstdint>

class Decoder {
public:
    Decoder() = default;

    DecodeResult decode(const Memory& memory, std::uint32_t address) const;
};
