#ifndef PERSWIFIMANAGER_H
#define PERSWIFIMANAGER_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define WIFI_CONNECT_TIMEOUT 30

class PersWiFiManager {

  public:

    typedef std::function<void(void)> WiFiChangeHandlerFunction;

    PersWiFiManager(ESP8266WebServer& s, DNSServer& d);

    bool attemptConnection(const String& ssid = "", const String& pass = "");

    void setupWiFiHandlers();

    bool begin(const String& ssid = "", const String& pass = "");

    String getApSsid();

    void setApCredentials(const String& apSsid, const String& apPass = "");

    void setConnectNonBlock(bool b);

    void handleWiFi();

    void startApMode();

    void onConnect(WiFiChangeHandlerFunction fn);

    void onAp(WiFiChangeHandlerFunction fn);

  private:
    ESP8266WebServer * _server;
    DNSServer * _dnsServer;
    String _apSsid, _apPass;

    bool _connectNonBlock;
    unsigned long _connectStartTime;

    WiFiChangeHandlerFunction _connectHandler;
    WiFiChangeHandlerFunction _apHandler;

};//class

#endif

