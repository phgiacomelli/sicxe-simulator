#include "EncodingTestSupport.hpp"
#include "InstructionTestSupport.hpp"
#include "assembler/DataDirective.hpp"
#include "assembler/InstructionEncoder.hpp"
#include "assembler/PassOne.hpp"
#include "core/AddressResolver.hpp"
#include "core/Decoder.hpp"
#include "core/Machine.hpp"

#include <algorithm>
#include <gtest/gtest.h>

namespace assembler {
namespace {
using Bytes = std::vector<std::uint8_t>;
using encoding_test::located;

TEST(InstructionEncoderIntegrationTest, All38CatalogOpcodesRoundTripThroughPublicEncoder) {
    for (const auto& expected : test::ExpectedIsa) {
        SCOPED_TRACE(expected.name);
        std::string suffix;
        switch (expected.operands) {
            case OperandPattern::None:
                break;
            case OperandPattern::OneRegister:
                suffix = " T";
                break;
            case OperandPattern::TwoRegisters:
                suffix = " A,X";
                break;
            case OperandPattern::RegisterAndCount:
                suffix = " T,16";
                break;
            case OperandPattern::Memory:
                suffix = " #0x123";
                break;
        }
        const auto line = located(std::string(expected.name) + suffix);
        const auto result = encodeInstruction(line, SymbolTable{}, {});
        ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
        const auto& bytes = std::get<EncodedInstruction>(result).bytes;
        const bool format2 = expected.formats == InstructionFormats::Format2;
        EXPECT_EQ(bytes[0],
                  format2 ? expected.opcode : (expected.opcode | (suffix.empty() ? 3 : 1)));
        Memory memory(16);
        for (std::size_t i = 0; i < bytes.size(); ++i)
            memory.writeByte(i, bytes[i]);
        const auto decoded = Decoder{}.decode(memory, 0);
        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
        const auto& instruction = std::get<DecodedInstruction>(decoded);
        EXPECT_EQ(instruction.opcode, expected.vmOpcode);
        EXPECT_EQ(instruction.length, format2 ? 2 : 3);
        EXPECT_EQ(instruction.format,
                  format2 ? InstructionFormat::Format2 : InstructionFormat::Format3);
    }
}

TEST(InstructionEncoderIntegrationTest, All27MemoryOpcodesResolveEveryModeAndAddressingStrategy) {
    struct Strategy {
        bool extended;
        bool symbolic;
        std::uint32_t target;
        BaseState base;
        std::uint8_t flags;
        std::uint32_t field;
    };
    const Strategy strategies[] = {
        {false, false, 0x234, {}, 0, 0x234},
        {false, true, 0x1100, {}, 0x20, 0xFD},
        {false, true, 0x5123, 0x5000, 0x40, 0x123},
        {true, true, 0xABCDE, {}, 0x10, 0xABCDE},
        {true, false, 0xABCDE, {}, 0x10, 0xABCDE},
    };
    const std::pair<const char*, std::uint8_t> modes[] = {
        {"", 3}, {"#", 1}, {"@", 2}, {"indexed", 3}};
    for (const auto& expected : test::ExpectedIsa) {
        if (expected.operands != OperandPattern::Memory)
            continue;
        for (const auto& strategy : strategies) {
            for (const auto& [mode, ni] : modes) {
                const bool indexed = std::string_view(mode) == "indexed";
                const std::string token =
                    strategy.symbolic ? "Target" : std::to_string(strategy.target);
                const std::string source = std::string(strategy.extended ? "+" : "") +
                                           std::string(expected.name) + " " +
                                           (indexed ? "" : mode) + token + (indexed ? ",X" : "");
                SCOPED_TRACE(source);
                SymbolTable symbols;
                ASSERT_FALSE(symbols.define("TARGET", strategy.target, {2, 1}));
                const auto result =
                    encodeInstruction(located(source, 0x1000), symbols, strategy.base);
                ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
                const auto& bytes = std::get<EncodedInstruction>(result).bytes;
                EXPECT_EQ(bytes.front(), expected.opcode | ni);
                EXPECT_EQ(bytes[1] & 0xF0, strategy.flags | (indexed ? 0x80 : 0));
                Memory memory(AddressSpaceBytes);
                for (std::size_t i = 0; i < bytes.size(); ++i)
                    memory.writeByte(0x1000 + i, bytes[i]);
                if (ni == 2)
                    memory.writeWord(strategy.target, 0x80000);
                const auto decoded = Decoder{}.decode(memory, 0x1000);
                ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
                const auto& instruction = std::get<DecodedInstruction>(decoded);
                EXPECT_EQ(instruction.opcode, expected.vmOpcode);
                EXPECT_EQ(instruction.encodedOperand, strategy.field);
                EXPECT_EQ(instruction.format,
                          strategy.extended ? InstructionFormat::Format4
                                            : InstructionFormat::Format3);
                EXPECT_EQ(instruction.n, (ni & 2) != 0);
                EXPECT_EQ(instruction.i, (ni & 1) != 0);
                EXPECT_EQ(instruction.x, indexed);
                RegisterFile registers;
                registers.setB(strategy.base.value_or(0));
                registers.setX(2);
                const auto resolved = AddressResolver{}.resolve(
                    instruction, 0x1000 + instruction.length, memory, registers);
                ASSERT_TRUE(std::holds_alternative<ResolvedOperand>(resolved));
                const auto& operand = std::get<ResolvedOperand>(resolved);
                if (ni == 1) {
                    ASSERT_TRUE(std::holds_alternative<ImmediateValue>(operand));
                    EXPECT_EQ(std::get<ImmediateValue>(operand).value, strategy.target);
                } else {
                    ASSERT_TRUE(std::holds_alternative<MemoryAddress>(operand));
                    EXPECT_EQ(std::get<MemoryAddress>(operand).address,
                              ni == 2 ? 0x80000U : strategy.target + (indexed ? 2 : 0));
                }
            }
        }
    }
}

TEST(InstructionEncoderIntegrationTest,
     PcNegativeAndBothDisplacementBoundariesResolveToOriginalTarget) {
    for (const auto target : {0x803U, 0x1000U, 0x1003U, 0x1802U}) {
        SymbolTable symbols;
        ASSERT_FALSE(symbols.define("TARGET", target, {2, 1}));
        const auto result = encodeInstruction(located("LDA #TARGET", 0x1000), symbols, {});
        ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
        Memory memory(AddressSpaceBytes);
        const auto& bytes = std::get<EncodedInstruction>(result).bytes;
        for (std::size_t i = 0; i < bytes.size(); ++i)
            memory.writeByte(0x1000 + i, bytes[i]);
        const auto decoded = Decoder{}.decode(memory, 0x1000);
        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
        RegisterFile registers;
        const auto resolved = AddressResolver{}.resolve(
            std::get<DecodedInstruction>(decoded), 0x1003, memory, registers);
        ASSERT_TRUE(std::holds_alternative<ResolvedOperand>(resolved));
        EXPECT_EQ(std::get<ImmediateValue>(std::get<ResolvedOperand>(resolved)).value, target);
    }
}

TEST(InstructionEncoderIntegrationTest,
     PassOneDataAndReservationsKeepAddressesAndBaseIsProvidedPerCall) {
    const auto pass = PassOne::run(SourceParser::parse(" START 0x100\n"
                                                       "ENTRY +LDB #TABLE\n"
                                                       " BASE TABLE\n"
                                                       " LDA TABLE\n"
                                                       " NOBASE\n"
                                                       " +J DONE\n"
                                                       "GAP RESB 4096\n"
                                                       "TABLE WORD 42\n"
                                                       "DONE J DONE\n"
                                                       " END ENTRY\n"));
    ASSERT_TRUE(pass.succeeded());
    ASSERT_TRUE(pass.findSymbol("TABLE"));
    EXPECT_EQ(pass.findSymbol("TABLE")->address, 0x110BU);
    EXPECT_EQ(pass.findSymbol("DONE")->address, 0x110EU);
    const auto& tableLoad = pass.lines[3];
    EXPECT_EQ(tableLoad.address, 0x104U);
    const auto active = encodeInstruction(tableLoad, pass.symbolTable, 0x110B);
    ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(active));
    EXPECT_EQ(std::get<EncodedInstruction>(active).bytes, (Bytes{0x03, 0x40, 0}));
    const auto disabled = encodeInstruction(tableLoad, pass.symbolTable, {});
    EXPECT_EQ(std::get<InstructionEncodingDiagnostic>(disabled).code,
              InstructionEncodingCode::DisplacementOutOfRange);
    EXPECT_EQ(pass.lines[2].directive, LocatedDirectiveKind::Base);
    EXPECT_EQ(pass.lines[4].directive, LocatedDirectiveKind::NoBase);
    const auto gap = encodeData(pass.lines[6], pass.symbolTable);
    ASSERT_TRUE(std::holds_alternative<EncodedData>(gap));
    EXPECT_EQ(std::get<DataReservation>(std::get<EncodedData>(gap)).sizeBytes, 4096U);

    // In-memory test fixture only: no PassTwo, writer or output file is introduced.
    Bytes image(pass.programLength, 0);
    for (const auto& line : pass.lines) {
        Bytes bytes;
        if (line.isInstruction()) {
            const auto result = encodeInstruction(
                line, pass.symbolTable, line.address == 0x104 ? BaseState{0x110B} : BaseState{});
            ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
            bytes = std::get<EncodedInstruction>(result).bytes;
        } else if (line.category == LocatedLineCategory::Data) {
            const auto result = encodeData(line, pass.symbolTable);
            ASSERT_TRUE(std::holds_alternative<EncodedData>(result));
            bytes = std::get<DataBytes>(std::get<EncodedData>(result)).bytes;
        } else
            continue;
        ASSERT_EQ(bytes.size(), line.sizeBytes);
        ASSERT_LE(line.address - pass.startAddress + bytes.size(), image.size());
        std::copy(bytes.begin(), bytes.end(), image.begin() + line.address - pass.startAddress);
    }
    Machine machine;
    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(
        machine.loadProgram(image, pass.startAddress, pass.initialPc)));
    for (int step = 0; step < 3; ++step)
        ASSERT_TRUE(std::holds_alternative<StepSuccess>(machine.step()));
    const auto snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.registers.b, 0x110BU);
    EXPECT_EQ(snapshot.registers.a, 42U);
    EXPECT_EQ(snapshot.registers.pc, 0x110EU);
    EXPECT_FALSE(snapshot.lastFault);
}

TEST(InstructionEncoderIntegrationTest,
     EncodedProgramCallsReturnsAndStoresExpectedResultInMachine) {
    const auto pass = PassOne::run(SourceParser::parse("PROG START 0x100\n"
                                                       "ENTRY LDA #5\n"
                                                       " ADD #7\n"
                                                       " STA RESULT\n"
                                                       " +JSUB SUBR1\n"
                                                       " J DONE\n"
                                                       "RESULT WORD 0\n"
                                                       "SUBR1 LDA RESULT\n"
                                                       " ADD #1\n"
                                                       " RSUB\n"
                                                       "DONE J DONE\n"
                                                       " END ENTRY\n"));
    ASSERT_TRUE(pass.succeeded());
    const auto originalLines = pass.lines;
    const auto referenceCount = pass.symbolTable.references().size();
    Bytes image;
    for (const auto& line : pass.lines) {
        Bytes bytes;
        if (line.isInstruction()) {
            const auto result = encodeInstruction(line, pass.symbolTable, {});
            ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
            bytes = std::get<EncodedInstruction>(result).bytes;
        } else if (line.category == LocatedLineCategory::Data) {
            const auto result = encodeData(line, pass.symbolTable);
            ASSERT_TRUE(std::holds_alternative<EncodedData>(result));
            bytes = std::get<DataBytes>(std::get<EncodedData>(result)).bytes;
        } else
            continue;
        EXPECT_EQ(line.address, pass.startAddress + image.size());
        EXPECT_EQ(bytes.size(), line.sizeBytes);
        image.insert(image.end(), bytes.begin(), bytes.end());
    }
    // Expected image calculated independently from the encoder and catalog.
    const Bytes expected{0x01, 0x00, 0x05, 0x19, 0x00, 0x07, 0x0F, 0x20, 0x07, 0x4B, 0x10,
                         0x01, 0x13, 0x3F, 0x20, 0x0C, 0,    0,    0,    0x03, 0x2F, 0xFA,
                         0x19, 0,    1,    0x4F, 0,    0,    0x3F, 0x2F, 0xFD};
    EXPECT_EQ(image, expected);
    EXPECT_EQ(pass.lines, originalLines);
    EXPECT_EQ(pass.symbolTable.references().size(), referenceCount);
    Machine machine;
    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(
        machine.loadProgram(image, pass.startAddress, pass.initialPc)));
    for (int step = 0; step < 8; ++step)
        ASSERT_TRUE(std::holds_alternative<StepSuccess>(machine.step()));
    const auto snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.registers.a, 13U);
    EXPECT_EQ(snapshot.registers.l, 0x10DU);
    EXPECT_EQ(snapshot.registers.pc, 0x11CU);
    EXPECT_EQ(snapshot.memory[0x110], 0);
    EXPECT_EQ(snapshot.memory[0x111], 0);
    EXPECT_EQ(snapshot.memory[0x112], 12);
    EXPECT_FALSE(snapshot.lastFault);
}

TEST(InstructionEncoderIntegrationTest,
     LastByteLayoutRemainsEncodableButMachineKeepsPostFetchRestriction) {
    const auto pass = PassOne::run(SourceParser::parse(" START 0xFFFFD\nLAST RSUB\n END LAST\n"));
    ASSERT_TRUE(pass.succeeded());
    const auto result = encodeInstruction(pass.lines[1], pass.symbolTable, {});
    ASSERT_TRUE(std::holds_alternative<EncodedInstruction>(result));
    const auto& bytes = std::get<EncodedInstruction>(result).bytes;
    Machine machine;
    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(
        machine.loadProgram(bytes, pass.startAddress, pass.initialPc)));
    const auto decoded = machine.snapshot().currentInstruction;
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->opcode, Opcode::RSUB);
    const auto step = machine.step();
    ASSERT_TRUE(std::holds_alternative<MachineFault>(step));
    EXPECT_EQ(std::get<CycleFault>(std::get<MachineFault>(step).cause).code,
              CycleFaultCode::NextPcOutOfRange);
}

} // namespace
} // namespace assembler
