#include "LinkPassTwo.hpp"

#include "ObjectValidation.hpp"

#include <map>

namespace linker {
namespace {

using FieldKey = std::pair<std::uint32_t, std::uint8_t>;

std::uint32_t readField(const std::vector<std::optional<std::uint8_t>>& text,
                        std::uint32_t offset,
                        std::uint8_t halfBytes) {
    const auto first = *text[offset];
    const auto second = *text[offset + 1U];
    const auto third = *text[offset + 2U];
    if (halfBytes == 5U) {
        return (static_cast<std::uint32_t>(first & 0x0FU) << 16U) |
               (static_cast<std::uint32_t>(second) << 8U) | third;
    }
    return (static_cast<std::uint32_t>(first) << 16U) | (static_cast<std::uint32_t>(second) << 8U) |
           third;
}

void writeField(std::vector<std::optional<std::uint8_t>>& text,
                std::uint32_t offset,
                std::uint8_t halfBytes,
                std::uint32_t value) {
    const auto first = *text[offset];
    if (halfBytes == 5U) {
        text[offset] = static_cast<std::uint8_t>((first & 0xF0U) | ((value >> 16U) & 0x0FU));
    } else {
        text[offset] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    }
    text[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    text[offset + 2U] = static_cast<std::uint8_t>(value & 0xFFU);
}

ObjectDiagnostic
makeDiagnostic(ObjectDiagnosticCode code, const ObjectPosition& position, std::string message) {
    return ObjectDiagnostic{code, position, std::move(message)};
}

} // namespace

LinkPassTwoResult LinkPassTwo::run(std::span<const ObjectModule> modules,
                                   const LinkPassOneResult& passOne) {
    LinkPassTwoResult result;
    if (!passOne.succeeded()) {
        result.diagnostics = passOne.diagnostics;
        return result;
    }
    const auto expectedPassOne = LinkPassOne::run(modules);
    if (!expectedPassOne.succeeded() || expectedPassOne != passOne) {
        result.diagnostics.push_back(makeDiagnostic(
            ObjectDiagnosticCode::ValueOutOfRange,
            ObjectPosition{"", 1, 1},
            "Resultado da primeira passagem nao corresponde aos modulos recebidos."));
        return result;
    }
    LinkPassTwoIntermediate intermediate;
    intermediate.text.resize(passOne.totalLength);
    intermediate.totalLength = passOne.totalLength;
    intermediate.entryOffset = passOne.entryOffset;

    for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        const auto& section = passOne.sections[moduleIndex];
        if (section.moduleIndex != moduleIndex ||
            section.length != modules[moduleIndex].header.length ||
            section.key != GlobalSymbolTable::normalizeName(modules[moduleIndex].header.name) ||
            section.baseOffset > passOne.totalLength ||
            section.length > passOne.totalLength - section.baseOffset) {
            result.diagnostics.push_back(makeDiagnostic(
                ObjectDiagnosticCode::ValueOutOfRange,
                modules[moduleIndex].header.position,
                "Layout da primeira passagem nao corresponde aos modulos recebidos."));
            return result;
        }
        for (const auto& text : modules[moduleIndex].text) {
            if (text.offset > section.length || text.bytes.size() > section.length - text.offset) {
                result.diagnostics.push_back(makeDiagnostic(
                    ObjectDiagnosticCode::ValueOutOfRange, text.position, "Texto fora da secao."));
                continue;
            }
            for (std::size_t index = 0; index < text.bytes.size(); ++index) {
                intermediate.text[section.baseOffset + text.offset + index] = text.bytes[index];
            }
        }
    }
    if (!result.diagnostics.empty())
        return result;

    for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        const auto& module = modules[moduleIndex];
        const auto& section = passOne.sections[moduleIndex];
        std::map<FieldKey, std::vector<const ModificationRecord*>> fields;
        for (const auto& modification : module.modifications) {
            if (modification.halfBytes != 5U && modification.halfBytes != 6U) {
                result.diagnostics.push_back(
                    makeDiagnostic(ObjectDiagnosticCode::InvalidModification,
                                   modification.position,
                                   "Comprimento de M deve ser 05 ou 06 meios bytes."));
                continue;
            }
            if (modification.offset > section.length || 3U > section.length - modification.offset ||
                section.baseOffset > intermediate.totalLength ||
                modification.offset > intermediate.totalLength - section.baseOffset ||
                3U > intermediate.totalLength - section.baseOffset - modification.offset) {
                result.diagnostics.push_back(
                    makeDiagnostic(ObjectDiagnosticCode::ValueOutOfRange,
                                   modification.position,
                                   "Modificacao fora do texto combinado."));
                continue;
            }
            fields[{section.baseOffset + modification.offset, modification.halfBytes}].push_back(
                &modification);
        }

        // Different widths at one address describe incompatible field layouts.
        std::map<std::uint32_t, std::uint8_t> widthAtOffset;
        for (const auto& [key, records] : fields) {
            const auto [offset, width] = key;
            const auto [it, inserted] = widthAtOffset.emplace(offset, width);
            if (!inserted && it->second != width) {
                result.diagnostics.push_back(
                    makeDiagnostic(ObjectDiagnosticCode::InvalidModification,
                                   records.front()->position,
                                   "Modificacoes no mesmo campo devem ter o mesmo comprimento."));
            }
            for (std::uint32_t byte = 0; byte < 3U; ++byte) {
                if (!intermediate.text[offset + byte].has_value()) {
                    result.diagnostics.push_back(
                        makeDiagnostic(ObjectDiagnosticCode::MissingText,
                                       records.front()->position,
                                       "Modificacao aponta para buraco no texto."));
                    break;
                }
            }
        }
        if (!result.diagnostics.empty())
            continue;

        for (const auto& [key, records] : fields) {
            const auto [offset, width] = key;
            std::int64_t knownContribution = 0;
            std::int64_t coefficient = 0;
            bool valid = true;
            for (const auto* record : records) {
                const auto symbol = passOne.symbolTable.find(record->symbol);
                if (!symbol) {
                    result.diagnostics.push_back(makeDiagnostic(
                        ObjectDiagnosticCode::UndeclaredReference,
                        record->position,
                        "Simbolo global de modificacao nao definido: '" + record->symbol + "'."));
                    valid = false;
                    continue;
                }
                const std::int64_t sign = record->sign == ModificationSign::Add ? 1 : -1;
                knownContribution += sign * static_cast<std::int64_t>(symbol->globalOffset);
                coefficient += sign;
            }
            if (!valid)
                continue;

            const std::uint32_t mask = width == 5U ? 0xFFFFFU : 0xFFFFFFU;
            const std::uint32_t addend = readField(intermediate.text, offset, width);
            const auto patched = static_cast<std::uint32_t>(
                (static_cast<std::int64_t>(addend) + knownContribution) & mask);
            writeField(intermediate.text, offset, width, patched);
            if (coefficient != 0) {
                intermediate.residuals.push_back(LinkModificationResidual{
                    offset, width, coefficient, records.front()->position});
            }
        }
    }

    if (!result.diagnostics.empty())
        return result;
    result.intermediate = std::move(intermediate);
    return result;
}

LinkPassTwoResult runLinkPassTwo(std::span<const ObjectModule> modules,
                                 const LinkPassOneResult& passOne) {
    return LinkPassTwo::run(modules, passOne);
}

} // namespace linker
