#pragma once

#include "core/Machine.hpp"

#include <QMainWindow>

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    Machine machine_;
};