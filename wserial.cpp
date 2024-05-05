/*
###########################################################################
# File: wserial.cpp                                                       #
# File Created: Friday, 19th April 2024 4:13:35 pm                        #
# Author: Sergey Ko                                                       #
# Last Modified: Saturday, 4th May 2024 12:31:47 am                       #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#include "wserial.h"

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
