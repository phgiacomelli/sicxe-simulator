#include "MainWindow.hpp"

#include <QLabel>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("SIC/XE Simulator");
    resize(1000, 700);

    auto* label = new QLabel("SIC/XE Simulator", this);

    label->setAlignment(Qt::AlignCenter);

    setCentralWidget(label);
}