#include "ParameterSubstitution.hpp"

#include "SourceScanner.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace macro {
namespace {

using ActiveBindings = std::vector<const BoundMacroArguments*>;

struct Replacement {
    std::size_t begin;
    std::size_t length;
    std::string value;
};

SourceLine substituteLine(const TemplateLine& line, const ActiveBindings& bindings) {
    std::vector<Replacement> replacements;
    for (const auto& reference : line.references) {
        // A null binding preserves a nested definition's own parameters.
        if (reference.scopeDepth >= bindings.size() || !bindings[reference.scopeDepth])
            continue;
        const auto& arguments = *bindings[reference.scopeDepth];
        if (reference.formalIndex >= arguments.values.size())
            continue;
        const auto& field =
            reference.field == TemplateField::Label ? line.source.label : line.source.operand;
        if (!field || reference.begin > field->text.size() ||
            reference.length > field->text.size() - reference.begin)
            continue;
        const auto begin = field->position.column - 1 + reference.begin;
        if (begin > line.source.text.size() || reference.length > line.source.text.size() - begin)
            continue;
        replacements.push_back({begin, reference.length, arguments.values[reference.formalIndex]});
    }
    if (replacements.empty())
        return line.source;

    std::sort(replacements.begin(), replacements.end(), [](const auto& left, const auto& right) {
        return left.begin > right.begin;
    });
    auto text = line.source.text;
    for (const auto& replacement : replacements)
        text.replace(replacement.begin, replacement.length, replacement.value);

    // Reuse the lexical scanner so empty fields, quotes and byte columns agree
    // with the generated text. This does not parse or validate assembly.
    auto scanned = SourceScanner::scan(text);
    auto result = scanned.lines.empty() ? SourceLine{} : std::move(scanned.lines.front());
    result.number = line.source.number;
    result.terminated = line.source.terminated;
    for (auto* field : {&result.label, &result.operation, &result.operand, &result.comment}) {
        if (*field)
            (*field)->position.line = result.number;
    }
    if (result.openQuote)
        result.openQuote->line = result.number;
    return result;
}

void appendElements(const TemplateScope& scope,
                    const ActiveBindings& bindings,
                    std::vector<SourceLine>& output);

void appendNestedScope(const TemplateScope& scope,
                       const ActiveBindings& bindings,
                       std::vector<SourceLine>& output) {
    output.push_back(substituteLine(scope.beginLine, bindings));
    output.push_back(substituteLine(scope.prototypeLine, bindings));
    appendElements(scope, bindings, output);
    output.push_back(substituteLine(scope.endLine, bindings));
}

void appendElements(const TemplateScope& scope,
                    const ActiveBindings& bindings,
                    std::vector<SourceLine>& output) {
    for (const auto& element : scope.elements) {
        if (const auto* line = std::get_if<TemplateLine>(&element)) {
            output.push_back(substituteLine(*line, bindings));
        } else {
            const auto& nested = std::get<std::shared_ptr<const TemplateScope>>(element);
            auto nestedBindings = bindings;
            nestedBindings.insert(nestedBindings.begin(), nullptr);
            appendNestedScope(*nested, nestedBindings, output);
        }
    }
}

} // namespace

SubstitutionResult ParameterSubstitution::instantiate(const MacroTemplate& macroTemplate,
                                                      const BoundMacroArguments& arguments) {
    ActiveBindings bindings{&arguments};
    for (const auto& enclosing : macroTemplate.enclosingArguments_)
        bindings.push_back(&enclosing);

    SubstitutionResult result;
    for (const auto& element : macroTemplate.root().elements) {
        if (const auto* line = std::get_if<TemplateLine>(&element)) {
            result.lines.push_back(substituteLine(*line, bindings));
            continue;
        }
        const auto& nested = std::get<std::shared_ptr<const TemplateScope>>(element);
        const auto begin = result.lines.size();
        auto nestedBindings = bindings;
        nestedBindings.insert(nestedBindings.begin(), nullptr);
        appendNestedScope(*nested, nestedBindings, result.lines);

        std::vector<BoundMacroArguments> captured;
        captured.reserve(bindings.size());
        for (const auto* binding : bindings)
            captured.push_back(*binding);
        result.definitions.push_back(
            {begin, result.lines.size(), MacroTemplate{*nested, std::move(captured)}});
    }
    return result;
}

std::vector<SourceLine> ParameterSubstitution::substitute(const MacroTemplate& macroTemplate,
                                                          const BoundMacroArguments& arguments) {
    return instantiate(macroTemplate, arguments).lines;
}

} // namespace macro
