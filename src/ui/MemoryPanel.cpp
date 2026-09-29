#include "MemoryPanel.hpp"

#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

MemoryPanel::MemoryPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* topLayout = new QHBoxLayout;
    topLayout->addWidget(new QLabel("Ir para Endereço (Hex):", this));
    lineAddress_ = new QLineEdit("000000", this);
    rangeLength_ = new QSpinBox(this);
    rangeLength_->setRange(1, 4096);
    rangeLength_->setValue(256);
    rangeLength_->setSuffix(" bytes");
    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet("color: red;");
    auto* btnSearch = new QPushButton("Buscar", this);
    topLayout->addWidget(lineAddress_);
    topLayout->addWidget(new QLabel("Faixa:", this));
    topLayout->addWidget(rangeLength_);
    topLayout->addWidget(btnSearch);
    topLayout->addStretch();

    table_ = new QTableWidget(16, 16, this);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); // Apenas leitura

    QFont monoFont("Consolas", 10);
    monoFont.setStyleHint(QFont::Monospace);
    table_->setFont(monoFont);

    QStringList headers;
    for (int i = 0; i < 16; ++i) {
        headers << QString::asprintf("%X", i);
    }
    table_->setHorizontalHeaderLabels(headers);

    layout->addLayout(topLayout);
    layout->addWidget(errorLabel_);
    layout->addWidget(table_);

    connect(btnSearch, &QPushButton::clicked, this, &MemoryPanel::onSearchClicked);
    connect(lineAddress_, &QLineEdit::returnPressed, this, &MemoryPanel::onSearchClicked);
    connect(rangeLength_, &QSpinBox::valueChanged, this, [this] { refreshTable(); });
}

void MemoryPanel::updateMemory(const std::vector<uint8_t>& memory,
                               const std::vector<std::uint32_t>& changedAddresses) {
    currentMemory_ = memory;
    changedAddresses_ = changedAddresses;
    refreshTable();
}

void MemoryPanel::onSearchClicked() {
    bool ok;
    uint32_t addr = lineAddress_->text().toUInt(&ok, 16);
    if (!ok || addr >= currentMemory_.size()) {
        errorLabel_->setText("Endereço fora da memória ou inválido.");
        return;
    }
    errorLabel_->clear();
    currentBaseAddress_ = addr;
    refreshTable();
}

void MemoryPanel::refreshTable() {
    if (currentMemory_.empty())
        return;

    const int rows = (rangeLength_->value() + 15) / 16;
    table_->setRowCount(rows);
    QStringList vHeaders;
    for (int row = 0; row < rows; ++row) {
        const auto rowAddress = static_cast<std::uint64_t>(currentBaseAddress_) + row * 16;
        vHeaders << QString::asprintf("%06llX", static_cast<unsigned long long>(rowAddress));

        for (int col = 0; col < 16; ++col) {
            const auto addr = rowAddress + col;
            const bool inRange = row * 16 + col < rangeLength_->value();
            QString val = "--";
            if (inRange && addr < currentMemory_.size()) {
                val = QString::asprintf("%02X", currentMemory_[addr]);
            }
            auto* item = new QTableWidgetItem(val);
            item->setTextAlignment(Qt::AlignCenter);
            if (inRange && addr < currentMemory_.size() &&
                std::find(changedAddresses_.begin(), changedAddresses_.end(), addr) !=
                    changedAddresses_.end()) {
                item->setBackground(QColor("#FFE08A"));
                item->setForeground(QColor("#101010"));
            }
            table_->setItem(row, col, item);
        }
    }
    table_->setVerticalHeaderLabels(vHeaders);
}
