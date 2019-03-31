# Ceres Helper  version 4.4g 
The Ceres helper is a micro controller automating the measurement of soil moisture and temperature for use in efficient watering of your soil based life.

It uses an arduino MKR1000, a rs485 transmitter, 6 - rs485 moisture and temperature reading sensors, an I2C LCD display, A DHT22 air temp and humidity sensor to collect information, send it to (formerly)Carriots.com (now https://www.altairsmartcore.com) 

In order to get the Ceres Helper online, you will need to ADD your Wifi SSID and Wifi password in the "arduino_secrets.h" file, under the wifi settings
To allow the device to send data streams every hour and status streams every 10 minutes to (formerly)Carriots.com (now https://www.altairsmartcore.com), you will need to set up an account, add a device ID(name), retrieve the API key for the device and ADD your API key and Device ID(name) in the "arduino_secrets.h" file, under the API Key settings


