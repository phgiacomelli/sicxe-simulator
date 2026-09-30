#pragma once
#include "core/Machine.hpp"

#include <QWidget>
#include <optional>

class QTextEdit;

class FaultPanel : public QWidget {
    Q_OBJECT
public:
    explicit FaultPanel(QWidget* parent = nullptr);

    // Atualiza o painel recebendo a falha (ou ausência dela) do último snapshot
    void updateFault(const std::optional<MachineFault>& fault);

private:
    QTextEdit* textDisplay_;
    QString formatFault(const MachineFault& fault) const;
};