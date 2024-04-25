/*
#####################################################################################
# File: serial.h                                                                    #
# File Created: Friday, 19th April 2024 4:13:41 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 25th April 2024 2:21:37 pm                               #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _ESPSERIAL_H_
#define _ESPSERIAL_H_

#include "src/WebSocketsServer.h"

extern char serialCommand;
extern char userCommand;
extern WebSocketsServer webSocket;

void discardSerialRxData();

// serial mirror class for websocket logs
class SerialMirror : public Stream
{
    size_t write(const uint8_t *data, size_t size)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(data, size);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data, size);
        return size;
    }

    size_t write(const char *data, size_t size)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(data, size);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data, size);
        return size;
    }

    size_t write(uint8_t data)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(&data, 1);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data);
        return 1;
    }

    size_t write(char data)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(&data, 1);
        } else {
            webSocket.disconnect();
        }
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

#if defined(ESP8266)
// serial mirror class for websocket logs
SerialMirror SerialM;
#else
#define SerialM Serial
#endif

#define LOGF(D, ...)                                                      \
do {                                                                      \
    SerialM.printf(D, ##__VA_ARGS__);                                     \
} while(0)
#define LOGN(D)                                                             \
do {                                                                        \
    SerialM.println(D);                                                     \
} while(0)
#define LOG(D)                                                                  \
do {                                                                             \
    SerialM.print(D);                                                            \
} while(0)

// extern SerialMirror SerialM;

#endif                              // _ESPSERIAL_H_