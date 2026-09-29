#pragma once
#include "application/MachineController.hpp"

#include <QWidget>

class QPushButton;
class QLabel;

class ExecutionPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExecutionPanel(QWidget* parent = nullptr);

    void setExecutionState(bool loaded, bool running, bool faulted);
    void showCurrentInstruction(const std::optional<DecodedInstruction>& instruction);

public slots:
    // Novos métodos para atualizar a interface com a instrução lida
    void updateInstruction(const StepResult& result);
    void clearInstruction();

signals:
    void loadRequested();
    void stepRequested();
    void runRequested();
    void pauseRequested();
    void resetRequested();

private:
    QPushButton* btnLoad_;
    QPushButton* btnStep_;
    QPushButton* btnRun_;
    QPushButton* btnPause_;
    QPushButton* btnReset_;
    QLabel* labelInstruction_; // Rótulo visual da instrução
    QLabel* labelCurrent_;
    QLabel* labelStatus_;
};
