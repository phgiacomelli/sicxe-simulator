#pragma once
#include "core/Machine.hpp"

#include <QWidget>
#include <cstdint>
#include <vector>

class QTableWidget;
class QLineEdit;
class QSpinBox;
class QLabel;

class MemoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit MemoryPanel(QWidget* parent = nullptr);
    void updateMemory(const std::vector<uint8_t>& memory,
                      const std::vector<std::uint32_t>& changedAddresses = {});

private slots:
    void onSearchClicked();

private:
    QTableWidget* table_;
    QLineEdit* lineAddress_;
    QSpinBox* rangeLength_;
    QLabel* errorLabel_;
    std::vector<uint8_t> currentMemory_;
    std::vector<std::uint32_t> changedAddresses_;
    uint32_t currentBaseAddress_ = 0;

    void refreshTable();
};
