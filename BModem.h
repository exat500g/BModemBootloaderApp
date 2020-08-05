#pragma once

#include <QtGlobal>
#include <QObject>
#include <QTimer>

class BModem : public QObject{
    Q_OBJECT
public:
    enum class State{
        IDLE,
        STARTING,
        DOWNLOADING,
        FINISHING,
        SUCCESS,
        FAILED,
    };
    Q_ENUM(State)
public:
    QByteArray fileData;
    uint32_t packetNum=0;
    uint32_t txPacket=0;
    uint8_t  txCounter=0;
    uint32_t errorCount=0;
    State    state=State::IDLE;
    QTimer   timer;

public:
    BModem();
    bool loadData(QByteArray data);
    bool start();
    bool isIdle();

signals:
    void txDataRequest(QByteArray);
public slots:
    void onRxDataReady(QByteArray data);
private:
    void sendPacket(uint8_t type,uint8_t counter,QByteArray data);
    Q_INVOKABLE void onTimeout();
};
