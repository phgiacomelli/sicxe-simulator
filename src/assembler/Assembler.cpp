#include "Assembler.hpp"

#include "PassOne.hpp"
#include "PassTwo.hpp"
#include "SourceParser.hpp"

namespace assembler {

AssemblyResult assemble(std::string_view sourceText) {
    const auto parsed = SourceParser::parse(sourceText);
    if (!parsed.succeeded())
        return AssemblyFailure{parsed.diagnostics};
    return PassTwo::run(PassOne::run(parsed));
}

} // namespace assembler
