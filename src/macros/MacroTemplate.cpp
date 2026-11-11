
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
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

bool isIdentifierPart(char value) {
    return isIdentifierStart(value) ||
           (value >= '0' && value <= '9') ||
           value == '_';
}

char upperAscii(char value) {
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }

    return value;
}

std::string normalize(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    for (const char character : value) {
        result.push_back(upperAscii(character));
    }

    return result;
}

struct VisibleFormal {
    std::size_t scopeDepth{};
    std::size_t formalIndex{};
};

std::optional<VisibleFormal> findVisibleFormal(
    std::string_view name,
    const std::vector<const MacroPrototype*>& scopes) {

    const auto key = normalize(name);

    // Os escopos são examinados do mais interno para o mais externo.
    // Assim, um formal interno prevalece sobre um formal externo
    // que tenha o mesmo nome.
    for (std::size_t depth = 0; depth < scopes.size(); ++depth) {
        const auto& formals = scopes[depth]->formals;

        for (std::size_t index = 0; index < formals.size(); ++index) {
            if (formals[index].key == key) {
                return VisibleFormal{depth, index};
            }
        }
    }

    return std::nullopt;
}

bool isIndexRegister(
    std::string_view text,
    std::size_t begin,
    std::size_t end) {

    // O marcador de indexação deve ser exatamente X.
    if (normalize(text.substr(begin, end - begin)) != "X") {
        return false;
    }

    // Verifica o caractere anterior, ignorando espaços.
    std::size_t previous = begin;

    while (previous > 0 &&
           (text[previous - 1] == ' ' ||
            text[previous - 1] == '\t')) {
        --previous;
    }

    if (previous == 0 || text[previous - 1] != ',') {
        return false;
    }

    // Verifica se depois do X não existe outro token.
    std::size_t next = end;

    while (next < text.size() &&
           (text[next] == ' ' || text[next] == '\t')) {
        ++next;
    }

    return next == text.size();
}

void collectReferences(
    const SourceField& field,
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

        if (insideQuote || !isIdentifierStart(character)) {
            ++cursor;
            continue;
        }

        const std::size_t begin = cursor++;

        while (cursor < text.size() &&
               isIdentifierPart(text[cursor])) {
            ++cursor;
        }

        const std::size_t length = cursor - begin;
        const auto token = text.substr(begin, length);
        const auto key = normalize(token);

        // Prefixos C e X de constantes entre aspas não são parâmetros.
        if (cursor < text.size() &&
            text[cursor] == '\'' &&
            (key == "C" || key == "X")) {
            continue;
        }

        // O X de ,X é o marcador de indexação SIC/XE.
        if (isIndexRegister(text, begin, cursor)) {
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

TemplateLine makeTemplateLine(
    const SourceLine& source,
    const std::vector<const MacroPrototype*>& scopes) {

    TemplateLine result;
    result.source = source;

    // Substituições são permitidas somente no campo de rótulo
    // e no campo de operando.
    if (source.label) {
        collectReferences(
            *source.label,
            TemplateField::Label,
            scopes,
            result.references);
    }

    if (source.operand) {
        collectReferences(
            *source.operand,
            TemplateField::Operand,
            scopes,
            result.references);
    }

    return result;
}

using ScopeBuildResult =
    std::variant<TemplateScope, MacroDiagnostic>;

ScopeBuildResult buildScope(
    const MacroDefinition& definition,
    std::vector<const MacroPrototype*> ancestors) {

    // Obtém os parâmetros formais da definição atual.
    const auto prototypeResult =
        MacroArguments::parsePrototype(definition);

    const auto* prototype =
        std::get_if<MacroPrototype>(&prototypeResult);

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
        if (body[index].directive() ==
            MacroDirective::DefinitionBegin) {

            const auto captured = DefinitionCapture::capture(
                std::span<const SourceLine>{body},
                index);

            const auto* nested =
                std::get_if<CapturedDefinition>(&captured);

            if (!nested) {
                return std::get<MacroDiagnostic>(captured);
            }

            // O escopo atual passa a ser ancestral da definição interna.
            auto nestedAncestors = visibleScopes;

            auto nestedResult = buildScope(
                nested->definition,
                std::move(nestedAncestors));

            const auto* nestedScope =
                std::get_if<TemplateScope>(&nestedResult);

            if (!nestedScope) {
                return std::get<MacroDiagnostic>(nestedResult);
            }

            // Armazena a definição interna sem perder sua posição
            // relativa às demais linhas do corpo.
            scope.elements.emplace_back(
                std::make_shared<TemplateScope>(*nestedScope));

            // Pula todas as linhas da definição interna já capturada.
            index = nested->nextLine;

            continue;
        }

        // Linha comum do corpo da macro.
        scope.elements.emplace_back(
            makeTemplateLine(body[index], visibleScopes));

        ++index;
    }

    return scope;
}

} // namespace

MacroTemplate::MacroTemplate(TemplateScope root)
    : root_(std::move(root)) {
}

MacroTemplateResult MacroTemplate::fromDefinition(
    const MacroDefinition& definition) {

    auto result = buildScope(definition, {});

    const auto* scope = std::get_if<TemplateScope>(&result);

    if (!scope) {
        return MacroTemplateResult{
            std::get<MacroDiagnostic>(std::move(result))
        };
    }

    return MacroTemplateResult{
        MacroTemplate{std::move(*scope)}
    };
}

const TemplateScope& MacroTemplate::root() const noexcept {
    return root_;
}

} // namespace macro
