#include "ObjectPassTwo.hpp"

#include "InstructionEncoder.hpp"
#include "linker/ObjectRecords.hpp"

#include <utility>

namespace assembler {
namespace {

linker::ObjectPosition objectPosition(SourcePosition position) {
    return {{}, position.line, position.column};
}

AssemblyFailure failure(AssemblyDiagnosticCode code, SourcePosition position, std::string message) {
    return {{{code, position, std::move(message)}}};
}

void appendText(linker::ObjectModule& module,
                std::uint32_t offset,
                const std::vector<std::uint8_t>& bytes,
                SourcePosition position) {
    for (auto byte : bytes) {
        if (module.text.empty() || module.text.back().bytes.size() == linker::MaxTextRecordBytes ||
            module.text.back().offset + module.text.back().bytes.size() != offset)
            module.text.push_back({offset, {}, objectPosition(position)});
        module.text.back().bytes.push_back(byte);
        ++offset;
    }
}

} // namespace

ObjectAssemblyResult ObjectPassTwo::run(const ObjectPassOneResult& passOne) {
    AssemblyFailure failed;
    for (const auto& diagnostic : passOne.diagnostics)
        failed.diagnostics.push_back(toAssemblyDiagnostic(diagnostic));
    if (!failed.diagnostics.empty())
        return failed;
    if (passOne.section.length != passOne.programLength ||
        passOne.section.startAddress != passOne.startAddress ||
        passOne.startAddress > MaxAddressableByte ||
        passOne.programLength > AddressSpaceBytes - passOne.startAddress)
        return failure(AssemblyDiagnosticCode::LayoutMismatch,
                       passOne.section.position,
                       "Secao diverge do layout da primeira passagem.");

    linker::ObjectModule module;
    module.header = {
        passOne.section.name, passOne.programLength, objectPosition(passOne.section.position)};
    for (const auto& symbol : passOne.exports)
        module.definitions.push_back(
            {symbol.name, symbol.offset, objectPosition(symbol.declaration)});
    for (const auto& symbol : passOne.externalReferences)
        module.references.push_back({symbol.name, objectPosition(symbol.declaration)});
    if (passOne.entry)
        module.entry = {passOne.entry->offset, objectPosition(passOne.entry->position)};

    BaseState base;
    std::uint32_t nextOffset = 0;
    for (const auto& line : passOne.lines) {
        if (line.located.isComment())
            continue;
        const SourcePosition position{line.source().number, 1};
        if (line.source().kind != SourceLineKind::Statement || !line.source().statement)
            return failure(AssemblyDiagnosticCode::LayoutMismatch,
                           position,
                           "Linha de fonte invalida na primeira passagem.");
        if (line.offset != nextOffset || line.address() != passOne.startAddress + line.offset ||
            line.offset > passOne.programLength ||
            line.sizeBytes() > passOne.programLength - line.offset)
            return failure(AssemblyDiagnosticCode::LayoutMismatch,
                           position,
                           "Linha diverge do layout da primeira passagem.");
        nextOffset += static_cast<std::uint32_t>(line.sizeBytes());
        std::vector<std::uint8_t> bytes;
        auto modify = [&](std::uint32_t offset,
                          std::uint8_t width,
                          bool subtract,
                          const std::string& symbol,
                          SourcePosition source) {
            module.modifications.push_back(
                {offset,
                 width,
                 subtract ? linker::ModificationSign::Subtract : linker::ModificationSign::Add,
                 symbol,
                 objectPosition(source)});
        };
        if (line.located.isInstruction()) {
            InstructionEncodingResult encoded;
            if (!line.memoryOperand) {
                // Format 2 and RSUB keep the existing validation and encoding paths.
                encoded = encodeInstruction(line.located, passOne.symbolTable, base);
            } else {
                const auto& memory = *line.memoryOperand;
                const bool extended = line.located.format == LocatedInstructionFormat::Format4;
                const auto specResult = findInstruction(line.source().statement->operation);
                const auto* spec =
                    std::get_if<std::reference_wrapper<const InstructionSpec>>(&specResult);
                if (!spec || spec->get().operands != OperandPattern::Memory ||
                    (extended && !spec->get().supportsExtended()) ||
                    line.located.opcode != spec->get().opcode ||
                    line.located.mnemonic != spec->get().mnemonic ||
                    line.located.extended != extended ||
                    (line.located.format != LocatedInstructionFormat::Format3 && !extended))
                    return failure(AssemblyDiagnosticCode::LayoutMismatch,
                                   position,
                                   "Instrucao diverge do catalogo e formato da primeira passagem.");
                std::int64_t target = 0;
                SourceField source;
                if (const auto* integer = std::get_if<IntegerOperand>(&memory.target)) {
                    target = integer->value;
                    source = {integer->text, integer->position};
                } else if (const auto* local = std::get_if<LocalSymbolReference>(&memory.target)) {
                    // PC/base relative uses assembly coordinates; format 4 stores a section offset.
                    target = extended ? local->offset : local->address;
                    source = {local->source, local->position};
                    if (extended)
                        modify(line.offset + 1, 5, false, module.header.name, local->position);
                } else {
                    const auto& external = std::get<ExternalSymbolReference>(memory.target);
                    if (!extended)
                        return failure(AssemblyDiagnosticCode::InvalidOperation,
                                       external.position,
                                       "Referencia externa exige formato 4.");
                    source = {external.source, external.position};
                    // Zero is the known addend, not an invented external address.
                    modify(line.offset + 1, 5, false, external.name, external.position);
                }
                const MemoryInstructionOperand resolved{memory.mode,
                                                        memory.indexed,
                                                        memory.targetKind() ==
                                                                ObjectValueKind::AbsoluteInteger
                                                            ? InstructionValueKind::Integer
                                                            : InstructionValueKind::Symbol,
                                                        source};
                encoded = encodeResolvedMemoryInstruction(line.located, resolved, target, base);
            }
            if (const auto* error = std::get_if<InstructionEncodingDiagnostic>(&encoded))
                return AssemblyFailure{{toAssemblyDiagnostic(*error)}};
            bytes = std::move(std::get<EncodedInstruction>(encoded).bytes);
        } else if (line.directive == ObjectDirectiveKind::Byte && line.byteData) {
            bytes = *line.byteData;
        } else if (line.directive == ObjectDirectiveKind::Word && line.wordExpression) {
            const auto& expression = *line.wordExpression;
            const auto firstModification = module.modifications.size();
            // Local differences cancel; external terms preserve their declared order and signs.
            const auto balance = expression.localRelocationBalance();
            for (std::int64_t count = 0; count < (balance < 0 ? -balance : balance); ++count)
                modify(line.offset, 6, balance < 0, module.header.name, position);
            for (const auto& term : expression.terms)
                if (const auto* external = std::get_if<ExternalSymbolReference>(&term.value))
                    modify(line.offset,
                           6,
                           term.sign == ObjectTermSign::Subtract,
                           external->name,
                           term.position);
            const auto addend = expression.evaluateInitialAddend();
            const bool hasPendingRelocation = module.modifications.size() != firstModification;
            if (!hasPendingRelocation &&
                (addend < Word24BitRange.min || addend > Word24BitRange.max))
                return failure(AssemblyDiagnosticCode::ValueOutOfRange,
                               position,
                               "Valor de WORD fora do intervalo de 24 bits.");
            // The linker adds each pending term modulo the 24-bit field width.
            const auto word = static_cast<std::uint32_t>(addend) & 0xFFFFFFU;
            bytes = {static_cast<std::uint8_t>(word >> 16),
                     static_cast<std::uint8_t>(word >> 8),
                     static_cast<std::uint8_t>(word)};
        } else if (line.directive == ObjectDirectiveKind::Base && line.baseOperand) {
            if (const auto* local = std::get_if<LocalSymbolReference>(&*line.baseOperand))
                base = local->address;
            else
                return failure(AssemblyDiagnosticCode::LayoutMismatch,
                               position,
                               "BASE exige referencia local classificada.");
        } else if (line.directive == ObjectDirectiveKind::NoBase) {
            base.reset();
        } else if (line.directive != ObjectDirectiveKind::Start &&
                   line.directive != ObjectDirectiveKind::End &&
                   line.directive != ObjectDirectiveKind::ExtDef &&
                   line.directive != ObjectDirectiveKind::ExtRef &&
                   line.directive != ObjectDirectiveKind::Resb &&
                   line.directive != ObjectDirectiveKind::Resw) {
            return failure(AssemblyDiagnosticCode::LayoutMismatch,
                           position,
                           "Linha sem dados classificados para emissao objeto.");
        }
        const bool reservation = line.directive == ObjectDirectiveKind::Resb ||
                                 line.directive == ObjectDirectiveKind::Resw;
        if (!reservation && bytes.size() != line.sizeBytes())
            return failure(AssemblyDiagnosticCode::LayoutMismatch,
                           position,
                           "Tamanho emitido diverge da primeira passagem.");
        appendText(module, line.offset, bytes, position);
    }
    if (nextOffset != passOne.programLength)
        return failure(AssemblyDiagnosticCode::LayoutMismatch,
                       passOne.section.position,
                       "Comprimento emitido diverge da primeira passagem.");
    // Validate the complete sparse model with the common object contract before publishing it.
    const auto rendered = linker::renderObject(module);
    if (const auto* errors = std::get_if<linker::ObjectFailure>(&rendered)) {
        for (const auto& error : errors->diagnostics)
            failed.diagnostics.push_back({AssemblyDiagnosticCode::LayoutMismatch,
                                          {error.position.record, error.position.column},
                                          error.message});
        return failed;
    }
    return module;
}

} // namespace assembler
