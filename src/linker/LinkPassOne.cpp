#include "LinkPassOne.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace linker {
namespace {

ObjectPosition at(ObjectPosition position, std::size_t offset) {
    position.column += offset;
    return position;
}

bool isAsciiLetter(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool isAsciiDigit(char value) {
    return value >= '0' && value <= '9';
}

std::optional<std::size_t> invalidNameOffset(std::string_view value) {
    if (value.empty() || !isAsciiLetter(value.front())) {
        return 0;
    }
    for (std::size_t index = 1; index < value.size(); ++index) {
        if (index >= 6 ||
            (!isAsciiLetter(value[index]) && !isAsciiDigit(value[index]) && value[index] != '_')) {
            return index;
        }
    }
    return std::nullopt;
}

std::string formatPosition(const ObjectPosition& position) {
    std::string text;
    if (!position.file.empty()) {
        text += position.file + ":";
    }
    text += std::to_string(position.record) + ":" + std::to_string(position.column);
    return text;
}

ObjectDiagnostic makeDuplicateSymbolDiagnostic(std::string_view name,
                                               const ObjectPosition& position,
                                               const GlobalSymbol& existing) {
    return ObjectDiagnostic{
        ObjectDiagnosticCode::DuplicateName,
        position,
        "Simbolo global '" + std::string{name} + "' duplicado; '" + existing.name +
            "' ja foi definido em " + formatPosition(existing.position) + ".",
    };
}

} // namespace

std::string GlobalSymbolTable::normalizeName(std::string_view name) {
    std::string key{name};
    for (char& value : key) {
        if (value >= 'a' && value <= 'z') {
            value = static_cast<char>(value - 'a' + 'A');
        }
    }
    return key;
}

bool GlobalSymbolTable::isValidName(std::string_view name) {
    return !invalidNameOffset(name).has_value();
}

std::optional<ObjectDiagnostic> GlobalSymbolTable::defineSection(std::string_view name,
                                                                 std::size_t moduleIndex,
                                                                 std::uint32_t baseOffset,
                                                                 ObjectPosition position) {
    if (const auto invalid = invalidNameOffset(name)) {
        return ObjectDiagnostic{
            ObjectDiagnosticCode::InvalidName,
            at(position, *invalid),
            "Nome de secao invalido.",
        };
    }
    const std::string key = normalizeName(name);
    if (const auto it = indexByName_.find(key); it != indexByName_.end()) {
        return makeDuplicateSymbolDiagnostic(name, position, symbols_[it->second]);
    }
    const std::size_t index = symbols_.size();
    symbols_.push_back(GlobalSymbol{
        std::string{name},
        key,
        GlobalSymbolKind::Section,
        moduleIndex,
        std::string{name},
        baseOffset,
        0U,
        baseOffset,
        std::move(position),
    });
    indexByName_.emplace(key, index);
    return std::nullopt;
}

std::optional<ObjectDiagnostic> GlobalSymbolTable::defineSymbol(std::string_view name,
                                                                std::size_t moduleIndex,
                                                                std::string_view sectionName,
                                                                std::uint32_t sectionBaseOffset,
                                                                std::uint32_t relativeOffset,
                                                                ObjectPosition position) {
    if (const auto invalid = invalidNameOffset(name)) {
        return ObjectDiagnostic{
            ObjectDiagnosticCode::InvalidName,
            at(position, *invalid),
            "Nome de definicao invalido.",
        };
    }
    const std::string key = normalizeName(name);
    if (const auto it = indexByName_.find(key); it != indexByName_.end()) {
        return makeDuplicateSymbolDiagnostic(name, position, symbols_[it->second]);
    }
    const std::size_t index = symbols_.size();
    symbols_.push_back(GlobalSymbol{
        std::string{name},
        key,
        GlobalSymbolKind::DefinedSymbol,
        moduleIndex,
        std::string{sectionName},
        sectionBaseOffset,
        relativeOffset,
        sectionBaseOffset + relativeOffset,
        std::move(position),
    });
    indexByName_.emplace(key, index);
    return std::nullopt;
}

std::optional<GlobalSymbol> GlobalSymbolTable::find(std::string_view name) const {
    const std::string key = normalizeName(name);
    if (const auto it = indexByName_.find(key); it != indexByName_.end()) {
        return symbols_[it->second];
    }
    return std::nullopt;
}

std::optional<std::uint32_t> GlobalSymbolTable::resolve(std::string_view name) const {
    if (const auto symbol = find(name)) {
        return symbol->globalOffset;
    }
    return std::nullopt;
}

bool GlobalSymbolTable::contains(std::string_view name) const {
    return indexByName_.contains(normalizeName(name));
}

bool GlobalSymbolTable::isDefined(std::string_view name) const {
    return contains(name);
}

const std::vector<GlobalSymbol>& GlobalSymbolTable::symbols() const noexcept {
    return symbols_;
}

std::size_t GlobalSymbolTable::size() const noexcept {
    return symbols_.size();
}

bool GlobalSymbolTable::empty() const noexcept {
    return symbols_.empty();
}

std::optional<GlobalSymbol> LinkPassOneResult::findSymbol(std::string_view name) const {
    return symbolTable.find(name);
}

std::optional<std::uint32_t> LinkPassOneResult::resolveSymbol(std::string_view name) const {
    return symbolTable.resolve(name);
}

std::optional<SectionLayout> LinkPassOneResult::findSection(std::string_view name) const {
    const std::string key = GlobalSymbolTable::normalizeName(name);
    for (const auto& section : sections) {
        if (section.key == key) {
            return section;
        }
    }
    return std::nullopt;
}

LinkPassOneResult LinkPassOne::run(std::span<const ObjectModule> modules) {
    LinkPassOneResult result;
    if (modules.empty()) {
        result.diagnostics.push_back(ObjectDiagnostic{
            ObjectDiagnosticCode::MissingRecord,
            ObjectPosition{"", 1, 1},
            "A ligacao exige pelo menos um modulo objeto.",
        });
        return result;
    }

    result.sections.reserve(modules.size());
    std::uint64_t runningOffset = 0;
    bool addressSpaceOverflowed = false;
    std::optional<LinkedEntryPoint> firstExplicitEntry;

    for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        const auto& module = modules[moduleIndex];
        const auto invalidHeaderName = invalidNameOffset(module.header.name);
        if (invalidHeaderName.has_value()) {
            result.diagnostics.push_back(ObjectDiagnostic{
                ObjectDiagnosticCode::InvalidName,
                at(module.header.position, 1 + *invalidHeaderName),
                "Nome de secao invalido.",
            });
        }

        const bool singleLengthValid = module.header.length <= ObjectAddressSpaceBytes;
        if (!singleLengthValid) {
            result.diagnostics.push_back(ObjectDiagnostic{
                ObjectDiagnosticCode::ValueOutOfRange,
                at(module.header.position, 13),
                "Comprimento excede o espaco de 20 bits.",
            });
        } else if (!addressSpaceOverflowed &&
                   module.header.length > ObjectAddressSpaceBytes - runningOffset) {
            result.diagnostics.push_back(ObjectDiagnostic{
                ObjectDiagnosticCode::ValueOutOfRange,
                at(module.header.position, 13),
                "Comprimento combinado das secoes excede o espaco de 20 bits.",
            });
            addressSpaceOverflowed = true;
        }

        const auto sectionBase = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(runningOffset, ObjectAddressSpaceBytes));
        const std::string sectionKey = GlobalSymbolTable::normalizeName(module.header.name);

        result.sections.push_back(SectionLayout{
            module.header.name,
            sectionKey,
            moduleIndex,
            sectionBase,
            module.header.length,
            module.header.position,
        });

        if (singleLengthValid && !addressSpaceOverflowed) {
            runningOffset += module.header.length;
        }

        std::unordered_set<std::string> moduleNames;
        if (!invalidHeaderName.has_value()) {
            moduleNames.insert(sectionKey);
            if (auto diagnostic = result.symbolTable.defineSection(
                    module.header.name, moduleIndex, sectionBase, module.header.position)) {
                result.diagnostics.push_back(std::move(*diagnostic));
            }
        }

        for (const auto& definition : module.definitions) {
            if (const auto invalid = invalidNameOffset(definition.name)) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::InvalidName,
                    at(definition.position, *invalid),
                    "Nome de definicao invalido.",
                });
                continue;
            }
            const std::string defKey = GlobalSymbolTable::normalizeName(definition.name);
            const bool duplicateInModule = !moduleNames.insert(defKey).second;
            if (definition.offset >= module.header.length ||
                definition.offset >= ObjectAddressSpaceBytes) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::ValueOutOfRange,
                    at(definition.position, 6),
                    "Definicao fora da secao.",
                });
                continue;
            }
            if (const auto existing = result.symbolTable.find(defKey)) {
                result.diagnostics.push_back(
                    makeDuplicateSymbolDiagnostic(definition.name, definition.position, *existing));
            } else if (duplicateInModule) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::DuplicateName,
                    definition.position,
                    "Nome de secao/definicao/referencia repetido.",
                });
            } else if (auto diagnostic = result.symbolTable.defineSymbol(definition.name,
                                                                         moduleIndex,
                                                                         module.header.name,
                                                                         sectionBase,
                                                                         definition.offset,
                                                                         definition.position)) {
                result.diagnostics.push_back(std::move(*diagnostic));
            }
        }

        std::unordered_set<std::string> declaredReferences;
        for (const auto& reference : module.references) {
            if (const auto invalid = invalidNameOffset(reference.name)) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::InvalidName,
                    at(reference.position, *invalid),
                    "Nome de referencia invalido.",
                });
                continue;
            }
            std::string refKey = GlobalSymbolTable::normalizeName(reference.name);
            if (!moduleNames.insert(refKey).second) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::DuplicateName,
                    reference.position,
                    "Nome de secao/definicao/referencia repetido.",
                });
            }
            declaredReferences.insert(std::move(refKey));
        }

        const std::uint32_t effectiveLength =
            std::min<std::uint32_t>(module.header.length, ObjectAddressSpaceBytes);
        std::vector<bool> present(effectiveLength, false);
        for (const auto& text : module.text) {
            if (text.bytes.empty() || text.bytes.size() > MaxTextRecordBytes) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::InvalidTextLength,
                    at(text.position, 7),
                    "Registro T exige de 1 a 30 bytes.",
                });
                continue;
            }
            if (text.offset > effectiveLength ||
                text.bytes.size() > effectiveLength - text.offset) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::ValueOutOfRange,
                    at(text.position, 1),
                    "Texto fora da secao.",
                });
                continue;
            }
            bool overlapped = false;
            for (std::size_t index = 0; index < text.bytes.size(); ++index) {
                if (present[text.offset + index]) {
                    overlapped = true;
                }
                present[text.offset + index] = true;
            }
            if (overlapped) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::OverlappingText,
                    at(text.position, 1),
                    "Registros T sobrepostos.",
                });
            }
        }

        for (const auto& modification : module.modifications) {
            if (modification.halfBytes != 5 && modification.halfBytes != 6) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::InvalidModification,
                    at(modification.position, 7),
                    "Comprimento de M deve ser 05 ou 06 meios bytes.",
                });
                continue;
            }
            if (modification.sign != ModificationSign::Add &&
                modification.sign != ModificationSign::Subtract) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::InvalidModification,
                    at(modification.position, 9),
                    "Sinal de modificacao invalido.",
                });
                continue;
            }
            if (modification.offset >= effectiveLength ||
                3U > effectiveLength - modification.offset) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::ValueOutOfRange,
                    at(modification.position, 1),
                    "Modificacao fora da secao.",
                });
                continue;
            }
            bool covered = true;
            for (std::uint32_t index = 0; index < 3; ++index) {
                if (!present[modification.offset + index]) {
                    covered = false;
                    break;
                }
            }
            if (!covered) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::MissingText,
                    at(modification.position, 1),
                    "Modificacao exige tres bytes presentes em T.",
                });
                continue;
            }
            if (const auto invalid = invalidNameOffset(modification.symbol)) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::InvalidName,
                    at(modification.position, 10 + *invalid),
                    "Nome de modificacao invalido.",
                });
                continue;
            }
            const std::string modKey = GlobalSymbolTable::normalizeName(modification.symbol);
            const bool isSectionOrigin = !invalidHeaderName.has_value() && modKey == sectionKey;
            if (!isSectionOrigin && !declaredReferences.contains(modKey)) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::UndeclaredReference,
                    at(modification.position, 10),
                    "Simbolo de M nao declarado em R nem igual a secao.",
                });
            }
        }

        if (module.entry.offset.has_value()) {
            const bool entryInRange = *module.entry.offset < module.header.length &&
                                      *module.entry.offset < ObjectAddressSpaceBytes;
            if (!entryInRange) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::ValueOutOfRange,
                    at(module.entry.position, 1),
                    "Entrada fora da secao.",
                });
            }
            if (firstExplicitEntry.has_value()) {
                result.diagnostics.push_back(ObjectDiagnostic{
                    ObjectDiagnosticCode::DuplicateEntry,
                    module.entry.position,
                    "Entrada explicita duplicada em E; ja definida na secao '" +
                        firstExplicitEntry->sectionName + "' em " +
                        formatPosition(firstExplicitEntry->position) + ".",
                });
            } else {
                firstExplicitEntry = LinkedEntryPoint{
                    moduleIndex,
                    module.header.name,
                    sectionBase,
                    *module.entry.offset,
                    sectionBase + *module.entry.offset,
                    true,
                    module.entry.position,
                };
                if (entryInRange) {
                    result.explicitEntry = *firstExplicitEntry;
                }
            }
        }
    }

    result.totalLength =
        static_cast<std::uint32_t>(std::min<std::uint64_t>(runningOffset, ObjectAddressSpaceBytes));
    result.symbols = result.symbolTable.symbols();
    if (result.explicitEntry.has_value()) {
        result.entry = *result.explicitEntry;
        result.entryOffset = result.explicitEntry->globalOffset;
    } else {
        result.entry = LinkedEntryPoint{
            0,
            modules.front().header.name,
            0,
            0,
            0,
            false,
            modules.front().entry.position,
        };
        result.entryOffset = 0;
    }

    return result;
}

LinkPassOneResult runLinkPassOne(std::span<const ObjectModule> modules) {
    return LinkPassOne::run(modules);
}

} // namespace linker
