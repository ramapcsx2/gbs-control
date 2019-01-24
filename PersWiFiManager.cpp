/* PersWiFiManager
   version 3.0.1
   https://r-downing.github.io/PersWiFiManager/

   modified for inclusion in gbs-control
   see /3rdparty/PersWiFiManager/ for original code and license
*/
#if defined(ESP8266)
#include "PersWiFiManager.h"

#define WIFI_HTM_PROGMEM
#ifdef WIFI_HTM_PROGMEM
const char wifi_htm[] PROGMEM = R"=====(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no"/><title>ESP WiFi</title><script>function g(i){return document.getElementById(i);};function p(t,l){if(confirm(t)) window.location=l;};function E(s){return document.createElement(s)};var S="setAttribute",A="appendChild",H="innerHTML",X,wl;function scan(){if(X) return;X=new XMLHttpRequest(),wl=document.getElementById('wl');wl[H]="Scanning...";X.onreadystatechange=function(){if (this.readyState==4&&this.status==200){X=0;wl[H]="";this.responseText.split("\n").forEach(function (e){let t=e.split(","), s=t.slice(2).join(',');var d=E('div'),i=E('a'),c=E('a');i[S]('class','s'); c[S]('class','q');i.onclick=function(){g('s').value=s;g('p').focus();};i[A](document.createTextNode(s));c[H]=t[0]+"%"+(parseInt(t[1])?"\uD83D\uDD12":"\u26A0");wl[A](i); wl[A](c);wl[A](document.createElement('br'));});}};X.open("GET","wifi/list",true);X.send();};</script><style>input{padding:5px;font-size:1em;width:95%;}body{text-align:center;font-family:verdana;background-color:black;color:white;}a{color:#1fa3ec;}button{border:0;border-radius:0.3em;background-color:#1fa3ec;color:#fff;line-height:2.4em;font-size:1.2em;width:100%;display:block;}.q{float:right;}.s{display:inline-block;width:14em;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}#wl{line-height:1.5em;}</style></head><body><div style='text-align:left;display:inline-block;width:320px;padding:5px'><button onclick="scan()">&#x21bb; Scan</button><p id='wl'></p><form method='post' action='/wifi/connect'><input id='s' name='n' length=32 placeholder='SSID'><br><input id='p' name='p' length=64 type='password' placeholder='password'><br><br><button type='submit'>Connect</button></form><br><br><button onclick="p('Reboot device?','/wifi/rst')">Reboot</button><br><a href="javascript:history.back()">Back</a> |<a href="/">Home</a></div></body></html>)=====";
#endif

extern const char* device_hostname_full;

PersWiFiManager::PersWiFiManager(ESP8266WebServer& s, DNSServer& d) {
  _server = &s;
  _dnsServer = &d;
  _apPass = "";
} //PersWiFiManager

bool PersWiFiManager::attemptConnection(const String& ssid, const String& pass) {
  //attempt to connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.hostname(device_hostname_full);
  if (ssid.length()) {
    if (pass.length()) WiFi.begin(ssid.c_str(), pass.c_str());
    else WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin();
  }

  //if in nonblock mode, skip this loop
  _connectStartTime = millis();// + 1;
  while (!_connectNonBlock && _connectStartTime) {
    handleWiFi();
    delay(10);
  }

  return (WiFi.status() == WL_CONNECTED);

} //attemptConnection

void PersWiFiManager::handleWiFi() {
  if (!_connectStartTime) return;

  if (WiFi.status() == WL_CONNECTED) {
    _connectStartTime = 0;
    if (_connectHandler) _connectHandler();
    return;
  }

  //if failed or not connected and time is up
  if ((WiFi.status() == WL_CONNECT_FAILED) || ((WiFi.status() != WL_CONNECTED) && ((millis() - _connectStartTime) > (1000 * WIFI_CONNECT_TIMEOUT)))) {
    startApMode();
    _connectStartTime = 0; //reset connect start time
  }

} //handleWiFi

void PersWiFiManager::startApMode(){
  //start AP mode
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  _apPass.length() ? WiFi.softAP(getApSsid().c_str(), _apPass.c_str(), 6) : WiFi.softAP(getApSsid().c_str());

  _dnsServer->stop();
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is DNSReplyCode::NonExistentDomain
  _dnsServer->setErrorReplyCode(DNSReplyCode::ServerFailure);
  // modify TTL associated  with the domain name (in seconds) // default is 60 seconds
  _dnsServer->setTTL(300); // (in seconds) as per example
  _dnsServer->start((byte)53, device_hostname_full, apIP);

  if (_apHandler) _apHandler();  
}//startApMode

void PersWiFiManager::setConnectNonBlock(bool b) {
  _connectNonBlock = b;
} //setConnectNonBlock

void PersWiFiManager::setupWiFiHandlers() {
  // note: removed DNS server setup here

  //_server->on("/", [&] () {
  //  _server->send(200, "text/html", FPSTR(HTML));
  //});
    
  _server->on("/wifi/list", [&] () {
    //scan for wifi networks
    int n = WiFi.scanNetworks();

    //build array of indices
    int ix[n];
    for (int i = 0; i < n; i++) ix[i] = i;

    //sort by signal strength
    for (int i = 0; i < n; i++) for (int j = 1; j < n - i; j++) if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1])) std::swap(ix[j], ix[j - 1]);
    //remove duplicates
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (WiFi.SSID(ix[i]).equals(WiFi.SSID(ix[j])) && WiFi.encryptionType(ix[i]) == WiFi.encryptionType(ix[j])) ix[j] = -1;

    //build plain text string of wifi info
    //format [signal%]:[encrypted 0 or 1]:SSID
    String s = "";
    s.reserve(2050);
    for (int i = 0; i < n && s.length() < 2000; i++) { //check s.length to limit memory usage
      if (ix[i] != -1) {
        s += String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2) + ","
             + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1) + "," + WiFi.SSID(ix[i]);
      }
    }

    //send string to client
    _server->send(200, "text/plain", s);
  }); //_server->on /wifi/list

  _server->on("/wifi/connect", [&]() {
    Serial.println("connecting");
    _server->send(200, "text/html", "connecting... please reset the ESP8266 in a few seconds");
    _server->handleClient();
    delay(100);
    attemptConnection(_server->arg("n"), _server->arg("p"));
    unsigned long to = millis();
    while ((WiFi.status() != WL_CONNECTED) && ((millis() - to) < 10000)) {
      delay(200);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nconnected");
      delay(100);
    }
    // module should restart
    ESP.restart();
  }); //_server->on /wifi/connect

  _server->on("/wifi/ap", [&](){
    _server->send(200, "text/html", "access point: "+getApSsid());
    startApMode();
  }); //_server->on /wifi/ap

  _server->on("/wifi/rst", [&]() {
    _server->send(200, "text/html", "Rebooting...");
    delay(100);
    ESP.restart();
  });

#ifdef WIFI_HTM_PROGMEM
  _server->on("/wifi.htm", [&]() {
    _server->send(200, "text/html", wifi_htm);
  });
#endif

}//setupWiFiHandlers

bool PersWiFiManager::begin(const String& ssid, const String& pass) {
  setupWiFiHandlers();
  return attemptConnection(ssid, pass); //switched order of these two for return
} //begin

String PersWiFiManager::getApSsid() {
  return _apSsid.length() ? _apSsid : "gbscontrol";
} //getApSsid

void PersWiFiManager::setApCredentials(const String& apSsid, const String& apPass) {
  if (apSsid.length()) _apSsid = apSsid;
  if (apPass.length() >= 8) _apPass = apPass;
} //setApCredentials

void PersWiFiManager::onConnect(WiFiChangeHandlerFunction fn) {
  _connectHandler = fn;
}

void PersWiFiManager::onAp(WiFiChangeHandlerFunction fn) {
  _apHandler = fn;
}
#endif
