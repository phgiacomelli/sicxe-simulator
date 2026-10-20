#pragma once

#include "AssemblyResult.hpp"
#include "PassOne.hpp"

namespace assembler {

class PassTwo {
public:
    static AssemblyResult run(const PassOneResult& passOne);
};

} // namespace assembler
