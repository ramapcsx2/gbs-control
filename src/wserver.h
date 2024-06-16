/*
#####################################################################################
# File: wserver.h                                                                    #
# File Created: Friday, 19th April 2024 3:11:47 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 13th June 2024 4:40:11 pm                      #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/
/*
    DEVELOPER MEMO:
        1. WS messages (_WS, _WSN, _WSF) must not begin with '#' (hash symbol) since
        WebUI filters it, so messages withou hash going to terminal (developer mode)
        and those with hash are treated as a binary data

*/

#ifndef _ESPWSERVER_H_
#define _ESPWSERVER_H_

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "options.h"
#include "presets.h"
#include "tv5725.h"
#include "slot.h"
#include "wserial.h"
#include "wifiman.h"

extern ESP8266WebServer server;
extern char serialCommand;
extern char userCommand;

const char indexPage[] PROGMEM = "/__index";
const char backupFile[] PROGMEM = "/__backup";
const char notFouldMessage[] PROGMEM = "<p style=\"align:center;\">404 | Page Not Found</p>";
const char lomemMessage[] PROGMEM = "<div style=\"align:center;font-size:16px;\"><p>Not enough memory to continue (%.2f kb).</p><p>Please, try again in a few moments...</p></div>";
const char mimeTextHtml[] PROGMEM = "text/html";
const char mimeOctetStream[] PROGMEM = "application/octet-stream";
const char mimeAppJson[] PROGMEM = "application/json";

void serverInit();
void serverWebSocketToggleDeveloperMode();
void serverWebSocketInit();
void serverHome();
void serverSC();
void serverUC();
// void serverWiFiConnect();
void serverSlots();
void serverSlotSet();
void serverSlotSave();
void serverSlotRemove();
void serverFsUploadResponder();
void serverFsUploadHandler();
void serverBackupDownload();
void extractBackup();
// void serverFsDir();
// void serverFsFormat();
void serverWiFiStatus();
// void serverRestoreFilters();
void serverWiFiList();
void serverWiFiWPS();
void serverWiFiConnect();
void serverWiFiAP();
void serverWiFiReset();
// utils
void printInfo();
void printVideoTimings();
void fastGetBestHtotal();

#if defined(ESP8266)

void handleSerialCommand();
void handleUserCommand();
void initUpdateOTA();
void fsToFactory();
void webSocketEvent(uint8_t num, uint8_t type, uint8_t * payload, size_t length);

#endif              // defined(ESP8266)

#endif                              // _ESPWSERVER_H_