#!/bin/bash
arduino-cli core install esp8266:esp8266
arduino-cli lib install NTPClient ESP8266-ping RTClib 
arduino-cli lib uninstall TinyWireM

cd ~/Arduino/libraries/
git clone git@github.com:adafruit/Adafruit_SSD1306.git
 
