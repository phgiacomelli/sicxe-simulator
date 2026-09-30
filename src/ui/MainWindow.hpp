#pragma once
#include "application/MachineController.hpp"

#include <QMainWindow>

class ExecutionPanel;
class RegisterPanel;
class MemoryPanel;
class FaultPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    MachineController* controller_;
    ExecutionPanel* executionPanel_;
    RegisterPanel* registerPanel_;
    MemoryPanel* memoryPanel_;
    FaultPanel* faultPanel_;
    bool loaded_ = false;
    bool running_ = false;
    bool faulted_ = false;
    bool highlightNextSnapshot_ = false;
    std::vector<std::uint32_t> changedMemory_;

    void setupUi();
    void updateExecutionState();
};
