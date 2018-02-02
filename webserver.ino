#if defined(ESP8266) || defined(ESP32)
const char* ssid = "gbs";
const char* password =  "qqqqqqqq";

WiFiServer server(80);

const char HTML[] PROGMEM = "<head><style>html{font-family: 'Droid Sans', sans-serif;}h1{position: relative; margin-left: -22px; /* 15px padding + 7px border ribbon shadow*/ margin-right: -22px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff;}h1:before,h1:after{position: absolute; left: 0; bottom: -6px; content:''; border-top: 6px solid #555; border-left: 6px solid transparent;}h1:before{border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px;}.button{background-color: #008CBA; /* Blue */ border: none; border-radius: 12px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; -webkit-transition-duration: 0.4s; /* Safari */ transition-duration: 0.4s; width: 250px; float: left;}.button:hover{background-color: #4CAF50;}.button a{color: white; text-decoration: none;}.button .tooltiptext{visibility: hidden; width: 100px; background-color: black; color: #fff; text-align: center; padding: 5px 0; border-radius: 6px; height: 12px; font-size: 10px; /* Position the tooltip */ position: absolute; z-index: 1; margin-left: 10px;}.button:hover .tooltiptext{visibility: visible;}br{clear: left;}h2{clear: left; padding-top: 10px;}</style><script>function loadDoc(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"http://192.168.4.1/serial_\"+link, true); xhttp.send();}</script></head><body><h1>Standard-Presets</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('9')\">NTSC Feedback Clock<span class=\"tooltiptext\">Serial 9</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('2')\">PAL Feedback Clock<span class=\"tooltiptext\">Serial 2</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('3')\">YPBPR (OFW)<span class=\"tooltiptext\">Serial 3</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('r')\">PAL 240p<span class=\"tooltiptext\">Serial r</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('e')\">NTSC 240p<span class=\"tooltiptext\">Serial e</span></button><br><h1>Picture Control</h1><h2>Scaling</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('5')\">Vertical Smaller<span class=\"tooltiptext\">Serial 5</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('4')\">Vertical Larger<span class=\"tooltiptext\">Serial 4</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('h')\">Horizontal Smaller<span class=\"tooltiptext\">Serial h</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('z')\">Horizontal Larger<span class=\"tooltiptext\">Serial z</span></button><h2>Movement</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('*')\">Vertical Up<span class=\"tooltiptext\">Serial *</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('/')\">Vertical Down<span class=\"tooltiptext\">Serial /</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('-')\">Horizontal Left<span class=\"tooltiptext\">Serial -</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('+')\">Horizontal Right<span class=\"tooltiptext\">Serial +</span></button><h2>Blanking</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('0')\">Move HS Left<span class=\"tooltiptext\">Serial 0</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('1')\">Move HS Right<span class=\"tooltiptext\">Serial 1</span></button><br><h1>En-/Disable</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('m')\">SyncWatcher<span class=\"tooltiptext\">Serial m</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('.')\">SyncLock<span class=\"tooltiptext\">Serial .</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('k')\">Passthrough<span class=\"tooltiptext\">Serial k</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('c')\">Auto Gain ADC<span class=\"tooltiptext\">Serial c</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('l')\">SyncProcessor<span class=\"tooltiptext\">Serial l</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('v')\">SyncProcessor (fuzzySPWrite)<span class=\"tooltiptext\">Serial v</span></button><br><h1>Parameters</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('t')\">Toggle Bit<span class=\"tooltiptext\">Serial t</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('s')\">Set Value<span class=\"tooltiptext\">Serial s</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('g')\">Get Value<span class=\"tooltiptext\">Serial g</span></button><br><h1>Informations</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('i')\">Print Infos<span class=\"tooltiptext\">Serial i</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc(',')\">Get Video Timings<span class=\"tooltiptext\">Serial ,</span></button><br><h1>Internal</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('x')\">ADC Target<span class=\"tooltiptext\">Serial x</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('w')\">w<span class=\"tooltiptext\">Serial w</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('o')\">Oversampling Ratio<span class=\"tooltiptext\">Serial o</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('8')\">ADC + / SP +<span class=\"tooltiptext\">Serial 8</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('7')\">Set PhaseADC<span class=\"tooltiptext\">Serial 7</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('6')\">Set PhaseSP<span class=\"tooltiptext\">Serial 6</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('f')\">Show Noise<span class=\"tooltiptext\">Serial f</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('a')\">HTotal++<span class=\"tooltiptext\">Serial a</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('n')\">PLL divider<span class=\"tooltiptext\">Serial n</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('b')\">Advance Phase<span class=\"tooltiptext\">Serial b</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('d')\">Dump Registers<span class=\"tooltiptext\">Serial d</span></button><br><h1>Reset</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('j')\">Reset PLL/PLLAD<span class=\"tooltiptext\">Serial j</span></button><button class=\"button\" type=\"button\" onclick=\"loadDoc('q')\">Reset Digital<span class=\"tooltiptext\">Serial q</span></button></body>";

char linebuf[80];
int charcount = 0;

void start_webserver()
{
  WiFi.softAP(ssid, password);
  Serial.print("starting web server on: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}

void setPreset(const uint8_t* programArray)
{
  writeProgramArrayNew(programArray);
  doPostPresetLoadSteps();
}

void syncWatcher()
{
  if (rto->syncWatcher == true) {
    rto->syncWatcher = false;
    Serial.println(F("off"));
  }
  else {
    rto->syncWatcher = true;
    Serial.println(F("on"));
  }
}

void passthrough()
{
  static uint8_t readout = 0;
  static boolean sp_passthrough_enabled = false;
  if (!sp_passthrough_enabled) {
    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout | (1 << 7));
    // clock output (for measurment)
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout | (1 << 4));
    readFromRegister(0x49, 1, &readout);
    writeOneByte(0x49, readout & ~(1 << 1));

    sp_passthrough_enabled = true;
  }
  else {
    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));
    sp_passthrough_enabled = false;
  }
}

void autoGainADC()
{
  if (rto->autoGainADC == true) {
    rto->autoGainADC = false;
    resetADCAutoGain();
    Serial.println(F("off"));
  }
  else {
    rto->autoGainADC = true;
    resetADCAutoGain();
    Serial.println(F("on"));
  }
}

void handleWebClient()
{
  WiFiClient client = server.available();
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

          // client.println(HTML) segfaults on ESP8266 due to not being 32bit aligned. Use FPSTR(HTML) instead.
          client.println(FPSTR(HTML));

          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
          if (strstr(linebuf, "GET /serial_") > 0) {
            Serial.print("Linebuf[12]: "); Serial.println(linebuf[1]);
            globalCommand = linebuf[12];
          }
          // you're starting a new line
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
#endif
