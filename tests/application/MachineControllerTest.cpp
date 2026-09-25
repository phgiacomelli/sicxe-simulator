#include "application/MachineController.hpp"

#include "application/HexParser.hpp"

#include <QSignalSpy>
#include <QTest>

class MachineControllerTest : public QObject {
    Q_OBJECT
private slots:
    void commandsAreQueuedOnceAndSnapshotsAreIndependent() {
        MachineController controller(nullptr, 64);
        QSignalSpy loads(&controller, &MachineController::loadFinished);
        QSignalSpy steps(&controller, &MachineController::stepFinished);
        QSignalSpy resets(&controller, &MachineController::resetFinished);
        QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
        controller.loadProgram(HexParser::parse("01 00 05 19 00 01", 0, 0));
        controller.step();
        controller.step();
        QCOMPARE(loads.count(), 0);
        QTRY_COMPARE(steps.count(), 2);
        QCOMPARE(loads.count(), 1);
        QCOMPARE(snapshots.count(), 3);
        auto saved = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
        QCOMPARE(saved->registers.a, 6U);
        QCOMPARE(saved->registers.pc, 6U);
        controller.reset();
        QTRY_COMPARE(resets.count(), 1);
        auto cleared = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
        QCOMPARE(cleared->registers.a, 0U);
        QCOMPARE(cleared->memory[0], 0);
        QCOMPARE(saved->registers.a, 6U);
        QCOMPARE(saved->memory[0], 1);
    }

    void invalidInputAndAddressesPreserveSession() {
        MachineController controller(nullptr, 64);
        QSignalSpy loads(&controller, &MachineController::loadFinished);
        QSignalSpy rejected(&controller, &MachineController::inputRejected);
        QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
        controller.loadProgram(HexParser::parse("01 00 05", 0, 0));
        controller.step();
        QTRY_COMPARE(snapshots.count(), 2);
        auto before = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
        controller.loadProgram(HexParser::parse("GG", 0, 0));
        QTRY_COMPARE(rejected.count(), 1);
        QCOMPARE(qvariant_cast<InputFault>(rejected[0][0]).code, InputFaultCode::InvalidDigit);
        QCOMPARE(loads.count(), 1);
        const ProgramInput invalid[] = {
            {{1}, 64, 0}, {{1}, 0, 64}, {{1, 2}, 63, 0}, {{}, 0, 0}, {{1}, 0, 0x1000000}};
        const LoadFaultCode codes[] = {LoadFaultCode::LoadAddressOutOfRange,
                                       LoadFaultCode::InitialPcOutOfRange,
                                       LoadFaultCode::ProgramExceedsMemory,
                                       LoadFaultCode::EmptyProgram,
                                       LoadFaultCode::InitialPcOutOfRange};
        for (int i = 0; i < 5; ++i) {
            controller.loadProgram(invalid[i]);
            QTRY_COMPARE(loads.count(), i + 2);
            QCOMPARE(std::get<LoadFault>(qvariant_cast<LoadResult>(loads.last()[0])).code,
                     codes[i]);
            auto after = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
            QVERIFY(after->registers == before->registers);
            QVERIFY(after->memory == before->memory);
        }
    }

    void faultsRetainDetailsAndStopContinuousExecution() {
        MachineController controller(nullptr, 64);
        QSignalSpy steps(&controller, &MachineController::stepFinished);
        QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
        QSignalSpy running(&controller, &MachineController::runningChanged);
        controller.loadProgram(HexParser::parse("FF", 7, 7));
        controller.run();
        QTRY_COMPARE(steps.count(), 1);
        QCOMPARE(running.count(), 2);
        QVERIFY(!running.last()[0].toBool());
        // StepResult deliberately has no default constructor in the core contract.
        const auto value = steps[0][0];
        QVERIFY(value.metaType() == QMetaType::fromType<StepResult>());
        const auto fault =
            std::get<MachineFault>(*static_cast<const StepResult*>(value.constData()));
        QCOMPARE(fault.instructionAddress, 7U);
        const auto cause = std::get<DecodeFault>(fault.cause);
        QCOMPARE(cause.code, DecodeFaultCode::UnknownOpcode);
        QCOMPARE(cause.rawBytes[0], 0xFF);
        auto snapshot = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
        QVERIFY(snapshot->lastFault.has_value());
        QCOMPARE(snapshot->lastFault->instructionAddress, fault.instructionAddress);
        QCOMPARE(std::get<DecodeFault>(snapshot->lastFault->cause).rawBytes, cause.rawBytes);
        controller.loadProgram(HexParser::parse("GG", 0, 0));
        controller.loadProgram(ProgramInput{{1}, 64, 0});
        controller.requestSnapshot();
        QTRY_COMPARE(snapshots.count(), 4);
        auto preserved = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
        QVERIFY(preserved->lastFault.has_value());
        QCOMPARE(preserved->lastFault->instructionAddress, fault.instructionAddress);
        QCOMPARE(std::get<DecodeFault>(preserved->lastFault->cause).rawBytes, cause.rawBytes);
        QTest::qWait(10);
        QCOMPARE(steps.count(), 1);
    }

    void runIsResponsiveAndPauseIsAtInstructionBoundary() {
        MachineController controller(nullptr, 64);
        QSignalSpy steps(&controller, &MachineController::stepFinished);
        QSignalSpy running(&controller, &MachineController::runningChanged);
        QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
        int heartbeats = 0;
        QTimer heartbeat;
        heartbeat.setInterval(1);
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++heartbeats; });
        heartbeat.start();
        connect(&controller, &MachineController::stepFinished, this, [&] {
            if (steps.count() == 5)
                controller.pause();
        });
        controller.loadProgram(HexParser::parse("19 00 01 3F 00 00", 0, 0));
        controller.run();
        controller.run();
        QTRY_COMPARE(running.count(), 2);
        QCOMPARE(steps.count(), 5);
        QVERIFY(heartbeats > 0);
        auto paused = qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0]);
        QCOMPARE(paused->registers.a, 3U);
        QCOMPARE(paused->registers.pc, 3U);
        QTest::qWait(10);
        QCOMPARE(steps.count(), 5);
        controller.step();
        QTRY_COMPARE(steps.count(), 6);
        QCOMPARE(qvariant_cast<ImmutableMachineSnapshot>(snapshots.last()[0])->registers.pc, 0U);
    }

    void queuedSignalsPreserveResultsAndLoadStopsRun() {
        MachineController controller(nullptr, 64);
        QSignalSpy steps(&controller, &MachineController::stepFinished);
        QSignalSpy running(&controller, &MachineController::runningChanged);
        QSignalSpy loads(&controller, &MachineController::loadFinished);
        ImmutableMachineSnapshot delivered;
        std::optional<StepResult> deliveredStep;
        connect(
            &controller,
            &MachineController::snapshotChanged,
            this,
            [&](ImmutableMachineSnapshot value) { delivered = value; },
            Qt::QueuedConnection);
        connect(
            &controller,
            &MachineController::stepFinished,
            this,
            [&](StepResult value) { deliveredStep = value; },
            Qt::QueuedConnection);
        controller.loadProgram(HexParser::parse("3F 00 00", 0, 0));
        controller.run();
        QTRY_VERIFY(steps.count() > 0);
        QTRY_VERIFY(deliveredStep.has_value());
        QCOMPARE(std::get<StepSuccess>(*deliveredStep).nextPc, 0U);
        QCOMPARE(std::get<StepSuccess>(*deliveredStep).instruction.opcode, Opcode::J);
        controller.loadProgram(HexParser::parse("01 00 09", 10, 10));
        QTRY_COMPARE(loads.count(), 2);
        QTRY_VERIFY(delivered && delivered->registers.pc == 10U);
        QCOMPARE(running.count(), 2);
        const auto count = steps.count();
        QTest::qWait(10);
        QCOMPARE(steps.count(), count);
        controller.run();
        controller.reset();
        QTRY_COMPARE(running.count(), 4);
        QTRY_VERIFY(delivered->registers.pc == 0U);
    }

    void invalidLoadKeepsRunningAndSnapshotRequestDoesNotStep() {
        MachineController controller(nullptr, 64);
        QSignalSpy running(&controller, &MachineController::runningChanged);
        QSignalSpy steps(&controller, &MachineController::stepFinished);
        QSignalSpy rejected(&controller, &MachineController::inputRejected);
        QSignalSpy loads(&controller, &MachineController::loadFinished);
        QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
        controller.loadProgram(HexParser::parse("3F 00 00", 0, 0));
        controller.run();
        QTRY_VERIFY(steps.count() > 0);
        controller.loadProgram(HexParser::parse("X", 0, 0));
        controller.loadProgram(ProgramInput{{1}, 64, 0});
        QTRY_COMPARE(rejected.count(), 1);
        QTRY_COMPARE(loads.count(), 2);
        QCOMPARE(running.count(), 1);
        const auto count = steps.count();
        QTRY_VERIFY(steps.count() > count);
        controller.pause();
        QTRY_COMPARE(running.count(), 2);
        const auto pausedCount = steps.count();
        const auto snapshotCount = snapshots.count();
        controller.requestSnapshot();
        QTRY_COMPARE(snapshots.count(), snapshotCount + 1);
        QCOMPARE(steps.count(), pausedCount);
    }

    void reentrantCommandsWaitForSnapshotPublication() {
        MachineController controller(nullptr, 64);
        QSignalSpy snapshots(&controller, &MachineController::snapshotChanged);
        connect(
            &controller, &MachineController::stepFinished, &controller, &MachineController::reset);
        controller.loadProgram(HexParser::parse("01 00 05", 0, 0));
        controller.step();
        QTRY_COMPARE(snapshots.count(), 3);
        QCOMPARE(qvariant_cast<ImmutableMachineSnapshot>(snapshots[1][0])->registers.a, 5U);
        QCOMPARE(qvariant_cast<ImmutableMachineSnapshot>(snapshots[2][0])->registers.a, 0U);
    }
};

QTEST_GUILESS_MAIN(MachineControllerTest)
#include "MachineControllerTest.moc"
