# ArcticTracker-ESP32

Arctic Tracker (v.3) is an APRS tracker platform based on the ESP32S3 MCU module, a GPS, a display and a 
VHF transceiver module. Hardware prototypes were created mainly as experimental prototypes to show how we can build a 
tracker using affordable modules. The Arctic Tracker is also a IoT device capable of using WIFI and the internet when this 
is available: For easy configuration, for pushing APRS data, etc. It can also function as a igate. 

It is based on the earlier Arctic Tracker (v.2) prototype which used an ESP32. This was again based on the even earlier 
Arctic Tracker (v.1) prototype which used a Teensy 3 MCU module and a ESP-8266 module (with NodeMCU). 

See http://www.hamlabs.no for some blogging about this project. 

## Supported hardware

* _LilyGo T-TWR-plus_ 2.0: See https://www.lilygo.cc/products/t-twr-plus?variant=42911934185653
* _Arctic Tracker hardware_: A working hardware prototype was built and demonstrated. More info on: http://hamlabs.no/2023/01/10/arctic_third_round/ A updated PCB layout has been produced. 5 trackers have been made and tested.

## Implemented features

This is the firmware. It is implemented in C and based on the ESP-IDF which again is based on FreeRTOS. 
It is fairly complete now. The following features are implemented:

* Command shell running on a serial port (USB). This allows settings of various parameters, using persistent storage (flash).
* Internetworking using WIFI. Automatically connect to access points available. User can set up 
  an ordered list of APs to try. It can also function as its own access point.
* Webserver/REST API.
* Interface with GPS for position and time. 
* OLED display, status screens and menu. Use button to operate.
* Sending APRS packets. Tracking, smart beaconing.
* Receiving APRS packets. 
* Add highly compressed earlier position reports to packets. This can improve trails significantly.
  See how this is done here: http://hamlabs.no/2020/11/02/improving-trails-with-arctic-tracker/. 
* Digipeater and igate. 
* Firmware upgrades over the air (OTA).
* Basic information on battery and charging.
* Track logging. Store positions in flash memory e.g. every 5 seconds and upload to a REST
  API on a Polaric Server when network is available. 

## REST API
A REST API is provided for external apps (typically). It mainly has methods for reading and updating settings. A strong authentication scheme based on SHA256 HMAC is used. A web-browser-based client is under development and the tracker support CORS to allow clients have origins other than the tracker itself. The tracker supports mDNS which allows discovering trackers (or at least finding their IP addresses) that are on the same LAN. This is still somewhat work-in-progress...

A web-client (it is work-in-progress) is here: https://github.com/Hamlabs/ArcticTracker-Webapp

## Future work

A version 4 tracker will probably move on to exploring LoRa and FSK modes on 70 cm. There are also some other VHF-modules around that can be interesting to look at without a separate PA module: The SR-FRS-2WVS (2 watts), the SA-868 (2 watts), maybe the SR-FRS-4W. The SA868 comes with a programmable version and I wonder if it could be somehow optimized a bit for APRS? 

I am open for ideas and contributions :)
 

