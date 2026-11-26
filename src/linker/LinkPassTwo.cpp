#include "LinkPassTwo.hpp"

#include "ObjectValidation.hpp"
#include "RelocationField.hpp"

#include <array>
#include <map>
#include <unordered_set>
#include <utility>

namespace linker {
namespace {

using FieldKey = std::pair<std::uint32_t, std::uint8_t>;

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
    if (modules.empty() || passOne.sections.size() != modules.size() ||
        passOne.totalLength > ObjectAddressSpaceBytes ||
        (passOne.totalLength == 0 ? passOne.entryOffset != 0
                                  : passOne.entryOffset >= passOne.totalLength)) {
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

    std::uint32_t nextBase = 0;
    for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        const auto& module = modules[moduleIndex];
        const auto& section = passOne.sections[moduleIndex];
        if (section.moduleIndex != moduleIndex || section.baseOffset != nextBase ||
            section.key != detail::normalize(module.header.name) ||
            section.length != module.header.length ||
            section.length > passOne.totalLength - nextBase) {
            result.diagnostics.push_back(makeDiagnostic(
                ObjectDiagnosticCode::ValueOutOfRange,
                module.header.position,
                "Layout da primeira passagem nao corresponde aos modulos recebidos."));
            return result;
        }
        nextBase += section.length;
        const auto diagnosticsBeforeModule = result.diagnostics.size();
        std::vector<bool> present(section.length, false);
        for (const auto& text : module.text) {
            if (auto error = detail::validateTextRecord(text, present)) {
                result.diagnostics.push_back(std::move(*error));
                continue;
            }
            for (std::size_t index = 0; index < text.bytes.size(); ++index)
                intermediate.text[section.baseOffset + text.offset + index] = text.bytes[index];
        }
        std::unordered_set<std::string> references;
        for (const auto& reference : module.references)
            references.insert(detail::normalize(reference.name));

        std::map<FieldKey, std::vector<const ModificationRecord*>> fields;
        for (const auto& modification : module.modifications) {
            if (auto error = detail::validateModificationRecord(
                    modification, present, section.key, references)) {
                result.diagnostics.push_back(std::move(*error));
                continue;
            }
            fields[{section.baseOffset + modification.offset, modification.halfBytes}].push_back(
                &modification);
        }
        if (result.diagnostics.size() != diagnosticsBeforeModule)
            continue;

        // Repeated terms in one field combine. Distinct fields must be disjoint
        // so their remaining load-address terms can be applied independently.
        std::uint32_t previousEnd = section.baseOffset;
        for (const auto& [key, records] : fields) {
            const auto offset = key.first;
            if (offset < previousEnd) {
                result.diagnostics.push_back(
                    makeDiagnostic(ObjectDiagnosticCode::InvalidModification,
                                   records.front()->position,
                                   "Campos de modificacao distintos compartilham bytes."));
            }
            previousEnd = offset + 3U;
        }
        if (result.diagnostics.size() != diagnosticsBeforeModule)
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

            const auto fieldWidth = static_cast<RelocationWidth>(width);
            const auto mask = relocationMask(fieldWidth);
            std::array field{*intermediate.text[offset],
                             *intermediate.text[offset + 1U],
                             *intermediate.text[offset + 2U]};
            const auto addend = static_cast<std::int64_t>(readRelocationField(field, fieldWidth));
            const auto valueBeforeLoad = addend + knownContribution;
            const auto patched = static_cast<std::uint32_t>(valueBeforeLoad) & mask;
            writeRelocationField(field, fieldWidth, patched);
            for (std::size_t index = 0; index < field.size(); ++index) {
                intermediate.text[offset + index] = field[index];
            }
            const bool needsAddressCheck =
                width == 5U && (valueBeforeLoad < 0 || valueBeforeLoad > mask);
            if (coefficient != 0 || needsAddressCheck) {
                intermediate.residuals.push_back(
                    LinkModificationResidual{offset,
                                             width,
                                             coefficient,
                                             records.front()->position,
                                             width == 5U ? valueBeforeLoad : patched});
            }
        }
    }

    if (nextBase != passOne.totalLength) {
        result.diagnostics.push_back(
            makeDiagnostic(ObjectDiagnosticCode::ValueOutOfRange,
                           modules.back().header.position,
                           "Comprimento combinado diverge da primeira passagem."));
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
