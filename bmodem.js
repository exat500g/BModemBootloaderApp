/*
YModem有毛病,还不如自己仿YModem写一个BModem
*/
const SerialPort = require("serialport");
const fs = require("fs");
const crc16 = require("crc").crc16xmodem;

const SOH=0x01; /* start of 128-byte data packet */
const EOT=0x04; /* end of transmission */
const ACK=0x06; /* acknowledge */
const NAK=0x15; /* negative acknowledge */
const CAN=0x18; // cancel
const PKG_SIZE=0x80;

const BModem = function(portname,filename,progressCallback){
    var sp=null;
    var fwdata=null;
    const rebootCmd=Buffer.from([0xFE,0xFD,0xF0,0x02,0xEB,0x00,0x4F,0x40,0x55,0xAA]);
    const start = new Promise((resolve,reject)=>{
        if(!portname){
            reject("PORT: no portname");
            return;
        }
        sp=new SerialPort(portname,{autoOpen:true,baudrate:115200,parity:'even'},(err)=>{
            if(err){
                reject("PORT: "+ portname+" open failed, "+err);
                return;
            }
            try{
                fwdata=fs.readFileSync(filename);
            }catch(err){
                sp.close(()=>{
                    reject("FILE: "+filename +" open failed, "+err);
                });
                return;
            }
            sp.on('data',onData);
            sp.write(rebootCmd);
            asyncStart(fwdata).then(()=>{
                sp.close(()=>{
                    resolve();
                });
            }).catch((err)=>{
                sp.close(()=>{
                    reject(err);
                });
            });
        });
    });

    var cbOnRx = null;
    var rxCmd = null;
    const onData=(data)=>{
        if(data.length>2 && typeof(progressCallback)=='function'){
            progressCallback("rx: "+data.toString());
        }
        console.log("onData=",data,",text=",String(data));
        rxCmd=data[data.length-1];
        if(cbOnRx){
            cbOnRx();
        }
    }

    const writePacket = function(type,counter,data){
        console.log("writePacket:type=",type,",counter=",counter,"size=",data.length);
        rxCmd=null;
        return new Promise((resolve,reject)=>{
            const head=new Buffer.from([type,counter,0xFF-counter]);
            const crc_num=crc16(data);
            const crc=Buffer.from([crc_num/0x100,crc_num%0x100]);
            const packet = Buffer.concat([head,data,crc],head.length+data.length+crc.length);
            sp.drain((err)=>{
                if(err){
                    console.log(err);
                    return;
                }
                sp.write(packet,(err)=>{
                    if(err){
                        reject(err);
                        return;
                    }
                    resolve();
                });
            });
        });
    }
    const rx = function(){
        return new Promise((resolve,reject)=>{
            if(typeof(rxCmd)=='number'){
                resolve(rxCmd);
                rxCmd=null;
                return;
            }
            cbOnRx=()=>{
                cbOnRx=null;
                clearTimeout(to);
                resolve(rxCmd);
                rxCmd=null;
            }
            const to=setTimeout(()=>{
                cbOnRx=null;
                rxCmd=null;
                reject("rx timeouted");
            },10000);
        });
    }

    const asyncStart = async function(data){
        {
            let align = new Buffer.alloc(PKG_SIZE-data.length%PKG_SIZE,0);
            data=Buffer.concat([data,align],data.length+align.length);
        }
        const MAX_ERR=10;
        var errCount=0;
        {
            let buffer=Buffer.alloc(128,0);
            buffer.writeUInt32LE(data.length,0);
            while(1){
                const type=await rx();
                if(type==NAK){
                    errCount++;
                    await writePacket(SOH,0,buffer);
                }else if(type==ACK){
                    errCount=0;
                    if(typeof(progressCallback)=='function'){
                        progressCallback("transmit started");
                    }
                    break;
                }else if(type==CAN){
                    throw "client canceled";
                }else{
                    console.log("unknow head=",type);
                    errCount++;
                }
                if(errCount>=MAX_ERR){
                    throw "HEAD: error to much";
                }
            }
        }
        {
            const pkgNum=data.length/PKG_SIZE;
            let pkgId=0;
            await writePacket(SOH,pkgId+1,data.slice(pkgId*PKG_SIZE,pkgId*PKG_SIZE+PKG_SIZE));
            while(1){
                const type=await rx();
                if(type==ACK || type==NAK){
                    if(type==ACK){
                        errCount=0;
                        pkgId++;
                        if(typeof(progressCallback)=='function'){
                            progressCallback(pkgId,pkgNum);
                        }
                        if(pkgId>pkgNum){
                            break;
                        }
                    }else{
                        errCount++;
                    }
                    if(pkgId < pkgNum){
                        await writePacket(SOH,pkgId+1,data.slice(pkgId*PKG_SIZE,pkgId*PKG_SIZE+PKG_SIZE));
                    }else if(pkgId==pkgNum){
                        console.log("EOT sended");
                        sp.drain((err)=>{
                            if(err){
                                console.log(err);
                                return;
                            }
                            sp.write(Buffer.from([EOT]));
                        });
                    }
                }else if(type==CAN){
                    throw "client canceled";
                }else{
                    errCount++;
                    console.log("unknow cmd:",type," err=",errCount);
                }
                if(errCount>=MAX_ERR){
                    throw "DATA: error to much: "+errCount;
                }
            }
        }
        return;
    }
    return start;
}

module.exports = BModem;

