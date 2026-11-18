#include "ObjectRecords.hpp"

#include "ObjectValidation.hpp"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace linker {
namespace {

using detail::invalidName;
using detail::normalize;

ObjectPosition at(ObjectPosition position, std::size_t offset) {
    position.column += offset;
    return position;
}

ObjectFailure failure(ObjectDiagnosticCode code, ObjectPosition position, std::string message) {
    return {{{code, std::move(position), std::move(message)}}};
}

class Fields {
public:
    std::string_view text;
    ObjectPosition position;
    std::optional<ObjectDiagnostic> error;

    void reject(ObjectDiagnosticCode code, std::size_t columnOffset, std::string message) {
        if (!error)
            error = ObjectDiagnostic{code, at(position, columnOffset), std::move(message)};
    }

    bool size(std::size_t expected) {
        if (text.size() == expected)
            return true;
        reject(ObjectDiagnosticCode::InvalidRecordLength,
               std::min(text.size(), expected),
               "Comprimento do registro invalido.");
        return false;
    }

    std::uint32_t number(std::size_t start, std::size_t width) {
        if (start > text.size() || width > text.size() - start) {
            reject(ObjectDiagnosticCode::InvalidRecordLength, text.size(), "Campo truncado.");
            return 0;
        }
        std::uint32_t value = 0;
        for (std::size_t index = start; index < start + width; ++index) {
            const char character = text[index];
            unsigned int digit;
            if (character >= '0' && character <= '9')
                digit = character - '0';
            else if (character >= 'A' && character <= 'F')
                digit = character - 'A' + 10;
            else if (character >= 'a' && character <= 'f')
                digit = character - 'a' + 10;
            else {
                reject(ObjectDiagnosticCode::InvalidHex, index, "Digito hexadecimal invalido.");
                return 0;
            }
            value = value * 16 + digit;
        }
        return value;
    }

    std::string name(std::size_t start) {
        if (start > text.size() || 6 > text.size() - start) {
            reject(ObjectDiagnosticCode::InvalidRecordLength, text.size(), "Nome truncado.");
            return {};
        }
        auto value = text.substr(start, 6);
        while (!value.empty() && value.back() == ' ')
            value.remove_suffix(1);
        if (const auto invalid = invalidName(value))
            reject(ObjectDiagnosticCode::InvalidName, start + *invalid, "Nome objeto invalido.");
        return std::string{value};
    }
};

void hex(std::ostream& output, std::uint32_t value, int width) {
    output << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value
           << std::dec;
}

void name(std::ostream& output, const std::string& value) {
    output << value;
    if (value.size() < 6)
        output << std::string(6 - value.size(), ' ');
}

std::optional<ObjectDiagnostic> validate(const ObjectModule& module) {
    if (const auto invalid = invalidName(module.header.name))
        return ObjectDiagnostic{ObjectDiagnosticCode::InvalidName,
                                at(module.header.position, 1 + *invalid),
                                "Nome de secao invalido."};
    if (module.header.length > ObjectAddressSpaceBytes)
        return ObjectDiagnostic{ObjectDiagnosticCode::ValueOutOfRange,
                                at(module.header.position, 13),
                                "Comprimento excede o espaco de 20 bits."};
    const auto section = normalize(module.header.name);
    std::unordered_set<std::string> names{section};
    for (const auto& definition : module.definitions) {
        if (const auto invalid = invalidName(definition.name))
            return ObjectDiagnostic{ObjectDiagnosticCode::InvalidName,
                                    at(definition.position, *invalid),
                                    "Nome de definicao invalido."};
        if (definition.offset >= module.header.length)
            return ObjectDiagnostic{ObjectDiagnosticCode::ValueOutOfRange,
                                    at(definition.position, 6),
                                    "Definicao fora da secao."};
        if (!names.insert(normalize(definition.name)).second)
            return ObjectDiagnostic{ObjectDiagnosticCode::DuplicateName,
                                    definition.position,
                                    "Nome de secao/definicao/referencia repetido."};
    }
    std::unordered_set<std::string> references;
    for (const auto& reference : module.references) {
        if (const auto invalid = invalidName(reference.name))
            return ObjectDiagnostic{ObjectDiagnosticCode::InvalidName,
                                    at(reference.position, *invalid),
                                    "Nome de referencia invalido."};
        auto key = normalize(reference.name);
        if (!names.insert(key).second)
            return ObjectDiagnostic{ObjectDiagnosticCode::DuplicateName,
                                    reference.position,
                                    "Nome de secao/definicao/referencia repetido."};
        references.insert(std::move(key));
    }
    std::vector<bool> present(module.header.length, false);
    for (const auto& text : module.text)
        if (auto diagnostic = detail::validateTextRecord(text, present))
            return diagnostic;
    for (const auto& modification : module.modifications)
        if (auto diagnostic =
                detail::validateModificationRecord(modification, present, section, references))
            return diagnostic;
    if (module.entry.offset && *module.entry.offset >= module.header.length)
        return ObjectDiagnostic{ObjectDiagnosticCode::ValueOutOfRange,
                                at(module.entry.position, 1),
                                "Entrada fora da secao."};
    return std::nullopt;
}

} // namespace

ObjectParseResult parseObject(std::string_view records, std::string_view fileName) {
    ObjectModule module;
    bool header = false;
    bool ended = false;
    std::size_t cursor = 0;
    std::size_t record = 1;
    std::size_t phase = 0;
    while (cursor < records.size()) {
        const auto separator = records.find_first_of("\r\n", cursor);
        const auto end = separator == std::string_view::npos ? records.size() : separator;
        Fields fields{records.substr(cursor, end - cursor), {std::string{fileName}, record, 1}, {}};
        if (ended)
            return failure(ObjectDiagnosticCode::UnexpectedRecord,
                           fields.position,
                           "Registro depois do encerramento E.");
        if (fields.text.empty())
            return failure(
                ObjectDiagnosticCode::InvalidRecordType, fields.position, "Registro vazio.");
        if (!header && fields.text.front() != 'H')
            return failure(
                ObjectDiagnosticCode::MissingRecord, fields.position, "Cabecalho H ausente.");
        const auto nextPhase = std::string_view{"HDRTME"}.find(fields.text.front());
        if (nextPhase == std::string_view::npos)
            return failure(ObjectDiagnosticCode::InvalidRecordType,
                           fields.position,
                           "Tipo de registro desconhecido.");
        if (nextPhase < phase)
            return failure(ObjectDiagnosticCode::UnexpectedRecord,
                           fields.position,
                           "Registro fora da ordem H/D/R/T/M/E.");
        phase = nextPhase;
        switch (fields.text.front()) {
            case 'H':
                if (header)
                    return failure(ObjectDiagnosticCode::UnexpectedRecord,
                                   fields.position,
                                   "Cabecalho H repetido.");
                if (fields.size(19)) {
                    module.header = {fields.name(1), fields.number(13, 6), fields.position};
                    if (fields.number(7, 6) != 0)
                        fields.reject(ObjectDiagnosticCode::NonZeroOrigin,
                                      7,
                                      "A origem da secao deve ser zero.");
                    if (module.header.length > ObjectAddressSpaceBytes)
                        fields.reject(ObjectDiagnosticCode::ValueOutOfRange,
                                      13,
                                      "Comprimento excede o espaco de 20 bits.");
                }
                header = true;
                break;
            case 'D':
            case 'R': {
                const auto width = fields.text.front() == 'D' ? 12U : 6U;
                if (fields.text.size() == 1 || (fields.text.size() - 1) % width != 0) {
                    fields.reject(ObjectDiagnosticCode::InvalidRecordLength,
                                  fields.text.size(),
                                  "Campos de definicao/referencia incompletos.");
                    break;
                }
                for (std::size_t start = 1; start < fields.text.size() && !fields.error;
                     start += width) {
                    const auto name = fields.name(start);
                    if (fields.text.front() == 'D')
                        module.definitions.push_back(
                            {name, fields.number(start + 6, 6), at(fields.position, start)});
                    else
                        module.references.push_back({name, at(fields.position, start)});
                }
                break;
            }
            case 'T': {
                if (fields.text.size() < 9) {
                    fields.size(9);
                    break;
                }
                const auto offset = fields.number(1, 6);
                const auto count = fields.number(7, 2);
                if (count == 0 || count > MaxTextRecordBytes) {
                    fields.reject(ObjectDiagnosticCode::InvalidTextLength,
                                  7,
                                  "Registro T exige de 1 a 30 bytes.");
                    break;
                }
                if (fields.size(9 + 2 * count)) {
                    TextRecord text{offset, {}, fields.position};
                    for (std::size_t index = 0; index < count && !fields.error; ++index)
                        text.bytes.push_back(
                            static_cast<std::uint8_t>(fields.number(9 + 2 * index, 2)));
                    module.text.push_back(std::move(text));
                }
                break;
            }
            case 'M': {
                if (!fields.size(fields.text.size() == 9 ? 9 : 16))
                    break;
                ModificationRecord modification{fields.number(1, 6),
                                                static_cast<std::uint8_t>(fields.number(7, 2)),
                                                ModificationSign::Add,
                                                module.header.name,
                                                fields.position};
                if (fields.text.size() == 16) {
                    if (fields.text[9] == '-')
                        modification.sign = ModificationSign::Subtract;
                    else if (fields.text[9] != '+')
                        fields.reject(ObjectDiagnosticCode::InvalidModification,
                                      9,
                                      "Sinal de modificacao deve ser '+' ou '-'.");
                    modification.symbol = fields.name(10);
                }
                module.modifications.push_back(std::move(modification));
                break;
            }
            case 'E':
                if (fields.size(fields.text.size() == 1 ? 1 : 7)) {
                    module.entry.position = fields.position;
                    if (fields.text.size() == 7) {
                        module.entry.offset = fields.number(1, 6);
                        if (*module.entry.offset >= module.header.length)
                            fields.reject(
                                ObjectDiagnosticCode::ValueOutOfRange, 1, "Entrada fora da secao.");
                    }
                }
                ended = true;
                break;
            default:
                fields.reject(
                    ObjectDiagnosticCode::InvalidRecordType, 0, "Tipo de registro desconhecido.");
                break;
        }
        if (fields.error)
            return ObjectFailure{{std::move(*fields.error)}};
        cursor = end;
        if (cursor < records.size()) {
            const auto terminator = records[cursor++];
            if (terminator == '\r' && cursor < records.size() && records[cursor] == '\n')
                ++cursor;
        }
        ++record;
    }
    if (!header || !ended)
        return failure(ObjectDiagnosticCode::MissingRecord,
                       {std::string{fileName}, record, 1},
                       "Objeto exige H e E.");
    if (auto diagnostic = validate(module))
        return ObjectFailure{{std::move(*diagnostic)}};
    return module;
}

ObjectRenderResult renderObject(const ObjectModule& module) {
    if (auto diagnostic = validate(module))
        return ObjectFailure{{std::move(*diagnostic)}};
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << 'H';
    name(output, module.header.name);
    hex(output, 0, 6);
    hex(output, module.header.length, 6);
    output << '\n';
    for (std::size_t begin = 0; begin < module.definitions.size(); begin += 6) {
        output << 'D';
        for (auto index = begin; index < std::min(begin + 6, module.definitions.size()); ++index) {
            name(output, module.definitions[index].name);
            hex(output, module.definitions[index].offset, 6);
        }
        output << '\n';
    }
    for (std::size_t begin = 0; begin < module.references.size(); begin += 12) {
        output << 'R';
        for (auto index = begin; index < std::min(begin + 12, module.references.size()); ++index)
            name(output, module.references[index].name);
        output << '\n';
    }
    for (const auto& text : module.text) {
        output << 'T';
        hex(output, text.offset, 6);
        hex(output, static_cast<std::uint32_t>(text.bytes.size()), 2);
        for (const auto byte : text.bytes)
            hex(output, byte, 2);
        output << '\n';
    }
    for (const auto& modification : module.modifications) {
        output << 'M';
        hex(output, modification.offset, 6);
        hex(output, modification.halfBytes, 2);
        output << (modification.sign == ModificationSign::Add ? '+' : '-');
        name(output, modification.symbol);
        output << '\n';
    }
    output << 'E';
    if (module.entry.offset)
        hex(output, *module.entry.offset, 6);
    output << '\n';
    return output.str();
}

} // namespace linker
