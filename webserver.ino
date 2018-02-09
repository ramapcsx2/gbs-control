#if defined(ESP8266) || defined(ESP32)
#if defined(ESP8266)
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> // install WiFiManager library by tzapu first!
#endif

const char* ap_ssid = "gbs";
const char* ap_password =  "qqqqqqqq";

WiFiServer webserver(80);

const char HTML[] PROGMEM = "<head><style>html{font-family: 'Droid Sans', sans-serif;}h1{position: relative; margin-left: -22px; /* 15px padding + 7px border ribbon shadow*/ margin-right: -22px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff;}h1:before,h1:after{position: absolute; left: 0; bottom: -6px; content:''; border-top: 6px solid #555; border-left: 6px solid transparent;}h1:before{border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px;}.button{background-color: #008CBA; /* Blue */ border: none; border-radius: 12px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 28px; margin: 10px 10px; cursor: pointer; -webkit-transition-duration: 0.4s; /* Safari */ transition-duration: 0.4s; width: 440px; float: left;}.button:hover{background-color: #4CAF50;}.button a{color: white; text-decoration: none;}.button .tooltiptext{visibility: hidden; width: 100px; background-color: black; color: #fff; text-align: center; padding: 5px 0; border-radius: 6px; height: 12px; font-size: 10px; /* Position the tooltip */ position: absolute; z-index: 1; margin-left: 10px;}.button:hover .tooltiptext{visibility: visible;}br{clear: left;}h2{clear: left; padding-top: 10px;}</style><script>function loadDoc(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"serial_\"+link, true); xhttp.send();}</script></head><body><h1>Standard-Presets</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('r')\">PAL 240p<span class=\"tooltiptext\">Serial r</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('e')\">NTSC 240p<span class=\"tooltiptext\">Serial e</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('2')\">PAL Feedback Clock<span class=\"tooltiptext\">Serial 2</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('9')\">NTSC Feedback Clock<span class=\"tooltiptext\">Serial 9</span></button><br><h1>Picture Control</h1><h2>Scaling</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('5')\">Vertical Smaller<span class=\"tooltiptext\">Serial 5</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('4')\">Vertical Larger<span class=\"tooltiptext\">Serial 4</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('h')\">Horizontal Smaller<span class=\"tooltiptext\">Serial h</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('z')\">Horizontal Larger<span class=\"tooltiptext\">Serial z</span></button><h2>Movement</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('*')\">Vertical Up<span class=\"tooltiptext\">Serial *</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('/')\">Vertical Down<span class=\"tooltiptext\">Serial /</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('-')\">Horizontal Left<span class=\"tooltiptext\">Serial -</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('+')\">Horizontal Right<span class=\"tooltiptext\">Serial +</span></button><h2>Blanking</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('0')\">Move HS Left<span class=\"tooltiptext\">Serial 0</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('1')\">Move HS Right<span class=\"tooltiptext\">Serial 1</span></button><br><h1>En-/Disable</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('m')\">SyncWatcher<span class=\"tooltiptext\">Serial m</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('.')\">SyncLock<span class=\"tooltiptext\">Serial .</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('k')\">Passthrough<span class=\"tooltiptext\">Serial k</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('l')\">SyncProcessor<span class=\"tooltiptext\">Serial l</span></button><br><h1>Informations</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('i')\">Print Infos<span class=\"tooltiptext\">Serial i</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc(',')\">Get Video Timings<span class=\"tooltiptext\">Serial ,</span></button><br><h1>Internal</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('o')\">Oversampling Ratio<span class=\"tooltiptext\">Serial o</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('8')\">ADC / SP Phase++<span class=\"tooltiptext\">Serial 8</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('7')\">Set PhaseADC<span class=\"tooltiptext\">Serial 7</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('6')\">Set PhaseSP<span class=\"tooltiptext\">Serial 6</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('f')\">Show Noise<span class=\"tooltiptext\">Serial f</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('a')\">HTotal++<span class=\"tooltiptext\">Serial a</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('n')\">PLL divider++<span class=\"tooltiptext\">Serial n</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('b')\">Advance Phase<span class=\"tooltiptext\">Serial b</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('c')\">Enable OTA<span class=\"tooltiptext\">Serial c</span></button><br><h1>Reset</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('j')\">Reset PLL/PLLAD<span class=\"tooltiptext\">Serial j</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('q')\">Reset Digital<span class=\"tooltiptext\">Serial q</span></button></body>";

void start_webserver()
{
#if defined(ESP8266)
  WiFiManager wifiManager;
  wifiManager.setTimeout(180); // in seconds
  wifiManager.autoConnect(ap_ssid, ap_password);
  // The WiFiManager library will spawn an access point, waiting to be configured.
  // Once configured, it stores the credentials and restarts the board.
  // On restart, it tries to connect to the configured AP. If successfull, it resumes execution here.
  // Option: A timeout closes the configuration AP after 120 seconds, resuming gbs-control (but without any web server)
#else if defined(ESP32)
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("starting web server on: ");
  Serial.println(WiFi.softAPIP());
#endif

  WiFi.persistent(false); // this hopefully prevents repeated flash writes (SDK bug)
  // new WiFiManager library should improve this, update once it's out.

  webserver.begin();
}

void handleWebClient()
{
  char linebuf[160]; // was 80 but that's too small for some user agent strings
  uint16_t charcount = 0;

  WiFiClient client = webserver.available();
  if (client) {
    Serial.println("New client");
    memset(linebuf, 0, sizeof(linebuf));
    charcount = 0;
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        //read char by char HTTP request
        linebuf[charcount] = c;
        if (charcount < sizeof(linebuf) - 1) charcount++;

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println();
          client.println("<!DOCTYPE HTML><html>");
          client.println(FPSTR(HTML)); // client.println(HTML) segfaults on ESP8266 due to not being 32bit aligned. Use FPSTR(HTML) instead.
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          if (strstr(linebuf, "GET /serial_") /* > 0 */) { // strstr returns a pointer or null, comparing with 0 threw a warning
            Serial.print("Linebuf[12]: "); Serial.println(linebuf[12]);
            globalCommand = linebuf[12];
          }
          currentLineIsBlank = true;
          memset(linebuf, 0, sizeof(linebuf));
          charcount = 0;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
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
#endif
