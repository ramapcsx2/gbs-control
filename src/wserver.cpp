/*
#####################################################################################
# fs::File: server.cpp                                                                  #
# fs::File Created: Friday, 19th April 2024 3:11:40 pm                                  #
# Author: Sergey Ko                                                                 #
# Last Modified: Thursday, 30th May 2024 12:59:27 pm                      #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#include "wserver.h"

#define LOMEM_WEB ((ESP.getFreeHeap() < 10000UL))
#define ASSERT_LOMEM_RETURN()                                \
    do                                                       \
    {                                                        \
        if (LOMEM_WEB)                                       \
        {                                                    \
            char msg[128] = "";                              \
            sprintf_P(msg, lomemMessage, ESP.getFreeHeap()); \
            server.send(200, mimeTextHtml, msg);             \
            return;                                          \
        }                                                    \
    } while (0)
#define ASSERT_LOMEM_GOTO(G) \
    do                       \
    {                        \
        if (LOMEM_WEB)       \
            goto G;          \
    } while (0)

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
    server.on(F("/data/restore"), HTTP_POST, serverFsUploadResponder, serverFsUploadHandler);
    server.on(F("/data/backup"), HTTP_GET, serverBackupDownload);
    // server.on(F("/data/dir"), HTTP_GET, serverFsDir);
    // server.on(F("/data/format"), HTTP_GET, serverFsFormat);
    server.on(F("/wifi/status"), HTTP_GET, serverWiFiStatus);
    server.on(F("/gbs/restore-filters"), HTTP_GET, serverRestoreFilters);
    // WiFi API
    server.on(F("/wifi/list"), HTTP_POST, serverWiFiList);
    server.on("/wifi/wps", HTTP_POST, serverWiFiWPS);
    server.on("/wifi/connect", HTTP_POST, serverWiFiConnect);
    server.on("/wifi/ap", HTTP_POST, serverWiFiAP);
    server.on("/wifi/rst", HTTP_POST, serverWiFiReset);

    // in case if something went wrong last time
    LittleFS.remove(FPSTR(backupFile));

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
    fs::File index = LittleFS.open(FPSTR(indexPage), "r");
    if (index)
    {
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
    if (server.args() > 0)
    {
        const String p = server.argName(0);

        serialCommand = p.charAt(0);

        // hack, problem with '+' command received via url param
        if (serialCommand == ' ')
        {
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
    if (server.args() > 0)
    {
        String p = server.argName(0);
        userCommand = p.charAt(0);
        // _DBG(F("[w] user command received: "));
        // _DBGF("%c\n", userCommand);
    }
    server.send(200, mimeAppJson, F("{}"));
}

/**
 * @brief Sends the slots data to WebUI
 *
 *
 */
void serverSlots()
{
    ASSERT_LOMEM_RETURN();
    fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");

    if (!slotsBinaryFile)
    {
        _DBGN(F("/slots.bin not found, attempting to create"));
        fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
        if (slotsBinaryFile)
        {
            SlotMetaArray slotsObject;
            String slot_name = String(emptySlotName);
            for (int i = 0; i < SLOTS_TOTAL; i++)
            {
                // slotsObject.slot[i].slot = i;
                // slotsObject.slot[i].presetNameID = 0;
                slotsObject.slot[i].resolutionID = Output240p;
                slotsObject.slot[i].scanlines = 0;
                slotsObject.slot[i].scanlinesStrength = 0;
                slotsObject.slot[i].wantVdsLineFilter = false;
                slotsObject.slot[i].wantStepResponse = true;
                slotsObject.slot[i].wantPeaking = true;
                strncpy(slotsObject.slot[i].name, slot_name.c_str(), sizeof(slotsObject.slot[i].name));
            }
            slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
        }
        else
        {
            _DBGN(F("unable to create /slots.bin"));
            goto stream_slots_bin_failed;
        }
    }

    server.streamFile(slotsBinaryFile, mimeOctetStream);
    slotsBinaryFile.close();
    return;
stream_slots_bin_failed:
    server.send(500, mimeAppJson, F("false"));
}

/**
 * @brief This function does:
 *  1. Update current slot name and save it into user preferences
 *  2. Load new preset
 *
 */
void serverSlotSet()
{
    ASSERT_LOMEM_GOTO(server_slot_set_failure);

    if (server.args() > 0)
    {
        SlotMetaArray slotsObject;
        // const String slotParamValue = server.arg(String(F("index"))).toInt();
        uint8_t slotIndex = lowByte(server.arg(String(F("index"))).toInt());
        // char slotValue[2];
        // slotParamValue.toCharArray(slotValue, sizeof(slotValue));
        // uopt->presetSlot = static_cast<uint8_t>(slotParamValue.charAt(0));
        uopt->presetSlot = slotIndex;
        fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");
        if (slotsBinaryFile)
        {
            slotsBinaryFile.read((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFile.close();
            // update parameters
            rto->resolutionID = static_cast<OutputResolution>(slotsObject.slot[slotIndex].resolutionID);
            uopt->wantScanlines = slotsObject.slot[slotIndex].scanlines;
            uopt->scanlineStrength = slotsObject.slot[slotIndex].scanlinesStrength;
            uopt->wantVdsLineFilter = slotsObject.slot[slotIndex].wantVdsLineFilter;
            uopt->wantStepResponse = slotsObject.slot[slotIndex].wantStepResponse;
            uopt->wantPeaking = slotsObject.slot[slotIndex].wantPeaking;
        }
        else
        {
            _DBGN(F("unable to read /slots.bin"));
            goto server_slot_set_failure;
        }
        //// uopt->presetPreference = OutputCustomized;
        // rto->presetID = OutputCustom;
        // saveUserPrefs();     // user prefs being saved in userCommand handler
        _WSF(PSTR("slot change success: %d\n"), uopt->presetSlot);
        server.send(200, mimeAppJson, F("true"));
        // begin loading new preset on the next loop
        userCommand = '3';
        return;
    }
server_slot_set_failure:
    server.send(500, mimeAppJson, F("false"));
}

/**
 * @brief Save server slot name
 *
 */
void serverSlotSave()
{
    bool result = false;
    uint8_t i = 0;
    ASSERT_LOMEM_GOTO(server_slot_save_end);
    // TODO: too many open-closes...
    if (server.args() > 0)
    {
        // slot name
        String slotName = server.arg(String(F("name")));
        uint8_t slotIndex = server.arg(String(F("index"))).toInt();
        SlotMetaArray slotsObject;
        uint8_t k = slotName.length();
        fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");

        if (slotsBinaryFile)
        {
            // read data into slotsObject
            slotsBinaryFile.read((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFile.close();
        }
        else
        {
            String slot_name = String(emptySlotName);
            while (i < SLOTS_TOTAL)
            {
                // slotsObject.slot[i].slot = i;
                // slotsObject.slot[i].presetNameID = 0;
                slotsObject.slot[i].scanlines = 0;
                slotsObject.slot[i].resolutionID = Output240p;
                slotsObject.slot[i].scanlinesStrength = 0;
                slotsObject.slot[i].wantVdsLineFilter = false;
                slotsObject.slot[i].wantStepResponse = true;
                slotsObject.slot[i].wantPeaking = true;
                strncpy(slotsObject.slot[i].name,
                    slot_name.c_str(),
                    sizeof(slotsObject.slot[i].name)
                );
                i++;
                delay(1);
            }
            // file doesn't exist, let's create one
            slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
            slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFile.close();
        }

        // updating the SlotMetaObject with data received
        // uint8_t slotIndex = lowByte(slotIndexString.toInt());
        uint8_t nameMaxLen = sizeof(slotsObject.slot[slotIndex].name);
        const char space = 0x20;
        if (slotIndex >= SLOTS_TOTAL)
        {
            _WSF(PSTR("too much slots, max allowed: %d\n"), (uint8_t)SLOTS_TOTAL);
            goto server_slot_save_end;
        }
        // fail proof
        if(slotName.length() >= nameMaxLen) {
            _WSF(PSTR("slot name is too long, max allowed: %d\n"), nameMaxLen);
            goto server_slot_save_end;
        }
        // slotsObject.slot[slotIndex].slot = slotIndex;
        // slotsObject.slot[slotIndex].presetNameID = 0;
        // slotName.toCharArray(slotsObject.slot[slotIndex].name, sizeof(slotsObject.slot[slotIndex].name));
        strcpy(slotsObject.slot[slotIndex].name, slotName.c_str());
        while(k < nameMaxLen - 1) {
            slotsObject.slot[slotIndex].name[k] = space;
            k ++;
        }
        delay(10);
        slotsObject.slot[slotIndex].resolutionID = rto->resolutionID;
        slotsObject.slot[slotIndex].scanlines = uopt->wantScanlines;
        slotsObject.slot[slotIndex].scanlinesStrength = uopt->scanlineStrength;
        slotsObject.slot[slotIndex].wantVdsLineFilter = uopt->wantVdsLineFilter;
        slotsObject.slot[slotIndex].wantStepResponse = uopt->wantStepResponse;
        slotsObject.slot[slotIndex].wantPeaking = uopt->wantPeaking;

        slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
        slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();

        result = true;
    }

server_slot_save_end:
    server.send(200, mimeAppJson, result ? F("true") : F("false"));
}

/**
 * @brief Remove slot request
 *
 */
void serverSlotRemove()
{
    bool result = false;
    uint8_t i = 0;
    int16_t slotIndex = server.arg(String(F("index"))).toInt();
    SlotMetaArray slotsObject;
    if (server.args() > 0 && slotIndex >= 0)
    {
        char buffer[4] = "";
        // auto currentSlot = String(slotIndexMap).charAt(slotIndex);
        if (slotIndex > SLOTS_TOTAL || slotIndex < 0)
        {
            _DBGN(F("wrong slot index, abort"));
            goto server_slot_remove_end;
        }
        // preset file extension
        sprintf(buffer, PSTR(".%d"), slotIndex);

        fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");

        if (!slotsBinaryFile) {
            _DBGN(F("failed to read slots.bin"));
            goto server_slot_remove_end;
        }

        slotsBinaryFile.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();

        _DBGF(
            PSTR("removing slot: %s and related presets...\n"),
            slotsObject.slot[slotIndex].name
        );

        // remove every preset file of this slot
        fs::Dir root = LittleFS.openDir("/");
        while (root.next())
        {
            String fn = root.fileName();
            if (fn.lastIndexOf(buffer) == (int)(fn.length() - 3))
                LittleFS.remove(fn);
            delay(10);
        }
        // lets reset the slot data in binary file
        while (i < SLOTS_TOTAL)
        {
            if (i == slotIndex)
            {
                String slot_name = String(emptySlotName);
                // slotsObject.slot[i].slot = i;
                // slotsObject.slot[i].presetNameID = 0;
                slotsObject.slot[i].resolutionID = Output240p;
                slotsObject.slot[i].scanlines = 0;
                slotsObject.slot[i].scanlinesStrength = 0;
                slotsObject.slot[i].wantVdsLineFilter = false;
                slotsObject.slot[i].wantStepResponse = true;
                slotsObject.slot[i].wantPeaking = true;
                strncpy(slotsObject.slot[i].name,
                    slot_name.c_str(),
                        sizeof(slotsObject.slot[i].name)
                );
                break;
            }
            i++;
            delay(1);
        }

        // uint8_t loopCount = 0;
        // uint8_t flag = 1;
        // String sInd = String(slotIndexMap);
        // while (flag != 0) {
        //     currentSlot = sInd[currentSlot + loopCount];
        //     nextSlot = sInd[currentSlot + loopCount + 1];
        //     flag = 0;
        //     flag += LittleFS.rename("/preset_ntsc." + String((char)(nextSlot)), "/preset_ntsc." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_pal." + String((char)(nextSlot)), "/preset_pal." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_ntsc_480p." + String((char)(nextSlot)), "/preset_ntsc_480p." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_pal_576p." + String((char)(nextSlot)), "/preset_pal_576p." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_ntsc_720p." + String((char)(nextSlot)), "/preset_ntsc_720p." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_ntsc_1080p." + String((char)(nextSlot)), "/preset_ntsc_1080p." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_medium_res." + String((char)(nextSlot)), "/preset_medium_res." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_vga_upscale." + String((char)(nextSlot)), "/preset_vga_upscale." + String((char)currentSlot));
        //     flag += LittleFS.rename("/preset_unknown." + String((char)(nextSlot)), "/preset_unknown." + String((char)currentSlot));

        //     slotsObject.slot[currentSlot + loopCount].slot = slotsObject.slot[currentSlot + loopCount + 1].slot;
        //     slotsObject.slot[currentSlot + loopCount].presetID = slotsObject.slot[currentSlot + loopCount + 1].presetID;
        //     slotsObject.slot[currentSlot + loopCount].scanlines = slotsObject.slot[currentSlot + loopCount + 1].scanlines;
        //     slotsObject.slot[currentSlot + loopCount].scanlinesStrength = slotsObject.slot[currentSlot + loopCount + 1].scanlinesStrength;
        //     slotsObject.slot[currentSlot + loopCount].wantVdsLineFilter = slotsObject.slot[currentSlot + loopCount + 1].wantVdsLineFilter;
        //     slotsObject.slot[currentSlot + loopCount].wantStepResponse = slotsObject.slot[currentSlot + loopCount + 1].wantStepResponse;
        //     slotsObject.slot[currentSlot + loopCount].wantPeaking = slotsObject.slot[currentSlot + loopCount + 1].wantPeaking;
        //     // slotsObject.slot[currentSlot + loopCount].name = slotsObject.slot[currentSlot + loopCount + 1].name;
        //     strncpy(slotsObject.slot[currentSlot + loopCount].name, slotsObject.slot[currentSlot + loopCount + 1].name, 25);
        //     loopCount++;
        // }

        slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
        slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();
        result = true;
    }
    delay(10);
    // reset parameters
    setResetParameters();
    _DBGN(F("slot has been removed, parameters now reset to defaults"));
    // also reset resolution
    rto->resolutionID = Output240p;

server_slot_remove_end:
    server.send(200, mimeAppJson, result ? F("true") : F("false"));
}

/**
 * @brief Upload file final response
 *
 */
void serverFsUploadResponder()
{
    server.send(200, mimeAppJson, "true");
}

/**
 * @brief Upload file handler
 *
 */
void serverFsUploadHandler()
{
    HTTPUpload &upload = server.upload();
    static fs::File _tempFile;
    static bool err = false;
    if (upload.status == UPLOAD_FILE_START)
    {
        webSocket.close();
        dnsServer.stop();
        _tempFile = LittleFS.open(FPSTR(backupFile), "w");
        _DBGF(PSTR("uploading file: %s"), upload.filename.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE && !err)
    {
        if (_tempFile.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            err = true;
            _DBGN(F(" write failed, abort"));
            goto upload_file_close;
        }
        else
        {
            _DBG(F("."));
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        _DBGN(F(" complete"));
        err = false;
        // extract from backup at the next loop()
        userCommand = 'u';
        goto upload_file_close;
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        err = false;
        LittleFS.remove(FPSTR(backupFile));
        _DBGN(F(" aborted"));
        goto upload_file_close;
    }
    return;
upload_file_close:
    _tempFile.close();
}

/**
 * @brief Backup slots and its presets
 *      The commented code below may be used in future in case of large size
 *      of data to backup. It does the following:
 *        1. creates backup file with all data needed to backup
 *        2. streams backup file to client
 *        3. removes backup file
 */
void serverBackupDownload()
{
    // fs::File _b = LittleFS.open(FPSTR(backupFile), "w");
    // if(!_b) {
    //     _DBGN(F("failed to create backup file, can not continue..."));
    //     return;
    // }
    StreamString _b;
    // size_t contentLength = 0;
    fs::Dir dir = LittleFS.openDir("/");
    String ts = server.arg(String(F("ts")));
    webSocket.close();
    // ASSERT_LOMEM_GOTO(fs_dl_handle_fail);
    while (dir.next())
    {
        if (dir.isFile() && dir.fileName().indexOf(PSTR("__")) == -1)
        {
            fs::File _f = LittleFS.open(dir.fileName(), "r");
            if (_f)
            {
                // write file name
                _b.print(dir.fileName());
                _b.println();
                // slots.bin will be processed differently
                // if(dir.fileName().compareTo(String(slotsFile).substring(1)) != 0) {
                    // file size
                    _b.print(dir.fileSize());
                    _b.println();
                    // write file contents
                    while(_f.available()) {
                        _b.write(_f.read());
                    }
                    _DBGF(PSTR("%s dump complete\n"), dir.fileName().c_str());
                /* } else {
                    SlotMetaArray slotsObject;
                    // reading into mem
                    _f.read((byte *)&slotsObject, sizeof(slotsObject));
                    _f.close();
                    uint8_t i = 0;
                    String slot_name = String(emptySlotName);
                    while(i < SLOTS_TOTAL) {
                        if(strcmp(slotsObject.slot[i].name, slot_name.c_str()) != 0) {
                            _DBGF(PSTR("slot: %s\n"), slotsObject.slot[i].name);
                            _b.println(i);
                            _b.println(slotsObject.slot[i].name);
                            _b.println(slotsObject.slot[i].resolutionID);
                            _b.println(slotsObject.slot[i].scanlines);
                            _b.println(slotsObject.slot[i].scanlinesStrength);
                            _b.println(slotsObject.slot[i].wantVdsLineFilter);
                            _b.println(slotsObject.slot[i].wantStepResponse);
                            _b.println(slotsObject.slot[i].wantPeaking);
                        } else
                            break;
                        i++;
                    }
                    // since we're unable to have number of not empty slots beforehand,
                    // let's leave here a "stop string" for now. The principle of
                    // slots storage may need to change to allow the simplier logic.
                    _b.println("\t\t");
                    _DBGN(F("slots.bin dump complete"));
                } */
                _f.close();
            }
        }
    }
    delay(100);
    server.sendHeader(PSTR("Content-Disposition"), String(PSTR("attachment; filename=\"gbs-backup-")) + ts + String(PSTR(".bin\"")));
    server.sendHeader(PSTR("Cache-Control"), PSTR("private, max-age=0"));
    server.sendHeader(PSTR("Content-Description"), PSTR("File Transfer"));
    server.sendHeader(PSTR("Content-Transfer-Encoding"), PSTR("binary"));
    server.send(200, mimeOctetStream, _b.c_str(), _b.length());
    /* server.streamFile(_b, mimeOctetStream);
    // cleanup
    _b.close();
    LittleFS.remove(FPSTR(backupFile)); */

    webSocket.begin();
}

/**
 * @brief Restore data from backupFile
 *
 */
void extractBackup() {
    char buffer[32] = "";
    char fname[32] = "";
    size_t fsize = 0;
    fs::File _f;
    fs::File _b = LittleFS.open(FPSTR(backupFile), "r");
    if(!_b) {
        _DBGN(F("failed to access backup file, abort..."));
        return;
    }
    // process files
    while(_b.available()) {
        // name
        if(_b.readBytesUntil('\n', buffer, 32) != 0) {
            strncpy(fname, buffer, 32);
            // fix last captured character
            fname[strlen(fname)-1] = '\0';
            memset(buffer, '\0', 32);
            // it is not slots.bin file
            // if(strcmp(fname, String(slotsFile).substring(1).c_str()) != 0) {
                if(_b.readBytesUntil('\n', buffer, 32) != 0) {
                    fsize = (size_t)String(buffer).toInt();
                    if(fsize != 0) {
                        size_t cursor = 0;
                        _f = LittleFS.open(fname, "w");
                        // transfer data
                        if(_f) {
                            while(_b.available() && cursor != fsize) {
                                _f.write(_b.read());
                                cursor ++;
                            }
                            _f.close();
                            _DBGF(PSTR("%s extraction complete, size: %ld\n"), fname, fsize);
                        } else
                            _DBGF(PSTR("unable to open/create %s (size: %ld)\n"), fname, fsize);
                    }
                }
            /* } else {
                // it is slots.bin file
                _f = LittleFS.open(FPSTR(slotsFile), "r");
                if(_f) {
                    SlotMetaArray slotsObject;
                    _f.read((byte *)&slotsObject, sizeof(slotsObject));
                    _f.close();
                    uint8_t k = 0;
                    String data = "";
                    while(_b.available()) {
                        // starts from slotID, so it's stop string or slotID
                        data =_b.readString();
                        // stop condition, see serverBackupDownload()
                        if(data.compareTo(String(F("\t\t"))) == 0)
                            break;
                        // mind data order!
                        k = data.toInt();
                        strcpy(slotsObject.slot[k].name, _b.readString().c_str());
                        slotsObject.slot[k].resolutionID = _b.readString().charAt(0);
                        slotsObject.slot[k].scanlines = _b.readString().toInt();
                        slotsObject.slot[k].scanlinesStrength = _b.readString().toInt();
                        slotsObject.slot[k].wantVdsLineFilter = _b.readString().toInt();
                        slotsObject.slot[k].wantStepResponse = _b.readString().toInt();
                        slotsObject.slot[k].wantPeaking = _b.readString().toInt();
                    }
                    // open for writing
                    _f = LittleFS.open(FPSTR(slotsFile), "w");
                    _f.write((byte *)&slotsObject, sizeof(slotsObject));
                    _f.close();
                    _DBGN(F("slots.bin extraction complete"));
                } else
                    _DBGN(F("unable to open/create slots.bin"));
            } */
        }
    }
    _b.close();
    // cleanup
    LittleFS.remove(FPSTR(backupFile));
    _DBGN(F("restore success, restarting..."));
}

/**
 * @brief
 *
 */
// void serverFsDir()
// {
//     String output = "[";
//     fs::Dir dir = LittleFS.openDir("/");
//     ASSERT_LOMEM_GOTO(server_fs_dir_failure);

//     while (dir.next())
//     {
//         if (dir.fileName().indexOf(PSTR(".html")) == -1)
//         {
//             output += "\"";
//             output += dir.fileName();
//             output += "\",";
//             delay(1); // wifi stack
//         }
//     }

//     output += "]";

//     output.replace(",]", "]");

//     server.send(200, mimeAppJson, output);
//     return;

// server_fs_dir_failure:
//     server.send_P(200, mimeAppJson, PSTR("false"));
// }

/**
 * @brief Do not use format method.
 *      We need a soft method to be able to remove all files
 *      except ones that strictly required
 *
 */
// void serverFsFormat()
// {
//     server.send(200, mimeAppJson, LittleFS.format() ? F("true") : F("false"));
// }

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
                    : String(F("{\"mode\":\"sta\",\"ssid\":\"") + WiFi.SSID() + F("\"}")));
}

/**
 * @brief
 *
 */
void serverRestoreFilters()
{
    SlotMetaArray slotsObject;
    fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");
    bool result = false;
    if (slotsBinaryFile)
    {
        slotsBinaryFile.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();
        // auto currentSlot = String(slotIndexMap).indexOf(uopt->presetSlot);
        // if (currentSlot == -1)
        // {
        //     goto server_restore_filters_end;
        // }

        uopt->wantScanlines = slotsObject.slot[uopt->presetSlot].scanlines;

        _WSF(PSTR("slot: %s (ID %d) scanlines: "), slotsObject.slot[uopt->presetSlot].name, uopt->presetSlot);
        if (uopt->wantScanlines)
        {
            _WSN(F("on (Line Filter recommended)"));
        }
        else
        {
            disableScanlines();
            _WSN(F("off"));
        }
        saveUserPrefs();

        uopt->scanlineStrength = slotsObject.slot[uopt->presetSlot].scanlinesStrength;
        uopt->wantVdsLineFilter = slotsObject.slot[uopt->presetSlot].wantVdsLineFilter;
        uopt->wantStepResponse = slotsObject.slot[uopt->presetSlot].wantStepResponse;
        uopt->wantPeaking = slotsObject.slot[uopt->presetSlot].wantPeaking;
        result = true;
    }

// server_restore_filters_end:
    server.send(200, mimeAppJson, result ? F("true") : F("false"));
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
    while (n == -1)
    {
        n = WiFi.scanComplete();
        yield();
    }
    if (n > 0)
    {
        // TODO: fuzzy logic
        // build array of indices
        int ix[n];
        for (i = 0; i < n; i++)
            ix[i] = i;

        // sort by signal strength
        for (i = 0; i < n; i++)
        {
            for (int j = 1; j < n - i; j++)
            {
                if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1]))
                {
                    std::swap(ix[j], ix[j - 1]);
                }
            }
        }
        // remove duplicates
        for (i = 0; i < n; i++)
        {
            for (int j = i + 1; j < n; j++)
            {
                if (WiFi.SSID(ix[i]).equals(WiFi.SSID(ix[j])) && WiFi.encryptionType(ix[i]) == WiFi.encryptionType(ix[j]))
                    ix[j] = -1;
            }
        }
        // TODO: END
        i = 0;
        server.chunkedResponseModeStart(200, mimeTextHtml);
        while (i < n && ix[i] != -1)
        { // check s.length to limit memory usage
            s = String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2) + "," + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1) + "," + WiFi.SSID(ix[i]);
            server.sendContent(s.c_str());
            i++;
        }
        server.chunkedResponseFinalize();
    }
    else
        server.send(200, mimeTextHtml, F("{}"));
}

/**
 * @brief
 *
 */
void serverWiFiWPS()
{
    // if (WiFi.status() != WL_CONNECTED) {
    if (wifiStartWPS())
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
 * @brief switch WiFi in STA mode
 *
 */
void serverWiFiConnect()
{
    String ssid = server.arg(String(F("n")));
    String pwd = server.arg(String(F("p")));

    if (ssid.length() > 0) {
        wifiStartStaMode(ssid, pwd);
        server.send(200, mimeAppJson, F("{}"));
    } else
        server.send(406, mimeAppJson, F("{}"));

    // delay(100);
    // ESP.reset();
    userCommand = 'a'; // restart device at the next loop()
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
 * @brief Prints current system & PLC parameters
 *         into WS and Serial
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
    if (!GBS::STATUS_SYNC_PROC_HSACT::read())
    {
        h = HSp = ' ';
    }
    if (!GBS::STATUS_SYNC_PROC_VSACT::read())
    {
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

    if (stat0FIrq != 0x00)
    {
        // clear 0_0F interrupt bits regardless of syncwatcher status
        clearIrqCounter++;
        if (clearIrqCounter >= 50)
        {
            clearIrqCounter = 0;
            GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
            GBS::INTERRUPT_CONTROL_00::write(0x00);
        }
    }

    yield();
    
    if (GBS::STATUS_SYNC_PROC_HSACT::read())
    { // else source might not be active
        for (uint8_t i = 0; i < 9; i++)
        {
            if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1)
            {
                lockCounter++;
            }
            else
            {
                for (int i = 0; i < 10; i++)
                {
                    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1)
                    {
                        lockCounter++;
                        break;
                    }
                }
            }
        }
    }
    delay(10);
    lockCounterPrevious = getMovingAverage(lockCounter);
}

/**
 * @brief
 *
 */
void printVideoTimings()
{
    // if (rto->presetID < 0x20) {
    if (rto->resolutionID != PresetHdBypass && rto->resolutionID != PresetBypassRGBHV)
    {
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
            GBS::VDS_HB_SP::read());
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
            GBS::VDS_VB_SP::read());
        // IF V offset
        _WSF(
            PSTR("IF VB ST/SP  : %d %d\n"),
            GBS::IF_VB_ST::read(),
            GBS::IF_VB_SP::read());
    }
    else
    {
        _WSF(
            PSTR("\nHD_HSYNC_RST : %d\nHD_INI_ST    : %d\nHS ST/SP     : %d %d\nHB ST/SP     : %d %d\n------\n"),
            GBS::HD_HSYNC_RST::read(),
            GBS::HD_INI_ST::read(),
            GBS::SP_CS_HS_ST::read(),
            GBS::SP_CS_HS_SP::read(),
            GBS::HD_HB_ST::read(),
            GBS::HD_HB_SP::read());
        // vertical
        _WSF(
            PSTR("VS ST/SP     : %d %d\nVB ST/SP     : %d %d\n"),
            GBS::HD_VS_ST::read(),
            GBS::HD_VS_SP::read(),
            GBS::HD_VB_ST::read(),
            GBS::HD_VB_SP::read());
    }

    _WSF(
        PSTR("CsVT         : %d\nCsVS_ST/SP   : %d %d\n"),
        GBS::STATUS_SYNC_PROC_VTOTAL::read(),
        getCsVsStart(),
        getCsVsStop());
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
    if (FrameSync::vsyncInputSample(&inStart, &inStop))
    {
        inPeriod = (inStop - inStart) >> 1;
        if (inPeriod > 1)
        {
            inHz = (double)1000000 / (double)inPeriod;
        }
        _WSF(PSTR("inPeriod: %ld in Hz: %d\n"), inPeriod, inHz);
    }
    else
    {
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
        bestHtotal60);
    // if (bestHtotal > 800 && bestHtotal < 3200) {
    // applyBestHTotal((uint16_t)bestHtotal);
    // FrameSync::resetWithoutRecalculation(); // was single use of this function, function has changed since
    // }
}

#if defined(ESP8266)

/**
 * @brief extrated from main.cpp->loop()
 *
 */
void handleSerialCommand()
{
    static uint8_t readout = 0;
    static uint8_t segmentCurrent = 255;
    static uint8_t registerCurrent = 255;
    static uint8_t inputToogleBit = 0;
    static uint8_t inputStage = 0;

    if (Serial.available())
    {
        serialCommand = Serial.read();
    }
    else if (inputStage > 0)
    {
        // multistage with no more data
        _WSN(F(" abort"));
        discardSerialRxData();
        serialCommand = ' ';
    }

    if (serialCommand != '@')
    {
        _WSF(commandDescr,
            "serial",
            serialCommand,
            serialCommand,
            // uopt->presetPreference,
            uopt->presetSlot,
            // rto->presetID
            rto->resolutionID,
            rto->resolutionID);
        // multistage with bad characters?
        if (inputStage > 0)
        {
            // need 's', 't' or 'g'
            if (serialCommand != 's' && serialCommand != 't' && serialCommand != 'g')
            {
                discardSerialRxData();
                _WSN(F(" abort"));
                serialCommand = ' ';
            }
        }

        switch (serialCommand)
        {
        case ' ':
            // skip spaces
            inputStage = segmentCurrent = registerCurrent = 0; // and reset these
            break;
        case 'd':
        {
            // don't store scanlines
            if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1)
            {
                disableScanlines();
            }
            // pal forced 60hz: no need to revert here. let the voffset function handle it

            if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0)
            {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            // dump
            for (int segment = 0; segment <= 5; segment++)
            {
                dumpRegisters(segment);
            }
            _WSN(F("};"));
        }
        break;
        case '+':
            _WSN(F("hor. +"));
            shiftHorizontalRight();
            // shiftHorizontalRightIF(4);
            break;
        case '-':
            _WSN(F("hor. -"));
            shiftHorizontalLeft();
            // shiftHorizontalLeftIF(4);
            break;
        case '*':
            shiftVerticalUpIF();
            break;
        case '/':
            shiftVerticalDownIF();
            break;
        case 'z':
            _WSN(F("scale+"));
            scaleHorizontal(2, true);
            break;
        case 'h':
            _WSN(F("scale-"));
            scaleHorizontal(2, false);
            break;
        case 'q':
            resetDigital();
            delay(2);
            ResetSDRAM();
            delay(2);
            togglePhaseAdjustUnits();
            // enableVDS();
            break;
        case 'D':
            _WS(F("debug view: "));
            if (GBS::ADC_UNUSED_62::read() == 0x00)
            { // "remembers" debug view
                // if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(0); } // 3_4e 0 // enable peaking but don't store
                GBS::VDS_PK_LB_GAIN::write(0x3f);                   // 3_45
                GBS::VDS_PK_LH_GAIN::write(0x3f);                   // 3_47
                GBS::ADC_UNUSED_60::write(GBS::VDS_Y_OFST::read()); // backup
                GBS::ADC_UNUSED_61::write(GBS::HD_Y_OFFSET::read());
                GBS::ADC_UNUSED_62::write(1); // remember to remove on restore
                GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 0x24);
                GBS::HD_Y_OFFSET::write(GBS::HD_Y_OFFSET::read() + 0x24);
                if (!rto->inputIsYPbPr)
                {
                    // RGB input that has HD_DYN bypassed, use it now
                    GBS::HD_DYN_BYPS::write(0);
                    GBS::HD_U_OFFSET::write(GBS::HD_U_OFFSET::read() + 0x24);
                    GBS::HD_V_OFFSET::write(GBS::HD_V_OFFSET::read() + 0x24);
                }
                // GBS::IF_IN_DREG_BYPS::write(1); // enhances noise from not delaying IF processing properly
                _WSN(F("on"));
            }
            else
            {
                // if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(1); } // 3_4e 0
                // if (rto->presetID == 0x05) {
                if (rto->resolutionID == Output1080p)
                {
                    GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                    GBS::VDS_PK_LH_GAIN::write(0x0A); // 3_47
                }
                else
                {
                    GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                    GBS::VDS_PK_LH_GAIN::write(0x18); // 3_47
                }
                GBS::VDS_Y_OFST::write(GBS::ADC_UNUSED_60::read()); // restore
                GBS::HD_Y_OFFSET::write(GBS::ADC_UNUSED_61::read());
                if (!rto->inputIsYPbPr)
                {
                    // RGB input, HD_DYN_BYPS again
                    GBS::HD_DYN_BYPS::write(1);
                    GBS::HD_U_OFFSET::write(0); // probably just 0 by default
                    GBS::HD_V_OFFSET::write(0); // probably just 0 by default
                }
                // GBS::IF_IN_DREG_BYPS::write(0);
                GBS::ADC_UNUSED_60::write(0); // .. and clear
                GBS::ADC_UNUSED_61::write(0);
                GBS::ADC_UNUSED_62::write(0);
                _WSN(F("off"));
            }
            serialCommand = '@';
            break;
        case 'C':
            _WSN(F("PLL: ICLK"));
            // display clock in last test best at 0x85
            GBS::PLL648_CONTROL_01::write(0x85);
            GBS::PLL_CKIS::write(1); // PLL use ICLK (instead of oscillator)
            latchPLLAD();
            // GBS::VDS_HSCALE::write(512);
            rto->syncLockFailIgnore = 16;
            FrameSync::reset(uopt->frameTimeLockMethod); // adjust htotal to new display clock
            rto->forceRetime = true;
            // applyBestHTotal(FrameSync::init()); // adjust htotal to new display clock
            // applyBestHTotal(FrameSync::init()); // twice
            // GBS::VDS_FLOCK_EN::write(1); //risky
            delay(200);
            break;
        case 'Y':
            writeProgramArrayNew(ntsc_1280x720, false);
            doPostPresetLoadSteps();
            break;
        case 'y':
            writeProgramArrayNew(pal_1280x720, false);
            doPostPresetLoadSteps();
            break;
        case 'P':
            _WS(F("auto deinterlace: "));
            rto->deinterlaceAutoEnabled = !rto->deinterlaceAutoEnabled;
            if (rto->deinterlaceAutoEnabled)
            {
                _WSN(F("on"));
            }
            else
            {
                _WSN(F("off"));
            }
            break;
        case 'p':
            if (!rto->motionAdaptiveDeinterlaceActive)
            {
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1)
                { // don't rely on rto->scanlinesEnabled
                    disableScanlines();
                }
                enableMotionAdaptDeinterlace();
            }
            else
            {
                disableMotionAdaptDeinterlace();
            }
            break;
        case 'k':
            bypassModeSwitch_RGBHV();
            break;
        case 'K': // set outputBypass
            setOutModeHdBypass(false);
            // uopt->presetPreference = OutputBypass;
            rto->resolutionID = OutputBypass;
            _WSN(F("OutputBypass mode is now active"));
            saveUserPrefs();
            break;
        case 'T':
            _WS(F("auto gain "));
            if (uopt->enableAutoGain == 0)
            {
                uopt->enableAutoGain = 1;
                setAdcGain(AUTO_GAIN_INIT);
                GBS::DEC_TEST_ENABLE::write(1);
                _WSN(F("on"));
            }
            else
            {
                uopt->enableAutoGain = 0;
                GBS::DEC_TEST_ENABLE::write(0);
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case 'e':
            writeProgramArrayNew(ntsc_240p, false);
            doPostPresetLoadSteps();
            break;
        case 'r':
            writeProgramArrayNew(pal_240p, false);
            doPostPresetLoadSteps();
            break;
        case '.':
        {
            if (!rto->outModeHdBypass)
            {
                // bestHtotal recalc
                rto->autoBestHtotalEnabled = true;
                rto->syncLockFailIgnore = 16;
                rto->forceRetime = true;
                FrameSync::reset(uopt->frameTimeLockMethod);

                if (!rto->syncWatcherEnabled)
                {
                    bool autoBestHtotalSuccess = 0;
                    delay(30);
                    autoBestHtotalSuccess = runAutoBestHTotal();
                    if (!autoBestHtotalSuccess)
                    {
                        _WSN(F("(unchanged)"));
                    }
                }
            }
        }
        break;
        case '!':
            // fastGetBestHtotal();
            // readEeprom();
            _WSF(PSTR("sfr: %.02f\n pll: %l\n"), getSourceFieldRate(1), getPllRate());
            break;
        case '$':
        {
            // EEPROM write protect pin (7, next to Vcc) is under original MCU control
            // MCU drags to Vcc to write, EEPROM drags to Gnd normally
            // This routine only works with that "WP" pin disconnected
            // 0x17 = input selector? // 0x18 = input selector related?
            // 0x54 = coast start 0x55 = coast end
            uint16_t writeAddr = 0x54;
            const uint8_t eepromAddr = 0x50;
            for (; writeAddr < 0x56; writeAddr++)
            {
                Wire.beginTransmission(eepromAddr);
                Wire.write(writeAddr >> 8);     // high addr byte, 4 bits +
                Wire.write((uint8_t)writeAddr); // mlow addr byte, 8 bits = 12 bits (0xfff max)
                Wire.write(0x10);               // coast end value ?
                Wire.endTransmission();
                delay(5);
            }

            // Wire.beginTransmission(eepromAddr);
            // Wire.write((uint8_t)0); Wire.write((uint8_t)0);
            // Wire.write(0xff); // force eeprom clear probably
            // Wire.endTransmission();
            // delay(5);

            _WSN(F("done"));
        }
        break;
        case 'j':
            // resetPLL();
            latchPLLAD();
            break;
        case 'J':
            resetPLLAD();
            break;
        case 'v':
            rto->phaseSP += 1;
            rto->phaseSP &= 0x1f;
            _WSF(PSTR("SP: %d\n"), rto->phaseSP);
            setAndLatchPhaseSP();
            // setAndLatchPhaseADC();
            break;
        case 'b':
            advancePhase();
            latchPLLAD();
            _WSF(PSTR("ADC: %d\n"), rto->phaseADC);
            break;
        case '#':
            rto->videoStandardInput = 13;
            applyPresets(13);
            // _WSN(getStatus00IfHsVsStable());
            // globalDelay++;
            // _WSN(globalDelay);
            break;
        case 'n':
        {
            uint16_t pll_divider = GBS::PLLAD_MD::read();
            pll_divider += 1;
            GBS::PLLAD_MD::write(pll_divider);
            GBS::IF_HSYNC_RST::write((pll_divider / 2));
            GBS::IF_LINE_SP::write(((pll_divider / 2) + 1) + 0x40);
            _WSF(PSTR("PLL div: 0x%04X %d\n"), pll_divider, pll_divider);
            // set IF before latching
            // setIfHblankParameters();
            latchPLLAD();
            delay(1);
            // applyBestHTotal(GBS::VDS_HSYNC_RST::read());
            updateClampPosition();
            updateCoastPosition(0);
        }
        break;
        case 'N':
        {
            // if (GBS::RFF_ENABLE::read()) {
            //   disableMotionAdaptDeinterlace();
            // }
            // else {
            //   enableMotionAdaptDeinterlace();
            // }

            // GBS::RFF_ENABLE::write(!GBS::RFF_ENABLE::read());

            if (rto->scanlinesEnabled)
            {
                rto->scanlinesEnabled = false;
                disableScanlines();
            }
            else
            {
                rto->scanlinesEnabled = true;
                enableScanlines();
            }
        }
        break;
        case 'a':
            _WSF(PSTR("HTotal++: %d\n"), GBS::VDS_HSYNC_RST::read() + 1);
            if (GBS::VDS_HSYNC_RST::read() < 4095)
            {
                if (uopt->enableFrameTimeLock)
                {
                    // syncLastCorrection != 0 check is included
                    FrameSync::reset(uopt->frameTimeLockMethod);
                }
                rto->forceRetime = 1;
                applyBestHTotal(GBS::VDS_HSYNC_RST::read() + 1);
            }
            break;
        case 'A':
            _WSF(PSTR("HTotal--: %d\n"), GBS::VDS_HSYNC_RST::read() - 1);
            if (GBS::VDS_HSYNC_RST::read() > 0)
            {
                if (uopt->enableFrameTimeLock)
                {
                    // syncLastCorrection != 0 check is included
                    FrameSync::reset(uopt->frameTimeLockMethod);
                }
                rto->forceRetime = 1;
                applyBestHTotal(GBS::VDS_HSYNC_RST::read() - 1);
            }
            break;
        case 'M':
        {
        }
        break;
        case 'm':
            _WS(F("syncwatcher "));
            if (rto->syncWatcherEnabled == true)
            {
                rto->syncWatcherEnabled = false;
                if (rto->videoIsFrozen)
                {
                    unfreezeVideo();
                }
                _WSN(F("off"));
            }
            else
            {
                rto->syncWatcherEnabled = true;
                _WSN(F("on"));
            }
            break;
        case ',':
            printVideoTimings();
            break;
        case 'i':
            rto->printInfos = !rto->printInfos;
            break;
        case 'c':               // enableOTA mode
            initUpdateOTA();
            rto->allowUpdatesOTA = true;
            _WSN(F("OTA Updates on"));
            break;
        case 'G':
            _WS(F("Debug Pings "));
            if (!rto->enableDebugPings)
            {
                _WSN(F("on"));
                rto->enableDebugPings = 1;
            }
            else
            {
                _WSN(F("off"));
                rto->enableDebugPings = 0;
            }
            break;
        case 'u':
            ResetSDRAM();
            break;
        case 'f':
            _WS(F("peaking "));
            if (uopt->wantPeaking == 0)
            {
                uopt->wantPeaking = 1;
                GBS::VDS_PK_Y_H_BYPS::write(0);
                _WSN(F("on"));
            }
            else
            {
                uopt->wantPeaking = 0;
                GBS::VDS_PK_Y_H_BYPS::write(1);
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case 'F': // switch ADC filter button (dev.mode)
            _WS(F("ADC filter "));
            if (GBS::ADC_FLTR::read() > 0)
            {
                GBS::ADC_FLTR::write(0);
                _WSN(F("off"));
            }
            else
            {
                GBS::ADC_FLTR::write(3);
                _WSN(F("on"));
            }
            break;
        case 'L':
        {
            // Component / VGA Output
            uopt->wantOutputComponent = !uopt->wantOutputComponent;
            OutputComponentOrVGA();
            saveUserPrefs();
            // apply 1280x720 preset now, otherwise a reboot would be required
            uint8_t videoMode = getVideoMode();
            if (videoMode == 0)
                videoMode = rto->videoStandardInput;
            // OutputResolution backup = rto->presetID;
            OutputResolution backup = rto->resolutionID;
            // uopt->presetPreference = Output720P;
            // rto->presetID = Output720p;
            rto->resolutionID = Output720p;
            rto->videoStandardInput = 0; // force hard reset
            applyPresets(videoMode);
            // rto->presetID = backup;
            rto->resolutionID = backup;
            // uopt->presetPreference = backup;
        }
        break;
        case 'l':
            _WSN(F("resetSyncProcessor"));
            // freezeVideo();
            resetSyncProcessor();
            // delay(10);
            // unfreezeVideo();
            break;
        case 'Z':
        {
            uopt->matchPresetSource = !uopt->matchPresetSource;
            saveUserPrefs();
            uint8_t vidMode = getVideoMode();
            // if (uopt->presetPreference == 0 && GBS::GBS_PRESET_ID::read() == 0x11) {
            if (rto->resolutionID == Output240p && GBS::GBS_PRESET_ID::read() == Output960p50)
            {
                applyPresets(vidMode);
                // } else if (uopt->presetPreference == 4 && GBS::GBS_PRESET_ID::read() == 0x02) {
            }
            else if (rto->resolutionID == Output1024p && GBS::GBS_PRESET_ID::read() == Output1024p)
            {
                applyPresets(vidMode);
            }
        }
        break;
        case 'W':
            uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
            break;
        case 'E':
            writeProgramArrayNew(ntsc_1280x1024, false);
            _WSN(F("ntsc_1280x1024 is now active"));
            doPostPresetLoadSteps();
            break;
        case 'R':
            writeProgramArrayNew(pal_1280x1024, false);
            _WSN(F("pal_1280x1024 is now active"));
            doPostPresetLoadSteps();
            break;
        case '0':
            moveHS(4, true);
            break;
        case '1':
            moveHS(4, false);
            break;
        case '2':
            writeProgramArrayNew(pal_768x576, false); // ModeLine "720x576@50" 27 720 732 795 864 576 581 586 625 -hsync -vsync
            _WSN(F("pal_768x576 is now active"));
            doPostPresetLoadSteps();
            break;
        case '3':
            //
            break;
        case '4':
        {
            // scale vertical +
            if (GBS::VDS_VSCALE::read() <= 256)
            {
                _WSN(F("limit"));
                break;
            }
            scaleVertical(2, true);
            // actually requires full vertical mask + position offset calculation
        }
        break;
        case '5':
        {
            // scale vertical -
            if (GBS::VDS_VSCALE::read() == 1023)
            {
                _WSN(F("limit"));
                break;
            }
            scaleVertical(2, false);
            // actually requires full vertical mask + position offset calculation
        }
        break;
        case '6':
            if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass)
            {
                if (GBS::IF_HBIN_SP::read() >= 10)
                {                                                        // IF_HBIN_SP: min 2
                    GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() - 8); // canvas move right
                    if ((GBS::IF_HSYNC_RST::read() - 4) > ((GBS::PLLAD_MD::read() >> 1) + 5))
                    {
                        GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() - 4); // shrink 1_0e
                        GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 4);     // and 1_22 to go with it
                    }
                }
                else
                {
                    _WSN(F("limit"));
                }
            }
            else if (!rto->outModeHdBypass)
            {
                if (GBS::IF_HB_SP2::read() >= 4)
                    GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() - 4); // 1_1a
                else
                    GBS::IF_HB_SP2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                if (GBS::IF_HB_ST2::read() >= 4)
                    GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() - 4); // 1_18
                else
                    GBS::IF_HB_ST2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                _WSF(PSTR("IF_HB_ST2: 0x%04X IF_HB_SP2: 0x%04X\n"),
                    GBS::IF_HB_ST2::read(), GBS::IF_HB_SP2::read());
            }
            break;
        case '7':
            if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass)
            {
                if (GBS::IF_HBIN_SP::read() < 0x150)
                {                                                        // (arbitrary) max limit
                    GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() + 8); // canvas move left
                }
                else
                {
                    _WSN(F("limit"));
                }
            }
            else if (!rto->outModeHdBypass)
            {
                if (GBS::IF_HB_SP2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                    GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() + 4); // 1_1a
                else
                    GBS::IF_HB_SP2::write(0);
                if (GBS::IF_HB_ST2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                    GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() + 4); // 1_18
                else
                    GBS::IF_HB_ST2::write(0);
                _WSF(PSTR("IF_HB_ST2: 0x%04X IF_HB_SP2: 0x%04X\n"),
                     GBS::IF_HB_ST2::read(), GBS::IF_HB_SP2::read());
            }
            break;
        case '8': // invert sync button (dev. mode)
            // _WSN("invert sync");
            rto->invertSync = !rto->invertSync;
            invertHS();
            invertVS();
            // optimizePhaseSP();
            break;
        case '9':
            writeProgramArrayNew(ntsc_720x480, false);
            doPostPresetLoadSteps();
            break;
        case 'o':
        { // oversampring button (dev.mode)
            if (rto->osr == 1)
            {
                setOverSampleRatio(2, false);
            }
            else if (rto->osr == 2)
            {
                setOverSampleRatio(4, false);
            }
            else if (rto->osr == 4)
            {
                setOverSampleRatio(1, false);
            }
            delay(4);
            optimizePhaseSP();
            _WSF(PSTR("OSR 0x%02X\n"), rto->osr);
            rto->phaseIsSet = 0; // do it again in modes applicable
        }
        break;
        case 'g':
            inputStage++;
            // we have a multibyte command
            if (inputStage > 0)
            {
                if (inputStage == 1)
                {
                    segmentCurrent = Serial.parseInt();
                    _WSF(PSTR("G%d"), segmentCurrent);
                }
                else if (inputStage == 2)
                {
                    char szNumbers[3];
                    szNumbers[0] = Serial.read();
                    szNumbers[1] = Serial.read();
                    szNumbers[2] = '\0';
                    // char * pEnd;
                    registerCurrent = strtol(szNumbers, NULL, 16);
                    _WSF(PSTR("R 0x%04X"), registerCurrent);
                    if (segmentCurrent <= 5)
                    {
                        writeOneByte(0xF0, segmentCurrent);
                        readFromRegister(registerCurrent, 1, &readout);
                        _WSF(PSTR(" value: 0x%02X\n"), readout);
                    }
                    else
                    {
                        discardSerialRxData();
                        _WSN(F("abort"));
                    }
                    inputStage = 0;
                }
            }
            break;
        case 's':
            inputStage++;
            // we have a multibyte command
            if (inputStage > 0)
            {
                if (inputStage == 1)
                {
                    segmentCurrent = Serial.parseInt();
                    _WSF(PSTR("S%d"), segmentCurrent);
                }
                else if (inputStage == 2)
                {
                    char szNumbers[3];
                    for (uint8_t a = 0; a <= 1; a++)
                    {
                        // ascii 0x30 to 0x39 for '0' to '9'
                        if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                            (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                            (Serial.peek() >= 'A' && Serial.peek() <= 'F'))
                        {
                            szNumbers[a] = Serial.read();
                        }
                        else
                        {
                            szNumbers[a] = 0; // NUL char
                            Serial.read();    // but consume the char
                        }
                    }
                    szNumbers[2] = '\0';
                    // char * pEnd;
                    registerCurrent = strtol(szNumbers, NULL, 16);
                    _WSF(PSTR("R 0x%04X"), registerCurrent);
                }
                else if (inputStage == 3)
                {
                    char szNumbers[3];
                    for (uint8_t a = 0; a <= 1; a++)
                    {
                        if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                            (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                            (Serial.peek() >= 'A' && Serial.peek() <= 'F'))
                        {
                            szNumbers[a] = Serial.read();
                        }
                        else
                        {
                            szNumbers[a] = 0; // NUL char
                            Serial.read();    // but consume the char
                        }
                    }
                    szNumbers[2] = '\0';
                    // char * pEnd;
                    inputToogleBit = strtol(szNumbers, NULL, 16);
                    if (segmentCurrent <= 5)
                    {
                        writeOneByte(0xF0, segmentCurrent);
                        readFromRegister(registerCurrent, 1, &readout);
                        _WSF(PSTR(" (was 0x%04X)"), readout);
                        writeOneByte(registerCurrent, inputToogleBit);
                        readFromRegister(registerCurrent, 1, &readout);
                        _WSF(PSTR(" is now: 0x%02X\n"), readout);
                    }
                    else
                    {
                        discardSerialRxData();
                        _WSN(F("abort"));
                    }
                    inputStage = 0;
                }
            }
            break;
        case 't':
            inputStage++;
            // we have a multibyte command
            if (inputStage > 0)
            {
                if (inputStage == 1)
                {
                    segmentCurrent = Serial.parseInt();
                    _WSF(PSTR("T%d"), segmentCurrent);
                }
                else if (inputStage == 2)
                {
                    char szNumbers[3];
                    for (uint8_t a = 0; a <= 1; a++)
                    {
                        // ascii 0x30 to 0x39 for '0' to '9'
                        if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                            (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                            (Serial.peek() >= 'A' && Serial.peek() <= 'F'))
                        {
                            szNumbers[a] = Serial.read();
                        }
                        else
                        {
                            szNumbers[a] = 0; // NUL char
                            Serial.read();    // but consume the char
                        }
                    }
                    szNumbers[2] = '\0';
                    // char * pEnd;
                    registerCurrent = strtol(szNumbers, NULL, 16);
                    _WSF(PSTR("R 0x%04X"), registerCurrent);
                }
                else if (inputStage == 3)
                {
                    if (Serial.peek() >= '0' && Serial.peek() <= '7')
                    {
                        inputToogleBit = Serial.parseInt();
                    }
                    else
                    {
                        inputToogleBit = 255; // will get discarded next step
                    }
                    _WSF(PSTR(" Bit: %d"), inputToogleBit);
                    inputStage = 0;
                    if ((segmentCurrent <= 5) && (inputToogleBit <= 7))
                    {
                        writeOneByte(0xF0, segmentCurrent);
                        readFromRegister(registerCurrent, 1, &readout);
                        _WSF(PSTR(" (was 0x%04X)"), readout);
                        writeOneByte(registerCurrent, readout ^ (1 << inputToogleBit));
                        readFromRegister(registerCurrent, 1, &readout);
                        _WSF(PSTR(" is now: 0x%02X\n"), readout);
                    }
                    else
                    {
                        discardSerialRxData();
                        inputToogleBit = registerCurrent = 0;
                        _WSN(F("abort"));
                    }
                }
            }
            break;
        case '<':
        {
            if (segmentCurrent != 255 && registerCurrent != 255)
            {
                writeOneByte(0xF0, segmentCurrent);
                readFromRegister(registerCurrent, 1, &readout);
                writeOneByte(registerCurrent, readout - 1); // also allow wrapping
                _WSF(PSTR("S%d_0x%04X"), segmentCurrent, registerCurrent);
                readFromRegister(registerCurrent, 1, &readout);
                _WSF(PSTR(" : 0x%04X\n"), readout);
            }
        }
        break;
        case '>':
        {
            if (segmentCurrent != 255 && registerCurrent != 255)
            {
                writeOneByte(0xF0, segmentCurrent);
                readFromRegister(registerCurrent, 1, &readout);
                writeOneByte(registerCurrent, readout + 1);
                _WSF(PSTR("S%d_0x%04X"), segmentCurrent, registerCurrent);
                readFromRegister(registerCurrent, 1, &readout);
                _WSF(PSTR(" : 0x%04X\n"), readout);
            }
        }
        break;
        case '_':
        {
            uint32_t ticks = FrameSync::getPulseTicks();
            _WSN(ticks);
        }
        break;
        case '~':
            goLowPowerWithInputDetection(); // test reset + input detect
            break;
        case 'w':
        {
            // Serial.flush();
            uint16_t value = 0;
            String what = Serial.readStringUntil(' ');

            if (what.length() > 5)
            {
                _WSN(F("abort"));
                inputStage = 0;
                break;
            }
            if (what.equals("f"))
            {
                if (rto->extClockGenDetected)
                {
                    _WSF(PSTR("old freqExtClockGen: %l\n"), rto->freqExtClockGen);
                    rto->freqExtClockGen = Serial.parseInt();
                    // safety range: 1 - 250 MHz
                    if (rto->freqExtClockGen >= 1000000 && rto->freqExtClockGen <= 250000000)
                    {
                        Si.setFreq(0, rto->freqExtClockGen);
                        rto->clampPositionIsSet = 0;
                        rto->coastPositionIsSet = 0;
                    }
                    _WSF(PSTR("set freqExtClockGen: %l\n"), rto->freqExtClockGen);
                }
                break;
            }

            value = Serial.parseInt();
            if (value < 4096)
            {
                _WSF(PSTR("set %s %d\n"), what.c_str(), value);
                if (what.equals("ht"))
                {
                    // set_htotal(value);
                    if (!rto->outModeHdBypass)
                    {
                        rto->forceRetime = 1;
                        applyBestHTotal(value);
                    }
                    else
                    {
                        GBS::VDS_HSYNC_RST::write(value);
                    }
                }
                else if (what.equals("vt"))
                {
                    set_vtotal(value);
                }
                else if (what.equals("hsst"))
                {
                    setHSyncStartPosition(value);
                }
                else if (what.equals("hssp"))
                {
                    setHSyncStopPosition(value);
                }
                else if (what.equals("hbst"))
                {
                    setMemoryHblankStartPosition(value);
                }
                else if (what.equals("hbsp"))
                {
                    setMemoryHblankStopPosition(value);
                }
                else if (what.equals("hbstd"))
                {
                    setDisplayHblankStartPosition(value);
                }
                else if (what.equals("hbspd"))
                {
                    setDisplayHblankStopPosition(value);
                }
                else if (what.equals("vsst"))
                {
                    setVSyncStartPosition(value);
                }
                else if (what.equals("vssp"))
                {
                    setVSyncStopPosition(value);
                }
                else if (what.equals("vbst"))
                {
                    setMemoryVblankStartPosition(value);
                }
                else if (what.equals("vbsp"))
                {
                    setMemoryVblankStopPosition(value);
                }
                else if (what.equals("vbstd"))
                {
                    setDisplayVblankStartPosition(value);
                }
                else if (what.equals("vbspd"))
                {
                    setDisplayVblankStopPosition(value);
                }
                else if (what.equals("sog"))
                {
                    setAndUpdateSogLevel(value);
                }
                else if (what.equals("ifini"))
                {
                    GBS::IF_INI_ST::write(value);
                    // _WSN(GBS::IF_INI_ST::read());
                }
                else if (what.equals("ifvst"))
                {
                    GBS::IF_VB_ST::write(value);
                    // _WSN(GBS::IF_VB_ST::read());
                }
                else if (what.equals("ifvsp"))
                {
                    GBS::IF_VB_SP::write(value);
                    // _WSN(GBS::IF_VB_ST::read());
                }
                else if (what.equals("vsstc"))
                {
                    setCsVsStart(value);
                }
                else if (what.equals("vsspc"))
                {
                    setCsVsStop(value);
                }
            }
            else
            {
                _WSN(F("abort"));
            }
        }
        break;
        case 'x':
        {
            uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
            GBS::IF_HBIN_SP::write(if_hblank_scale_stop + 1);
            _WSF(PSTR("1_26: 0x%04X\n"), (if_hblank_scale_stop + 1));
        }
        break;
        case 'X':
        {
            uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
            GBS::IF_HBIN_SP::write(if_hblank_scale_stop - 1);
            _WSF(PSTR("1_26: 0x%04X\n"), (if_hblank_scale_stop - 1));
        }
        break;
        case '(':
        {
            writeProgramArrayNew(ntsc_1920x1080, false);
            doPostPresetLoadSteps();
        }
        break;
        case ')':
        {
            writeProgramArrayNew(pal_1920x1080, false);
            doPostPresetLoadSteps();
        }
        break;
        case 'V':
        {
            _WS(F("step response "));
            uopt->wantStepResponse = !uopt->wantStepResponse;
            if (uopt->wantStepResponse)
            {
                GBS::VDS_UV_STEP_BYPS::write(0);
                _WSN(F("on"));
            }
            else
            {
                GBS::VDS_UV_STEP_BYPS::write(1);
                _WSN(F("off"));
            }
            saveUserPrefs();
        }
        break;
        case 'S':
        {
            snapToIntegralFrameRate();
            break;
        }
        case ':':
            externalClockGenSyncInOutRate();
            break;
        case ';':
            externalClockGenResetClock();
            if (rto->extClockGenDetected)
            {
                rto->extClockGenDetected = 0;
                _WSN(F("ext clock gen bypass"));
            }
            else
            {
                rto->extClockGenDetected = 1;
                _WSN(F("ext clock gen active"));
                externalClockGenSyncInOutRate();
            }
            //{
            //  float bla = 0;
            //  double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
            //  bla = esp8266_clock_freq / (double)FrameSync::getPulseTicks();
            //  _WSN(bla, 5);
            //}
            break;
        default:
            _WSF(PSTR("unknown command: 0x%04X\n"), serialCommand);
            break;
        }

        delay(1); // give some time to read in eventual next chars

        // a web ui or terminal command has finished. good idea to reset sync lock timer
        // important if the command was to change presets, possibly others
        lastVsyncLock = millis();

        if (!Serial.available())
        {
            // in case we handled a Serial or web server command and there's no more extra commands
            // but keep debug view command (resets once called)
            if (serialCommand != 'D')
            {
                serialCommand = '@';
            }
            wifiLoop(1);
        }
    }
}

/**
 * @brief initially was Type2 command
 *
 */
void handleUserCommand()
{
    if (userCommand != '@')
    {
        _WSF(
            commandDescr,
            "user",
            userCommand,
            userCommand,
            // uopt->presetPreference,
            uopt->presetSlot,
            // rto->presetID
            rto->resolutionID,
            rto->resolutionID);
        switch (userCommand)
        {
        case '0':
            _WS(F("pal force 60hz "));
            if (uopt->PalForce60 == 0)
            {
                uopt->PalForce60 = 1;
                _WSN(F("on"));
            }
            else
            {
                uopt->PalForce60 = 0;
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case '1':               // reset to defaults button
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
        case '3': // before: load custom preset, now: slot change
        {
            // @sk: see serverSlotSet()
            // uopt->presetPreference = OutputCustomized; // custom
            // TODO: unknown logic
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it
                rto->videoStandardInput = 15;
            } else {
                // also does loadPresetFromLFS()
                applyPresets(rto->videoStandardInput);
            }
            // save new slotID into preferences
            saveUserPrefs();
        }
        break;
        // case '4': // save custom preset
        //     savePresetToFS();
        //     // uopt->presetPreference = OutputCustomized; // custom
        //     rto->resolutionID = OutputCustom; // custom
        //     saveUserPrefs();
        //     break;
        case '5':
            // Frame Time Lock toggle
            uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
            saveUserPrefs();
            if (uopt->enableFrameTimeLock)
            {
                _WSN(F("FTL on"));
            }
            else
            {
                _WSN(F("FTL off"));
            }
            if (!rto->extClockGenDetected)
            {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->enableFrameTimeLock)
            {
                activeFrameTimeLockInitialSteps();
            }
            break;
        case '6':
            //
            break;
        case '7':
            uopt->wantScanlines = !uopt->wantScanlines;
            _WS(F("scanlines: "));
            if (uopt->wantScanlines)
            {
                _WSN(F("on (Line Filter recommended)"));
            }
            else
            {
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
            delay(100);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            break;
        case 'e': // print files on data
        {
            fs::Dir dir = LittleFS.openDir("/");
            while (dir.next())
            {
                _WSF(
                    PSTR("%s %ld\n"),
                    dir.fileName().c_str(),
                    dir.fileSize());
                // delay(1); // wifi stack
            }
            //
            fs::File f = LittleFS.open(FPSTR(preferencesFile), "r");
            if (!f)
            {
                _WSN(F("\nfailed opening preferences file"));
            }
            else
            {
                _WSF(
                    PSTR("\npresetNameID = %c\npreset slot = %c\nframe time lock = %c\n" \
                        "frame lock method = %c\nauto gain = %c\n" \
                        "scanlines = %c\ncomponent output = %c\ndeinterlacer mode = %c\n" \
                        "line filter = %c\npeaking = %c\npreferScalingRgbhv = %c\n" \
                        "6-tap = %c\npal force60 = %c\nmatched = %c\n" \
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
                    f.read());

                f.close();
            }
        }
        break;
        /**
         * 1. change resolution
         * 2. update registers if videoStandardInput != 14
         * 3. update/save preset data
         */
        case 'f':           // 960p
        case 'g':           // 720p
        case 'h':           // 480p
        case 'p':           // 1024p
        case 's':           // 1080p
        case 'L':           // 15kHz/downscale
        case 'j':           // 240p
        case 'k':           // 576p
        {
            // load preset via webui
            uint8_t videoMode = getVideoMode();
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput; // last known good as fallback
            // else videoMode stays 0 and we'll apply via some assumptions
            // TODO missing resolutions below ?
            if (userCommand == 'f')
                // uopt->presetPreference = Output960P; // 1280x960
                rto->resolutionID = Output960p; // 1280x960
            if (userCommand == 'g')
                // uopt->presetPreference = Output720P; // 1280x720
                rto->resolutionID = Output720p; // 1280x720
            if (userCommand == 'h')
                // uopt->presetPreference = Output480P; // 720x480/768x576
                rto->resolutionID = Output480p; // 720x480/768x576
            if (userCommand == 'p')
                // uopt->presetPreference = Output1024P; // 1280x1024
                rto->resolutionID = Output1024p; // 1280x1024
            if (userCommand == 's')
                // uopt->presetPreference = Output1080P; // 1920x1080
                rto->resolutionID = Output1080p; // 1920x1080
            if (userCommand == 'L')
                // uopt->presetPreference = OutputDownscale; // downscale
                rto->resolutionID = Output15kHz; // downscale
            if (userCommand == 'k')
                rto->resolutionID = Output576p50; // PAL
            if (userCommand == 'j')
                rto->resolutionID = Output240p; // 240p

            rto->useHdmiSyncFix = 1; // disables sync out when programming preset
            if (rto->videoStandardInput == 14)
            {
                // vga upscale path: let synwatcher handle it
                _DBGN(F("video input is #14, switch to #15"));
                rto->videoStandardInput = 15;
            }
            else
            {
                // normal path
                _DBGF(PSTR("apply preset of videoMode: %d resolution: %d\n"), videoMode,  rto->resolutionID);
                applyPresets(videoMode);
            }
            saveUserPrefs();
        }
        break;
        case 'i':
            // toggle active frametime lock method
            if (!rto->extClockGenDetected)
            {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->frameTimeLockMethod == 0)
            {
                uopt->frameTimeLockMethod = 1;
            }
            else if (uopt->frameTimeLockMethod == 1)
            {
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

                switch (PLL_MS)
                {
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
                if (memClock != 10)
                {
                    _WSF(PSTR("SDRAM clock: %dMhz\n"), memClock);
                }
                else
                {
                    _WSN(F("SDRAM clock: feedback clock"));
                }
            }
            break;
        case 'm':
            _WS(F("Line Filter: "));
            if (uopt->wantVdsLineFilter)
            {
                uopt->wantVdsLineFilter = 0;
                GBS::VDS_D_RAM_BYPS::write(1);
                _WSN(F("off"));
            }
            else
            {
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
        case 'A':
        {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd > 4) && (hbspd < (htotal - 4)))
            {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
            }
            else
            {
                _WSN(F("limit"));
            }
        }
        break;
        case 'B':
        {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd < (htotal - 4)) && (hbspd > 4))
            {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
            }
            else
            {
                _WSN(F("limit"));
            }
        }
        break;
        case 'C':
        {
            // vert mask +
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd > 6) && (vbspd < (vtotal - 4)))
            {
                GBS::VDS_DIS_VB_ST::write(vbstd - 2);
                GBS::VDS_DIS_VB_SP::write(vbspd + 2);
            }
            else
            {
                _WSN(F("limit"));
            }
        }
        break;
        case 'D':
        {
            // vert mask -
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd < (vtotal - 4)) && (vbspd > 6))
            {
                GBS::VDS_DIS_VB_ST::write(vbstd + 2);
                GBS::VDS_DIS_VB_SP::write(vbspd - 2);
            }
            else
            {
                _WSN(F("limit"));
            }
        }
        break;
        case 'q':
            if (uopt->deintMode != 1)
            {
                uopt->deintMode = 1;
                disableMotionAdaptDeinterlace();
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read())
                {
                    disableScanlines();
                }
                saveUserPrefs();
            }
            _WSN(F("Deinterlacer: Bob"));
            break;
        case 'r':
            if (uopt->deintMode != 0)
            {
                uopt->deintMode = 0;
                saveUserPrefs();
                // will enable next loop()
            }
            _WSN(F("Deinterlacer: Motion Adaptive"));
            break;
        case 't':
            // unused now
            _WS(F("6-tap: "));
            if (uopt->wantTap6 == 0)
            {
                uopt->wantTap6 = 1;
                GBS::VDS_TAP6_BYPS::write(0);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() - 1);
                _WSN(F("on"));
            }
            else
            {
                uopt->wantTap6 = 0;
                GBS::VDS_TAP6_BYPS::write(1);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case 'u':       // extract backup
            extractBackup();
            // reset device to apply new configuration
            delay(1000);
            ESP.reset();
        //     // restart to attempt wifi station mode connect
        //     wifiStartStaMode("");
        //     delay(100);
        //     ESP.reset();
            // break;
        case 'v':
        {
            uopt->wantFullHeight = !uopt->wantFullHeight;
            saveUserPrefs();
            uint8_t vidMode = getVideoMode();
            // if (uopt->presetPreference == 5) {
            if (rto->resolutionID == Output1080p)
            {
                if (GBS::GBS_PRESET_ID::read() == 0x05 || GBS::GBS_PRESET_ID::read() == 0x15)
                {
                    applyPresets(vidMode);
                }
            }
        }
        break;
        case 'w':
            uopt->enableCalibrationADC = !uopt->enableCalibrationADC;
            saveUserPrefs();
            break;
        case 'x':
            uopt->preferScalingRgbhv = !uopt->preferScalingRgbhv;
            _WS(F("preferScalingRgbhv: "));
            if (uopt->preferScalingRgbhv)
            {
                _WSN(F("on"));
            }
            else
            {
                _WSN(F("off"));
            }
            saveUserPrefs();
            break;
        case 'X':
            _WS(F("ExternalClockGenerator "));
            if (uopt->disableExternalClockGenerator == 0)
            {
                uopt->disableExternalClockGenerator = 1;
                _WSN(F("disabled"));
            }
            else
            {
                uopt->disableExternalClockGenerator = 0;
                _WSN("enabled");
            }
            saveUserPrefs();
            break;
        case 'z':
            // sog slicer level
            if (rto->currentLevelSOG > 0)
            {
                rto->currentLevelSOG -= 1;
            }
            else
            {
                rto->currentLevelSOG = 16;
            }
            setAndUpdateSogLevel(rto->currentLevelSOG);
            optimizePhaseSP();
            _WSF(
                PSTR("Phase: %d SOG: %d\n"),
                rto->phaseSP,
                rto->currentLevelSOG);
            break;
        case 'E':
            // test option for now
            _WS(F("IF Auto Offset: "));
            toggleIfAutoOffset();
            if (GBS::IF_AUTO_OFST_EN::read())
            {
                _WSN(F("on"));
            }
            else
            {
                _WSN(F("off"));
            }
            break;
        case 'F':
            // freeze pic
            if (GBS::CAPTURE_ENABLE::read())
            {
                GBS::CAPTURE_ENABLE::write(0);
            }
            else
            {
                GBS::CAPTURE_ENABLE::write(1);
            }
            break;
        case 'K':
            // scanline strength
            if (uopt->scanlineStrength >= 0x10)
            {
                uopt->scanlineStrength -= 0x10;
            }
            else
            {
                uopt->scanlineStrength = 0x50;
            }
            if (rto->scanlinesEnabled)
            {
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
                GBS::VDS_Y_OFST::read());
            break;
        case 'T':
            // brightness--
            GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 1);
            _WSF(
                PSTR("Brightness-- : %d\n"),
                GBS::VDS_Y_OFST::read());
            break;
        case 'N':
            // contrast++
            GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 1);
            _WSF(
                PSTR("Contrast++ : %d\n"),
                GBS::VDS_Y_GAIN::read());
            break;
        case 'M':
            // contrast--
            GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 1);
            _WSF(
                PSTR("Contrast-- : %d\n"),
                GBS::VDS_Y_GAIN::read());
            break;
        case 'Q':
            // pb/u gain++
            GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() + 1);
            _WSF(
                PSTR("Pb/U gain++ : %d\n"),
                GBS::VDS_UCOS_GAIN::read());
            break;
        case 'H':
            // pb/u gain--
            GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() - 1);
            _WSF(
                PSTR("Pb/U gain-- : %d\n"),
                GBS::VDS_UCOS_GAIN::read());
            break;
            break;
        case 'P':
            // pr/v gain++
            GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() + 1);
            _WSF(
                PSTR("Pr/V gain++ : %d\n"),
                GBS::VDS_VCOS_GAIN::read());
            break;
        case 'S':
            // pr/v gain--
            GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() - 1);
            _WSF(
                PSTR("Pr/V gain-- : %d\n"),
                GBS::VDS_VCOS_GAIN::read());
            break;
        case 'O':
            // info
            if (GBS::ADC_INPUT_SEL::read() == 1)
            {
                _WSF(
                    PSTR("RGB reg\n------------\nY_OFFSET: %d\n" \
                        "U_OFFSET: %d\nV_OFFSET: %d\nY_GAIN: %d\n" \
                        "USIN_GAIN: %d\nUCOS_GAIN: %d\n"),
                    GBS::VDS_Y_OFST::read(),
                    GBS::VDS_U_OFST::read(),
                    GBS::VDS_V_OFST::read(),
                    GBS::VDS_Y_GAIN::read(),
                    GBS::VDS_USIN_GAIN::read(),
                    GBS::VDS_UCOS_GAIN::read());
            }
            else
            {
                _WSF(
                    PSTR("YPbPr reg\n------------\nY_OFFSET: %d\n"
                         "U_OFFSET: %d\nV_OFFSET: %d\nY_GAIN: %d\n"
                         "USIN_GAIN: %d\nUCOS_GAIN: %d\n"),
                    GBS::VDS_Y_OFST::read(),
                    GBS::VDS_U_OFST::read(),
                    GBS::VDS_V_OFST::read(),
                    GBS::VDS_Y_GAIN::read(),
                    GBS::VDS_USIN_GAIN::read(),
                    GBS::VDS_UCOS_GAIN::read());
            }
            break;
        case 'U':
            // default
            if (GBS::ADC_INPUT_SEL::read() == 1)
            {
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
            }
            else
            {
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

        userCommand = '@'; // in case we handled web server command
        lastVsyncLock = millis();
        wifiLoop(1);
    }
}

/**
 * @brief
 *
 */
void initUpdateOTA()
{
    // @sk: MDNS already started
    // ArduinoOTA.setHostname("GBS OTA");
    // ArduinoOTA.setPassword("admin");
    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
    // update: no password is as (in)secure as this publicly stated hash..
    // rely on the user having to enable the OTA feature on the web ui

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "firmware";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating LittleFS this would be the place to unmount LittleFS using LittleFS.end()
        LittleFS.end();
        _WSF(
            PSTR("%s OTA update start\n"),
            type.c_str()
        );
    });
    ArduinoOTA.onEnd([]() {
        _WSN(F("OTA update end"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        _WSF(PSTR("progress: %u%%\n"), (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        _WSF(PSTR("OTA error[%u]: "), error);
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
}

#endif // defined(ESP8266)