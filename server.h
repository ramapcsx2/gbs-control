/*
#####################################################################################
# File: server.h                                                                    #
# File Created: Friday, 19th April 2024 3:11:47 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 25th April 2024 3:50:47 pm                               #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _ESPWSERVER_H_
#define _ESPWSERVER_H_

#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "webui_html.h"
#include "options.h"
#include "slot.h"
#include "serial.h"
#include "wifiman.h"

extern ESP8266WebServer server;
extern char serialCommand;
extern char userCommand;
extern struct userOptions * uopt;
extern struct runTimeOptions *rto;

extern void saveUserPrefs();
extern void disableScanlines();

String slotIndexMap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,";
const char mimeTextHtml[] PROGMEM = "text/html";
const char mimeOctetStream[] PROGMEM = "application/octet-stream";
const char mimeAppJson[] PROGMEM = "application/json";

void serverInit();
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
void serverFsDownloadHandler();
void serverFsDir();
void serverFsFormat();
void serverWiFiStatus();
void serverRestoreFilters();
void serverWiFiList();
void serverWiFiWPS();
void serverWiFiConnect();
void serverWiFiAP();
void serverWiFiReset();

#endif                              // _ESPWSERVER_H_