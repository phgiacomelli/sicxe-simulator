#pragma once

#include <QMainWindow>

#include "core/Machine.hpp"

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    Machine machine_;
};