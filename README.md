# ArcticTracker-ESP32



Arctic Tracker (v.3) is an APRS tracker platform based on the ESP32S3
MCU module, a GPS, a display and a VHF transceiver module. 
It is also a IoT device capable of using WIFI and the 
internet when this is available: For easy configuration, for
pushing APRS data, etc. It can also function as a igate. 

It is based on the earlier Arctic Tracker (v.2) prototype which 
used an ESP23. This was based on the even earlier Arctic Tracker 
(v.1) prototype which used a Teensy 3 MCU module and a ESP-8266 
module (with NodeMCU). 

See http://www.hamlabs.no for some blogging about this project. 
A working prototype was built and demonstrated. The last arcticle describes what is done with version 3. It is now implemented: http://hamlabs.no/2023/01/10/arctic_third_round/

## Implementation status

It is implemented in C and based on the ESP-IDF which 
again is based on FreeRTOS. It is fairly complete now. The following is implemented :

* Command shell running on a serial port (USB). This allows settings of various parameters, using persistent storage (flash).
* Webserver/REST API.
* Internetworking using WIFI. Automatically connect to access points available. User can set up 
  an ordered list of APs to try. It can also function as its own access point. 
* Interface with GPS for position and time. 
* OLED display, status screens and menu. Use button to operate.
* Controlling radio.
* Sending APRS packets. Tracking, smart beaconing.
* Receiving APRS packets. 
* Add highly compressed earlier position reports to packets. This can improve trails significantly.
  See how this is done here: http://hamlabs.no/2020/11/02/improving-trails-with-arctic-tracker/. 
* Digipeater and igate. 
* Firmware upgrades over the air (OTA).
* Track logging. Store positions in flash memory e.g. every 5 seconds and upload to a REST
  API on a Polaric Server when network is available. 

## REST API
A REST API has replaced the old web-server. It mainly has methods for reading and updating settings. A strong authentication scheme based on SHA256 HMAC is used. A web-browser-based client is under development and the tracker support CORS to allow clients have origins other than the tracker itself. The tracker supports mDNS which allows discovering trackers (or at least finding their IP addresses) on the LAN. 

The web-client is here: https://github.com/Hamlabs/ArcticTracker-Webapp

## Hardware

A updated PCB layout has been produced and is currently being tested. 5 trackers have been made. It works! 

## Future work

A version 4 tracker will probably abandon APRS (AFSK) completely and move on to exploring 
LoRa and FSK modes on 70 cm. 


