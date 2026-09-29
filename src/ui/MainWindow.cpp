#include "MainWindow.hpp"

#include "ExecutionPanel.hpp"
#include "FaultPanel.hpp"
#include "InputFaultText.hpp"
#include "MemoryPanel.hpp"
#include "ProgramLoadDialog.hpp"
#include "RegisterPanel.hpp"

#include <QDockWidget>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("SIC/XE Simulator");
    resize(1200, 800);
    controller_ = new MachineController(this);
    setupUi();
}

void MainWindow::setupUi() {
    executionPanel_ = new ExecutionPanel(this);
    registerPanel_ = new RegisterPanel(this);
    memoryPanel_ = new MemoryPanel(this);
    faultPanel_ = new FaultPanel(this);

    setCentralWidget(memoryPanel_);

    auto* execDock = new QDockWidget("Controle de Execucao", this);
    execDock->setWidget(executionPanel_);
    execDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::TopDockWidgetArea, execDock);

    auto* regDock = new QDockWidget("Registradores", this);
    regDock->setWidget(registerPanel_);
    regDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, regDock);

    auto* faultDock = new QDockWidget("Diagnostico e Falhas", this);
    faultDock->setWidget(faultPanel_);
    faultDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, faultDock);

    connect(executionPanel_, &ExecutionPanel::loadRequested, this, [this] {
        auto* dialog = new ProgramLoadDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::ApplicationModal);
        connect(
            dialog, &ProgramLoadDialog::programReady, controller_, &MachineController::loadProgram);
        connect(controller_,
                &MachineController::loadFinished,
                dialog,
                &ProgramLoadDialog::handleLoadResult);
        dialog->open();
    });

    connect(controller_, &MachineController::inputRejected, this, [this](InputFault fault) {
        QMessageBox::critical(this, "Erro de Carga", inputFaultMessage(fault));
    });

    connect(executionPanel_, &ExecutionPanel::stepRequested, controller_, &MachineController::step);
    connect(executionPanel_, &ExecutionPanel::runRequested, controller_, &MachineController::run);
    connect(
        executionPanel_, &ExecutionPanel::pauseRequested, controller_, &MachineController::pause);
    connect(
        executionPanel_, &ExecutionPanel::resetRequested, controller_, &MachineController::reset);

    connect(controller_, &MachineController::runningChanged, this, [this](bool running) {
        running_ = running;
        updateExecutionState();
    });

    connect(controller_,
            &MachineController::snapshotChanged,
            this,
            [this](ImmutableMachineSnapshot snapshot) {
                registerPanel_->updateRegisters(snapshot->registers, highlightNextSnapshot_);
                memoryPanel_->updateMemory(snapshot->memory, changedMemory_);
                faultPanel_->updateFault(snapshot->lastFault);
                if (loaded_)
                    executionPanel_->showCurrentInstruction(snapshot->currentInstruction);
                highlightNextSnapshot_ = false;
                changedMemory_.clear();
            });

    connect(controller_, &MachineController::stepFinished, this, [this](const StepResult& result) {
        executionPanel_->updateInstruction(result);
        faulted_ = std::holds_alternative<MachineFault>(result);
        highlightNextSnapshot_ = true;
        changedMemory_.clear();
        if (const auto* success = std::get_if<StepSuccess>(&result)) {
            if (success->effect.memoryWrite) {
                if (const auto* byte = std::get_if<ByteWrite>(&*success->effect.memoryWrite)) {
                    changedMemory_.push_back(byte->address);
                } else {
                    const auto& word = std::get<WordWrite>(*success->effect.memoryWrite);
                    changedMemory_ = {word.address, word.address + 1, word.address + 2};
                }
            }
        }
        updateExecutionState();
    });

    connect(controller_, &MachineController::resetFinished, this, [this] {
        loaded_ = false;
        faulted_ = false;
        highlightNextSnapshot_ = false;
        changedMemory_.clear();
        executionPanel_->clearInstruction();
        updateExecutionState();
    });
    connect(controller_, &MachineController::loadFinished, this, [this](const LoadResult& result) {
        if (std::holds_alternative<LoadSuccess>(result)) {
            loaded_ = true;
            faulted_ = false;
            highlightNextSnapshot_ = false;
            changedMemory_.clear();
            executionPanel_->clearInstruction();
            updateExecutionState();
        }
    });
    controller_->requestSnapshot();
}

void MainWindow::updateExecutionState() {
    executionPanel_->setExecutionState(loaded_, running_, faulted_);
}
