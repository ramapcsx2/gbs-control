/* PersWiFiManager
   version 3.0.1
   https://r-downing.github.io/PersWiFiManager/

   modified for inclusion in gbs-control
   see /3rdparty/PersWiFiManager/ for original code and license
*/
#if defined(ESP8266)
#ifndef PERSWIFIMANAGER_H
#define PERSWIFIMANAGER_H

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

#define WIFI_CONNECT_TIMEOUT 45

class PersWiFiManager
{

public:
    typedef std::function<void(void)> WiFiChangeHandlerFunction;

    PersWiFiManager(AsyncWebServer &s, DNSServer &d);

    bool attemptConnection(const String &ssid = "", const String &pass = "");

    void setupWiFiHandlers();

    bool begin(const String &ssid = "", const String &pass = "");

    String getApSsid();

    void setApCredentials(const String &apSsid, const String &apPass = "");

    void setConnectNonBlock(bool b);

    void handleWiFi();

    void startApMode();

    void onConnect(WiFiChangeHandlerFunction fn);

    void onAp(WiFiChangeHandlerFunction fn);

private:
    AsyncWebServer *_server;
    DNSServer *_dnsServer;
    String _apSsid, _apPass;

    bool _connectNonBlock;
    unsigned long _connectStartTime;

    WiFiChangeHandlerFunction _connectHandler;
    WiFiChangeHandlerFunction _apHandler;

}; //class

#endif
#endif
