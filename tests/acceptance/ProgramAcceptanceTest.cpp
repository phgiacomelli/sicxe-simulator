#include "application/HexParser.hpp"
#include "application/MachineController.hpp"

#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct Segment {
    std::size_t offset;
    const char* hex;
};

struct Case {
    const char* name;
    std::vector<Segment> segments;
    std::uint32_t loadAddress;
    std::uint32_t initialPc;
    std::size_t memorySize;
    int maxSteps;
    RegisterSnapshot expectedRegisters;
    std::vector<std::pair<std::size_t, std::uint8_t>> expectedWrites{};
    std::optional<MachineFault> expectedFault{};
    std::optional<Opcode> expectedCurrentOpcode{Opcode::LDA};
    InstructionFormat expectedCurrentFormat{InstructionFormat::Sic};
    std::array<std::uint8_t, 4> expectedCurrentBytes{};
};

RegisterSnapshot registers(std::uint32_t pc,
                           std::uint32_t a = 0,
                           std::uint32_t x = 0,
                           std::uint32_t l = 0,
                           std::uint32_t b = 0,
                           std::uint32_t sw = 0,
                           ConditionCode cc = ConditionCode::Equal) {
    RegisterSnapshot result;
    result.pc = pc;
    result.a = a;
    result.x = x;
    result.l = l;
    result.b = b;
    result.sw = sw;
    result.conditionCode = cc;
    return result;
}

std::vector<Case> cases() {
    return {
        {"load-add-store",
         {{0, "01 00 05 19 00 07 0F 00 20"}},
         0,
         0,
         256,
         3,
         registers(9, 12),
         {{0x20, 0}, {0x21, 0}, {0x22, 12}}},
        {"compare-less",
         {{0, "01 00 05 29 00 06"}},
         0,
         0,
         256,
         2,
         registers(6, 5, 0, 0, 0, 2, ConditionCode::Less)},
        {"compare-equal", {{0, "01 00 05 29 00 05"}}, 0, 0, 256, 2, registers(6, 5)},
        {"compare-greater",
         {{0, "01 00 05 29 00 04"}},
         0,
         0,
         256,
         2,
         registers(6, 5, 0, 0, 0, 1, ConditionCode::Greater)},
        {"direct", {{0, "03 00 20"}, {0x20, "00 00 07"}}, 0, 0, 256, 1, registers(3, 7)},
        {"indirect",
         {{0, "02 00 20"}, {0x20, "00 00 30"}, {0x30, "00 00 09"}},
         0,
         0,
         256,
         1,
         registers(3, 9)},
        {"indexed",
         {{0, "05 00 02 03 80 30"}, {0x32, "00 00 11"}},
         0,
         0,
         256,
         2,
         registers(6, 17, 2)},
        {"pc-relative", {{0, "03 20 0D"}, {0x10, "00 00 13"}}, 0, 0, 256, 1, registers(3, 19)},
        {"base-relative",
         {{0, "69 00 40 03 40 05"}, {0x45, "00 00 21"}},
         0,
         0,
         256,
         2,
         registers(6, 33, 0, 0, 64)},
        {"conditional-taken",
         {{0, "01 00 05 29 00 05 33 00 12"}},
         0,
         0,
         256,
         3,
         registers(0x12, 5)},
        {"conditional-not-taken",
         {{0, "01 00 05 29 00 05 37 00 12"}},
         0,
         0,
         256,
         3,
         registers(9, 5)},
        {"jsub-rsub", {{0, "4B 00 10"}, {0x10, "4F 00 00"}}, 0, 0, 256, 2, registers(3, 0, 0, 3)},
        {"format-4", {{0, "01 10 01 23"}}, 0x100, 0x100, 512, 1, registers(0x104, 0x123)},
        {"shifts", {{0, "01 00 01 A4 00 A8 00"}}, 0, 0, 256, 3, registers(7, 1)},
        {"unknown-opcode",
         {{0, "FF"}},
         0,
         0,
         256,
         1,
         registers(0),
         {},
         MachineFault{0, DecodeFault{DecodeFaultCode::UnknownOpcode, 0, {0xFF}, 1}},
         std::nullopt},
        {"memory-out-of-range",
         {{0, "03 00 3F"}},
         0,
         0,
         64,
         1,
         registers(0),
         {},
         MachineFault{0, ExecuteFault{ExecuteFaultCode::MemoryAccessOutOfRange, 0}},
         Opcode::LDA,
         InstructionFormat::Format3,
         {0x03, 0x00, 0x3F}},
        {"negative-pc-relative",
         {{0, "03 28 00"}},
         0,
         0,
         256,
         1,
         registers(0),
         {},
         MachineFault{0, ResolveFault{ResolveFaultCode::AddressOutOfRange, -2045}},
         Opcode::LDA,
         InstructionFormat::Format3,
         {0x03, 0x28, 0x00}},
        {"division-by-zero",
         {{0, "01 00 09 25 00 00"}},
         0,
         0,
         256,
         2,
         registers(3, 9),
         {},
         MachineFault{3, ExecuteFault{ExecuteFaultCode::DivisionByZero, 3, 0}},
         Opcode::DIV,
         InstructionFormat::Format3,
         {0x25, 0x00, 0x00}},
    };
}

std::string asHex(const std::vector<std::uint8_t>& bytes) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const auto byte : bytes) {
        if (!result.empty())
            result += ' ';
        result += digits[byte >> 4];
        result += digits[byte & 0x0F];
    }
    return result;
}

std::vector<std::uint8_t> imageFor(const Case& item) {
    std::vector<std::uint8_t> image;
    for (const auto& segment : item.segments) {
        const auto parsed = HexParser::parse(segment.hex, 0, 0);
        if (!std::holds_alternative<ProgramInput>(parsed))
            return {};
        const auto& bytes = std::get<ProgramInput>(parsed).bytes;
        if (image.size() < segment.offset + bytes.size())
            image.resize(segment.offset + bytes.size());
        std::copy(bytes.begin(), bytes.end(), image.begin() + segment.offset);
    }
    return image;
}

bool faultMatches(const MachineFault& actual, const MachineFault& expected) {
    if (actual.instructionAddress != expected.instructionAddress ||
        actual.cause.index() != expected.cause.index()) {
        return false;
    }
    return std::visit(
        [&](const auto& expectedCause) {
            using Fault = std::decay_t<decltype(expectedCause)>;
            const auto& actualCause = std::get<Fault>(actual.cause);
            if constexpr (std::is_same_v<Fault, DecodeFault>) {
                return actualCause.code == expectedCause.code &&
                       actualCause.instructionAddress == expectedCause.instructionAddress &&
                       actualCause.rawBytes == expectedCause.rawBytes &&
                       actualCause.bytesRead == expectedCause.bytesRead;
            } else {
                return actualCause == expectedCause;
            }
        },
        expected.cause);
}

} // namespace

class ProgramAcceptanceTest : public QObject {
    Q_OBJECT
private slots:
    void encodedProgramsProduceExpectedSnapshots() {
        for (const auto& item : cases()) {
            const auto image = imageFor(item);
            QVERIFY2(!image.empty(), item.name);
            MachineController controller(nullptr, item.memorySize);
            QSignalSpy loads(&controller, &MachineController::loadFinished);
            QSignalSpy steps(&controller, &MachineController::stepFinished);
            QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
            controller.loadProgram(
                HexParser::parse(asHex(image), item.loadAddress, item.initialPc));
            QTRY_COMPARE(loads.count(), 1);
            QVERIFY2(std::holds_alternative<LoadSuccess>(qvariant_cast<LoadResult>(loads[0][0])),
                     item.name);
            for (int n = 0; n < item.maxSteps; ++n) {
                controller.step();
                QTRY_COMPARE(steps.count(), n + 1);
            }
            QCOMPARE(snapshots.count(), item.maxSteps + 1);
            const auto final = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
            if (final->registers != item.expectedRegisters)
                qWarning() << item.name << "actual PC/A/X/L/B/SW" << final->registers.pc
                           << final->registers.a << final->registers.x << final->registers.l
                           << final->registers.b << final->registers.sw;
            QVERIFY2(final->registers == item.expectedRegisters, item.name);
            std::vector<std::uint8_t> expectedMemory(item.memorySize);
            std::copy(image.begin(), image.end(), expectedMemory.begin() + item.loadAddress);
            for (const auto& [address, value] : item.expectedWrites)
                expectedMemory[address] = value;
            QVERIFY2(final->memory == expectedMemory, item.name);
            QCOMPARE(final->lastFault.has_value(), item.expectedFault.has_value());
            if (item.expectedFault)
                QVERIFY2(faultMatches(*final->lastFault, *item.expectedFault), item.name);
            QCOMPARE(final->currentInstruction.has_value(), item.expectedCurrentOpcode.has_value());
            if (item.expectedCurrentOpcode) {
                const auto& current = *final->currentInstruction;
                QVERIFY2(current.opcode == *item.expectedCurrentOpcode, item.name);
                QVERIFY2(current.format == item.expectedCurrentFormat, item.name);
                QCOMPARE(current.instructionAddress, item.expectedRegisters.pc);
                QCOMPARE(current.length, std::uint8_t{3});
                QVERIFY2(current.rawBytes == item.expectedCurrentBytes, item.name);
            }
        }
    }
};

QTEST_GUILESS_MAIN(ProgramAcceptanceTest)
#include "ProgramAcceptanceTest.moc"
