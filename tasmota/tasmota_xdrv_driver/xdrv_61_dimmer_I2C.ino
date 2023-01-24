/*
  xdrv_128_dimer_I2C.ino - dimmer via I2C Interface for Tasmota

  SPDX-FileCopyrightText: 2022 Theo Arends and f-reiling

  SPDX-License-Identifier: GPL-3.0-only
*/

#ifdef USE_I2C
#ifdef USE_MCP47X6
/*********************************************************************************************\
 * DS3502 - digital potentiometer (https://datasheets.maximintegrated.com/en/ds/DS3502.pdf)
 *
 * I2C Address: 0x28 .. 0x2B
\*********************************************************************************************/

#define XDRV_61                  61
#define XI2C_67                  67  // See I2CDEVICES.md

#include  <Wire.h>
#include "Adafruit_SGP40.h"
Adafruit_SGP40 sgp40;

//#include  "./MCP47X6/MCP47X6.h" //in Tasmota directory
#include  "MCP47X6.h" //
MCP47X6 theDAC = MCP47X6(MCP47X6_DEFAULT_ADDRESS);

void MCP47X6SetWiper(uint32_t idx) {
      AddLog(LOG_LEVEL_DEBUG, "HZ:Set Wiper %d", Settings->mcp47x6_state);
      theDAC.setOutputLevel(Settings->mcp47x6_state);
}

void MCP47X6Detect(void) {
    AddLog(LOG_LEVEL_INFO, "HZ:Detect");
    theDAC.begin();
    theDAC.setVReference(MCP47X6_VREF_VDD);
    theDAC.setGain(MCP47X6_GAIN_1X);
    theDAC.saveSettings();  
}


/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

const char kMCP47X6Commands[] PROGMEM = "|"  // No Prefix
  "Regler" ;

void (* const MCP47X6Command[])(void) PROGMEM = {
  &CmndWiper };

void CmndWiper(void) {
  //Wiper<x> 0..4095
      AddLog(LOG_LEVEL_INFO, "%s %d %d", XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 4095)) {
    Settings->mcp47x6_state = XdrvMailbox.payload;
    MCP47X6SetWiper(0);
    ResponseCmndIdxNumber(Settings->mcp47x6_state);
  } 
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv61(uint32_t function) {
  if (!I2cEnabled(XI2C_67)) { return false; }

  bool result = false;
  switch (function) {
    case FUNC_INIT:
      MCP47X6Detect();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kMCP47X6Commands, MCP47X6Command);
      break;
  }
  return result;
}

#endif // USE_MCP47X6
#endif // USE_IC2