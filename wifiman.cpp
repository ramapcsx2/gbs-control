/*
#####################################################################################
# File: wifiman.cpp                                                                 #
# File Created: Friday, 19th April 2024 2:25:33 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 25th April 2024 2:29:05 pm                               #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/


#include "wifiman.h"

static time_t _connectCheckTime = 0;
static time_t lastTimePing = 0;

void wifiEventHandler(System_Event_t *e)
{
    if (e->event == WIFI_EVENT_STAMODE_CONNECTED) {
        LOG(F("(WiFi): STA mode connected; IP: "));
        LOGN(WiFi.localIP().toString());
        if (MDNS.begin(device_hostname_partial, WiFi.localIP())) { // MDNS request for gbscontrol.local
            //Serial.println("MDNS started");
            MDNS.addService("http", "tcp", 80); // Add service to MDNS-SD
            MDNS.announce();
        }
        LOGN(st_info_string);
    }
    else if(e->event == WIFI_EVENT_MODE_CHANGE)
    {
        if(WiFi.getMode() == WIFI_AP_STA) {
            LOGN(F(ap_info_string));
        // add mdns announce here as well?
        }
    } else if(e->event == WIFI_EVENT_STAMODE_DISCONNECTED) {
        _connectCheckTime = millis();
        LOGF("Station disconnected, reason: %ld", event.reason);
    }
}


/**
 * @brief
 *
 */
void wifiInit() {
    wifi_set_event_handler_cb(wifiEventHandler);

    if (WiFi.SSID().length() == 0) {
        // no stored network to connect to > start AP mode right away
        wifiStartApMode();
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start((byte)53, "*", WiFi.localIP()); //used for captive portal in AP mode
    } else {
        wifiStartStaMode(); // first try connecting to stored network, go AP mode after timeout
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
    // millis() at this point: typically 65ms
    // start web services as early in boot as possible
    WiFi.mode(WIFI_STA);
    WiFi.hostname(device_hostname_partial); // was _full
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    if (ssid.length()) {
        if (pass.length())
            WiFi.begin(ssid.c_str(), pass.c_str());
        else
            WiFi.begin(ssid.c_str());
    } else {
        WiFi.begin();
    }
    return (WiFi.status() == WL_CONNECTED);
}

/**
 * @brief
 *
 */
void wifiStartApMode() {
    wifi_set_event_handler_cb(wifiEventHandler);
    // WiFi.disconnect();
    // delay(100);
    // IPAddress apIP(192, 168, 4, 1);
    WiFi.mode(WIFI_AP_STA);
    // WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ap_ssid, strlen(ap_password) != 0 ? ap_password : NULL);
}

/**
 * @brief Put this method inside of main loop
 *
 */
void wifiLoop(bool instant) {
    if(WiFi.getMode() != WIFI_AP_STA
        && millis() > (_connectCheckTime + 5000)) {
        // check WiFi connection status, switch to AP if not connected
        if (WiFi.status() != WL_CONNECTED) {
            wifiStartApMode();
        }
        _connectCheckTime = millis();
    }
    if (rto->webServerEnabled && rto->webServerStarted) {
        MDNS.update();
        dnsServer.processNextRequest();

        if ((millis() - lastTimePing) > 953) { // slightly odd value so not everything happens at once
            webSocket.broadcastPing();
        }
        if (((millis() - lastTimePing) > 973) || instant) {
            if ((webSocket.connectedClients(false) > 0) || instant) { // true = with compliant ping
                updateWebSocketData();
            }
            lastTimePing = millis();
        }
        if (rto->allowUpdatesOTA) {
            ArduinoOTA.handle();
        }
    }
}