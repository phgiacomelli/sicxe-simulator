#include "ObjectAssembler.hpp"

namespace assembler {

ObjectAssemblyResult assembleObject(std::string_view sourceText) {
    return ObjectPassTwo::run(ObjectPassOne::run(sourceText));
}

} // namespace assembler
