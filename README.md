
This folder contains firmware source code and documentation for OpenGarage. For details, visit [http://opengarage.io](http://opengarage.io)

For Firmware release notes, go to [https://github.com/OpenGarage/OpenGarage-Firmware/releases](https://github.com/OpenGarage/OpenGarage-Firmware/releases)

<img src="Screenshots/1_og_ap.jpg" height=350> <img src="Screenshots/2_og_home.jpg" height=350> <img src="Screenshots/3_og_log.jpg" height=350> <img src="Screenshots/4_og_options_basic.jpg" height=350><img src="Screenshots/5_og_options_integration.jpg" height=350> <img src="Screenshots/6_og_options_advanced.jpg" height=350> <img src="Screenshots/7_og_update.jpg" height=350> <img src="Screenshots/8_og_blynk_app.jpg" height=350>


### Firmware Compilation Instructions:

#### Requirement:

* Arduino (https://arduino.cc) with ESP8266 core 2.7.4 for Arduino (https://github.com/esp8266/Arduino/releases/tag/2.7.4)
* Instead of installing Arduino, you can also directly use make (this folder includes a copy of **makeESPArduino**: https://github.com/plerup/makeEspArduino)
* Blynk library for Arduino (https://github.com/blynkkk/blynk-library) (version 0.61)
* MQTT PubSUbClient https://github.com/knolleary/pubsubclient (version 2.8)
* AM2320 library: https://github.com/hibikiledo/AM2320/releases
* OneWire library: https://www.pjrc.com/teensy/td_libs_OneWire.html
* DallasTemperature library: https://github.com/milesburton/Arduino-Temperature-Control-Library/releases
* DHTesp library: https://github.com/beegee-tokyo/DHTesp/releases

* This (OpenGarage) source code

#### Setting up a Dev Environment

* Install Arduino with ESP8266 core 2.7.4 (or use make directly, following https://github.com/plerup/makeEspArduino).
* Install Arduino libraries referenced above.
* Download this repository and extract the OpenGarage library to your Arduino's `libraries` folder.

#### Compilation

To compile the firmware code using makeESPArduino, simply run `make` in command line. You may need to open `Makefile` and modify some path variables therein to match where you installed the `esp8266` folder.

To compile using Arduino: launch Arduino, and select

* File -> Examples -> OpenGarage -> mainArduino.
* Tools -> Board -> Generic ESP8266 Module (if this is not available, check if you've installed the ESP8266 core).
* Tools -> Flash Mode -> DIO.
* Tools -> Flash Size -> 2M (1M SPIFFS).

Press Ctrl + R to compile. The compiled firmware (named mainArduino.cpp.bin) is by default copied to a temporary folder.

The firmware supports built-in web interface. The HTML data are embedded as program memory strings. Go to the `html` subfolder, which contains the original HTML files, and a `html2raw` tool to convert these HTML files into program memory strings. If you make changes to the HTML files, re-run `html2raw` to re-generate the program memory strings. You can also directly modify the program memory strings, but keeping the original HTML files makes it easy to check and verify your HTML files in a browser.


#### Uploading

As OpenGarage firmware supports OTA (over-the-air) update, you can upload the firmware through the web interface. At the homepage, find the **Update** button and follow that to upload a new firmware. If your OpenGarage is in AP mode and not connected to any WiFi network yet, you can open http://192.168.4.1/update and that's the same interface.

#### Firmware User Manual and API

Go to the `doc` folder to find the user manual as well as the API document for each firmware version.


