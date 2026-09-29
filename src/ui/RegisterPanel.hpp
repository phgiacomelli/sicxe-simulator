#pragma once
#include "core/Machine.hpp"

#include <QWidget>
#include <optional>

class QLineEdit;

class RegisterPanel : public QWidget {
    Q_OBJECT
public:
    explicit RegisterPanel(QWidget* parent = nullptr);
    void updateRegisters(const RegisterSnapshot& snapshot, bool highlightChanges = true);

private:
    QLineEdit* lineA_;
    QLineEdit* lineX_;
    QLineEdit* lineL_;
    QLineEdit* lineB_;
    QLineEdit* lineS_;
    QLineEdit* lineT_;
    QLineEdit* lineF_;
    QLineEdit* linePC_;
    QLineEdit* lineSW_;
    QLineEdit* lineCC_;

    // Variável para guardar o estado anterior e comparar as mudanças
    std::optional<RegisterSnapshot> lastSnapshot_;

    QLineEdit* createDisplayField();
};
