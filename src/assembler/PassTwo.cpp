#include "PassTwo.hpp"

#include "AddressLimits.hpp"
#include "DataDirective.hpp"
#include "InstructionEncoder.hpp"
#include "OperandValue.hpp"
#include "RawImageWriter.hpp"

#include <utility>

namespace assembler {
namespace {

AssemblyDiagnostic layoutError(const LocatedLine& line) {
    return {AssemblyDiagnosticCode::LayoutMismatch,
            {line.source.number, 1},
            "Linha localizada diverge do layout da passagem 1."};
}

} // namespace

AssemblyResult PassTwo::run(const PassOneResult& passOne) {
    AssemblyFailure failed;
    for (const auto& diagnostic : passOne.diagnostics)
        failed.diagnostics.push_back(toAssemblyDiagnostic(diagnostic));
    if (!failed.diagnostics.empty())
        return failed;
    if (passOne.initialPc > MaxAddressableByte)
        return AssemblyFailure{{{AssemblyDiagnosticCode::ValueOutOfRange,
                                 {1, 1},
                                 "PC inicial fora do espaco enderecavel de 20 bits."}}};

    AssemblySuccess success;
    success.loadAddress = passOne.startAddress;
    success.initialPc = passOne.initialPc;
    success.symbols = passOne.symbolTable.symbols();
    BaseState base;

    for (const auto& line : passOne.lines) {
        AssemblyListingLine entry{
            line.source.number, line.source.text, line.listingAddress(), {}, 0};
        if (line.isComment()) {
            if (line.sizeBytes != 0 || line.source.statement ||
                (line.source.kind != SourceLineKind::Blank &&
                 line.source.kind != SourceLineKind::Comment))
                failed.diagnostics.push_back(layoutError(line));
        } else if (!line.source.statement || line.source.kind != SourceLineKind::Statement) {
            failed.diagnostics.push_back(layoutError(line));
        } else if (line.isInstruction()) {
            const auto result = encodeInstruction(line, passOne.symbolTable, base);
            if (const auto* diagnostic = std::get_if<InstructionEncodingDiagnostic>(&result))
                failed.diagnostics.push_back(toAssemblyDiagnostic(*diagnostic));
            else {
                entry.bytes = std::get<EncodedInstruction>(result).bytes;
                if (entry.bytes.size() != line.sizeBytes)
                    failed.diagnostics.push_back(layoutError(line));
            }
        } else if (line.category == LocatedLineCategory::Data ||
                   line.category == LocatedLineCategory::Reservation) {
            const auto result = encodeData(line, passOne.symbolTable);
            if (const auto* diagnostic = std::get_if<DataDiagnostic>(&result)) {
                failed.diagnostics.push_back(toAssemblyDiagnostic(*diagnostic));
            } else {
                const auto& data = std::get<EncodedData>(result);
                if (const auto* bytes = std::get_if<DataBytes>(&data))
                    entry.bytes = bytes->bytes;
                else
                    entry.reservedBytes = std::get<DataReservation>(data).sizeBytes;
                if (entry.bytes.size() + entry.reservedBytes != line.sizeBytes)
                    failed.diagnostics.push_back(layoutError(line));
            }
        } else if (line.isDirective() && line.directive && line.sizeBytes == 0) {
            switch (*line.directive) {
                case LocatedDirectiveKind::Start:
                case LocatedDirectiveKind::End:
                    break;
                case LocatedDirectiveKind::NoBase:
                    base.reset();
                    break;
                case LocatedDirectiveKind::Base: {
                    base.reset();
                    const auto& operand = line.source.statement->operand;
                    if (!operand) {
                        failed.diagnostics.push_back(layoutError(line));
                        break;
                    }
                    const auto parsed = parseValue(*operand);
                    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsed)) {
                        failed.diagnostics.push_back(toAssemblyDiagnostic(*diagnostic));
                        break;
                    }
                    const auto resolved = resolveOperand(
                        std::get<OperandValue>(parsed), passOne.symbolTable, Address20BitRange);
                    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&resolved))
                        failed.diagnostics.push_back(toAssemblyDiagnostic(*diagnostic));
                    else {
                        const auto address = std::get<std::int64_t>(resolved);
                        if (address < 0 || address > MaxAddressableByte)
                            failed.diagnostics.push_back(layoutError(line));
                        else
                            base = static_cast<std::uint32_t>(address);
                    }
                    break;
                }
                default:
                    failed.diagnostics.push_back(layoutError(line));
                    break;
            }
        } else {
            failed.diagnostics.push_back(layoutError(line));
        }
        success.listing.push_back(std::move(entry));
    }
    if (!failed.diagnostics.empty())
        return failed;
    auto image = RawImageWriter::build(success.loadAddress, passOne.programLength, success.listing);
    if (const auto* diagnostic = std::get_if<AssemblyDiagnostic>(&image))
        return AssemblyFailure{{*diagnostic}};
    success.bytes = std::get<std::vector<std::uint8_t>>(std::move(image));
    return success;
}

} // namespace assembler
