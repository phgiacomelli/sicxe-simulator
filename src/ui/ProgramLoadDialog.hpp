#pragma once
#include "application/ProgramInput.hpp"
#include "core/Machine.hpp"

#include <QDialog>

class QRadioButton;
class QTextEdit;
class QLineEdit;
class QLabel;
class QPushButton;

class ProgramLoadDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgramLoadDialog(QWidget* parent = nullptr);

public slots:
    void handleLoadResult(const LoadResult& result);
    void reject() override;

signals:
    void programReady(ProgramInput program);

private slots:
    void onInputTypeChanged();
    void onBrowseClicked();
    void onAcceptClicked();

private:
    QRadioButton* radioHex_;
    QRadioButton* radioBin_;
    QTextEdit* textHexInput_;
    QLineEdit* lineBinPath_;
    QLineEdit* lineLoadAddress_;
    QLineEdit* lineInitialPc_;
    QLabel* labelError_;
    QLabel* labelBinError_;
    QLabel* labelLoadError_;
    QLabel* labelPcError_;
    QPushButton* btnBrowse_;
    QPushButton* btnOk_;
    QPushButton* btnCancel_;
    bool pending_ = false;

    void displayError(const InputFault& fault);
};
