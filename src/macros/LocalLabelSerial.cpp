#include "LocalLabelSerial.hpp"

#include "SourceScanner.hpp"
#include "assembler/SymbolTable.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace macro {
namespace {

bool isIdentifierStart(char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

bool isTokenPart(char character) {
    return isIdentifierStart(character) || (character >= '0' && character <= '9') ||
           character == '_' || character == '.' || static_cast<unsigned char>(character) >= 128;
}

struct Replacement {
    std::size_t begin;
    std::size_t length;
    std::string value;
};

MacroDiagnostic failure(MacroDiagnosticCode code,
                        SourcePosition position,
                        std::string message,
                        std::span<const ExpansionFrame> calls) {
    return {code, position, std::move(message), {calls.begin(), calls.end()}};
}

MacroDiagnostic duplicate(std::string_view name,
                          SourcePosition position,
                          SourcePosition previous,
                          std::span<const ExpansionFrame> calls) {
    return failure(
        MacroDiagnosticCode::DuplicateLabel,
        position,
        "Rotulo '" + std::string{name} + "' colide com simbolo definido ou reservado na linha " +
            std::to_string(previous.line) + ", coluna " + std::to_string(previous.column) + ".",
        calls);
}

std::optional<MacroDiagnostic> validateSerial(std::size_t serial,
                                              std::size_t limit,
                                              SourcePosition position,
                                              std::span<const ExpansionFrame> calls) {
    if (serial == 0 || serial > limit)
        return failure(MacroDiagnosticCode::ExpansionLimitExceeded,
                       position,
                       "Serial fora do intervalo permitido (1 ate o limite de expansao).",
                       calls);
    return std::nullopt;
}

SourceLine applyReplacements(const SourceLine& line, std::vector<Replacement>& replacements) {
    if (replacements.empty())
        return line;
    std::sort(replacements.begin(), replacements.end(), [](const auto& left, const auto& right) {
        return left.begin > right.begin;
    });
    auto text = line.text;
    for (const auto& replacement : replacements)
        text.replace(replacement.begin, replacement.length, replacement.value);
    auto scanned = SourceScanner::scan(text);
    auto result = scanned.lines.empty() ? SourceLine{} : std::move(scanned.lines.front());
    result.number = line.number;
    result.terminated = line.terminated;
    for (auto* field : {&result.label, &result.operation, &result.operand, &result.comment}) {
        if (*field)
            (*field)->position.line = result.number;
    }
    if (result.openQuote)
        result.openQuote->line = result.number;
    return result;
}

} // namespace

LocalLabelSerial::LocalLabelSerial(std::size_t maxSerial) : maxSerial_{maxSerial} {
}

bool LocalLabelSerial::isLocalLabelMarker(std::string_view token) {
    if (token.size() < 4)
        return false;
    const auto offset = token.size() - 4;
    return token[offset] == '.' && (token[offset + 1] == 'S' || token[offset + 1] == 's') &&
           (token[offset + 2] == 'E' || token[offset + 2] == 'e') &&
           (token[offset + 3] == 'R' || token[offset + 3] == 'r');
}

std::string_view LocalLabelSerial::extractPrefix(std::string_view token) {
    return isLocalLabelMarker(token) ? token.substr(0, token.size() - 4) : std::string_view{};
}

std::string LocalLabelSerial::formatGeneratedName(std::string_view prefix, std::size_t serial) {
    return std::string{prefix} + "_M" + std::to_string(serial);
}

std::variant<std::string, MacroDiagnostic>
LocalLabelSerial::resolveGeneratedName(std::string_view prefix,
                                       std::size_t serial,
                                       SourcePosition tokenPosition,
                                       std::span<const ExpansionFrame> calls) {
    if (!assembler::SymbolTable::isValidIdentifier(prefix))
        return failure(MacroDiagnosticCode::InvalidLabel,
                       tokenPosition,
                       "Marcador '.SER' exige prefixo iniciado por letra, seguido de letras, "
                       "digitos ou '_'.",
                       calls);

    const auto key = assembler::SymbolTable::normalizeName(prefix);
    auto& cache = callLabels_[serial];
    if (const auto found = cache.find(key); found != cache.end())
        return found->second;

    auto generated = formatGeneratedName(prefix, serial);
    if (!assembler::SymbolTable::isValidIdentifier(generated))
        return failure(
            MacroDiagnosticCode::InvalidLabel, tokenPosition, "Nome local gerado invalido.", calls);
    if (assembler::SymbolTable::isReservedName(generated))
        return failure(MacroDiagnosticCode::ReservedName,
                       tokenPosition,
                       "Nome local gerado reservado pelo montador.",
                       calls);

    const auto generatedKey = assembler::SymbolTable::normalizeName(generated);
    if (const auto defined = definedLabels_.find(generatedKey); defined != definedLabels_.end())
        return duplicate(generated, tokenPosition, defined->second.position, calls);
    if (const auto reserved = generatedNames_.find(generatedKey);
        reserved != generatedNames_.end() && reserved->second.serial != serial)
        return duplicate(generated, tokenPosition, reserved->second.firstUse, calls);

    generatedNames_.try_emplace(generatedKey, GeneratedName{serial, tokenPosition});
    cache.emplace(key, generated);
    return generated;
}

std::variant<std::size_t, MacroDiagnostic>
LocalLabelSerial::allocateSerial(SourcePosition invocationPosition,
                                 std::span<const ExpansionFrame> calls) {
    // This same check covers a configured limit and the real SIZE_MAX bound:
    // increment only while the last allocated value is strictly below it.
    if (serialCounter_ >= maxSerial_)
        return failure(MacroDiagnosticCode::ExpansionLimitExceeded,
                       invocationPosition,
                       "Limite de expansao atingido: contador de serial esgotado.",
                       calls);
    return ++serialCounter_;
}

PreparedSerialLineResult LocalLabelSerial::prepareExpandedLine(
    const SourceLine& line, std::size_t serial, std::span<const ExpansionFrame> calls) {
    if (const auto invalid = validateSerial(serial, maxSerial_, {line.number, 1}, calls))
        return *invalid;
    PreparedSerialLine prepared{
        line, {line.number, 1}, {calls.begin(), calls.end()}, std::nullopt, {}};
    if (line.kind != SourceLineKind::Code)
        return prepared;

    std::vector<Replacement> replacements;
    if (line.label && isLocalLabelMarker(line.label->text)) {
        auto resolved = resolveGeneratedName(
            extractPrefix(line.label->text), serial, line.label->position, calls);
        if (const auto* diagnostic = std::get_if<MacroDiagnostic>(&resolved))
            return *diagnostic;
        replacements.push_back({line.label->position.column - 1,
                                line.label->text.size(),
                                std::get<std::string>(resolved)});
        prepared.labelSerial = serial;
    }

    if (line.operand) {
        const std::string_view text = line.operand->text;
        bool insideQuote = false;
        for (std::size_t cursor = 0; cursor < text.size();) {
            const auto character = text[cursor];
            if (character == '\'') {
                insideQuote = !insideQuote;
                ++cursor;
                continue;
            }
            if (insideQuote || !isTokenPart(character)) {
                ++cursor;
                continue;
            }
            const auto begin = cursor++;
            while (cursor < text.size() && isTokenPart(text[cursor]))
                ++cursor;
            const auto token = text.substr(begin, cursor - begin);
            if (!isLocalLabelMarker(token))
                continue;
            const SourcePosition position{line.number, line.operand->position.column + begin};
            auto resolved = resolveGeneratedName(extractPrefix(token), serial, position, calls);
            if (const auto* diagnostic = std::get_if<MacroDiagnostic>(&resolved))
                return *diagnostic;
            replacements.push_back({line.operand->position.column - 1 + begin,
                                    token.size(),
                                    std::get<std::string>(resolved)});
        }
    }
    prepared.line = applyReplacements(line, replacements);
    // applyReplacements orders edits right-to-left. Walk them left-to-right
    // to locate each generated occurrence in the rewritten text without
    // confusing a literal with the same spelling for a generated argument.
    std::size_t originalCursor = 0;
    std::size_t renderedCursor = 0;
    for (auto replacement = replacements.rbegin(); replacement != replacements.rend();
         ++replacement) {
        renderedCursor += replacement->begin - originalCursor;
        prepared.generatedNames.push_back(
            {{replacement->value, {line.number, renderedCursor + 1}}, serial});
        renderedCursor += replacement->value.size();
        originalCursor = replacement->begin + replacement->length;
    }
    return prepared;
}

LineProcessResult LocalLabelSerial::emitLine(const PreparedSerialLine& prepared) {
    const auto& line = prepared.line;
    if (line.kind == SourceLineKind::Code && line.label) {
        const auto key = assembler::SymbolTable::normalizeName(line.label->text);
        const auto reservation = generatedNames_.find(key);
        if (prepared.labelSerial) {
            if (reservation == generatedNames_.end() ||
                reservation->second.serial != *prepared.labelSerial)
                return failure(MacroDiagnosticCode::InvalidLabel,
                               line.label->position,
                               "Identidade de rotulo gerado nao corresponde a uma reserva.",
                               prepared.calls);
        } else if (reservation != generatedNames_.end()) {
            return duplicate(line.label->text,
                             line.label->position,
                             reservation->second.firstUse,
                             prepared.calls);
        }
        const auto previous = definedLabels_.find(key);
        if (previous != definedLabels_.end() && (prepared.labelSerial || previous->second.serial))
            return duplicate(
                line.label->text, line.label->position, previous->second.position, prepared.calls);

        // Two ordinary labels are preserved for the assembler to validate.
        definedLabels_.try_emplace(key,
                                   LabelDefinition{line.label->position, prepared.labelSerial});
    }
    origins_.push_back({nextExpandedLine_++, prepared.source, prepared.calls});
    return line;
}

LineProcessResult LocalLabelSerial::processCommonLine(const SourceLine& line) {
    return emitLine({line, {line.number, 1}, {}, std::nullopt, {}});
}

LineProcessResult LocalLabelSerial::processExpandedLine(const SourceLine& line,
                                                        std::size_t serial,
                                                        std::span<const ExpansionFrame> calls) {
    auto prepared = prepareExpandedLine(line, serial, calls);
    if (const auto* diagnostic = std::get_if<MacroDiagnostic>(&prepared))
        return *diagnostic;
    return emitLine(std::get<PreparedSerialLine>(prepared));
}

ExpandedLinesResult LocalLabelSerial::processExpandedLines(std::span<const SourceLine> lines,
                                                           std::size_t serial,
                                                           std::span<const ExpansionFrame> calls) {
    if (const auto invalid = validateSerial(
            serial, maxSerial_, {lines.empty() ? 1 : lines.front().number, 1}, calls))
        return *invalid;
    std::vector<SourceLine> output;
    output.reserve(lines.size());
    for (const auto& line : lines) {
        auto result = processExpandedLine(line, serial, calls);
        if (const auto* diagnostic = std::get_if<MacroDiagnostic>(&result))
            return *diagnostic;
        output.push_back(std::move(std::get<SourceLine>(result)));
    }
    return output;
}

ExpandedLinesResult LocalLabelSerial::expandCall(std::span<const SourceLine> lines,
                                                 SourcePosition invocationPosition,
                                                 std::span<const ExpansionFrame> calls) {
    const auto serial = allocateSerial(invocationPosition, calls);
    if (const auto* diagnostic = std::get_if<MacroDiagnostic>(&serial))
        return *diagnostic;
    return processExpandedLines(lines, std::get<std::size_t>(serial), calls);
}

ExpandedLinesResult
LocalLabelSerial::processSubstitutionResult(const SubstitutionResult& substitution,
                                            std::size_t serial,
                                            std::span<const ExpansionFrame> calls) {
    if (const auto invalid = validateSerial(serial, maxSerial_, {1, 1}, calls))
        return *invalid;
    std::vector<SourceLine> output;
    std::size_t index = 0;
    while (index < substitution.lines.size()) {
        const auto definition =
            std::find_if(substitution.definitions.begin(),
                         substitution.definitions.end(),
                         [index](const auto& candidate) {
                             return index >= candidate.beginLine && index < candidate.nextLine;
                         });
        if (definition != substitution.definitions.end()) {
            index = definition->nextLine;
            continue;
        }
        auto result = processExpandedLine(substitution.lines[index], serial, calls);
        if (const auto* diagnostic = std::get_if<MacroDiagnostic>(&result))
            return *diagnostic;
        output.push_back(std::move(std::get<SourceLine>(result)));
        ++index;
    }
    return output;
}

const std::vector<SourceOrigin>& LocalLabelSerial::origins() const noexcept {
    return origins_;
}

std::vector<SourceOrigin> LocalLabelSerial::takeOrigins() noexcept {
    return std::exchange(origins_, {});
}

std::size_t LocalLabelSerial::currentSerial() const noexcept {
    return serialCounter_;
}

std::size_t LocalLabelSerial::nextExpandedLine() const noexcept {
    return nextExpandedLine_;
}

std::size_t LocalLabelSerial::maxSerial() const noexcept {
    return maxSerial_;
}

void LocalLabelSerial::setMaxSerial(std::size_t limit) noexcept {
    maxSerial_ = limit;
}

bool LocalLabelSerial::isLabelDefined(std::string_view name) const {
    return definedLabels_.contains(assembler::SymbolTable::normalizeName(name));
}

std::optional<SourcePosition> LocalLabelSerial::findLabelDefinition(std::string_view name) const {
    const auto found = definedLabels_.find(assembler::SymbolTable::normalizeName(name));
    return found == definedLabels_.end() ? std::nullopt
                                         : std::optional<SourcePosition>{found->second.position};
}

} // namespace macro
