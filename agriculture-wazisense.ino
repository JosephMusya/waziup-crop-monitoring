#include <Wire.h>
// you have to install the "LiquidCrystal" library published by Arduino from Tools > Library Manager
#include "LiquidCrystal.h"

/*-------*/

#include <xlpp.h>
XLPP xlpp(120);

/*-------*/
// you have to install the "I2C-Sensor-Lib iLib" library published by Ingmar Splitt from Tools > Library Manager
#include "i2c_SI7021.h"
SI7021 si7021;

/*-------*/

#include "WaziDev.h"
WaziDev wazidev;

/*-------*/

// The solar input is attached to pin A2 with a voltage divider
// We need to calibrate it based on our solar panel
// Be careful: Do not connect a high voltage solar panel to the board i.e. Max: 6V
#define SolarLevelPin A2
/*-------*/

// To save power we use a digital pin to power on the soil moisture sensor
// whenever we need to read it
#define SoilMoistureDataPin A6
#define SoilMoisturePowerPin 6

/*-------*/

//LDR configuration
#define LDR_PowerPin 7
#define LDR_DataPin A7

#define NightThreshold 700


//Led configuration, deactivate the led on production(When device is deployed)
#define LEDPin 8

//Device check for deployment and idle mode
bool debug = true;  //Set to true by default, change to false on deplyment to conserve power (In off state, the debug led is turned off)
bool sleep = false;

/*-----------------------------*/

// This function reads an analogue pin couple of times
// and makes an average of it to have more accuracy
unsigned int analogReadAvg(unsigned int pin)
{
#define TotalReads 10

  unsigned int sum = 0;
  for (int i = 0; i != TotalReads; i++)
  {
    delay(50);
    sum += analogRead(pin);
  }

  return sum / TotalReads;
}

/*-----------------------------*/

void setup()
{
  Serial.begin(38400);

  /*-----------*/

  unsigned char LoRaWANKey[16] = {0x23, 0x15, 0x8D, 0x3B, 0xBC, 0x31, 0xE6, 0xAF, 0x67, 0x0D, 0x19, 0x5B, 0x5A, 0xED, 0x55, 0x28}; // 23158D3BBC31E6AF670D195B5AED5528
  unsigned char DevAddr[4] = {0x26, 0x01, 0x1D, 0x87};                                                                             // 26011D87

  /*-----------*/

  uint8_t errSetup = wazidev.setupLoRaWAN(DevAddr, LoRaWANKey);
  if (errSetup != 0)
  {
    serialPrintf("LoRaWAN Err %d\n", errSetup);
    delay(60000);
    return;
  }

  /*-----------*/

  pinMode(LEDPin, OUTPUT);
  pinMode(SoilMoisturePowerPin, OUTPUT);
  pinMode(LDR_PowerPin, OUTPUT);

  digitalWrite(LDR_PowerPin,HIGH);
  /*-----------*/

  serialPrintf("Probe SI7021: ");
  if (si7021.initialize())
  {
    serialPrintf("SI7021 Sensor found!\n");
  }
  else
  {

    serialPrintf("SI7021 Sensor missing\n");
    while (1)
    {
      delay(1 * 60 * 1000);
    };
  }

  /*-----------*/

  for (int i = 0; i != 3; i++)
  {

    digitalWrite(LEDPin, HIGH);
    delay(50);
    digitalWrite(LEDPin, LOW);
    delay(50);
  }
}

/*-----------------------------*/

void collectData() {
  float humidity, temperature;

  si7021.getHumidity(humidity);
  si7021.getTemperature(temperature);
  si7021.triggerMeasurement();

  serialPrintf("\nTemperature:\t\t%d.%02d \nRelative Humidity:\t%d.%02d", (int)temperature, (int)(temperature * 100) % 100, (int)humidity, (int)(humidity * 100) % 100);

  /*----------*/

  unsigned int solarLevel = analogReadAvg(SolarLevelPin);
  serialPrintf("\nSolar Level:\t\t%d", solarLevel);

  /*----------*/

  // Power on the sensor
  digitalWrite(SoilMoisturePowerPin, HIGH);
  delay(1000);

  unsigned int moistureLevel = analogReadAvg(SoilMoistureDataPin);
  serialPrintf("\nMoisture Level:\t\t%d", moistureLevel);

  // Power off the sensor
  digitalWrite(SoilMoisturePowerPin, LOW);

  /*----------*/

  xlpp.reset();
  xlpp.addTemperature(1, temperature);
  xlpp.addRelativeHumidity(2, humidity);
  xlpp.addAnalogInput(3, moistureLevel);
  xlpp.addAnalogInput(4, solarLevel);


  serialPrintf("\nLoRaWAN send ... ");
  int e = wazidev.sendLoRaWAN(xlpp.getBuffer(), xlpp.getSize());
  if (e == 0)
  {
    serialPrintf("\tOK");
  }
  else
  {
    serialPrintf("\nErr: %d", e);
  }
}

void loop(void)
{
  serialPrintf("\n\n---------------------\n");

  float lightIntensity = analogRead(A7);  //Read the light intensity level in every iteration
  Serial.print("Darkness --------------->");
  Serial.print(lightIntensity);
  Serial.println();
  /*----------*/
  //Blink led to test device functionality in development mode.
  //Turn debug off to save power in deployment mode
  if (debug) {
    digitalWrite(LEDPin, HIGH);
    delay(500);
    digitalWrite(LEDPin, LOW);
    delay(500);
  }

  if (lightIntensity > NightThreshold){
    sleep = true; 
  }

  else{
    sleep = false;
  }

  /*----------*/
  //Collect and send data if the daythreshold is reached.
  //Else keep the device in sleep mode
  if (!sleep) {
    collectData();
    serialPrintf("\nWaiting for 15(m)... ");
    // wait before pushing, we need to use sleep mode for a better battery performance
    delay(3000); // 15 * 60 * 1000 : 15 minutes
    serialPrintf("Done waiting");
  }
  else{
    if (debug){
      serialPrintf("Device in sleep mode...");
      Serial.println(NightThreshold);
    }
  }
}
