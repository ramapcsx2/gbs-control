#if defined(ESP8266) || defined(ESP32)
#include "FS.h"
#if defined(ESP8266)
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> // install WiFiManager library by tzapu first!
extern "C" {
#include <user_interface.h>
}
#endif
#if defined(ESP32)
#include "SPIFFS.h"
#endif

const char* ap_ssid = "gbscontrol";
const char* ap_password =  "qqqqqqqq";

WiFiServer webserver(80);

const char HTML[] PROGMEM = "<head><link rel=\"icon\" href=\"data:,\"><style>html{font-family: 'Droid Sans', sans-serif;}h1{position: relative; margin-left: -22px; /* 15px padding + 7px border ribbon shadow*/ margin-right: -22px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff;}h1:before,h1:after{position: absolute; left: 0; bottom: -6px; content:''; border-top: 6px solid #555; border-left: 6px solid transparent;}h1:before{border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px;}.button{background-color: #008CBA; /* Blue */ border: none; border-radius: 12px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 28px; margin: 10px 10px; cursor: pointer; -webkit-transition-duration: 0.4s; /* Safari */ transition-duration: 0.4s; width: 440px; float: left;}.button:hover{background-color: #4CAF50;}.button a{color: white; text-decoration: none;}.button .tooltiptext{visibility: hidden; width: 100px; background-color: black; color: #fff; text-align: center; padding: 5px 0; border-radius: 6px; height: 12px; font-size: 10px; /* Position the tooltip */ position: absolute; z-index: 1; margin-left: 10px;}.button:hover .tooltiptext{visibility: visible;}br{clear: left;}h2{clear: left; padding-top: 10px;}</style><script>function loadDoc(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"serial_\"+link, true); xhttp.send();} function loadUser(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"user_\"+link, true); xhttp.send();}</script></head><body><h1>Standard-Presets</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('r')\">PAL Normal<span class=\"tooltiptext\">Serial r</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('2')\">PAL Feedback Clock<span class=\"tooltiptext\">Serial 2</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('e')\">NTSC Normal<span class=\"tooltiptext\">Serial e</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('9')\">NTSC Feedback Clock<span class=\"tooltiptext\">Serial 9</span></button><br><h1>Picture Control</h1><h2>Scaling</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('4')\">Vertical Larger<span class=\"tooltiptext\">Serial 4</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('5')\">Vertical Smaller<span class=\"tooltiptext\">Serial 5</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('z')\">Horizontal Larger<span class=\"tooltiptext\">Serial z</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('h')\">Horizontal Smaller<span class=\"tooltiptext\">Serial h</span></button><h2>Move Picture (Framebuffer)</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('*')\">Vertical Up<span class=\"tooltiptext\">Serial *</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('/')\">Vertical Down<span class=\"tooltiptext\">Serial /</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('-')\">Horizontal Left<span class=\"tooltiptext\">Serial -</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('+')\">Horizontal Right<span class=\"tooltiptext\">Serial +</span></button><h2>Move Picture (Blanking, Horizontal / Vertical Sync)</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('7')\">Move VS Up<span class=\"tooltiptext\">Serial 7</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('6')\">Move VS Down<span class=\"tooltiptext\">Serial 6</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('1')\">Move HS Left<span class=\"tooltiptext\">Serial 1</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('0')\">Move HS Right<span class=\"tooltiptext\">Serial 0</span></button><br><h1>En-/Disable</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('m')\">SyncWatcher<span class=\"tooltiptext\">Serial m</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('k')\">Passthrough<span class=\"tooltiptext\">Serial k</span></button><br><h1>Information</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('i')\">Print Infos<span class=\"tooltiptext\">Serial i</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc(',')\">Get Video Timings<span class=\"tooltiptext\">Serial ,</span></button><br><h1>Internal</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('o')\">Oversampling Ratio<span class=\"tooltiptext\">Serial o</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('E')\">ADC / SP Phase++<span class=\"tooltiptext\">Serial E</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('W')\">Set PhaseADC<span class=\"tooltiptext\">Serial W</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('Q')\">Set PhaseSP<span class=\"tooltiptext\">Serial Q</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('f')\">Show Noise<span class=\"tooltiptext\">Serial f</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('a')\">HTotal++<span class=\"tooltiptext\">Serial a</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('n')\">PLL divider++<span class=\"tooltiptext\">Serial n</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('b')\">Advance Phase<span class=\"tooltiptext\">Serial b</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('c')\">Enable OTA<span class=\"tooltiptext\">Serial c</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('8')\">Invert Sync<span class=\"tooltiptext\">Serial 8</span></button><br><h1>Reset</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('j')\">Reset PLL/PLLAD<span class=\"tooltiptext\">Serial j</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('q')\">Reset Chip<span class=\"tooltiptext\">Serial q</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('.')\">Reset SyncLock<span class=\"tooltiptext\">Serial .</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('l')\">Reset SyncProcessor<span class=\"tooltiptext\">Serial l</span></button><br><h1>Preferences</h1><button class=\"button\" type=\"button\" onclick=\"loadUser('1')\">Prefer Normal Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('0')\">Prefer Feedback Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('2')\">load custom preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('3')\">save custom preset</button></body>";

void start_webserver()
{
#if defined(ESP8266)
  WiFi.hostname("gbscontrol"); // not every router updates the old hostname though (mine doesn't)

  // hostname fix: spoof MAC by increasing the last octet by 1
  // Routers now see this as a new device and respect the hostname.
  uint8_t macAddr[6];
  Serial.print("orig. MAC: ");  Serial.println(WiFi.macAddress());
  WiFi.macAddress(macAddr); // macAddr now holds the current device MAC
  macAddr[5] += 1; // change last octet by 1
  wifi_set_macaddr(STATION_IF, macAddr);
  Serial.print("new MAC:   ");  Serial.println(WiFi.macAddress());

  WiFiManager wifiManager;
  wifiManager.setTimeout(180); // in seconds
  wifiManager.autoConnect(ap_ssid, ap_password);
  // The WiFiManager library will spawn an access point, waiting to be configured.
  // Once configured, it stores the credentials and restarts the board.
  // On restart, it tries to connect to the configured AP. If successfull, it resumes execution here.
  // Option: A timeout closes the configuration AP after 180 seconds, resuming gbs-control (but without any web server)
  Serial.print("dnsIP: "); Serial.println(WiFi.dnsIP());
  Serial.print("hostname: "); Serial.println(WiFi.hostname());
#elif defined(ESP32)
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("starting web server on: ");
  Serial.println(WiFi.softAPIP());
#endif

  WiFi.persistent(false); // this hopefully prevents repeated flash writes (SDK bug)
  // new WiFiManager library should improve this, update once it's out.

  // file system (web page, custom presets, ect)
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
  }

  webserver.begin();
}

void initUpdateOTA() {
  ArduinoOTA.setHostname("GBS OTA");

  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, uint16_t length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}

uint8_t* loadPresetFromSPIFFS() {
  static uint8_t preset[592];
  String s = "";
  Serial.print("free ram before: "); Serial.println(ESP.getFreeHeap());
  File f = SPIFFS.open("/preset0.txt", "r");
  if (!f) {
    Serial.println("open preset file failed");
  }
  else {
    Serial.println("preset0.txt ok");
    s = f.readStringUntil('}');
    f.close();
  }

  char *tmp;
  uint16_t i = 0;
  tmp = strtok(&s[0], ",");
  while (tmp) {
    preset[i++] = (uint8_t)atoi(tmp);
    tmp = strtok(NULL, ",");
  }
  Serial.print("free ram after read: "); Serial.println(ESP.getFreeHeap());

  return preset;
}

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ 500 // we have the RAM
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
uint16_t req_index = 0; // index into HTTP_req buffer

void handleWebClient()
{
  WiFiClient client = webserver.available();
  if (client) {
    //Serial.println("New client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    uint16_t client_timeout = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // limit the size of the stored received HTTP request
        // buffer first part of HTTP request in HTTP_req array (string)
        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;   // save one request character
          req_index++;
        }

        // last line of client request is blank and ends with \n
        // respond to client only after last line received
        if (c == '\n' && currentLineIsBlank) {
          // display received HTTP request on serial port
          //Serial.println("HTTP Request: "); Serial.print(HTTP_req); Serial.println();

          if (strstr(HTTP_req, "Accept: */*")) { // this is a xhttp request, no need to send the whole page again
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: close");
            client.println();
          }
          else {
            // send standard http response header ..
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            // .. and our page
            client.println("<!DOCTYPE HTML><html>");
            client.println(FPSTR(HTML)); // FPSTR(HTML) reads 32bit aligned
            client.println("</html>");
          }
          // reset buffer index and all buffer elements to 0
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SZ);
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          if (strstr(HTTP_req, "GET /serial_")) {
            //Serial.print("HTTP_req[12]: "); Serial.println(HTTP_req[12]);
            globalCommand = HTTP_req[12];
            // reset buffer index and all buffer elements to 0 (we don't care about the rest)
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
          }
          else if (strstr(HTTP_req, "GET /user_")) {
            //Serial.print("HTTP_req[10]: "); Serial.println(HTTP_req[10]);
            if (HTTP_req[10] == '0') {
              uopt->preferFeedbackClockPresets = true;
            }
            else if (HTTP_req[10] == '1') {
              uopt->preferFeedbackClockPresets = false;
            }
            else if (HTTP_req[10] == '2') {
              uint8_t* preset = loadPresetFromSPIFFS();
              writeProgramArrayNew(preset);
              doPostPresetLoadSteps();
            }
            else if (HTTP_req[10] == '3') {
              // save
              // maybe insert a flag / cheap crc on first line, check that on load
            }

            // reset buffer index and all buffer elements to 0 (we don't care about the rest)
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
          }
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } else {
#if defined(ESP8266)
        if (client.status() == 4) {
          client_timeout++;
        }
        else {
          Serial.print("client status: "); Serial.println(client.status());
        }
#endif
        if (client_timeout > 10000) {
          //Serial.println("This socket's dead, Jim");
          break;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    //Serial.println("client disconnected");
  }
}
#endif
