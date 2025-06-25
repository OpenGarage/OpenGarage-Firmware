## Firmware Compilation Instructions:

There are three methods to compile the firmware. You can choose any of them.

### Method 1: Use PlatformIO
Install Visual Studio Code (VS Code) (https://code.visualstudio.com/). Then launch VS Code and install the **platformio** extension. Once the extension is installed, open this `OpenGarage` folder. A `platformio.ini` file is provided in the folder, which defines all libraries and settings required to compile the firmware. Click `PlatformIO:Build` at the bottom of the screen to build the firwmare.

### Method 2: Use makeEspArduino and Makefile

#### Step A: Install ESP8266 core and required libraries
Follow https://github.com/plerup/makeEspArduino to install **ESP8266 core 3.1.2**. Then download or `git clone` the following libraries to a folder such as `$(HOME)/Arduino/libraries` (note the version of each library):

* Blynk library for Arduino (https://github.com/blynkkk/blynk-library) (v1.0.1)
* MQTT PubSUbClient https://github.com/knolleary/pubsubclient (v2.8)
* OneWire library: https://www.pjrc.com/teensy/td_libs_OneWire.html
* DallasTemperature library: https://github.com/milesburton/Arduino-Temperature-Control-Library/releases (v3.9.1)
* DHTesp library: https://github.com/beegee-tokyo/DHTesp/releases (v1.18)
* OpenThingsFramework library: https://github.com/OpenThingsIO/OpenThings-Framework-Firmware-Library
* WebSocket library: https://github.com/Links2004/arduinoWebSockets/releases (v2.3.5)
* This (OpenGarage) source code (download the repository and extract the `OpenGarage` folder to your Arduino's `libraries` folder.)

#### Step B: Use the Makefile

To compile the firmware code using makeESPArduino, simply run `make` in command line. Before doing so, you probaby need to open `Makefile` and modify some path variables therein to match where you installed the `esp8266` folder.


### Method 3: Use the Arduino IDE

#### Step A: Install Arduino IDE, ESP8266 core, and required libraries

* Install Arduino IDE (https://arduino.cc).
* Launch Arduino IDE and install ESP8266 core 1.3.2 for Arduino (https://github.com/esp8266/Arduino/releases/tag/3.1.2)
* Install the libraries listed above.
* Download this repository and extract the `OpenGarage` folder to your Arduino's `libraries` folder.

#### Step B: Compile
To compile using Arduino: launch Arduino, and select

* File -> Examples -> OpenGarage -> mainArduino.
* Tools -> Board -> Generic ESP8266 Module (if this is not available, check if you've installed the ESP8266 core).
* Tools -> Flash Mode -> DIO.
* Tools -> Flash Size -> **4M (1M SPIFFS)**.

Press Ctrl + R to compile. The compiled firmware (named mainArduino.cpp.bin) is by default copied to a temporary folder.

### Making changes to the firmware
If you want to customize the firmware, feel free to modify any part of the source code. The firmware supports built-in web interface. The HTML data are embedded as program memory strings. Go to the `html` subfolder, which contains the original HTML files, and a `html2raw` tool to convert these HTML files into program memory strings. If you make changes to the HTML files, re-run `html2raw` to re-generate the program memory strings. You can also directly modify the program memory strings, but keeping the original HTML files makes it easy to check and verify your HTML files in a browser.


