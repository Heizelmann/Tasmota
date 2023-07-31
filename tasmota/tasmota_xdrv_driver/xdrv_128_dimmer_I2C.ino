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
  bool overheated;
  uint8_t currentLevel = 0;
  uint32_t timer1;
  TimerStateType timerState = S_OFF;
  float temperature[2];
  uint32_t illuminance;
} SDimmer;

struct ADI_DIMMER_SETTINGS {
  uint8_t level1=73; 
  uint8_t level2=39;
  uint16_t dur1=10000;
  uint16_t dur2=10000;
  uint8_t maxTemp;
  uint8_t maxLux;
} AdiSettings;

MCP47X6 theDAC = MCP47X6(MCP47X6_DEFAULT_ADDRESS);

bool AdiInit(void) {
 #ifdef ADI_DEBUG 
    AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Advanced Dimmer Driver Starting"));
  #endif  // ADI_DEBUG

  AdiGetSettings();
  AdiSaveSettings();

  // TSettings type is defined in Tasmota_types.h
  //Settings is declared in tasmota.ino
  
  Settings->seriallog_level = 0;
  Settings->flag.mqtt_serial = 0;  // Disable serial logging
  //Settings->ledstate = 0;          // Disable LED usage

  // If the module was just changed to this module, set the defaults.
  if (TasmotaGlobal.module_changed) {
     AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "---MODUL CHANGED"));
    //Settings->flag.pwm_control = true;     // SetOption15 - Switch between commands PWM or COLOR/DIMMER/CT/CHANNEL
    //Settings->bri_power_on = Settings->bri_preset_low = Settings->bri_preset_high = 0;
  }

  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Detect MCP47x6 DAC"));
  theDAC.begin();
  theDAC.setVReference(MCP47X6_VREF_VDD);
  theDAC.setGain(MCP47X6_GAIN_1X);
  theDAC.saveSettings();

  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "light_type %d"),TasmotaGlobal.light_type);
  TasmotaGlobal.devices_present++;
  TasmotaGlobal.light_type += LT_SERIAL1; //ligttype for one channel dimmer, needed to activate module commands
    
  #ifdef ADI_DEBUG
    AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "light_type %d"),TasmotaGlobal.light_type);
  #endif

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
  //AdiSettings.maxTemp = 0; 
  if (strstr(SettingsText(SET_SHD_PARAM), ",") != nullptr){
    #ifdef ADI_DEBUG
      AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Loading params: %s"), SettingsText(SET_SHD_PARAM));
    #endif  // ADI_DEBUG
    AdiSettings.level1 = atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 1));
    AdiSettings.level2 = atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 2));
    AdiSettings.dur1 =   atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 3));
    AdiSettings.dur2 =   atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 4));
    AdiSettings.maxTemp =   atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 5));
    AdiSettings.maxLux =   atoi(subStr(parameters, SettingsText(SET_SHD_PARAM), ",", 6));
  }
}

void AdiSaveSettings(void){
  char parameters[32];
  snprintf_P(parameters, sizeof(parameters), PSTR("%d,%d,%d,%d,%d,%d"),
               AdiSettings.level1,
               AdiSettings.level2,
               AdiSettings.dur1,
               AdiSettings.dur2,
               AdiSettings.maxTemp,
               AdiSettings.maxLux);
  SettingsUpdateText(SET_SHD_PARAM, parameters);
}

// send dimmer desired value [0..100] to external fader engine
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

// send brightness value to DAC, brightness 0..255 --> output 0..4095
void ADISetValue(uint16_t brightness){
  #ifdef ADI_DEBUG
    AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "SetValue %d"), brightness*16); //TODO replace 16 by bitresolution of DAC, currently fixed to 12 bit
  #endif  // ADI_DEBUG
  theDAC.setOutputLevel((uint16_t)(brightness*16));
}

// called at regulary time intervals to react on timer state changed
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
#define D_PRFX_ADI "ADI"          // Advanced DImmer
#define D_CMND_LEVELa "LEVELA"    // Dimmer level for ON state
#define D_CMND_LEVELb "LEVELB"    // Dimmer level for DIM state
#define D_CMND_DURa  "DURA"       // Duration of ON state
#define D_CMND_DURb "DURB"        // Duration od DIM state
#define D_CMND_TEMP_TH "MAXTEMP"  // Temperature threshold for switch off
#define D_CMND_LUX_TH "MAXLUX"    // Light threshold for motion detect

const char kADICommands[] PROGMEM = D_PRFX_ADI "|" 
  D_CMND_LEVELa "|" D_CMND_LEVELb "|" D_CMND_DURa "|" D_CMND_DURb "|" D_CMND_TEMP_TH "|" D_CMND_LUX_TH ;

void (* const ADICommand[])(void) PROGMEM = {
  &CmndADILevel1, &CmndADILevel2, &CmndADIDur1, &CmndADIDur2, &CmndADIMaxTemp, &CmndADIMaxLux };


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

void CmndADIMaxTemp(void) {
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 90)) {
    AdiSettings.maxTemp = XdrvMailbox.payload;
   } 
   AdiSaveSettings();
   ResponseCmndIdxNumber(AdiSettings.maxTemp);

}

void CmndADIMaxLux(void) {
  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "%s %d %d"), XdrvMailbox.command, XdrvMailbox.index,XdrvMailbox.payload);
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 255)) {
    AdiSettings.maxLux = XdrvMailbox.payload;
   } 
   AdiSaveSettings();
   ResponseCmndIdxNumber(AdiSettings.maxLux);

}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

// Find appropriate unit for measurement type.
const char *UnitfromType1(const char *type)
{
  if (strcmp(type, "time") == 0) {
    return "seconds";
  }
  if (strcmp(type, "temperature") == 0 || strcmp(type, "dewpoint") == 0) {
    return "celsius";
  }
  if (strcmp(type, "pressure") == 0) {
    return "hpa";
  }
  if (strcmp(type, "voltage") == 0) {
    return "volts";
  }
  if (strcmp(type, "current") == 0) {
    return "amperes";
  }
  if (strcmp(type, "mass") == 0) {
    return "grams";
  }
  if (strcmp(type, "carbondioxide") == 0) {
    return "ppm";
  }
  if (strcmp(type, "humidity") == 0) {
    return "percentage";
  }
  if (strcmp(type, "id") == 0) {
    return "untyped";
  }
  if (strcmp(type, "illuminance") == 0) {
    return "lx";
  }
  return "";
}

// Replace spaces and periods in metric name to match Prometheus metrics
// convention.
String FormatMetricName(const char *metric) {
  String formatted = metric;
  formatted.toLowerCase();
  formatted.replace(" ", "_");
  formatted.replace(".", "_");
  return formatted;
}

void getSensorData(){
       char namebuf[64];
       uint8_t tcnt = 0;

        // Get MQTT sensor data as JSONstring
        ResponseClear();
        MqttShowSensor(true); //Pull sensor data
        String jsonStr = ResponseData();
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "JSON-STRING V1.1 : %s "), jsonStr.c_str());

        // Parse JSON String to get specific sensor data
        JsonParser parser((char *)jsonStr.c_str());
        JsonParserObject root = parser.getRootObject();

        /* This is wrong
        if (!root) { ResponseCmndChar_P(PSTR("Invalid JSON")); return; }
        uint16_t d = root.getUInt(PSTR("Illuminance"), 0xFFFF);   // case insensitive
        //bool     b = root.getBool(PSTR("Power"), false);
        float    f = root.getFloat(PSTR("Temperature"), -100);
        char str_temp[5];
        AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE00: %d, %s"),d, dtostrf(f, 3, 2, str_temp));
        */

        if (root) { // did JSON parsing succeed?
          for (auto key1 : root) {
              JsonParserToken value1 = key1.getValue();
              if (value1.isObject()) {
                JsonParserObject Object2 = value1.getObject();
                for (auto key2 : Object2) {
                  JsonParserToken value2 = key2.getValue();
                  if (value2.isObject()) {
                    JsonParserObject Object3 = value2.getObject();
                    for (auto key3 : Object3) {
                      const char *value = key3.getValue().getStr(nullptr);
                      if (value != nullptr && isdigit(value[0])) {
                        String sensor = FormatMetricName(key2.getStr());
                        String type = FormatMetricName(key3.getStr());

                        snprintf_P(namebuf, sizeof(namebuf), PSTR("sensors_%s_%s"),
                          type.c_str(), UnitfromType1(type.c_str()));
                        ///////////WritePromMetricStr(namebuf, kPromMetricGauge, value,  PSTR("sensor"), sensor.c_str(),nullptr);
                        AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE1: %s = %s from sensor %s"),namebuf, value, sensor.c_str());
                      }
                    }
                  } else {
                    const char *value = value2.getStr(nullptr);
                    if (value != nullptr && isdigit(value[0])) {
                      String sensor = FormatMetricName(key1.getStr());
                      String type = FormatMetricName(key2.getStr());
                      if (strcmp(type.c_str(), "totalstarttime") != 0) {  // this metric causes Prometheus of fail
                        snprintf_P(namebuf, sizeof(namebuf), PSTR("sensors_%s_%s"),
                          type.c_str(), UnitfromType1(type.c_str()));

                        if (strcmp(type.c_str(), "id") == 0) {            // this metric is NaN, so convert it to a label, see Wi-Fi metrics above
                          /*WritePromMetricInt32(namebuf, kPromMetricGauge, 1,
                            PSTR("sensor"), sensor.c_str(),
                            PSTR("id"), value,
                            nullptr);
                            */
                           //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE2: %s = %s from sensor %s"),namebuf, value, sensor.c_str());

                        } else {

                          // sensor.c_str() // Name des Sensors
                          // type.c_str()   // Type e.g. Temperatur, Illuminance
                          //value // Wert als String
    

                          /*WritePromMetricStr(namebuf, kPromMetricGauge, value,
                            PSTR("sensor"), sensor.c_str(),
                            nullptr);
                            */
                          if(strcmp(namebuf,"sensors_illuminance_lx")==0){
                            SDimmer.illuminance = value2.getUInt();
                            //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE3 Illuminance = %d "),SDimmer.illuminance);
                          }
                          else if(strcmp(namebuf,"sensors_temperature_celsius")==0){
                            SDimmer.temperature[tcnt] = value2.getFloat(); 
                            char str_temp[4];
                            //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE3 Temperature %d = %s "),tcnt,dtostrf(SDimmer.temperature[tcnt],3, 1, str_temp));
                             //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE3 Temperature = %f "),SDimmer.temperature);//!!! does not work, not implemented
                            tcnt++;
                          }
                          else {
                            AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE3: %s = %s from sensor %s"),namebuf, value, sensor.c_str());
                          }
                        }
                      }
                    }
                  }
                }
              } else {
                const char *value = value1.getStr(nullptr);
                String sensor = FormatMetricName(key1.getStr());

                if (value != nullptr && isdigit(value[0] && strcmp(sensor.c_str(), "time") != 0)) {  //remove false 'time' metric
                  /*WritePromMetricStr(PSTR("sensors"), kPromMetricGauge, value,
                    PSTR("sensor"), sensor.c_str(),
                    nullptr);
                    */
                  AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "PARSE4: sensors = %s from sensor %s"), value, sensor.c_str());  
                }
              }
            }
          }
  }
  
  bool Xdrv128(uint32_t function) {
  if (!I2cEnabled(XI2C_128)) { return false; }

  bool result = false;
  switch (function) {
    case FUNC_BUTTON_MULTI_PRESSED:
         AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "---Button Pressed: idx:%d, cnt: : %d"),XdrvMailbox.index, XdrvMailbox.payload);
         DimmerButtonPressed();
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "pwm_min_perc:%d, pwm_max_perc:%d"),Light.pwm_min, Light.pwm_max);
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "brightness:%d"),light_state.getBri());
        //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "percent:%d"),light_state.getDimmer(0));
        result = true;
       break;
    case FUNC_SET_CHANNELS:
        result = AdiSetChannels();
        break;
    case FUNC_MODULE_INIT:
         AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "FUNC_MODULE_INIT"));
         result = AdiInit();
      break;
      case FUNC_EVERY_SECOND:
        getSensorData();
        if ( TasmotaGlobal.temperature_celsius > AdiSettings.maxTemp ){
          SDimmer.overheated = true;
        }
        else if (TasmotaGlobal.temperature_celsius < AdiSettings.maxTemp - 3){
          SDimmer.overheated = false;
        }
       if (SDimmer.overheated) {
        AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "OVERHEATED"));
       }
        break;
     case FUNC_EVERY_50_MSECOND:
        DimmerAnimate();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kADICommands, ADICommand);
      break;
    //case FUNC_BUTTON_PRESSED:
        //if (!XdrvMailbox.index && ((PRESSED == XdrvMailbox.payload) && (NOT_PRESSED == Button.last_state[XdrvMailbox.index]))) {
           //AddLog(LOG_LEVEL_INFO, PSTR(ADI_LOGNAME "Button pressed:%d : %d"),XdrvMailbox.index, XdrvMailbox.payload);
        // }
     //   result = true;
     // break;
  }
  return result;
}

#endif // USE_MCP47X6
#endif // USE_IC2
#endif // USE_LIGHT