#include <ESP8266WiFi.h>
#include "options.h"
#include "tv5725.h"
#include "slot.h" 
#include "src/WebSockets.h"
#include "src/WebSocketsServer.h"

#include "LCDMenu.h"
#include <hd44780.h>                        //16x2 LCD I2C header
#include <hd44780ioClass/hd44780_I2Cexp.h>  //16x2 LCD I2C header



//////////////////////////////////////////////////
typedef TV5725<GBS_ADDR> GBS;
extern void applyPresets(uint8_t videoMode);
extern void setOutModeHdBypass(bool bypass);
extern void saveUserPrefs();
extern void savePresetToSPIFFS();
extern float getOutputFrameRate();
extern void loadDefaultUserOptions();
extern uint8_t getVideoMode();
extern runTimeOptions *rto;
extern userOptions *uopt;
extern WebSocketsServer webSocket;
///////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
hd44780_I2Cexp lcd;       //LCD this is the class member we are using through out the program to print to lcd
//LCD geometry
const int LCD_COLS = 16;  //LCD
const int LCD_ROWS = 2;   //LCD
int	status = lcd.begin(LCD_COLS, LCD_ROWS); //int status;  //LCD
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
SlotMetaArray LCDslotsObject; // contains all of our saved preset names
/////////////////////////////////////////////////////////////////////////



//LCD menu strings//////////////////////////////////////////////////////////
String LCD_menu[4] =        {"RESOLUTION", "PRESET", "MISC", "CURRENT SETTINGS"};
String LCD_Resolutions[7] = {"1280x960", "1280x1024", "1280x720", "1920x1080", "480/576", "DOWNSCALE", "PASS-THROUGH"}; 
String LCD_Presets[8] =     {"1", "2", "3", "4", "5", "6", "7", "BACK"};
String LCD_Option[4] =      {"RESET GBS", "RESTORE FACTORY", "BACK"}; 
//////////////////////////////////////////////////////////////////////////////////////


////////////ENCODER AND MENU RELATED VARIABLES///////////////////////////////////////////
//default values
int LCD_menuItem = 1;               //default is 1 (displays main menu)
int LCD_subsetFrame = 0;            //0 default
int LCD_selectOption = 0;           //0 default
int LCD_page = 0;                   //0 default
//default values
int LCD_lastCount = 0;
volatile int LCD_encoder_pos = 0;   //0 default
volatile int LCD_main_pointer = 0;  //0 default // volatile vars change done with ISR
volatile int LCD_pointer_count = 0; //0 default
volatile int LCD_sub_pointer = 0;   //0 default
////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int LCD_pin_clk = 14;            //D5 = GPIO14 (input of one direction for encoder)
const int LCD_pin_data = 13;           //D7 = GPIO13	(input of one direction for encoder)
const int LCD_pin_switch = 0;          //D3 = GPIO0 pulled HIGH, else boot fail (middle push button for encoder)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





//////////////LCD start of LCD MENU FUNCTION////////////////////////////////////////////
//LCD menu main functionality
void settingsMenuLCD()
{//main menu
    uint8_t videoMode = getVideoMode();
    byte button_nav = digitalRead(LCD_pin_switch);
    if (button_nav == LOW) {
        delay(350);         //TODO
        LCD_subsetFrame++; //this button counter for navigating menu
        LCD_selectOption++;
    }
    
///////////////////////////////////////////////////////////
//main menu //LCD
///////////////////////////////////////////////////////////
    if (LCD_page == 0)
     {
       
        delay(LCD_REFRESHRATE);
        lcd_pointerfunction();
    }
    //cursor location inside  main menu
    if (LCD_main_pointer == 0 && LCD_subsetFrame == 0) {
        LCD_pointer_count = 0;
        LCD_menuItem = 1;

        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("> " + LCD_menu[0]); //-> resolutions 
        
        lcd.setCursor(0,1); 
        lcd.print(LCD_menu[1]);        //presets         
       
    }
    if (LCD_main_pointer == 15 && LCD_subsetFrame == 0) {
        LCD_pointer_count = 0;
        LCD_menuItem = 2;

        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print(LCD_menu[0]);        //resolutions
        
        lcd.setCursor(0,1); 
        lcd.print("> " + LCD_menu[1]); //->presets 

    }
    if (LCD_main_pointer == 30 && LCD_subsetFrame == 0) {
        LCD_pointer_count = 0;
        LCD_sub_pointer = 0;
        LCD_menuItem = 3;

        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("> " + LCD_menu[2]); //->option
        
        lcd.setCursor(0,1); 
        lcd.print(LCD_menu[3]);        //current settings 

    }
    if (LCD_main_pointer == 45 && LCD_subsetFrame == 0) {
        LCD_pointer_count = 0;
        LCD_menuItem = 4;

        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print(LCD_menu[2]);        //option 
        
        lcd.setCursor(0,1); 
        lcd.print("> " + LCD_menu[3]); //-> current settings 
    }

 
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    //resolution pages //LCD  
    ///////////////////////////////////////////////////////////
    if (LCD_menuItem == 1 && LCD_subsetFrame == 1)
     {
        delay(LCD_REFRESHRATE);
        LCD_page = 1;
        LCD_main_pointer = 0;
        
    
    if (  (LCD_pointer_count >= 0 && LCD_pointer_count <= 7)  ) //7 being LCD__reso array item "back"
    {

        if(LCD_pointer_count < 7) //7 being the LCD_Resolutions array number for going "back"
        {
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("RESOLUTION: ");  
        lcd.setCursor(0,1); 
        lcd.print(String(LCD_Resolutions[LCD_pointer_count]));  
        }
        else if(LCD_pointer_count >= 7)  //7 being LCD__reso array item "back"
        {
        LCD_pointer_count = 7; //7 being LCD_reso array item "back"
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("RESOLUTION: ");  
        lcd.setCursor(0,1); 
        lcd.print(String("BACK")); 
        }
     } 
     else if (LCD_pointer_count < 0)  { LCD_pointer_count = 0; } //do not want it going below 0 as it causes issues
     else if (LCD_pointer_count > 7)  { LCD_pointer_count = 7; } //7 being lcd_resolutions array item "back"

    }  
 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    ///////////////////////////////////////////////////////////
    //resolutions selections //LCD
    ///////////////////////////////////////////////////////////
    //1280x960
    if (LCD_menuItem == 1) {
        if (LCD_pointer_count == 0 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
                 
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("1280x960"); 
        lcd.setCursor(0,1); 
        lcd.print("LOADED!"); 
       
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = Output960P;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}    //reset back into main starting menu
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;} //reset back into resolutions
                
        }
        //1280x1024
        if (LCD_pointer_count == 1 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
                    
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("1280x1024"); 
        lcd.setCursor(0,1); 
        lcd.print("LOADED!");  
        
           // }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = Output1024P;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;}
        }
        //1280x720
        if (LCD_pointer_count == 2 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
           
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("1280x720");  
        lcd.setCursor(0,1); 
        lcd.print("LOADED!");  
        
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = Output720P;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;}
        }
        //1920x1080
        if (LCD_pointer_count == 3 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
          
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("1920x1080"); 
        lcd.setCursor(0,1); 
        lcd.print("LOADED!"); 
        
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = Output1080P;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;}
        }
        //720x480
        if (LCD_pointer_count == 4 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
           
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("480/576"); 
        lcd.setCursor(0,1); 
        lcd.print("LOADED!"); 
      
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = Output480P;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;}
        }
        //downscale
        if (LCD_pointer_count == 5 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
          
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("DOWNSCALE");  
        lcd.setCursor(0,1); 
        lcd.print("LOADED!");  
        
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = OutputDownscale;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;}
        }
        //passthrough/bypass
        if (LCD_pointer_count == 6 && LCD_selectOption == 2) {
            LCD_subsetFrame = 3;
           
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("PASSTHROUGH");  
        lcd.setCursor(0,1); 
        lcd.print("LOADED!");  
       
            setOutModeHdBypass(false);
            uopt->presetPreference = OutputBypass;
            if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) {
                rto->autoBestHtotalEnabled = 0;
                if (rto->applyPresetDoneStage == 11) {
                    rto->applyPresetDoneStage = 1;
                } else {
                    rto->applyPresetDoneStage = 10;
                }
            } else {
                rto->applyPresetDoneStage = 1;
            }
            saveUserPrefs();
            delay(1000); //catchup
            if(RESET_TO_MAIN_MENU == 1) {LCD_resetToXMenu(RETURN_TO_MAIN);}
            else{ LCD_selectOption = 1; LCD_subsetFrame = 1;}
        }
        //go back
        if (LCD_pointer_count == 7 && LCD_selectOption == 2) {
            LCD_page = 0;
            LCD_subsetFrame = 0;
            LCD_main_pointer = 0;
            LCD_sub_pointer = 0;
            LCD_selectOption = 0;

            LCD_menuItem = 1;
            LCD_pointer_count = 0;
            delay(50); //catchup
        }
    }
//////////////////////////////////////////////////////////////////////////////////////////////
//Presets pages //preset slot listings //LCD
//////////////////////////////////////////////////////////////////////////////////////////////
    if (LCD_menuItem == 2 && LCD_subsetFrame == 1)
        {
        LCD_page = 1;
        LCD_main_pointer = 0;
        printSlotNameToLCD(LCD_pointer_count);
        }  
/////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
//preset selection //LCD
//////////////////////////////////////////////////////////////////////////////////////////////
if (LCD_menuItem == 2 && LCD_selectOption == 2)
{ 
 LCD_PresetLoadSelection(LCD_pointer_count,  printSlotNameToLCD(LCD_pointer_count) ); //all functionality inside this function
}
///////////////////////////////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////////////////////////////////
//Option pages //LCD 
//////////////////////////////////////////////////////////////////////////////////////////////
    if (LCD_menuItem == 3 && LCD_subsetFrame == 1)
     { 
        delay(LCD_REFRESHRATE); 
        LCD_page = 1;
        LCD_main_pointer = 0;
        lcd_pointerfunction(); 
      
        if (LCD_pointer_count >= 2) 
          { LCD_pointer_count = 2; } 

        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("MISC: ");  
        lcd.setCursor(0,1); 
        lcd.print(LCD_Option[LCD_pointer_count]);  
     }
    
   
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Option selection //LCD
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        if (LCD_menuItem == 3) {
      
//RESET GBS//////////////////////////////////////////
        if (LCD_pointer_count == 0 && LCD_selectOption == 2) {
        LCD_subsetFrame = 3;
            
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("RESETTING GBS");  
        lcd.setCursor(0,1); 
        lcd.print("PLEASE WAIT...");  
        delay(1000);

    
            webSocket.close();
            delay(60);
            ESP.reset();
            LCD_selectOption = 0;
            LCD_subsetFrame = 0;

        }
//FACTORY RESTORE SETTINGS////////////////////////////////
        if (LCD_pointer_count == 1 && LCD_selectOption == 2)
        { 
            LCD_subsetFrame = 3;

        //ABORT CODE//////////////////////////////////////////////////////////////////
            LCD_selectOption = 0;
        for (int time = 30; time != 0; time--)
        {
            
        delay(1000);//delay 1 second per loop tick
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("FACTORY RESTORE");  
        lcd.setCursor(0,1); 
        lcd.print("IN: ");  
        lcd.print(String(time) + " SECONDS" ); 
       
        
          if(digitalRead(LCD_pin_switch) == LOW) //if push-button: abort factory reset and return to main menu
           {
              LCD_selectOption == 0;
              lcd.clear();
              lcd.setCursor(0,0); 
              lcd.print("FACTORY RESTORE:");  
              lcd.setCursor(0,1);
              lcd.print("ABORTED!"); 
              delay(2500); //give user a chance to see they aborted

            LCD_page = 0;
            LCD_subsetFrame = 0;
            LCD_main_pointer = 30;
            LCD_sub_pointer = 0;
            LCD_selectOption = 0;

            LCD_menuItem = 3;
            LCD_pointer_count = 0;
            return;
           }
        } ////////////////////////////////////////////////////////////////////
        //DIDNT ABORT
        lcd.clear();
        lcd.setCursor(0,0); 
        lcd.print("DEFAULTS LOADING");  
        lcd.setCursor(0,1); 
        lcd.print("PLEASE WAIT..."); 
        delay(1000);
        
            webSocket.close();
            saveUserPrefs();
            delay(60);
            ESP.reset(); 
            LCD_selectOption = 1;
            LCD_subsetFrame = 1;
        }
/////////////////////////////////////////////////////////////////////////
//BACK BUTTON FUNCTIONALITY
        if (LCD_pointer_count >= 2  && LCD_selectOption == 2)  //change pointer count logic if increasing amount of Option items; >=4 etc
         { 
            LCD_page = 0;
            LCD_subsetFrame = 0;
            LCD_main_pointer = 30;
            LCD_sub_pointer = 0;
            LCD_selectOption = 0;

            LCD_menuItem = 3;
            LCD_pointer_count = 0;
        }
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Current Settings pages //LCD
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (LCD_menuItem == 4 && LCD_subsetFrame == 1) {
    delay(LCD_REFRESHRATE);  
    boolean vsyncActive = 0;
    boolean hsyncActive = 0;
    float ofr = getOutputFrameRate();  // Output frame rate
    uint8_t currentInput = GBS::ADC_INPUT_SEL::read();  // 1 = RGB, else YPbPr
    rto->presetID = GBS::GBS_PRESET_ID::read();

    lcd.clear();

    // --- Line 1: Resolution ---
    lcd.setCursor(0, 0);
    switch (rto->presetID) {
        case 0x01: case 0x11:
            lcd.print("1280x960");
            break;
        case 0x02: case 0x12:
            lcd.print("1280x1024");
            break;
        case 0x03: case 0x13:
            lcd.print("1280x720");
            break;
        case 0x05: case 0x15:
            lcd.print("1920x1080");
            break;
        case 0x06: case 0x16:
            lcd.print("DOWNSCALE");
            break;
        case 0x04:
            lcd.print("720x480");
            break;
        case 0x14:
            lcd.print("768x576");
            break;
        default:
            lcd.print("BYPASS");
            break;
    }

    // --- Line 2: Input Type + Hz + Sync ---
    lcd.setCursor(0, 1);

    if (currentInput == 1) {
        lcd.print("RGB ");
    } else {
        lcd.print("YPbpR ");
    }

    // Limit Hz to 2 decimal places
    lcd.print(String(ofr, 2));
    lcd.print("Hz");

    // Add sync indicators (e.g. H V)
    if (currentInput == 1) {
        vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
        if (vsyncActive) {
            lcd.print(" V");
            hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
            if (hsyncActive) {
                lcd.print("H");
            }
        }
    }

     delay(300); //400 IS STABLE: this delay prevents system crashing when loading an active signal from bypass. //if it crashes on signal increase delay
}
    
    
//////////////////////////////////////////////////////////////////////////////////////////////
//current setting Selection //LCD
//////////////////////////////////////////////////////////////////////////////////////////////
               if (LCD_selectOption == 2)
            {
                {
                if (LCD_pointer_count >= 3) {LCD_pointer_count = 3;}
                if (LCD_pointer_count <= 0) {LCD_pointer_count = 0;}
              
                 delay(200);
                 LCD_resetToXMenu(RETURN_TO_MAIN);
                 return;
                }
             
            }
        
}//end of settingsMenuLCD()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    


//LCD FUNCTIONS
 ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_pointerfunction() //LCD 
{
    if (LCD_main_pointer <= 0) {
        LCD_main_pointer = 0;
    }
    if (LCD_main_pointer >= 45) { //limits
        LCD_main_pointer = 45;
    }

    if (LCD_pointer_count <= 0) {
        LCD_pointer_count = 0;
    } else if (LCD_pointer_count >= 3) {
        LCD_pointer_count = 3;
    }
}

void lcd_subpointerfunction()
{
    if (LCD_sub_pointer < 0)
     {
        LCD_sub_pointer = 0;
        LCD_subsetFrame = 1;
        LCD_page = 1;
     }
    if (LCD_sub_pointer > 45)
     { //limits to switch between the two pages
        LCD_sub_pointer = 0;    //TODO
        LCD_subsetFrame = 2;
        LCD_page = 2;
     }
   
    if (LCD_pointer_count < 0) 
      { LCD_pointer_count = 0; }
    else if (LCD_pointer_count >= SLOTS_TOTAL) //originally '7' //slots total is now the max amount of preset slots indicated in slot.h header file which is around 72
     {
        LCD_pointer_count = SLOTS_TOTAL;
     }
}




String printSlotNameToLCD(int index) //index = current lcd_pointer_count
{
               delay(LCD_REFRESHRATE);

               // if index (value of LCD_pointer_count) out of bounds
                if (index < 0 || index >= SLOTS_TOTAL)
                { 
                    if (index <= 0) {index = 0;}
                    if (index >= SLOTS_TOTAL) {index = SLOTS_TOTAL;}
                    //return "NULL"; 
                } 


                //Back button functionality!
                //convoluted way to make sure current slot never goes past the first "Empty" slot (and print BACK in place of the first empty slot)
                int current_index = index;                                                      //current lcd_pointer_count sent to function

                String valueOfEmptySlotID = "Empty                   " ;                        //value stored in bin for empty slots
              //if (String(LCDslotsObject.slot[current_index].name) == valueOfEmptySlotID)        //if current slot we're on is an empty slot
            if (String(LCDslotsObject.slot[current_index].name) == valueOfEmptySlotID || String(LCDslotsObject.slot[current_index].name) == "" ) //"" is that value of slots that have not been created with webapp startup yet
            {
                int slotBeforeEmpty;

               while (String(LCDslotsObject.slot[current_index].name) == valueOfEmptySlotID || String(LCDslotsObject.slot[current_index].name) == ""  ) //"" is that value of slots that have not been created with webapp startup yet
                {

                delay(50);
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("PRESET SLOT: ");
                lcd.setCursor(0,1);
                lcd.print("BACK");
               

                --current_index; //deincrement until we find an actual preset with an ID which should be the slot right before the empty slot
                 slotBeforeEmpty = current_index;

                    if (  valueOfEmptySlotID != String(LCDslotsObject.slot[current_index].name) ) //convoluted way to make sure current slot never goes past the first "Empty" slot
                    {
                        LCD_pointer_count = (slotBeforeEmpty + 1); //this should put lcd_counter_pointer right on the first empty slot
                         return "BACK"; //pointer is now on first empty slot, so we return "BACK" to pretend it doesnt exist.
                    }

                } 
            }
            
                else 
                {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("PRESET SLOT: ");
                lcd.setCursor(0,1);

                if (index <= 0) {current_index = 0;}    // if index (value of LCD_pointer_count) out of bounds
                lcd.print(   String(LCDslotsObject.slot[current_index].name)   );
                return String(LCDslotsObject.slot[current_index].name);
                }
               
              
}

void LCD_PresetLoadSelection(int index, String slotName)
{
    delay(LCD_REFRESHRATE); 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// how presetIDS are stored sequentially   0-25 is A-Z capital letters     26-51 is a-z lowercase letters.     52-61 is numeral 0-9     62-71 is special characters -._~()!*:,; */
const char slotCharset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcedefghijklmnopqrstuvwxyz0123456789-._~()!*:,;" ;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


     //back button functionality
    String valueOfEmptySlotID = "Empty                   " ;     //slot name value stored in gbs of empty slots [19 spaces]
    if ( ( slotName == "BACK" && LCD_selectOption == 2 ) || (slotName == "NULL" && LCD_selectOption == 2 )) //if currently on empty slot and pressed button returns to main menu
    {
        
            LCD_page = 0;
            LCD_subsetFrame = 0;
            LCD_main_pointer = 15; //15 cursor back on presets when arriving back into main menu
            LCD_sub_pointer = 0;
            LCD_selectOption = 0;

            LCD_menuItem = 2;      //2 cursor back on presets when arriving back into main menu
            LCD_pointer_count = 0;
            return;
            
    }
    //selected preset loading functionality
    else if ( LCD_selectOption == 2)
    {
        
            //LCD_subsetFrame = 3; 
            uopt->presetSlot =  slotCharset[index];  // we feed it the letter its looking for which correlates to the PRESET slot identifier a..b..c etc
            uopt->presetPreference = OutputCustomized;
            saveUserPrefs();
            
          
                  lcd.clear();
                  lcd.setCursor(0,0); 
                  lcd.print(slotName);  
                  lcd.setCursor(0,1); 
                  lcd.print("LOADED!");  
                  delay(300);
          
           
            uopt->presetPreference = OutputCustomized;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);             //allowing "catchup"   
            LCD_selectOption = 1; //reset select container 
           // LCD_subsetFrame = 1;  //switch back to prev frame (prev screen)???
           

        if (RESET_TO_MAIN_MENU == 1)//go back to main menu after loading a preset, otherwise wait for "BACK" button press.
        {
       /////////////////////////////////////
            LCD_page = 0;
            LCD_subsetFrame = 0;
            LCD_main_pointer = 15; //15 cursor back on presets when arriving back into main menu
            LCD_sub_pointer = 0;
            LCD_selectOption = 0;

            LCD_menuItem = 2;      //2 cursor back on presets when arriving back into main menu
            LCD_pointer_count = 0;
       /////////////////////////////////////
       }
    }

}



void LCD_printSplash(int logo_select) //0 = Animated GBS[C] logo          //logo code generated with https://chareditor.com/
{
     

if (logo_select == 0){
  //insert new splashscreen here
  //lcd.clear();
  //lcd.setCursor(0,0);
  //lcd.print("YOUR CUSTOM SPLASH TEXT HERE");
  //go into LCDmenu.h and change splashscreen type to '0'
}

else if (logo_select == 1) //animated GBS[C] logo
{

//frame 1
{
lcd.clear();

    byte name0x4[] =  { B11111, B10000, B10111, B10111, B10111, B10111, B10000, B11111 };
    byte name0x5[] =  { B11111, B00001, B11111, B11111, B00001, B11101, B00001, B11111 };
    byte name0x6[] =  { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x7[] =  { B00000, B11110, B00100, B01000, B01100, B00010, B11110, B00000 };
    byte name0x8[] =  { B00000, B01111, B01000, B01111, B00000, B00000, B01111, B00000 };
    byte name0x9[] =  { B00000, B11110, B00000, B11110, B00010, B00010, B11110, B00000 };
    byte name0x10[] = { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x11[] = { B00000, B11100, B00000, B00000, B00000, B00000, B11100, B00000 };


  
  lcd.createChar(0, name0x4);
  lcd.setCursor(4, 0);
  lcd.write(0);
  
  lcd.createChar(1, name0x5);
  lcd.setCursor(5, 0);
  lcd.write(1);
  
  lcd.createChar(2, name0x6);
  lcd.setCursor(6, 0);
  lcd.write(2);
  
  lcd.createChar(3, name0x7);
  lcd.setCursor(7, 0);
  lcd.write(3);
  
  lcd.createChar(4, name0x8);
  lcd.setCursor(8, 0);
  lcd.write(4);
  
  lcd.createChar(5, name0x9);
  lcd.setCursor(9, 0);
  lcd.write(5);
  
  lcd.createChar(6, name0x10);
  lcd.setCursor(10, 0);
  lcd.write(6);
  
  lcd.createChar(7, name0x11);
  lcd.setCursor(11, 0);
  lcd.write(7);

delay(200);
}
//frame 2
lcd.clear();

{
    byte name0x9[] =  { B00000, B11110, B00000, B11110, B00010, B00010, B11110, B00000 };
    byte name0x4[] =  { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x5[] =  { B00000, B11110, B00000, B00000, B11110, B00010, B11110, B00000 };
    byte name0x6[] =  { B11111, B10000, B10111, B10111, B10111, B10111, B10000, B11111 };
    byte name0x7[] =  { B11111, B00001, B11011, B10111, B10011, B11101, B00001, B11111 };
    byte name0x8[] =  { B00000, B01111, B01000, B01111, B00000, B00000, B01111, B00000 };
    byte name0x10[] = { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x11[] = { B00000, B11100, B00000, B00000, B00000, B00000, B11100, B00000 };


  
  lcd.createChar(0, name0x9);
  lcd.setCursor(9, 0);
  lcd.write(0);
  
  lcd.createChar(1, name0x4);
  lcd.setCursor(4, 0);
  lcd.write(1);
  
  lcd.createChar(2, name0x5);
  lcd.setCursor(5, 0);
  lcd.write(2);
  
  lcd.createChar(3, name0x6);
  lcd.setCursor(6, 0);
  lcd.write(3);
  
  lcd.createChar(4, name0x7);
  lcd.setCursor(7, 0);
  lcd.write(4);
  
  lcd.createChar(5, name0x8);
  lcd.setCursor(8, 0);
  lcd.write(5);
  
  lcd.createChar(6, name0x10);
  lcd.setCursor(10, 0);
  lcd.write(6);
  
  lcd.createChar(7, name0x11);
  lcd.setCursor(11, 0);
  lcd.write(7);

delay(200);
}
//frame 3
{
    lcd.clear();
    byte name0x9[] =  { B11111, B00001, B11111, B00001, B11101, B11101, B00001, B11111 };
    byte name0x4[] =  { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x5[] =  { B00000, B11110, B00000, B00000, B11110, B00010, B11110, B00000 };
    byte name0x6[] =  { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x7[] =  { B00000, B11110, B00100, B01000, B01100, B00010, B11110, B00000 };
    byte name0x8[] =  { B11111, B10000, B10111, B10000, B11111, B11111, B10000, B11111 };
    byte name0x10[] = { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x11[] = { B00000, B11100, B00000, B00000, B00000, B00000, B11100, B00000 };


  
  lcd.createChar(0, name0x9);
  lcd.setCursor(9, 0);
  lcd.write(0);
  
  lcd.createChar(1, name0x4);
  lcd.setCursor(4, 0);
  lcd.write(1);
  
  lcd.createChar(2, name0x5);
  lcd.setCursor(5, 0);
  lcd.write(2);
  
  lcd.createChar(3, name0x6);
  lcd.setCursor(6, 0);
  lcd.write(3);
  
  lcd.createChar(4, name0x7);
  lcd.setCursor(7, 0);
  lcd.write(4);
  
  lcd.createChar(5, name0x8);
  lcd.setCursor(8, 0);
  lcd.write(5);
  
  lcd.createChar(6, name0x10);
  lcd.setCursor(10, 0);
  lcd.write(6);
  
  lcd.createChar(7, name0x11);
  lcd.setCursor(11, 0);
  lcd.write(7);

delay(200);
}
//frame 4
{
    lcd.clear();
    byte name0x11[] = { B11111, B00011, B11111, B11111, B11111, B11111, B00011, B11111 };
    byte name0x4[] = { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x5[] = { B00000, B11110, B00000, B00000, B11110, B00010, B11110, B00000 };
    byte name0x6[] = { B00000, B01111, B01000, B01000, B01000, B01000, B01111, B00000 };
    byte name0x7[] = { B00000, B11110, B00100, B01000, B01100, B00010, B11110, B00000 };
    byte name0x8[] = { B00000, B01111, B01000, B01111, B00000, B00000, B01111, B00000 };
    byte name0x9[] = { B00000, B11110, B00000, B11110, B00010, B00010, B11110, B00000 };
    byte name0x10[] = { B11111, B10000, B10111, B10111, B10111, B10111, B10000, B11111 };


  lcd.createChar(0, name0x11);
  lcd.setCursor(11, 0);
  lcd.write(0);
  
  lcd.createChar(1, name0x4);
  lcd.setCursor(4, 0);
  lcd.write(1);
  
  lcd.createChar(2, name0x5);
  lcd.setCursor(5, 0);
  lcd.write(2);
  
  lcd.createChar(3, name0x6);
  lcd.setCursor(6, 0);
  lcd.write(3);
  
  lcd.createChar(4, name0x7);
  lcd.setCursor(7, 0);
  lcd.write(4);
  
  lcd.createChar(5, name0x8);
  lcd.setCursor(8, 0);
  lcd.write(5);
  
  lcd.createChar(6, name0x9);
  lcd.setCursor(9, 0);
  lcd.write(6);
  
  lcd.createChar(7, name0x10);
  lcd.setCursor(10, 0);
  lcd.write(7);
}

}//end of logo_select 1

else if (logo_select == 2){
}
else if (logo_select == 3){
}

///////////////////////////////////////////////
}//end of splash function


void LCD_Load_PresetIDs_Into_slotobject()
{
    // initialize and load saved slot names into LCDslotsobject
    // we do it here ONCE, looping it in main LCD loop crashes the system //place somewhere where it will not be looped
                LCDslotsObject;
                File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");
                slotsBinaryFileRead.read((byte *)&LCDslotsObject, sizeof(LCDslotsObject));
                slotsBinaryFileRead.close();
             
}

int getCurrentSlotIndexFromPresetSlotChar(char slotChar) {
    const char slotCharset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,;";
    for (int i = 0; i < strlen(slotCharset); i++) {
        if (slotCharset[i] == slotChar)
            return i;
    }
    return -1; // Not found
}



String getSlotName()
{
//get current selected slot name

int currentSlotIndex = getCurrentSlotIndexFromPresetSlotChar(uopt->presetSlot);
char id = LCDslotsObject.slot[currentSlotIndex].presetID;


       String slotName = LCDslotsObject.slot[currentSlotIndex].name; 
       return slotName;
}



void LCD_resetToXMenu(int location) //uses enum LOCATIONS for values
{
 switch(location)
    {
    case 0: LCD_menuItem = 1; LCD_subsetFrame = 0; LCD_page = 0; LCD_main_pointer = 0; LCD_selectOption = 0; LCD_pointer_count = 0;                break; //load into main menu     //stay on defaults
    case 1: LCD_menuItem = 1; LCD_subsetFrame = 1; LCD_page = 1; LCD_main_pointer = 0; LCD_selectOption = 1; LCD_pointer_count = 0;                break; //load into resolutions
    case 2: LCD_menuItem = 2; LCD_subsetFrame = 1; LCD_page = 1; LCD_main_pointer = 0; LCD_selectOption = 1; LCD_pointer_count = 0;                break; //load into presets
    case 3: LCD_menuItem = 3; LCD_subsetFrame = 1; LCD_page = 1; LCD_main_pointer = 0; LCD_selectOption = 1; LCD_pointer_count = 0;                break; //load into Option
    case 4: LCD_menuItem = 4; LCD_subsetFrame = 1; LCD_page = 1; LCD_main_pointer = 0; LCD_selectOption = 1; LCD_pointer_count = 0;                break; //load into signal display 
    default: break;
    }

}

