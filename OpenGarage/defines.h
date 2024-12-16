/* OpenGarage Firmware
 *
 * OpenGarage macro defines and hardware pin assignments
 * Mar 2016 @ OpenGarage.io
 *
 * This file is part of the OpenGarage library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _DEFINES_H
#define _DEFINES_H

/** Firmware version, hardware version, and maximal values */
#define OG_FWV     123 // Firmware version: 123 means 1.2.3

/** GPIO pins */
#define PIN_RELAY  15 // D8 on nodemcu
#define PIN_BUTTON  0
#define PIN_TRIG   12 // D6 on nodemcu
#define PIN_ECHO   14 // D5 on nodemcu
#define PIN_LED     2
#define PIN_RESET  16
#define PIN_BUZZER 13 // D7 on nodemcu
#define PIN_SWITCH  4 // switch sensor: D2 on nodemcu
#define PIN_TH      5 // temperature sensor: D1 on nodemcu

// Default device name
#define DEFAULT_NAME    "My OpenGarage"
// Default device key
#define DEFAULT_DKEY    "opendoor"
// File System
#define FILESYS         SPIFFS
// Config file name
#define CONFIG_FNAME    "/config.dat"
// Log file name
#define LOG_FNAME       "/log2.dat"

#define DEFAULT_NTP1    "time.google.com"
#define DEFAULT_NTP2    "time.cloudflare.com"
#define DEFAULT_NTP3    "time.windows.com"

#define DEFAULT_BLYNK_DMN  "blynk.openthings.io"
#define DEFAULT_BLYNK_PRT  8080
#define DEFAULT_OTC_DMN    "ws.cloud.openthings.io"
#define DEFAULT_OTC_PRT    80

#define DEFAULT_SMTP_SERVER "smtp.gmail.com"
#define DEFAULT_SMTP_PORT   465

#define OG_SN1_CEILING  0x00 // SN1 is built-in ultrasonic sensor
#define OG_SN1_SIDE     0x01

#define OG_SN2_NONE     0x00 // SN2 is optional switch sensor
#define OG_SN2_NC       0x01 // NC: normally closed
#define OG_SN2_NO       0x02 // NO: normally open

#define OG_SNO_1ONLY    0x00 // use SN1 only
#define OG_SNO_2ONLY    0x01 // use SN2 only
#define OG_SNO_AND      0x02 // SN1 AND SN2
#define OG_SNO_OR       0x03 // SN1 OR SN2

#define OG_SFI_MEDIAN    0x00 // sensor filter: median method
#define OG_SFI_CONSENSUS 0x01 // concensus method

#define OG_VEH_ABSENT   0x00
#define OG_VEH_PRESENT  0x01
#define OG_VEH_UNKNOWN  0x02
#define OG_VEH_NOTAVAIL 0x03

#define OG_ALM_NONE     0x00
#define OG_ALM_5        0x01
#define OG_ALM_10       0x02

#define OG_TSN_NONE     0x00
#define OG_TSN_AM2320   0x01
#define OG_TSN_DHT11    0x02
#define OG_TSN_DHT22    0x03
#define OG_TSN_DS18B20  0x04

#define OG_MOD_AP       0xA9
#define OG_MOD_STA      0x2A

#define OG_AUTO_NONE    0x00
#define OG_AUTO_NOTIFY  0x01
#define OG_AUTO_CLOSE   0x02

//Automation Option C - Notify settings
#define OG_NOTIFY_NONE  0x00
#define OG_NOTIFY_DO    0x01
#define OG_NOTIFY_DC    0x02
#define OG_NOTIFY_VL    0x04
#define OG_NOTIFY_VA    0x08

#define OG_STATE_INITIAL        0
#define OG_STATE_CONNECTING     1
#define OG_STATE_CONNECTED      2
#define OG_STATE_TRY_CONNECT    3
#define OG_STATE_WAIT_RESTART   4
#define OG_STATE_RESET          9

#define OG_LIGHT_BLINK_FOREVER  0
#define OG_LIGHT_BLINK_MAX      99	// limited by rcnt to 99
#define OG_LIGHT_BLINK_TIME		25
#define OG_LIGHT_BLINK_NOTIFY	2000 // specifies how long the last blink should last before blinking turns off

#define CLOUD_NONE  0
#define CLOUD_BLYNK 1
#define CLOUD_OTC   2

#define BLYNK_PIN_DOOR  V0
#define BLYNK_PIN_RELAY V1
#define BLYNK_PIN_LCD   V2
#define BLYNK_PIN_DIST  V3
#define BLYNK_PIN_CAR   V4
#define BLYNK_PIN_IP    V5
#define BLYNK_PIN_TEMP  V6
#define BLYNK_PIN_HUMID V7

#define DEFAULT_LOG_SIZE  100
#define MAX_LOG_SIZE      500
#define ALARM_FREQ       1000
// door status histogram
// number of values (maximum is 8)
#define DOOR_STATUS_HIST_K        4
#define DOOR_STATUS_REMAIN_CLOSED 0
#define DOOR_STATUS_REMAIN_OPEN   1
#define DOOR_STATUS_JUST_OPENED   2
#define DOOR_STATUS_JUST_CLOSED   3
#define DOOR_STATUS_MIXED         4

#define CLD_NONE  0
#define CLD_BLYNK 1
#define CLD_OTC   2

typedef enum {
	OPTION_FWV = 0, // firmware version
	OPTION_SN1,     // distance sensor mounting method
	OPTION_SN2,     // switch sensor type
	OPTION_SNO,     // sensor logic
	OPTION_DTH,     // distance threshold for door
	OPTION_VTH,     // distance threshold for vehicle
	OPTION_RIV,     // status check interval
	OPTION_ALM,     // alarm mode
	OPTION_AOO,     // no alarm on opening
	OPTION_LSZ,     // log size
	OPTION_TSN,     // temperature sensor type
	OPTION_HTP,     // http port
	OPTION_CDT,     // click delay time
	OPTION_DRI,     // distance sensor reading interval
	OPTION_SFI,     // sensor filter method
	OPTION_CMR,     // consensus method margin
	OPTION_STO,     // sensor timeout option
	OPTION_MOD,     // mode
	OPTION_ATI,     // automation interval (in minutes)
	OPTION_ATO,     // automation options
	OPTION_ATIB,    // automation interval B (in hours)
	OPTION_ATOB,    // automation options B
	OPTION_NOTO,    // notification options
	OPTION_BAS,     // blink count before turning the light off (0 means infinity)
	OPTION_USI,     // use static IP
	OPTION_SSID,    // wifi ssid
	OPTION_PASS,    // wifi password
	OPTION_CLD,     // Cloud connection (0: no; 1: Blynk: 2: OTC)
	OPTION_AUTH,    // Cloud authentication token
	OPTION_BDMN,    // Cloud server (for backward compatibility, it's named bdmn)
	OPTION_BPRT,    // Cloud port (for backward compatibility, it's named bprt)
	OPTION_DKEY,    // device key
	OPTION_NAME,    // device name
	OPTION_IFTT,    // IFTTT token
	OPTION_MQEN,    // MQTT enable
	OPTION_MQTT,    // MQTT server
	OPTION_MQPT,    // MQTT port
	OPTION_MQUR,    // MQTT user name (optional)
	OPTION_MQPW,    // MQTT password (optional)
	OPTION_MQTP,    // MQTT topic (optional)
	OPTION_EMEN,	// Email enable
	OPTION_SMTP,	// SMTP Server
	OPTION_SPRT,	// SMTP Port
	OPTION_SEND,	// Sender Email
	OPTION_APWD,	// SMTP App Password
	OPTION_RECP,	// Recipient Email
	OPTION_DVIP,    // device IP
	OPTION_GWIP,    // gateway IP
	OPTION_SUBN,    // subnet
	OPTION_DNS1,    // dns1 IP
	OPTION_NTP1,    // custom NTP server
	OPTION_HOST,    // custom host name
	NUM_OPTIONS     // number of options
} OG_OPTION_enum;

// if button is pressed for 1 seconds, report IP
#define BUTTON_REPORTIP_TIMEOUT 800
// if button is pressed for at least 5 seconds, reset to AP mode
#define BUTTON_APRESET_TIMEOUT  4500
// if button is pressed for at least 10 seconds, factory reset
#define BUTTON_FACRESET_TIMEOUT 9500

#define LED_FAST_BLINK  100
#define LED_SLOW_BLINK  500

#define TIME_SYNC_TIMEOUT 3600 //Issues connecting to MQTT can throw off the time function, sync more often

#define TMP_BUFFER_SIZE 100

/** Serial debug functions */
//#define SERIAL_DEBUG

#if defined(SERIAL_DEBUG)
	#define DEBUG_PRINT(x)   Serial.print(x)
	#define DEBUG_PRINTLN(x) Serial.println(x)
#else
	#define DEBUG_PRINT(x)   {}
	#define DEBUG_PRINTLN(x) {}
#endif

typedef unsigned char byte;
typedef unsigned long ulong;
typedef unsigned int  uint;

#endif  // _DEFINES_H
