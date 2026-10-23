#include "DefinitionCapture.hpp"

#include "assembler/SymbolTable.hpp"

#include <optional>
#include <utility>

namespace macro {
namespace {

MacroDiagnostic diagnostic(MacroDiagnosticCode code, SourcePosition position, std::string message) {
    return {code, position, std::move(message), {}};
}

SourcePosition linePosition(const SourceLine& line) {
    if (line.operation)
        return line.operation->position;
    if (line.label)
        return line.label->position;
    return {line.number, 1};
}

std::optional<MacroDiagnostic> directiveFields(const SourceLine& line) {
    if (line.label || line.operand) {
        return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                          line.label ? line.label->position : line.operand->position,
                          "Diretiva de macro nao aceita rotulo ou operando.");
    }
    return std::nullopt;
}

} // namespace

DefinitionCaptureResult DefinitionCapture::capture(std::span<const SourceLine> lines,
                                                   std::size_t beginIndex) {
    if (beginIndex >= lines.size()) {
        return diagnostic(MacroDiagnosticCode::MissingPrototype,
                          {lines.empty() ? 1 : lines.back().number + 1, 1},
                          "Definicao ausente no fim da entrada.");
    }
    const auto& begin = lines[beginIndex];
    if (begin.directive() == MacroDirective::DefinitionEnd) {
        return diagnostic(MacroDiagnosticCode::UnexpectedEnd,
                          linePosition(begin),
                          "MCEND sem MCDEFN correspondente.");
    }
    if (begin.directive() != MacroDirective::DefinitionBegin) {
        return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                          linePosition(begin),
                          "Captura exige uma linha MCDEFN.");
    }
    if (const auto invalid = directiveFields(begin))
        return *invalid;

    auto prototypeIndex = beginIndex + 1;
    while (prototypeIndex < lines.size() && lines[prototypeIndex].kind != SourceLineKind::Code)
        ++prototypeIndex;
    // MCDEFN/MCEND cannot be macro names: here either means a missing prototype.
    if (prototypeIndex == lines.size() ||
        lines[prototypeIndex].directive() != MacroDirective::None) {
        return diagnostic(MacroDiagnosticCode::MissingPrototype,
                          linePosition(begin),
                          "MCDEFN exige um prototipo na proxima linha ativa.");
    }
    const auto& prototype = lines[prototypeIndex];
    if (!prototype.operation ||
        !assembler::SymbolTable::isValidIdentifier(prototype.operation->text)) {
        return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                          linePosition(prototype),
                          "Prototipo exige um nome de macro valido no campo de operacao.");
    }
    if (assembler::SymbolTable::isReservedName(prototype.operation->text)) {
        return diagnostic(MacroDiagnosticCode::ReservedName,
                          prototype.operation->position,
                          "Nome de macro reservado pelo montador.");
    }

    std::size_t level = 1;
    for (auto index = prototypeIndex + 1; index < lines.size(); ++index) {
        const auto& line = lines[index];
        const auto directive = line.directive();
        if (directive == MacroDirective::None)
            continue;
        if (const auto invalid = directiveFields(line))
            return *invalid;
        if (directive == MacroDirective::DefinitionBegin) {
            ++level;
        } else if (--level == 0) {
            std::vector<SourceLine> body(lines.begin() + prototypeIndex + 1, lines.begin() + index);
            return CapturedDefinition{MacroDefinition{begin, prototype, std::move(body), line},
                                      index + 1};
        }
    }
    return diagnostic(MacroDiagnosticCode::UnterminatedDefinition,
                      linePosition(begin),
                      "Definicao sem MCEND correspondente.");
}

} // namespace macro
