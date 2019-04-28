# ArcticTracker-ESP32

Arctic Tracker (v.2) is an APRS tracker platform based on the ESP32
MCU module, a GPS, a display and a VHF transceiver module. 
It is also a IoT device capable of using WIFI and the 
internet when this is available: For easy configuration, for
pushing APRS data, etc. It can also function as a igate. 

It is based on the earlier Arctic Tracker (v.1) prototype which 
used a Teensy 3 MCU module and a ESP-8266 module (with NodeMCU). 

See http://www.hamlabs.no for some blogging about this project. 
A working prototype was built and demonstrated (except we didn't get 
the PA to work properly). The design with ESP-8266 had some 
limitations and complicating factors, and as the ESP-32 arrived 
on the scene it became clear that this would be a better platform. 

## Firmware implementation

It is implemented in C and based on the ESP-IDF which 
again is based on FreeRTOS. Webserver is based on libesphttpd: 
See https://github.com/chmorgan/libesphttpd

Currently the following is implemented or ported: 

* Command shell. 
* Settings of various parameters, using persistent storage (flash). 
* Internetworking using WIFI. Automatically connect to access points available. User can set up an ordered list of APs to try. It can also function as its own access point. 
* Webserver. Used for setup. 
* Interface with GPS. 
* LCD display, button handler, etc..
* Controlling radio and PA module
* Sending APRS packets
* Firmware upgrades over the air (OTA). 

A updated PCB layout has been produced and is currently being tested. It works! 




