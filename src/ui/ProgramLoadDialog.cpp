#include "ProgramLoadDialog.hpp"

#include "InputFaultText.hpp"
#include "application/HexParser.hpp"
#include "application/RawBinaryReader.hpp"

#include <QButtonGroup>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTextEdit>
#include <QVBoxLayout>

ProgramLoadDialog::ProgramLoadDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Carregar Programa");
    resize(500, 400);

    radioHex_ = new QRadioButton("Texto Hexadecimal", this);
    radioBin_ = new QRadioButton("Arquivo Binario", this);
    radioHex_->setChecked(true);

    auto* typeGroup = new QButtonGroup(this);
    typeGroup->addButton(radioHex_);
    typeGroup->addButton(radioBin_);

    textHexInput_ = new QTextEdit(this);
    lineBinPath_ = new QLineEdit(this);
    lineBinPath_->setObjectName("binaryPath");
    lineBinPath_->setEnabled(false);

    btnBrowse_ = new QPushButton("Procurar...", this);
    btnBrowse_->setEnabled(false);

    auto* binLayout = new QHBoxLayout;
    binLayout->addWidget(lineBinPath_);
    binLayout->addWidget(btnBrowse_);

    lineLoadAddress_ = new QLineEdit("0", this);
    lineLoadAddress_->setObjectName("loadAddress");
    lineInitialPc_ = new QLineEdit("0", this);
    lineInitialPc_->setObjectName("initialPc");

    labelError_ = new QLabel(this);
    labelError_->setStyleSheet("color: red; font-weight: bold;");
    labelBinError_ = new QLabel(this);
    labelBinError_->setStyleSheet("color: red;");
    labelLoadError_ = new QLabel(this);
    labelPcError_ = new QLabel(this);
    labelLoadError_->setStyleSheet("color: red;");
    labelPcError_->setStyleSheet("color: red;");

    btnOk_ = new QPushButton("Carregar", this);
    btnCancel_ = new QPushButton("Cancelar", this);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk_);
    btnLayout->addWidget(btnCancel_);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(radioHex_);
    mainLayout->addWidget(textHexInput_);
    mainLayout->addWidget(labelError_);
    mainLayout->addWidget(radioBin_);
    mainLayout->addLayout(binLayout);
    mainLayout->addWidget(labelBinError_);

    auto* formLayout = new QFormLayout;
    formLayout->addRow("Endereço de Carga (Hex):", lineLoadAddress_);
    formLayout->addRow(QString(), labelLoadError_);
    formLayout->addRow("PC Inicial (Hex):", lineInitialPc_);
    formLayout->addRow(QString(), labelPcError_);
    mainLayout->addLayout(formLayout);

    mainLayout->addLayout(btnLayout);

    connect(radioHex_, &QRadioButton::toggled, this, &ProgramLoadDialog::onInputTypeChanged);
    connect(btnBrowse_, &QPushButton::clicked, this, &ProgramLoadDialog::onBrowseClicked);
    connect(btnOk_, &QPushButton::clicked, this, &ProgramLoadDialog::onAcceptClicked);
    connect(btnCancel_, &QPushButton::clicked, this, &ProgramLoadDialog::reject);
}

void ProgramLoadDialog::onInputTypeChanged() {
    bool isHex = radioHex_->isChecked();
    textHexInput_->setEnabled(isHex);
    lineBinPath_->setEnabled(!isHex);
    btnBrowse_->setEnabled(!isHex);
}

void ProgramLoadDialog::onBrowseClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Selecionar Arquivo Binario");
    if (!path.isEmpty()) {
        lineBinPath_->setText(path);
    }
}

void ProgramLoadDialog::onAcceptClicked() {
    labelError_->clear();
    labelBinError_->clear();
    labelLoadError_->clear();
    labelPcError_->clear();
    if (pending_)
        return;
    bool loadOk, pcOk;
    uint32_t loadAddress = lineLoadAddress_->text().toUInt(&loadOk, 16);
    uint32_t initialPc = lineInitialPc_->text().toUInt(&pcOk, 16);

    if (!loadOk || !pcOk) {
        if (!loadOk)
            labelLoadError_->setText("Endereço de carga inválido.");
        if (!pcOk)
            labelPcError_->setText("PC inicial inválido.");
        return;
    }

    ProgramInputResult result;
    if (radioHex_->isChecked()) {
        result =
            HexParser::parse(textHexInput_->toPlainText().toStdString(), loadAddress, initialPc);
    } else {
        if (lineBinPath_->text().isEmpty()) {
            labelBinError_->setText("Selecione um arquivo binário.");
            return;
        }
        result = RawBinaryReader::read(lineBinPath_->text().toStdString(), loadAddress, initialPc);
    }

    if (auto* fault = std::get_if<InputFault>(&result)) {
        displayError(*fault);
        return;
    }

    pending_ = true;
    btnOk_->setEnabled(false);
    btnCancel_->setEnabled(false);
    emit programReady(std::get<ProgramInput>(result));
}

void ProgramLoadDialog::reject() {
    if (!pending_)
        QDialog::reject();
}

void ProgramLoadDialog::handleLoadResult(const LoadResult& result) {
    if (!pending_)
        return;
    pending_ = false;
    btnOk_->setEnabled(true);
    btnCancel_->setEnabled(true);
    if (const auto* fault = std::get_if<LoadFault>(&result)) {
        QString message;
        switch (fault->code) {
            case LoadFaultCode::EmptyProgram:
                labelError_->setText("Programa vazio.");
                return;
            case LoadFaultCode::LoadAddressOutOfRange:
                message = "Endereço de carga fora da memória.";
                labelLoadError_->setText(message);
                return;
            case LoadFaultCode::ProgramExceedsMemory:
                labelLoadError_->setText("Programa ultrapassa o fim da memória.");
                return;
            case LoadFaultCode::InitialPcOutOfRange:
                labelPcError_->setText("PC inicial fora da memória ou da arquitetura.");
                return;
        }
    }
    accept();
}

void ProgramLoadDialog::displayError(const InputFault& fault) {
    const bool fileError =
        fault.code == InputFaultCode::OpenFailed || fault.code == InputFaultCode::ReadFailed;
    (fileError ? labelBinError_ : labelError_)->setText(inputFaultMessage(fault));
}
