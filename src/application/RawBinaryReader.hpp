#pragma once

#include "ProgramInput.hpp"

#include <filesystem>
#include <istream>

class RawBinaryReader {
public:
    static ProgramInputResult
    read(const std::filesystem::path& path, std::uint32_t loadAddress, std::uint32_t initialPc);
    static ProgramInputResult
    read(std::istream& stream, std::uint32_t loadAddress, std::uint32_t initialPc);
};
