#include "core/Machine.hpp"
#include "ui/ExecutionPanel.hpp"
#include "ui/FaultPanel.hpp"
#include "ui/MainWindow.hpp"
#include "ui/MemoryPanel.hpp"
#include "ui/ProgramLoadDialog.hpp"
#include "ui/RegisterPanel.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QTest>
#include <QTextEdit>
#include <optional>

class InterfaceTest : public QObject {
    Q_OBJECT
private slots:
    void controlsRequireLoadedProgram() {
        ExecutionPanel panel;
        const auto buttons = panel.findChildren<QPushButton*>();
        QPushButton* step = nullptr;
        QPushButton* run = nullptr;
        for (auto* button : buttons) {
            if (button->text() == "Executar Passo")
                step = button;
            if (button->text() == "Executar")
                run = button;
        }
        QVERIFY(step);
        QVERIFY(run);
        QVERIFY(!step->isEnabled());
        QVERIFY(!run->isEnabled());
    }

    void parsedInputWaitsForMachineValidation() {
        ProgramLoadDialog dialog;
        QSignalSpy ready(&dialog, &ProgramLoadDialog::programReady);
        dialog.findChild<QTextEdit*>()->setPlainText("01 00 05");
        QPushButton* load = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                load = button;
        }
        QVERIFY(load);
        QTest::mouseClick(load, Qt::LeftButton);
        QCOMPARE(dialog.result(), 0);
        QCOMPARE(ready.count(), 1);
    }

    void binaryFileCanBeSelectedAndSubmittedForMachineValidation() {
        QTemporaryFile file;
        QVERIFY(file.open());
        QCOMPARE(file.write(QByteArray::fromHex("010005")), 3);
        QVERIFY(file.flush());
        file.close();

        ProgramLoadDialog dialog;
        std::optional<ProgramInput> submitted;
        connect(&dialog, &ProgramLoadDialog::programReady, &dialog, [&](ProgramInput program) {
            submitted = std::move(program);
        });
        QRadioButton* binary = nullptr;
        for (auto* radio : dialog.findChildren<QRadioButton*>()) {
            if (radio->text().startsWith("Arquivo Binario"))
                binary = radio;
        }
        QVERIFY(binary);
        binary->click();
        auto* path = dialog.findChild<QLineEdit*>("binaryPath");
        QVERIFY(path);
        path->setText(file.fileName());
        dialog.findChild<QLineEdit*>("loadAddress")->setText("10");
        dialog.findChild<QLineEdit*>("initialPc")->setText("10");
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                QTest::mouseClick(button, Qt::LeftButton);
        }
        QVERIFY(submitted.has_value());
        QCOMPARE(submitted->bytes, (std::vector<std::uint8_t>{0x01, 0x00, 0x05}));
        QCOMPARE(submitted->loadAddress, 0x10U);
        QCOMPARE(submitted->initialPc, 0x10U);
        QCOMPARE(dialog.result(), 0);
    }

    void currentInstructionShowsDecodedOpcodeAndBytes() {
        ExecutionPanel panel;
        Machine machine(64);
        const std::vector<std::uint8_t> bytes{0x01, 0x00, 0x05};
        machine.loadProgram(bytes, 0, 0);
        panel.showCurrentInstruction(machine.snapshot().currentInstruction);
        bool found = false;
        for (auto* label : panel.findChildren<QLabel*>()) {
            if (label->text().contains("LDA") && label->text().contains("01 00 05"))
                found = true;
        }
        QVERIFY(found);
    }

    void memoryRangeAndChangedByteAreVisible() {
        MemoryPanel panel;
        std::vector<std::uint8_t> memory(64);
        memory[16] = 0xAB;
        memory[17] = 0xCD;
        memory[32] = 0xEF;
        panel.updateMemory(memory, {17});
        auto* address = panel.findChild<QLineEdit*>();
        auto* length = panel.findChild<QSpinBox*>();
        auto* table = panel.findChild<QTableWidget*>();
        QVERIFY(address);
        QVERIFY(length);
        QVERIFY(table);
        address->setText("10");
        length->setValue(32);
        QPushButton* search = nullptr;
        for (auto* button : panel.findChildren<QPushButton*>()) {
            if (button->text() == "Buscar")
                search = button;
        }
        QVERIFY(search);
        QTest::mouseClick(search, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 0)->text(), QString("AB"));
        QCOMPARE(table->item(0, 1)->text(), QString("CD"));
        QVERIFY(table->item(0, 1)->background().color() != table->item(0, 0)->background().color());
        address->setText("20");
        length->setValue(16);
        QTest::mouseClick(search, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->text(), QString("EF"));
    }

    void memoryRangeDoesNotHighlightBytesOutsideTheSelectedLength() {
        MemoryPanel panel;
        std::vector<std::uint8_t> memory(64);
        panel.updateMemory(memory, {47});
        auto* address = panel.findChild<QLineEdit*>();
        auto* length = panel.findChild<QSpinBox*>();
        auto* table = panel.findChild<QTableWidget*>();
        QVERIFY(address && length && table);
        address->setText("10");
        length->setValue(17);
        for (auto* button : panel.findChildren<QPushButton*>()) {
            if (button->text() == "Buscar")
                QTest::mouseClick(button, Qt::LeftButton);
        }
        QCOMPARE(table->item(1, 15)->text(), QString("--"));
        QCOMPARE(table->item(1, 15)->background().color(), table->item(1, 1)->background().color());
    }

    void faultPanelShowsTypedDecodeDetails() {
        Machine machine(64);
        const std::vector<std::uint8_t> bytes{0xFF};
        machine.loadProgram(bytes, 7, 7);
        machine.step();
        FaultPanel panel;
        panel.updateFault(machine.snapshot().lastFault);
        const QString text = panel.findChild<QTextEdit*>()->toPlainText();
        QVERIFY(text.contains("UnknownOpcode"));
        QVERIFY(text.contains("FF"));
        QVERIFY(text.contains("000007"));
    }

    void invalidMachineLoadKeepsDialogOpenAndExplainsField() {
        ProgramLoadDialog dialog;
        dialog.findChild<QTextEdit*>()->setPlainText("01 00 05");
        QPushButton* load = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                load = button;
        }
        QVERIFY(load);
        QTest::mouseClick(load, Qt::LeftButton);
        dialog.handleLoadResult(LoadFault{LoadFaultCode::InitialPcOutOfRange, 0x100000});
        QCOMPARE(dialog.result(), 0);
        QVERIFY(load->isEnabled());
        bool explained = false;
        for (auto* label : dialog.findChildren<QLabel*>()) {
            if (label->text().contains("PC inicial fora"))
                explained = true;
        }
        QVERIFY(explained);
        QTest::mouseClick(load, Qt::LeftButton);
        dialog.handleLoadResult(LoadSuccess{0, 3, 0});
        QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    }

    void controlsTrackReadyRunningFaultAndReset() {
        ExecutionPanel panel;
        QPushButton *step = nullptr, *run = nullptr, *pause = nullptr, *reset = nullptr;
        for (auto* button : panel.findChildren<QPushButton*>()) {
            if (button->text() == "Executar Passo")
                step = button;
            if (button->text() == "Executar")
                run = button;
            if (button->text() == "Pausar")
                pause = button;
            if (button->text() == "Reset")
                reset = button;
        }
        QVERIFY(step && run && pause && reset);
        panel.setExecutionState(true, false, false);
        QVERIFY(step->isEnabled() && run->isEnabled() && reset->isEnabled());
        panel.setExecutionState(true, true, false);
        QVERIFY(pause->isEnabled());
        QVERIFY(!step->isEnabled() && !run->isEnabled());
        panel.setExecutionState(true, false, true);
        QVERIFY(!step->isEnabled() && !run->isEnabled() && reset->isEnabled());
        panel.setExecutionState(false, false, false);
        QVERIFY(!step->isEnabled() && !run->isEnabled() && !reset->isEnabled());
    }

    void windowLoadsStepsAndUpdatesPanels() {
        MainWindow window;
        window.show();
        auto* panel = window.findChild<ExecutionPanel*>();
        QVERIFY(panel);
        QPushButton *load = nullptr, *step = nullptr;
        for (auto* button : panel->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar Programa")
                load = button;
            if (button->text() == "Executar Passo")
                step = button;
        }
        QVERIFY(load && step);
        QTest::mouseClick(load, Qt::LeftButton);
        auto* dialog = window.findChild<ProgramLoadDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTextEdit*>()->setPlainText("01 00 05");
        QPushButton* confirm = nullptr;
        for (auto* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                confirm = button;
        }
        QVERIFY(confirm);
        QTest::mouseClick(confirm, Qt::LeftButton);
        QTRY_VERIFY(step->isEnabled());
        QTest::mouseClick(step, Qt::LeftButton);
        auto* registers = window.findChild<RegisterPanel*>();
        QVERIFY(registers);
        QTRY_VERIFY(registers->findChildren<QLineEdit*>()[0]->text() == "000005");
    }

    void windowStoreUpdatesMemoryAndResetClearsVisibleState() {
        MainWindow window;
        window.show();
        auto* execution = window.findChild<ExecutionPanel*>();
        auto* registers = window.findChild<RegisterPanel*>();
        auto* memory = window.findChild<MemoryPanel*>();
        QVERIFY(execution && registers && memory);
        auto findButton = [execution](const QString& name) -> QPushButton* {
            for (auto* button : execution->findChildren<QPushButton*>()) {
                if (button->text() == name)
                    return button;
            }
            return nullptr;
        };
        auto* load = findButton("Carregar Programa");
        auto* step = findButton("Executar Passo");
        auto* reset = findButton("Reset");
        QVERIFY(load && step && reset);
        QTest::mouseClick(load, Qt::LeftButton);
        auto* dialog = window.findChild<ProgramLoadDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTextEdit*>()->setPlainText("01 00 05 0F 00 20");
        for (auto* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                QTest::mouseClick(button, Qt::LeftButton);
        }
        QTRY_VERIFY(step->isEnabled());
        QTest::mouseClick(step, Qt::LeftButton);
        auto* a = registers->findChildren<QLineEdit*>()[0];
        QTRY_COMPARE(a->text(), QString("000005"));
        QTest::mouseClick(step, Qt::LeftButton);
        auto* table = memory->findChild<QTableWidget*>();
        QVERIFY(table);
        QTRY_COMPARE(table->item(2, 2)->text(), QString("05"));
        QVERIFY(table->item(2, 2)->background().color() != table->item(2, 3)->background().color());
        QTest::mouseClick(reset, Qt::LeftButton);
        QTRY_COMPARE(a->text(), QString("000000"));
        QTRY_COMPARE(table->item(2, 2)->text(), QString("00"));
        QVERIFY(!step->isEnabled());
    }

    void windowFaultDisplaysDetailsUntilReset() {
        MainWindow window;
        window.show();
        auto* execution = window.findChild<ExecutionPanel*>();
        auto* faults = window.findChild<FaultPanel*>();
        QVERIFY(execution && faults);
        auto findButton = [execution](const QString& name) -> QPushButton* {
            for (auto* button : execution->findChildren<QPushButton*>()) {
                if (button->text() == name)
                    return button;
            }
            return nullptr;
        };
        auto* load = findButton("Carregar Programa");
        auto* step = findButton("Executar Passo");
        auto* reset = findButton("Reset");
        QVERIFY(load && step && reset);
        QTest::mouseClick(load, Qt::LeftButton);
        auto* dialog = window.findChild<ProgramLoadDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTextEdit*>()->setPlainText("FF");
        dialog->findChild<QLineEdit*>("loadAddress")->setText("7");
        dialog->findChild<QLineEdit*>("initialPc")->setText("7");
        for (auto* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                QTest::mouseClick(button, Qt::LeftButton);
        }
        QTRY_VERIFY(step->isEnabled());
        QTest::mouseClick(step, Qt::LeftButton);
        auto* faultText = faults->findChild<QTextEdit*>();
        QVERIFY(faultText);
        QTRY_VERIFY(faultText->toPlainText().contains("UnknownOpcode"));
        QVERIFY(faultText->toPlainText().contains("000007"));
        QVERIFY(faultText->toPlainText().contains("FF"));
        QVERIFY(!step->isEnabled());
        QTest::mouseClick(reset, Qt::LeftButton);
        QTRY_VERIFY(faultText->toPlainText().contains("Nenhuma falha"));
    }

    void malformedHexStaysInDialogWithPosition() {
        ProgramLoadDialog dialog;
        dialog.findChild<QTextEdit*>()->setPlainText("01 GG");
        QPushButton* load = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                load = button;
        }
        QVERIFY(load);
        QTest::mouseClick(load, Qt::LeftButton);
        QCOMPARE(dialog.result(), 0);
        bool explained = false;
        for (auto* label : dialog.findChildren<QLabel*>()) {
            if (label->text().contains("posição 3"))
                explained = true;
        }
        QVERIFY(explained);
    }

    void runCanBePausedAndResumedFromWindow() {
        MainWindow window;
        window.show();
        auto* panel = window.findChild<ExecutionPanel*>();
        QVERIFY(panel);
        auto button = [panel](const QString& text) -> QPushButton* {
            for (auto* item : panel->findChildren<QPushButton*>())
                if (item->text() == text)
                    return item;
            return nullptr;
        };
        QVERIFY(button("Carregar Programa") && button("Executar") && button("Pausar"));
        QTest::mouseClick(button("Carregar Programa"), Qt::LeftButton);
        auto* dialog = window.findChild<ProgramLoadDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTextEdit*>()->setPlainText("19 00 01 3F 00 00");
        QPushButton* confirm = nullptr;
        for (auto* item : dialog->findChildren<QPushButton*>())
            if (item->text() == "Carregar")
                confirm = item;
        QVERIFY(confirm);
        QTest::mouseClick(confirm, Qt::LeftButton);
        QTRY_VERIFY(button("Executar")->isEnabled());
        QTest::mouseClick(button("Executar"), Qt::LeftButton);
        QTRY_VERIFY(button("Pausar")->isEnabled());
        QTest::mouseClick(button("Pausar"), Qt::LeftButton);
        QTRY_VERIFY(button("Executar")->isEnabled());
        QTest::mouseClick(button("Executar"), Qt::LeftButton);
        QTRY_VERIFY(button("Pausar")->isEnabled());
        QTest::mouseClick(button("Pausar"), Qt::LeftButton);
        QTRY_VERIFY(button("Executar")->isEnabled());
    }

    void registerHighlightOnlyRepresentsRequestedStep() {
        RegisterPanel panel;
        RegisterSnapshot before;
        before.conditionCode = ConditionCode::Equal;
        panel.updateRegisters(before, false);
        RegisterSnapshot after = before;
        after.a = 5;
        after.pc = 3;
        panel.updateRegisters(after, true);
        auto* a = panel.findChildren<QLineEdit*>()[0];
        QCOMPARE(a->text(), QString("000005"));
        QVERIFY(a->styleSheet().contains("background-color"));
        panel.updateRegisters(before, false);
        QVERIFY(a->styleSheet().isEmpty());
    }

    void windowShowsInitialVmSnapshot() {
        MainWindow window;
        auto* registers = window.findChild<RegisterPanel*>();
        QVERIFY(registers);
        auto* f = registers->findChildren<QLineEdit*>()[6];
        QTRY_COMPARE(f->text(), QString("000000000000"));
    }

    void pendingLoadCannotBeCancelledBeforeResult() {
        ProgramLoadDialog dialog;
        dialog.open();
        dialog.findChild<QTextEdit*>()->setPlainText("01 00 05");
        QPushButton* load = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>())
            if (button->text() == "Carregar")
                load = button;
        QVERIFY(load);
        QTest::mouseClick(load, Qt::LeftButton);
        dialog.reject();
        QVERIFY(dialog.isVisible());
        dialog.handleLoadResult(LoadSuccess{0, 3, 0});
        QVERIFY(!dialog.isVisible());
    }

    void rejectedReloadPreservesVisibleSession() {
        MainWindow window;
        window.show();
        auto* panel = window.findChild<ExecutionPanel*>();
        auto* registers = window.findChild<RegisterPanel*>();
        QVERIFY(panel && registers);
        QPushButton *load = nullptr, *step = nullptr;
        for (auto* button : panel->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar Programa")
                load = button;
            if (button->text() == "Executar Passo")
                step = button;
        }
        QVERIFY(load && step);
        auto confirmDialog = [&window]() -> ProgramLoadDialog* {
            auto* dialog = window.findChild<ProgramLoadDialog*>();
            if (!dialog)
                return nullptr;
            dialog->findChild<QTextEdit*>()->setPlainText("01 00 05");
            return dialog;
        };
        auto clickConfirm = [](ProgramLoadDialog* dialog) {
            for (auto* button : dialog->findChildren<QPushButton*>())
                if (button->text() == "Carregar")
                    QTest::mouseClick(button, Qt::LeftButton);
        };
        QTest::mouseClick(load, Qt::LeftButton);
        auto* first = confirmDialog();
        QVERIFY(first);
        clickConfirm(first);
        QTRY_VERIFY(step->isEnabled());
        QTest::mouseClick(step, Qt::LeftButton);
        auto* a = registers->findChildren<QLineEdit*>()[0];
        QTRY_COMPARE(a->text(), QString("000005"));
        QTest::mouseClick(load, Qt::LeftButton);
        auto* second = confirmDialog();
        QVERIFY(second);
        auto* pc = second->findChild<QLineEdit*>("initialPc");
        QVERIFY(pc);
        pc->setText("100000");
        clickConfirm(second);
        bool error = false;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                     for (auto* label : second->findChildren<QLabel*>())
                                         if (label->text().contains("PC inicial fora"))
                                             error = true;
                                     return error;
                                 })(),
                                 1000);
        QVERIFY(second->isVisible());
        QCOMPARE(a->text(), QString("000005"));
        second->reject();
        QVERIFY(step->isEnabled());
    }

    void windowExplainsLoadAddressAndProgramSizeErrors() {
        MainWindow window;
        window.show();
        auto* execution = window.findChild<ExecutionPanel*>();
        QVERIFY(execution);
        QPushButton* open = nullptr;
        for (auto* button : execution->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar Programa")
                open = button;
        }
        QVERIFY(open);
        QTest::mouseClick(open, Qt::LeftButton);
        auto* dialog = window.findChild<ProgramLoadDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTextEdit*>()->setPlainText("01 00 05");
        auto* address = dialog->findChild<QLineEdit*>("loadAddress");
        QVERIFY(address);
        QPushButton* confirm = nullptr;
        for (auto* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Carregar")
                confirm = button;
        }
        QVERIFY(confirm);
        auto hasError = [dialog](const QString& expected) {
            for (auto* label : dialog->findChildren<QLabel*>()) {
                if (label->text().contains(expected))
                    return true;
            }
            return false;
        };
        address->setText("100000");
        QTest::mouseClick(confirm, Qt::LeftButton);
        QTRY_VERIFY(hasError("Endereço de carga fora"));
        QVERIFY(dialog->isVisible());
        address->setText("FFFFF");
        QTest::mouseClick(confirm, Qt::LeftButton);
        QTRY_VERIFY(hasError("ultrapassa o fim"));
        QVERIFY(dialog->isVisible());
    }
};

QTEST_MAIN(InterfaceTest)
#include "InterfaceTest.moc"
