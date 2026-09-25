#pragma once

#include "ProgramInput.hpp"
#include "core/Machine.hpp"

#include <QObject>
#include <QTimer>
#include <memory>

using ImmutableMachineSnapshot = std::shared_ptr<const MachineSnapshot>;
Q_DECLARE_METATYPE(ProgramInputResult)
Q_DECLARE_METATYPE(InputFault)
Q_DECLARE_METATYPE(LoadResult)
Q_DECLARE_METATYPE(StepResult)
Q_DECLARE_METATYPE(ImmutableMachineSnapshot)

class MachineController : public QObject {
    Q_OBJECT
public:
    explicit MachineController(QObject* parent = nullptr,
                               std::size_t memorySize = Machine::DefaultMemorySize);

public slots:
    void loadProgram(ProgramInputResult input);
    void reset();
    void step();
    void run();
    void pause();
    void requestSnapshot();

signals:
    void inputRejected(InputFault fault);
    void loadFinished(LoadResult result);
    void stepFinished(StepResult result);
    void resetFinished();
    void runningChanged(bool running);
    void snapshotChanged(ImmutableMachineSnapshot snapshot);

private:
    Machine machine_;
    QTimer* timer_;
    void setRunning(bool running);
    void executeStep();
    void publishSnapshot();
};
