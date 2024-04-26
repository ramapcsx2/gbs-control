/*
#####################################################################################
# File: wifiman.cpp                                                                 #
# File Created: Friday, 19th April 2024 2:25:33 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Friday, 26th April 2024 12:01:36 am                                #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/


#include "wifiman.h"

static unsigned long _connectCheckTime = 0;
static unsigned long _lastTimePing = 0;

#define THIS_DEVICE_MASTER
#ifdef THIS_DEVICE_MASTER
const char *ap_ssid = "gbscontrol";
const char *ap_password = "qqqqqqqq";
// change device_hostname_full and device_hostname_partial to rename the device
// (allows 2 or more on the same network)
// new: only use _partial throughout, comply to standards
const char *device_hostname_full = "gbscontrol.local";
static const char *device_hostname_partial = "gbscontrol"; // for MDNS
static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbscontrol, pass 'qqqqqqqq'): Access 'gbscontrol.local' in your browser";
const char st_info_string[] PROGMEM =
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

/**
 * @brief
 *
 * @param e
 */
static void wifiEventHandler(System_Event_t *e)
{
    if (e->event == WIFI_EVENT_STAMODE_CONNECTED)
    {
        LOG(F("(WiFi): STA mode connected; IP: "));
        LOGN(WiFi.localIP().toString());
        if (MDNS.begin(device_hostname_partial, WiFi.localIP())) { // MDNS request for gbscontrol.local
            //Serial.println("MDNS started");
            MDNS.addService("http", "tcp", 80); // Add service to MDNS-SD
            MDNS.announce();
        }
        LOGN(FPSTR(st_info_string));
    }
    else if(e->event == WIFI_EVENT_MODE_CHANGE)
    {
            LOGF("WiFi mode changed, now: %d\n", WiFi.getMode());
    }
    else if(e->event == WIFI_EVENT_STAMODE_DISCONNECTED)
    {
        _connectCheckTime = 0;
        LOGN("disconnected from AP, reconnect...");
    }
    else if(e->event == WIFI_EVENT_SOFTAPMODE_STACONNECTED)
    {
        uint8_t * mac = e->event_info.sta_connected.mac;
        LOGF("station connected, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else if(e->event == WIFI_EVENT_SOFTAPMODE_STADISCONNECTED)
    {
        uint8_t * mac = e->event_info.sta_disconnected.mac;
        LOGF("station disconnected, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

/**
 * @brief
 *
 */
void updateWebSocketData()
{
    if (rto->webServerEnabled && rto->webServerStarted) {
        if (webSocket.connectedClients() > 0) {

            constexpr size_t MESSAGE_LEN = 6;
            char toSend[MESSAGE_LEN] = {0};
            toSend[0] = '#'; // makeshift ping in slot 0

            if (rto->isCustomPreset) {
                toSend[1] = '9';
            } else
                switch (rto->presetID) {
                    case 0x01:
                    case 0x11:
                        toSend[1] = '1';
                        break;
                    case 0x02:
                    case 0x12:
                        toSend[1] = '2';
                        break;
                    case 0x03:
                    case 0x13:
                        toSend[1] = '3';
                        break;
                    case 0x04:
                    case 0x14:
                        toSend[1] = '4';
                        break;
                    case 0x05:
                    case 0x15:
                        toSend[1] = '5';
                        break;
                    case 0x06:
                    case 0x16:
                        toSend[1] = '6';
                        break;
                    case PresetHdBypass:    // bypass 1
                    case PresetBypassRGBHV: // bypass 2
                        toSend[1] = '8';
                        break;
                    default:
                        toSend[1] = '0';
                        break;
                }

            toSend[2] = (char)uopt->presetSlot;

            // '@' = 0x40, used for "byte is present" detection; 0x80 not in ascii table
            toSend[3] = '@';
            toSend[4] = '@';
            toSend[5] = '@';

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

            // send ping and stats
            if (ESP.getFreeHeap() > 14000) {
                webSocket.broadcastTXT(toSend, MESSAGE_LEN);
            } else {
                webSocket.disconnect();
            }
        }
    }
}

/**
 * @brief
 *
 */
void wifiInit() {
    wifi_set_event_handler_cb(wifiEventHandler);

    if (!wifiStartStaMode("")) {
        // no stored network to connect to > start AP mode right away
        wifiStartApMode();
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
    // stop DNS
    dnsServer.stop();
    // and off we go...
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.hostname(device_hostname_partial);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    if (ssid.length()) {
        if (pass.length())
            WiFi.begin(ssid.c_str(), pass.c_str());
        else
            WiFi.begin(ssid.c_str());
    } else {
        const String ssid = WiFi.SSID();
        if(ssid.length() != 0)
            WiFi.begin(ssid.c_str());
    }
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
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    ret = WiFi.softAP(ap_ssid, strlen(ap_password) != 0 ? ap_password : NULL, 1, 0 ,2);
    if(ret) {
        dnsServer.stop();
        dnsServer.setTTL(300); // (in seconds) as per example
        dnsServer.start(53, "*", apIP);
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        if(!dnsServer.start((byte)53, "*", apIP)) //used for captive portal in AP mode
            LOGN(F("DNS no sockets available..."));

        LOGN(FPSTR(ap_info_string));
    }
    return ret;
}

/**
 * @brief Put this method inside of main loop
 *
 */
void wifiLoop(bool instant) {
    if(millis() > (_connectCheckTime + 5000U)) {
        if(WiFi.getMode() != WIFI_AP_STA) {
            // check WiFi connection status, switch to AP if not connected
            if (WiFi.status() != WL_CONNECTED)
                wifiStartApMode();
        }
        // else {
        //    scan and connect?
        // }
        _connectCheckTime = millis();
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
        if (rto->allowUpdatesOTA) {
            ArduinoOTA.handle();
        }
    }
}

/**
 * @brief Temporary solution
 * @todo this function is used by wserver since ap_ssid is not available there.
 *
 * @return const char*
 */
const char * wifiGetApSSID() {
    return ap_ssid;
}