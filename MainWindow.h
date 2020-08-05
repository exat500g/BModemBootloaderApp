#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "BModem.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void on_btnOpen_clicked();
    void onTimer();
    void on_btnLoad_clicked();
    void on_btnDownload_clicked();
    QVariant getOrSet(QString key,QVariant defaultValue);
private:
    BModem bmodem;
    QTimer timer;
    Ui::MainWindow *ui;
    QMessageBox msgBox;
    QSettings setting;
    QSerialPort serialport;
    QString defaultCom;
};

#endif // MAINWINDOW_H
