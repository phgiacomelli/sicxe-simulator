#pragma once

#include "AssemblyResult.hpp"
#include "ObjectPassOne.hpp"
#include "linker/ObjectModule.hpp"

#include <variant>

namespace assembler {

using ObjectAssemblyResult = std::variant<linker::ObjectModule, AssemblyFailure>;

class ObjectPassTwo {
public:
    // Consumes a successful, unmodified first-pass snapshot. No I/O or partial output.
    static ObjectAssemblyResult run(const ObjectPassOneResult& passOne);
};

} // namespace assembler
