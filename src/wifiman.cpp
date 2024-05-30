/*
#####################################################################################
# File: wifiman.cpp                                                                 #
# File Created: Friday, 19th April 2024 2:25:33 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 30th May 2024 12:54:36 pm                      #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#include "wifiman.h"

static unsigned long _connectCheckTime = 0;
static unsigned long _lastTimePing = 0;

#ifdef THIS_DEVICE_MASTER

static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbscontrol, pass 'qqqqqqqq'): Access 'gbscontrol.local' in your browser";
const char st_info_string[] PROGMEM =
    "(WiFi): Access 'http://gbscontrol:80' or 'http://gbscontrol.local' (or device IP) in your browser";

#else

static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbsslave, pass 'qqqqqqqq'): Access 'gbsslave.local' in your browser";
static const char st_info_string[] PROGMEM =
    "(WiFi): Access 'http://gbsslave:80' or 'http://gbsslave.local' (or device IP) in your browser";

#endif

/**
 * @brief
 *
 * @param e
 */
static void wifiEventHandler(System_Event_t *e)
{
    if (e->event == WIFI_EVENT_STAMODE_CONNECTED)
    {
        // _DBGN(F("(WiFi): STA mode connected"));
        _connectCheckTime = 0;
    }
    else if(e->event == WIFI_EVENT_STAMODE_GOT_IP)
    {
        _DBG(F("(WiFi): got IP: "));
        _DBGN(WiFi.localIP().toString());
        if (MDNS.begin(String(gbsc_device_hostname).c_str(), WiFi.localIP())) { // MDNS request for gbscontrol.local
            MDNS.addService("http", "tcp", 80); // Add service to MDNS-SD
            MDNS.announce();
        }
        _DBGN(FPSTR(st_info_string));
    }
    else if(e->event == WIFI_EVENT_MODE_CHANGE)
    {
        if(e->event_info.opmode_changed.new_opmode == WIFI_AP) {
            MDNS.end();
        }
        _DBGF("(WiFi) mode changed, now: %d\n", WiFi.getMode());
    }
    else if(e->event == WIFI_EVENT_STAMODE_DISCONNECTED)
    {
        _connectCheckTime = millis();
        _DBGN("disconnected from AP, reconnect...");
    }
    else if(e->event == WIFI_EVENT_SOFTAPMODE_STACONNECTED)
    {
        uint8_t * mac = e->event_info.sta_connected.mac;
        _DBGF("station connected, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else if(e->event == WIFI_EVENT_SOFTAPMODE_STADISCONNECTED)
    {
        uint8_t * mac = e->event_info.sta_disconnected.mac;
        _DBGF("station disconnected, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

/**
 * @brief Response to WS with current
 *          system status and preset data
 *          the data structure must match those
 *          which is in webUI (see: createWebSocket())
 *
 */
void updateWebSocketData()
{
    // assert free heap
    if (ESP.getFreeHeap() > 14000) {
        webSocket.disconnect();
    }

    if (rto->webServerEnabled && rto->webServerStarted) {
        if (webSocket.connectedClients() > 0) {
            constexpr size_t MESSAGE_LEN = 8;
            char toSend[MESSAGE_LEN] = "";
            toSend[0] = '#'; // makeshift ping in slot 0
            // slotID is INTEGER
            toSend[1] = uopt->presetSlot + '0';
            // TODO: resolutionID must be INTEGER too
            toSend[2] = static_cast<char>(rto->resolutionID);
            // @sk: left here for reference
            //     switch (rto->presetID) {
            //         case 0x11:    // Output960p
            //             toSend[1] = '1';
            //             break;
            //         case 0x12:        // Output1024p
            //             toSend[1] = '2';
            //             break;
            //         case 0x13:        // Output720p
            //             toSend[1] = '3';
            //             break;
            //         case 0x14:        // Output480p
            //             toSend[1] = '4';
            //             break;
            //         case 0x15:    // Output1080p
            //             toSend[1] = '5';
            //             break;
            //         case 0x16:   // Output15kHz
            //             toSend[1] = '6';
            //             break;
            //         case OutputBypass:        // bypass 0
            //             toSend[1] = '8';
            //             break;
            //         case PresetHdBypass:    // bypass 1
            //             toSend[1] = '9';
            //             break;
            //         case PresetBypassRGBHV: // bypass 2
            //             toSend[1] = 'A';
            //             break;
            //         case OutputCustom:
            //             toSend[1] = 'C';
            //             break;
            //         default:
            //             toSend[1] = '0';
            //             break;
            //     }

            // '@' = 0x40, used for "byte is present" detection; 0x80 not in ascii table
            toSend[3] = '@';
            toSend[4] = '@';
            toSend[5] = '@';
            toSend[6] = '0';
            toSend[7] = '0';

            if (uopt->enableAutoGain) {
                toSend[3] |= (1 << 0);
            }
            if (uopt->wantScanlines) {
                toSend[3] |= (1 << 1);
            }
            if (uopt->wantVdsLineFilter) {
                toSend[3] |= (1 << 2);
            }
            if (uopt->wantPeaking) {
                toSend[3] |= (1 << 3);
            }
            if (uopt->PalForce60) {
                toSend[3] |= (1 << 4);
            }
            if (uopt->wantOutputComponent) {
                toSend[3] |= (1 << 5);
            }

            if (uopt->matchPresetSource) {
                toSend[4] |= (1 << 0);
            }
            if (uopt->enableFrameTimeLock) {
                toSend[4] |= (1 << 1);
            }
            if (uopt->deintMode) {
                toSend[4] |= (1 << 2);
            }
            if (uopt->wantTap6) {
                toSend[4] |= (1 << 3);
            }
            if (uopt->wantStepResponse) {
                toSend[4] |= (1 << 4);
            }
            if (uopt->wantFullHeight) {
                toSend[4] |= (1 << 5);
            }

            if (uopt->enableCalibrationADC) {
                toSend[5] |= (1 << 0);
            }
            if (uopt->preferScalingRgbhv) {
                toSend[5] |= (1 << 1);
            }
            if (uopt->disableExternalClockGenerator) {
                toSend[5] |= (1 << 2);
            }

            // developer panel controls status
            /// print info button
            if(rto->printInfos) {
                toSend[6] |= (1 << 0);
            }
            if(rto->invertSync) {
                toSend[6] |= (1 << 1);
            }
            if(rto->osr != 0) {
                toSend[6] |= (1 << 2);
            }
            if(GBS::ADC_FLTR::read() != 0) {
                toSend[6] |= (1 << 3);
            }

            // system tab controls
            if(rto->allowUpdatesOTA) {
                toSend[7] |= (1 << 0);
            }

            webSocket.broadcastTXT(toSend, MESSAGE_LEN);
        }
    }
}

/**
 * @brief
 *
 */
void wifiInit() {
    wifi_set_event_handler_cb(wifiEventHandler);
    WiFi.persistent(true);
    WiFi.setAutoReconnect(false);
    WiFi.hostname(String(gbsc_device_hostname).c_str());
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setOutputPower(16.0f);         // float: min 0.0f, max 20.5f

    if (!wifiStartStaMode("")) {
        _connectCheckTime = millis();
        // no stored network to connect to > start AP mode right away
        // wifiStartApMode();
    }
}

/**
 * @brief
 *
 */
void wifiDisable() {
    //WiFi.disconnect(); // deletes credentials
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
}

/**
 * @brief
 *
 */
bool wifiStartStaMode(const String & ssid, const String & pass) {
    // int8_t cntr = 10;
    // and off we go...
    WiFi.mode(WIFI_STA);
    if (ssid.length()) {
        WiFi.begin(ssid.c_str(), pass.c_str());
    } else {
        // using credentials stored in flash
        WiFi.begin();
    }
    _DBG(F("connecting to: "));
    _DBGF("%s...\n", ssid.c_str());
    // no fancy stuffs here :)
    // while(WiFi.status() == WL_DISCONNECTED && cntr != 0) {
    //     delay(500);
    //     cntr--;
    // }
    delay(100);
    return (WiFi.status() == WL_CONNECTED);
}

/**
 * @brief
 *
 */
bool wifiStartApMode() {
    bool ret  = false;
    // WiFi.disconnect();
    // delay(100);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    ret = WiFi.softAP(wifiGetApSSID().c_str(), strlen_P(ap_password) != 0 ? wifiGetApPASS().c_str() : NULL, 1, 0 ,2);
    if(ret) {
        dnsServer.stop();
        dnsServer.setTTL(300); // (in seconds) as per example
        dnsServer.start(53, "*", apIP);
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        if(!dnsServer.start((byte)53, "*", apIP)) //used for captive portal in AP mode
            _DBGN(F("DNS no sockets available..."));

        _DBGN(FPSTR(ap_info_string));
    }
    return ret;
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool wifiStartWPS() {
    _DBGN(F("starting WPS"));
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    bool ret = WiFi.beginWPSConfig();
    if(ret) {
        String newSSID = WiFi.SSID();
        if(newSSID.length() > 0) {
            _DBGF("WPS connected to SSID: %s\n", newSSID.c_str());
            ret = true;
        } else {
            _DBGN(F("WPS failed. please try again"));
            ret = false;
        }
    }
    return ret;
}

/**
 * @brief Put this method inside of main loop
 *
 */
void wifiLoop(bool instant) {
    if(WiFi.status() != WL_CONNECTED
        && _connectCheckTime != 0
            && millis() > (_connectCheckTime + 10000UL)) {
        // if empty - use last stored credentials
        String s = WiFi.SSID();
        _DBGF("SSID: %s\n", WiFi.SSID().c_str());
        if(s.length() != 0) {
            _connectCheckTime = millis();
            WiFi.reconnect();
        } else {
            wifiStartApMode();
            _connectCheckTime = 0;
        }
    }
    if (rto->webServerEnabled && rto->webServerStarted) {
        MDNS.update();
        dnsServer.processNextRequest();

        if ((millis() - _lastTimePing) > 953) { // slightly odd value so not everything happens at once
            webSocket.broadcastPing();
        }
        if (((millis() - _lastTimePing) > 973) || instant) {
            if ((webSocket.connectedClients(false) > 0) || instant) { // true = with compliant ping
                updateWebSocketData();
            }
            _lastTimePing = millis();
        }
    }
}

/**
 * @brief Temporary solution
 * @todo this function is used by wserver since ap_ssid is not available there.
 *
 * @return String
 */
const String wifiGetApSSID() {
    return String(ap_ssid);
}

/**
 * @brief Temporary solution
 *
 * @return String
 */
const String wifiGetApPASS() {
    return String(ap_password);
}

/**
 * @brief Returns 0 if NOT connected to AP or NOT in AP_STA mode
 *
 * @param r
 * @return true
 * @return false
 */
int8_t wifiGetRSSI() {
    if ((WiFi.status() == WL_CONNECTED) || (WiFi.getMode() == WIFI_AP)) {
        return (int8_t)wifi_station_get_rssi();
    }
    return 0;
}

/**
 * @brief Erase previous credentials and reset the device
 *
 */
// void wifiReset() {
//     WiFi.disconnect();
//     delay(100);
//     ESP.reset();
// }