#include "core/Machine.hpp"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint8_t Immediate = 0x01;
constexpr std::uint8_t Indirect = 0x02;
constexpr std::uint8_t Simple = 0x03;
constexpr std::uint8_t PcRelative = 0x2;

constexpr std::uint32_t MemorySize = Machine::DefaultMemorySize;
constexpr std::uint32_t LastAddress = MemorySize - 1;

Bytes concat(std::initializer_list<Bytes> parts) {
    Bytes result;
    for (const auto& part : parts) {
        result.insert(result.end(), part.begin(), part.end());
    }
    return result;
}

Bytes format2(Opcode opcode, RegisterId r1, RegisterId r2 = RegisterId::A) {
    const auto high = static_cast<std::uint8_t>(r1);
    const auto low = static_cast<std::uint8_t>(r2);
    return {static_cast<std::uint8_t>(opcode), static_cast<std::uint8_t>((high << 4) | low)};
}

Bytes format2Nibble(Opcode opcode, RegisterId r1, std::uint8_t nibble) {
    const auto high = static_cast<std::uint8_t>(r1);
    return {static_cast<std::uint8_t>(opcode), static_cast<std::uint8_t>((high << 4) | nibble)};
}

Bytes format3(Opcode opcode, std::uint8_t mode, std::uint16_t field, std::uint8_t flags = 0) {
    return {static_cast<std::uint8_t>(static_cast<std::uint8_t>(opcode) | mode),
            static_cast<std::uint8_t>((flags << 4) | ((field >> 8) & 0x0F)),
            static_cast<std::uint8_t>(field & 0xFF)};
}

Bytes format4(Opcode opcode, std::uint8_t mode, std::uint32_t address) {
    return {static_cast<std::uint8_t>(static_cast<std::uint8_t>(opcode) | mode),
            static_cast<std::uint8_t>(0x10 | ((address >> 16) & 0x0F)),
            static_cast<std::uint8_t>((address >> 8) & 0xFF),
            static_cast<std::uint8_t>(address & 0xFF)};
}

Bytes sic(Opcode opcode, std::uint16_t address) {
    return {static_cast<std::uint8_t>(opcode),
            static_cast<std::uint8_t>((address >> 8) & 0x7F),
            static_cast<std::uint8_t>(address & 0xFF)};
}

Bytes word(std::uint32_t value) {
    return {static_cast<std::uint8_t>((value >> 16) & 0xFF),
            static_cast<std::uint8_t>((value >> 8) & 0xFF),
            static_cast<std::uint8_t>(value & 0xFF)};
}

class Program {
public:
    Program& at(std::size_t offset, const Bytes& code) {
        if (bytes_.size() < offset + code.size()) {
            bytes_.resize(offset + code.size(), 0x00);
        }
        std::copy(code.begin(), code.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
        return *this;
    }

    const Bytes& bytes() const {
        return bytes_;
    }

private:
    Bytes bytes_;
};

// Deixa A, X, L, B, S, T e o Condition Code com valores distintos de zero:
// A=0x11 X=0x22 L=0x33 B=0x44 S=0x55 T=0x66 CC=Less.
constexpr std::size_t PreambleSteps = 7;
constexpr std::uint32_t PreambleEnd = 0x15;

Program preambleProgram() {
    Program program;
    program.at(0x00,
               concat({format3(Opcode::LDA, Immediate, 0x11),
                       format3(Opcode::LDX, Immediate, 0x22),
                       format3(Opcode::LDL, Immediate, 0x33),
                       format3(Opcode::LDB, Immediate, 0x44),
                       format3(Opcode::LDS, Immediate, 0x55),
                       format3(Opcode::LDT, Immediate, 0x66),
                       format3(Opcode::COMP, Immediate, 0x99)}));
    return program;
}

template <typename Cause> Cause causeOf(const MachineFault& fault) {
    return std::get<Cause>(fault.cause);
}

bool isLoadFault(const LoadResult& result) {
    return std::holds_alternative<LoadFault>(result);
}

bool isAllZero(const Bytes& bytes) {
    for (const auto byte : bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

Bytes memoryOperand(Opcode opcode) {
    return format3(opcode, Simple, 0x80);
}

Bytes jumpOperand(Opcode opcode) {
    return format3(opcode, Simple, 0x60);
}

} // namespace

class MachineTest : public ::testing::Test {
protected:
    Machine machine;

    void load(const Bytes& bytes, std::uint32_t address, std::uint32_t initialPc) {
        const auto result = machine.loadProgram(bytes, address, initialPc);
        ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    }

    void load(const Bytes& bytes, std::uint32_t address) {
        load(bytes, address, address);
    }

    void load(const Bytes& bytes) {
        load(bytes, 0, 0);
    }

    StepSuccess stepOk() {
        const auto result = machine.step();
        EXPECT_TRUE(std::holds_alternative<StepSuccess>(result));
        return std::get<StepSuccess>(result);
    }

    void stepTimes(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            stepOk();
        }
    }

    RegisterSnapshot registers() const {
        return machine.snapshot().registers;
    }

    // Carrega o programa e executa o preâmbulo, deixando um estado arquitetural não trivial.
    void prepare(const Program& program) {
        load(program.bytes());
        stepTimes(PreambleSteps);
    }

    // Executa um passo que deve falhar e confirma que memória, registradores e PC não mudaram.
    MachineFault stepAtomicFault() {
        const MachineSnapshot before = machine.snapshot();
        const auto result = machine.step();
        EXPECT_TRUE(std::holds_alternative<MachineFault>(result));
        const MachineSnapshot after = machine.snapshot();

        EXPECT_EQ(after.registers, before.registers);
        EXPECT_EQ(after.registers.pc, before.registers.pc);
        EXPECT_TRUE(after.memory == before.memory) << "a memória mudou durante uma falha";
        EXPECT_TRUE(after.lastFault.has_value());
        return std::get<MachineFault>(result);
    }

    void expectJump(Opcode opcode, std::uint16_t a, std::uint16_t b, std::uint32_t expectedPc) {
        SCOPED_TRACE(testing::Message() << "A=" << a << " comparado com " << b);
        const Program program = conditionalProgram(opcode, a, b);
        load(program.bytes());
        stepTimes(2);
        const RegisterSnapshot before = registers();

        const StepSuccess success = stepOk();

        RegisterSnapshot expected = before;
        expected.pc = expectedPc;
        EXPECT_EQ(registers(), expected);
        EXPECT_EQ(success.nextPc, expectedPc);
        EXPECT_EQ(success.instruction.opcode, opcode);
    }

    static constexpr std::uint32_t JumpTarget = 0x20;
    static constexpr std::uint32_t NotTakenPc = 0x09;

    // LDA #a; COMP #b; <salto> JumpTarget
    static Program conditionalProgram(Opcode jump, std::uint16_t a, std::uint16_t b) {
        Program program;
        program.at(0x00, format3(Opcode::LDA, Immediate, a));
        program.at(0x03, format3(Opcode::COMP, Immediate, b));
        program.at(0x06, format3(jump, Simple, JumpTarget));
        return program;
    }
};

// ---------------------------------------------------------------------------------------------
// Estado inicial e snapshot
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, StartsWithOneMebibyteOfZeroedMemoryAndZeroedRegisters) {
    const MachineSnapshot snapshot = machine.snapshot();

    EXPECT_EQ(snapshot.memory.size(), 1048576U);
    EXPECT_TRUE(isAllZero(snapshot.memory));
    EXPECT_EQ(snapshot.registers.a, 0U);
    EXPECT_EQ(snapshot.registers.x, 0U);
    EXPECT_EQ(snapshot.registers.l, 0U);
    EXPECT_EQ(snapshot.registers.b, 0U);
    EXPECT_EQ(snapshot.registers.s, 0U);
    EXPECT_EQ(snapshot.registers.t, 0U);
    EXPECT_EQ(snapshot.registers.f, 0U);
    EXPECT_EQ(snapshot.registers.pc, 0U);
    EXPECT_EQ(snapshot.registers.sw, 0U);
    EXPECT_EQ(snapshot.registers.conditionCode, ConditionCode::Equal);
    EXPECT_FALSE(snapshot.lastFault.has_value());
}

TEST_F(MachineTest, SnapshotAfterSuccessShowsOnlyTheCommittedChanges) {
    load(format3(Opcode::LDA, Immediate, 5));
    const MachineSnapshot before = machine.snapshot();

    const StepSuccess success = stepOk();
    const MachineSnapshot after = machine.snapshot();

    RegisterSnapshot expected = before.registers;
    expected.a = 5;
    expected.pc = 3;
    EXPECT_EQ(after.registers, expected);
    EXPECT_TRUE(after.memory == before.memory);
    EXPECT_FALSE(after.lastFault.has_value());
    EXPECT_EQ(success.nextPc, 3U);
    ASSERT_TRUE(success.effect.registerWrite.has_value());
    EXPECT_EQ(success.effect.registerWrite->target, RegisterId::A);
    EXPECT_EQ(success.effect.registerWrite->value, 5U);
}

TEST_F(MachineTest, SnapshotIdentifiesInstructionAtCurrentPc) {
    load(concat({format3(Opcode::LDA, Immediate, 5), format3(Opcode::ADD, Immediate, 1)}));
    const auto before = machine.snapshot();
    ASSERT_TRUE(before.currentInstruction.has_value());
    EXPECT_EQ(before.currentInstruction->opcode, Opcode::LDA);
    EXPECT_EQ(before.currentInstruction->rawBytes[0], 0x01);
    EXPECT_EQ(before.currentInstruction->length, 3);
    stepOk();
    const auto after = machine.snapshot();
    ASSERT_TRUE(after.currentInstruction.has_value());
    EXPECT_EQ(after.currentInstruction->opcode, Opcode::ADD);
    EXPECT_EQ(after.currentInstruction->instructionAddress, 3U);
}

TEST_F(MachineTest, SnapshotIsAnImmutableCopyOfTheState) {
    load(format3(Opcode::LDA, Immediate, 5));
    const MachineSnapshot before = machine.snapshot();

    stepOk();

    EXPECT_EQ(before.registers.pc, 0U);
    EXPECT_EQ(before.registers.a, 0U);
    EXPECT_EQ(machine.snapshot().registers.pc, 3U);
}

TEST_F(MachineTest, SnapshotReportsConditionCodeProducedByComparison) {
    load(concat({format3(Opcode::LDA, Immediate, 5),
                 format3(Opcode::COMP, Immediate, 6),
                 format3(Opcode::COMP, Immediate, 5),
                 format3(Opcode::COMP, Immediate, 4)}));

    stepTimes(2);
    EXPECT_EQ(registers().conditionCode, ConditionCode::Less);
    stepOk();
    EXPECT_EQ(registers().conditionCode, ConditionCode::Equal);
    stepOk();
    EXPECT_EQ(registers().conditionCode, ConditionCode::Greater);
}

// ---------------------------------------------------------------------------------------------
// loadProgram
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, LoadCopiesBytesAndSetsInitialPc) {
    const Bytes program{0x01, 0x02, 0x03, 0x04};

    const auto result = machine.loadProgram(program, 0x100, 0x102);

    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    EXPECT_EQ(std::get<LoadSuccess>(result), (LoadSuccess{0x100, 4, 0x102}));
    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.registers.pc, 0x102U);
    EXPECT_EQ(snapshot.memory[0xFF], 0);
    EXPECT_EQ(snapshot.memory[0x100], 0x01);
    EXPECT_EQ(snapshot.memory[0x101], 0x02);
    EXPECT_EQ(snapshot.memory[0x102], 0x03);
    EXPECT_EQ(snapshot.memory[0x103], 0x04);
    EXPECT_EQ(snapshot.memory[0x104], 0);
    EXPECT_EQ(snapshot.registers.a, 0U);
    EXPECT_FALSE(snapshot.lastFault.has_value());
}

TEST_F(MachineTest, LoadsAtTheStartOfMemory) {
    const Bytes program{0xAA, 0xBB};

    const auto result = machine.loadProgram(program, 0, 0);

    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.memory[0], 0xAA);
    EXPECT_EQ(snapshot.memory[1], 0xBB);
    EXPECT_EQ(snapshot.registers.pc, 0U);
}

TEST_F(MachineTest, LoadsUpToTheLastByteOfMemory) {
    const Bytes program{0x11, 0x22, 0x33, 0x44};

    const auto result = machine.loadProgram(program, MemorySize - 4, MemorySize - 4);

    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.memory[MemorySize - 5], 0);
    EXPECT_EQ(snapshot.memory[MemorySize - 4], 0x11);
    EXPECT_EQ(snapshot.memory[LastAddress], 0x44);
    EXPECT_EQ(snapshot.registers.pc, MemorySize - 4);
}

TEST_F(MachineTest, LoadsASingleByteAtTheLastAddress) {
    const Bytes program{0x5A};

    const auto result = machine.loadProgram(program, LastAddress, LastAddress);

    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    EXPECT_EQ(machine.snapshot().memory[LastAddress], 0x5A);
}

TEST_F(MachineTest, LoadsAProgramThatFillsTheWholeMemory) {
    const Bytes program(MemorySize, 0xEE);

    const auto result = machine.loadProgram(program, 0, 0);

    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.memory.front(), 0xEE);
    EXPECT_EQ(snapshot.memory.back(), 0xEE);
}

TEST_F(MachineTest, RejectsEmptyProgram) {
    const auto result = machine.loadProgram(std::span<const std::uint8_t>{}, 0, 0);

    ASSERT_TRUE(std::holds_alternative<LoadFault>(result));
    EXPECT_EQ(std::get<LoadFault>(result).code, LoadFaultCode::EmptyProgram);
}

TEST_F(MachineTest, RejectsProgramCrossingTheEndOfMemory) {
    const Bytes program{1, 2, 3, 4};

    const auto crossing = machine.loadProgram(program, MemorySize - 3, MemorySize - 3);
    const Bytes tooLarge(MemorySize + 1, 0x01);
    const auto larger = machine.loadProgram(tooLarge, 0, 0);

    ASSERT_TRUE(std::holds_alternative<LoadFault>(crossing));
    EXPECT_EQ(std::get<LoadFault>(crossing).code, LoadFaultCode::ProgramExceedsMemory);
    EXPECT_EQ(std::get<LoadFault>(crossing).offendingValue, MemorySize + 1);
    ASSERT_TRUE(std::holds_alternative<LoadFault>(larger));
    EXPECT_EQ(std::get<LoadFault>(larger).code, LoadFaultCode::ProgramExceedsMemory);
}

TEST_F(MachineTest, RejectsLoadAddressOutsideMemory) {
    const Bytes program{1};

    const auto atSize = machine.loadProgram(program, MemorySize, 0);
    const auto huge = machine.loadProgram(program, 0xFFFFFFFF, 0);

    ASSERT_TRUE(std::holds_alternative<LoadFault>(atSize));
    EXPECT_EQ(std::get<LoadFault>(atSize).code, LoadFaultCode::LoadAddressOutOfRange);
    EXPECT_EQ(std::get<LoadFault>(atSize).offendingValue, MemorySize);
    ASSERT_TRUE(std::holds_alternative<LoadFault>(huge));
    EXPECT_EQ(std::get<LoadFault>(huge).code, LoadFaultCode::LoadAddressOutOfRange);
}

TEST_F(MachineTest, RejectsInitialPcOutsideMemory) {
    const Bytes program{1};

    const auto atSize = machine.loadProgram(program, 0, MemorySize);
    const auto wide = machine.loadProgram(program, 0, 0xFFFFFF);

    ASSERT_TRUE(std::holds_alternative<LoadFault>(atSize));
    EXPECT_EQ(std::get<LoadFault>(atSize).code, LoadFaultCode::InitialPcOutOfRange);
    ASSERT_TRUE(std::holds_alternative<LoadFault>(wide));
    EXPECT_EQ(std::get<LoadFault>(wide).code, LoadFaultCode::InitialPcOutOfRange);
}

TEST_F(MachineTest, AcceptsInitialPcOutsideTheLoadedRangeButInsideMemory) {
    const Bytes program{1, 2};

    const auto result = machine.loadProgram(program, 0x100, LastAddress);

    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(result));
    EXPECT_EQ(machine.snapshot().registers.pc, LastAddress);
}

TEST_F(MachineTest, RejectedLoadsLeaveTheWholeStateUntouched) {
    // Estado não trivial: registradores, memória escrita por um store e uma falha registrada.
    Program program = preambleProgram();
    program.at(PreambleEnd, format3(Opcode::STA, Simple, 0x100));
    program.at(0x18, {0xFF});
    load(program.bytes());
    stepTimes(PreambleSteps + 1);
    ASSERT_TRUE(std::holds_alternative<MachineFault>(machine.step()));
    const MachineSnapshot before = machine.snapshot();
    ASSERT_TRUE(before.lastFault.has_value());
    ASSERT_EQ(before.memory[0x102], 0x11);

    const Bytes bytes{1, 2, 3, 4};
    const Bytes tooLarge(MemorySize + 1, 0x01);
    const std::span<const std::uint8_t> empty;
    EXPECT_TRUE(isLoadFault(machine.loadProgram(empty, 0x400, 0x400)));
    EXPECT_TRUE(isLoadFault(machine.loadProgram(bytes, MemorySize, 0)));
    EXPECT_TRUE(isLoadFault(machine.loadProgram(bytes, MemorySize - 3, 0)));
    EXPECT_TRUE(isLoadFault(machine.loadProgram(tooLarge, 0, 0)));
    EXPECT_TRUE(isLoadFault(machine.loadProgram(bytes, 0x400, MemorySize)));

    const MachineSnapshot after = machine.snapshot();
    EXPECT_EQ(after.registers, before.registers);
    EXPECT_TRUE(after.memory == before.memory) << "a memória mudou após uma carga rejeitada";
    ASSERT_TRUE(after.lastFault.has_value());
    EXPECT_EQ(after.lastFault->instructionAddress, before.lastFault->instructionAddress);
}

TEST_F(MachineTest, ValidLoadDiscardsThePreviousState) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format3(Opcode::STA, Simple, 0x100));
    program.at(0x18, {0xFF});
    load(program.bytes());
    stepTimes(PreambleSteps + 1);
    ASSERT_TRUE(std::holds_alternative<MachineFault>(machine.step()));

    load({0x77}, 0x400);

    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.memory[0x400], 0x77);
    EXPECT_EQ(snapshot.memory[0x100], 0) << "memória antiga não foi zerada";
    EXPECT_EQ(snapshot.memory[0x0], 0x00);
    EXPECT_EQ(snapshot.registers.pc, 0x400U);
    EXPECT_EQ(snapshot.registers.a, 0U);
    EXPECT_EQ(snapshot.registers.x, 0U);
    EXPECT_EQ(snapshot.registers.l, 0U);
    EXPECT_EQ(snapshot.registers.sw, 0U);
    EXPECT_FALSE(snapshot.lastFault.has_value());
}

// ---------------------------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, ResetZeroesMemoryRegistersPcAndClearsFaults) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format3(Opcode::STA, Simple, 0x100));
    program.at(0x18, {0xFF});
    load(program.bytes());
    stepTimes(PreambleSteps + 1);
    ASSERT_TRUE(std::holds_alternative<MachineFault>(machine.step()));
    ASSERT_TRUE(machine.snapshot().lastFault.has_value());

    machine.reset();

    const MachineSnapshot snapshot = machine.snapshot();
    const MachineSnapshot fresh = Machine{}.snapshot();
    EXPECT_EQ(snapshot.memory.size(), 1048576U);
    EXPECT_TRUE(snapshot.memory == fresh.memory) << "a memória não foi zerada";
    EXPECT_EQ(snapshot.registers, fresh.registers);
    EXPECT_EQ(snapshot.registers.a, 0U);
    EXPECT_EQ(snapshot.registers.x, 0U);
    EXPECT_EQ(snapshot.registers.l, 0U);
    EXPECT_EQ(snapshot.registers.b, 0U);
    EXPECT_EQ(snapshot.registers.s, 0U);
    EXPECT_EQ(snapshot.registers.t, 0U);
    EXPECT_EQ(snapshot.registers.f, 0U);
    EXPECT_EQ(snapshot.registers.pc, 0U);
    EXPECT_EQ(snapshot.registers.sw, 0U);
    EXPECT_FALSE(snapshot.lastFault.has_value());
}

TEST_F(MachineTest, ResetLeavesTheMachineReadyToExecuteFromAddressZero) {
    load(format3(Opcode::LDA, Immediate, 5), 0x50);
    stepOk();

    machine.reset();
    const StepSuccess success = stepOk();

    EXPECT_EQ(success.instruction.instructionAddress, 0U);
    EXPECT_EQ(registers().pc, 3U);
}

// ---------------------------------------------------------------------------------------------
// Avanço do PC por formato
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, PcAdvancesTwoBytesForFormat2) {
    load(format2(Opcode::RMO, RegisterId::A, RegisterId::X), 0x10);

    const StepSuccess success = stepOk();

    EXPECT_EQ(success.instruction.format, InstructionFormat::Format2);
    EXPECT_EQ(success.nextPc, 0x12U);
    EXPECT_EQ(success.nextPc, *computePostFetchPc(0x10, success.instruction.length));
    EXPECT_EQ(registers().pc, 0x12U);
}

TEST_F(MachineTest, PcAdvancesThreeBytesForFormat3) {
    load(format3(Opcode::LDA, Immediate, 5), 0x10);

    const StepSuccess success = stepOk();

    EXPECT_EQ(success.instruction.format, InstructionFormat::Format3);
    EXPECT_EQ(success.nextPc, 0x13U);
    EXPECT_EQ(success.nextPc, *computePostFetchPc(0x10, success.instruction.length));
    EXPECT_EQ(registers().pc, 0x13U);
    EXPECT_EQ(registers().a, 5U);
}

TEST_F(MachineTest, PcAdvancesFourBytesForFormat4) {
    load(format4(Opcode::LDA, Immediate, 5), 0x10);

    const StepSuccess success = stepOk();

    EXPECT_EQ(success.instruction.format, InstructionFormat::Format4);
    EXPECT_EQ(success.nextPc, 0x14U);
    EXPECT_EQ(success.nextPc, *computePostFetchPc(0x10, success.instruction.length));
    EXPECT_EQ(registers().pc, 0x14U);
    EXPECT_EQ(registers().a, 5U);
}

TEST_F(MachineTest, PcAdvancesThreeBytesForSicFormat) {
    load(sic(Opcode::LDA, 0x0010), 0x10);

    const StepSuccess success = stepOk();

    EXPECT_EQ(success.instruction.format, InstructionFormat::Sic);
    EXPECT_EQ(success.nextPc, 0x13U);
    EXPECT_EQ(registers().pc, 0x13U);
}

TEST_F(MachineTest, Format1OpcodeIsNotPartOfTheInstructionSetAndFailsAsUnknown) {
    // As 38 instruções da VM não incluem o formato 1; FIX (0xC4) é tratado como desconhecido.
    load({0xC4}, 0x10);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<DecodeFault>(fault).code, DecodeFaultCode::UnknownOpcode);
    EXPECT_EQ(registers().pc, 0x10U);
}

// ---------------------------------------------------------------------------------------------
// Controle de fluxo
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, JTransfersUnconditionallyToTheResolvedAddress) {
    prepare(preambleProgram().at(PreambleEnd, format3(Opcode::J, Simple, 0x40)));
    const RegisterSnapshot before = registers();

    const StepSuccess success = stepOk();

    RegisterSnapshot expected = before;
    expected.pc = 0x40;
    EXPECT_EQ(registers(), expected);
    EXPECT_EQ(success.nextPc, 0x40U);
}

TEST_F(MachineTest, JSupportsPcRelativeIndirectImmediateAndFormat4Addressing) {
    load(format3(Opcode::J, Simple, 0xFFD, PcRelative), 0x10);
    stepOk();
    EXPECT_EQ(registers().pc, 0x10U) << "J * deve permanecer no mesmo endereço";
    stepOk();
    EXPECT_EQ(registers().pc, 0x10U);

    load(Program{}.at(0, format3(Opcode::J, Indirect, 0x40)).at(0x40, word(0x000123)).bytes());
    stepOk();
    EXPECT_EQ(registers().pc, 0x123U);

    load(format3(Opcode::J, Immediate, 0x50));
    stepOk();
    EXPECT_EQ(registers().pc, 0x50U);

    load(format4(Opcode::J, Simple, 0x12345));
    stepOk();
    EXPECT_EQ(registers().pc, 0x12345U);
}

TEST_F(MachineTest, JEQIsTakenInInitialStateBecauseConditionCodeStartsEqual) {
    load(format3(Opcode::JEQ, Simple, 0x30));

    stepOk();

    EXPECT_EQ(registers().pc, 0x30U);
}

TEST_F(MachineTest, JEQTakenOnlyWhenEqual) {
    expectJump(Opcode::JEQ, 5, 5, JumpTarget);
}

TEST_F(MachineTest, JEQNotTakenFallsThroughToPostFetchPc) {
    expectJump(Opcode::JEQ, 5, 6, NotTakenPc);
    expectJump(Opcode::JEQ, 7, 5, NotTakenPc);
}

TEST_F(MachineTest, JGTTakenOnlyWhenGreater) {
    expectJump(Opcode::JGT, 7, 5, JumpTarget);
}

TEST_F(MachineTest, JGTNotTakenFallsThroughToPostFetchPc) {
    expectJump(Opcode::JGT, 5, 5, NotTakenPc);
    expectJump(Opcode::JGT, 3, 5, NotTakenPc);
}

TEST_F(MachineTest, JLTTakenOnlyWhenLess) {
    expectJump(Opcode::JLT, 3, 5, JumpTarget);
}

TEST_F(MachineTest, JLTNotTakenFallsThroughToPostFetchPc) {
    expectJump(Opcode::JLT, 5, 5, NotTakenPc);
    expectJump(Opcode::JLT, 7, 5, NotTakenPc);
}

TEST_F(MachineTest, JSUBStoresPostFetchPcInLForFormat3) {
    prepare(preambleProgram().at(PreambleEnd, format3(Opcode::JSUB, Simple, 0x100)));
    ASSERT_EQ(registers().l, 0x33U);
    const RegisterSnapshot before = registers();

    const StepSuccess success = stepOk();

    RegisterSnapshot expected = before;
    expected.l = PreambleEnd + 3;
    expected.pc = 0x100;
    EXPECT_EQ(registers(), expected);
    EXPECT_EQ(success.nextPc, 0x100U);
}

TEST_F(MachineTest, JSUBStoresPostFetchPcInLForFormat4) {
    load(format4(Opcode::JSUB, Simple, 0x12345), 0x10);

    stepOk();

    EXPECT_EQ(registers().l, 0x14U);
    EXPECT_EQ(registers().pc, 0x12345U);
}

TEST_F(MachineTest, RSUBTransfersToTheAddressStoredInL) {
    load(concat({format3(Opcode::LDL, Immediate, 0x40), format3(Opcode::RSUB, Simple, 0)}));
    stepOk();
    const RegisterSnapshot before = registers();

    stepOk();

    RegisterSnapshot expected = before;
    expected.pc = 0x40;
    EXPECT_EQ(registers(), expected);
}

TEST_F(MachineTest, RSUBAcceptsTheSicEncoding) {
    load(concat({format3(Opcode::LDL, Immediate, 0x40), sic(Opcode::RSUB, 0)}));

    stepTimes(2);

    EXPECT_EQ(registers().pc, 0x40U);
}

TEST_F(MachineTest, JSUBFollowedByRSUBReturnsToTheInstructionAfterTheCall) {
    Program program;
    program.at(0x00, format3(Opcode::LDA, Immediate, 1));
    program.at(0x03, format3(Opcode::JSUB, Simple, 0x20));
    program.at(0x06, format3(Opcode::LDA, Immediate, 3));
    program.at(0x20, format3(Opcode::LDA, Immediate, 2));
    program.at(0x23, format3(Opcode::RSUB, Simple, 0));
    load(program.bytes());

    stepOk();
    EXPECT_EQ(registers().a, 1U);
    stepOk();
    EXPECT_EQ(registers().l, 0x06U);
    EXPECT_EQ(registers().pc, 0x20U);
    stepOk();
    EXPECT_EQ(registers().a, 2U);
    stepOk();
    EXPECT_EQ(registers().pc, 0x06U);
    EXPECT_EQ(registers().l, 0x06U);
    stepOk();
    EXPECT_EQ(registers().a, 3U);
    EXPECT_EQ(registers().pc, 0x09U);
}

TEST_F(MachineTest, PostFetchPcAtMemoryEndFailsBeforeAJumpCanExecute) {
    Program program;
    program.at(0x00, format4(Opcode::J, Simple, LastAddress - 2));
    program.at(LastAddress - 2, format3(Opcode::J, Simple, 0x10));
    load(program.bytes());
    stepOk();

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<CycleFault>(fault).code, CycleFaultCode::NextPcOutOfRange);
    EXPECT_EQ(causeOf<CycleFault>(fault).offendingValue, MemorySize);
    EXPECT_EQ(registers().pc, LastAddress - 2);
}

// ---------------------------------------------------------------------------------------------
// Uma instrução de cada família e as 38 instruções pelo ciclo completo
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, ExecutesOneInstructionOfEachFamily) {
    load(concat({format3(Opcode::LDA, Immediate, 5),
                 format3(Opcode::ADD, Immediate, 3),
                 format3(Opcode::J, Simple, 0x40)}));

    stepOk();
    EXPECT_EQ(registers().a, 5U) << "família de transferência";
    stepOk();
    EXPECT_EQ(registers().a, 8U) << "família aritmética";
    stepOk();
    EXPECT_EQ(registers().pc, 0x40U) << "família de controle";
}

TEST_F(MachineTest, ExecutesEveryRecognizedInstructionThroughTheFullCycle) {
    const Bytes data = word(5);

    struct Case {
        Opcode opcode;
        Bytes code;
    };
    const std::vector<Case> cases{
        {Opcode::ADD, memoryOperand(Opcode::ADD)},
        {Opcode::AND, memoryOperand(Opcode::AND)},
        {Opcode::COMP, memoryOperand(Opcode::COMP)},
        {Opcode::DIV, memoryOperand(Opcode::DIV)},
        {Opcode::J, jumpOperand(Opcode::J)},
        {Opcode::JEQ, jumpOperand(Opcode::JEQ)},
        {Opcode::JGT, jumpOperand(Opcode::JGT)},
        {Opcode::JLT, jumpOperand(Opcode::JLT)},
        {Opcode::JSUB, jumpOperand(Opcode::JSUB)},
        {Opcode::LDA, memoryOperand(Opcode::LDA)},
        {Opcode::LDB, memoryOperand(Opcode::LDB)},
        {Opcode::LDCH, memoryOperand(Opcode::LDCH)},
        {Opcode::LDL, memoryOperand(Opcode::LDL)},
        {Opcode::LDS, memoryOperand(Opcode::LDS)},
        {Opcode::LDT, memoryOperand(Opcode::LDT)},
        {Opcode::LDX, memoryOperand(Opcode::LDX)},
        {Opcode::MUL, memoryOperand(Opcode::MUL)},
        {Opcode::OR, memoryOperand(Opcode::OR)},
        {Opcode::RSUB, format3(Opcode::RSUB, Simple, 0)},
        {Opcode::STA, memoryOperand(Opcode::STA)},
        {Opcode::STB, memoryOperand(Opcode::STB)},
        {Opcode::STCH, memoryOperand(Opcode::STCH)},
        {Opcode::STL, memoryOperand(Opcode::STL)},
        {Opcode::STS, memoryOperand(Opcode::STS)},
        {Opcode::STT, memoryOperand(Opcode::STT)},
        {Opcode::STX, memoryOperand(Opcode::STX)},
        {Opcode::SUB, memoryOperand(Opcode::SUB)},
        {Opcode::TIX, memoryOperand(Opcode::TIX)},
        {Opcode::ADDR, format2(Opcode::ADDR, RegisterId::A, RegisterId::X)},
        {Opcode::CLEAR, format2(Opcode::CLEAR, RegisterId::T)},
        {Opcode::COMPR, format2(Opcode::COMPR, RegisterId::A, RegisterId::X)},
        {Opcode::DIVR, format2(Opcode::DIVR, RegisterId::S, RegisterId::A)},
        {Opcode::MULR, format2(Opcode::MULR, RegisterId::A, RegisterId::X)},
        {Opcode::RMO, format2(Opcode::RMO, RegisterId::A, RegisterId::X)},
        {Opcode::SHIFTL, format2Nibble(Opcode::SHIFTL, RegisterId::A, 3)},
        {Opcode::SHIFTR, format2Nibble(Opcode::SHIFTR, RegisterId::A, 3)},
        {Opcode::SUBR, format2(Opcode::SUBR, RegisterId::A, RegisterId::X)},
        {Opcode::TIXR, format2(Opcode::TIXR, RegisterId::T)},
    };
    ASSERT_EQ(cases.size(), 38U);

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testing::Message() << "opcode 0x" << std::hex << int(testCase.opcode));
        Program program = preambleProgram();
        program.at(PreambleEnd, format3(Opcode::J, Simple, 0x40));
        program.at(0x40, testCase.code);
        program.at(0x80, data);
        load(program.bytes());
        stepTimes(PreambleSteps + 1);
        ASSERT_EQ(registers().pc, 0x40U);

        const auto result = machine.step();

        ASSERT_TRUE(std::holds_alternative<StepSuccess>(result));
        EXPECT_EQ(std::get<StepSuccess>(result).instruction.opcode, testCase.opcode);
    }
}

// ---------------------------------------------------------------------------------------------
// Escritas de memória no limite
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, WordStoreEndingAtTheLastByteOfMemoryIsCommitted) {
    load(concat({format3(Opcode::LDA, Immediate, 0x12), format4(Opcode::STA, Simple, 0xFFFFD)}));
    stepOk();

    const StepSuccess success = stepOk();

    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.memory[0xFFFFC], 0x00);
    EXPECT_EQ(snapshot.memory[0xFFFFD], 0x00);
    EXPECT_EQ(snapshot.memory[0xFFFFE], 0x00);
    EXPECT_EQ(snapshot.memory[0xFFFFF], 0x12);
    ASSERT_TRUE(success.effect.memoryWrite.has_value());
    EXPECT_EQ(std::get<WordWrite>(*success.effect.memoryWrite), (WordWrite{0xFFFFD, 0x12}));
}

TEST_F(MachineTest, ByteStoreAtTheLastByteOfMemoryIsCommitted) {
    load(concat({format3(Opcode::LDA, Immediate, 0x7A), format4(Opcode::STCH, Simple, 0xFFFFF)}));

    stepTimes(2);

    const MachineSnapshot snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.memory[0xFFFFE], 0x00);
    EXPECT_EQ(snapshot.memory[0xFFFFF], 0x7A);
}

// ---------------------------------------------------------------------------------------------
// Falhas e atomicidade
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, UnknownOpcodeFailsWithoutChangingState) {
    prepare(preambleProgram().at(PreambleEnd, {0xFF}));

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(fault.instructionAddress, PreambleEnd);
    EXPECT_EQ(causeOf<DecodeFault>(fault).code, DecodeFaultCode::UnknownOpcode);
    EXPECT_EQ(causeOf<DecodeFault>(fault).instructionAddress, PreambleEnd);
    EXPECT_EQ(registers().pc, PreambleEnd);
}

TEST_F(MachineTest, TruncatedInstructionAtTheLastByteFailsWithoutChangingState) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format4(Opcode::J, Simple, LastAddress));
    program.at(LastAddress, {0x00});
    load(program.bytes());
    stepTimes(PreambleSteps + 1);
    ASSERT_EQ(registers().pc, LastAddress);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(fault.instructionAddress, LastAddress);
    EXPECT_EQ(causeOf<DecodeFault>(fault).code, DecodeFaultCode::TruncatedInstruction);
    EXPECT_EQ(causeOf<DecodeFault>(fault).bytesRead, 1);
}

TEST_F(MachineTest, TruncatedFormat4InstructionFailsWithoutChangingState) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format4(Opcode::J, Simple, LastAddress - 2));
    program.at(LastAddress - 2, {0x01, 0x10, 0x00});
    load(program.bytes());
    stepTimes(PreambleSteps + 1);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<DecodeFault>(fault).code, DecodeFaultCode::TruncatedInstruction);
    EXPECT_EQ(causeOf<DecodeFault>(fault).bytesRead, 3);
}

TEST_F(MachineTest, NegativePcRelativeAddressFailsAsInvalidAddressWithoutChangingState) {
    prepare(preambleProgram().at(PreambleEnd, format3(Opcode::LDA, Simple, 0x800, PcRelative)));

    const MachineFault fault = stepAtomicFault();

    const auto resolveFault = causeOf<ResolveFault>(fault);
    EXPECT_EQ(resolveFault.code, ResolveFaultCode::AddressOutOfRange);
    EXPECT_EQ(resolveFault.offendingValue, static_cast<std::int64_t>(PreambleEnd + 3) - 2048);
}

TEST_F(MachineTest, IndirectAddressOutsideMemoryFailsWithoutChangingState) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format3(Opcode::LDA, Indirect, 0x30));
    program.at(0x30, word(0xFFFFFF));
    prepare(program);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<ResolveFault>(fault).code, ResolveFaultCode::AddressOutOfRange);
    EXPECT_EQ(causeOf<ResolveFault>(fault).offendingValue, 0xFFFFFF);
}

TEST_F(MachineTest, DivisionByZeroFailsWithoutPartialEffects) {
    prepare(preambleProgram().at(PreambleEnd, format3(Opcode::DIV, Immediate, 0)));

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<ExecuteFault>(fault).code, ExecuteFaultCode::DivisionByZero);
    EXPECT_EQ(causeOf<ExecuteFault>(fault).instructionAddress, PreambleEnd);
    EXPECT_EQ(registers().a, 0x11U);
}

TEST_F(MachineTest, WordStoreCrossingTheEndOfMemoryFailsWithoutChangingState) {
    for (const std::uint32_t address : {LastAddress - 1, LastAddress}) {
        SCOPED_TRACE(testing::Message() << "endereço 0x" << std::hex << address);
        prepare(preambleProgram().at(PreambleEnd, format4(Opcode::STA, Simple, address)));

        const MachineFault fault = stepAtomicFault();

        EXPECT_EQ(causeOf<CycleFault>(fault).code, CycleFaultCode::MemoryWriteOutOfRange);
        EXPECT_EQ(causeOf<CycleFault>(fault).offendingValue, address);
    }
}

TEST_F(MachineTest, RSUBToAnAddressOutsideMemoryFailsWithoutChangingState) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format3(Opcode::LDL, Simple, 0x30));
    program.at(0x18, format3(Opcode::RSUB, Simple, 0));
    program.at(0x30, word(0x200000));
    prepare(program);
    stepOk();
    ASSERT_EQ(registers().l, 0x200000U);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<CycleFault>(fault).code, CycleFaultCode::NextPcOutOfRange);
    EXPECT_EQ(causeOf<CycleFault>(fault).offendingValue, 0x200000);
    EXPECT_EQ(registers().pc, 0x18U);
}

TEST_F(MachineTest, SequentialPcBeyondTheLastByteFailsWithoutChangingState) {
    Program program = preambleProgram();
    program.at(PreambleEnd, format4(Opcode::J, Simple, LastAddress - 1));
    program.at(LastAddress - 1, format2(Opcode::RMO, RegisterId::A, RegisterId::X));
    load(program.bytes());
    stepTimes(PreambleSteps + 1);
    ASSERT_EQ(registers().pc, LastAddress - 1);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<CycleFault>(fault).code, CycleFaultCode::NextPcOutOfRange);
    EXPECT_EQ(causeOf<CycleFault>(fault).offendingValue, MemorySize);
}

TEST_F(MachineTest, PostFetchPcOutsideMemoryIsRejectedBeforeExecution) {
    machine = Machine{4};
    load(format2(Opcode::CLEAR, RegisterId::PC), 2, 2);
    const MachineSnapshot before = machine.snapshot();
    ASSERT_EQ(before.memory.size(), 4U);
    ASSERT_EQ(before.registers.pc, 2U);

    const auto result = machine.step();

    ASSERT_TRUE(std::holds_alternative<MachineFault>(result));
    const MachineFault& fault = std::get<MachineFault>(result);
    EXPECT_EQ(fault.instructionAddress, 2U);
    EXPECT_EQ(causeOf<CycleFault>(fault).code, CycleFaultCode::NextPcOutOfRange);
    EXPECT_EQ(causeOf<CycleFault>(fault).offendingValue, 4);
    const MachineSnapshot after = machine.snapshot();
    EXPECT_EQ(after.registers.pc, 2U);
    EXPECT_EQ(after.registers, before.registers);
    EXPECT_TRUE(after.memory == before.memory);
}

TEST_F(MachineTest, PostFetchPcBeyondArchitecturalWidthFailsWithoutChangingState) {
    machine = Machine{0x1000010};
    load(format3(Opcode::LDA, Immediate, 5), 0xFFFFFE, 0xFFFFFE);

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<CycleFault>(fault).code, CycleFaultCode::PostFetchPcOutOfRange);
    EXPECT_EQ(causeOf<CycleFault>(fault).offendingValue, 0x1000001);
}

TEST_F(MachineTest, FailedStepRecordsTheFaultInTheSnapshot) {
    prepare(preambleProgram().at(PreambleEnd, {0xFF}));
    ASSERT_FALSE(machine.snapshot().lastFault.has_value());

    stepAtomicFault();

    const MachineSnapshot snapshot = machine.snapshot();
    ASSERT_TRUE(snapshot.lastFault.has_value());
    EXPECT_EQ(snapshot.lastFault->instructionAddress, PreambleEnd);
    EXPECT_EQ(causeOf<DecodeFault>(*snapshot.lastFault).code, DecodeFaultCode::UnknownOpcode);
}

// ---------------------------------------------------------------------------------------------
// Escritas de formato 2 em PC e SW
// ---------------------------------------------------------------------------------------------

TEST_F(MachineTest, RmoIntoPcIsATransferToTheCopiedValue) {
    load(concat({format3(Opcode::LDA, Immediate, 0x40),
                 format2(Opcode::RMO, RegisterId::A, RegisterId::PC)}));
    stepOk();

    const StepSuccess success = stepOk();

    EXPECT_EQ(registers().pc, 0x40U);
    EXPECT_EQ(registers().a, 0x40U);
    EXPECT_EQ(success.nextPc, 0x40U);
    EXPECT_FALSE(success.effect.registerWrite.has_value());
}

TEST_F(MachineTest, ClearPcTransfersToAddressZero) {
    load(format2(Opcode::CLEAR, RegisterId::PC), 0x10);

    const StepSuccess success = stepOk();

    EXPECT_EQ(success.nextPc, 0U);
    EXPECT_EQ(registers().pc, 0U);
    EXPECT_FALSE(success.effect.registerWrite.has_value());
}

TEST_F(MachineTest, FormatTwoInstructionsReadTheProgramCounterAfterFetch) {
    load(format2(Opcode::RMO, RegisterId::PC, RegisterId::L), 0x10);

    stepOk();

    EXPECT_EQ(registers().l, 0x12U);
    EXPECT_EQ(registers().pc, 0x12U);
}

TEST_F(MachineTest, SwCannotBeTheDestinationOfAFormat2InstructionAndStateIsPreserved) {
    prepare(preambleProgram().at(PreambleEnd, format2(Opcode::RMO, RegisterId::A, RegisterId::SW)));

    const MachineFault fault = stepAtomicFault();

    EXPECT_EQ(causeOf<ExecuteFault>(fault).code, ExecuteFaultCode::InvalidOperand);
}
