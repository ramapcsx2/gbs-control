/*
#####################################################################################
# File: wserial.h                                                                    #
# File Created: Friday, 19th April 2024 4:13:41 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Monday, 27th May 2024 12:23:27 pm                        #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _WSERIAL_H_
#define _WSERIAL_H_

#include <WebSocketsServer.h>

extern char serialCommand;
extern char userCommand;
extern WebSocketsServer webSocket;

// TODO: useless ?
#define LOMEM_SP                        ((ESP.getFreeHeap() > 10000))
#define ASSERT_LOMEM_SP()   do {                                    \
    if(LOMEM_SP) webSocket.disconnect();                            \
} while(0)

void discardSerialRxData();

#if defined(ESP8266)
// serial mirror class for websocket logs
class SerialMirror : public Stream
{
    size_t write(const uint8_t *data, size_t size)
    {
        webSocket.broadcastTXT(data, size);
        Serial.write(data, size);
        return size;
    }

    size_t write(const char *data, size_t size)
    {
        webSocket.broadcastTXT(data, size);
        Serial.write(data, size);
        return size;
    }

    size_t write(uint8_t data)
    {
        webSocket.broadcastTXT(&data, 1);
        Serial.write(data);
        return 1;
    }

    size_t write(char data)
    {
        webSocket.broadcastTXT(&data, 1);
        Serial.write(data);
        return 1;
    }

    int available()
    {
        return 0;
    }
    int read()
    {
        return -1;
    }
    int peek()
    {
        return -1;
    }
    void flush() {}
};

extern SerialMirror SerialM;

// console at websocket
#define _WSF(D, ...)                                                        \
do {                                                                        \
    SerialM.printf(D, ##__VA_ARGS__);                                       \
} while(0)
#define _WSN(D)                                                             \
do {                                                                        \
    SerialM.println(D);                                                     \
} while(0)
#define _WS(D)                                                              \
do {                                                                        \
    SerialM.print(D);                                                       \
} while(0)

#else                               // ESP8266

#define _WSF(D, ...)    (void)0
#define _WSN(D)         (void)0
#define _WS(D)          (void)0

#endif                              // ESP8266

// logging via Serial
#define _DBGF(D, ...)                                                        \
do {                                                                        \
    Serial.printf(D, ##__VA_ARGS__);                                       \
} while(0)
#define _DBGN(D)                                                             \
do {                                                                        \
    Serial.println(D);                                                     \
} while(0)
#define _DBG(D)                                                              \
do {                                                                        \
    Serial.print(D);                                                       \
} while(0)

#endif                              // _WSERIAL_H_