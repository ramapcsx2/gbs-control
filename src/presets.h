/*
#####################################################################################
# File: presets.h                                                                    #
# File Created: Thursday, 2nd May 2024 6:38:29 pm                                   #
# Author:                                                                           #
# Last Modified: Sunday, 5th May 2024 3:48:42 pm                          #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _PRESET_H_
#define _PRESET_H_

#include <LittleFS.h>
#include "options.h"
#include "video.h"
#include "osd.h"
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_720x480.h"
#include "pal_768x576.h"
#include "ntsc_1280x720.h"
#include "ntsc_1280x1024.h"
#include "ntsc_1920x1080.h"
#include "ntsc_downscale.h"
#include "pal_1280x720.h"
#include "pal_1280x1024.h"
#include "pal_1920x1080.h"
#include "pal_downscale.h"
#include "presetMdSection.h"
#include "presetDeinterlacerSection.h"

extern char serialCommand;
extern char userCommand;

void loadDefaultUserOptions();
void loadPresetMdSection();
void setResetParameters();
void doPostPresetLoadSteps();
void applyPresets(uint8_t result);
void loadPresetDeinterlacerSection();

#if defined(ESP8266)

const uint8_t *loadPresetFromLFS(byte forVideoMode);
void savePresetToFS();
void saveUserPrefs();

#endif                      // defined(ESP8266)

#endif                              // _PRESET_H_
