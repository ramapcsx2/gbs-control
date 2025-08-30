#ifndef LCDMENU_H
#define LCDMENU_H




/*//ABOUT SECTION://////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
This is for use with a 16x2 I2C LCD screen & the Rotary Encoder (EC11) board schematic as recommended by: https://ramapcsx2.github.io/gbs-control/Wiki/OLED-Add-On-Setup-Info.html
Depending on your LCD screen you may or may not need it connected to 5v (instead of 3.3v). if you dont see anything on your screen you may need to adjust the contrast on your I2C backpack and make sure USE_LCD_MENU is set to 1 in main file. 
This has only been tested with the basic Rotary Encoder (EC11) board schematic; no resistors for a cleaner encoder experience on board.
All software setup and wire installation is the same as normal OLED version of GBS-CONTROL. Use the guide on the main GBS-CONTROL github.
Using library manager, install the following library: "hd44780" [Version 1.3.2] by Bill Perry
Make sure you backup your current slot files in the web application before doing any flashing.

TESTED:
LoL1n(new NodeMCU v3) with breakout dev-board. ArduinoIDE setting: [160HMZ (FS:1MB OTA:~1019kb)]
I2C LCD1602 interface backpack   &  Generic LCD 16x2 display module
*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////MANUAL SETTINGS/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define USE_REALTIME_UPDATING  1       // 1 = when you make or change a preset name your changes will instantly appear in the menu    
                                       // 0 = you will need to restart the GBS to see changes with saved slot names and such. 

                                            
#define RESET_TO_MAIN_MENU     1       // 0 = do not reset to main menu from resolutions/presets/ after selecting an option  (stay inside menu until back is selected)
                                       // 1 = reset to main menu        from resolutions/presets/ after selecting an option
                                       // USE CASE EXAMPLE: if you are loading directly into PRESETS SUBMENU and never want to leave until you press BACK

#define LCD_REFRESHRATE        200     // 200 = default (100-300 recommended) (sets delay in menuing loops before clearing/refreshing screen) if you set it below 200 you may get system crashes


//if you want to make your own screensaver splash go to " void LCD_printSplash(int logo_select) " in the LCD_Menu.cpp file.
//and use the link listed there to generate your own splash screen to use with this! just replace whats there and add nothing more.
//or just make your own custom version however you want with lcd.print("example");
#define LCD_SPLASH_SCREEN_TYPE 1       // 0 = add in your own custom text in LCDmenu.cpp LCD_printSplash() function
                                       // 1 = Display ANIMATED "GBS[C]" splashscreen
                                       
                                    
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





void settingsMenuLCD();     // main menu function
void LCD_printSplash(int logo_select);

void LCD_resetToXMenu(int location);
void lcd_subpointerfunction();
void lcd_pointerfunction();
void ICACHE_RAM_ATTR isrRotaryEncoder();

String getSlotName();
String printSlotNameToLCD(int index);
void LCD_PresetLoadSelection(int index, String slotName);
int getCurrentSlotIndexFromPresetSlotChar(char slotChar); 
void LCD_Load_PresetIDs_Into_slotobject();

void createDefaultSlotsIfMissing();

enum LOCATIONS { //for use with resettoXmenu
    RETURN_TO_MAIN,
    RETURN_TO_RESOLUTIONS,
    RETURN_TO_PRESETS,
    RETURN_TO_OPTIONS,
    RETURN_TO_SIGNAL
};

#endif
