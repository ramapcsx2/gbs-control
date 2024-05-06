/*
###########################################################################
# File: menu.h                                                            #
# File Created: Thursday, 2nd May 2024 11:31:43 pm                        #
# Author:                                                                 #
# Last Modified: Sunday, 5th May 2024 4:01:54 pm                          #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#ifndef _MENU_H_
#define _MENU_H_

#include <SSD1306.h>
#include "options.h"
#include "wserial.h"

extern SSD1306Wire display;

void handleButtons(void);

#if !USE_NEW_OLED_MENU

void IRAM_ATTR isrRotaryEncoder();
void settingsMenuOLED();
void pointerfunction();
void subpointerfunction();

#else                       // \! USE_NEW_OLED_MENU

#include "OLEDMenuImplementation.h"

void IRAM_ATTR isrRotaryEncoderRotateForNewMenu();
void IRAM_ATTR isrRotaryEncoderPushForNewMenu();

#endif                      // \! USE_NEW_OLED_MENU

void menuInit();
void menuLoop();

#endif                                  // _MENU_H_
