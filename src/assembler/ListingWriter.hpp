#pragma once

#include "AssemblyResult.hpp"

#include <string>
#include <string_view>

namespace assembler {

class ListingWriter {
public:
    static std::string render(const AssemblySuccess& program);
    static std::string diagnostics(const AssemblyFailure& failure, std::string_view sourceName);
};

} // namespace assembler
