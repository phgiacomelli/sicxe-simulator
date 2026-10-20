#include "assembler/InstructionEncoder.hpp"

#include "EncodingTestSupport.hpp"
#include "assembler/Format2Encoder.hpp"
#include "assembler/InstructionOperand.hpp"

#include <array>
#include <functional>
#include <gtest/gtest.h>
#include <limits>

namespace assembler {
namespace {
using Code = InstructionEncodingCode;
using Bytes = std::vector<std::uint8_t>;
using encoding_test::located;

TEST(InstructionEncoderTest, EmitsManuallyExpectedBytesAcrossAllModes) {
    struct Case {
        const char* source;
        std::uint32_t address;
        BaseState base;
        std::uint32_t target;
        Bytes expected;
    };
    const Case cases[] = {
        {"LDA #0", 0, {}, 0, {0x01, 0, 0}},
        {"LDA #4095", 0, {}, 0, {0x01, 0x0F, 0xFF}},
        {"LDA 0x123", 0, {}, 0, {0x03, 0x01, 0x23}},
        {"LDA @0x123", 0, {}, 0, {0x02, 0x01, 0x23}},
        {"LDA 0x123,X", 0, {}, 0, {0x03, 0x81, 0x23}},
        {"LDA Target", 0x1000, {}, 0x1010, {0x03, 0x20, 0x0D}},
        {"LDA #Target", 0x1000, {}, 0x1010, {0x01, 0x20, 0x0D}},
        {"LDA @Target", 0x1000, {}, 0x1010, {0x02, 0x20, 0x0D}},
        {"LDA Target,X", 0x1000, {}, 0x1010, {0x03, 0xA0, 0x0D}},
        {"LDA Target", 0x1000, {}, 0x1000, {0x03, 0x2F, 0xFD}},
        {"LDA Target", 0x1000, {}, 0x1003, {0x03, 0x20, 0}},
        {"LDA Target", 0x1000, 0x3000, 0x3100, {0x03, 0x41, 0}},
        {"LDA #Target", 0x1000, 0x3000, 0x3100, {0x01, 0x41, 0}},
        {"LDA @Target", 0x1000, 0x3000, 0x3100, {0x02, 0x41, 0}},
        {"LDA Target,X", 0x1000, 0x3000, 0x3100, {0x03, 0xC1, 0}},
        {"+LDA 0xABCDE", 0, {}, 0, {0x03, 0x1A, 0xBC, 0xDE}},
        {"+LDA #0x12345", 0, {}, 0, {0x01, 0x11, 0x23, 0x45}},
        {"+LDA @0x12345", 0, {}, 0, {0x02, 0x11, 0x23, 0x45}},
        {"+LDA 0x12345,X", 0, {}, 0, {0x03, 0x91, 0x23, 0x45}},
        {"+LDA Target", 0x1000, 0x3000, 0xABCDE, {0x03, 0x1A, 0xBC, 0xDE}},
        {"+LDA #Target", 0x1000, 0x3000, 0xABCDE, {0x01, 0x1A, 0xBC, 0xDE}},
        {"+LDA @Target", 0x1000, 0x3000, 0xABCDE, {0x02, 0x1A, 0xBC, 0xDE}},
        {"+LDA Target,X", 0x1000, 0x3000, 0xABCDE, {0x03, 0x9A, 0xBC, 0xDE}},
        {"+LDA #0", 0, {}, 0, {0x01, 0x10, 0, 0}},
        {"+LDA #0xFFFFF", 0, {}, 0, {0x01, 0x1F, 0xFF, 0xFF}},
        {"RSUB", 0x1000, 0x3000, 0, {0x4F, 0, 0}},
    };
    for (const auto& input : cases) {
        SCOPED_TRACE(input.source);
        SymbolTable symbols;
        ASSERT_FALSE(symbols.define("TARGET", input.target, {2, 1}));
        const auto line = located(input.source, input.address);
        const auto original = line;
        const auto result = encodeInstruction(line, symbols, input.base);
        ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
        EXPECT_EQ(std::get<EncodedInstruction>(result).bytes, input.expected);
        EXPECT_EQ(line, original);
        EXPECT_TRUE(symbols.references().empty());
        EXPECT_EQ(std::get<EncodedInstruction>(result).bytes.size(), line.sizeBytes);
    }
}

TEST(InstructionEncoderTest, DelegatesEveryFormat2OpcodeWithExistingBytesAndValidation) {
    const std::pair<const char*, Bytes> cases[] = {
        {"ADDR A,X", {0x90, 0x01}},
        {"CLEAR SW", {0xB4, 0x90}},
        {"COMPR PC,SW", {0xA0, 0x89}},
        {"DIVR L,B", {0x9C, 0x23}},
        {"MULR S,T", {0x98, 0x45}},
        {"RMO X,A", {0xAC, 0x10}},
        {"SHIFTL T,16", {0xA4, 0x5F}},
        {"SHIFTR PC,1", {0xA8, 0x80}},
        {"SUBR B,S", {0x94, 0x34}},
        {"TIXR L", {0xB8, 0x20}},
    };
    SymbolTable symbols;
    for (const auto& [source, expected] : cases) {
        SCOPED_TRACE(source);
        const auto line = located(source, 0x100);
        const auto encoded = encodeInstruction(line, symbols, {});
        ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(encoded));
        EXPECT_EQ(std::get<EncodedInstruction>(encoded).bytes, expected);
        const auto& spec =
            std::get<std::reference_wrapper<const InstructionSpec>>(findInstruction(line.mnemonic))
                .get();
        const auto operand =
            std::get<InstructionOperand>(parseInstructionOperand(spec, line.source));
        const auto delegated = std::get<Format2Bytes>(encodeFormat2(spec, operand));
        EXPECT_EQ(std::get<EncodedInstruction>(encoded).bytes,
                  (Bytes{delegated.begin(), delegated.end()}));
    }
}

TEST(InstructionEncoderTest, PreservesPackage2DiagnosticsAndTheirPositions) {
    const std::pair<const char*, InstructionDiagnosticCode> cases[] = {
        {"HALT", InstructionDiagnosticCode::UnknownInstruction},
        {"ADDF 1", InstructionDiagnosticCode::UnknownInstruction},
        {"+RSUB", InstructionDiagnosticCode::InvalidExtendedFormat},
        {"+CLEAR A", InstructionDiagnosticCode::InvalidExtendedFormat},
        {"RSUB 1", InstructionDiagnosticCode::UnexpectedOperand},
        {"LDA", InstructionDiagnosticCode::MissingOperand},
        {"LDA #", InstructionDiagnosticCode::MissingValue},
        {"LDA #-1", InstructionDiagnosticCode::NegativeImmediate},
        {"LDA #-0", InstructionDiagnosticCode::NegativeImmediate},
        {"LDA #VALUE,X", InstructionDiagnosticCode::InvalidIndexing},
        {"LDA @VALUE,X", InstructionDiagnosticCode::InvalidIndexing},
        {"LDA VALUE,Y", InstructionDiagnosticCode::InvalidIndexing},
        {"LDA VALUE+1", InstructionDiagnosticCode::UnsupportedOperandSyntax},
        {"LDA =VALUE", InstructionDiagnosticCode::UnsupportedOperandSyntax},
        {"LDA 0xGG", InstructionDiagnosticCode::InvalidValue},
        {"CLEAR F", InstructionDiagnosticCode::InvalidRegister},
        {"RMO A", InstructionDiagnosticCode::WrongOperandCount},
        {"SHIFTL A,17", InstructionDiagnosticCode::InvalidShiftCount},
    };
    for (const auto& [source, expected] : cases) {
        SCOPED_TRACE(source);
        const auto result = encodeInstruction(located(source), SymbolTable{}, {});
        ASSERT_TRUE(std::holds_alternative<InstructionEncodingDiagnostic>(result));
        const auto& diagnostic = std::get<InstructionEncodingDiagnostic>(result);
        EXPECT_EQ(diagnostic.code, Code::InvalidInstruction);
        ASSERT_TRUE(std::holds_alternative<InstructionDiagnostic>(diagnostic.cause));
        const auto& cause = std::get<InstructionDiagnostic>(diagnostic.cause);
        EXPECT_EQ(cause.code, expected);
        EXPECT_EQ(diagnostic.position, cause.source.position);
        EXPECT_EQ(toAssemblyDiagnostic(diagnostic), toAssemblyDiagnostic(cause));
    }
    const auto negative = encodeInstruction(located("LDA #-1"), SymbolTable{}, {});
    EXPECT_EQ(std::get<InstructionEncodingDiagnostic>(negative).position, (SourcePosition{1, 7}));
}

TEST(InstructionEncoderTest, PreservesPackage3ErrorsWithoutTruncationOrPartialBytes) {
    const std::pair<const char*, OperandDiagnosticCode> cases[] = {
        {"LDA NEVER", OperandDiagnosticCode::UndefinedSymbol},
        {"+LDA #NEVER", OperandDiagnosticCode::UndefinedSymbol},
        {"LDA -1", OperandDiagnosticCode::NegativeValueNotAllowed},
        {"LDA -0", OperandDiagnosticCode::NegativeValueNotAllowed},
        {"LDA 4096", OperandDiagnosticCode::ValueOutOfRange},
        {"LDA #4096", OperandDiagnosticCode::ValueOutOfRange},
        {"+LDA 0x100000", OperandDiagnosticCode::ValueOutOfRange},
        {"+LDA #0x100000", OperandDiagnosticCode::ValueOutOfRange},
        {"LDA 9223372036854775808", OperandDiagnosticCode::IntegerOverflow},
        {"+LDA 0xFFFFFFFFFFFFFFFFFFFF", OperandDiagnosticCode::IntegerOverflow},
        {"LDA LDA", OperandDiagnosticCode::ReservedSymbolName},
    };
    for (const auto& [source, expected] : cases) {
        SCOPED_TRACE(source);
        const auto result = encodeInstruction(located(source, 0x1000), SymbolTable{}, 0);
        ASSERT_TRUE(std::holds_alternative<InstructionEncodingDiagnostic>(result));
        const auto& diagnostic = std::get<InstructionEncodingDiagnostic>(result);
        EXPECT_EQ(diagnostic.code, Code::InvalidOperand);
        ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(diagnostic.cause));
        const auto& cause = std::get<OperandDiagnostic>(diagnostic.cause);
        EXPECT_EQ(cause.code, expected);
        EXPECT_EQ(diagnostic.position, cause.position);
        EXPECT_EQ(toAssemblyDiagnostic(diagnostic), toAssemblyDiagnostic(cause));
    }
}

TEST(InstructionEncoderTest, ChecksPcSignedBoundariesAndAlwaysPrefersPcOverBase) {
    struct Case {
        std::uint32_t target;
        BaseState base;
        std::optional<Bytes> expected;
    };
    const Case cases[] = {
        {0x803, {}, Bytes{0x03, 0x28, 0}},     // -2048
        {0x1802, {}, Bytes{0x03, 0x27, 0xFF}}, // +2047
        {0x802, {}, {}},
        {0x1803, {}, {}}, // -2049 / +2048
        {0x802, 0x800, Bytes{0x03, 0x40, 2}},
        {0x1803, 0x1000, Bytes{0x03, 0x48, 3}},
        {0x1003, 0x1003, Bytes{0x03, 0x20, 0}},
        {0x1000, 0x1000, Bytes{0x03, 0x2F, 0xFD}},
    };
    for (const auto& input : cases) {
        SCOPED_TRACE(input.target);
        SymbolTable symbols;
        ASSERT_FALSE(symbols.define("TARGET", input.target, {2, 1}));
        const auto line = located("LDA TARGET", 0x1000);
        const auto result = encodeInstruction(line, symbols, input.base);
        if (input.expected) {
            ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
            EXPECT_EQ(std::get<EncodedInstruction>(result).bytes, *input.expected);
        } else {
            ASSERT_TRUE(std::holds_alternative<InstructionEncodingDiagnostic>(result));
            const auto& error = std::get<InstructionEncodingDiagnostic>(result);
            EXPECT_EQ(error.code, Code::DisplacementOutOfRange);
            EXPECT_EQ(error.position, (SourcePosition{1, 6}));
            EXPECT_EQ(toAssemblyDiagnostic(error).code, AssemblyDiagnosticCode::ValueOutOfRange);
        }
        EXPECT_EQ(line.sizeBytes, 3U);
        EXPECT_FALSE(line.extended);
    }
}

TEST(InstructionEncoderTest, DistinguishesDisabledBaseFromZeroAndChecksUnsignedDisplacements) {
    struct Case {
        std::uint32_t target;
        BaseState base;
        std::optional<Bytes> expected;
    };
    const Case cases[] = {
        {0, {}, {}},
        {0, 0, Bytes{0x03, 0x40, 0}},
        {4095, 0, Bytes{0x03, 0x4F, 0xFF}},
        {4096, 0, {}},
        {0x9000, 0x9000, Bytes{0x03, 0x40, 0}},
        {0x9FFF, 0x9000, Bytes{0x03, 0x4F, 0xFF}},
        {0xA000, 0x9000, {}},
        {0x8FFF, 0x9000, {}},
    };
    for (const auto& input : cases) {
        SCOPED_TRACE(input.target);
        SymbolTable symbols;
        ASSERT_FALSE(symbols.define("TARGET", input.target, {2, 1}));
        const auto result = encodeInstruction(located("LDA TARGET", 0x5000), symbols, input.base);
        if (input.expected) {
            ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
            EXPECT_EQ(std::get<EncodedInstruction>(result).bytes, *input.expected);
        } else {
            ASSERT_TRUE(std::holds_alternative<InstructionEncodingDiagnostic>(result));
            EXPECT_EQ(std::get<InstructionEncodingDiagnostic>(result).code,
                      Code::DisplacementOutOfRange);
        }
    }
    // Numeric values never switch to PC/base, even if those would reach them.
    const auto numeric = encodeInstruction(located("LDA 4096", 0x1000), SymbolTable{}, 4096);
    ASSERT_TRUE(std::holds_alternative<InstructionEncodingDiagnostic>(numeric));
    const auto direct = encodeInstruction(located("LDA 0", 0x5000), SymbolTable{}, 0);
    EXPECT_EQ(std::get<EncodedInstruction>(direct).bytes, (Bytes{0x03, 0, 0}));
}

TEST(InstructionEncoderTest, RejectsEveryInconsistentLayoutField) {
    const std::function<void(LocatedLine&)> mutations[] = {
        [](auto& line) { line.sizeBytes = 4; },
        [](auto& line) { line.sizeBytes = std::numeric_limits<std::size_t>::max(); },
        [](auto& line) { line.opcode.reset(); },
        [](auto& line) { line.opcode = 0xFF; },
        [](auto& line) { line.format.reset(); },
        [](auto& line) { line.format = LocatedInstructionFormat::Format4; },
        [](auto& line) { line.format = static_cast<LocatedInstructionFormat>(255); },
        [](auto& line) { line.mnemonic = "LDX"; },
        [](auto& line) { line.extended = true; },
        [](auto& line) { line.directive = LocatedDirectiveKind::Base; },
        [](auto& line) { line.source.statement->operation.source.text = "LDX"; },
        [](auto& line) { line.source.statement->operation.key = "lda"; },
    };
    for (const auto& mutate : mutations) {
        auto line = located("LDA #1");
        mutate(line);
        const auto result = encodeInstruction(line, SymbolTable{}, {});
        ASSERT_TRUE(std::holds_alternative<InstructionEncodingDiagnostic>(result));
        EXPECT_EQ(std::get<InstructionEncodingDiagnostic>(result).code, Code::LayoutMismatch);
    }
    for (auto category : {LocatedLineCategory::Comment,
                          LocatedLineCategory::Data,
                          LocatedLineCategory::Directive,
                          LocatedLineCategory::Reservation}) {
        auto line = located("LDA #1");
        line.category = category;
        EXPECT_EQ(
            std::get<InstructionEncodingDiagnostic>(encodeInstruction(line, SymbolTable{}, {}))
                .code,
            Code::InvalidInstruction);
    }
    auto line = located("LDA #1");
    line.source.statement.reset();
    EXPECT_EQ(
        std::get<InstructionEncodingDiagnostic>(encodeInstruction(line, SymbolTable{}, {})).code,
        Code::InvalidSourceLine);
    line = located("LDA #1");
    line.source.kind = SourceLineKind::Invalid;
    EXPECT_EQ(
        std::get<InstructionEncodingDiagnostic>(encodeInstruction(line, SymbolTable{}, {})).code,
        Code::InvalidSourceLine);
}

TEST(InstructionEncoderTest, BoundsSymbolsBaseAndInstructionPlacementWithoutWrapping) {
    SymbolTable symbols;
    ASSERT_FALSE(symbols.define("OUTSIDE", 0x100000, {2, 1}));
    for (const auto* source : {"LDA OUTSIDE", "+LDA #OUTSIDE"}) {
        const auto result = encodeInstruction(located(source), symbols, {});
        EXPECT_EQ(std::get<InstructionEncodingDiagnostic>(result).code, Code::AddressOutOfRange);
    }
    for (const auto base : {0x100000U, std::numeric_limits<std::uint32_t>::max()}) {
        EXPECT_EQ(std::get<InstructionEncodingDiagnostic>(
                      encodeInstruction(located("LDA #1"), symbols, base))
                      .code,
                  Code::InvalidBaseState);
    }
    for (const auto* source : {"LDA #1", "+LDA #1", "CLEAR A", "RSUB"}) {
        auto line = located(source);
        line.address = AddressSpaceBytes - static_cast<std::uint32_t>(line.sizeBytes);
        EXPECT_TRUE(
            std::holds_alternative<EncodedInstruction>(encodeInstruction(line, symbols, {})));
        ++line.address;
        EXPECT_EQ(
            std::get<InstructionEncodingDiagnostic>(encodeInstruction(line, symbols, {})).code,
            Code::AddressOutOfRange);
        line.address = std::numeric_limits<std::uint32_t>::max();
        EXPECT_EQ(
            std::get<InstructionEncodingDiagnostic>(encodeInstruction(line, symbols, {})).code,
            Code::AddressOutOfRange);
    }
    // The PC is 0x100000 here: a nearby backward target still encodes without PC wrap.
    ASSERT_FALSE(symbols.define("LAST", MaxAddressableByte, {3, 1}));
    const auto end = encodeInstruction(located("J LAST", AddressSpaceBytes - 3), symbols, {});
    EXPECT_EQ(std::get<EncodedInstruction>(end).bytes, (Bytes{0x3F, 0x2F, 0xFF}));
}

TEST(InstructionEncoderTest, RepeatedCallsAreDeterministicAndDoNotKeepBaseOrSymbols) {
    SymbolTable symbols;
    ASSERT_FALSE(symbols.define("TARGET", 0, {2, 1}));
    const auto line = located("LDA TARGET", 0x5000);
    for (int repeat = 0; repeat < 2; ++repeat) {
        EXPECT_EQ(std::get<EncodedInstruction>(encodeInstruction(line, symbols, 0)).bytes,
                  (Bytes{0x03, 0x40, 0}));
        EXPECT_EQ(
            std::get<InstructionEncodingDiagnostic>(encodeInstruction(line, symbols, {})).code,
            Code::DisplacementOutOfRange);
        const auto undefined = encodeInstruction(line, SymbolTable{}, 0);
        EXPECT_EQ(
            std::get<OperandDiagnostic>(std::get<InstructionEncodingDiagnostic>(undefined).cause)
                .code,
            OperandDiagnosticCode::UndefinedSymbol);
    }
    EXPECT_TRUE(symbols.references().empty());
}

} // namespace
} // namespace assembler
