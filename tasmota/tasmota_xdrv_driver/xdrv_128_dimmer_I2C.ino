/*
  xdrv_128_dimer_I2C.ino - dimmer via I2C Interface for Tasmota

  SPDX-FileCopyrightText: 2022 Theo Arends and f-reiling

  SPDX-License-Identifier: GPL-3.0-only
*/

#ifdef USE_I2C
#ifdef USE_MCP47X6

#define XDRV_128                 61
#define XI2C_128                 67  // See I2CDEVICES.md

#include  <Wire.h>

#include  "MCP47X6.h"

enum TimerStateType {S_OFF, S_ON, S_DIM, S_DIS };


const char kMCP47X6Commands[] PROGMEM = "|"  // No Prefix
  "Regler" ;

void (* const MCP47X6Command[])(void) PROGMEM = {
  &CmndWiper };

struct SDIMMER {
  uint8_t currentLevel = 0;
  uint32_t timer1;
  TimerStateType timerState = S_OFF;
} SDimmer;

struct SDIMMER_SETTINGS {
  uint16_t dur1=10000;
  uint16_t dur2=10000;
  uint16_t level1=3000; 
  uint16_t level2=1600;
 } SSettings;




MCP47X6 theDAC = MCP47X6(MCP47X6_DEFAULT_ADDRESS);

void MCP47X6SetWiper(uint32_t idx) {
      AddLog(LOG_LEVEL_DEBUG, "HZ:Set Wiper %d", Settings->mcp47x6_state);
      theDAC.setOutputLevel(Settings->mcp47x6_state);
}

bool MCP47X6Detect(void) {
    AddLog(LOG_LEVEL_INFO, "HZ:Detect");
    theDAC.begin();
    theDAC.setVReference(MCP47X6_VREF_VDD);
    theDAC.setGain(MCP47X6_GAIN_1X);
    theDAC.saveSettings();
    return true; //TODO  
}



void CmndWiper(void) {
  //Wiper<x> 0..4095
      AddLog(LOG_LEVEL_INFO, "%s %d %d", XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 4095)) {
    Settings->mcp47x6_state = XdrvMailbox.payload;
    MCP47X6SetWiper(0);
    ResponseCmndIdxNumber(Settings->mcp47x6_state);
  } 
}

void DimmerSetValue(uint16_t value){
     theDAC.setOutputLevel(value);
     AddLog(LOG_LEVEL_INFO, PSTR("changeDimmer");
     light_controller.changeDimmer(value)):
}

void DimmerAnimate(){
    switch (SDimmer.timerState) {
    case S_OFF:
      break;
    case S_ON:
      if ( millis() - SDimmer.timer1 > SSettings.dur1) {
        SDimmer.timerState = S_DIM;
        AddLog(LOG_LEVEL_INFO, "-->Dim");
        SDimmer.timer1 = millis();
        DimmerSetValue(SSettings.level2);
      }
      break;
    case S_DIM:
      if ( millis() - SDimmer.timer1 > SSettings.dur2) {
        SDimmer.timerState = S_OFF;
        AddLog(LOG_LEVEL_INFO, "-->Off");
        DimmerSetValue(0);
      }
      break;
    case S_DIS:
      break;
  }

//      theDAC.setOutputLevel(Settings->mcp47x6_state);
}

void DimmerTrigger() {
  DEBUG_PRINTLN("triggerLight");
  switch (SDimmer.timerState) {
    case S_OFF:
      SDimmer.timerState = S_ON;
      SDimmer.timer1 = millis();
      AddLog(LOG_LEVEL_INFO, "On");
      DimmerSetValue( SSettings.level1 );
       break;
    case S_ON: case S_DIS:
      SDimmer.timerState = S_OFF;
      AddLog(LOG_LEVEL_INFO, "Off");
      DimmerSetValue(0);
      break;
    case S_DIM:
      SDimmer.timerState = S_ON;
      SDimmer.timer1 = millis();
      AddLog(LOG_LEVEL_INFO, "Retrigger");
      DimmerSetValue( SSettings.level1 );
      break;
  }
}

void DimmerButtonPressed(){
      AddLog(LOG_LEVEL_INFO, PSTR("Multi Button pressed:%d"),XdrvMailbox.payload);       
      DimmerTrigger();
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv128(uint32_t function) {
  if (!I2cEnabled(XI2C_128)) { return false; }

  bool result = false;
  switch (function) {
    case FUNC_MODULE_INIT:
      result = MCP47X6Detect();
      break;
     case FUNC_EVERY_50_MSECOND:
        DimmerAnimate();
        break;
    case FUNC_COMMAND:
       result = DecodeCommand(kMCP47X6Commands, MCP47X6Command);
      break;
    case FUNC_BUTTON_MULTI_PRESSED:
        DimmerButtonPressed();
        AddLog(LOG_LEVEL_INFO, PSTR("pwm_min:%d"),Light.pwm_min);
        AddLog(LOG_LEVEL_INFO, PSTR("getBri:%d"),light_state.getBri());
        AddLog(LOG_LEVEL_INFO, PSTR("getDimmer:%d"),light_state.getDimmer(0));
        result = true;
       break;
    /*case FUNC_BUTTON_PRESSED:
        if (!XdrvMailbox.index && ((PRESSED == XdrvMailbox.payload) && (NOT_PRESSED == Button.last_state[XdrvMailbox.index]))) {
           AddLog(LOG_LEVEL_INFO, PSTR("Button pressed:%d"),XdrvMailbox.payload);
         }
        result = true;
      break;*/
  }
  return result;
}

#endif // USE_MCP47X6
#endif // USE_IC2