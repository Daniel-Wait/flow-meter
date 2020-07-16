# Sinapi Flowmeter

This is part of a project issued by Sinapi Biomedical. The project objective was 
to design a flowmeter which uses sensors of varying ranges and accuracies.

The project is designed for a working system with the following components:
~ Arduino Uno MCU
~ Arduino Mega Proto Shield R3
~ Honeywell HSCDRRD002NDSA5 Pressure Sensor
~ Honeywell HAFBLF0050C4AX5 Digital Airflow Sensor - 50 SCCM range (order delayed)
~ Honeywell HAFBLF0750CAAX5 Analog Airflow Sensor - 750 SCCM range (order delayed)
~ Honeywell AWM5104VN Analog Airflow Sensor - 20 SLPM range
~ Pololu U3V70F12 12V Step-Up Voltage Regulator (for AWM5104VN)
~ Adafruit ADS1115 16-Bit ADC (for analog airflow sensors)
~ Adafruit LCD 1.8" TFT Display Breakout and Shield (order delayed)
~ Romoss PH80 Solo 6 5V Power Bank
~ 2 buttons to 'sample' or 'record' data

Listed below are the peripherals used and relevant components.
SPI:
~ LCD
~ Pressure Sensor

I2C:
~ ADC
~ Digital Airflow Sensor (incomplete)

GPIO:
~ Pressure sensor Chip Select
~ 'Sample' Button
~ 'Record' Button
 
For the final project, the AWN5104VN was removed. However, the 12V Voltage Regulator 
and connectors were maintained for expandability. The code for this analog sensor to 
function is commented but still availabe to the user.
