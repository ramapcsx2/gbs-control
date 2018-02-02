#if defined(ESP8266) || defined(ESP32)
const char* ssid = "gbs";
const char* password =  "qqqqqqqq";

WiFiServer server(80);

const char HTML[] PROGMEM = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>html{font-family: 'Droid Sans', sans-serif;}h1{position: relative; margin-left: -22px; /* 15px padding + 7px border ribbon shadow*/ margin-right: -22px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff;}h1:before,h1:after{position: absolute; left: 0; bottom: -6px; content:''; border-top: 6px solid #555; border-left: 6px solid transparent;}h1:before{border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px;}.button{background-color: #008CBA; /* Blue */ border: none; border-radius: 12px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; -webkit-transition-duration: 0.4s; /* Safari */ transition-duration: 0.4s; width: 250px; float: left;}.button:hover{background-color: #4CAF50;}.button a{color: white; text-decoration: none;}a.button .tooltiptext{visibility: hidden; width: 100px; background-color: black; color: #fff; text-align: center; padding: 5px 0; border-radius: 6px; height: 12px; font-size: 10px; /* Position the tooltip */ position: absolute; z-index: 1; margin-left: 10px;}a.button:hover .tooltiptext{visibility: visible;}br{clear: left;}h2{clear: left; padding-top: 10px;}</style></head><body><h1>Standard-Presets</h1><a class=\"button\" href=\"/ntscFeedbackClock\">NTSC Feedback Clock<span class=\"tooltiptext\">Serial 9</span></a><a class=\"button\" href=\"/palFeedbackClock\">PAL Feedback Clock<span class=\"tooltiptext\">Serial 2</span></a><a class=\"button\" href=\"/ofw_ypbpr\">YPBPR (OFW)<span class=\"tooltiptext\">Serial 3</span></a><a class=\"button\" href=\"/pal240p\">PAL 240p<span class=\"tooltiptext\">Serial r</span></a><a class=\"button\" href=\"/ntsc240p\">NTSC 240p<span class=\"tooltiptext\">Serial e</span></a><br><h1>Picture Control</h1><h2>Scaling</h2><a class=\"button\" href=\"/scaleVerticalSmaller\">Vertical Smaller<span class=\"tooltiptext\">Serial 4</span></a><a class=\"button\" href=\"/scaleVerticalLarger\">Vertical Larger<span class=\"tooltiptext\">Serial 5</span></a><a class=\"button\" href=\"/scaleHorizontalSmaller\">Horizontal Smaller<span class=\"tooltiptext\">Serial h</span></a><a class=\"button\" href=\"/scaleHorizontalLarger\">Horizontal Larger<span class=\"tooltiptext\">Serial z</span></a><h2>Movement</h2><a class=\"button\" href=\"/shiftVerticalUp\">Vertical Up<span class=\"tooltiptext\">Serial *</span></a><a class=\"button\" href=\"/shiftVerticalDown\">Vertical Down<span class=\"tooltiptext\">Serial /</span></a><a class=\"button\" href=\"/shiftHorizontalLeft\">Horizontal Left<span class=\"tooltiptext\">Serial -</span></a><a class=\"button\" href=\"/shiftHorizontalRight\">Horizontal Right<span class=\"tooltiptext\">Serial +</span></a><h2>Blanking</h2><a class=\"button\" href=\"/moveHSLeft\">Move HS Left<span class=\"tooltiptext\">Serial 0</span></a><a class=\"button\" href=\"/moveHSRight\">Move HS Right<span class=\"tooltiptext\">Serial 1</span></a><br><h1>En-/Disable</h1><a class=\"button\" href=\"/syncWatcher\">SyncWatcher<span class=\"tooltiptext\">Serial m</span></a><a class=\"button\" href=\"/syncLock\">SyncLock<span class=\"tooltiptext\">Serial .</span></a><a class=\"button\" href=\"/passthrough\">Passthrough<span class=\"tooltiptext\">Serial k</span></a><a class=\"button\" href=\"/autoGainADC\">Auto Gain ADC<span class=\"tooltiptext\">Serial c</span></a><a class=\"button\" href=\"/SyncProcessorOffOn\">SyncProcessor<span class=\"tooltiptext\">Serial l</span></a><a class=\"button\" href=\"/syncProcessor\">SyncProcessor (fuzzySPWrite)<span class=\"tooltiptext\">Serial v</span></a><br><h1>Parameters</h1><a class=\"button\" href=\"/toggleBit\">Toggle Bit<span class=\"tooltiptext\">Serial t</span></a><a class=\"button\" href=\"/setValue\">Set Value<span class=\"tooltiptext\">Serial s</span></a><a class=\"button\" href=\"/getValue\">Get Value<span class=\"tooltiptext\">Serial g</span></a><br><h1>Informations</h1><a class=\"button\" href=\"/printInfos\">Print Infos<span class=\"tooltiptext\">Serial i</span></a><a class=\"button\" href=\"/getVideoTimings\">Get Video Timings<span class=\"tooltiptext\">Serial ,</span></a><br><h1>Internal</h1><a class=\"button\" href=\"/ADC_Target\">ADC Target<span class=\"tooltiptext\">Serial x</span></a><a class=\"button\" href=\"/w\">w <span class=\"tooltiptext\">Serial w</span></a><a class=\"button\" href=\"/oversamplingRatio\">Oversampling Ratio<span class=\"tooltiptext\">Serial o</span></a><a class=\"button\" href=\"/setPhaseADC_PhaseSP\">ADC + / SP +<span class=\"tooltiptext\">Serial 8</span></a><a class=\"button\" href=\"/setPhaseADC\">Set PhaseADC<span class=\"tooltiptext\">Serial 7</span></a><a class=\"button\" href=\"/setPhaseSP\">Set PhaseSP<span class=\"tooltiptext\">Serial 6</span></a><a class=\"button\" href=\"/showNoise\">Show Noise<span class=\"tooltiptext\">Serial f</span></a><a class=\"button\" href=\"/htotal\">HTotal++<span class=\"tooltiptext\">Serial a</span></a><a class=\"button\" href=\"/pllDivider\">PLL divider<span class=\"tooltiptext\">Serial n</span></a><a class=\"button\" href=\"/advancePhase\">Advance Phase<span class=\"tooltiptext\">Serial b</span></a><a class=\"button\" href=\"/dumpRegisters\">Dump Registers<span class=\"tooltiptext\">Serial d</span></a><br><h1>Reset</h1><a class=\"button\" href=\"/resetPLL\">Reset PLL/PLLAD<span class=\"tooltiptext\">Serial j</span></a><a class=\"button\" href=\"/resetDigital\">Reset Digital<span class=\"tooltiptext\">Serial q</span></a></body>";

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
          if (strstr(linebuf, "GET /palFeedbackClock") > 0) {
            //setPreset(pal_feedbackclock);
            globalCommand = '2';
          }
          else if (strstr(linebuf, "GET /pal240p") > 0) {
            setPreset(pal_240p);
          }
          else if (strstr(linebuf, "GET /ntscFeedbackClock") > 0) {
            setPreset(ntsc_feedbackclock);
          }
          else if (strstr(linebuf, "GET /ntsc240p") > 0) {
            setPreset(ntsc_240p);
          }
          else if (strstr(linebuf, "GET /ofw_ypbpr") > 0) {
            setPreset(ofw_ypbpr);
          }
          else if (strstr(linebuf, "GET /scaleVerticalSmaller") > 0) {
            scaleVertical(1, false);
          }
          else if (strstr(linebuf, "GET /scaleVerticalLarger") > 0) {
            scaleVertical(1, true);
          }
          else if (strstr(linebuf, "GET /scaleHorizontalSmaller") > 0) {
            scaleHorizontalSmaller();
          }
          else if (strstr(linebuf, "GET /scaleHorizontalLarger") > 0) {
            scaleHorizontalLarger();
          }
          else if (strstr(linebuf, "GET /shiftVerticalUp") > 0) {
            shiftVerticalUp();
          }
          else if (strstr(linebuf, "GET /shiftVerticalDown") > 0) {
            shiftVerticalDown();
          }
          else if (strstr(linebuf, "GET /shiftHorizontalLeft") > 0) {
            shiftHorizontalLeft();
          }
          else if (strstr(linebuf, "GET /shiftHorizontalRight") > 0) {
            shiftHorizontalRight();
          }
          else if (strstr(linebuf, "GET /moveHSLeft") > 0) {
            moveHS(1, true);
          }
          else if (strstr(linebuf, "GET /moveHSRight") > 0) {
            moveHS(1, false);
          }
          else if (strstr(linebuf, "GET /syncWatcher") > 0) {
            syncWatcher();
          }
          else if (strstr(linebuf, "GET /syncLock") > 0) {
            rto->syncLockFound = !rto->syncLockFound;
          }
          else if (strstr(linebuf, "GET /passthrough") > 0) {
            passthrough();
          }
          else if (strstr(linebuf, "GET /autoGainADC") > 0) {
            autoGainADC();
          }
          else if (strstr(linebuf, "GET /SyncProcessorOffOn") > 0) {
            SyncProcessorOffOn();
          }
          else if (strstr(linebuf, "GET /syncProcessor") > 0) {
            fuzzySPWrite();
            SyncProcessorOffOn();
          }
          else if (strstr(linebuf, "GET /toggleBit") > 0) {
          }
          else if (strstr(linebuf, "GET /setValue") > 0) {
          }
          else if (strstr(linebuf, "GET /getValue") > 0) {
          }
          else if (strstr(linebuf, "GET /printInfos") > 0) {
            rto->printInfos = !rto->printInfos;
          }
          else if (strstr(linebuf, "GET /getVideoTimings") > 0) {
            getVideoTimings();
          }
          else if (strstr(linebuf, "GET /resetPLL") > 0) {
            resetPLL();
            resetPLLAD();
          }
          else if (strstr(linebuf, "GET /resetDigital") > 0) {
            resetDigital();
            enableVDS();
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
