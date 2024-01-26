/* 
   See https://github.com/PSHarold/OLED-SSD1306-Menu 
   for original code, license and documentation
*/
#ifndef OLED_MENU_CONFIG_H_
#define OLED_MENU_CONFIG_H_
// include your translations here
#include "OLEDMenuTranslations.h"
#define OLED_MENU_WIDTH 128
#define OLED_MENU_HEIGHT 64
#define OLED_MENU_MAX_SUBITEMS_NUM 16 // should be less than 256
#define OLED_MENU_MAX_ITEMS_NUM 32    // should be less than 1024
#define OLED_MENU_MAX_DEPTH 8 // maximum levels of submenus
#define OLED_MENU_REFRESH_INTERVAL_IN_MS 50 // not precise
#define OLED_MENU_SCREEN_SAVER_REFRESH_INTERVAL_IN_MS 5000 // not precise
#define OLED_MENU_SCROLL_LEAD_IN_TIME_IN_MS 600
#define OLED_MENU_SCREEN_SAVER_KICK_IN_SECONDS 180 // after "OLED_MENU_SCREEN_SAVE_KICK_IN_SECONDS" seconds, screen saver will show up until any key is pressed
#define OLED_MENU_OVER_DRAW 0
#define OLED_MENU_RESET_ALWAYS_SCROLL_ON_SELECTION 0
#define OLED_MENU_STATUS_BAR_HEIGHT (OLED_MENU_HEIGHT / 4) // status bar uses the top 1/4 of the scree
#define OLED_MENU_WRAPPING_SPACE (OLED_MENU_WIDTH / 3)


// do not edit these
#define OLED_MENU_USABLE_AREA_HEIGHT (OLED_MENU_HEIGHT - OLED_MENU_STATUS_BAR_HEIGHT)
#define OLED_MENU_SCROLL_LEAD_IN_FRAMES (OLED_MENU_SCROLL_LEAD_IN_TIME_IN_MS / OLED_MENU_REFRESH_INTERVAL_IN_MS)
#endif