// Designed for Wemos D1 mini

#define IS_DEBUG
#ifdef  IS_DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#include  <Wire.h>
#include  <MCP47X6.h>
MCP47X6 theDAC = MCP47X6(MCP47X6_DEFAULT_ADDRESS);

#include <OneWire.h>
#include <DallasTemperature.h>

#include "OneButton.h" // from https://github.com/mathertel/OneButton
#define RELEASE "0.8"
#define SERIAL_BAUD 115200

//for LED status
#include <Ticker.h>   //included in library for ESP8266, NOT an external lib
Ticker ticker1;
Ticker ticker2;

//#define LED_BUILTIN D4   //D4 is pin 2 on WEMOS D1 mini see https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h
#define SWITCH_PIN    D3
#define LED_PIN       D8   //is LED_BUILTIN on WEMOS D1 mini see https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h
#define BUTTON_PIN    D7
#define TEMPSENS_PIN  D6
int ledState = LOW;

OneWire oneWire(TEMPSENS_PIN);
DallasTemperature sensors(&oneWire);

OneButton button1(BUTTON_PIN, true);
OneButton button2(SWITCH_PIN, true);


void setup() {
  Serial.begin(SERIAL_BAUD);   delay(100);
  Serial.print(F("\r\nFirmware Release: "));
  Serial.println(RELEASE);
  Serial.print(F("\r\nChip ID: 0x"));
  Serial.println(ESP.getChipId(), HEX);
  Serial.print(F("Flash Size (bytes): "));
  Serial.println(ESP.getFlashChipSize());

  // Setup and Test LEDs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  ticker1.attach(0.6, tick1);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);

  // Setup buttons
  button1.attachClick(click1);
  button2.attachClick(click2);
  button1.attachDoubleClick(doubleclick1);
  button2.attachDoubleClick(doubleclick2);
  button1.attachDuringLongPress(duringLongPress1);
  button2.attachDuringLongPress(duringLongPress2);
  button1.attachLongPressStop(longPressStop1);
  button2.attachLongPressStop(longPressStop2);

  // Start the DS18B20 sensor
  sensors.begin();

  Wire.begin();
  Serial.println(theDAC.testConnection() ? "MCP47X6 connection successful" : "MCP47X6 connection failed");

  theDAC.begin();
  theDAC.setVReference(MCP47X6_VREF_VDD);
  theDAC.setGain(MCP47X6_GAIN_1X);
  theDAC.saveSettings();
}

static uint16_t _level = 0;
static int _dmode = 1;
const static int _maxLevel = 4095;
const static int _incLevel = 4;

void loop() {

  button1.tick();
  button2.tick();

  delay(2);

}

void tick1() {
  int state = digitalRead(LED_BUILTIN );  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN , !state);     // set pin to the opposite state
}

void click1() {
  DEBUG_PRINTLN("Button 1 click.");
  //webSocketPrinter.println("Button 1 click.");
  //triggerLight();
} // click1

void click2() {
  DEBUG_PRINTLN("Button 2 .");
  //webSocketPrinter.println("Button 2 click.");
  //triggerLight();
} // click1

void doubleclick1() {
  DEBUG_PRINTLN("Button 1 doubleclick.");
  if (_level < _maxLevel) {
    _dmode = 0;
    _level = _maxLevel;
  } else {
    _dmode = 1;
    _level = 0;
  }
  theDAC.setOutputLevel(_level);
} // doubleclick1

void doubleclick2() {
  DEBUG_PRINTLN("Button 2 doubleclick.");
  //switchOn("Button2");
} // doubleclick1

uint16_t _speed = 10;
unsigned long previousMillis = 0;
void duringLongPress1() {
  if (millis() - previousMillis < _speed)
    return;
  previousMillis = millis();
  if (_dmode == 1) {
    _level = _level + _incLevel;
    if (_level >= _maxLevel) {
      getTemp();
      _level = _maxLevel;
    }
  } else {
    if (_level > _incLevel) {
      _level = _level - _incLevel;
    } else {
      _level = 0;
       getTemp();
    }
  }
  DEBUG_PRINT("Button 1 duringLongPress." );
  DEBUG_PRINTLN(_level);
  theDAC.setOutputLevel(_level);
} // duringLongPress1

void duringLongPress2() {
  DEBUG_PRINTLN("Button 2 duringLongPress.");
  //switchOn("Button2");
} // duringLongPress2

void longPressStop1() {
  DEBUG_PRINT ("Button 1 longPressStop.");
  _dmode = !_dmode;
  DEBUG_PRINTLN(_dmode);
} // longPressStop1

void longPressStop2() {
  DEBUG_PRINT ("Button 2 longPressStop.");
  _dmode = !_dmode;
  DEBUG_PRINTLN(_dmode);
} // longPressStop2

void getTemp() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("ºC");
}
