#include "ExecutionPanel.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <variant>

// Helper para converter o enum Opcode num texto legível
QString opcodeToString(Opcode op) {
    switch (op) {
        case Opcode::ADD:
            return "ADD";
        case Opcode::ADDR:
            return "ADDR";
        case Opcode::AND:
            return "AND";
        case Opcode::CLEAR:
            return "CLEAR";
        case Opcode::COMP:
            return "COMP";
        case Opcode::COMPR:
            return "COMPR";
        case Opcode::DIV:
            return "DIV";
        case Opcode::DIVR:
            return "DIVR";
        case Opcode::J:
            return "J";
        case Opcode::JEQ:
            return "JEQ";
        case Opcode::JGT:
            return "JGT";
        case Opcode::JLT:
            return "JLT";
        case Opcode::JSUB:
            return "JSUB";
        case Opcode::LDA:
            return "LDA";
        case Opcode::LDB:
            return "LDB";
        case Opcode::LDCH:
            return "LDCH";
        case Opcode::LDL:
            return "LDL";
        case Opcode::LDS:
            return "LDS";
        case Opcode::LDT:
            return "LDT";
        case Opcode::LDX:
            return "LDX";
        case Opcode::MUL:
            return "MUL";
        case Opcode::MULR:
            return "MULR";
        case Opcode::OR:
            return "OR";
        case Opcode::RMO:
            return "RMO";
        case Opcode::RSUB:
            return "RSUB";
        case Opcode::SHIFTL:
            return "SHIFTL";
        case Opcode::SHIFTR:
            return "SHIFTR";
        case Opcode::STA:
            return "STA";
        case Opcode::STB:
            return "STB";
        case Opcode::STCH:
            return "STCH";
        case Opcode::STL:
            return "STL";
        case Opcode::STS:
            return "STS";
        case Opcode::STT:
            return "STT";
        case Opcode::STX:
            return "STX";
        case Opcode::SUB:
            return "SUB";
        case Opcode::SUBR:
            return "SUBR";
        case Opcode::TIX:
            return "TIX";
        case Opcode::TIXR:
            return "TIXR";
        default:
            return "UNKNOWN";
    }
}

ExecutionPanel::ExecutionPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    btnLoad_ = new QPushButton("Carregar Programa", this);
    btnStep_ = new QPushButton("Executar Passo", this);
    btnRun_ = new QPushButton("Executar", this);
    btnPause_ = new QPushButton("Pausar", this);
    btnReset_ = new QPushButton("Reset", this);

    btnPause_->setEnabled(false);
    btnStep_->setEnabled(false);
    btnRun_->setEnabled(false);
    btnReset_->setEnabled(false);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnStep_);
    btnLayout->addWidget(btnRun_);
    btnLayout->addWidget(btnPause_);
    btnLayout->addWidget(btnReset_);

    // Cria e estiliza o campo da instrução atual
    labelInstruction_ = new QLabel("Última Instrução: Nenhuma", this);
    labelInstruction_->setStyleSheet("font-weight: bold; color: #E0E0E0; background-color: #333; "
                                     "padding: 5px; border-radius: 3px;");
    labelInstruction_->setAlignment(Qt::AlignCenter);
    labelCurrent_ = new QLabel("Instrução atual: Nenhuma", this);
    labelCurrent_->setAlignment(Qt::AlignCenter);
    labelStatus_ = new QLabel("Estado: Sem programa", this);

    layout->addWidget(btnLoad_);
    layout->addLayout(btnLayout);
    layout->addWidget(labelStatus_);
    layout->addWidget(labelCurrent_);
    layout->addWidget(labelInstruction_);
    layout->addStretch();

    connect(btnLoad_, &QPushButton::clicked, this, &ExecutionPanel::loadRequested);

    connect(btnStep_, &QPushButton::clicked, this, &ExecutionPanel::stepRequested);
    connect(btnRun_, &QPushButton::clicked, this, &ExecutionPanel::runRequested);
    connect(btnPause_, &QPushButton::clicked, this, &ExecutionPanel::pauseRequested);
    connect(btnReset_, &QPushButton::clicked, this, &ExecutionPanel::resetRequested);
}

void ExecutionPanel::setExecutionState(bool loaded, bool running, bool faulted) {
    btnLoad_->setEnabled(!running);
    btnStep_->setEnabled(loaded && !running && !faulted);
    btnRun_->setEnabled(loaded && !running && !faulted);
    btnReset_->setEnabled(loaded && !running);
    btnPause_->setEnabled(running);
    labelStatus_->setText(!loaded   ? "Estado: Sem programa"
                          : faulted ? "Estado: Falha"
                          : running ? "Estado: Executando"
                                    : "Estado: Pronto");
}

void ExecutionPanel::showCurrentInstruction(const std::optional<DecodedInstruction>& instruction) {
    if (!instruction) {
        labelCurrent_->setText("Instrução atual: Não decodificável");
        return;
    }
    QStringList bytes;
    for (std::uint8_t index = 0; index < instruction->length; ++index) {
        bytes << QString("%1").arg(instruction->rawBytes[index], 2, 16, QChar('0')).toUpper();
    }
    labelCurrent_->setText(QString("Instrução atual: %1 em %2 — %3")
                               .arg(opcodeToString(instruction->opcode))
                               .arg(instruction->instructionAddress, 6, 16, QChar('0'))
                               .arg(bytes.join(' ')));
}

void ExecutionPanel::updateInstruction(const StepResult& result) {
    if (const auto* success = std::get_if<StepSuccess>(&result)) {
        QString opName = opcodeToString(success->instruction.opcode);
        QString addr = QString::asprintf("%06X", success->instruction.instructionAddress);
        labelInstruction_->setText(
            QString("Última Instrução: %1 (no endereço %2)").arg(opName, addr));
        labelInstruction_->setStyleSheet("font-weight: bold; color: #87CEFA; background-color: "
                                         "#333; padding: 5px; border-radius: 3px;"); // Azul claro
    } else {
        labelInstruction_->setText("Última Instrução: FALHA");
        labelInstruction_->setStyleSheet("font-weight: bold; color: #DC143C; background-color: "
                                         "#333; padding: 5px; border-radius: 3px;"); // Vermelho
    }
}

void ExecutionPanel::clearInstruction() {
    labelInstruction_->setText("Última Instrução: Nenhuma");
    labelCurrent_->setText("Instrução atual: Nenhuma");
    labelInstruction_->setStyleSheet("font-weight: bold; color: #E0E0E0; background-color: #333; "
                                     "padding: 5px; border-radius: 3px;");
}
