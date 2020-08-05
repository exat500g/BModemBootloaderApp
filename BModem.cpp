#include "BModem.h"
#include <QDebug>

#if 1

static uint16_t updateCRC16(uint16_t crcIn, uint8_t byte) {
    uint32_t crc = crcIn;
    uint32_t in = byte | 0x100;
    do {
        crc <<= 1;
        in <<= 1;
        if(in & 0x100)
            ++crc;
        if(crc & 0x10000)
            crc ^= 0x1021;
    }
    while(!(in & 0x10000));
    return crc & 0xffffu;
}

static uint16_t calcCRC16(const uint8_t* data, uint32_t size) {
    uint32_t crc = 0;
    const uint8_t* dataEnd = data+size;
    while(data < dataEnd) {
        crc = updateCRC16(crc, *data++);
    }
    crc = updateCRC16(crc, 0);
    crc = updateCRC16(crc, 0);
    return crc&0xffffu;
}

static const uint8_t SOH=0x01; /* start of 128-byte data packet */
static const uint8_t EOT=0x04; /* end of transmission */
static const uint8_t ACK=0x06; /* acknowledge */
static const uint8_t NAK=0x15; /* negative acknowledge */
static const uint8_t CAN=0x18; // cancel
static const uint8_t PKG_SIZE=0x80;
static const uint8_t MAX_ERROR = 10;

BModem::BModem(){
    timer.setInterval(1000);
    connect(&timer,&QTimer::timeout,this,&BModem::onTimeout);
    timer.setSingleShot(false);
    state=State::IDLE;
}

bool BModem::loadData(QByteArray data){
    if(state==State::IDLE || state==State::SUCCESS || state==State::FAILED){
        fileData=data;
        packetNum=fileData.size()/PKG_SIZE;
        uint32_t mod=fileData.size()%PKG_SIZE;
        if(mod > 0){
            packetNum++;
            fileData.append(PKG_SIZE-mod,0);
        }
        txPacket=0;
        return true;
    }
    return false;
}

bool BModem::start(){
    if(state==State::IDLE || state==State::SUCCESS || state==State::FAILED){
        txPacket=0;
        txCounter=0;
        errorCount=0;
        state=State::STARTING;
        timer.start();
        return true;
    }
    return false;
}

bool BModem::isIdle(){
    if(state==State::IDLE || state==State::SUCCESS || state==State::FAILED){
        return true;
    }
    return false;
}

void BModem::onRxDataReady(QByteArray data){
    if(data.size()!=1 || state==State::IDLE){
        qDebug()<<__FUNCTION__<<"ignore message:"<<data;
        return;
    }
    qDebug()<<__FUNCTION__<<"receive cmd:"<<data;
    uint8_t type=static_cast<uint8_t>(data.at(0));
    if(type==CAN || errorCount>MAX_ERROR){
        state=State::FAILED;
        timer.stop();
    }else if(state==State::STARTING){
        if(type==NAK){
            QByteArray data(PKG_SIZE,0);
            uint32_t* sizePtr=reinterpret_cast<uint32_t*>(data.data());
            sizePtr[0]=fileData.size();
            sendPacket(SOH,0,data);
            errorCount++;
        }else if(type==ACK){
            errorCount=0;
            state=State::DOWNLOADING;
            txCounter=1;
            txPacket=0;
            sendPacket(SOH,txCounter,fileData.mid(txPacket*PKG_SIZE,PKG_SIZE));
        }
    }else if(state==State::DOWNLOADING){
        if(type==ACK){
            errorCount=0;
            txPacket++;
            txCounter++;
            if(txPacket>=packetNum){
                state=State::FINISHING;
                sendPacket(EOT,txCounter,QByteArray(PKG_SIZE,0));
            }else{
                sendPacket(SOH,txCounter,fileData.mid(txPacket*PKG_SIZE,PKG_SIZE));
            }
        }else if(type==NAK){
            sendPacket(SOH,txCounter,fileData.mid(txPacket*PKG_SIZE,PKG_SIZE));
        }
    }else if(state==State::FINISHING){
        if(type==ACK){
            timer.stop();
            state=State::SUCCESS;
        }else if(type==NAK){
            sendPacket(EOT,txCounter,QByteArray(PKG_SIZE,0));
        }
    }
}

void BModem::sendPacket(uint8_t type, uint8_t counter, QByteArray data){
    uint8_t head[3]= {type,counter,static_cast<uint8_t>(0xFF-counter)};
    uint16_t crc16;
    crc16=calcCRC16(reinterpret_cast<const uint8_t*>(data.constData()),data.size());
    uint8_t crc[2]= {(uint8_t)(crc16>>8),(uint8_t)(crc16&0x00FF)};
    QByteArray frameData;
    frameData.append((char*)head,3);
    frameData.append(data);
    frameData.append((char*)crc,2);
    emit txDataRequest(frameData);
}

void BModem::onTimeout(){
    if(state==State::IDLE || state==State::SUCCESS || state==State::FAILED){
        timer.stop();
    }
    errorCount++;
    if(errorCount > MAX_ERROR){
        state=State::FAILED;
        timer.stop();
    }
}

#endif
