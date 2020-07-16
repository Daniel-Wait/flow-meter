/*
  DigitalReadSerial

  Reads a digital input on pin 2, prints the result to the Serial Monitor

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/DigitalReadSerial
*/
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

#define OUTPUT_MIN 1638        // 1638 counts (10% of 2^14 counts or 0x0666)
#define OUTPUT_MAX 14745       // 14745 counts (90% of 2^14 counts or 0x3999)
#define PRESSURE_MIN -497.69        // min is 0 for sensors that give absolute values
#define PRESSURE_MAX 497.69   // 1.6bar (I want results in bar)


// For the breakout board, you can use any 2 or 3 pins.
// These pins will also work for the 1.8" TFT shield.
#define TFT_CS         7
#define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         8


Adafruit_ADS1115 ads1115(0x48);

// For 1.44" and 1.8" TFT with ST7735 (including HalloWing) use:
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

uint8_t pressureChipSel = 10;

// digital pin 2 has a pushbutton attached to it. Give it a name:
uint8_t sampleButton = 2;
uint8_t recordButton = 3;

uint8_t prevState = 1;
uint8_t samp_nrec = 1;
uint8_t sampleStatePrev = 1;
uint8_t recordStatePrev = 1;
long unsigned samplePress = 0;
long unsigned recordPress = 0;

const unsigned long samplePeriod = 5;
const uint8_t numReadings = 5;
unsigned long sampleTime = 0;
uint8_t sampleNumber = 0;

float airflow50CCM = 0;
float airflow750CCM = 0;
//float airflow20LPM =0;
float instAirflow = 0; 
float maxAirflow = 0;
float minAirflow = 0;
float readingsAirflow[5] = {0};
float totalAirflow = 0;
float averageAirflow = 0;

float instPressure = 0; 
float maxPressure = 0;
float minPressure = 0;
float readingsPressure[5] = {0};
float totalPressure = 0;
float averagePressure = 0;

const uint8_t adcChannel = 0;
const uint8_t adcVref = 5;

// the setup routine runs once when you press reset:
void setup()
{
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  //Wake-up I2C bus
  Wire.begin();
  
  // set the pushbutton's pin an input:
  pinMode(sampleButton, INPUT_PULLUP);
  pinMode(recordButton, INPUT_PULLUP);

  //pressure sensor
  SPI.begin();
  pinMode(pressureChipSel, OUTPUT);
  digitalWrite(pressureChipSel, HIGH);

  //A/D converter (for 20LPM max and 750CCM max airflow sensors)
  ads1115.begin();
  ads1115.setGain(GAIN_TWOTHIRDS);
  
  //50sccm max airflow sensor
  userDelay(17);
  uint8_t* a, b;
  getDataHAF50(a, b);
  userDelay(10);

  // Use this initializer if using a 1.8" TFT screen:
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);
  tft.setTextWrap(false);
  delay(1000); 
}

// the loop routine runs over and over again forever:
void loop() 
{
  //--------------------------------------------------------------------------//
  //set flag to indicate operation mode via button input:
  //--------------------------------------------------------------------------//
  optionSampleRecord();
  
  //--------------------------------------------------------------------------//
  //read ADC values:
  //--------------------------------------------------------------------------//
  //uint16_t adcValue20LPM = ads1115.readADC_SingleEnded(0);
  //float adcVolts20LPM = adcValue20LPM*0.1875/1000;
  
  uint16_t adcValue750 = ads1115.readADC_SingleEnded(1);
  float adcVolts750 = adcValue750*0.1875/1000;
   
  //--------------------------------------------------------------------------//
  //calculate applied flow from airflow sensors:
  //--------------------------------------------------------------------------//  
  /* Analog AWM5104:
   *AIRFLOW[0;20](LPM) and OUT[1;5](V) 
   *Linear relation :. AIRFLOW = 5*(OUT-1) 
   *Multiply by 1000 to convert from LPM to CCM
  */
  //airflow20LPM = (adcVolts20LPM-1)*5;
  //airflow20LPM *= 1000;

  /* Analog HAFBLF0750CAA5:
   *Flow Applied = Full Scale Flow*(Vo/Vs - 0.5)/0.4
  */
  airflow750CCM = 750*(adcVolts750/5 - 0.5)/0.4;

  /* Digital HAFBLF0050C4AX5:
   *Get 2 bytes of data from the digial airflow sensor
   *Digital Output Code = MSB<<8+LSB (Byte 1 = MSB and Byte 2 = LSB)
   *Flow Applied = Full Scale Flow * [(Digital Output Code/16384) - 0.5]/0.4
  */
  byte a, b;
  getDataHAF50(&a, &b);
  int digiOutCode = a*256 + b;
  airflow50CCM = ((float)(digiOutCode)/16384 - 0.5);
  airflow50CCM *= 125;

  //--------------------------------------------------------------------------//
  //calculate pressure from sensor:
  //--------------------------------------------------------------------------//
  /* 002NDSA5 sensor:
   * SPI read pressure in Pa
   * convert to cmH2O
   */
  instPressure = 1.0 * ((float)readSensor(pressureChipSel) - OUTPUT_MIN) * (PRESSURE_MAX - PRESSURE_MIN) / (OUTPUT_MAX - OUTPUT_MIN) + PRESSURE_MIN;
  instPressure /= 98.06649999999982;

  //--------------------------------------------------------------------------//
  //select appropriate sensor:
  //--------------------------------------------------------------------------//
  instAirflow = airflow50CCM;

  if(airflow50CCM > 50)
  {
    instAirflow = airflow750CCM;
  }

  if(airflow50CCM < -50)
  {
    instAirflow = airflow750CCM;
  }
  /*if(airflow750CCM > 750)
  {
    instAirflow = airflow20LPM;
  }*/
  
  //--------------------------------------------------------------------------//
  //run in mode defined by flag:
  //--------------------------------------------------------------------------//
  if(prevState != samp_nrec)
  {
    tft.fillScreen(ST77XX_BLACK);
  }
  if(!samp_nrec)
  {
    record();   
    
    tft.setCursor(0, 0);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.println("RECORDING");
    tft.println("");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.println("AIRFLOW (sccm)");
    tft.println("Max          Min");
      
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(0, 35);
    tft.print(maxAirflow, 1);
    tft.print("  "); 
    tft.setCursor(tft.width()/2, 35);
    tft.print(minAirflow, 1);
    tft.println("  "); 
    tft.println("");
    
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.println("PRESSURE (Pa)");
    tft.println("Max          Min");
    
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(0, 85);
    tft.print(maxPressure, 1);
    tft.print("  "); 
    tft.setCursor(tft.width()/2, 85);
    tft.print(minPressure, 1);
    tft.println("  "); 
    tft.println("");
    
  }
  else
  {
    sample();
    
    tft.setCursor(0, 0);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
    tft.println("SAMPLING");
    tft.println("");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.println("AIRFLOW (sccm)");
    
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(averageAirflow);
    tft.println("     ");
    tft.println("");
    
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.println("PRESSURE (Pa)");
    
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(averagePressure);
    tft.println("     ");
  }

  //--------------------------------------------------------------------------//
  //delay for sensors
  //--------------------------------------------------------------------------//
  userDelay(100);

  /*Serial.println("==================");
  Serial.print("adcValue: ");
  Serial.println(adcValue750);
  Serial.print("adcVolts: ");
  Serial.println(adcVolts750);
  Serial.print("airflow750CCM: ");                     
  Serial.println(airflow750CCM);
  Serial.println(" ");
  Serial.print("Bytes from register (decimal): ");
  Serial.print(a);
  Serial.print(" & ");
  Serial.println(b);
  Serial.print("digiOutCode: "); 
  Serial.println(digiOutCode);
  Serial.print("airflow50CCM: "); 
  Serial.println(airflow50CCM);
  Serial.println("");
  Serial.print("instAirflow: ");
  Serial.print(instAirflow);
  Serial.println("");
  Serial.print("instPressure: ");
  Serial.println(instPressure);*/
}

void optionSampleRecord(void)
{
  // read the button input pins:
  uint8_t sampleState = digitalRead(sampleButton);
  uint8_t recordState = digitalRead(recordButton);

  //save time of button falling edge inputs: 
  if((sampleStatePrev) && (!sampleState))
  {
    samplePress = millis();
  }
  if((recordStatePrev) && (!recordState))
  {
    recordPress = millis();
  }

  prevState = samp_nrec;

  //sample or record if button is low for 150ms:
  if((!recordState) && (millis() - recordPress > 100))
  {
    samp_nrec = 0;
    
    //reset data for sampling:
    sampleTime = 0;
    sampleNumber = 0;    
    readingsAirflow[5] = {0};
    totalAirflow = 0;
    averageAirflow = 0;
    readingsPressure[5] = {0};
    totalPressure = 0;
    averagePressure = 0;

    //reset minima and maxima:
    minAirflow = 0;
    maxAirflow = 0;
    minPressure = 0;
    maxPressure = 0;
  }
  if((!sampleState) && (millis() - samplePress > 150))
  {
    samp_nrec = 1;
  }

  // print out the state of the button:
  /*Serial.print(sampleStatePrev);
  Serial.print(", ");
  Serial.print(sampleState);
  Serial.print(", ");
  Serial.print(recordStatePrev);
  Serial.print(", ");
  Serial.print(recordState);
  Serial.print(", ");
  Serial.println(samp_nrec);*/
  
  sampleStatePrev = sampleState;
  recordStatePrev = recordState;
}

void record(void)
{ 
  if(instAirflow > maxAirflow)
  {
    maxAirflow = instAirflow;
  }
  
  if(instAirflow < minAirflow)
  {
    minAirflow = instAirflow;
  }

  if(instPressure > maxPressure)
  {
    maxPressure = instPressure;
  }
  
  if(instPressure < minPressure)
  {
    minPressure = instPressure;
  }

  /*Serial.print(minAirflow);
  Serial.print(" ,");
  Serial.print(maxAirflow);
  Serial.print(" ,");
  Serial.print(minPressure);
  Serial.print(" ,");
  Serial.println(maxPressure);
  userDelay(1000);*/
}

void sample(void)
{ 
  long unsigned current = millis();

  if(current - sampleTime >= 5)
  {
    sampleTime = millis();
    
    // subtract the last reading:
    totalAirflow = totalAirflow - readingsAirflow[sampleNumber];
    totalPressure = totalPressure - readingsPressure[sampleNumber];
    
    // read from the sensor:
    readingsAirflow[sampleNumber] = instAirflow;
    readingsPressure[sampleNumber] = instPressure;
    
    // add the reading to the total:
    totalAirflow = totalAirflow + readingsAirflow[sampleNumber];
    totalPressure = totalPressure + readingsPressure[sampleNumber];

    //get averages
    averageAirflow = totalAirflow/numReadings;
    averagePressure = totalPressure/numReadings;
    
    /*Serial.print(sampleNumber);
    Serial.print(", ");
    Serial.print(totalAirflow);
    Serial.print(", ");
    Serial.print(averageAirflow);
    Serial.print(", ");
    Serial.print(totalPressure);
    Serial.print(", ");
    Serial.println(averagePressure);
    userDelay(1000);*/

    // advance to the next position in the array:
    ++sampleNumber;

    // if we're at the end of the array...
    if (sampleNumber >= numReadings) {
    // ...wrap around to the beginning:
      sampleNumber = 0;
    }
    
  }
}

int16_t readSensor (uint8_t selectPin) 
{
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE1)); // Set to 800kHz, MSB and MODE1
  digitalWrite(selectPin, LOW);       //pull Chipselect Pin to Low

  int inByte_1 = SPI.transfer(0x00);  // Read first Byte of Pressure
  int inByte_2 = SPI.transfer(0x00);  // Read second Byte of Pressure

  digitalWrite(selectPin, HIGH);      //pull Chipselect Pin to High
  SPI.endTransaction();               //end SPI Transaction
  int16_t pressure_dig = inByte_1 << 8 | inByte_2;
  return pressure_dig; //return digital Pressure Value
}

void userDelay(int period)
{
  unsigned long previousMillis = millis(); //time now
  while(millis() - previousMillis < period)
  {
    
  }
}

void getDataHAF50(byte *byte1, byte *byte2)
{
  //digital flow sensor addr = 0x49
  uint8_t HAF50 = 0x49;
  
  //Move reister pointer back to first register
  Wire.beginTransmission(HAF50);
  Wire.write(0);
  Wire.endTransmission();

  //Send content of first two registers
  Wire.requestFrom(HAF50, 2);

  //Store data from registers
  *byte1 = Wire.read();
  *byte2 = Wire.read();

  //1ms response time
  userDelay(1);
}
  /*
  0_ _ _ _ _ 1
  |          |
  |          |
  |  screen  |
  |          |
  |          |
  3_ _ _ _ _ 2
 */
void testdrawtext(char *text, uint16_t color) {
  tft.setRotation(1);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}
