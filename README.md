![LilyGo tracker](t_twr.jpg)
# ArcticTracker-ESP32

Arctic Tracker (v.3) is an APRS tracker platform based on the ESP32S3 MCU module, a GPS, a display and a 
VHF transceiver module. Hardware prototypes were created mainly as experimental prototypes to show how we can build a 
tracker using affordable modules. The Arctic Tracker is also a IoT device capable of using WIFI and the internet when this 
is available: For easy configuration, for pushing APRS data, etc. It can also function as a igate. 

It is based on the earlier Arctic Tracker (v.2) prototype which used an ESP32. This was again based on the even earlier 
Arctic Tracker (v.1) prototype which used a Teensy 3 MCU module and a ESP-8266 module (with NodeMCU). 

See http://www.hamlabs.no for some blogging about this project and on the [port to the T-TWR](http://hamlabs.no/2024/03/22/arctic-tracker-software-on-lilygo-t-twr-plus/) in particular.

## Supported hardware

* [_LilyGo T-TWR-plus 2.0_](https://www.lilygo.cc/products/t-twr-plus?variant=42911934185653). Put a wire between the SQL output of the radio module and the IO15 pin (and maybe also a 1nF capacitor between IO15 and GND to suppress RF pickup). 
* [_Arctic Tracker hardware_](http://hamlabs.no/2023/01/10/arctic_third_round/): A working hardware prototype was built and demonstrated. A updated PCB layout has been produced. 5 trackers have been made and tested.

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
  See [how this is done here](http://hamlabs.no/2020/11/02/improving-trails-with-arctic-tracker/). 
* Digipeater and igate. 
* Firmware upgrades over the air (OTA).
* Basic information on battery and charging.
* Track logging. Store positions in flash memory e.g. every 5 seconds and upload to a REST
  API on a Polaric Server when network is available. 

## REST API and security
A REST API is provided for external apps (typically). It mainly has methods for reading and updating settings. A strong authentication scheme based on SHA256 HMAC is used. A web-browser-based client is under development and the tracker support CORS to allow clients have origins other than the tracker itself. The tracker supports mDNS which allows discovering trackers (or at least finding their IP addresses) that are on the same LAN. This is still somewhat work-in-progress...

A [web-client is here](https://github.com/Hamlabs/ArcticTracker-Webapp): This is also contained in the tracker itself to allow configuration using the softAP mode. A smartphone app is under way. 

The webserver uses HTTPS (SSL/TLS). In the current version, a self-signed certificate is embedded in the firmware. This means that you will need to accept an exception for this certificate in the browser the first time you access the tracker. Also, if the private key is embedded into published firmware code, it can (for skilled hackers) be exposed. This doesn't affect the authentication scheme though. For stronger security consider building your firmware yourself and be sure to create a new certificate. A certificate signed by a CA could be used. 

## Building the firmware
It can be built with *esp-idf* (version 5.0.x) and the *idf.py* tool. Follow the instructions to install the *esp-idf* and run the necessary scripts there first to set it up. Download the *Arctic Tracker* repository in another directory. cd to this directory and run the following commands to add external components.: 
  ```
  idf.py add-dependency "espressif/mdns^1.2.4" 
  idf.py add-dependency "espressif/led_strip^2.5.3" 
  ```
The *led_strip* component is for the LilyGo T-TWR plus (neopixel LED). For this device you will also need to download *XPowersLib* and edit the EXTRA_COMPONENT_DIRS setting in CMakeLists.txt (in the top level directory) to the location where you installed it.

It is a good idea to generate a new SSL certificate now and then. You could also just cd to the directory and run the command inside the gencert.sh script. You should have openssl installed on your computer to do this. 
  ```
  cd components/networking/cert; sh gencert.sh
  ```
You may start menuconfig and go to the *Arctic Tracker Config* and check if the right target device is selected. More settings are in *main/defines.h*.
  ```
  idf.py menuconfig
  ```
  
To build the firmware, run
  ```
  idf.py build
  ```
This will build everything. You may flash the firmware directly from idf.py this way. Note that this writes everything to the flash: Including the partition table, a bootloader and the webapp. 
  ```
  idf.py flash
  ```

## Flashing a binary
We intend to post pre-compiled binaries with each release and are also available [here](https://arctictracker.no/download/). The complicating factor is that there are more than one way to do it and that the firmware consists of multiple parts: The bootloader, the partition table, the webapp, etc.. The most flexible option is probably to use *esptool* or a similar program, but you will need to know some technical details. It is also possible to convert the binary to the UF2 format using *uf2conv* and use a uf2 bootloader.

Download the proper *ArcticTracker_xx.zip* file and unpack it in a directory. Go to that directory and use *esptool* or a similar tool (on Windows, we may use the *flash download tool*). The *flash_all.sh* script shows how to use *esptool*. The same parameters can be used in the Windows *flash download tool*. On a Linux system you may just run the *flash_all.sh* script to flash everything. 

You may choose to update only the app (ArcticTracker.bin) or the Webapp (webapp.bin) if you want and if the other parts are in place. Use the the addresses provided. 


## Setup of the tracker - the command shell
Plug a USB cable into the tracker and your computer. A serial interface will appear. Start a (serial) terminal program and connect to the serial interface (on Linux it is /dev/ttyACM0). Alternatively, the monitor command of the idf.py may be used. It may be necessary to reset the tracker to get the command prompt (cmd:). 

The command-shell let you configure everything and is useful in developing and debugging the software. Be sure to set the *callsign*. The *'help'* command shows the available commands. The *'tracker on'* command turns on the tracking. *'radio on'* command turns on the radio. The *'wifi on'* command turns on the WIFI. The *'ap'* command lets you set up a list of WIFI access points. The tracker will try to connect to these in order if they are in range. Also, before you try to use the webapp, use the *'api-key'* to set a secret key to be used for the webapp to authenticate. *'api-origins'* should be set to a regular expression matching the origins expected for the webapp. ".*" (dot star) will match all. If things are working as expected, you should to be able to get to the most important settings with a web-browser. 

The tracker is also able to function as its own access point (*'softap'* command). Info about ip-address, etc. is shown on the display so you can connect your browser to it. 

## Future work
When planning a version 4 tracker we consider moving on to exploring LoRa and FSK modes on 70 cm. However, in Norway, there are legal issues with spread-spectrum modes on HAM-radio on the 70cm band, so it may have to wait. There are also some other VHF-modules around that can be interesting to look at without a separate PA module: The SR-FRS-2WVS (2 watts), the SA-868 (2 watts) is used by the T-TWR device and is supported already. The SA868 comes with a programmable version and I wonder if it could be somehow optimized a bit for APRS? 

I am open for ideas and contributions :)
 

