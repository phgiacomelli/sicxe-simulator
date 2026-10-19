#include "InstructionOperand.hpp"

#include "OperandValue.hpp"

#include <array>
#include <utility>

namespace assembler {
namespace {
using Code = InstructionDiagnosticCode;

bool space(char c) {
    return c == ' ' || c == '\t';
}
bool letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool digit(char c) {
    return c >= '0' && c <= '9';
}

SourceField slice(const SourceField& field, std::size_t begin, std::size_t end) {
    while (begin < end && space(field.text[begin])) {
        ++begin;
    }
    while (end > begin && space(field.text[end - 1])) {
        --end;
    }
    return {field.text.substr(begin, end - begin),
            {field.position.line, field.position.column + begin}};
}
InstructionDiagnostic error(Code code, const SourceField& field, std::string message) {
    return {code, field, std::move(message)};
}
std::variant<RegisterToken, InstructionDiagnostic> parseRegister(const SourceField& field) {
    constexpr std::array Names{"A", "X", "L", "B", "S", "T", "PC", "SW"};
    constexpr std::array Registers{InstructionRegister::A,
                                   InstructionRegister::X,
                                   InstructionRegister::L,
                                   InstructionRegister::B,
                                   InstructionRegister::S,
                                   InstructionRegister::T,
                                   InstructionRegister::PC,
                                   InstructionRegister::SW};
    std::string key = field.text;
    for (auto& c : key) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    for (std::size_t i = 0; i < Names.size(); ++i) {
        if (key == Names[i]) {
            return RegisterToken{Registers[i], field};
        }
    }
    return error(Code::InvalidRegister, field, "Registrador invalido; F nao pertence ao recorte.");
}

std::variant<MemoryInstructionOperand, InstructionDiagnostic> memoryOperand(SourceField field) {
    auto mode = InstructionAddressing::Simple;
    if (field.text.starts_with('#') || field.text.starts_with('@')) {
        mode = field.text.front() == '#' ? InstructionAddressing::Immediate
                                         : InstructionAddressing::Indirect;
        field = slice(field, 1, field.text.size());
    }
    bool indexed = false;
    const auto comma = field.text.find(',');
    if (comma != std::string::npos) {
        const auto suffix = slice(field, comma + 1, field.text.size());
        if (mode != InstructionAddressing::Simple || (suffix.text != "X" && suffix.text != "x")) {
            return error(Code::InvalidIndexing,
                         slice(field, comma, field.text.size()),
                         "Indexacao exige modo simples e sufixo ,X.");
        }
        indexed = true;
        field = slice(field, 0, comma);
    }
    if (field.text.empty()) {
        return error(Code::MissingValue, field, "Valor do operando ausente.");
    }
    auto token = std::string_view{field.text};
    bool integer = digit(token.front()) || token.front() == '+' || token.front() == '-';
    std::size_t firstDigit = integer && (token.front() == '+' || token.front() == '-') ? 1 : 0;
    for (std::size_t i = 0; i < token.size(); ++i) {
        const auto c = token[i];
        if (c == '=' || c == '*' || c == '/' || c == '(' || c == ')' ||
            ((c == '+' || c == '-') &&
             (i != 0 || !integer || token.size() <= 1 || !digit(token[1])))) {
            return error(Code::UnsupportedOperandSyntax,
                         field,
                         "Literal, expressao ou atributo nao suportado.");
        }
    }
    if (integer) {
        bool hex = token.substr(firstDigit).starts_with("0x") ||
                   token.substr(firstDigit).starts_with("0X");
        if (hex) {
            firstDigit += 2;
        }
        bool valid = firstDigit < token.size();
        for (std::size_t i = firstDigit; i < token.size(); ++i) {
            const auto c = token[i];
            valid =
                valid && (digit(c) || (hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))));
        }
        if (!valid) {
            return error(Code::InvalidValue, field, "Token numerico invalido.");
        }
        if (mode == InstructionAddressing::Immediate && token.front() == '-') {
            return error(
                Code::NegativeImmediate, field, "Imediato numerico negativo nao e permitido.");
        }
    } else {
        bool valid = letter(token.front());
        for (char c : token) {
            valid = valid && (letter(c) || digit(c) || c == '_');
        }
        if (!valid) {
            return error(Code::InvalidValue, field, "Token de rotulo invalido.");
        }
    }
    return MemoryInstructionOperand{mode,
                                    indexed,
                                    integer ? InstructionValueKind::Integer
                                            : InstructionValueKind::Symbol,
                                    std::move(field)};
}
} // namespace

bool isInstructionRegister(InstructionRegister reg) {
    const auto value = static_cast<std::uint8_t>(reg);
    return value <= 5 || value == 8 || value == 9;
}

InstructionOperandResult parseInstructionOperand(const InstructionSpec& spec,
                                                 const SourceLine& parsedLine) {
    if (parsedLine.kind != SourceLineKind::Statement || !parsedLine.statement) {
        return error(Code::InvalidSourceLine,
                     {parsedLine.text, {parsedLine.number, 1}},
                     "Exige linha de instrucao sintaticamente valida.");
    }
    const auto& statement = *parsedLine.statement;
    const auto found = findInstruction(statement.operation);
    if (const auto* failure = std::get_if<InstructionDiagnostic>(&found)) {
        return *failure;
    }
    if (std::get<std::reference_wrapper<const InstructionSpec>>(found).get() != spec) {
        return error(Code::InstructionMismatch,
                     statement.operation.source,
                     "Descritor nao corresponde a operacao.");
    }
    const auto form = spec.formats == InstructionFormats::Format2
                          ? InstructionForm::Format2
                          : (statement.operation.key.starts_with('+') ? InstructionForm::Format4
                                                                      : InstructionForm::Format3);
    const auto success = [&](InstructionOperandData data) -> InstructionOperandResult {
        return InstructionOperand{form, statement.operation.source, std::move(data)};
    };
    if (spec.operands == OperandPattern::None) {
        if (statement.operand) {
            return error(Code::UnexpectedOperand, *statement.operand, "RSUB nao aceita operando.");
        }
        return success(NoInstructionOperand{});
    }
    if (!statement.operand) {
        auto end = statement.operation.source;
        end.position.column += end.text.size();
        end.text.clear();
        return error(Code::MissingOperand, end, "Operando obrigatorio ausente.");
    }
    const auto& field = *statement.operand;
    if (spec.operands == OperandPattern::Memory) {
        const auto memory = memoryOperand(field);
        if (const auto* failure = std::get_if<InstructionDiagnostic>(&memory)) {
            return *failure;
        }
        return success(std::get<MemoryInstructionOperand>(memory));
    }
    const auto comma = field.text.find(',');
    const bool single = spec.operands == OperandPattern::OneRegister;
    if ((single && comma != std::string::npos) || (!single && comma == std::string::npos)) {
        return error(Code::WrongOperandCount,
                     comma == std::string::npos ? field : slice(field, comma, field.text.size()),
                     "Numero de operandos incorreto.");
    }
    const auto first = slice(field, 0, single ? field.text.size() : comma);
    const auto second = single ? SourceField{} : slice(field, comma + 1, field.text.size());
    if (first.text.empty() ||
        (!single && (second.text.empty() || second.text.find(',') != std::string::npos))) {
        return error(Code::WrongOperandCount,
                     first.text.empty() ? first : second,
                     "Operandos vazios ou excedentes.");
    }
    const auto r1 = parseRegister(first);
    if (const auto* failure = std::get_if<InstructionDiagnostic>(&r1)) {
        return *failure;
    }
    const auto reg = std::get<RegisterToken>(r1);
    if (single) {
        return success(OneRegisterOperand{reg});
    }
    if (spec.operands == OperandPattern::RegisterAndCount) {
        const auto count = parseKnownInteger(second, {1, 16, false});
        if (std::holds_alternative<OperandDiagnostic>(count)) {
            return error(
                Code::InvalidShiftCount, second, "Contagem de shift deve ser inteiro de 1 a 16.");
        }
        return success(RegisterCountOperand{
            reg, static_cast<std::uint8_t>(std::get<std::int64_t>(count)), second});
    }
    const auto r2 = parseRegister(second);
    if (const auto* failure = std::get_if<InstructionDiagnostic>(&r2)) {
        return *failure;
    }
    return success(TwoRegisterOperand{reg, std::get<RegisterToken>(r2)});
}
} // namespace assembler
