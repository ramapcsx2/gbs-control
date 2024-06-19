/*
###########################################################################
# File: wserial.cpp                                                       #
# File Created: Friday, 19th April 2024 4:13:35 pm                        #
# Author: Sergey Ko                                                       #
# Last Modified: Tuesday, 18th June 2024 1:29:36 pm                       #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#include "wserial.h"

static char * serialBuffer = nullptr;

/**
 * @brief If developer mode enabled serialBuffer will be created.
 *        If developer mode disabled serialBuffer will be destroyed.
 */
void serialDevModeToggle() {
    if(uopt.developerMode) {
        if(serialBuffer == nullptr) {
            serialBuffer = (char *)malloc(SERIAL_BUFFER_MAX_LEN);
            serialBufferClean();
        }
        return;
    }
    // destroy buffer if not the developer mode
    free(serialBuffer);
    serialBuffer = nullptr;
}

/**
 * @brief
 *
 */
void discardSerialRxData()
{
    uint16_t maxThrowAway = 0x1fff;
    while (Serial.available() && maxThrowAway > 0) {
        Serial.read();
        maxThrowAway--;
    }
}

/**
 * @brief Returns serial buffer data. bufferLength (string length)
 *          may be retrieved by passing bufferLength argumentß
 *
 * @param bufferLength
 * @return const uint8_t*
 */
const char * serialGetBuffer() {
    return serialBuffer;
}

/**
 * @brief Empty serial buffer
 *
 */
void serialBufferClean() {
    memset(serialBuffer, '\0', SERIAL_BUFFER_MAX_LEN);
}

/**
 * @brief adding serial output data to accumulator (serialBuffer)
 *
 * @param data
 * @param size
 */
void SerialMirror::pushToBuffer(void * data, size_t size) {
    if(!uopt.developerMode)
        return;
    uint16_t i = 0;
    const char * d = reinterpret_cast<char *>(data);
    size_t buffLen = strlen(serialBuffer);
    while((buffLen + i) < SERIAL_BUFFER_MAX_LEN && i < size) {
        *(serialBuffer + buffLen + i) = *(d + i);
        i++;
    }
    // do not clog the output, use only for debug
    // if(i < size)
    //     _DBGN(F("(!) serial buffer is full, data truncated..."));
}

