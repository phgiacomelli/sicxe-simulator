#include "ObjectValidation.hpp"

namespace linker::detail {
namespace {

ObjectPosition at(ObjectPosition position, std::size_t offset) {
    position.column += offset;
    return position;
}

bool letter(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

} // namespace

std::optional<std::size_t> invalidName(std::string_view value) {
    if (value.empty() || !letter(value.front()))
        return 0;
    for (std::size_t index = 1; index < value.size(); ++index)
        if (index >= 6 || (!letter(value[index]) && !(value[index] >= '0' && value[index] <= '9') &&
                           value[index] != '_'))
            return index;
    return std::nullopt;
}

std::string normalize(std::string_view name) {
    std::string key{name};
    for (auto& value : key)
        if (value >= 'a' && value <= 'z')
            value = static_cast<char>(value - 'a' + 'A');
    return key;
}

std::optional<ObjectDiagnostic> validateTextRecord(const TextRecord& text,
                                                   std::vector<bool>& present) {
    if (text.bytes.empty() || text.bytes.size() > MaxTextRecordBytes)
        return ObjectDiagnostic{ObjectDiagnosticCode::InvalidTextLength,
                                at(text.position, 7),
                                "Registro T exige de 1 a 30 bytes."};
    if (text.offset > present.size() || text.bytes.size() > present.size() - text.offset)
        return ObjectDiagnostic{
            ObjectDiagnosticCode::ValueOutOfRange, at(text.position, 1), "Texto fora da secao."};
    bool overlapped = false;
    for (std::size_t index = 0; index < text.bytes.size(); ++index) {
        if (present[text.offset + index])
            overlapped = true;
        present[text.offset + index] = true;
    }
    if (overlapped)
        return ObjectDiagnostic{ObjectDiagnosticCode::OverlappingText,
                                at(text.position, 1),
                                "Registros T sobrepostos."};
    return std::nullopt;
}

std::optional<ObjectDiagnostic>
validateModificationRecord(const ModificationRecord& modification,
                           const std::vector<bool>& present,
                           std::string_view section,
                           const std::unordered_set<std::string>& references) {
    if (modification.halfBytes != 5 && modification.halfBytes != 6)
        return ObjectDiagnostic{ObjectDiagnosticCode::InvalidModification,
                                at(modification.position, 7),
                                "Comprimento de M deve ser 05 ou 06 meios bytes."};
    if (modification.sign != ModificationSign::Add &&
        modification.sign != ModificationSign::Subtract)
        return ObjectDiagnostic{ObjectDiagnosticCode::InvalidModification,
                                at(modification.position, 9),
                                "Sinal de modificacao invalido."};
    if (modification.offset >= present.size() || 3U > present.size() - modification.offset)
        return ObjectDiagnostic{ObjectDiagnosticCode::ValueOutOfRange,
                                at(modification.position, 1),
                                "Modificacao fora da secao."};
    for (std::uint32_t index = 0; index < 3; ++index)
        if (!present[modification.offset + index])
            return ObjectDiagnostic{ObjectDiagnosticCode::MissingText,
                                    at(modification.position, 1),
                                    "Modificacao exige tres bytes presentes em T."};
    if (const auto invalid = invalidName(modification.symbol))
        return ObjectDiagnostic{ObjectDiagnosticCode::InvalidName,
                                at(modification.position, 10 + *invalid),
                                "Nome de modificacao invalido."};
    const auto symbol = normalize(modification.symbol);
    if (symbol != section && !references.contains(symbol))
        return ObjectDiagnostic{ObjectDiagnosticCode::UndeclaredReference,
                                at(modification.position, 10),
                                "Simbolo de M nao declarado em R nem igual a secao."};
    return std::nullopt;
}

} // namespace linker::detail
