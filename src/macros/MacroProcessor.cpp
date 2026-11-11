#include "MacroProcessor.hpp"

#include "DefinitionCapture.hpp"
#include "LocalLabelSerial.hpp"
#include "MacroTable.hpp"
#include "SourceScanner.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

namespace macro {
namespace {

constexpr std::size_t maxDepth = 128;
constexpr std::size_t maxGeneratedLines = 100000;

class Engine {
public:
    MacroResult run(std::string_view text) {
        const auto source = SourceScanner::scan(text);
        if (auto error = process(source.lines, {}, 0, {}))
            return MacroFailure{{std::move(*error)}};
        return MacroSuccess{std::move(output_), serials_.takeOrigins()};
    }

private:
    MacroTable table_;
    // Templates belong to immutable definition versions, including captured bindings.
    std::unordered_map<MacroTable::Definition, std::shared_ptr<const MacroTemplate>> templates_;
    LocalLabelSerial serials_;
    std::size_t generatedLines_{};
    std::string output_;
    bool pendingSeparator_{};

    std::optional<MacroDiagnostic> emit(const PreparedSerialLine& prepared) {
        auto result = serials_.emitLine(prepared);
        if (auto* error = std::get_if<MacroDiagnostic>(&result))
            return *error;
        if (pendingSeparator_)
            output_ += '\n';
        output_ += prepared.line.text;
        if (prepared.line.terminated)
            output_ += '\n';
        pendingSeparator_ = !prepared.line.terminated;
        return std::nullopt;
    }

    std::optional<MacroDiagnostic>
    process(std::span<const SourceLine> lines,
            std::span<const PreparedMacroDefinition> definitions,
            std::size_t serial,
            const std::vector<ExpansionFrame>& calls,
            std::span<const std::vector<GeneratedNameUse>> generated = {}) {
        auto contextual = [&](MacroDiagnostic error) {
            error.calls = calls;
            return std::optional<MacroDiagnostic>{std::move(error)};
        };
        for (std::size_t index = 0; index < lines.size();) {
            const auto& line = lines[index];
            if (line.directive() != MacroDirective::None) {
                auto captured = DefinitionCapture::capture(lines, index);
                if (auto* error = std::get_if<MacroDiagnostic>(&captured))
                    return contextual(*error);
                auto& definition = std::get<CapturedDefinition>(captured);
                const auto prepared =
                    std::find_if(definitions.begin(), definitions.end(), [index](const auto& item) {
                        return item.beginLine == index;
                    });
                std::shared_ptr<const MacroTemplate> macroTemplate;
                if (prepared != definitions.end()) {
                    macroTemplate = std::make_shared<const MacroTemplate>(prepared->macroTemplate);
                    index = prepared->nextLine;
                } else {
                    auto compiled = MacroTemplate::fromDefinition(definition.definition);
                    if (auto* error = std::get_if<MacroDiagnostic>(&compiled.value))
                        return contextual(*error);
                    macroTemplate = std::make_shared<const MacroTemplate>(
                        std::move(std::get<MacroTemplate>(compiled.value)));
                    index = definition.nextLine;
                }
                const auto name = definition.definition.name().text;
                table_.define(std::move(definition.definition));
                templates_.emplace(table_.find(name), std::move(macroTemplate));
                continue;
            }

            PreparedSerialLine prepared{line, {line.number, 1}, calls, std::nullopt, {}};
            if (serial != 0) {
                auto result = serials_.prepareExpandedLine(
                    line,
                    serial,
                    calls,
                    generated.empty() ? std::span<const GeneratedNameUse>{} : generated[index]);
                if (auto* error = std::get_if<MacroDiagnostic>(&result))
                    return *error;
                prepared = std::move(std::get<PreparedSerialLine>(result));
            }
            ++index;
            const auto definition =
                prepared.line.operation ? table_.find(prepared.line.operation->text) : nullptr;
            if (!definition) {
                if (auto error = emit(prepared))
                    return error;
                continue;
            }
            // Keep this version alive across calls which can redefine the active name.
            const auto macroTemplate = templates_.at(definition);
            const auto& prototype = macroTemplate->root().prototype;
            auto chain = calls;
            chain.push_back({prototype.macroName.text,
                             prototype.macroName.position,
                             prepared.line.operation->position});
            const bool recursive = std::any_of(calls.begin(), calls.end(), [&](const auto& frame) {
                // Lookup uses the same case-insensitive table semantics as normal calls.
                return table_.find(frame.macroName) == definition;
            });
            if (recursive || calls.size() >= maxDepth)
                return MacroDiagnostic{recursive ? MacroDiagnosticCode::RecursiveExpansion
                                                 : MacroDiagnosticCode::ExpansionLimitExceeded,
                                       prepared.line.operation->position,
                                       recursive ? "Chamada recursiva de macro."
                                                 : "Limite de 128 chamadas ativas excedido.",
                                       std::move(chain)};
            auto boundResult = MacroArguments::bind(prototype, prepared.line);
            if (auto* error = std::get_if<MacroDiagnostic>(&boundResult))
                return contextual(*error);
            auto bound = std::move(std::get<BoundMacroArguments>(boundResult));
            for (auto& argument : bound.arguments) {
                if (!argument.position)
                    continue;
                for (auto use : prepared.generatedNames) {
                    if (use.token.position.column < argument.position->column)
                        continue;
                    const auto offset = use.token.position.column - argument.position->column;
                    if (offset <= argument.value.size() &&
                        argument.value.compare(offset, use.token.text.size(), use.token.text) ==
                            0) {
                        use.token.position.column = offset + 1;
                        argument.generatedNames.push_back(std::move(use));
                    }
                }
            }
            if (prepared.line.comment) {
                auto comment = SourceScanner::scan(prepared.line.comment->text).lines.front();
                comment.number = line.number;
                comment.terminated = true;
                if (auto error = emit({comment, prepared.line.comment->position, calls, {}, {}}))
                    return error;
            }
            auto allocated = serials_.allocateSerial(prepared.line.operation->position, chain);
            if (auto* error = std::get_if<MacroDiagnostic>(&allocated))
                return *error;
            auto substitution = ParameterSubstitution::instantiate(*macroTemplate, bound);
            if (substitution.lines.size() > maxGeneratedLines - generatedLines_)
                return MacroDiagnostic{MacroDiagnosticCode::ExpansionLimitExceeded,
                                       prepared.line.operation->position,
                                       "Limite de 100000 linhas geradas excedido.",
                                       chain};
            generatedLines_ += substitution.lines.size();
            if (auto error = process(substitution.lines,
                                     substitution.definitions,
                                     std::get<std::size_t>(allocated),
                                     chain,
                                     substitution.generatedNames))
                return error;
        }
        return std::nullopt;
    }
};

} // namespace

MacroResult expand(std::string_view sourceText) {
    return Engine{}.run(sourceText);
}

} // namespace macro
