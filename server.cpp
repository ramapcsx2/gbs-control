/*
#####################################################################################
# File: server.cpp                                                                  #
# File Created: Friday, 19th April 2024 3:11:40 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 25th April 2024 3:50:47 pm                               #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#include "server.h"

/**
 * @brief Initializer
 *
 */
void serverInit()
{
    server.on(F("/"), HTTP_GET, serverHome);
    server.on(F("/sc"), HTTP_GET, serverSC);
    server.on(F("/uc"), HTTP_GET, serverUC);
    server.on(F("/bin/slots.bin"), HTTP_GET, serverSlots);
    server.on(F("/slot/set"), HTTP_GET, serverSlotSet);
    server.on(F("/slot/save"), HTTP_GET, serverSlotSave);
    server.on(F("/slot/remove"), HTTP_GET, serverSlotRemove);
    // server.on(F("/spiffs/upload"), HTTP_GET, serverFsUpload);
    server.on(F("/spiffs/upload"), HTTP_POST, serverFsUploadResponder, serverFsUploadHandler);
    server.on(F("/spiffs/download"), HTTP_GET, serverFsDownloadHandler);
    server.on(F("/spiffs/dir"), HTTP_GET, serverFsDir);
    server.on(F("/spiffs/format"), HTTP_GET, serverFsFormat);
    server.on(F("/wifi/status"), HTTP_GET, serverWiFiStatus);
    server.on(F("/gbs/restore-filters"), HTTP_GET, serverRestoreFilters);
    // WiFi API
    server.on(F("/wifi/list"), HTTP_ANY, serverWiFiList);
    server.on("/wifi/wps", HTTP_ANY, serverWiFiWPS);
    server.on("/wifi/connect", HTTP_ANY, serverWiFiConnect);
    server.on("/wifi/ap", HTTP_ANY, serverWiFiAP);
    server.on("/wifi/rst", HTTP_ANY, serverWiFiReset);

    server.begin(); // Webserver for the site
}

/**
 * @brief
 *
 */
void serverHome()
{
    // Serial.println("sending web page");
    if (ESP.getFreeHeap() > 10000) {
        server.sendHeader(F("Content-Encoding"), "gzip");
        server.send(200, mimeTextHtml, webui_html, sizeof(webui_html));
    }
}

/**
 * @brief
 *
 */
void serverSC()
{
    if (ESP.getFreeHeap() > 10000) {
        if (server.args() > 0) {
            String p = server.arg(0);
            // Serial.println(p->name());
            serialCommand = p.charAt(0);

            // hack, problem with '+' command received via url param
            if (serialCommand == ' ') {
                serialCommand = '+';
            }
        }
        server.send(200); // reply
    }
}

/**
 * @brief
 *
 */
void serverUC()
{
    if (ESP.getFreeHeap() > 10000) {
        if (server.args() > 0) {
            String p = server.arg(0);
            // Serial.println(p->name());
            userCommand = p.charAt(0);
        }
        server.send(200); // reply
    }
}

/**
 * @brief
 *
 */
void serverSlots()
{
    if (ESP.getFreeHeap() > 10000) {
        SlotMetaArray slotsObject;
        File slotsBinaryFileRead = LittleFS.open(SLOTS_FILE, "r");

        if (!slotsBinaryFileRead) {
            File slotsBinaryFileWrite = LittleFS.open(SLOTS_FILE, "w");
            for (int i = 0; i < SLOTS_TOTAL; i++) {
                slotsObject.slot[i].slot = i;
                slotsObject.slot[i].presetID = 0;
                slotsObject.slot[i].scanlines = 0;
                slotsObject.slot[i].scanlinesStrength = 0;
                slotsObject.slot[i].wantVdsLineFilter = false;
                slotsObject.slot[i].wantStepResponse = true;
                slotsObject.slot[i].wantPeaking = true;
                char emptySlotName[25] = "Empty                   ";
                strncpy(slotsObject.slot[i].name, emptySlotName, 25);
            }
            slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
        }

        server.streamFile(slotsBinaryFileRead, String(mimeOctetStream));

        slotsBinaryFileRead.close();
    }
}

/**
 * @brief
 *
 */
void serverSlotSet()
{
    bool result = false;

    if (ESP.getFreeHeap() > 10000) {
        if (server.args() > 0) {
            String slotParamValue = server.arg(0);
            char slotValue[2];
            slotParamValue.toCharArray(slotValue, sizeof(slotValue));
            uopt->presetSlot = (uint8_t)slotValue[0];
            uopt->presetPreference = OutputCustomized;
            saveUserPrefs();
            result = true;
        }
    }

    server.send(200, mimeAppJson, result ? "true" : "false");
}

/**
 * @brief
 *
 */
void serverSlotSave()
{
    bool result = false;

    if (ESP.getFreeHeap() > 10000) {
        if (server.args() > 0) {
            SlotMetaArray slotsObject;
            File slotsBinaryFileRead = LittleFS.open(SLOTS_FILE, "r");

            if (slotsBinaryFileRead) {
                slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
                slotsBinaryFileRead.close();
            } else {
                File slotsBinaryFileWrite = LittleFS.open(SLOTS_FILE, "w");

                for (int i = 0; i < SLOTS_TOTAL; i++) {
                    slotsObject.slot[i].slot = i;
                    slotsObject.slot[i].presetID = 0;
                    slotsObject.slot[i].scanlines = 0;
                    slotsObject.slot[i].scanlinesStrength = 0;
                    slotsObject.slot[i].wantVdsLineFilter = false;
                    slotsObject.slot[i].wantStepResponse = true;
                    slotsObject.slot[i].wantPeaking = true;
                    char emptySlotName[25] = "Empty                   ";
                    strncpy(slotsObject.slot[i].name, emptySlotName, 25);
                }

                slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
                slotsBinaryFileWrite.close();
            }

            // index param
            String slotIndexString = server.arg(0);
            uint8_t slotIndex = lowByte(slotIndexString.toInt());
            if (slotIndex >= SLOTS_TOTAL) {
                goto fail;
            }

            // name param
            String slotName = server.arg(1);

            char emptySlotName[25] = "                        ";
            strncpy(slotsObject.slot[slotIndex].name, emptySlotName, 25);

            slotsObject.slot[slotIndex].slot = slotIndex;
            slotName.toCharArray(slotsObject.slot[slotIndex].name, sizeof(slotsObject.slot[slotIndex].name));
            slotsObject.slot[slotIndex].presetID = rto->presetID;
            slotsObject.slot[slotIndex].scanlines = uopt->wantScanlines;
            slotsObject.slot[slotIndex].scanlinesStrength = uopt->scanlineStrength;
            slotsObject.slot[slotIndex].wantVdsLineFilter = uopt->wantVdsLineFilter;
            slotsObject.slot[slotIndex].wantStepResponse = uopt->wantStepResponse;
            slotsObject.slot[slotIndex].wantPeaking = uopt->wantPeaking;

            File slotsBinaryOutputFile = LittleFS.open(SLOTS_FILE, "w");
            slotsBinaryOutputFile.write((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryOutputFile.close();

            result = true;
        }
    }

fail:
    server.send(200, mimeAppJson, result ? "true" : "false");
}

/**
 * @brief Remove slot request
 *
 */
void serverSlotRemove()
{
    bool result = false;
    char param = server.arg(0).charAt(0);
    if (server.args() > 0) {
        if (param == '0') {
            LOG(F("Wait..."));
            result = true;
        } else {
            Ascii8 slot = uopt->presetSlot;
            Ascii8 nextSlot;
            auto currentSlot = slotIndexMap.indexOf(slot);

            SlotMetaArray slotsObject;
            File slotsBinaryFileRead = LittleFS.open(SLOTS_FILE, "r");
            slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFileRead.close();
            String slotName = slotsObject.slot[currentSlot].name;

            // remove preset files
            LittleFS.remove("/preset_ntsc." + String((char)slot));
            LittleFS.remove("/preset_pal." + String((char)slot));
            LittleFS.remove("/preset_ntsc_480p." + String((char)slot));
            LittleFS.remove("/preset_pal_576p." + String((char)slot));
            LittleFS.remove("/preset_ntsc_720p." + String((char)slot));
            LittleFS.remove("/preset_ntsc_1080p." + String((char)slot));
            LittleFS.remove("/preset_medium_res." + String((char)slot));
            LittleFS.remove("/preset_vga_upscale." + String((char)slot));
            LittleFS.remove("/preset_unknown." + String((char)slot));

            uint8_t loopCount = 0;
            uint8_t flag = 1;
            while (flag != 0) {
                slot = slotIndexMap[currentSlot + loopCount];
                nextSlot = slotIndexMap[currentSlot + loopCount + 1];
                flag = 0;
                flag += LittleFS.rename("/preset_ntsc." + String((char)(nextSlot)), "/preset_ntsc." + String((char)slot));
                flag += LittleFS.rename("/preset_pal." + String((char)(nextSlot)), "/preset_pal." + String((char)slot));
                flag += LittleFS.rename("/preset_ntsc_480p." + String((char)(nextSlot)), "/preset_ntsc_480p." + String((char)slot));
                flag += LittleFS.rename("/preset_pal_576p." + String((char)(nextSlot)), "/preset_pal_576p." + String((char)slot));
                flag += LittleFS.rename("/preset_ntsc_720p." + String((char)(nextSlot)), "/preset_ntsc_720p." + String((char)slot));
                flag += LittleFS.rename("/preset_ntsc_1080p." + String((char)(nextSlot)), "/preset_ntsc_1080p." + String((char)slot));
                flag += LittleFS.rename("/preset_medium_res." + String((char)(nextSlot)), "/preset_medium_res." + String((char)slot));
                flag += LittleFS.rename("/preset_vga_upscale." + String((char)(nextSlot)), "/preset_vga_upscale." + String((char)slot));
                flag += LittleFS.rename("/preset_unknown." + String((char)(nextSlot)), "/preset_unknown." + String((char)slot));

                slotsObject.slot[currentSlot + loopCount].slot = slotsObject.slot[currentSlot + loopCount + 1].slot;
                slotsObject.slot[currentSlot + loopCount].presetID = slotsObject.slot[currentSlot + loopCount + 1].presetID;
                slotsObject.slot[currentSlot + loopCount].scanlines = slotsObject.slot[currentSlot + loopCount + 1].scanlines;
                slotsObject.slot[currentSlot + loopCount].scanlinesStrength = slotsObject.slot[currentSlot + loopCount + 1].scanlinesStrength;
                slotsObject.slot[currentSlot + loopCount].wantVdsLineFilter = slotsObject.slot[currentSlot + loopCount + 1].wantVdsLineFilter;
                slotsObject.slot[currentSlot + loopCount].wantStepResponse = slotsObject.slot[currentSlot + loopCount + 1].wantStepResponse;
                slotsObject.slot[currentSlot + loopCount].wantPeaking = slotsObject.slot[currentSlot + loopCount + 1].wantPeaking;
                // slotsObject.slot[currentSlot + loopCount].name = slotsObject.slot[currentSlot + loopCount + 1].name;
                strncpy(slotsObject.slot[currentSlot + loopCount].name, slotsObject.slot[currentSlot + loopCount + 1].name, 25);
                loopCount++;
            }

            File slotsBinaryFileWrite = LittleFS.open(SLOTS_FILE, "w");
            slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFileWrite.close();
            LOGF("Preset \"%s\" removed\n", slotName.c_str());
            result = true;
        }
    }

    // fail:
    server.send(200, mimeAppJson, result ? "true" : "false");
}

/**
 * @brief
 *
 */
void serverFsUploadResponder()
{
    server.send(200, mimeAppJson, "true");
}

/**
 * @brief
 *
 */
void serverFsUploadHandler()
{
    HTTPUpload &upload = server.upload();
    static fs::File _tempFile;
    static bool err;
    if (upload.status == UPLOAD_FILE_START) {
        WiFiUDP::stopAll();
        _tempFile = LittleFS.open("/" + upload.filename, "w");
        err = false;
    } else if (upload.status == UPLOAD_FILE_WRITE && upload.contentLength != 0 && !err) {
        if (_tempFile.write(upload.buf, upload.contentLength) != upload.contentLength) {
            err = true;
            LOGN(F("upload file write faled"));
        } else {
            LOG(F("."));
        }
    } else if (upload.status == UPLOAD_FILE_END && !err) {
        _tempFile.close();
        LOGN(F("upload file complete"));
        err = false;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        err = false;
        _tempFile.close();
        LOGN(F("upload file aborted"));
    }
}

/**
 * @brief
 *
 */
void serverFsDownloadHandler()
{
    if (ESP.getFreeHeap() > 10000) {
        if (server.args() > 0) {
            String _fn = server.arg(0);
            if (_fn.length() != 0) {
                fs::File _f = LittleFS.open(_fn, "r");
                if (_f) {
                    server.streamFile(_f, mimeOctetStream);
                    _f.close();
                    goto fs_dl_hanle_pass;
                }
            }
        }
    }
    server.send(200, mimeAppJson, "false");
fs_dl_hanle_pass:
    return;
}

/**
 * @brief
 *
 */
void serverFsDir()
{
    if (ESP.getFreeHeap() > 10000) {
        fs::Dir dir = LittleFS.openDir("/");
        String output = "[";

        while (dir.next()) {
            output += "\"";
            output += dir.fileName();
            output += "\",";
            delay(1); // wifi stack
        }

        output += "]";

        output.replace(",]", "]");

        server.send(200, mimeAppJson, output);
        return;
    }
    server.send(200, mimeAppJson, "false");
}

/**
 * @brief
 *
 */
void serverFsFormat()
{
    server.send(200, mimeAppJson, LittleFS.format() ? "true" : "false");
}

/**
 * @brief
 *
 */
void serverWiFiStatus()
{
    WiFiMode_t wifiMode = WiFi.getMode();
    server.send(200, mimeAppJson, wifiMode == WIFI_AP ? "{\"mode\":\"ap\"}" : "{\"mode\":\"sta\",\"ssid\":\"" + WiFi.SSID() + "\"}");
}

/**
 * @brief
 *
 */
void serverRestoreFilters()
{
    SlotMetaArray slotsObject;
    File slotsBinaryFileRead = LittleFS.open(SLOTS_FILE, "r");
    bool result = false;
    if (slotsBinaryFileRead) {
        slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileRead.close();
        auto currentSlot = slotIndexMap.indexOf(uopt->presetSlot);
        if (currentSlot == -1) {
            goto fail;
        }

        uopt->wantScanlines = slotsObject.slot[currentSlot].scanlines;

        LOG(F("slot: "));
        LOG(uopt->presetSlot);
        LOG(F("scanlines: "));
        if (uopt->wantScanlines) {
            LOG(F("on (Line Filter recommended)"));
        } else {
            disableScanlines();
            LOG(F("off"));
        }
        saveUserPrefs();

        uopt->scanlineStrength = slotsObject.slot[currentSlot].scanlinesStrength;
        uopt->wantVdsLineFilter = slotsObject.slot[currentSlot].wantVdsLineFilter;
        uopt->wantStepResponse = slotsObject.slot[currentSlot].wantStepResponse;
        uopt->wantPeaking = slotsObject.slot[currentSlot].wantPeaking;
        result = true;
    }

fail:
    server.send(200, mimeAppJson, result ? "true" : "false");
}

/**
 * @brief
 *
 */
void serverWiFiList()
{
    String s;
    uint8_t cntr = 0;
    uint8_t i = 0;
    int8_t n = WiFi.scanNetworks();
    if (n == 0 || n == -2)
        n = WiFi.scanNetworks();
    else {
        while (n == -1) {
            n = WiFi.scanComplete();
            WDT_FEED();
        }
    }

    // TODO: fuzzy logic
    // build array of indices
    int ix[n];
    for (i = 0; i < n; i++)
        ix[i] = i;

    // sort by signal strength
    for (i = 0; i < n; i++) {
        for (int j = 1; j < n - i; j++) {
            if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1])) {
                std::swap(ix[j], ix[j - 1]);
            }
        }
    }
    // remove duplicates
    for (i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (WiFi.SSID(ix[i]).equals(WiFi.SSID(ix[j]))
                && WiFi.encryptionType(ix[i]) == WiFi.encryptionType(ix[j]))
                ix[j] = -1;
        }
    }
    // TODO: END
    server.chunkedResponseModeStart(200, mimeTextHtml);
    while (cntr < n || ix[i] != -1) { // check s.length to limit memory usage
        s = String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2)
            + "," + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1) + ","
                + WiFi.SSID(ix[i]);
        server.sendContent(s.c_str());
        cntr++;
    }
    server.chunkedResponseFinalize();
}

void serverWiFiWPS()
{
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.beginWPSConfig();
        delay(100);
        wifiStartStaMode("", "");
    }
    server.send(200, mimeTextHtml, FPSTR("attempting WPS"));
}

// void serverWiFiConnect()
// {
//     String ssid = server.arg("n");
//     String pwd = server.arg("p");
//     if (ssid.length() != 0) { // could be open
//         wifiStartStaMode(ssid, pwd);
//         server.send(200, mimeTextHtml, PSTR("connecting..."));
//     } else
//         server.send(406, mimeTextHtml, PSTR("empty ssid..."));
// }

/**
 * @brief
 *
 */
void serverWiFiConnect()
{
    server.send(200, mimeAppJson, "true");
    String ssid = server.arg(FPSTR("n"));
    String pwd = server.arg(FPSTR("p"));

    if (ssid.length()) {
        if (pwd.length()) {
            // false = only save credentials, don't connect
            WiFi.begin(ssid.c_str(), pwd.c_str());
        } else {
            WiFi.begin(ssid.c_str(), NULL);
        }
    } else {
        WiFi.begin();
    }

    userCommand = 'u'; // next loop, set wifi station mode and restart device
}

void serverWiFiAP()
{
    wifiStartApMode();
    String msg = String(F("access point: "));
    msg += ap_ssid;
    server.send(200, mimeTextHtml, msg.c_str());
}

void serverWiFiReset()
{
    server.send(200, mimeTextHtml, PSTR("Rebooting..."));
    delay(100);
    ESP.restart();
}