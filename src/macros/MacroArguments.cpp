#include "MacroArguments.hpp"

#include "assembler/SymbolTable.hpp"

#include <utility>

namespace macro {
namespace {

bool horizontalSpace(char value) {
    return value == ' ' || value == '\t';
}

MacroDiagnostic diagnostic(MacroDiagnosticCode code, SourcePosition position, std::string message) {
    return {code, position, std::move(message), {}};
}

SourcePosition linePosition(const SourceLine& line) {
    if (line.operation) {
        return line.operation->position;
    }
    if (line.label) {
        return line.label->position;
    }
    return {line.number, 1};
}

SourceField slice(const SourceField& field, std::size_t begin, std::size_t end) {
    while (begin < end && horizontalSpace(field.text[begin])) {
        ++begin;
    }
    while (end > begin && horizontalSpace(field.text[end - 1])) {
        --end;
    }
    return {field.text.substr(begin, end - begin),
            {field.position.line, field.position.column + begin}};
}

std::optional<SourcePosition> findOpenQuote(const SourceField& field) {
    std::optional<std::size_t> openOffset;
    for (std::size_t index = 0; index < field.text.size(); ++index) {
        if (field.text[index] == '\'') {
            if (openOffset) {
                openOffset.reset();
            } else {
                openOffset = index;
            }
        }
    }
    if (openOffset) {
        return SourcePosition{field.position.line, field.position.column + *openOffset};
    }
    return std::nullopt;
}

bool containsOutsideQuotes(std::string_view text, char target) {
    bool inQuotes = false;
    for (char value : text) {
        if (value == '\'') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && value == target) {
            return true;
        }
    }
    return false;
}

MacroCallResult parseCallWithPrototype(const SourceLine& line, const MacroPrototype* prototype) {
    if (line.kind != SourceLineKind::Code || !line.operation ||
        line.directive() != MacroDirective::None) {
        return diagnostic(MacroDiagnosticCode::InvalidArgument,
                          linePosition(line),
                          "Chamada exige uma linha de codigo com nome de macro.");
    }
    if (prototype != nullptr) {
        if (assembler::SymbolTable::normalizeName(line.operation->text) != prototype->macroKey) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              line.operation->position,
                              "Chamada nao corresponde ao prototipo da macro.");
        }
    } else if (!assembler::SymbolTable::isValidIdentifier(line.operation->text) ||
               assembler::SymbolTable::isReservedName(line.operation->text)) {
        return diagnostic(MacroDiagnosticCode::InvalidArgument,
                          line.operation->position,
                          "Chamada exige um nome de macro valido no campo de operacao.");
    }

    if (line.label) {
        if (const auto open = findOpenQuote(*line.label)) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              *open,
                              "Aspas simples nao foram fechadas no rotulo da chamada.");
        }
        if (containsOutsideQuotes(line.label->text, '=')) {
            return diagnostic(
                MacroDiagnosticCode::InvalidArgument,
                line.label->position,
                "Parametro por palavra-chave ou valor padrao nao suportado na chamada.");
        }
        if (prototype != nullptr && !prototype->labelFormal) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              line.label->position,
                              "Chamada rotulada para macro sem rotulo formal no prototipo.");
        }
    }

    MacroCall call{
        *line.operation,
        assembler::SymbolTable::normalizeName(line.operation->text),
        line.label,
        {},
    };

    if (line.operand) {
        const auto& operand = *line.operand;
        std::size_t segmentBegin = 0;
        std::optional<std::size_t> openQuoteOffset;
        for (std::size_t index = 0; index <= operand.text.size(); ++index) {
            const bool atEnd = index == operand.text.size();
            if (!atEnd) {
                if (operand.text[index] == '\'') {
                    if (openQuoteOffset) {
                        openQuoteOffset.reset();
                    } else {
                        openQuoteOffset = index;
                    }
                    continue;
                }
                if (operand.text[index] != ',' || openQuoteOffset) {
                    continue;
                }
            }
            if (openQuoteOffset) {
                return diagnostic(
                    MacroDiagnosticCode::InvalidArgument,
                    {operand.position.line, operand.position.column + *openQuoteOffset},
                    "Aspas simples nao foram fechadas nos argumentos da chamada.");
            }
            const auto item = slice(operand, segmentBegin, index);
            if (prototype != nullptr &&
                call.arguments.size() >= prototype->positionalFormals.size()) {
                return diagnostic(MacroDiagnosticCode::InvalidArgument,
                                  item.position,
                                  "Argumentos em excesso na chamada de macro.");
            }
            if (containsOutsideQuotes(item.text, '=')) {
                return diagnostic(
                    MacroDiagnosticCode::InvalidArgument,
                    item.position,
                    "Parametro por palavra-chave ou valor padrao nao suportado na chamada.");
            }
            call.arguments.push_back(item);
            segmentBegin = index + 1;
        }
    }

    if (line.openQuote) {
        return diagnostic(MacroDiagnosticCode::InvalidArgument,
                          *line.openQuote,
                          "Aspas simples nao foram fechadas na chamada de macro.");
    }
    return call;
}

} // namespace

std::optional<std::size_t> MacroPrototype::findFormal(std::string_view name) const {
    if (!assembler::SymbolTable::isValidIdentifier(name)) {
        return std::nullopt;
    }
    const auto key = assembler::SymbolTable::normalizeName(name);
    for (std::size_t index = 0; index < formals.size(); ++index) {
        if (formals[index].key == key) {
            return index;
        }
    }
    return std::nullopt;
}

const BoundMacroArgument* BoundMacroArguments::find(std::string_view formalName) const {
    if (!assembler::SymbolTable::isValidIdentifier(formalName)) {
        return nullptr;
    }
    const auto key = assembler::SymbolTable::normalizeName(formalName);
    for (const auto& argument : arguments) {
        if (argument.formal.key == key) {
            return &argument;
        }
    }
    return nullptr;
}

MacroPrototypeResult MacroArguments::parsePrototype(const SourceLine& line) {
    if (line.kind != SourceLineKind::Code || !line.operation ||
        line.directive() != MacroDirective::None) {
        return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                          linePosition(line),
                          "Prototipo exige um nome de macro valido no campo de operacao.");
    }
    if (!assembler::SymbolTable::isValidIdentifier(line.operation->text)) {
        return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                          line.operation->position,
                          "Prototipo exige um nome de macro valido no campo de operacao.");
    }
    if (assembler::SymbolTable::isReservedName(line.operation->text)) {
        return diagnostic(MacroDiagnosticCode::ReservedName,
                          line.operation->position,
                          "Nome de macro reservado pelo montador.");
    }

    MacroPrototype prototype{
        *line.operation,
        assembler::SymbolTable::normalizeName(line.operation->text),
        std::nullopt,
        {},
        {},
    };

    if (line.label) {
        if (!assembler::SymbolTable::isValidIdentifier(line.label->text)) {
            return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                              line.label->position,
                              "Rotulo formal invalido no prototipo.");
        }
        prototype.labelFormal = *line.label;
        prototype.formals.push_back(MacroFormal{
            MacroFormalKind::Label,
            *line.label,
            assembler::SymbolTable::normalizeName(line.label->text),
        });
    }

    if (line.operand) {
        const auto& operand = *line.operand;
        std::size_t segmentBegin = 0;
        std::optional<std::size_t> openQuoteOffset;
        for (std::size_t index = 0; index <= operand.text.size(); ++index) {
            const bool atEnd = index == operand.text.size();
            if (!atEnd) {
                if (operand.text[index] == '\'') {
                    if (openQuoteOffset) {
                        openQuoteOffset.reset();
                    } else {
                        openQuoteOffset = index;
                    }
                    continue;
                }
                if (operand.text[index] != ',' || openQuoteOffset) {
                    continue;
                }
            }
            if (openQuoteOffset) {
                return diagnostic(
                    MacroDiagnosticCode::InvalidPrototype,
                    {operand.position.line, operand.position.column + *openQuoteOffset},
                    "Aspas simples nao foram fechadas no prototipo.");
            }
            const auto item = slice(operand, segmentBegin, index);
            if (!assembler::SymbolTable::isValidIdentifier(item.text)) {
                return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                                  item.position,
                                  "Parametro formal invalido no prototipo.");
            }
            auto key = assembler::SymbolTable::normalizeName(item.text);
            if (prototype.findFormal(key).has_value()) {
                return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                                  item.position,
                                  "Parametro formal repetido no prototipo.");
            }
            prototype.positionalFormals.push_back(item);
            prototype.formals.push_back(
                MacroFormal{MacroFormalKind::Positional, item, std::move(key)});
            segmentBegin = index + 1;
        }
    }

    if (line.openQuote) {
        return diagnostic(MacroDiagnosticCode::InvalidPrototype,
                          *line.openQuote,
                          "Aspas simples nao foram fechadas no prototipo.");
    }
    return prototype;
}

MacroPrototypeResult MacroArguments::parsePrototype(const MacroDefinition& definition) {
    return parsePrototype(definition.prototype());
}

MacroCallResult MacroArguments::parseCall(const SourceLine& line) {
    return parseCallWithPrototype(line, nullptr);
}

MacroArgumentBindResult MacroArguments::bind(const MacroPrototype& prototype,
                                             const MacroCall& call) {
    if (call.macroKey != prototype.macroKey) {
        return diagnostic(MacroDiagnosticCode::InvalidArgument,
                          call.macroName.position,
                          "Chamada nao corresponde ao prototipo da macro.");
    }
    if (call.label) {
        if (const auto open = findOpenQuote(*call.label)) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              *open,
                              "Aspas simples nao foram fechadas no rotulo da chamada.");
        }
        if (containsOutsideQuotes(call.label->text, '=')) {
            return diagnostic(
                MacroDiagnosticCode::InvalidArgument,
                call.label->position,
                "Parametro por palavra-chave ou valor padrao nao suportado na chamada.");
        }
        if (!prototype.labelFormal) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              call.label->position,
                              "Chamada rotulada para macro sem rotulo formal no prototipo.");
        }
    }

    for (std::size_t index = 0; index < call.arguments.size(); ++index) {
        const auto& item = call.arguments[index];
        if (const auto open = findOpenQuote(item)) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              *open,
                              "Aspas simples nao foram fechadas nos argumentos da chamada.");
        }
        if (index >= prototype.positionalFormals.size()) {
            return diagnostic(MacroDiagnosticCode::InvalidArgument,
                              item.position,
                              "Argumentos em excesso na chamada de macro.");
        }
        if (containsOutsideQuotes(item.text, '=')) {
            return diagnostic(
                MacroDiagnosticCode::InvalidArgument,
                item.position,
                "Parametro por palavra-chave ou valor padrao nao suportado na chamada.");
        }
    }

    BoundMacroArguments bound;
    bound.macroName = call.macroName;
    bound.positionalValues.reserve(prototype.positionalFormals.size());
    bound.values.reserve(prototype.formals.size());
    bound.arguments.reserve(prototype.formals.size());

    if (prototype.labelFormal) {
        std::string value = call.label ? call.label->text : std::string{};
        const std::optional<SourcePosition> position =
            call.label ? std::optional<SourcePosition>{call.label->position} : std::nullopt;
        bound.labelValue = value;
        bound.values.push_back(value);
        bound.arguments.push_back(
            BoundMacroArgument{prototype.formals.front(), std::move(value), position});
    }

    const std::size_t formalOffset = prototype.labelFormal ? 1 : 0;
    for (std::size_t index = 0; index < prototype.positionalFormals.size(); ++index) {
        const bool provided = index < call.arguments.size();
        std::string value = provided ? call.arguments[index].text : std::string{};
        const std::optional<SourcePosition> position =
            provided ? std::optional<SourcePosition>{call.arguments[index].position} : std::nullopt;
        bound.positionalValues.push_back(value);
        bound.values.push_back(value);
        bound.arguments.push_back(BoundMacroArgument{
            prototype.formals[formalOffset + index], std::move(value), position});
    }

    return bound;
}

MacroArgumentBindResult MacroArguments::bind(const MacroPrototype& prototype,
                                             const SourceLine& callLine) {
    const auto parsedCall = parseCallWithPrototype(callLine, &prototype);
    if (const auto* failure = std::get_if<MacroDiagnostic>(&parsedCall)) {
        return *failure;
    }
    return bind(prototype, std::get<MacroCall>(parsedCall));
}

MacroArgumentBindResult MacroArguments::bind(const SourceLine& prototypeLine,
                                             const SourceLine& callLine) {
    const auto parsedPrototype = parsePrototype(prototypeLine);
    if (const auto* failure = std::get_if<MacroDiagnostic>(&parsedPrototype)) {
        return *failure;
    }
    return bind(std::get<MacroPrototype>(parsedPrototype), callLine);
}

MacroArgumentBindResult MacroArguments::bind(const MacroDefinition& definition,
                                             const SourceLine& callLine) {
    return bind(definition.prototype(), callLine);
}

} // namespace macro
