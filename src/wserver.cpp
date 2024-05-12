/*
#####################################################################################
# fs::File: server.cpp                                                                  #
# fs::File Created: Friday, 19th April 2024 3:11:40 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Saturday, 11th May 2024 5:37:34 pm                       #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#include "wserver.h"

#define LOMEM_WEB             ((ESP.getFreeHeap() < 10000UL))
#define ASSERT_LOMEM_RETURN() do {                          \
    if (LOMEM_WEB) {                                        \
        char msg[128] = "";                                 \
        sprintf_P(msg, lomemMessage, ESP.getFreeHeap());    \
        server.send(200, mimeTextHtml, msg);                \
        return;                                             \
    }                                                       \
} while(0)
#define ASSERT_LOMEM_GOTO(G) do {                           \
    if (LOMEM_WEB) goto G;                                  \
} while(0)

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
    // server.on(F("/data/upload"), HTTP_GET, serverFsUpload);
    server.on(F("/data/upload"), HTTP_POST, serverFsUploadResponder, serverFsUploadHandler);
    server.on(F("/data/download"), HTTP_GET, serverFsDownloadHandler);
    server.on(F("/data/dir"), HTTP_GET, serverFsDir);
    server.on(F("/data/format"), HTTP_GET, serverFsFormat);
    server.on(F("/wifi/status"), HTTP_GET, serverWiFiStatus);
    server.on(F("/gbs/restore-filters"), HTTP_GET, serverRestoreFilters);
    // WiFi API
    server.on(F("/wifi/list"), HTTP_POST, serverWiFiList);
    server.on("/wifi/wps", HTTP_POST, serverWiFiWPS);
    server.on("/wifi/connect", HTTP_POST, serverWiFiConnect);
    server.on("/wifi/ap", HTTP_POST, serverWiFiAP);
    server.on("/wifi/rst", HTTP_POST, serverWiFiReset);

    server.keepAlive(false);
    server.begin(); // Webserver for the site
}

/**
 * @brief
 *
 */
void serverHome()
{
    ASSERT_LOMEM_RETURN();
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    fs::File index = LittleFS.open(String(indexPage), "r");
    if(index) {
        server.streamFile(index, mimeTextHtml);
        index.close();
        return;
    }
    server.send_P(200, mimeTextHtml, notFouldMessage);
    // server.send(200, mimeTextHtml, webui_html, sizeof(webui_html));
}

/**
 * @brief Serial Command
 *
 */
void serverSC()
{
    ASSERT_LOMEM_RETURN();
    if (server.args() > 0) {
        const String p = server.argName(0);

        serialCommand = p.charAt(0);

        // hack, problem with '+' command received via url param
        if (serialCommand == ' ') {
            serialCommand = '+';
        }
        // _DBG(F("[w] serial command received: "));
        // _DBGF("%c\n", serialCommand);
    }
    server.send(200, mimeAppJson, F("{}"));
}

/**
 * @brief User Command
 *
 */
void serverUC()
{
    ASSERT_LOMEM_RETURN();
    if (server.args() > 0) {
        String p = server.argName(0);
        userCommand = p.charAt(0);
        // _DBG(F("[w] user command received: "));
        // _DBGF("%c\n", userCommand);
    }
    server.send(200, mimeAppJson, F("{}"));
}

/**
 * @brief
 *
 */
void serverSlots()
{
    ASSERT_LOMEM_RETURN();
    fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");

    if (!slotsBinaryFile) {
        _DBGN(F("/slots.bin not found, attempting to create"));
        fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
        if(slotsBinaryFile) {
            SlotMetaArray slotsObject;
            for (int i = 0; i < SLOTS_TOTAL; i++) {
                slotsObject.slot[i].slot = i;
                slotsObject.slot[i].presetID = 0;
                slotsObject.slot[i].scanlines = 0;
                slotsObject.slot[i].scanlinesStrength = 0;
                slotsObject.slot[i].wantVdsLineFilter = false;
                slotsObject.slot[i].wantStepResponse = true;
                slotsObject.slot[i].wantPeaking = true;
                String slot_name = String(emptySlotName);
                strncpy(slotsObject.slot[i].name, slot_name.c_str(), 25);
            }
            slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
        } else {
            _DBGN(F("unable to create /slots.bin"));
            goto stream_slots_bin_failed;
        }
    }

    server.streamFile(slotsBinaryFile, String(mimeOctetStream));
    slotsBinaryFile.close();
    return;
stream_slots_bin_failed:
    server.send(500, mimeAppJson, F("false"));
}

/**
 * @brief
 *
 */
void serverSlotSet()
{
    ASSERT_LOMEM_GOTO(server_slot_set_failure);

    if (server.args() > 0) {
        String slotParamValue = server.arg(0);
        char slotValue[2];
        slotParamValue.toCharArray(slotValue, sizeof(slotValue));
        uopt->presetSlot = (uint8_t)slotValue[0];
        // uopt->presetPreference = OutputCustomized;
        rto->presetID = OutputCustom;
        saveUserPrefs();
        // _DBG(F("[w] slot value upd. success: "));
        // _DBGF("%s\n", slotValue);
        server.send(200, mimeAppJson, F("true"));
        return;
    }
server_slot_set_failure:
    server.send(500, mimeAppJson, F("false"));
}

/**
 * @brief
 *
 */
void serverSlotSave()
{
    bool result = false;
    ASSERT_LOMEM_GOTO(server_slot_save_failure);

    if (server.args() > 0) {
        SlotMetaArray slotsObject;
        fs::File slotsBinaryFileRead = LittleFS.open(FPSTR(slotsFile), "r");

        if (slotsBinaryFileRead) {
            slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFileRead.close();
        } else {
            fs::File slotsBinaryFileWrite = LittleFS.open(FPSTR(slotsFile), "w");

            for (int i = 0; i < SLOTS_TOTAL; i++) {
                slotsObject.slot[i].slot = i;
                slotsObject.slot[i].presetID = 0;
                slotsObject.slot[i].scanlines = 0;
                slotsObject.slot[i].scanlinesStrength = 0;
                slotsObject.slot[i].wantVdsLineFilter = false;
                slotsObject.slot[i].wantStepResponse = true;
                slotsObject.slot[i].wantPeaking = true;
                String slot_name = String(emptySlotName);
                strncpy(slotsObject.slot[i].name, slot_name.c_str(), 25);
            }

            slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFileWrite.close();
        }

        // index param
        String slotIndexString = server.arg(0);
        uint8_t slotIndex = lowByte(slotIndexString.toInt());
        if (slotIndex >= SLOTS_TOTAL) {
            goto server_slot_save_failure;
        }

        // name param
        String slotName = server.arg(1);

        String slot_line = String(emptySlotLine);
        strncpy(slotsObject.slot[slotIndex].name, slot_line.c_str(), 25);

        slotsObject.slot[slotIndex].slot = slotIndex;
        slotName.toCharArray(slotsObject.slot[slotIndex].name, sizeof(slotsObject.slot[slotIndex].name));
        slotsObject.slot[slotIndex].presetID = rto->presetID;
        slotsObject.slot[slotIndex].scanlines = uopt->wantScanlines;
        slotsObject.slot[slotIndex].scanlinesStrength = uopt->scanlineStrength;
        slotsObject.slot[slotIndex].wantVdsLineFilter = uopt->wantVdsLineFilter;
        slotsObject.slot[slotIndex].wantStepResponse = uopt->wantStepResponse;
        slotsObject.slot[slotIndex].wantPeaking = uopt->wantPeaking;

        fs::File slotsBinaryOutputFile = LittleFS.open(FPSTR(slotsFile), "w");
        slotsBinaryOutputFile.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryOutputFile.close();

        result = true;
    }

server_slot_save_failure:
    server.send(200, mimeAppJson, result ? F("true") : F("false"));
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
            _DBGN(F("Wait..."));
            result = true;
        } else {
            uint8_t slot = uopt->presetSlot;
            uint8_t nextSlot;
            auto currentSlot = String(slotIndexMap).indexOf(slot);

            SlotMetaArray slotsObject;
            fs::File slotsBinaryFileRead = LittleFS.open(FPSTR(slotsFile), "r");
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
            String sInd = String(slotIndexMap);
            while (flag != 0) {
                slot = sInd[currentSlot + loopCount];
                nextSlot = sInd[currentSlot + loopCount + 1];
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

            fs::File slotsBinaryFileWrite = LittleFS.open(FPSTR(slotsFile), "w");
            slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFileWrite.close();
            _DBGF("Preset \"%s\" removed\n", slotName.c_str());
            result = true;
        }
    }

    // fail:
    server.send(200, mimeAppJson, result ? F("true") : F("false"));
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
    static bool err = false;
    if (upload.status == UPLOAD_FILE_START) {
        webSocket.close();
        dnsServer.stop();
        const String fname = "/" + upload.filename;
        _tempFile = LittleFS.open(fname, "w");
        _DBGF(PSTR("[w] upload %s "), upload.filename.c_str());
        err = false;
    } else if (upload.status == UPLOAD_FILE_WRITE && upload.contentLength != 0 && !err) {
        if (_tempFile.write(upload.buf, upload.contentLength) != upload.contentLength) {
            err = true;
            _DBGN(F(" write failed, abort"));
            goto upload_file_close;
        } else {
            _DBG(F("."));
        }
    } else if (upload.status == UPLOAD_FILE_END && !err) {
        _DBGN(F(" complete"));
        err = false;
        goto upload_file_close;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        err = false;
        LittleFS.remove(String("/" + upload.filename));
        _DBGN(F(" aborted"));
        goto upload_file_close;
    }
    return;
upload_file_close:
    _tempFile.close();
}

/**
 * @brief
 *
 */
void serverFsDownloadHandler()
{
    ASSERT_LOMEM_GOTO(fs_dl_handle_fail);
    if (server.args() > 0) {
        String _fn = server.arg(0);
        if (_fn.length() != 0 && _fn.indexOf(PSTR(".html")) == -1) {
            fs::File _f = LittleFS.open(_fn, "r");
            if (_f) {
                server.streamFile(_f, mimeOctetStream);
                _f.close();
                goto fs_dl_handle_pass;
            }
        }
    }
fs_dl_handle_fail:
    server.send(200, mimeAppJson, "false");
fs_dl_handle_pass:
    return;
}

/**
 * @brief
 *
 */
void serverFsDir()
{
    String output = "[";
    fs::Dir dir = LittleFS.openDir("/");
    ASSERT_LOMEM_GOTO(server_fs_dir_failure);

    while (dir.next()) {
        if(dir.fileName().indexOf(PSTR(".html")) == -1) {
            output += "\"";
            output += dir.fileName();
            output += "\",";
            delay(1); // wifi stack
        }
    }

    output += "]";

    output.replace(",]", "]");

    server.send(200, mimeAppJson, output);
    return;

server_fs_dir_failure:
    server.send_P(200, mimeAppJson, PSTR("false"));
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
    server.send(200, mimeAppJson,
            (wifiMode == WIFI_AP)
                ? F("{\"mode\":\"ap\"}")
                    : String(F("{\"mode\":\"sta\",\"ssid\":\"") + WiFi.SSID() + F("\"}"))
    );
}

/**
 * @brief
 *
 */
void serverRestoreFilters()
{
    SlotMetaArray slotsObject;
    fs::File slotsBinaryFileRead = LittleFS.open(FPSTR(slotsFile), "r");
    bool result = false;
    if (slotsBinaryFileRead) {
        slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileRead.close();
        auto currentSlot = String(slotIndexMap).indexOf(uopt->presetSlot);
        if (currentSlot == -1) {
            goto fail;
        }

        uopt->wantScanlines = slotsObject.slot[currentSlot].scanlines;

        _WSF(PSTR("slot: %d scanlines: "), uopt->presetSlot);
        if (uopt->wantScanlines) {
            _WS(F("on (Line Filter recommended)"));
        } else {
            disableScanlines();
            _WS(F("off"));
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
    uint8_t i = 0;
    int8_t n = WiFi.scanNetworks();
    while (n == -1) {
        n = WiFi.scanComplete();
        WDT_FEED();
    }
    if(n > 0) {
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
        i = 0;
        server.chunkedResponseModeStart(200, mimeTextHtml);
        while (i < n && ix[i] != -1) { // check s.length to limit memory usage
            s = String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2)
                + "," + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1) + ","
                    + WiFi.SSID(ix[i]);
            server.sendContent(s.c_str());
            i++;
        }
        server.chunkedResponseFinalize();
    } else
        server.send(200, mimeTextHtml, F("{}"));
}

/**
 * @brief
 *
 */
void serverWiFiWPS()
{
    // if (WiFi.status() != WL_CONNECTED) {
    if(wifiStartWPS())
        server.send(200, mimeTextHtml, F("WPS successful"));
    else
        server.send(200, mimeTextHtml, F("WPS failed"));
    // }
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
    String ssid = server.arg(String(F("n")));
    String pwd = server.arg(String(F("p")));

    if(wifiStartStaMode(ssid, pwd))
        server.send(200, mimeAppJson, F("{}"));
    else
        server.send(406, mimeAppJson, F("{}"));

    delay(100);
    ESP.reset();
    // userCommand = 'u'; // next loop, set wifi station mode and restart device
}

/**
 * @brief
 *
 */
void serverWiFiAP()
{
    String msg = String(F("access point: "));
    msg += wifiGetApSSID();
    server.send(200, mimeTextHtml, msg.c_str());
    wifiStartApMode();
}

/**
 * @brief
 *
 */
void serverWiFiReset()
{
    server.send(200, mimeTextHtml, PSTR("Rebooting..."));
    delay(100);
    ESP.restart();
}

/**
 * @brief
 *
 */
void printInfo()
{
    char print[128] = ""; // Increase if compiler complains about sprintf
    static uint8_t clearIrqCounter = 0;
    static uint8_t lockCounterPrevious = 0;
    uint8_t lockCounter = 0;

    int8_t wifi = wifiGetRSSI();

    uint16_t hperiod = GBS::HPERIOD_IF::read();
    uint16_t vperiod = GBS::VPERIOD_IF::read();
    uint8_t stat0FIrq = GBS::STATUS_0F::read();
    char HSp = GBS::STATUS_SYNC_PROC_HSPOL::read() ? '+' : '-'; // 0 = neg, 1 = pos
    char VSp = GBS::STATUS_SYNC_PROC_VSPOL::read() ? '+' : '-'; // 0 = neg, 1 = pos
    char h = 'H', v = 'V';
    if (!GBS::STATUS_SYNC_PROC_HSACT::read()) {
        h = HSp = ' ';
    }
    if (!GBS::STATUS_SYNC_PROC_VSACT::read()) {
        v = VSp = ' ';
    }

    // int charsToPrint =
    sprintf_P(print,
            systemInfo,
            hperiod, vperiod, lockCounterPrevious,
            GBS::ADC_RGCTRL::read(), GBS::ADC_GGCTRL::read(), GBS::ADC_BGCTRL::read(),
            GBS::STATUS_00::read(), GBS::STATUS_05::read(), GBS::SP_CS_0x3E::read(),
            h, HSp, v, VSp, stat0FIrq, GBS::TEST_BUS::read(), getVideoMode(),
            GBS::STATUS_SYNC_PROC_HTOTAL::read(), GBS::STATUS_SYNC_PROC_VTOTAL::read() /*+ 1*/, // emucrt: without +1 is correct line count
            GBS::STATUS_SYNC_PROC_HLOW_LEN::read(), rto->noSyncCounter, rto->continousStableCounter,
            rto->currentLevelSOG, wifi);

    // _WS(F("charsToPrint: ")); _WSN(charsToPrint);
    _WS(print);

    if (stat0FIrq != 0x00) {
        // clear 0_0F interrupt bits regardless of syncwatcher status
        clearIrqCounter++;
        if (clearIrqCounter >= 50) {
            clearIrqCounter = 0;
            GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
            GBS::INTERRUPT_CONTROL_00::write(0x00);
        }
    }

    yield();
    if (GBS::STATUS_SYNC_PROC_HSACT::read()) { // else source might not be active
        for (uint8_t i = 0; i < 9; i++) {
            if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
                lockCounter++;
            } else {
                for (int i = 0; i < 10; i++) {
                    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
                        lockCounter++;
                        break;
                    }
                }
            }
        }
    }
    lockCounterPrevious = getMovingAverage(lockCounter);
}

/**
 * @brief
 *
 */
void printVideoTimings()
{
    if (rto->presetID < 0x20) {
        // horizontal
        _WSF(
            PSTR("\nHT / scale   : %d %d\nHS ST/SP     : %d %d\nHB ST/SP(d)  : %d %d\nHB ST/SP     : %d %d\n------\n"),
            GBS::VDS_HSYNC_RST::read(),
            GBS::VDS_HSCALE::read(),
            GBS::VDS_HS_ST::read(),
            GBS::VDS_HS_SP::read(),
            GBS::VDS_DIS_HB_ST::read(),
            GBS::VDS_DIS_HB_SP::read(),
            GBS::VDS_HB_ST::read(),
            GBS::VDS_HB_SP::read()
            );
        // vertical
        _WSF(
            PSTR("VT / scale   : %d %d\nVS ST/SP     : %d %d\nVB ST/SP(d)  : %d %d\nVB ST/SP     : %d %d\n"),
            GBS::VDS_VSYNC_RST::read(),
            GBS::VDS_VSCALE::read(),
            GBS::VDS_VS_ST::read(),
            GBS::VDS_VS_SP::read(),
            GBS::VDS_DIS_VB_ST::read(),
            GBS::VDS_DIS_VB_SP::read(),
            GBS::VDS_VB_ST::read(),
            GBS::VDS_VB_SP::read()
            );
        // IF V offset
        _WSF(
            PSTR("IF VB ST/SP  : %d %d\n"),
            GBS::IF_VB_ST::read(),
            GBS::IF_VB_SP::read()
            );
    } else {
        _WSF(
            PSTR("\nHD_HSYNC_RST : %d\nHD_INI_ST    : %d\nHS ST/SP     : %d %d\nHB ST/SP     : %d %d\n------\n"),
            GBS::HD_HSYNC_RST::read(),
            GBS::HD_INI_ST::read(),
            GBS::SP_CS_HS_ST::read(),
            GBS::SP_CS_HS_SP::read(),
            GBS::HD_HB_ST::read(),
            GBS::HD_HB_SP::read()
            );
        // vertical
        _WSF(
            PSTR("VS ST/SP     : %d %d\nVB ST/SP     : %d %d\n"),
            GBS::HD_VS_ST::read(),
            GBS::HD_VS_SP::read(),
            GBS::HD_VB_ST::read(),
            GBS::HD_VB_SP::read()
            );
    }

    _WSF(
        PSTR("CsVT         : %d\nCsVS_ST/SP   : %d %d\n"),
        GBS::STATUS_SYNC_PROC_VTOTAL::read(),
        getCsVsStart(),
        getCsVsStop()
        );
}

/**
 * @brief
 *
 */
void fastGetBestHtotal()
{
    uint32_t inStart, inStop;
    signed long inPeriod = 1;
    double inHz = 1.0;
    GBS::TEST_BUS_SEL::write(0xa);
    if (FrameSync::vsyncInputSample(&inStart, &inStop)) {
        inPeriod = (inStop - inStart) >> 1;
        if (inPeriod > 1) {
            inHz = (double)1000000 / (double)inPeriod;
        }
        _WSF(PSTR("inPeriod: %ld in Hz: %d\n"), inPeriod, inHz);
    } else {
        _WSN(F("error"));
    }

    uint16_t newVtotal = GBS::VDS_VSYNC_RST::read();
    double bestHtotal = 108000000 / ((double)newVtotal * inHz); // 107840000
    double bestHtotal50 = 108000000 / ((double)newVtotal * 50);
    double bestHtotal60 = 108000000 / ((double)newVtotal * 60);
    _WSF(
        PSTR("newVtotal: %d\nbestHtotal: %d\nbestHtotal50: %d\nbestHtotal60: %d\n"),
        newVtotal,
        // display clock probably not exact 108mhz
        bestHtotal,
        bestHtotal50,
        bestHtotal60
    );
    // if (bestHtotal > 800 && bestHtotal < 3200) {
        // applyBestHTotal((uint16_t)bestHtotal);
        // FrameSync::resetWithoutRecalculation(); // was single use of this function, function has changed since
    // }
}

#if defined(ESP8266)

/**
 * @brief Type2 == userCommand
 *
 * @param argument
 */
void handleType2Command(char argument)
{
    _WSF(
        commandDescr,
        "user",
        argument,
        argument,
        // uopt->presetPreference,
        uopt->presetSlot,
        uopt->presetSlot,
        rto->presetID
    );
    switch (argument) {
        case '0':
            _WS(F("pal force 60hz "));
            if (uopt->PalForce60 == 0) {
                uopt->PalForce60 = 1;
                _WSN(F("on"));
            } else {
                uopt->PalForce60 = 0;
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case '1':
            // reset to defaults button
            webSocket.close();
            loadDefaultUserOptions();
            saveUserPrefs();
            _WSN(F("options set to defaults, restarting"));
            delay(100);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            //
            break;
        case '2':
            //
            break;
        case '3': // load custom preset
        {
            // uopt->presetPreference = OutputCustomized; // custom
            rto->presetID = OutputCustom; // custom
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
        } break;
        case '4': // save custom preset
            savePresetToFS();
            // uopt->presetPreference = OutputCustomized; // custom
            rto->presetID = OutputCustom; // custom
            saveUserPrefs();
            break;
        case '5':
            // Frame Time Lock toggle
            uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
            saveUserPrefs();
            if (uopt->enableFrameTimeLock) {
                _WSN(F("FTL on"));
            } else {
                _WSN(F("FTL off"));
            }
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->enableFrameTimeLock) {
                activeFrameTimeLockInitialSteps();
            }
            break;
        case '6':
            //
            break;
        case '7':
            uopt->wantScanlines = !uopt->wantScanlines;
            _WS(F("scanlines: "));
            if (uopt->wantScanlines) {
                _WSN(F("on (Line Filter recommended)"));
            } else {
                disableScanlines();
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case '9':
            //
            break;
        case 'a':
            webSocket.close();
            _WSN(F("restart"));
            delay(60);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            break;
        case 'e': // print files on data
        {
            fs::Dir dir = LittleFS.openDir("/");
            while (dir.next()) {
                _WSF(
                    PSTR("%s %ld\n"),
                    dir.fileName().c_str(),
                    dir.fileSize()
                    );
                delay(1); // wifi stack
            }
            //
            fs::File f = LittleFS.open(FPSTR(preferencesFile), "r");
            if (!f) {
                _WSN(F("failed opening preferences file"));
            } else {
                _WSF(
                    PSTR("preset preference = %c\nframe time lock = %c\n"\
                    "preset slot = %c\nframe lock method = %c\nauto gain = %c\n"\
                    "scanlines = %c\ncomponent output = %c\ndeinterlacer mode = %c\n"\
                    "line filter = %c\npeaking = %c\npreferScalingRgbhv = %c\n"\
                    "6-tap = %c\npal force60 = %c\nmatched = %c\n"\
                    "step response = %c\ndisable external clock generator = %c\n"),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read(),
                    f.read()
                );

                f.close();
            }
        } break;
        case 'f':
        case 'g':
        case 'h':
        case 'p':
        case 's':
        case 'L': {
            // load preset via webui
            uint8_t videoMode = getVideoMode();
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput; // last known good as fallback
            // else videoMode stays 0 and we'll apply via some assumptions
            // TODO missing resolutions below ?
            if (argument == 'f')
                // uopt->presetPreference = Output960P; // 1280x960
                rto->presetID = Output960p; // 1280x960
            if (argument == 'g')
                // uopt->presetPreference = Output720P; // 1280x720
                rto->presetID = Output720p; // 1280x720
            if (argument == 'h')
                // uopt->presetPreference = Output480P; // 720x480/768x576
                rto->presetID = Output480p; // 720x480/768x576
            if (argument == 'p')
                // uopt->presetPreference = Output1024P; // 1280x1024
                rto->presetID = Output1024p; // 1280x1024
            if (argument == 's')
                // uopt->presetPreference = Output1080P; // 1920x1080
                rto->presetID = Output1080p; // 1920x1080
            if (argument == 'L')
                // uopt->presetPreference = OutputDownscale; // downscale
                rto->presetID = Output15kHz; // downscale

            rto->useHdmiSyncFix = 1; // disables sync out when programming preset
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(videoMode);
            }
            saveUserPrefs();
        } break;
        case 'i':
            // toggle active frametime lock method
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->frameTimeLockMethod == 0) {
                uopt->frameTimeLockMethod = 1;
            } else if (uopt->frameTimeLockMethod == 1) {
                uopt->frameTimeLockMethod = 0;
            }
            saveUserPrefs();
            activeFrameTimeLockInitialSteps();
            break;
        case 'l':
            // cycle through available SDRAM clocks
            {
                uint8_t PLL_MS = GBS::PLL_MS::read();
                uint8_t memClock = 0;

                if (PLL_MS == 0)
                    PLL_MS = 2;
                else if (PLL_MS == 2)
                    PLL_MS = 7;
                else if (PLL_MS == 7)
                    PLL_MS = 4;
                else if (PLL_MS == 4)
                    PLL_MS = 3;
                else if (PLL_MS == 3)
                    PLL_MS = 5;
                else if (PLL_MS == 5)
                    PLL_MS = 0;

                switch (PLL_MS) {
                    case 0:
                        memClock = 108;
                        break;
                    case 1:
                        memClock = 81;
                        break; // goes well with 4_2C = 0x14, 4_2D = 0x27
                    case 2:
                        memClock = 10;
                        break; // feedback clock
                    case 3:
                        memClock = 162;
                        break;
                    case 4:
                        memClock = 144;
                        break;
                    case 5:
                        memClock = 185;
                        break; // slight OC
                    case 6:
                        memClock = 216;
                        break; // !OC!
                    case 7:
                        memClock = 129;
                        break;
                    default:
                        break;
                }
                GBS::PLL_MS::write(PLL_MS);
                ResetSDRAM();
                if (memClock != 10) {
                    _WSF(PSTR("SDRAM clock: %dMhz\n"), memClock);
                } else {
                    _WSN(F("SDRAM clock: feedback clock"));
                }
            }
            break;
        case 'm':
            _WS(F("Line Filter: "));
            if (uopt->wantVdsLineFilter) {
                uopt->wantVdsLineFilter = 0;
                GBS::VDS_D_RAM_BYPS::write(1);
                _WSN(F("off"));
            } else {
                uopt->wantVdsLineFilter = 1;
                GBS::VDS_D_RAM_BYPS::write(0);
                _WSN(F("on"));
            }
            saveUserPrefs();
            break;
        case 'n':
            uopt->enableAutoGain = 0;
            setAdcGain(GBS::ADC_RGCTRL::read() - 1);
            _WSF(PSTR("ADC gain++ : 0x%04X\n"), GBS::ADC_RGCTRL::read());
            break;
        case 'o':
            uopt->enableAutoGain = 0;
            setAdcGain(GBS::ADC_RGCTRL::read() + 1);
            _WSF(PSTR("ADC gain-- : 0x%04X\n"), GBS::ADC_RGCTRL::read());
            break;
        case 'A': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd > 4) && (hbspd < (htotal - 4))) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
            } else {
                _WSN(F("limit"));
            }
        } break;
        case 'B': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd < (htotal - 4)) && (hbspd > 4)) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
            } else {
                _WSN(F("limit"));
            }
        } break;
        case 'C': {
            // vert mask +
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd > 6) && (vbspd < (vtotal - 4))) {
                GBS::VDS_DIS_VB_ST::write(vbstd - 2);
                GBS::VDS_DIS_VB_SP::write(vbspd + 2);
            } else {
                _WSN(F("limit"));
            }
        } break;
        case 'D': {
            // vert mask -
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd < (vtotal - 4)) && (vbspd > 6)) {
                GBS::VDS_DIS_VB_ST::write(vbstd + 2);
                GBS::VDS_DIS_VB_SP::write(vbspd - 2);
            } else {
                _WSN(F("limit"));
            }
        } break;
        case 'q':
            if (uopt->deintMode != 1) {
                uopt->deintMode = 1;
                disableMotionAdaptDeinterlace();
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read()) {
                    disableScanlines();
                }
                saveUserPrefs();
            }
            _WSN(F("Deinterlacer: Bob"));
            break;
        case 'r':
            if (uopt->deintMode != 0) {
                uopt->deintMode = 0;
                saveUserPrefs();
                // will enable next loop()
            }
            _WSN(F("Deinterlacer: Motion Adaptive"));
            break;
        case 't':
            // unused now
            _WS(F("6-tap: "));
            if (uopt->wantTap6 == 0) {
                uopt->wantTap6 = 1;
                GBS::VDS_TAP6_BYPS::write(0);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() - 1);
                _WSN(F("on"));
            } else {
                uopt->wantTap6 = 0;
                GBS::VDS_TAP6_BYPS::write(1);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case 'u':
            // restart to attempt wifi station mode connect
            wifiStartStaMode("");
            delay(100);
            ESP.reset();
            break;
        case 'v': {
            uopt->wantFullHeight = !uopt->wantFullHeight;
            saveUserPrefs();
            uint8_t vidMode = getVideoMode();
            // if (uopt->presetPreference == 5) {
            if (rto->presetID == Output1080p) {
                if (GBS::GBS_PRESET_ID::read() == 0x05 || GBS::GBS_PRESET_ID::read() == 0x15) {
                    applyPresets(vidMode);
                }
            }
        } break;
        case 'w':
            uopt->enableCalibrationADC = !uopt->enableCalibrationADC;
            saveUserPrefs();
            break;
        case 'x':
            uopt->preferScalingRgbhv = !uopt->preferScalingRgbhv;
            _WS(F("preferScalingRgbhv: "));
            if (uopt->preferScalingRgbhv) {
                _WSN(F("on"));
            } else {
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case 'X':
            _WS(F("ExternalClockGenerator "));
            if (uopt->disableExternalClockGenerator == 0) {
                uopt->disableExternalClockGenerator = 1;
                _WSN(F("disabled"));
            } else {
                uopt->disableExternalClockGenerator = 0;
                _WSN("enabled");
            }
            saveUserPrefs();
            break;
        case 'z':
            // sog slicer level
            if (rto->currentLevelSOG > 0) {
                rto->currentLevelSOG -= 1;
            } else {
                rto->currentLevelSOG = 16;
            }
            setAndUpdateSogLevel(rto->currentLevelSOG);
            optimizePhaseSP();
            _WSF(
                PSTR("Phase: %d SOG: %d\n"),
                rto->phaseSP,
                rto->currentLevelSOG
            );
            break;
        case 'E':
            // test option for now
            _WS(F("IF Auto Offset: "));
            toggleIfAutoOffset();
            if (GBS::IF_AUTO_OFST_EN::read()) {
                _WSN(F("on"));
            } else {
                _WSN(F("off"));
            }
            break;
        case 'F':
            // freeze pic
            if (GBS::CAPTURE_ENABLE::read()) {
                GBS::CAPTURE_ENABLE::write(0);
            } else {
                GBS::CAPTURE_ENABLE::write(1);
            }
            break;
        case 'K':
            // scanline strength
            if (uopt->scanlineStrength >= 0x10) {
                uopt->scanlineStrength -= 0x10;
            } else {
                uopt->scanlineStrength = 0x50;
            }
            if (rto->scanlinesEnabled) {
                GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
                GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength);
            }
            saveUserPrefs();
            _WSF(PSTR("Scanline Brightness: 0x%04X\n"), uopt->scanlineStrength);
            break;
        case 'Z':
            // brightness++
            GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 1);
            _WSF(
                PSTR("Brightness++ : %d\n"),
                GBS::VDS_Y_OFST::read()
            );
            break;
        case 'T':
            // brightness--
            GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 1);
            _WSF(
                PSTR("Brightness-- : %d\n"),
                GBS::VDS_Y_OFST::read()
            );
            break;
        case 'N':
            // contrast++
            GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 1);
            _WSF(
                PSTR("Contrast++ : %d\n"),
                GBS::VDS_Y_GAIN::read()
            );
            break;
        case 'M':
            // contrast--
            GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 1);
            _WSF(
                PSTR("Contrast-- : %d\n"),
                GBS::VDS_Y_GAIN::read()
            );
            break;
        case 'Q':
            // pb/u gain++
            GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() + 1);
            _WSF(
                PSTR("Pb/U gain++ : %d\n"),
                GBS::VDS_UCOS_GAIN::read()
            );
            break;
        case 'H':
            // pb/u gain--
            GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() - 1);
            _WSF(
                PSTR("Pb/U gain-- : %d\n"),
                GBS::VDS_UCOS_GAIN::read()
            );
            break;
            break;
        case 'P':
            // pr/v gain++
            GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() + 1);
            _WSF(
                PSTR("Pr/V gain++ : %d\n"),
                GBS::VDS_VCOS_GAIN::read()
            );
            break;
        case 'S':
            // pr/v gain--
            GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() - 1);
            _WSF(
                PSTR("Pr/V gain-- : %d\n"),
                GBS::VDS_VCOS_GAIN::read()
            );
            break;
        case 'O':
            // info
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                _WSF(
                    PSTR("RGB reg\n------------\nY_OFFSET: %d\n"\
                    "U_OFFSET: %d\nV_OFFSET: %d\nY_GAIN: %d\n"\
                    "USIN_GAIN: %d\nUCOS_GAIN: %d\n"),
                    GBS::VDS_Y_OFST::read(),
                    GBS::VDS_U_OFST::read(),
                    GBS::VDS_V_OFST::read(),
                    GBS::VDS_Y_GAIN::read(),
                    GBS::VDS_USIN_GAIN::read(),
                    GBS::VDS_UCOS_GAIN::read()
                );
            } else {
                _WSF(
                    PSTR("YPbPr reg\n------------\nY_OFFSET: %d\n"\
                    "U_OFFSET: %d\nV_OFFSET: %d\nY_GAIN: %d\n"\
                    "USIN_GAIN: %d\nUCOS_GAIN: %d\n"),
                    GBS::VDS_Y_OFST::read(),
                    GBS::VDS_U_OFST::read(),
                    GBS::VDS_V_OFST::read(),
                    GBS::VDS_Y_GAIN::read(),
                    GBS::VDS_USIN_GAIN::read(),
                    GBS::VDS_UCOS_GAIN::read()
                );
            }
            break;
        case 'U':
            // default
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                GBS::VDS_Y_GAIN::write(128);
                GBS::VDS_UCOS_GAIN::write(28);
                GBS::VDS_VCOS_GAIN::write(41);
                GBS::VDS_Y_OFST::write(0);
                GBS::VDS_U_OFST::write(0);
                GBS::VDS_V_OFST::write(0);
                GBS::ADC_ROFCTRL::write(adco->r_off);
                GBS::ADC_GOFCTRL::write(adco->g_off);
                GBS::ADC_BOFCTRL::write(adco->b_off);
                _WSN(F("RGB:defauit"));
            } else {
                GBS::VDS_Y_GAIN::write(128);
                GBS::VDS_UCOS_GAIN::write(28);
                GBS::VDS_VCOS_GAIN::write(41);
                GBS::VDS_Y_OFST::write(254);
                GBS::VDS_U_OFST::write(3);
                GBS::VDS_V_OFST::write(3);
                GBS::ADC_ROFCTRL::write(adco->r_off);
                GBS::ADC_GOFCTRL::write(adco->g_off);
                GBS::ADC_BOFCTRL::write(adco->b_off);
                _WSN(F("YPbPr:defauit"));
            }
            break;
        default:
            break;
    }
}

/**
 * @brief
 *
 */
void initUpdateOTA()
{
    ArduinoOTA.setHostname("GBS OTA");

    // ArduinoOTA.setPassword("admin");
    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    // update: no password is as (in)secure as this publicly stated hash..
    // rely on the user having to enable the OTA feature on the web ui

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating LittleFS this would be the place to unmount LittleFS using LittleFS.end()
        LittleFS.end();
        _WSF(
            PSTR("%s %s\n"),
            String(F("Start updating ")).c_str(),
            type.c_str()
        );
    });
    ArduinoOTA.onEnd([]() {
        _WSN(F("\nEnd"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        _WSF(PSTR("Progress: %u%%\n"), (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        _WSF(PSTR("Error[%u]: "), error);
        if (error == OTA_AUTH_ERROR)
            _WSN(F("Auth Failed"));
        else if (error == OTA_BEGIN_ERROR)
            _WSN(F("Begin Failed"));
        else if (error == OTA_CONNECT_ERROR)
            _WSN(F("Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR)
            _WSN(F("Receive Failed"));
        else if (error == OTA_END_ERROR)
            _WSN(F("End Failed"));
    });
    ArduinoOTA.begin();
    yield();
}

#endif                                      // defined(ESP8266)