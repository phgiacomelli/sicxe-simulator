#pragma once

#include "MacroArguments.hpp"
#include "MacroDefinition.hpp"
#include "MacroDiagnostic.hpp"

#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

namespace macro {

enum class TemplateField {
    Label,
    Operand,
};

struct ParameterReference {
    TemplateField field{TemplateField::Operand};

    // Distância até o escopo que declarou o parâmetro.
    // 0 = escopo atual; 1 = escopo pai; 2 = avô.
    std::size_t scopeDepth{};

    // Índice do parâmetro no protótipo do escopo correspondente.
    std::size_t formalIndex{};

    // Posição e tamanho da ocorrência no campo original.
    std::size_t begin{};
    std::size_t length{};

    bool operator==(const ParameterReference&) const = default;
};

struct TemplateLine {
    SourceLine source;
    std::vector<ParameterReference> references;
};

struct TemplateScope;

using TemplateElement = std::variant<TemplateLine, std::shared_ptr<const TemplateScope>>;

struct TemplateScope {
    MacroPrototype prototype;

    TemplateLine beginLine;
    TemplateLine prototypeLine;

    // Mantém a ordem original das linhas e das definições internas.
    std::vector<TemplateElement> elements;

    TemplateLine endLine;
};

// Declaração antecipada para permitir que MacroTemplate
// retorne o resultado da construção.
struct MacroTemplateResult;

class MacroTemplate {
public:
    static MacroTemplateResult fromDefinition(const MacroDefinition& definition);

    const TemplateScope& root() const noexcept;

private:
    friend class ParameterSubstitution;
    explicit MacroTemplate(TemplateScope root,
                           std::vector<BoundMacroArguments> enclosingArguments = {});

    TemplateScope root_;
    // Captured ancestor values are never re-tokenized as local formals.
    std::vector<BoundMacroArguments> enclosingArguments_;
};

// Resultado da construção: uma MacroTemplate ou um diagnóstico.
struct MacroTemplateResult {
    std::variant<MacroTemplate, MacroDiagnostic> value;
};

} // namespace macro
