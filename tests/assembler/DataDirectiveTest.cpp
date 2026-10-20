#include "assembler/DataDirective.hpp"

#include "assembler/PassOne.hpp"
#include "assembler/SourceParser.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace assembler {
namespace {

SourceLine sourceLine(std::string_view source) {
    const auto parsed = SourceParser::parse(source);
    EXPECT_TRUE(parsed.succeeded());
    EXPECT_EQ(parsed.lines.size(), 1U);
    return parsed.lines.at(0);
}

LocatedLine locate(const SourceLine& source, DataMeasurement measured, std::uint32_t address = 0) {
    const auto& key = source.statement->operation.key;
    const auto directive = key == "BYTE"   ? LocatedDirectiveKind::Byte
                           : key == "WORD" ? LocatedDirectiveKind::Word
                           : key == "RESB" ? LocatedDirectiveKind::Resb
                                           : LocatedDirectiveKind::Resw;
    return {source,
            measured.reservation ? LocatedLineCategory::Reservation : LocatedLineCategory::Data,
            address,
            measured.sizeBytes,
            key,
            false,
            std::nullopt,
            std::nullopt,
            directive};
}

TEST(DataDirectiveTest, ByteCharactersAndHexPreserveAsciiCasePunctuationAndNull) {
    SymbolTable symbols;
    const std::pair<std::string, std::vector<std::uint8_t>> cases[] = {
        {" BYTE C'Ab ;,. X'", {'A', 'b', ' ', ';', ',', '.', ' ', 'X'}},
        {" byte c'EOF'", {'E', 'O', 'F'}},
        {" BYTE X'00aBcdFF'", {0, 0xAB, 0xCD, 0xFF}},
        {" BYTE x'0a0D'", {10, 13}},
        {" BYTE C''", {}},
        {" BYTE X''", {}},
        {std::string(" BYTE C'") + '\0' + '\x7f' + "'", {0, 0x7F}},
    };
    for (const auto& [source, expected] : cases) {
        SCOPED_TRACE(source);
        const auto line = sourceLine(source);
        const auto measured = measureData(line, symbols);
        ASSERT_TRUE(std::holds_alternative<DataMeasurement>(measured));
        const auto size = std::get<DataMeasurement>(measured);
        EXPECT_EQ(size.sizeBytes, expected.size());
        EXPECT_FALSE(size.reservation);
        const auto encoded = encodeData(locate(line, size), symbols);
        ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
        EXPECT_EQ(std::get<DataBytes>(std::get<EncodedData>(encoded)).bytes, expected);
    }
    EXPECT_TRUE(symbols.references().empty());
}

TEST(DataDirectiveTest, ByteRejectsMalformedLiteralsAndReportsByteColumns) {
    const std::pair<std::string, DataDiagnosticCode> cases[] = {
        {" BYTE 42", DataDiagnosticCode::InvalidByteLiteral},
        {" BYTE Q'AB'", DataDiagnosticCode::InvalidByteLiteral},
        {" BYTE C'a''b'", DataDiagnosticCode::InvalidByteLiteral},
        {" BYTE X'ABC'", DataDiagnosticCode::OddHexLength},
        {" BYTE X'0G'", DataDiagnosticCode::InvalidHexDigit},
        {" BYTE X'AA BB'", DataDiagnosticCode::InvalidHexDigit},
        {" BYTE C'\xC3\xA9'", DataDiagnosticCode::NonAsciiCharacter},
    };
    SymbolTable symbols;
    for (const auto& [source, code] : cases) {
        SCOPED_TRACE(source);
        const auto result = measureData(sourceLine(source), symbols);
        ASSERT_TRUE(std::holds_alternative<DataDiagnostic>(result));
        EXPECT_EQ(std::get<DataDiagnostic>(result).code, code);
        EXPECT_EQ(std::get<DataDiagnostic>(result).position.line, 1U);
    }
    auto invalid = measureData(sourceLine(" BYTE X'0G'"), symbols);
    EXPECT_EQ(std::get<DataDiagnostic>(invalid).position, (SourcePosition{1, 10}));
    invalid = measureData(sourceLine(" BYTE C'\x80'"), symbols);
    EXPECT_EQ(std::get<DataDiagnostic>(invalid).position, (SourcePosition{1, 9}));
}

TEST(DataDirectiveTest, WordEncodesFull24BitRangeInBigEndian) {
    SymbolTable symbols;
    const std::pair<std::string, std::vector<std::uint8_t>> cases[] = {
        {"0", {0, 0, 0}},
        {"+1", {0, 0, 1}},
        {"0x123456", {0x12, 0x34, 0x56}},
        {"-1", {0xFF, 0xFF, 0xFF}},
        {"-0x800000", {0x80, 0, 0}},
        {"8388607", {0x7F, 0xFF, 0xFF}},
        {"8388608", {0x80, 0, 0}},
        {"16777215", {0xFF, 0xFF, 0xFF}},
        {"-0", {0, 0, 0}},
        {"-8388608", {0x80, 0, 0}},
        {"0xFFFFFF", {0xFF, 0xFF, 0xFF}},
    };
    for (const auto& [value, expected] : cases) {
        SCOPED_TRACE(value);
        const auto line = sourceLine(" WORD " + value);
        const auto measured = measureData(line, symbols);
        ASSERT_TRUE(std::holds_alternative<DataMeasurement>(measured));
        EXPECT_EQ(std::get<DataMeasurement>(measured).sizeBytes, 3U);
        const auto encoded = encodeData(locate(line, {3, false}), symbols);
        ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
        EXPECT_EQ(std::get<DataBytes>(std::get<EncodedData>(encoded)).bytes, expected);
    }
}

TEST(DataDirectiveTest, ReusesNumericAndSymbolDiagnosticsWithoutPublishingBytes) {
    SymbolTable symbols;
    const std::pair<std::string, OperandDiagnosticCode> cases[] = {
        {" WORD -8388609", OperandDiagnosticCode::ValueOutOfRange},
        {" WORD 16777216", OperandDiagnosticCode::ValueOutOfRange},
        {" WORD 0x1000000", OperandDiagnosticCode::ValueOutOfRange},
        {" WORD 9223372036854775808", OperandDiagnosticCode::IntegerOverflow},
        {" WORD 0xGG", OperandDiagnosticCode::InvalidInteger},
        {" WORD VALUE+1", OperandDiagnosticCode::UnsupportedExpression},
        {" RESB -1", OperandDiagnosticCode::NegativeValueNotAllowed},
        {" RESW -0", OperandDiagnosticCode::NegativeValueNotAllowed},
        {" RESB FUTURE", OperandDiagnosticCode::SymbolNotAllowed},
        {" RESW VALUE+1", OperandDiagnosticCode::UnsupportedExpression},
        {" RESB 0x8000000000000000", OperandDiagnosticCode::IntegerOverflow},
    };
    ASSERT_FALSE(symbols.define("VALUE", 5, {2, 1}));
    for (const auto& [source, code] : cases) {
        SCOPED_TRACE(source);
        const auto line = sourceLine(source);
        const auto measured = measureData(line, symbols);
        ASSERT_TRUE(std::holds_alternative<DataDiagnostic>(measured));
        const auto& diagnostic = std::get<DataDiagnostic>(measured);
        ASSERT_TRUE(diagnostic.operandDiagnostic);
        EXPECT_EQ(diagnostic.operandDiagnostic->code, code);
        EXPECT_EQ(diagnostic.position, line.statement->operand->position);
        EXPECT_EQ(toAssemblyDiagnostic(diagnostic),
                  toAssemblyDiagnostic(*diagnostic.operandDiagnostic));
        EXPECT_TRUE(
            std::holds_alternative<DataDiagnostic>(encodeData(locate(line, {3, false}), symbols)));
    }
    EXPECT_TRUE(symbols.references().empty());
    // A previously defined label is still not an integer reservation count.
    EXPECT_TRUE(
        std::holds_alternative<DataDiagnostic>(measureData(sourceLine(" RESB VALUE"), symbols)));
}

TEST(DataDirectiveTest, WordReferencesRemainPendingUntilEncodingAndNeverBecomeZero) {
    SymbolTable symbols;
    const auto source = sourceLine(" WORD Future");
    const auto measured = measureData(source, symbols);
    ASSERT_TRUE(std::holds_alternative<DataMeasurement>(measured));
    ASSERT_EQ(symbols.references().size(), 1U);
    EXPECT_TRUE(symbols.hasPendingReferences());
    const auto located = locate(source, std::get<DataMeasurement>(measured));
    const auto missing = encodeData(located, symbols);
    ASSERT_TRUE(std::holds_alternative<DataDiagnostic>(missing));
    const auto& diagnostic = std::get<DataDiagnostic>(missing);
    ASSERT_TRUE(diagnostic.operandDiagnostic);
    EXPECT_EQ(diagnostic.operandDiagnostic->code, OperandDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(toAssemblyDiagnostic(diagnostic).code, AssemblyDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(diagnostic.position, (SourcePosition{1, 7}));
    ASSERT_FALSE(symbols.define("FUTURE", 0xABCDE, {2, 1}));
    const auto encoded = encodeData(located, symbols);
    ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
    EXPECT_EQ(std::get<DataBytes>(std::get<EncodedData>(encoded)).bytes,
              (std::vector<std::uint8_t>{0x0A, 0xBC, 0xDE}));
    EXPECT_EQ(symbols.references().size(), 1U);
}

TEST(DataDirectiveTest, ReservationsHaveSizeAndNoByteBufferIncludingZeroAndMaximum) {
    SymbolTable symbols;
    const std::pair<std::string, std::size_t> cases[] = {
        {" RESB 0", 0},
        {" RESW 0", 0},
        {" RESB 5", 5},
        {" RESW 0x10", 48},
        {" RESB 1048576", AddressSpaceBytes},
        {" RESW 349525", AddressSpaceBytes - 1},
    };
    for (const auto& [source, expected] : cases) {
        SCOPED_TRACE(source);
        const auto line = sourceLine(source);
        const auto result = measureData(line, symbols);
        ASSERT_TRUE(std::holds_alternative<DataMeasurement>(result));
        const auto measured = std::get<DataMeasurement>(result);
        EXPECT_EQ(measured.sizeBytes, expected);
        EXPECT_TRUE(measured.reservation);
        const auto encoded = encodeData(locate(line, measured), symbols);
        ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
        ASSERT_TRUE(std::holds_alternative<DataReservation>(std::get<EncodedData>(encoded)));
        EXPECT_EQ(std::get<DataReservation>(std::get<EncodedData>(encoded)).sizeBytes, expected);
    }
    for (const auto source : {" RESB 1048577",
                              " RESW 349526",
                              " RESW 9223372036854775807",
                              " RESB 9223372036854775807"}) {
        const auto result = measureData(sourceLine(source), symbols);
        ASSERT_TRUE(std::holds_alternative<DataDiagnostic>(result));
        EXPECT_EQ(std::get<DataDiagnostic>(result).code, DataDiagnosticCode::SizeOutOfRange);
    }
}

TEST(DataDirectiveTest, EncodingRejectsTamperedLayoutAndAddressOverflow) {
    SymbolTable symbols;
    auto line = locate(sourceLine(" WORD 1"), {3, false});
    line.sizeBytes = 4;
    EXPECT_EQ(std::get<DataDiagnostic>(encodeData(line, symbols)).code,
              DataDiagnosticCode::LayoutMismatch);
    line.sizeBytes = 3;
    line.category = LocatedLineCategory::Reservation;
    EXPECT_EQ(std::get<DataDiagnostic>(encodeData(line, symbols)).code,
              DataDiagnosticCode::LayoutMismatch);
    line.category = LocatedLineCategory::Data;
    line.directive = LocatedDirectiveKind::Byte;
    EXPECT_EQ(std::get<DataDiagnostic>(encodeData(line, symbols)).code,
              DataDiagnosticCode::LayoutMismatch);
    line.directive = LocatedDirectiveKind::Word;
    line.address = MaxAddressableByte - 1;
    EXPECT_EQ(std::get<DataDiagnostic>(encodeData(line, symbols)).code,
              DataDiagnosticCode::SizeOutOfRange);
    line.address = AddressSpaceBytes;
    EXPECT_EQ(std::get<DataDiagnostic>(encodeData(line, symbols)).code,
              DataDiagnosticCode::SizeOutOfRange);
    line.address = MaxAddressableByte - 2;
    EXPECT_TRUE(std::holds_alternative<EncodedData>(encodeData(line, symbols)));
}

TEST(DataDirectiveTest, RejectsMissingOperandsAndNonDataLines) {
    SymbolTable symbols;
    for (const auto source : {" BYTE", " WORD", " RESB", " RESW"}) {
        EXPECT_EQ(std::get<DataDiagnostic>(measureData(sourceLine(source), symbols)).code,
                  DataDiagnosticCode::MissingOperand);
    }
    for (const auto source : {" LDA #1", " +BYTE C'A'", " START 0"}) {
        EXPECT_EQ(std::get<DataDiagnostic>(measureData(sourceLine(source), symbols)).code,
                  DataDiagnosticCode::InvalidDirective);
    }
    for (const auto source : {"; comment", "  ", " BYTE C'open"}) {
        const auto parsed = SourceParser::parse(source);
        ASSERT_EQ(parsed.lines.size(), 1U);
        EXPECT_EQ(std::get<DataDiagnostic>(measureData(parsed.lines.front(), symbols)).code,
                  DataDiagnosticCode::InvalidSourceLine);
    }
}

TEST(DataDirectiveTest, PassOneUsesOneLayoutForInstructionsDataAndReservations) {
    const auto parsed = SourceParser::parse("PROG START 0x100\n"
                                            "PTR WORD later\n"
                                            "BACK WORD PROG\n"
                                            "TEXT BYTE C'Ab ;'\n"
                                            "HEX BYTE X'0011aF'\n"
                                            "GAP RESB 5\n"
                                            "WORDS RESW 2\n"
                                            "EMPTY BYTE C''\n"
                                            "ZERO RESW 0\n"
                                            "LATER CLEAR A\n"
                                            " +LDA PTR\n"
                                            " RSUB\n"
                                            " END LATER\n");
    ASSERT_TRUE(parsed.succeeded());
    const auto pass = PassOne::run(parsed);
    ASSERT_TRUE(pass.succeeded());
    EXPECT_EQ(pass.programLength, 33U);
    EXPECT_EQ(pass.initialPc, 0x118U);
    EXPECT_EQ(pass.findSymbol("GAP")->address, 0x10DU);
    EXPECT_EQ(pass.findSymbol("WORDS")->address, 0x112U);
    EXPECT_EQ(pass.findSymbol("EMPTY")->address, 0x118U);
    EXPECT_EQ(pass.findSymbol("ZERO")->address, 0x118U);
    EXPECT_EQ(pass.findSymbol("LATER")->address, 0x118U);
    std::size_t dataSize = 0;
    for (const auto& line : pass.lines) {
        if (line.category != LocatedLineCategory::Data &&
            line.category != LocatedLineCategory::Reservation)
            continue;
        const auto encoded = encodeData(line, pass.symbolTable);
        ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
        const auto& data = std::get<EncodedData>(encoded);
        const auto size = std::holds_alternative<DataBytes>(data)
                              ? std::get<DataBytes>(data).bytes.size()
                              : std::get<DataReservation>(data).sizeBytes;
        EXPECT_EQ(size, line.sizeBytes);
        dataSize += size;
        EXPECT_FALSE(line.opcode);
        EXPECT_FALSE(line.format);
    }
    EXPECT_EQ(dataSize, 24U);
    EXPECT_EQ(
        std::get<DataBytes>(std::get<EncodedData>(encodeData(pass.lines[1], pass.symbolTable)))
            .bytes,
        (std::vector<std::uint8_t>{0, 1, 0x18}));
}

TEST(DataDirectiveTest, InvalidDataStopsLayoutWithoutDefiningLaterLabels) {
    for (const auto directive :
         {"BYTE X'G0'", "WORD 16777216", "RESB -1", "RESW 349526", "RESB FUTURE"}) {
        const auto parsed =
            SourceParser::parse(std::string("BAD ") + directive + "\nAFTER RSUB\n END\n");
        const auto pass = PassOne::run(parsed);
        EXPECT_FALSE(pass.succeeded());
        EXPECT_FALSE(pass.findSymbol("BAD"));
        EXPECT_FALSE(pass.findSymbol("AFTER"));
        EXPECT_EQ(pass.programLength, 0U);
        ASSERT_EQ(pass.diagnostics.size(), 1U);
        EXPECT_EQ(pass.diagnostics.front().code, PassOneDiagnosticCode::InvalidDirectiveOperand);
    }
    const auto missing = PassOne::run(SourceParser::parse("PTR WORD NEVER\n END\n"));
    ASSERT_FALSE(missing.succeeded());
    ASSERT_EQ(missing.diagnostics.size(), 1U);
    EXPECT_EQ(missing.diagnostics.front().code, PassOneDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(missing.diagnostics.front().position, (SourcePosition{1, 10}));
}

TEST(DataDirectiveTest, PassOneChecksFinalByteAndWholeReservationBeforeAdvancing) {
    for (const auto source : {" START 0xFFFFF\nLAST BYTE X'FF'\n END\n",
                              " START 0xFFFFD\nLAST WORD -1\n END\n",
                              "ALL RESB 1048576\n END\n",
                              " START 0xFFFFD\nLAST RESW 1\n END\n"}) {
        const auto pass = PassOne::run(SourceParser::parse(source));
        ASSERT_TRUE(pass.succeeded());
        for (const auto& line : pass.lines) {
            if (line.category == LocatedLineCategory::Data ||
                line.category == LocatedLineCategory::Reservation)
                EXPECT_TRUE(
                    std::holds_alternative<EncodedData>(encodeData(line, pass.symbolTable)));
        }
    }
    for (const auto source : {" START 0xFFFFF\nBAD BYTE C'AB'\nAFTER RSUB\n END\n",
                              " START 0xFFFFE\nBAD WORD 0\nAFTER RSUB\n END\n",
                              " START 1\nBAD RESB 1048576\nAFTER RSUB\n END\n",
                              " START 0xFFFFE\nBAD RESW 1\nAFTER RSUB\n END\n"}) {
        const auto pass = PassOne::run(SourceParser::parse(source));
        ASSERT_FALSE(pass.succeeded());
        EXPECT_FALSE(pass.findSymbol("BAD"));
        EXPECT_FALSE(pass.findSymbol("AFTER"));
        EXPECT_EQ(pass.programLength, 0U);
        ASSERT_EQ(pass.diagnostics.size(), 1U);
        EXPECT_EQ(pass.diagnostics.front().code, PassOneDiagnosticCode::AddressOutOfRange);
    }
}

TEST(DataDirectiveTest, ByteLengthIsBoundedBeforeEncoding) {
    SymbolTable symbols;
    for (const bool hexadecimal : {false, true}) {
        auto line = sourceLine(hexadecimal ? " BYTE X''" : " BYTE C''");
        auto& field = *line.statement->operand;
        const std::size_t unit = hexadecimal ? 2 : 1;
        field.text = std::string(hexadecimal ? "X'" : "C'") +
                     std::string(AddressSpaceBytes * unit, hexadecimal ? 'F' : 'A') + "'";
        const auto measured = measureData(line, symbols);
        ASSERT_TRUE(std::holds_alternative<DataMeasurement>(measured));
        EXPECT_EQ(std::get<DataMeasurement>(measured).sizeBytes, AddressSpaceBytes);
        const auto encoded = encodeData(locate(line, std::get<DataMeasurement>(measured)), symbols);
        ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
        EXPECT_EQ(std::get<DataBytes>(std::get<EncodedData>(encoded)).bytes.size(),
                  AddressSpaceBytes);
        field.text.insert(field.text.size() - 1, unit, 'A');
        const auto oversized = measureData(line, symbols);
        ASSERT_TRUE(std::holds_alternative<DataDiagnostic>(oversized));
        EXPECT_EQ(std::get<DataDiagnostic>(oversized).code, DataDiagnosticCode::SizeOutOfRange);
    }
}

TEST(DataDirectiveTest, SymbolicWordNeverTruncatesAndSelfReferenceDoesNotLeakBetweenRuns) {
    SymbolTable symbols;
    ASSERT_FALSE(symbols.define("TOOBIG", 0x1000000, {2, 1}));
    const auto oversized = encodeData(locate(sourceLine(" WORD TOOBIG"), {3, false}), symbols);
    ASSERT_TRUE(std::holds_alternative<DataDiagnostic>(oversized));
    EXPECT_EQ(std::get<DataDiagnostic>(oversized).code, DataDiagnosticCode::SizeOutOfRange);

    const auto first = PassOne::run(SourceParser::parse(" START 0x123\nSELF WORD self\n END\n"));
    ASSERT_TRUE(first.succeeded());
    const auto encoded = encodeData(first.lines[1], first.symbolTable);
    ASSERT_TRUE(std::holds_alternative<EncodedData>(encoded));
    EXPECT_EQ(std::get<DataBytes>(std::get<EncodedData>(encoded)).bytes,
              (std::vector<std::uint8_t>{0, 1, 0x23}));
    const auto second = PassOne::run(SourceParser::parse("PTR WORD SELF\n END\n"));
    EXPECT_FALSE(second.succeeded());
    EXPECT_FALSE(second.findSymbol("SELF"));
}

TEST(DataDirectiveTest, ZeroSizeLinesCannotCreateLabelsPastEndOfAddressSpace) {
    for (const auto directive : {"BYTE C''", "BYTE X''", "RESB 0", "RESW 0"}) {
        const auto pass = PassOne::run(
            SourceParser::parse(std::string("ALL RESB 1048576\nPAST ") + directive + "\n END\n"));
        ASSERT_FALSE(pass.succeeded());
        ASSERT_EQ(pass.diagnostics.size(), 1U);
        EXPECT_EQ(pass.diagnostics.front().code, PassOneDiagnosticCode::AddressOutOfRange);
        EXPECT_FALSE(pass.findSymbol("PAST"));
        EXPECT_EQ(pass.programLength, AddressSpaceBytes);
    }
}

} // namespace
} // namespace assembler
