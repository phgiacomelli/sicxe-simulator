#include "MacroTemplate.hpp"

#include "DefinitionCapture.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace macro {
namespace {

bool isIdentifierStart(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool isTokenPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9') || value == '_' ||
           value == '.' || static_cast<unsigned char>(value) >= 128;
}

struct VisibleFormal {
    std::size_t scopeDepth{};
    std::size_t formalIndex{};
};

std::optional<VisibleFormal> findVisibleFormal(std::string_view name,
                                               const std::vector<const MacroPrototype*>& scopes) {
    // Os escopos são examinados do mais interno para o mais externo.
    // Assim, um formal interno prevalece sobre um formal externo
    // que tenha o mesmo nome.
    for (std::size_t depth = 0; depth < scopes.size(); ++depth) {
        if (const auto index = scopes[depth]->findFormal(name)) {
            return VisibleFormal{depth, *index};
        }
    }

    return std::nullopt;
}

void collectReferences(const SourceField& field,
                       TemplateField fieldKind,
                       const std::vector<const MacroPrototype*>& scopes,
                       std::vector<ParameterReference>& references) {
    const std::string_view text = field.text;

    bool insideQuote = false;

    for (std::size_t cursor = 0; cursor < text.size();) {
        const char character = text[cursor];

        // Conteúdo entre aspas simples nunca recebe substituição.
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
        const auto token = text.substr(begin, length);

        // Prefixos C e X de constantes entre aspas não são parâmetros.
        if (cursor < text.size() && text[cursor] == '\'' &&
            (token == "C" || token == "c" || token == "X" || token == "x")) {
            continue;
        }

        const auto visible = findVisibleFormal(token, scopes);

        if (!visible) {
            continue;
        }

        references.push_back({
            fieldKind,
            visible->scopeDepth,
            visible->formalIndex,
            begin,
            length,
        });
    }
}

TemplateLine makeTemplateLine(const SourceLine& source,
                              const std::vector<const MacroPrototype*>& scopes) {
    TemplateLine result;
    result.source = source;

    // Substituições são permitidas somente no campo de rótulo
    // e no campo de operando.
    if (source.label) {
        collectReferences(*source.label, TemplateField::Label, scopes, result.references);
    }

    if (source.operand) {
        collectReferences(*source.operand, TemplateField::Operand, scopes, result.references);
    }

    return result;
}

using ScopeBuildResult = std::variant<TemplateScope, MacroDiagnostic>;

ScopeBuildResult buildScope(const MacroDefinition& definition,
                            std::vector<const MacroPrototype*> ancestors) {
    // Obtém os parâmetros formais da definição atual.
    const auto prototypeResult = MacroArguments::parsePrototype(definition);

    const auto* prototype = std::get_if<MacroPrototype>(&prototypeResult);

    if (!prototype) {
        return std::get<MacroDiagnostic>(prototypeResult);
    }

    TemplateScope scope;

    scope.prototype = *prototype;

    scope.beginLine.source = definition.begin();
    scope.prototypeLine.source = definition.prototype();
    scope.endLine.source = definition.end();

    // O escopo atual tem prioridade sobre seus ancestrais.
    std::vector<const MacroPrototype*> visibleScopes;

    visibleScopes.reserve(ancestors.size() + 1);

    visibleScopes.push_back(&scope.prototype);

    for (const auto* ancestor : ancestors) {
        visibleScopes.push_back(ancestor);
    }

    const auto& body = definition.body();

    for (std::size_t index = 0; index < body.size();) {
        // Encontrou uma definição interna.
        if (body[index].directive() == MacroDirective::DefinitionBegin) {
            const auto captured =
                DefinitionCapture::capture(std::span<const SourceLine>{body}, index);

            const auto* nested = std::get_if<CapturedDefinition>(&captured);

            if (!nested) {
                return std::get<MacroDiagnostic>(captured);
            }

            // O escopo atual passa a ser ancestral da definição interna.
            auto nestedAncestors = visibleScopes;

            auto nestedResult = buildScope(nested->definition, std::move(nestedAncestors));

            auto* nestedScope = std::get_if<TemplateScope>(&nestedResult);

            if (!nestedScope) {
                return std::get<MacroDiagnostic>(nestedResult);
            }

            // Armazena a definição interna sem perder sua posição
            // relativa às demais linhas do corpo.
            scope.elements.emplace_back(
                std::make_shared<const TemplateScope>(std::move(*nestedScope)));

            // Pula todas as linhas da definição interna já capturada.
            index = nested->nextLine;

            continue;
        }

        // Linha comum do corpo da macro.
        scope.elements.emplace_back(makeTemplateLine(body[index], visibleScopes));

        ++index;
    }

    return scope;
}

} // namespace

MacroTemplate::MacroTemplate(TemplateScope root,
                             std::vector<BoundMacroArguments> enclosingArguments)
    : root_(std::move(root)), enclosingArguments_(std::move(enclosingArguments)) {
}

MacroTemplateResult MacroTemplate::fromDefinition(const MacroDefinition& definition) {
    auto result = buildScope(definition, {});

    auto* scope = std::get_if<TemplateScope>(&result);

    if (!scope) {
        return MacroTemplateResult{std::get<MacroDiagnostic>(std::move(result))};
    }

    return MacroTemplateResult{MacroTemplate{std::move(*scope)}};
}

const TemplateScope& MacroTemplate::root() const noexcept {
    return root_;
}

} // namespace macro
