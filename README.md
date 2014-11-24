ArduinoXBeeToWifi
=================

Arduino based XBee IO to WiFi Rest converter

This project used an Arduino Mega for its multiple serial ports.<br>
Serial is used for the PC monitoring and upload<br>
Serial2 is connected to an XBee configured for API mode and is a controller to receive messages from remote XBee devices<br>
Serial3 is connected to an ESP8266 serial to WiFi module

API IO packets are received from the XBee, parsed, then coverted to events that are sent to an openHAB server via Rest calls

