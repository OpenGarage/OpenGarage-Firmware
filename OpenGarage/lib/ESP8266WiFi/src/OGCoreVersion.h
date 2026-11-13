#pragma once
#include <core_version.h>
#if !defined(ARDUINO_ESP8266_MAJOR) || !defined(ARDUINO_ESP8266_MINOR) || !defined(ARDUINO_ESP8266_REVISION)
#error "OG vendored ESP8266WiFi requires identifiable ESP8266 Arduino core 3.1.2"
#elif ARDUINO_ESP8266_MAJOR != 3 || ARDUINO_ESP8266_MINOR != 1 || ARDUINO_ESP8266_REVISION != 2
#error "OG vendored ESP8266WiFi is pinned to ESP8266 Arduino core 3.1.2; review and rebase the fork before upgrading"
#endif
