SKETCH = ./ogMainArduino.ino
LIBS = $(ESP_LIBS)/Wire \
$(ESP_LIBS)/ESP8266WiFi \
$(ESP_LIBS)/ESP8266mDNS \
$(ESP_LIBS)/DNSServer \
$(ESP_LIBS)/ESP8266WebServer \
$(ESP_LIBS)/ESP8266HTTPClient \
$(ESP_LIBS)/Ticker \
$(ESP_LIBS)/Hash \
$(HOME)/Arduino/libraries/Blynk \
$(HOME)/Arduino/libraries/pubsubclient \
$(HOME)/Arduino/libraries/OneWire \
$(HOME)/Arduino/libraries/DHTesp \
$(HOME)/Arduino/libraries/DallasTemperature \
$(HOME)/Arduino/libraries/OTF-Controller-Library \
$(HOME)/Arduino/libraries/WebSockets \
.

ESP_ROOT = $(HOME)/esp8266_2.7.4/
BUILD_ROOT = /tmp/$(MAIN_NAME)

UPLOAD_SPEED = 460800
UPLOAD_VERB = -v
#UPLOAD_RESET = nodemcu

FLASH_DEF = 4M1M
FLASH_MODE = dio
FLASH_SPEED = 40 
F_CPU = 80000000L

BOARD = generic

include ./makeEspArduino.mk
