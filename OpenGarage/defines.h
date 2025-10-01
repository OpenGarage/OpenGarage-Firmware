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
#define OG_FWV     124 // Firmware version: 124 means 1.2.4

/** GPIO pins */
#define PIN_RELAY  15 // NodeMCU D8. Relay for triggering door action (for non-Security+).
#define PIN_BUTTON  0
#define PIN_TRIG   12 // NodeMCU D6. Distance sensor trigger.
#define PIN_ECHO   14 // NodeMCU D5. Distance sensor echo.
#define PIN_LED     2
#define PIN_SW_TX  15 // NodeMCU D8. Software serial output (for Security+).
#define PIN_SW_RX   5 // NodeMCU D1. Software serial input (for Security+).
#define PIN_BUZZER 13 // NodeMCU D7. Buzzer.
#define PIN_SWITCH  4 // NodeMCU D2. Optional switch sensor.
#define PIN_TH      4 // NodeMCU D2. Optional temperature sensor.
#define PIN_SWRX_DETECT 10 // Hardware version detect (if software rx exists)

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

#define STRING_RESERVE_SIZE 400

enum { // SN1 is built-in ultrasonic sensor
	OG_SN1_CEILING = 0,
	OG_SN1_SIDE,
};

enum { // SN2 is optional switch sensor
	OG_SN2_NONE = 0,
	OG_SN2_NC, // NC: normally closed
	OG_SN2_NO, // NO: normally open
};

enum {
	OG_SNO_1ONLY = 0, // use SN1 only
	OG_SNO_2ONLY, // use SN2 only
	OG_SNO_AND,   // SN1 AND SN2
	OG_SNO_OR,    // SN1 OR SN2
};

enum { // sensor filter
	OG_SFI_MEDIAN = 0, // median method
	OG_SFI_CONSENSUS,  // concensus method
};

enum { // vehicle status
	OG_VEH_ABSENT = 0,
	OG_VEH_PRESENT,
	OG_VEH_UNKNOWN,
	OG_VEH_NOTAVAIL,
};

enum { // sound alarm setting
	OG_ALM_NONE = 0,
	OG_ALM_5,
	OG_ALM_10,
};

enum { // temperature/humidity sensor
	OG_TSN_NONE = 0,
	OG_TSN_AM2320_RETIRED, // AM2320 is no longer supported due to pin 5 used for swrx
	OG_TSN_DHT11,
	OG_TSN_DHT22,
	OG_TSN_DS18B20,
};

enum {
	OG_MOD_AP = 0xA9,
	OG_MOD_STA = 0x2A,
};

enum { // automation actions
	OG_AUTO_NONE   = 0,
	OG_AUTO_NOTIFY = 1, // send notification
	OG_AUTO_CLOSE  = 2, // auto close
};

enum { // automation notification options
	OG_NOTIFY_NONE = 0x00,
	OG_NOTIFY_DO   = 0x01, // door open
	OG_NOTIFY_DC   = 0x02, // door close
	OG_NOTIFY_DS   = 0x04, // door stop
	OG_NOTIFY_VL   = 0x08,
	OG_NOTIFY_VA   = 0x10,
};

enum {
	OG_STATE_INITIAL = 0,
	OG_STATE_CONNECTING,
	OG_STATE_CONNECTED,
	OG_STATE_TRY_CONNECT,
	OG_STATE_WAIT_RESTART,
	OG_STATE_RESET = 9,
};

#define OG_LIGHT_BLINK_FOREVER  0
#define OG_LIGHT_BLINK_MAX      99	// limited by rcnt to 99
#define OG_LIGHT_BLINK_TIME     1
#define OG_LIGHT_BLINK_NOTIFY   2000 // specifies how long the last blink should last before blinking turns off

enum { // cloud settings
	CLOUD_NONE = 0,
	CLOUD_BLYNK,
	CLOUD_OTC,
};

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

// door status
enum {
	DOOR_STATUS_CLOSED = 0,
	DOOR_STATUS_OPEN,
	DOOR_STATUS_STOPPED,
	DOOR_STATUS_CLOSING,
	DOOR_STATUS_OPENING,
	DOOR_STATUS_UNKNOWN,
};

// door events
#define DOOR_STATUS_HIST_K        4
enum {
	DOOR_EVENT_REMAIN_CLOSED = 0,
	DOOR_EVENT_REMAIN_OPEN,
	DOOR_EVENT_JUST_OPENED,
	DOOR_EVENT_JUST_CLOSED,
	DOOR_EVENT_NONE,
	DOOR_EVENT_REMAIN_STOPPED,
	DOOR_EVENT_JUST_STOPPED,
	DOOR_EVENT_STILL_OPENING,
	DOOR_EVENT_START_OPENING,
	DOOR_EVENT_STILL_CLOSING,
	DOOR_EVENT_START_CLOSING,
};

// door actions
enum {
	DOOR_ACTION_TOGGLE = 0,
	DOOR_ACTION_CLOSE,
	DOOR_ACTION_OPEN
};

typedef enum {
	OPTION_FWV = 0, // firmware version
	OPTION_SN1,     // distance sensor mounting method
	OPTION_SN2,     // switch sensor type
	OPTION_SNO,     // sensor logic
	OPTION_SECV,    // security+ version (2.0, 1.0, or none)
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

//#define SERIAL_DEBUG
/** Serial debug functions */
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
