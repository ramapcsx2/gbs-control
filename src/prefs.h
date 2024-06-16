/*
###########################################################################
# File: prefs.h                                                           #
# File Created: Thursday, 13th June 2024 12:16:48 am                      #
# Author: Sergey Ko                                                       #
# Last Modified: Thursday, 13th June 2024 12:27:13 am                     #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#ifndef _PREFS_H_
#define _PREFS_H_

#include <LittleFS.h>
#include "options.h"
#include "wserial.h"

void loadDefaultUserOptions();
bool prefsLoad();
bool prefsSave();


#endif