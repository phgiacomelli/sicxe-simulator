#pragma once

#include "ProgramInput.hpp"

#include <string_view>

class HexParser {
public:
    static ProgramInputResult
    parse(std::string_view text, std::uint32_t loadAddress, std::uint32_t initialPc);
};
