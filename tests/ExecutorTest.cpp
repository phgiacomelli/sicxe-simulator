#include "core/Executor.hpp"

#include "core/Memory.hpp"
#include "core/RegisterFile.hpp"
#include "core/execution/ArithmeticOperations.hpp"
#include "core/execution/ControlOperations.hpp"
#include "core/execution/DataOperations.hpp"

#include <gtest/gtest.h>
#include <set>
#include <variant>
#include <vector>

namespace {

enum class Family {
    Control,
    Data,
    Arithmetic,
};

struct DispatchCase {
    Opcode opcode;
    Family family;
    ResolvedOperand operand;
};

constexpr std::uint32_t Target = 0x002000;
constexpr std::uint32_t DataAddress = 0x000200;

// Um operando válido para cada um dos 38 opcodes reconhecidos pela VM.
const std::vector<DispatchCase>& allDispatchCases() {
    static const std::vector<DispatchCase> cases{
        {Opcode::J, Family::Control, MemoryAddress{Target}},
        {Opcode::JEQ, Family::Control, MemoryAddress{Target}},
        {Opcode::JGT, Family::Control, MemoryAddress{Target}},
        {Opcode::JLT, Family::Control, MemoryAddress{Target}},
        {Opcode::JSUB, Family::Control, MemoryAddress{Target}},
        {Opcode::RSUB, Family::Control, NoOperand{}},

        {Opcode::CLEAR, Family::Data, SingleRegisterOperand{RegisterId::T}},
        {Opcode::RMO, Family::Data, RegisterPairOperands{RegisterId::A, RegisterId::X}},
        {Opcode::LDA, Family::Data, ImmediateValue{5}},
        {Opcode::LDB, Family::Data, ImmediateValue{5}},
        {Opcode::LDL, Family::Data, ImmediateValue{5}},
        {Opcode::LDS, Family::Data, ImmediateValue{5}},
        {Opcode::LDT, Family::Data, ImmediateValue{5}},
        {Opcode::LDX, Family::Data, ImmediateValue{5}},
        {Opcode::STA, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::STB, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::STL, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::STS, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::STT, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::STX, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::LDCH, Family::Data, ImmediateValue{5}},
        {Opcode::STCH, Family::Data, MemoryAddress{DataAddress}},
        {Opcode::AND, Family::Data, ImmediateValue{5}},
        {Opcode::OR, Family::Data, ImmediateValue{5}},

        {Opcode::ADD, Family::Arithmetic, ImmediateValue{5}},
        {Opcode::SUB, Family::Arithmetic, ImmediateValue{5}},
        {Opcode::MUL, Family::Arithmetic, ImmediateValue{5}},
        {Opcode::DIV, Family::Arithmetic, ImmediateValue{5}},
        {Opcode::COMP, Family::Arithmetic, ImmediateValue{5}},
        {Opcode::TIX, Family::Arithmetic, ImmediateValue{5}},
        {Opcode::ADDR, Family::Arithmetic, RegisterPairOperands{RegisterId::S, RegisterId::A}},
        {Opcode::SUBR, Family::Arithmetic, RegisterPairOperands{RegisterId::S, RegisterId::A}},
        {Opcode::MULR, Family::Arithmetic, RegisterPairOperands{RegisterId::S, RegisterId::A}},
        {Opcode::DIVR, Family::Arithmetic, RegisterPairOperands{RegisterId::S, RegisterId::A}},
        {Opcode::COMPR, Family::Arithmetic, RegisterPairOperands{RegisterId::S, RegisterId::A}},
        {Opcode::SHIFTL, Family::Arithmetic, RegisterAndNibbleOperand{RegisterId::A, 3}},
        {Opcode::SHIFTR, Family::Arithmetic, RegisterAndNibbleOperand{RegisterId::A, 3}},
        {Opcode::TIXR, Family::Arithmetic, SingleRegisterOperand{RegisterId::T}},
    };
    return cases;
}

std::size_t countFamily(Family family) {
    std::size_t count = 0;
    for (const auto& testCase : allDispatchCases()) {
        if (testCase.family == family) {
            ++count;
        }
    }
    return count;
}

} // namespace

class ExecutorTest : public ::testing::Test {
protected:
    Executor executor;
    Memory memory{0x100000};
    RegisterFile registers;
    std::uint32_t postFetchPc = 0x1003;

    void SetUp() override {
        registers.write(RegisterId::A, 0x000004);
        registers.write(RegisterId::S, 0x000002);
        registers.write(RegisterId::L, 0x000777);
    }

    DecodedInstruction instruction(Opcode opcode) const {
        DecodedInstruction decoded{};
        decoded.opcode = opcode;
        decoded.format = InstructionFormat::Format3;
        decoded.instructionAddress = 0x1000;
        decoded.length = 3;
        return decoded;
    }

    ExecuteResult dispatch(const DispatchCase& testCase) const {
        return executor.execute(
            instruction(testCase.opcode), testCase.operand, postFetchPc, memory, registers);
    }

    ExecuteResult callFamily(const DispatchCase& testCase) const {
        const auto decoded = instruction(testCase.opcode);
        const auto& operand = testCase.operand;
        switch (testCase.family) {
            case Family::Control:
                return executeControlOperation(decoded, operand, postFetchPc, memory, registers);
            case Family::Data:
                return executeDataOperation(decoded, operand, postFetchPc, memory, registers);
            case Family::Arithmetic:
                return executeArithmeticOperation(decoded, operand, postFetchPc, memory, registers);
        }
        return ExecuteFault{};
    }

    ExecutionEffect effectOf(Opcode opcode, const ResolvedOperand& operand) const {
        const auto result =
            executor.execute(instruction(opcode), operand, postFetchPc, memory, registers);
        return std::get<ExecutionEffect>(result);
    }
};

TEST_F(ExecutorTest, CoversExactlyThe38RecognizedOpcodesOnce) {
    std::set<Opcode> opcodes;
    for (const auto& testCase : allDispatchCases()) {
        opcodes.insert(testCase.opcode);
    }

    EXPECT_EQ(allDispatchCases().size(), 38U);
    EXPECT_EQ(opcodes.size(), 38U);
    EXPECT_EQ(countFamily(Family::Control), 6U);
    EXPECT_EQ(countFamily(Family::Data), 18U);
    EXPECT_EQ(countFamily(Family::Arithmetic), 14U);
}

TEST_F(ExecutorTest, DispatchesEveryOpcodeToAnEffect) {
    for (const auto& testCase : allDispatchCases()) {
        const auto result = dispatch(testCase);

        EXPECT_TRUE(std::holds_alternative<ExecutionEffect>(result))
            << "opcode 0x" << std::hex << static_cast<int>(testCase.opcode);
    }
}

TEST_F(ExecutorTest, ReturnsExactlyWhatTheOwningFamilyProduces) {
    for (const auto& testCase : allDispatchCases()) {
        EXPECT_EQ(dispatch(testCase), callFamily(testCase))
            << "opcode 0x" << std::hex << static_cast<int>(testCase.opcode);
    }
}

TEST_F(ExecutorTest, ControlOperationsTransferTheProgramCounter) {
    registers.setConditionCode(ConditionCode::Equal);

    const auto jump = effectOf(Opcode::JEQ, MemoryAddress{Target});
    const auto subroutine = effectOf(Opcode::JSUB, MemoryAddress{Target});
    const auto ret = effectOf(Opcode::RSUB, NoOperand{});

    EXPECT_EQ(jump.nextPc, Target);
    EXPECT_EQ(subroutine.nextPc, Target);
    ASSERT_TRUE(subroutine.registerWrite.has_value());
    EXPECT_EQ(subroutine.registerWrite->value, postFetchPc);
    EXPECT_EQ(ret.nextPc, 0x000777U);
}

TEST_F(ExecutorTest, DataAndArithmeticOperationsKeepSequentialPc) {
    const auto load = effectOf(Opcode::LDA, ImmediateValue{9});
    const auto add = effectOf(Opcode::ADD, ImmediateValue{9});

    EXPECT_EQ(load.nextPc, postFetchPc);
    EXPECT_EQ(add.nextPc, postFetchPc);
}

TEST_F(ExecutorTest, PropagatesFamilyFaults) {
    const auto div = instruction(Opcode::DIV);
    const auto jump = instruction(Opcode::J);
    const auto divisionByZero =
        executor.execute(div, ImmediateValue{0}, postFetchPc, memory, registers);
    const auto invalidOperand = executor.execute(jump, NoOperand{}, postFetchPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(divisionByZero));
    EXPECT_EQ(std::get<ExecuteFault>(divisionByZero).code, ExecuteFaultCode::DivisionByZero);
    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(invalidOperand));
    EXPECT_EQ(std::get<ExecuteFault>(invalidOperand).code, ExecuteFaultCode::InvalidOperand);
}

TEST_F(ExecutorTest, RejectsOpcodeValueOutsideTheRecognizedSet) {
    const auto unknown = instruction(static_cast<Opcode>(0xFF));
    const auto result = executor.execute(unknown, NoOperand{}, postFetchPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    const auto& fault = std::get<ExecuteFault>(result);
    EXPECT_EQ(fault.code, ExecuteFaultCode::UnsupportedOpcode);
    EXPECT_EQ(fault.instructionAddress, 0x1000U);
}

TEST_F(ExecutorTest, DoesNotMutateMemoryOrRegisters) {
    memory.writeWord(DataAddress, 0x00ABCD);
    const auto a = registers.read(RegisterId::A);
    const auto l = registers.read(RegisterId::L);

    for (const auto& testCase : allDispatchCases()) {
        dispatch(testCase);
    }

    EXPECT_EQ(memory.readWord(DataAddress), 0x00ABCDU);
    EXPECT_EQ(registers.read(RegisterId::A), a);
    EXPECT_EQ(registers.read(RegisterId::L), l);
    EXPECT_EQ(registers.read(RegisterId::PC), 0U);
}
