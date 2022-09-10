/*
SPIFFS-served REST API example for PersWiFiManager v3.0
*/

#define DEBUG_SERIAL //uncomment for Serial debugging statements

#ifdef DEBUG_SERIAL
#define DEBUG_BEGIN Serial.begin(115200)
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_BEGIN
#endif

//includes
#include <PersWiFiManager.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include <EasySSDP.h> // http://ryandowning.net/EasySSDP/

//extension of ESP8266WebServer with SPIFFS handlers built in
#include <SPIFFSReadServer.h> // http://ryandowning.net/SPIFFSReadServer/
// upload data folder to chip with Arduino ESP8266 filesystem uploader
// https://github.com/esp8266/arduino-esp8266fs-plugin

#include <DNSServer.h>
#include <FS.h>

#define DEVICE_NAME "ESP8266 DEVICE"


//server objects
SPIFFSReadServer server(80);
DNSServer dnsServer;
PersWiFiManager persWM(server, dnsServer);

////// Sample program data
int x;
String y;

void setup() {
  DEBUG_BEGIN; //for terminal debugging
  DEBUG_PRINT();
  
  //optional code handlers to run everytime wifi is connected...
  persWM.onConnect([]() {
    DEBUG_PRINT("wifi connected");
    DEBUG_PRINT(WiFi.localIP());
    EasySSDP::begin(server);
  });
  //...or AP mode is started
  persWM.onAp([](){
    DEBUG_PRINT("AP MODE");
    DEBUG_PRINT(persWM.getApSsid());
  });

  //allows serving of files from SPIFFS
  SPIFFS.begin();
  //sets network name for AP mode
  persWM.setApCredentials(DEVICE_NAME);
  //persWM.setApCredentials(DEVICE_NAME, "password"); optional password

  //make connecting/disconnecting non-blocking
  persWM.setConnectNonBlock(true);

  //in non-blocking mode, program will continue past this point without waiting
  persWM.begin();

  //handles commands from webpage, sends live data in JSON format
  server.on("/api", []() {
    DEBUG_PRINT("server.on /api");
    if (server.hasArg("x")) {
      x = server.arg("x").toInt();
      DEBUG_PRINT(String("x: ") + x);
    } //if
    if (server.hasArg("y")) {
      y = server.arg("y");
      DEBUG_PRINT("y: " + y);
    } //if

    //build json object of program data
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["x"] = x;
    json["y"] = y;

    char jsonchar[200];
    json.printTo(jsonchar); //print to char array, takes more memory but sends in one piece
    server.send(200, "application/json", jsonchar);

  }); //server.on api


  server.begin();
  DEBUG_PRINT("setup complete.");
} //void setup

void loop() {
  //in non-blocking mode, handleWiFi must be called in the main loop
  persWM.handleWiFi();

  dnsServer.processNextRequest();
  server.handleClient();

  //DEBUG_PRINT(millis());

  // do stuff with x and y

} //void loop

