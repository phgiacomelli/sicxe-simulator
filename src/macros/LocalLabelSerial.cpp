#include "LocalLabelSerial.hpp"

#include "SourceScanner.hpp"
#include "assembler/SymbolTable.hpp"

#include <algorithm>
#include <cctype>
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

SourceLine applyReplacements(const SourceLine& line, std::vector<Replacement>& replacements) {
    if (replacements.empty()) {
        return line;
    }

    std::sort(replacements.begin(), replacements.end(), [](const auto& left, const auto& right) {
        return left.begin > right.begin;
    });

    auto text = line.text;
    for (const auto& replacement : replacements) {
        text.replace(replacement.begin, replacement.length, replacement.value);
    }

    auto scanned = SourceScanner::scan(text);
    auto result = scanned.lines.empty() ? SourceLine{} : std::move(scanned.lines.front());
    result.number = line.number;
    result.terminated = line.terminated;
    for (auto* field : {&result.label, &result.operation, &result.operand, &result.comment}) {
        if (*field) {
            (*field)->position.line = result.number;
        }
    }
    if (result.openQuote) {
        result.openQuote->line = result.number;
    }
    return result;
}

} // namespace

LocalLabelSerial::LocalLabelSerial(std::size_t maxSerial)
    : maxSerial_{maxSerial} {
}

bool LocalLabelSerial::isLocalLabelMarker(std::string_view token) {
    if (token.size() < 4) {
        return false;
    }
    const std::size_t offset = token.size() - 4;
    return token[offset] == '.' &&
           (token[offset + 1] == 'S' || token[offset + 1] == 's') &&
           (token[offset + 2] == 'E' || token[offset + 2] == 'e') &&
           (token[offset + 3] == 'R' || token[offset + 3] == 'r');
}

std::string_view LocalLabelSerial::extractPrefix(std::string_view token) {
    if (!isLocalLabelMarker(token)) {
        return {};
    }
    return token.substr(0, token.size() - 4);
}

std::string LocalLabelSerial::formatGeneratedName(std::string_view prefix, std::size_t serial) {
    return std::string{prefix} + "_M" + std::to_string(serial);
}

std::variant<std::string, MacroDiagnostic> LocalLabelSerial::resolveGeneratedName(
    std::string_view prefix,
    std::size_t serial,
    SourcePosition tokenPosition,
    std::span<const ExpansionFrame> calls) {
    if (prefix.empty()) {
        std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
        return MacroDiagnostic{
            MacroDiagnosticCode::InvalidLabel,
            tokenPosition,
            "Marcador de rotulo local '.SER' sem prefixo valido.",
            std::move(callChain),
        };
    }

    if (!assembler::SymbolTable::isValidIdentifier(prefix)) {
        std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
        return MacroDiagnostic{
            MacroDiagnosticCode::InvalidLabel,
            tokenPosition,
            "Prefixo '" + std::string{prefix} +
                "' de rotulo local invalido; deve iniciar por letra e conter apenas letras, digitos ou '_'.",
            std::move(callChain),
        };
    }

    auto& cache = callLabels_[serial];
    const std::string key = assembler::SymbolTable::normalizeName(prefix);
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }

    std::string generated = formatGeneratedName(prefix, serial);
    if (!assembler::SymbolTable::isValidIdentifier(generated)) {
        std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
        return MacroDiagnostic{
            MacroDiagnosticCode::InvalidLabel,
            tokenPosition,
            "Rotulo gerado '" + generated +
                "' invalido; use uma letra seguida de letras, digitos ou '_'.",
            std::move(callChain),
        };
    }

    if (assembler::SymbolTable::isReservedName(generated)) {
        std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
        return MacroDiagnostic{
            MacroDiagnosticCode::ReservedName,
            tokenPosition,
            "Rotulo gerado '" + generated + "' colide com mnemonico ou diretiva reservada.",
            std::move(callChain),
        };
    }

    cache.emplace(key, generated);
    return generated;
}

std::variant<std::size_t, MacroDiagnostic> LocalLabelSerial::allocateSerial(
    SourcePosition invocationPosition,
    std::span<const ExpansionFrame> calls) {
    if (serialCounter_ > maxSerial_) {
        std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
        return MacroDiagnostic{
            MacroDiagnosticCode::ExpansionLimitExceeded,
            invocationPosition,
            "Limite de expansao atingido: contador de serial esgotado.",
            std::move(callChain),
        };
    }
    const std::size_t assigned = serialCounter_++;
    return assigned;
}

LineProcessResult LocalLabelSerial::processCommonLine(const SourceLine& line) {
    if (line.kind == SourceLineKind::Code && line.label) {
        const SourcePosition labelPosition{line.number, line.label->position.column};
        const std::string key = assembler::SymbolTable::normalizeName(line.label->text);
        const auto it = definedLabels_.find(key);
        if (it != definedLabels_.end()) {
            const auto& prev = it->second;
            return MacroDiagnostic{
                MacroDiagnosticCode::DuplicateLabel,
                labelPosition,
                "Rotulo '" + line.label->text + "' colide com simbolo ja existente definido na linha " +
                    std::to_string(prev.position.line) + ", coluna " + std::to_string(prev.position.column) + ".",
                {},
            };
        }
        definedLabels_.emplace(key, LabelDefinition{labelPosition, line.label->text});
    }

    SourceOrigin origin;
    origin.expandedLine = nextExpandedLine_++;
    origin.source = SourcePosition{line.number, 1};
    origin.calls = {};
    origins_.push_back(std::move(origin));

    return line;
}

LineProcessResult LocalLabelSerial::processExpandedLine(
    const SourceLine& line,
    std::size_t serial,
    std::span<const ExpansionFrame> calls) {
    if (serial > maxSerial_) {
        std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
        return MacroDiagnostic{
            MacroDiagnosticCode::ExpansionLimitExceeded,
            SourcePosition{line.number, 1},
            "Limite de expansao atingido: contador de serial esgotado.",
            std::move(callChain),
        };
    }

    if (line.kind != SourceLineKind::Code) {
        SourceOrigin origin;
        origin.expandedLine = nextExpandedLine_++;
        origin.source = SourcePosition{line.number, 1};
        origin.calls = std::vector<ExpansionFrame>(calls.begin(), calls.end());
        origins_.push_back(std::move(origin));
        return line;
    }

    std::vector<Replacement> replacements;
    std::optional<std::string> newLabelName;
    SourcePosition definedLabelPosition{};

    if (line.label) {
        definedLabelPosition = SourcePosition{line.number, line.label->position.column};
        if (isLocalLabelMarker(line.label->text)) {
            const auto prefix = extractPrefix(line.label->text);
            auto resolved = resolveGeneratedName(prefix, serial, definedLabelPosition, calls);
            if (const auto* diag = std::get_if<MacroDiagnostic>(&resolved)) {
                return *diag;
            }
            std::string generated = std::get<std::string>(resolved);
            replacements.push_back(Replacement{
                line.label->position.column - 1,
                line.label->text.size(),
                generated,
            });
            newLabelName = generated;
        } else {
            newLabelName = line.label->text;
        }
    }

    if (line.operand) {
        const std::string_view text = line.operand->text;
        bool insideQuote = false;

        for (std::size_t cursor = 0; cursor < text.size();) {
            const char character = text[cursor];

            if (character == '\'') {
                insideQuote = !insideQuote;
                ++cursor;
                continue;
            }

            if (insideQuote || !isTokenPart(character)) {
                ++cursor;
                continue;
            }

            const std::size_t begin = cursor++;
            while (cursor < text.size() && isTokenPart(text[cursor])) {
                ++cursor;
            }

            const std::size_t length = cursor - begin;
            const std::string_view token = text.substr(begin, length);

            // Prefixos de constantes C'...' e X'...' nao sao rotulos.
            if (cursor < text.size() && text[cursor] == '\'' &&
                (token == "C" || token == "c" || token == "X" || token == "x")) {
                continue;
            }

            if (isLocalLabelMarker(token)) {
                const auto prefix = extractPrefix(token);
                const SourcePosition tokenPos{
                    line.number,
                    line.operand->position.column + begin,
                };
                auto resolved = resolveGeneratedName(prefix, serial, tokenPos, calls);
                if (const auto* diag = std::get_if<MacroDiagnostic>(&resolved)) {
                    return *diag;
                }
                const std::size_t textOffset = line.operand->position.column - 1 + begin;
                replacements.push_back(Replacement{
                    textOffset,
                    length,
                    std::get<std::string>(resolved),
                });
            }
        }
    }

    if (newLabelName) {
        const std::string key = assembler::SymbolTable::normalizeName(*newLabelName);
        const auto it = definedLabels_.find(key);
        if (it != definedLabels_.end()) {
            std::vector<ExpansionFrame> callChain(calls.begin(), calls.end());
            const auto& prev = it->second;
            return MacroDiagnostic{
                MacroDiagnosticCode::DuplicateLabel,
                definedLabelPosition,
                "Rotulo '" + *newLabelName + "' colide com simbolo ja existente definido na linha " +
                    std::to_string(prev.position.line) + ", coluna " + std::to_string(prev.position.column) + ".",
                std::move(callChain),
            };
        }
        definedLabels_.emplace(key, LabelDefinition{definedLabelPosition, *newLabelName});
    }

    auto rendered = applyReplacements(line, replacements);

    SourceOrigin origin;
    origin.expandedLine = nextExpandedLine_++;
    origin.source = SourcePosition{line.number, 1};
    origin.calls = std::vector<ExpansionFrame>(calls.begin(), calls.end());
    origins_.push_back(std::move(origin));

    return rendered;
}

ExpandedLinesResult LocalLabelSerial::processExpandedLines(
    std::span<const SourceLine> lines,
    std::size_t serial,
    std::span<const ExpansionFrame> calls) {
    std::vector<SourceLine> output;
    output.reserve(lines.size());
    for (const auto& line : lines) {
        auto result = processExpandedLine(line, serial, calls);
        if (const auto* diag = std::get_if<MacroDiagnostic>(&result)) {
            return *diag;
        }
        output.push_back(std::move(std::get<SourceLine>(result)));
    }
    return output;
}

ExpandedLinesResult LocalLabelSerial::expandCall(
    std::span<const SourceLine> lines,
    SourcePosition invocationPosition,
    std::span<const ExpansionFrame> calls) {
    auto serial = allocateSerial(invocationPosition, calls);
    if (const auto* diag = std::get_if<MacroDiagnostic>(&serial)) {
        return *diag;
    }
    return processExpandedLines(lines, std::get<std::size_t>(serial), calls);
}

ExpandedLinesResult LocalLabelSerial::processSubstitutionResult(
    const SubstitutionResult& substitution,
    std::size_t serial,
    std::span<const ExpansionFrame> calls) {
    std::vector<SourceLine> output;
    std::size_t lineIndex = 0;
    while (lineIndex < substitution.lines.size()) {
        bool isDefinition = false;
        for (const auto& def : substitution.definitions) {
            if (lineIndex >= def.beginLine && lineIndex < def.nextLine) {
                lineIndex = def.nextLine;
                isDefinition = true;
                break;
            }
        }
        if (isDefinition) {
            continue;
        }
        auto result = processExpandedLine(substitution.lines[lineIndex], serial, calls);
        if (const auto* diag = std::get_if<MacroDiagnostic>(&result)) {
            return *diag;
        }
        output.push_back(std::move(std::get<SourceLine>(result)));
        ++lineIndex;
    }
    return output;
}

const std::vector<SourceOrigin>& LocalLabelSerial::origins() const noexcept {
    return origins_;
}

std::vector<SourceOrigin> LocalLabelSerial::takeOrigins() noexcept {
    return std::move(origins_);
}

std::size_t LocalLabelSerial::currentSerial() const noexcept {
    return serialCounter_ > 1 ? serialCounter_ - 1 : 1;
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
    const std::string key = assembler::SymbolTable::normalizeName(name);
    return definedLabels_.contains(key);
}

std::optional<SourcePosition> LocalLabelSerial::findLabelDefinition(std::string_view name) const {
    const std::string key = assembler::SymbolTable::normalizeName(name);
    const auto it = definedLabels_.find(key);
    if (it != definedLabels_.end()) {
        return it->second.position;
    }
    return std::nullopt;
}

} // namespace macro
