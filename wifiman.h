/*
#####################################################################################
# File: wifiman.h                                                                   #
# File Created: Friday, 19th April 2024 2:25:42 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 25th April 2024 3:01:44 pm                               #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _WIFIMAN_H
#define _WIFIMAN_H

#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "options.h"

#ifdef THIS_DEVICE_MASTER
const char *ap_ssid = "gbscontrol";
const char *ap_password = "qqqqqqqq";
// change device_hostname_full and device_hostname_partial to rename the device
// (allows 2 or more on the same network)
// new: only use _partial throughout, comply to standards
const char *device_hostname_full = "gbscontrol.local";
const char *device_hostname_partial = "gbscontrol"; // for MDNS
static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbscontrol, pass 'qqqqqqqq'): Access 'gbscontrol.local' in your browser";
static const char st_info_string[] PROGMEM =
    "(WiFi): Access 'http://gbscontrol:80' or 'http://gbscontrol.local' (or device IP) in your browser";
#else
const char *ap_ssid = "gbsslave";
const char *ap_password = "qqqqqqqq";
const char *device_hostname_full = "gbsslave.local";
const char *device_hostname_partial = "gbsslave"; // for MDNS
static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbsslave, pass 'qqqqqqqq'): Access 'gbsslave.local' in your browser";
static const char st_info_string[] PROGMEM =
    "(WiFi): Access 'http://gbsslave:80' or 'http://gbsslave.local' (or device IP) in your browser";
#endif

extern DNSServer dnsServer;
extern struct runTimeOptions *rto;
extern void updateWebSocketData();

//  HOWTO: Rely on the previously implemented logic
//    __________     _____________     __________________   yes  _____________________
//   | if ssid | -> | connect AP | -> | connect failed? | ----> | fallback to APmode |
//   ----------     -------------     ------------------        ---------------------
//      ^                 _________________     |_____________
//      |                |      yes       v          no      |
//      | no  ____________________       _______________     |
//      ---- | is now connected? | < -- | continue ops | < --
//           --------------------       ---------------
//
//  THE OBJECTIVE: Do not rely on build-in autoreconnect routine,
//                 use the custom one instead.

void wifiInit();
void wifiDisable();
bool wifiStartStaMode(const String & ssid, const String & pass);
void wifiStartApMode();
void wifiLoop(bool instant);

#endif                                  // _WIFIMAN_H