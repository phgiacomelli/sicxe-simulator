#include "DataDirective.hpp"

#include "AddressLimits.hpp"

#include <string_view>
#include <utility>

namespace assembler {
namespace {

DataDiagnostic fault(DataDiagnosticCode code, SourcePosition position, std::string message) {
    return {code, position, std::move(message), std::nullopt};
}

DataDiagnostic operandFault(const OperandDiagnostic& diagnostic) {
    return {
        DataDiagnosticCode::InvalidOperand, diagnostic.position, diagnostic.message, diagnostic};
}

int hexDigit(char character) {
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
    if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;
    return -1;
}

struct DataPlan {
    LocatedDirectiveKind kind;
    DataMeasurement measurement;
    std::string_view literal;
    bool hexadecimal{};
    std::optional<OperandValue> word;
};

using PlanResult = std::variant<DataPlan, DataDiagnostic>;

PlanResult planData(const SourceLine& line) {
    if (line.kind != SourceLineKind::Statement || !line.statement) {
        return fault(DataDiagnosticCode::InvalidSourceLine,
                     {line.number, 1},
                     "Diretiva exige uma linha sintaticamente valida.");
    }
    const auto& statement = *line.statement;
    const auto& operation = statement.operation;
    LocatedDirectiveKind kind;
    if (operation.key == "BYTE")
        kind = LocatedDirectiveKind::Byte;
    else if (operation.key == "WORD")
        kind = LocatedDirectiveKind::Word;
    else if (operation.key == "RESB")
        kind = LocatedDirectiveKind::Resb;
    else if (operation.key == "RESW")
        kind = LocatedDirectiveKind::Resw;
    else
        return fault(DataDiagnosticCode::InvalidDirective,
                     operation.source.position,
                     "Operacao nao e uma diretiva de dados/reserva.");
    if (!statement.operand) {
        return fault(DataDiagnosticCode::MissingOperand,
                     operation.source.position,
                     "Diretiva de dados/reserva exige operando.");
    }
    const auto& field = *statement.operand;
    if (kind == LocatedDirectiveKind::Word) {
        const auto parsed = parseValue(field);
        if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsed))
            return operandFault(*diagnostic);
        const auto& value = std::get<OperandValue>(parsed);
        if (std::holds_alternative<IntegerOperand>(value)) {
            const auto validated = validateInteger(value, Word24BitRange);
            if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&validated))
                return operandFault(*diagnostic);
        }
        return DataPlan{kind, {3, false}, {}, false, value};
    }
    if (kind == LocatedDirectiveKind::Resb || kind == LocatedDirectiveKind::Resw) {
        const auto count = parseKnownInteger(field, NonNegativeIntegerRange);
        if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&count))
            return operandFault(*diagnostic);
        const auto quantity = static_cast<std::uint64_t>(std::get<std::int64_t>(count));
        const std::uint64_t unit = kind == LocatedDirectiveKind::Resw ? 3 : 1;
        // Check before multiplying, even for INT64_MAX.
        if (quantity > AddressSpaceBytes / unit) {
            return fault(DataDiagnosticCode::SizeOutOfRange,
                         field.position,
                         "Reserva excede o espaco enderecavel.");
        }
        return DataPlan{
            kind, {static_cast<std::size_t>(quantity * unit), true}, {}, false, std::nullopt};
    }
    const std::string_view text = field.text;
    const bool hexadecimal = !text.empty() && (text[0] == 'X' || text[0] == 'x');
    const bool characters = !text.empty() && (text[0] == 'C' || text[0] == 'c');
    if (text.size() < 3 || (!hexadecimal && !characters) || text[1] != '\'' ||
        text.back() != '\'') {
        return fault(
            DataDiagnosticCode::InvalidByteLiteral, field.position, "BYTE exige C'...' ou X'...'.");
    }
    const auto literal = text.substr(2, text.size() - 3);
    for (std::size_t index = 0; index < literal.size(); ++index) {
        const SourcePosition position{field.position.line, field.position.column + 2 + index};
        if (literal[index] == '\'')
            return fault(DataDiagnosticCode::InvalidByteLiteral,
                         position,
                         "Aspas internas nao sao suportadas.");
        if (hexadecimal && hexDigit(literal[index]) < 0)
            return fault(
                DataDiagnosticCode::InvalidHexDigit, position, "Digito hexadecimal invalido.");
        if (!hexadecimal && static_cast<unsigned char>(literal[index]) > 0x7F)
            return fault(
                DataDiagnosticCode::NonAsciiCharacter, position, "BYTE C aceita somente ASCII.");
    }
    if (hexadecimal && literal.size() % 2 != 0) {
        return fault(
            DataDiagnosticCode::OddHexLength, field.position, "BYTE X exige pares de digitos.");
    }
    const auto size = hexadecimal ? literal.size() / 2 : literal.size();
    if (size > AddressSpaceBytes)
        return fault(DataDiagnosticCode::SizeOutOfRange,
                     field.position,
                     "Dados excedem o espaco enderecavel.");
    return DataPlan{kind, {size, false}, literal, hexadecimal, std::nullopt};
}

} // namespace

AssemblyDiagnostic toAssemblyDiagnostic(const DataDiagnostic& diagnostic) {
    if (diagnostic.operandDiagnostic)
        return toAssemblyDiagnostic(*diagnostic.operandDiagnostic);
    auto code = AssemblyDiagnosticCode::UnexpectedField;
    if (diagnostic.code == DataDiagnosticCode::MissingOperand)
        code = AssemblyDiagnosticCode::EmptyOperand;
    if (diagnostic.code == DataDiagnosticCode::InvalidSourceLine ||
        diagnostic.code == DataDiagnosticCode::InvalidDirective)
        code = AssemblyDiagnosticCode::InvalidOperation;
    if (diagnostic.code == DataDiagnosticCode::SizeOutOfRange ||
        diagnostic.code == DataDiagnosticCode::NonAsciiCharacter)
        code = AssemblyDiagnosticCode::ValueOutOfRange;
    return {code, diagnostic.position, diagnostic.message};
}

DataMeasureResult measureData(const SourceLine& line, SymbolTable& symbols) {
    const auto planned = planData(line);
    if (const auto* diagnostic = std::get_if<DataDiagnostic>(&planned))
        return *diagnostic;
    const auto& plan = std::get<DataPlan>(planned);
    if (plan.word) {
        if (const auto diagnostic = referenceOperand(*plan.word, symbols, Word24BitRange))
            return operandFault(*diagnostic);
    }
    return plan.measurement;
}

DataEncodeResult encodeData(const LocatedLine& line, const SymbolTable& symbols) {
    const auto planned = planData(line.source);
    if (const auto* diagnostic = std::get_if<DataDiagnostic>(&planned))
        return *diagnostic;
    const auto& plan = std::get<DataPlan>(planned);
    const auto position = line.source.statement->operation.source.position;
    const auto category =
        plan.measurement.reservation ? LocatedLineCategory::Reservation : LocatedLineCategory::Data;
    if (line.sizeBytes != plan.measurement.sizeBytes || line.category != category ||
        line.directive != plan.kind || line.extended) {
        return fault(DataDiagnosticCode::LayoutMismatch,
                     position,
                     "Diretiva diverge do layout fixado na passagem 1.");
    }
    if (line.address > MaxAddressableByte || line.sizeBytes > AddressSpaceBytes - line.address) {
        return fault(DataDiagnosticCode::SizeOutOfRange,
                     position,
                     "Dados/reserva excedem o espaco enderecavel.");
    }
    if (plan.measurement.reservation)
        return EncodedData{DataReservation{line.sizeBytes}};
    DataBytes output;
    output.bytes.reserve(line.sizeBytes);
    if (plan.word) {
        const auto resolved = resolveOperand(*plan.word, symbols, Word24BitRange);
        if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&resolved))
            return operandFault(*diagnostic);
        const auto value = std::get<std::int64_t>(resolved);
        // SymbolTable can also be constructed by consumers outside PassOne.
        if (value < Word24BitRange.min || value > Word24BitRange.max)
            return fault(DataDiagnosticCode::SizeOutOfRange,
                         line.source.statement->operand->position,
                         "Valor de WORD excede 24 bits.");
        const auto word = static_cast<std::uint32_t>(value);
        output.bytes = {static_cast<std::uint8_t>(word >> 16),
                        static_cast<std::uint8_t>(word >> 8),
                        static_cast<std::uint8_t>(word)};
    } else if (plan.hexadecimal) {
        for (std::size_t index = 0; index < plan.literal.size(); index += 2)
            output.bytes.push_back(static_cast<std::uint8_t>(hexDigit(plan.literal[index]) * 16 +
                                                             hexDigit(plan.literal[index + 1])));
    } else {
        for (const auto character : plan.literal)
            output.bytes.push_back(static_cast<std::uint8_t>(character));
    }
    return EncodedData{std::move(output)};
}

} // namespace assembler
