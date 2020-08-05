#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QDebug>
#include <math.h>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QMetaEnum>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    setting(qAppName()+".ini",QSettings::IniFormat)
{
    ui->setupUi(this);
    ui->boxCom->clear();
    auto ports=QSerialPortInfo::availablePorts();
    foreach(auto& port,ports){
        ui->boxCom->addItem(port.portName());
    }
    {
        defaultCom=getOrSet("DefaultCom","COM1").toString();
        int index=ui->boxCom->findText(defaultCom);
        if(index>=0){
            ui->boxCom->setCurrentIndex(index);
        }
    }
    timer.setInterval(100);
    timer.setSingleShot(false);
    connect(&timer,&QTimer::timeout,this,&MainWindow::onTimer);
    timer.start();
    connect(&bmodem,&BModem::txDataRequest,this,[=](QByteArray data){
        if(serialport.isOpen()){
            //qDebug()<<__FUNCTION__<<data.toHex();
            serialport.write(data);
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

QMetaObject::Connection conn;
void MainWindow::on_btnOpen_clicked(){
    if(serialport.isOpen()==false){
        serialport.setPortName(ui->boxCom->currentText());
        serialport.setBaudRate(115200);
        int party=getOrSet("Party",0).toInt();
        if(party!=0 && party!=2 && party!=3){
            party=0;
            setting.setValue("Party",0);
            setting.sync();
        }
        serialport.setParity(static_cast<QSerialPort::Parity>(party));
        serialport.open(QSerialPort::ReadWrite);
        if(serialport.isOpen()){
            conn=connect(&serialport,&QSerialPort::readyRead,[this](){
                QByteArray data=serialport.readAll();
                //qDebug()<<__FUNCTION__<<buf.toHex();
                bmodem.onRxDataReady(data);
                if(data.size()==1 && QChar(data.at(0)).isPrint()==false){
                    if(data.at(0)==0x15){
                        ui->debugOut->appendPlainText("HEX:0x15=NAK");
                    }else if(data.at(0)==0x18){
                        ui->debugOut->appendPlainText("HEX:0x18=CANCEL");
                    }else if(data.at(0)==0x06){
                        ui->debugOut->appendPlainText("HEX:0x06=ACK");
                    }else if(data.at(0)==0x01){
                        ui->debugOut->appendPlainText("HEX:0x01=START OF HEAD");
                    }else{
                        ui->debugOut->appendPlainText(QString("UNKNOW DEC:%1").arg(data.at(0)));
                    }
                }else{
                    ui->debugOut->appendPlainText(QString(data).trimmed());
                }
            });
        }
    }else{
        serialport.close();
        disconnect(conn);
    }
    if(serialport.isOpen()){
        ui->btnOpen->setText("Close");
    }else{
        ui->btnOpen->setText("Open");
    }
    onTimer();
}

void MainWindow::onTimer(){
    if(bmodem.isIdle() && bmodem.packetNum>0 && serialport.isOpen()){
        ui->btnDownload->setEnabled(true);
        ui->btnOpen->setEnabled(true);
        ui->btnLoad->setEnabled(true);
    }else{
        ui->btnDownload->setEnabled(false);
    }
    ui->labelError->setText(QString("%1").arg(bmodem.errorCount));
    ui->labelResult->setText(QMetaEnum::fromType<BModem::State>().valueToKeys(static_cast<int>(bmodem.state)));
    ui->progressBar->setValue(bmodem.txPacket);
}

void MainWindow::on_btnLoad_clicked(){
    if(bmodem.isIdle()){
        QString fn=QFileDialog::getOpenFileName(0,"Load Firmware",QString(),QStringLiteral("*.bin"));
        if(fn.isEmpty()==false){
            QFile file(fn);
            file.open(QFile::ReadOnly);
            if(file.isOpen()){
                bmodem.loadData(file.readAll());
                ui->btnLoad->setText("Loaded");
                ui->progressBar->setMaximum(bmodem.packetNum);
                ui->progressBar->setValue(0);
            }
        }
    }
}

void MainWindow::on_btnDownload_clicked(){
    if(bmodem.isIdle() && bmodem.packetNum>0){
        {
            if(defaultCom != ui->boxCom->currentText()){
                defaultCom = ui->boxCom->currentText();
                setting.setValue("DefaultCom",defaultCom);
                setting.sync();
            }
        }
        QByteArray rebootCommand=getOrSet("RebootCommand","FEFBDB0200EB000055AA").toByteArray();
        serialport.write(QByteArray::fromHex(rebootCommand));
        bmodem.start();
        ui->btnDownload->setEnabled(false);
        ui->btnLoad->setEnabled(false);
        ui->btnOpen->setEnabled(false);
    }
}

QVariant MainWindow::getOrSet(QString key, QVariant defaultValue){
    QVariant value=setting.value(key);
    if(value.isNull()){
        setting.setValue(key,defaultValue);
        setting.sync();
        value=defaultValue;
    }
    return value;
}
