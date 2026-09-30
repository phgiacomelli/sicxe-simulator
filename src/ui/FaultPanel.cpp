#include "FaultPanel.hpp"

#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>
#include <variant>

namespace {

QString codeName(DecodeFaultCode code) {
    switch (code) {
        case DecodeFaultCode::UnknownOpcode:
            return "UnknownOpcode";
        case DecodeFaultCode::TruncatedInstruction:
            return "TruncatedInstruction";
        case DecodeFaultCode::InvalidRegister:
            return "InvalidRegister";
        case DecodeFaultCode::InvalidFlagCombination:
            return "InvalidFlagCombination";
        case DecodeFaultCode::InvalidOperandEncoding:
            return "InvalidOperandEncoding";
    }
    return "DecodeFault";
}

QString codeName(ResolveFaultCode code) {
    switch (code) {
        case ResolveFaultCode::InvalidInstruction:
            return "InvalidInstruction";
        case ResolveFaultCode::InvalidRegister:
            return "InvalidRegister";
        case ResolveFaultCode::AddressOutOfRange:
            return "AddressOutOfRange";
        case ResolveFaultCode::IndirectReadOutOfRange:
            return "IndirectReadOutOfRange";
    }
    return "ResolveFault";
}

QString codeName(ExecuteFaultCode code) {
    switch (code) {
        case ExecuteFaultCode::UnsupportedOpcode:
            return "UnsupportedOpcode";
        case ExecuteFaultCode::InvalidOperand:
            return "InvalidOperand";
        case ExecuteFaultCode::DivisionByZero:
            return "DivisionByZero";
        case ExecuteFaultCode::MemoryAccessOutOfRange:
            return "MemoryAccessOutOfRange";
    }
    return "ExecuteFault";
}

QString codeName(CycleFaultCode code) {
    switch (code) {
        case CycleFaultCode::PostFetchPcOutOfRange:
            return "PostFetchPcOutOfRange";
        case CycleFaultCode::NextPcOutOfRange:
            return "NextPcOutOfRange";
        case CycleFaultCode::InvalidRegisterWrite:
            return "InvalidRegisterWrite";
        case CycleFaultCode::InvalidConditionCode:
            return "InvalidConditionCode";
        case CycleFaultCode::MemoryWriteOutOfRange:
            return "MemoryWriteOutOfRange";
        case CycleFaultCode::ConflictingEffect:
            return "ConflictingEffect";
    }
    return "CycleFault";
}

} // namespace

FaultPanel::FaultPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    textDisplay_ = new QTextEdit(this);
    textDisplay_->setReadOnly(true);

    QFont font("Consolas", 10);
    textDisplay_->setFont(font);

    layout->addWidget(textDisplay_);

    // Estado inicial
    updateFault(std::nullopt);
}

QString FaultPanel::formatFault(const MachineFault& fault) const {
    QString msg = QString("Falha da VM — PC: %1\n")
                      .arg(fault.instructionAddress, 6, 16, QChar('0'))
                      .toUpper();
    std::visit(
        [&msg](const auto& cause) {
            msg += QString("Código: %1\n").arg(codeName(cause.code));
            using Cause = std::decay_t<decltype(cause)>;
            if constexpr (std::is_same_v<Cause, DecodeFault>) {
                QStringList bytes;
                for (std::uint8_t i = 0; i < cause.bytesRead; ++i)
                    bytes << QString("%1").arg(cause.rawBytes[i], 2, 16, QChar('0')).toUpper();
                msg += QString("Bytes lidos: %1\n").arg(bytes.join(' '));
                msg += QString("Endereço de decodificação: %1\n")
                           .arg(cause.instructionAddress, 6, 16, QChar('0'))
                           .toUpper();
            } else {
                if constexpr (std::is_same_v<Cause, ExecuteFault>) {
                    msg += QString("Endereço de execução: %1\n")
                               .arg(cause.instructionAddress, 6, 16, QChar('0'))
                               .toUpper();
                }
                if (cause.offendingValue)
                    msg += QString("Valor associado: %1\n").arg(*cause.offendingValue);
            }
        },
        fault.cause);
    return msg;
}

void FaultPanel::updateFault(const std::optional<MachineFault>& fault) {
    if (!fault.has_value()) {
        textDisplay_->setStyleSheet(
            "QTextEdit { color: #2E8B57; font-weight: bold; }"); // Verde escuro
        textDisplay_->setText("Status: Execucao Normal. Nenhuma falha detectada.");
    } else {
        textDisplay_->setStyleSheet("QTextEdit { color: #DC143C; font-weight: bold; }"); // Vermelho
        textDisplay_->setText(formatFault(*fault));
    }
}
