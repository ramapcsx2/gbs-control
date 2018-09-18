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
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define WIFI_CONNECT_TIMEOUT 30

const char HTML[] PROGMEM = "<!DOCTYPE HTML><html lang=\"en\"><head><meta charset='utf-8'/><link rel='icon' href='data:,'><title>gbscontrol</title><style>html { font-family: 'Droid Sans', sans-serif; } h1 { position: relative; margin-left: -8px; margin-right: -8px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff; } h1:before,h1:after { position: absolute; left: 0; bottom: -6px; border-top: 6px solid #555; border-left: 6px solid transparent; } h1:before { border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px; } .button { background-color: #008CBA; border: none; border-radius: 12px; color: white; text-align: center; text-decoration: none; display: inline-block; font-size: larger; margin: 10px 10px; cursor: pointer; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; width: 36vw; height: 8vw; max-width: 360px; max-height: 100px; float: left; } .button:hover { background-color: #4CAF50; } .button a { color: white; text-decoration: none; } br { clear: left; } h2 { clear: left; padding-top: 10px; }</style><script>var eventsPrinted; var theTerminal; var connection = null; window.addEventListener('load', init, false); function loadDoc(link) { var xhttp = new XMLHttpRequest(); xhttp.open('POST', 'serial_', true); xhttp.send(link); } function loadUser(link) { var xhttp = new XMLHttpRequest(); xhttp.open('POST', 'user_', true); xhttp.send(link); } function checkConnection() { if (!connection) { createWebSocket(); } else if (connection.readyState == 3) { connection = null; } else if (connection.readyState == 1) { connection.send(''); } } function init() { theTerminal = document.getElementById('outputTextArea'); createWebSocket(); setInterval(checkConnection, 1000); eventsPrinted = 0; } function createWebSocket() { connection = new WebSocket('ws://' + location.hostname + ':81/',['arduino']); connection.onopen = function() {} ; connection.onerror = function(error) { checkConnection(); } ; connection.onmessage = function(e) { theTerminal.value += e.data; eventsPrinted++; if (eventsPrinted >= 400) { theTerminal.value = ''; eventsPrinted = 0; } theTerminal.scrollTop = theTerminal.scrollHeight; } ; connection.onclose = function() { console.log('WebSocket reconnecting..'); checkConnection(); } ; }</script></head><body><h1>Load Preset</h1><button class='button' type='button' onclick=\"loadUser('f')\">1280x960 (default)</button><button class='button' type='button' onclick=\"loadUser('p')\">1280x1024</button><button class='button' type='button' onclick=\"loadUser('g')\">1280x720</button><button class='button' type='button' onclick=\"loadUser('h')\">640x480</button><button class='button' type='button' onclick=\"loadUser('3')\">Custom Preset</button><br><h1>Picture Control</h1><h2>Move Picture</h2><button class='button' type='button' onclick=\"loadDoc('*')\">Picture Up</button><button class='button' type='button' onclick=\"loadDoc('/')\">Picture Down</button><button class='button' type='button' onclick=\"loadDoc('7')\">Picture Left</button><button class='button' type='button' onclick=\"loadDoc('6')\">Picture Right</button><h2>Scaling</h2><button class='button' type='button' onclick=\"loadDoc('4')\">Vertical Larger</button><button class='button' type='button' onclick=\"loadDoc('5')\">Vertical Smaller</button><button class='button' type='button' onclick=\"loadDoc('z')\">Horizontal Larger</button><button class='button' type='button' onclick=\"loadDoc('h')\">Horizontal Smaller</button><h2>Mask Setting</h2><button class='button' type='button' onclick=\"loadUser('A')\">H. Mask +</button><button class='button' type='button' onclick=\"loadUser('B')\">H. Mask -</button><button class='button' type='button' onclick=\"loadUser('C')\">V. Mask +</button><button class='button' type='button' onclick=\"loadUser('D')\">V. Mask -</button><h2>ADC Gain (brightness)</h2><button class='button' type='button' onclick=\"loadUser('n')\">More</button><button class='button' type='button' onclick=\"loadUser('o')\">Less</button><button class='button' type='button' onclick=\"loadDoc('T')\">Automatic Adjust Toggle</button><br><h1>En-/Disable</h1><button class='button' type='button' onclick=\"loadUser('7')\">Scanlines Toggle</button><button class='button' type='button' onclick=\"loadUser('m')\">DCTI (pixel edge enhance)</button><button class='button' type='button' onclick=\"loadDoc('K')\">Source Pass-through (SyncLock)</button><button class='button' type='button' onclick=\"loadDoc('C')\">H-PLL as clock source</button><br><h1>Preferences</h1><h2>Load on Start: Default Presets</h2><button class='button' type='button' onclick=\"loadUser('0')\">Prefer default 1280x960 Preset</button><button class='button' type='button' onclick=\"loadUser('2')\">Prefer default 1280x1024 Preset</button><button class='button' type='button' onclick=\"loadUser('9')\">Prefer default 1280x720 Preset</button><button class='button' type='button' onclick=\"loadUser('1')\">Prefer default 640x480 Preset</button><h2>Load on Start: Custom Presets</h2><button class='button' type='button' onclick=\"loadUser('b')\">Prefer Custom Preset Group 0</button><button class='button' type='button' onclick=\"loadUser('c')\">Prefer Custom Preset Group 1</button><button class='button' type='button' onclick=\"loadUser('d')\">Prefer Custom Preset Group 2</button><button class='button' type='button' onclick=\"loadUser('j')\">Prefer Custom Preset Group 3</button><button class='button' type='button' onclick=\"loadUser('k')\">Prefer Custom Preset Group 4</button><h2>Save Preset in selected Group</h2><button class='button' type='button' onclick=\"loadUser('4')\">Save Custom Preset</button><br><h2>Active FrameTime Lock (not compatible with all displays)</h2><button class='button' type='button' onclick=\"loadUser('5')\">FrameTime Lock ON</button><button class='button' type='button' onclick=\"loadUser('6')\">FrameTime Lock OFF</button><button class='button' type='button' onclick=\"loadUser('i')\">Switch Lock Method</button><br><h1>Development</h1><h2>Move Picture (memory blank, HS)</h2><button class='button' type='button' onclick=\"loadDoc('-')\">Left</button><button class='button' type='button' onclick=\"loadDoc('+')\">Right</button><button class='button' type='button' onclick=\"loadDoc('1')\">HS Left</button><button class='button' type='button' onclick=\"loadDoc('0')\">HS Right</button><br><h2>Information</h2><button class='button' type='button' onclick=\"loadDoc('i')\">Print Infos</button><button class='button' type='button' onclick=\"loadDoc(',')\">Get Video Timings</button><br><h2>Reset</h2><button class='button' type='button' onclick=\"loadDoc('j')\">Reset PLL/PLLAD</button><button class='button' type='button' onclick=\"loadDoc('q')\">Reset Chip</button><button class='button' type='button' onclick=\"loadDoc('.')\">Reset SyncLock</button><button class='button' type='button' onclick=\"loadDoc('l')\">Reset SyncProcessor</button><br><h2>Commands</h2><button class='button' type='button' onclick=\"loadDoc('D')\">Debug View</button><button class='button' type='button' onclick=\"loadDoc('f')\">Peaking</button><button class='button' type='button' onclick=\"loadDoc('F')\">ADC Filter</button><button class='button' type='button' onclick=\"loadDoc('o')\">Oversampling</button><button class='button' type='button' onclick=\"loadDoc('a')\">HTotal++</button><button class='button' type='button' onclick=\"loadDoc('n')\">PLL divider++</button><button class='button' type='button' onclick=\"loadDoc('b')\">Advance Phase</button><button class='button' type='button' onclick=\"loadDoc('8')\">Invert Sync</button><button class='button' type='button' onclick=\"loadDoc('c')\">Enable OTA</button><button class='button' type='button' onclick=\"loadDoc('m')\">SyncWatcher</button><br><h1>System</h1><button class='button' type='button' onclick=\"javascript:location.href='/wifi.htm'\">Connect to WiFi Network</button><button class='button' type='button' onclick=\"loadUser('a')\">Reset MCU</button><button class='button' type='button' onclick=\"loadUser('e')\">List Files, Options</button><button class='button' type='button' onclick=\"loadUser('l')\">Cycle SDRAM clock speed</button><br><form><textarea id='outputTextArea' readonly style='width:1000px; height:400px; font-size: large'></textarea></form></body></html>";

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
#endif
