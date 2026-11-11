
#include "ParameterSubstitution.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace macro {
namespace {

struct ActiveBinding {
    const BoundMacroArguments* arguments{};
};

struct Replacement {
    TemplateField field{TemplateField::Operand};

    std::size_t begin{};
    std::size_t length{};

    std::size_t absoluteBegin{};

    std::string value;
};

const SourceField* findSourceField(
    const SourceLine& line,
    TemplateField field) {

    if (field == TemplateField::Label) {
        return line.label ? &*line.label : nullptr;
    }

    return line.operand ? &*line.operand : nullptr;
}

SourceField* findSourceField(
    SourceLine& line,
    TemplateField field) {

    if (field == TemplateField::Label) {
        return line.label ? &*line.label : nullptr;
    }

    return line.operand ? &*line.operand : nullptr;
}

void adjustPosition(
    std::optional<SourcePosition>& position,
    const std::vector<Replacement>& replacements) {

    if (!position) {
        return;
    }

    const std::size_t originalColumn = position->column;

    std::ptrdiff_t adjustment = 0;

    for (const auto& replacement : replacements) {
        const std::size_t replacementEnd =
            replacement.absoluteBegin + replacement.length;

        // Somente substituições completamente anteriores
        // à posição devem alterar sua coluna.
        if (replacementEnd > originalColumn - 1) {
            continue;
        }

        adjustment +=
            static_cast<std::ptrdiff_t>(replacement.value.size()) -
            static_cast<std::ptrdiff_t>(replacement.length);
    }

    const auto adjustedColumn =
        static_cast<std::ptrdiff_t>(originalColumn) + adjustment;

    if (adjustedColumn > 0) {
        position->column = static_cast<std::size_t>(adjustedColumn);
    }
}

void adjustFieldPosition(
    std::optional<SourceField>& field,
    const std::vector<Replacement>& replacements) {

    if (!field) {
        return;
    }

    std::optional<SourcePosition> position{field->position};

    adjustPosition(position, replacements);

    if (position) {
        field->position = *position;
    }
}

std::vector<Replacement> collectReplacements(
    const TemplateLine& templateLine,
    const std::vector<ActiveBinding>& bindings) {

    std::vector<Replacement> replacements;

    for (const auto& reference : templateLine.references) {

        // Procura os argumentos do escopo que declarou o parâmetro.
        if (reference.scopeDepth >= bindings.size()) {
            continue;
        }

        const auto* arguments =
            bindings[reference.scopeDepth].arguments;

        // Um escopo interno ainda não está sendo expandido.
        // Seus próprios parâmetros devem permanecer intactos.
        if (!arguments) {
            continue;
        }

        if (reference.formalIndex >= arguments->values.size()) {
            continue;
        }

        const auto* field =
            findSourceField(templateLine.source, reference.field);

        if (!field) {
            continue;
        }

        if (reference.begin > field->text.size()) {
            continue;
        }

        if (reference.length >
            field->text.size() - reference.begin) {
            continue;
        }

        const std::size_t absoluteBegin =
            field->position.column - 1 + reference.begin;

        if (absoluteBegin > templateLine.source.text.size()) {
            continue;
        }

        if (reference.length >
            templateLine.source.text.size() - absoluteBegin) {
            continue;
        }

        replacements.push_back({
            reference.field,
            reference.begin,
            reference.length,
            absoluteBegin,
            arguments->values[reference.formalIndex],
        });
    }

    return replacements;
}

void applyFieldReplacements(
    SourceLine& line,
    TemplateField fieldKind,
    const std::vector<Replacement>& replacements) {

    auto* field = findSourceField(line, fieldKind);

    if (!field) {
        return;
    }

    std::vector<const Replacement*> fieldReplacements;

    for (const auto& replacement : replacements) {
        if (replacement.field == fieldKind) {
            fieldReplacements.push_back(&replacement);
        }
    }

    // Substituir da direita para a esquerda evita invalidar
    // os índices das ocorrências anteriores.
    std::sort(
        fieldReplacements.begin(),
        fieldReplacements.end(),
        [](const Replacement* left, const Replacement* right) {
            return left->begin > right->begin;
        });

    for (const auto* replacement : fieldReplacements) {
        field->text.replace(
            replacement->begin,
            replacement->length,
            replacement->value);
    }
}

SourceLine substituteLine(
    const TemplateLine& templateLine,
    const std::vector<ActiveBinding>& bindings) {

    SourceLine result = templateLine.source;

    auto replacements =
        collectReplacements(templateLine, bindings);

    // Primeiro modifica o texto completo da linha.
    // A ordem decrescente preserva os offsets originais.
    std::sort(
        replacements.begin(),
        replacements.end(),
        [](const Replacement& left, const Replacement& right) {
            return left.absoluteBegin > right.absoluteBegin;
        });

    for (const auto& replacement : replacements) {
        result.text.replace(
            replacement.absoluteBegin,
            replacement.length,
            replacement.value);
    }

    // Atualiza os campos individuais para que continuem
    // consistentes com o texto completo da linha.
    applyFieldReplacements(
        result,
        TemplateField::Label,
        replacements);

    applyFieldReplacements(
        result,
        TemplateField::Operand,
        replacements);

    // Ajusta as posições dos campos que aparecem depois
    // de uma substituição cujo tamanho foi alterado.
    adjustFieldPosition(result.label, replacements);
    adjustFieldPosition(result.operation, replacements);
    adjustFieldPosition(result.operand, replacements);
    adjustFieldPosition(result.comment, replacements);

    adjustPosition(result.openQuote, replacements);

    return result;
}

void appendElements(
    const TemplateScope& scope,
    const std::vector<ActiveBinding>& bindings,
    std::vector<SourceLine>& output);

void appendNestedScope(
    const TemplateScope& scope,
    const std::vector<ActiveBinding>& bindings,
    std::vector<SourceLine>& output) {

    // Ao emitir uma definição interna, preservamos suas
    // linhas estruturais e seu protótipo.
    output.push_back(substituteLine(scope.beginLine, bindings));

    output.push_back(substituteLine(scope.prototypeLine, bindings));

    appendElements(scope, bindings, output);

    output.push_back(substituteLine(scope.endLine, bindings));
}

void appendElements(
    const TemplateScope& scope,
    const std::vector<ActiveBinding>& bindings,
    std::vector<SourceLine>& output) {

    for (const auto& element : scope.elements) {

        if (const auto* line = std::get_if<TemplateLine>(&element)) {
            output.push_back(substituteLine(*line, bindings));
            continue;
        }

        const auto* nested =
            std::get_if<std::shared_ptr<TemplateScope>>(&element);

        if (!nested || !*nested) {
            continue;
        }

        // O novo escopo ainda não possui argumentos associados.
        // Seus parâmetros ficam na posição zero; os argumentos
        // dos escopos externos são deslocados uma posição.
        auto nestedBindings = bindings;

        nestedBindings.insert(
            nestedBindings.begin(),
            ActiveBinding{nullptr});

        appendNestedScope(
            **nested,
            nestedBindings,
            output);
    }
}

} // namespace

std::vector<SourceLine> ParameterSubstitution::substitute(
    const MacroTemplate& macroTemplate,
    const BoundMacroArguments& arguments) {

    std::vector<SourceLine> output;

    // A macro principal já está sendo expandida e, portanto,
    // seus parâmetros estão disponíveis na posição zero.
    const std::vector<ActiveBinding> bindings{
        ActiveBinding{&arguments},
    };

    // Não emitimos MCDEFN, protótipo e MCEND da macro principal.
    // Emitimos somente seu corpo.
    appendElements(
        macroTemplate.root(),
        bindings,
        output);

    return output;
}

} // namespace macro
