#include "RegisterPanel.hpp"

#include <QFont>
#include <QFormLayout>
#include <QLineEdit>
#include <QString>

RegisterPanel::RegisterPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QFormLayout(this);

    lineA_ = createDisplayField();
    lineX_ = createDisplayField();
    lineL_ = createDisplayField();
    lineB_ = createDisplayField();
    lineS_ = createDisplayField();
    lineT_ = createDisplayField();
    lineF_ = createDisplayField();
    linePC_ = createDisplayField();
    lineSW_ = createDisplayField();
    lineCC_ = createDisplayField();

    layout->addRow("A:", lineA_);
    layout->addRow("X:", lineX_);
    layout->addRow("L:", lineL_);
    layout->addRow("B:", lineB_);
    layout->addRow("S:", lineS_);
    layout->addRow("T:", lineT_);
    layout->addRow("F:", lineF_);
    layout->addRow("PC:", linePC_);
    layout->addRow("SW:", lineSW_);
    layout->addRow("CC:", lineCC_);
}

QLineEdit* RegisterPanel::createDisplayField() {
    auto* field = new QLineEdit("000000", this);
    field->setReadOnly(true);
    field->setAlignment(Qt::AlignRight);

    QFont font("Consolas", 10);
    font.setStyleHint(QFont::Monospace);
    field->setFont(font);

    return field;
}

void RegisterPanel::updateRegisters(const RegisterSnapshot& snapshot, bool highlightChanges) {
    bool hasLast = highlightChanges && lastSnapshot_.has_value();

    // Função auxiliar para atualizar e destacar o campo se houver mudança
    auto updateField = [hasLast](QLineEdit* field, auto newVal, auto oldVal, int width) {
        field->setText(QString("%1").arg(newVal, width, 16, QChar('0')).toUpper());
        if (hasLast && newVal != oldVal) {
            // O border força o Qt a desativar o tema do Windows e a aplicar as nossas cores
            field->setStyleSheet("color: #FFD700; font-weight: bold; background-color: #444; "
                                 "border: 1px solid #FFD700; border-radius: 2px;");
        } else {
            // Limpa o estilo se o valor não mudou neste passo
            field->setStyleSheet("");
        }
    };

    updateField(lineA_, snapshot.a, hasLast ? lastSnapshot_->a : snapshot.a, 6);
    updateField(lineX_, snapshot.x, hasLast ? lastSnapshot_->x : snapshot.x, 6);
    updateField(lineL_, snapshot.l, hasLast ? lastSnapshot_->l : snapshot.l, 6);
    updateField(lineB_, snapshot.b, hasLast ? lastSnapshot_->b : snapshot.b, 6);
    updateField(lineS_, snapshot.s, hasLast ? lastSnapshot_->s : snapshot.s, 6);
    updateField(lineT_, snapshot.t, hasLast ? lastSnapshot_->t : snapshot.t, 6);
    updateField(lineF_, snapshot.f, hasLast ? lastSnapshot_->f : snapshot.f, 12);
    updateField(linePC_, snapshot.pc, hasLast ? lastSnapshot_->pc : snapshot.pc, 6);
    updateField(lineSW_, snapshot.sw, hasLast ? lastSnapshot_->sw : snapshot.sw, 6);

    // Tratamento especial para o Condition Code (Enum)
    QString ccText = "-";
    if (snapshot.conditionCode.has_value()) {
        switch (snapshot.conditionCode.value()) {
            case ConditionCode::Equal:
                ccText = "Equal";
                break;
            case ConditionCode::Less:
                ccText = "Less";
                break;
            case ConditionCode::Greater:
                ccText = "Greater";
                break;
        }
    }
    lineCC_->setText(ccText);

    bool ccChanged = hasLast && (lastSnapshot_->conditionCode != snapshot.conditionCode);
    if (ccChanged) {
        lineCC_->setStyleSheet("color: #FFD700; font-weight: bold; background-color: #555;");
    } else {
        lineCC_->setStyleSheet("");
    }

    // Guarda o snapshot atual para a próxima comparação
    lastSnapshot_ = snapshot;
}
