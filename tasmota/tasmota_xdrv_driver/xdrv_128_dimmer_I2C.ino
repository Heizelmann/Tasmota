/*
  xdrv_128_dimer_I2C.ino - dimmer via I2C Interface for Tasmota

  SPDX-FileCopyrightText: 2022 Theo Arends and f-reiling

  SPDX-License-Identifier: GPL-3.0-only
*/

#ifdef USE_LIGHT
#ifdef USE_I2C
#ifdef USE_MCP47X6

#define XDRV_128                 61
#define XI2C_128                 67  // See I2CDEVICES.md

#define ADI_DEBUG
#define ADI_LOGNAME                 "ADI: "

#include  <Wire.h>

#include  "MCP47X6.h"

enum TimerStateType {S_OFF, S_ON, S_DIM, S_DIS };

boolean isMultiPressed;

struct SDIMMER {
  uint8_t currentLevel = 0;
  uint32_t timer1;
  TimerStateType timerState = S_OFF;
} SDimmer;

struct ADI_DIMMER_SETTINGS {
  uint8_t level1=73; 
  uint8_t level2=39;
  uint16_t dur1=10000;
  uint16_t dur2=10000;
} AdiSettings;

MCP47X6 theDAC = MCP47X6(MCP47X6_DEFAULT_ADDRESS);

bool AdiInit(void) {
  #ifdef ADI_DEBUG
    AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Advanced Dimmer Driver Starting"));
  #endif  // ADI_DEBUG

  AdiGetSettings();
  AdiSaveSettings();

  Settings->seriallog_level = 0;
  Settings->flag.mqtt_serial = 0;  // Disable serial logging
  //Settings->ledstate = 0;          // Disable LED usage

  // If the module was just changed to this module, set the defaults.
  if (TasmotaGlobal.module_changed) {
     AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "---MODUL CHANGED"));
    //Settings->flag.pwm_control = true;     // SetOption15 - Switch between commands PWM or COLOR/DIMMER/CT/CHANNEL
    //Settings->bri_power_on = Settings->bri_preset_low = Settings->bri_preset_high = 0;
  }

  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Detect"));
  theDAC.begin();
  theDAC.setVReference(MCP47X6_VREF_VDD);
  theDAC.setGain(MCP47X6_GAIN_1X);
  theDAC.saveSettings();

  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "light_type %d"),TasmotaGlobal.light_type);
  TasmotaGlobal.devices_present++;
  TasmotaGlobal.light_type += LT_SERIAL1; //ligttype for one channel dimmer, needed to activate module commands
    
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "light_type %d"),TasmotaGlobal.light_type);
   
  //TasmotaGlobal.light_type += LT_SERIAL;
  //TasmotaGlobal.light_driver = XLGT_11;

  return true; //TODO  
}

// Settings are stored as comma separted string in global settings
void AdiGetSettings(void){
  char parameters[32];
  //AdiSettings.level1 = 0;
  //AdiSettings.level2 = 0;
  //AdiSettings.dur1 = 0;
  //AdiSettings.dur2 = 0; 
  if (strstr(SettingsText(SET_SHD_PARAM), ",") != nullptr){
    #ifdef ADI_DEBUG
      AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Loading params: %s"), SettingsText(SET_SHD_PARAM));
    #endif  // ADI_DEBUG
    AdiSettings.level1 = atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 1));
    AdiSettings.level2 = atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 2));
    AdiSettings.dur1 =   atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 3));
    AdiSettings.dur2 =   atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 4));
  }
}

void AdiSaveSettings(void){
  char parameters[32];
  snprintf_P(parameters, sizeof(parameters), PSTR("%d,%d,%d,%d"),
               AdiSettings.level1,
               AdiSettings.level2,
               AdiSettings.dur1,
               AdiSettings.dur2);
  SettingsUpdateText(SET_SHD_PARAM, parameters);
}

// send desired value [0..100] to external fader engine
void AdiRequestValue(uint16_t value){
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "RequestValue %d"), value);
  light_controller.changeDimmer(value); 
  
  //XdrvMailbox.index = index;
  //XdrvMailbox.payload = dimmer;
  //CmndDimmer();
  //LightAnimate();
}

// callback from fader engine, triggered by FUNC_SET_CHANNELS
bool AdiSetChannels(void){
  uint16_t brightness = ((uint32_t *)XdrvMailbox.data)[0];
  // Use dimmer_hw_min and dimmer_hw_max to constrain our values if the light should be on
  //if (brightness > 0)
  //  brightness = changeUIntScale(brightness, 0, 255, Settings->dimmer_hw_min * 10, Settings->dimmer_hw_max * 10);
  ADISetValue(brightness);
  return true;
}

// send brighness value to DAC, brightness 0..255 --> output 0..4095
void ADISetValue(uint16_t brightness){
  #ifdef ADI_DEBUG
    AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "SetValue %d"), brightness*16); //TODO replace 16 by bitresolution of DAC, currently fixed to 12 bit
  #endif  // ADI_DEBUG
  theDAC.setOutputLevel((uint16_t)(brightness*16));
}

// called at regulary time intervals to check if timer state changed
void DimmerAnimate(){
  switch (SDimmer.timerState) {
  case S_OFF:
    break;
  case S_ON:
    if ( millis() - SDimmer.timer1 > AdiSettings.dur1*1000) {
      SDimmer.timerState = S_DIM;
      AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "-->Dim"));
      SDimmer.timer1 = millis();
      AdiRequestValue(AdiSettings.level2);
    }
    break;
  case S_DIM:
    if ( millis() - SDimmer.timer1 > AdiSettings.dur2*1000) {
      SDimmer.timerState = S_OFF;
      AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "-->Off"));
      AdiRequestValue(0);
    }
    break;
  case S_DIS:
    break;
  }
//      theDAC.setOutputLevel(Settings->mcp47x6_state);
}

// called on button pressed
void DimmerTrigger() {
  if(isMultiPressed) {  // Switch on without timer; keeps on until button pressed again
    SDimmer.timerState = S_DIS;
    AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "FixOn"));
    AdiRequestValue(100);
    }
  else{
    switch (SDimmer.timerState) {
      case S_OFF:
        SDimmer.timerState = S_ON;
        SDimmer.timer1 = millis();
        AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "On"));
        AdiRequestValue( AdiSettings.level1 );
        break;
      case S_ON: case S_DIS:
        SDimmer.timerState = S_OFF;
        AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Off"));
        AdiRequestValue(0);
        break;
      case S_DIM:
        SDimmer.timerState = S_ON;
        SDimmer.timer1 = millis();
        AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Retrigger"));
        AdiRequestValue( AdiSettings.level1 );
        break;
    }
  }
}

void DimmerButtonPressed(){
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Multi Button pressed:%d"),XdrvMailbox.payload);
  isMultiPressed = XdrvMailbox.payload > 1;        
  DimmerTrigger();
 }


/*********************************************************************************************\
 * Commands
\*********************************************************************************************/
/*void CmndRegler(void) {
  //Regler<x> 0..100
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 100)) {
    Settings->mcp47x6_state = XdrvMailbox.payload;
    SDimmer.timerState = S_DIS;
    AdiRequestValue(Settings->mcp47x6_state);
    ResponseCmndIdxNumber(Settings->mcp47x6_state);
  } 
}*/
#define D_PRFX_ADI "ADI"
#define D_CMND_LEVELa "LEVELA"
#define D_CMND_LEVELb "LEVELB"
#define D_CMND_DURa  "DURA"
#define D_CMND_DURb "DURB"

const char kADICommands[] PROGMEM = D_PRFX_ADI "|" 
  D_CMND_LEVELa "|" D_CMND_LEVELb "|" D_CMND_DURa "|" D_CMND_DURb ;

void (* const ADICommand[])(void) PROGMEM = {
  &CmndADILevel1, &CmndADILevel2, &CmndADIDur1, &CmndADIDur2  };


void CmndADILevel1(void) {
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 100)) {
    AdiSettings.level1 = XdrvMailbox.payload;
  }
  AdiSaveSettings();
  ResponseCmndIdxNumber(AdiSettings.level1); 
}

void CmndADILevel2(void) {
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 100)) {
    AdiSettings.level2 = XdrvMailbox.payload;
   }
 AdiSaveSettings();
   ResponseCmndIdxNumber(AdiSettings.level2);
}

void CmndADIDur1(void) {
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 5) && (XdrvMailbox.payload <= 600)) {
    AdiSettings.dur1 = XdrvMailbox.payload;
   } 
   AdiSaveSettings();
   ResponseCmndIdxNumber(AdiSettings.dur1);
}

void CmndADIDur2(void) {
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 5) && (XdrvMailbox.payload <= 600)) {
    AdiSettings.dur2 = XdrvMailbox.payload;
   } 
   AdiSaveSettings();
   ResponseCmndIdxNumber(AdiSettings.dur2);
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv128(uint32_t function) {
  if (!I2cEnabled(XI2C_128)) { return false; }

  bool result = false;
  switch (function) {
    case FUNC_SET_CHANNELS:
        result = AdiSetChannels();
        break;
    case FUNC_MODULE_INIT:
         AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "FUNC_MODULE_INIT"));
         result = AdiInit();
      break;
     case FUNC_EVERY_50_MSECOND:
        DimmerAnimate();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kADICommands, ADICommand);
      break;
    case FUNC_BUTTON_MULTI_PRESSED:
         AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "BUTTON_MULTI_PRESSED:%d : %d"),XdrvMailbox.index, XdrvMailbox.payload);
         //DimmerButtonPressed();
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "pwm_min_perc:%d, pwm_max_perc:%d"),Light.pwm_min, Light.pwm_max);
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "brightness:%d"),light_state.getBri());
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "percent:%d"),light_state.getDimmer(0));
        result = true;
       break;
    //case FUNC_BUTTON_PRESSED:
        //if (!XdrvMailbox.index && ((PRESSED == XdrvMailbox.payload) && (NOT_PRESSED == Button.last_state[XdrvMailbox.index]))) {
           AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Button pressed:%d : %d"),XdrvMailbox.index, XdrvMailbox.payload);
        // }
     //   result = true;
     // break;
  }
  return result;
}

#endif // USE_MCP47X6
#endif // USE_IC2
#endif // USE_LIGHT