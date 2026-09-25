#include "MachineController.hpp"

#include <QMetaObject>
#include <utility>

MachineController::MachineController(QObject* parent, std::size_t memorySize)
    : QObject(parent), machine_(memorySize), timer_(new QTimer(this)) {
    qRegisterMetaType<ProgramInputResult>();
    qRegisterMetaType<InputFault>();
    qRegisterMetaType<LoadResult>();
    qRegisterMetaType<StepResult>();
    qRegisterMetaType<ImmutableMachineSnapshot>();
    timer_->setInterval(1);
    connect(timer_, &QTimer::timeout, this, &MachineController::executeStep);
}

void MachineController::loadProgram(ProgramInputResult input) {
    QMetaObject::invokeMethod(
        this,
        [this, input = std::move(input)] {
            if (const auto* fault = std::get_if<InputFault>(&input)) {
                emit inputRejected(*fault);
                return;
            }
            const auto& program = std::get<ProgramInput>(input);
            const auto result =
                machine_.loadProgram(program.bytes, program.loadAddress, program.initialPc);
            if (std::holds_alternative<LoadSuccess>(result))
                setRunning(false);
            emit loadFinished(result);
            publishSnapshot();
        },
        Qt::QueuedConnection);
}

void MachineController::reset() {
    QMetaObject::invokeMethod(
        this,
        [this] {
            setRunning(false);
            machine_.reset();
            emit resetFinished();
            publishSnapshot();
        },
        Qt::QueuedConnection);
}

void MachineController::step() {
    QMetaObject::invokeMethod(
        this,
        [this] {
            setRunning(false);
            executeStep();
        },
        Qt::QueuedConnection);
}

void MachineController::run() {
    QMetaObject::invokeMethod(this, [this] { setRunning(true); }, Qt::QueuedConnection);
}

void MachineController::pause() {
    QMetaObject::invokeMethod(this, [this] { setRunning(false); }, Qt::QueuedConnection);
}

void MachineController::requestSnapshot() {
    QMetaObject::invokeMethod(this, [this] { publishSnapshot(); }, Qt::QueuedConnection);
}

void MachineController::setRunning(bool running) {
    if (running == timer_->isActive())
        return;
    if (running)
        timer_->start();
    else
        timer_->stop();
    emit runningChanged(running);
}

void MachineController::executeStep() {
    const auto result = machine_.step();
    if (std::holds_alternative<MachineFault>(result))
        setRunning(false);
    emit stepFinished(result);
    publishSnapshot();
}

void MachineController::publishSnapshot() {
    emit snapshotChanged(std::make_shared<const MachineSnapshot>(machine_.snapshot()));
}
