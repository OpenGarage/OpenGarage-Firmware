/* OpenGarage Firmware
 *
 * Main loop
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

#if defined(SERIAL_DEBUG)
  #define BLYNK_DEBUG
  #define BLYNK_PRINT Serial
#endif

#include <BlynkSimpleEsp8266.h>
#include <DNSServer.h>
#include <PubSubClient.h> //https://github.com/Imroy/pubsubclient

#include "pitches.h"
#include "OpenGarage.h"
#include "espconnect.h"

OpenGarage og;
ESP8266WebServer *server = NULL;
DNSServer *dns = NULL;

WidgetLED blynk_door(BLYNK_PIN_DOOR);
WidgetLED blynk_car(BLYNK_PIN_CAR);

static Ticker led_ticker;
static Ticker aux_ticker;
static Ticker ip_ticker;
static Ticker restart_ticker;

static WiFiClient wificlient;
PubSubClient mqttclient(wificlient);

static String scanned_ssids;
static byte read_cnt = 0;
static uint distance = 0;
static uint vdistance = 0;
static float tempC = 0;
static float humid = 0;
static byte door_status = 0; //0 down, 1 up
static int vehicle_status = 0; //0 No, 1 Yes, 2 Unknown (door open), 3 Option Disabled
static bool curr_cloud_access_en = false;
static uint led_blink_ms = LED_FAST_BLINK;
static ulong justopen_timestamp = 0;
static byte curr_mode;
// this is one byte storing the door status histogram
// maximum 8 bits
static byte door_status_hist = 0;
static ulong curr_utc_time = 0;
static ulong curr_utc_hour= 0;
static HTTPClient http;

void do_setup();

byte findKeyVal (const char *str, const char *key, char *strbuf=NULL, uint8_t maxlen=0) {
  uint8_t found=0;
  uint8_t i=0;
  const char *kp;
  kp=key;
  while(*str &&  *str!=' ' && *str!='\n' && found==0){
    if (*str == *kp){
      kp++;
      if (*kp == '\0'){
        str++;
        kp=key;
        if (*str == '='){
            found=1;
        }
      }
    } else {
      kp=key;
    }
    str++;
  }
  if(strbuf==NULL) return found; // if output buffer not provided, return right away

  if (found==1){
    // copy the value to a buffer and terminate it with '\0'
    while(*str &&  *str!=' ' && *str!='\n' && *str!='&' && i<maxlen-1){
      *strbuf=*str;
      i++;
      str++;
      strbuf++;
    }
    if (!(*str) ||  *str == ' ' || *str == '\n' || *str == '&') {
      *strbuf = '\0';
    } else {
      found = 0;  // Ignore partial values i.e. value length is larger than maxlen
      i = 0;
    }
  }
  return(i); // return the length of the value
}

void server_send_html_P(PGM_P content) {
  server->send_P(200, PSTR("text/html"), content);
  DEBUG_PRINT(strlen_P(content));
  DEBUG_PRINTLN(F(" bytes sent."));
}

void server_send_json(String json) {
  server->sendHeader("Access-Control-Allow-Origin", "*"); // from esp8266 2.4 this has to be sent explicitly
  server->send(200, "application/json", json);
}

void server_send_result(byte code, const char* item = NULL) {
  String json = F("{\"result\":");
  json += code;
  if (!item) item = "";
  json += F(",\"item\":\"");
  json += item;
  json += F("\"");
  json += F("}");
  server_send_json(json);
}

void server_send_result(const char* command, byte code, const char* item = NULL) {
  if(!command) server_send_result(code, item);
}

bool get_value_by_key(const char* key, uint& val) {
  if(server->hasArg(key)) {
    val = server->arg(key).toInt();   
    return true;
  } else {
    return false;
  }
}

bool get_value_by_key(const char* key, String& val) {
  if(server->hasArg(key)) {
    val = server->arg(key);   
    return true;
  } else {
    return false;
  }
}

bool findArg(const char *command, const char *name) {
  if(command) {
    return findKeyVal(command, name);
    // todo
  } else {
    return server->hasArg(name);
  }
}

char tmp_buffer[TMP_BUFFER_SIZE];

bool get_value_by_key(const char *command, const char *key, uint& val) {
  if(command) {
    byte ret = findKeyVal(command, key, tmp_buffer, TMP_BUFFER_SIZE);
    val = String(tmp_buffer).toInt();
    return ret;
  } else {
    return get_value_by_key(key, val);
  }
}

bool get_value_by_key(const char *command, const char *key, String& val) {
  if(command) {
    byte ret = findKeyVal(command, key, tmp_buffer, TMP_BUFFER_SIZE);
    val = String(tmp_buffer);
    return ret;
  } else {
    return get_value_by_key(key, val);
  }
}

String ipString;

void report_ip() {
  static uint notes[] = {NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4};
  static byte note = 0;
  static byte digit = 0;

  if(digit == ipString.length()) { // play ending note
    og.play_note(NOTE_C6); digit++; note=0;
    ip_ticker.once_ms(1000, report_ip);
    return;
  } else if(digit == ipString.length()+1) { // end
    og.play_note(0); note=0; digit=0;
    return;
  }
  char c = ipString.charAt(digit);
  if (c==' ') {
    og.play_note(0); digit++; note=0;
    ip_ticker.once_ms(1000, report_ip);
  } else if (c=='.') {
    og.play_note(NOTE_C5);
    digit++; note=0;
    ip_ticker.once_ms(500, report_ip);
  } else if (c>='0' && c<='9') {
    byte idx=9; // '0' maps to index 9;
    if(c>='1') idx=c-'1';
    if(note==idx+1) {
      og.play_note(0); note++;
      ip_ticker.once_ms(1000, report_ip);
    } else if(note==idx+2) {
      digit++; note=0;
      ip_ticker.once_ms(100, report_ip);
    } else {
      og.play_note(notes[note]);
      note++;
      ip_ticker.once_ms(500, report_ip);
    }
  }
}

void restart_in(uint32_t ms) {
  if(og.state != OG_STATE_WAIT_RESTART) {
    og.state = OG_STATE_WAIT_RESTART;
    DEBUG_PRINTLN(F("Prepare to restart..."));
    restart_ticker.once_ms(ms, og.restart);
  }
}

void on_home()
{
  if(curr_mode == OG_MOD_AP) {
    server_send_html_P(ap_home_html);
  } else {
    server_send_html_P(sta_home_html);
  }
}

void on_sta_view_options() {
  if(curr_mode == OG_MOD_AP) return;
  server_send_html_P(sta_options_html);
}

void on_sta_view_logs() {
  if(curr_mode == OG_MOD_AP) return;
  server_send_html_P(sta_logs_html);
}

char dec2hexchar(byte dec) {
  if(dec<10) return '0'+dec;
  else return 'A'+(dec-10);
}

String get_mac() {
  static String hex = "";
  if(!hex.length()) {
    byte mac[6];
    WiFi.macAddress(mac);

    for(byte i=0;i<6;i++) {
      hex += dec2hexchar((mac[i]>>4)&0x0F);
      hex += dec2hexchar(mac[i]&0x0F);
      if(i!=5) hex += ":";
    }
  }
  return hex;
}

String get_ap_ssid() {
  static String ap_ssid = "";
  if(!ap_ssid.length()) {
    byte mac[6];
    WiFi.macAddress(mac);
    ap_ssid = "OG_";
    for(byte i=3;i<6;i++) {
      ap_ssid += dec2hexchar((mac[i]>>4)&0x0F);
      ap_ssid += dec2hexchar(mac[i]&0x0F);
    }
  }
  return ap_ssid;
}

String get_ip() {
  String ip = "";
  IPAddress _ip = WiFi.localIP();
  ip = _ip[0];
  ip += ".";
  ip += _ip[1];
  ip += ".";
  ip += _ip[2];
  ip += ".";
  ip += _ip[3];
  return ip;
}

void sta_controller_fill_json(String& json) {
  json = "";
  json += F("{\"dist\":");
  json += distance;
  json += F(",\"door\":");
  json += door_status;
  json += F(",\"vehicle\":");
  json += vehicle_status;
  json += F(",\"rcnt\":");
  json += read_cnt;
  json += F(",\"fwv\":");
  json += og.options[OPTION_FWV].ival;
  json += F(",\"name\":\"");
  json += og.options[OPTION_NAME].sval;
  json += F("\",\"mac\":\"");
  json += get_mac();
  json += F("\",\"cid\":");
  json += ESP.getChipId();
  json += F(",\"rssi\":");
  json += (int16_t)WiFi.RSSI();
  if(og.options[OPTION_TSN].ival) {
  	json += F(",\"temp\":");
  	json += tempC;
  	json += F(",\"humid\":");
  	json += humid;
  }
  json += F("}");
}

void on_sta_controller() {
  if(curr_mode == OG_MOD_AP) return;
  String json;
  sta_controller_fill_json(json);
  server_send_json(json);
}

void on_sta_debug() {
  String json = "";
  json += F("{");
  json += F("\"rcnt\":");
  json += read_cnt;
  json += F(",\"fwv\":");
  json += og.options[OPTION_FWV].ival;
  json += F(",\"name\":\"");
  json += og.options[OPTION_NAME].sval;
  json += F("\",\"mac\":\"");
  json += get_mac();
  json += F("\",\"cid\":");
  json += ESP.getChipId();
  json += F(",\"rssi\":");
  json += (int16_t)WiFi.RSSI();
  json += F(",\"bssid\":\"");
  json += WiFi.BSSIDstr();
  json += F("\",\"build\":\"");
  json += (F(__DATE__));
  json += F("\",\"Freeheap\":");
  json += (uint16_t)ESP.getFreeHeap();
  json += F("}");
  server_send_json(json);
}

void sta_logs_fill_json(String& json) {
  json = "";
  json += F("{\"name\":\"");
  json += og.options[OPTION_NAME].sval;
  json += F("\",\"time\":");
  json += curr_utc_time;
  json += F(",\"logs\":[");
  if(!og.read_log_start()) {
    json += F("]}");
    server_send_json(json);
    return;
  }
  LogStruct l;
  for(uint i=0;i<og.options[OPTION_LSZ].ival;i++) {
    if(!og.read_log_next(l)) break;
    if(!l.tstamp) continue;
    json += F("[");
    json += l.tstamp;
    json += F(",");
    json += l.status;
    json += F(",");
    json += l.dist;
    json += F("],");
  }
  og.read_log_end();
  json.remove(json.length()-1); // remove the extra ,
  json += F("]}");
}

void on_sta_logs() {
  if(curr_mode == OG_MOD_AP) return;
  String json;
  sta_logs_fill_json(json);
  server_send_json(json);
}

bool verify_device_key() {
  if(server->hasArg("dkey") && (server->arg("dkey") == og.options[OPTION_DKEY].sval))
    return true;
  return false;
}

bool verify_device_key(const char* command) {
  if(command) return true;
  else return verify_device_key();
}

void on_reset_all(){
  if(!verify_device_key()) {
    server_send_result(HTML_UNAUTHORIZED);
    return;
  }

  og.state = OG_STATE_RESET;
  server_send_result(HTML_SUCCESS);
}

void on_clear_log() {
  if(!verify_device_key()) {
    server_send_result(HTML_UNAUTHORIZED);
    return;
  }
  og.log_reset();
  server_send_result(HTML_SUCCESS);  
}

void sta_change_controller_main(const char *command) {
  if(curr_mode == OG_MOD_AP) return;

  if(!verify_device_key(command)) {
    server_send_result(command, HTML_UNAUTHORIZED);
    return;
  }

  if(findArg(command, "click") || findArg(command, "close") || findArg(command, "open")) {
    DEBUG_PRINTLN(F("Received button request (click, close, or open)"));
    server_send_result(command, HTML_SUCCESS);
    //1 is open
    if ((findArg(command, "close") && door_status) ||
        (findArg(command, "open") && !door_status) ||
        (findArg(command, "click"))) {
      DEBUG_PRINTLN(F("Valid command recieved based on door status"));
      if(!og.options[OPTION_ALM].ival) {
        // if alarm is not enabled, trigger relay right away
        og.click_relay();
      } else if(og.options[OPTION_AOO].ival && !door_status) {
      	// if 'Do not alarm on open' is on, and door is about to be open, no alarm needed
      	og.click_relay();
      } else {
        // else, set alarm
        og.set_alarm();
      }
    }else{
      DEBUG_PRINTLN(F("Command request not valid, door already in requested state"));
    }
  } else if(findArg(command, "reboot")) {
    server_send_result(command, HTML_SUCCESS);
    //restart_ticker.once_ms(1000, og.restart);
    restart_in(1000);
  } else if(findArg(command, "apmode")) {
    server_send_result(command, HTML_SUCCESS);
    og.reset_to_ap();
  } else {
    server_send_result(command, HTML_NOT_PERMITTED);
  }

}

void on_sta_change_controller() {
  sta_change_controller_main(NULL);  
}

void sta_change_options_main(const char *command) {
  if(curr_mode == OG_MOD_AP) return;

  if(!verify_device_key(command)) {
    server_send_result(command, HTML_UNAUTHORIZED);
    return;
  }

  uint ival = 0;
  String sval;
  byte i;
  OptionStruct *o = og.options;
  
  byte usi = 0;
  // FIRST ROUND: check option validity
  // do not save option values yet
  for(i=0;i<NUM_OPTIONS;i++,o++) {
    const char *key = o->name.c_str();
    // these options cannot be modified here
    if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID1 ||
      i==OPTION_PASS1 || i==OPTION_SSID2 ||
      i==OPTION_PASS2 || i==OPTION_DKEY)
      continue;
    
    if(o->max) {  // integer options
      if(get_value_by_key(command, key, ival)) {
        if(ival>o->max) {	// check max limit
          server_send_result(command, HTML_DATA_OUTOFBOUND, key);
          return;
        }
        // check min limit
        if(i==OPTION_DRI && ival < 50) {
        	server_send_result(command,HTML_DATA_OUTOFBOUND, key);
        	return;
        }
        if(i==OPTION_LSZ && ival < 20) {
          // minimal log size is 20
          server_send_result(command, HTML_DATA_OUTOFBOUND, key);
          return;
        }
        if(i==OPTION_CDT && ival < 50) {
          // click delay time should be at least 50 ms
          server_send_result(command, HTML_DATA_OUTOFBOUND, key);
          return;
        }
        if(i==OPTION_USI && ival==1) {
          // mark device IP and gateway IP change
          usi = 1;
        }
      }
    }
  }
  
  // Check device IP and gateway IP changes
  String dvip, gwip, subn, dns1;
  const char* _dvip = "dvip";
  const char* _gwip = "gwip";
  const char* _subn = "subn";
  const char* _dns1 = "dns1";
  if(usi) {
    if(get_value_by_key(command, _dvip, dvip)) {
      if(get_value_by_key(command, _gwip, gwip)) {
        // check validity of IP address
        IPAddress ip;
        if(!ip.fromString(dvip)) {server_send_result(command, HTML_DATA_FORMATERROR, _dvip); return;}
        if(!ip.fromString(gwip)) {server_send_result(command, HTML_DATA_FORMATERROR, _gwip); return;}
        if(get_value_by_key(command, _subn, subn)) {
          if(!ip.fromString(subn)) {
            server_send_result(command, HTML_DATA_FORMATERROR, _subn);
            return;
          }
        }
        if(get_value_by_key(command, _dns1, dns1)) {
        	if(!ip.fromString(dns1)) {
            server_send_result(command, HTML_DATA_FORMATERROR, _dns1);
            return;
        	}
        }
      } else {
        server_send_result(command, HTML_DATA_MISSING, _gwip);
        return;
      }              
    } else {
      server_send_result(command, HTML_DATA_MISSING, _dvip);
      return;
    }
  }
  // Check device key change
  String nkey, ckey;
  const char* _nkey = "nkey";
  const char* _ckey = "ckey";
  
  if(get_value_by_key(command, _nkey, nkey)) {
    if(get_value_by_key(command, _ckey, ckey)) {
      if(!nkey.equals(ckey)) {
        server_send_result(command, HTML_MISMATCH, _ckey);
        return;
      }
    } else {
      server_send_result(command, HTML_DATA_MISSING, _ckey);
      return;
    }
  }
  
  // SECOND ROUND: change option values
  o = og.options;
  for(i=0;i<NUM_OPTIONS;i++,o++) {
    const char *key = o->name.c_str();
    // these options cannot be modified here
    if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID1 ||
      i==OPTION_PASS1 || i==OPTION_SSID2 ||
      i==OPTION_PASS2 || i==OPTION_DKEY)
      continue;
    
    if(o->max) {  // integer options
      if(get_value_by_key(command, key, ival)) {
        o->ival = ival;
      }
    } else {
      if(get_value_by_key(command, key, sval)) {
        o->sval = sval;
      }
    }
  }

  if(usi) {
    get_value_by_key(command, _dvip, dvip);
    get_value_by_key(command, _gwip, gwip);
    og.options[OPTION_DVIP].sval = dvip;
    og.options[OPTION_GWIP].sval = gwip;
    if(get_value_by_key(command, _subn, subn)) {
      og.options[OPTION_SUBN].sval = subn;
    }
    if(get_value_by_key(command, _dns1, dns1)) {
    	og.options[OPTION_DNS1].sval = dns1;
    }
  }
  
  if(get_value_by_key(command, _nkey, nkey)) {
      og.options[OPTION_DKEY].sval = nkey;
  }

  og.options_save();
  server_send_result(command, HTML_SUCCESS);
}

void on_sta_change_options() {
  sta_change_options_main(NULL);
}

void sta_options_fill_json(String& json) {
  json = "{";
  OptionStruct *o = og.options;
  for(byte i=0;i<NUM_OPTIONS;i++,o++) {
    if(!o->max) {
      if(i==OPTION_PASS1 || i==OPTION_PASS2 || i==OPTION_DKEY) { // do not output password or device key
        continue;
      } else {
        json += F("\"");
        json += o->name;
        json += F("\":");
        json += F("\"");
        json += o->sval;
        json += F("\"");
        json += ",";
      }
    } else {  // if this is a int option
      json += F("\"");
      json += o->name;
      json += F("\":");
      json += o->ival;
      json += ",";
    }
  }
  json.remove(json.length()-1); // remove the extra ,
  json += F("}");
}

void on_sta_options() {
  if(curr_mode == OG_MOD_AP) return;
  String json;
  sta_options_fill_json(json);
  server_send_json(json);
}

void on_ap_scan() {
  if(curr_mode == OG_MOD_STA) return;
  server_send_json(scanned_ssids);
}

void on_ap_change_config() {
  if(curr_mode == OG_MOD_STA) return;
  if(server->hasArg("ssid1")&&server->arg("ssid1").length()!=0) {
    og.options[OPTION_SSID1].sval = server->arg("ssid1");
    og.options[OPTION_PASS1].sval = server->arg("pass1");
    og.options[OPTION_SSID2].sval = server->arg("ssid2");
    og.options[OPTION_PASS2].sval = server->arg("pass2");

    // if cloud token is provided, save it
    if(server->hasArg("auth")&&server->arg("auth").length()!=0)
      og.options[OPTION_AUTH].sval = server->arg("auth");
    og.options_save();
    server_send_result(HTML_SUCCESS);
    og.state = OG_STATE_TRY_CONNECT;

  } else {
    server_send_result(HTML_DATA_MISSING, "ssid");
  }
}

void on_ap_try_connect() {
  if(curr_mode == OG_MOD_STA) return;
  String json = "{";
  json += F("\"ip\":");
  json += (WiFi.status() == WL_CONNECTED) ? (uint32_t)WiFi.localIP() : 0;
  json += F("}");
  server_send_json(json);
  if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
    /*DEBUG_PRINTLN(F("STA connected, updating option file"));
    og.options[OPTION_MOD].ival = OG_MOD_STA;
    if(og.options[OPTION_AUTH].sval.length() == 32) {
      og.options[OPTION_ACC].ival = OG_ACC_BOTH;
    }
    og.options_save();*/
    DEBUG_PRINTLN(F("IP received by client, restart."));
    //restart_ticker.once_ms(1000, og.restart); // restart once client receives IP address
    restart_in(1000);
  }
}

void on_ap_debug() {
  String json = "";
  json += F("{");
  json += F("\"dist\":");
  json += og.read_distance();
  json += F(",\"fwv\":");
  json += og.options[OPTION_FWV].ival;
  json += F("}");
  server_send_json(json);
}

// MQTT callback to read "Button" requests
void mqtt_callback(const MQTT::Publish& pub) { 
  //DEBUG_PRINT("MQTT Message Received: ");
  //DEBUG_PRINT(pub.topic());
  //DEBUG_PRINT(" Data: ");
  //DEBUG_PRINTLN(pub.payload_string());

  //Accept button on any topic for backwards compat with existing code - use IN messages below if possible
  if (pub.payload_string() == "Button") {
    DEBUG_PRINTLN(F("MQTT Button request received, change door state"));
    if(!og.options[OPTION_ALM].ival) {
      // if alarm is not enabled, trigger relay right away
      og.click_relay();
    } else if(og.options[OPTION_AOO].ival && !door_status) {
      // if 'Do not alarm on open' is on, and door is about to be open, no alarm needed
      og.click_relay();
	  } else {
      // else, set alarm
      og.set_alarm();
    }
  }
  //Accept click for consistency with api, open and close should be used instead, use IN topic if possible
  if (pub.topic() == og.options[OPTION_NAME].sval +"/IN/STATE" ){
    DEBUG_PRINT(F("MQTT IN Message detected, check data for action, Data:"));
    DEBUG_PRINTLN(pub.payload_string());
    if ( (pub.payload_string() == "close" && door_status) || (pub.payload_string() == "open" && !door_status) || pub.payload_string() == "click") {
      DEBUG_PRINTLN(F("Command is valid based on existing state, trigger change"));
      if(!og.options[OPTION_ALM].ival) {
        // if alarm is not enabled, trigger relay right away
        og.click_relay();
      } else if(og.options[OPTION_AOO].ival && !door_status) {
      	// if 'Do not alarm on open' is on, and door is about to be open, no alarm needed
      	og.click_relay();
	    } else {
        // else, set alarm
        og.set_alarm();
      }
    }else if ( (pub.payload_string() == "close" && !door_status) || (pub.payload_string() == "open" && door_status) ){
      DEBUG_PRINTLN(F("Command request not valid, door already in requested state"));
    }
    else {
      DEBUG_PRINT(F("Unrecognized MQTT data/command:"));
      DEBUG_PRINTLN(pub.payload_string());
    }
  }
}

void do_setup()
{
  DEBUG_BEGIN(115200);
  if(server) {
    delete server;
    server = NULL;
  }
  WiFi.persistent(false); // turn off persistent, fixing flash crashing issue
  og.begin();
  og.options_setup();
  og.init_sensors();
  if(og.get_mode() == OG_MOD_AP) og.play_startup_tune();
  DEBUG_PRINT(F("Complile Info: "));
  DEBUG_PRINT(F(__DATE__));
  DEBUG_PRINT(F(" "));
  DEBUG_PRINTLN(F(__TIME__));
  curr_cloud_access_en = og.get_cloud_access_en();
  curr_mode = og.get_mode();
  if(!server) {
    server = new ESP8266WebServer(og.options[OPTION_HTP].ival);
    if(curr_mode == OG_MOD_AP) dns = new DNSServer();
    DEBUG_PRINT(F("server started @ "));
    DEBUG_PRINTLN(og.options[OPTION_HTP].ival);
  }
  led_blink_ms = LED_FAST_BLINK;
  
}

void process_ui()
{
  // process button
  static ulong button_down_time = 0;
  if(og.get_button() == LOW) {
    if(!button_down_time) {
      button_down_time = millis();
    } else {
      ulong curr = millis();
      if(curr > button_down_time + BUTTON_FACRESET_TIMEOUT) {
        led_blink_ms = 0;
        og.set_led(LOW);
      } else if(curr > button_down_time + BUTTON_APRESET_TIMEOUT) {
        led_blink_ms = 0;
        og.set_led(HIGH);
      }
    }
  }
  else {
    if (button_down_time > 0) {
      ulong curr = millis();
      if(curr > button_down_time + BUTTON_FACRESET_TIMEOUT) {
        og.state = OG_STATE_RESET;
      } else if(curr > button_down_time + BUTTON_APRESET_TIMEOUT) {
        og.reset_to_ap();
      } else if(curr > button_down_time + BUTTON_REPORTIP_TIMEOUT) {        
        // report IP
        ipString = get_ip();
        ipString.replace(".", ". ");
        report_ip();
      } else if(curr > button_down_time + 50) {
        og.click_relay();
      }
      button_down_time = 0;
    }
  }
  // process led
  static ulong led_toggle_timeout = 0;
  if(led_blink_ms) {
    if(millis() > led_toggle_timeout) {
      // toggle led
      og.set_led(1-og.get_led());
      led_toggle_timeout = millis() + led_blink_ms;
    }
  }  
}

byte check_door_status_hist() {
  // perform pattern matching of door status histogram
  // and return the corresponding results
  const byte allones = (1<<DOOR_STATUS_HIST_K)-1;       // 0b1111
  const byte lowones = (1<<(DOOR_STATUS_HIST_K/2))-1; // 0b0011
  const byte highones= lowones << (DOOR_STATUS_HIST_K/2); // 0b1100
  
  byte _hist = door_status_hist & allones;  // get the lowest K bits
  if(_hist == 0) return DOOR_STATUS_REMAIN_CLOSED;
  if(_hist == allones) return DOOR_STATUS_REMAIN_OPEN;
  if(_hist == lowones) return DOOR_STATUS_JUST_OPENED;
  if(_hist == highones) return DOOR_STATUS_JUST_CLOSED;

  return DOOR_STATUS_MIXED;
}

void on_sta_update() {
  server_send_html_P(sta_update_html);
}

void on_ap_update() {
  server_send_html_P(ap_update_html);
}

void on_sta_upload_fin() {

  if(!verify_device_key()) {
    server_send_result(HTML_UNAUTHORIZED);
    Update.end(false); // Update.reset(); FAB
    return;
  }

  // finish update and check error
  if(!Update.end(true) || Update.hasError()) {
    server_send_result(HTML_UPLOAD_FAILED);
    return;
  }
  
  server_send_result(HTML_SUCCESS);
  //restart_ticker.once_ms(1000, og.restart);
  restart_in(1000);
}

void on_ap_upload_fin() { on_sta_upload_fin(); }

void on_sta_upload() {
  HTTPUpload& upload = server->upload();
  if(upload.status == UPLOAD_FILE_START){
    DEBUG_PRINTLN(F("Stopping all network clients"));
    WiFiUDP::stopAll();
    Blynk.disconnect(); // disconnect Blynk during firmware upload
    mqttclient.disconnect();
    DEBUG_PRINT(F("prepare to upload: "));
    DEBUG_PRINTLN(upload.filename);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace()-0x1000)&0xFFFFF000;
    if(!Update.begin(maxSketchSpace)) {
      DEBUG_PRINTLN(F("not enough space"));
    }
  } else if(upload.status == UPLOAD_FILE_WRITE) {
    DEBUG_PRINT(".");
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      DEBUG_PRINTLN(F("size mismatch"));
    }
      
  } else if(upload.status == UPLOAD_FILE_END) {
    
    DEBUG_PRINTLN(F("upload completed"));
       
  } else if(upload.status == UPLOAD_FILE_ABORTED){
    Update.end();
    DEBUG_PRINTLN(F("upload aborted"));
  }
  delay(0);    
}

void on_ap_upload() {
  HTTPUpload& upload = server->upload();
  if(upload.status == UPLOAD_FILE_START){
    Serial.println(F("prepare to upload: "));
    Serial.println(upload.filename);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace()-0x1000)&0xFFFFF000;
    if(!Update.begin(maxSketchSpace)) {
      Serial.println(F("not enough space"));
    }
  } else if(upload.status == UPLOAD_FILE_WRITE) {
    Serial.print(".");
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.println(F("size mismatch"));
    }
      
  } else if(upload.status == UPLOAD_FILE_END) {
    
    Serial.println(F("upload completed"));
       
  } else if(upload.status == UPLOAD_FILE_ABORTED){
    Update.end();
    Serial.println(F("upload aborted"));
  }
  delay(0);    
}

void check_status_ap() {
  static ulong cs_timeout = 0;
  if(millis() > cs_timeout) {
    Serial.println(og.read_distance());
    Serial.println(OG_FWV);
    cs_timeout = millis() + 2000;
  }
}

bool mqtt_connect_subscibe() {
  static ulong mqtt_subscribe_timeout = 0;
  if(curr_utc_time > mqtt_subscribe_timeout) {
    if (!mqttclient.connected()) {
      DEBUG_PRINT(F("MQTT Not connected- (Re)connect MQTT"));
      mqttclient.set_server(og.options[OPTION_MQTT].sval, 1883);
      if (mqttclient.connect(og.options[OPTION_NAME].sval)) {
        mqttclient.set_callback(mqtt_callback); 		
        mqttclient.subscribe(og.options[OPTION_NAME].sval);
        mqttclient.subscribe(og.options[OPTION_NAME].sval +"/IN/#");
        DEBUG_PRINTLN(F("......Success, Subscribed to MQTT Topic"));
        return true;
      }else {
        DEBUG_PRINTLN(F("......Failed to Connect to MQTT"));
        mqtt_subscribe_timeout = curr_utc_time + 50; //Takes about 5 seconds to get through the loop
        return false;
      }
    }
  }
}

void perform_notify(String s) {
  DEBUG_PRINT(F("Sending Notify to connected systems, value:"));
  DEBUG_PRINTLN(s);
  // Blynk notification
  if(curr_cloud_access_en && Blynk.connected()) {
    DEBUG_PRINTLN(F(" Blynk Notify"));
    Blynk.notify(s);
  }

  // IFTTT notification
  if(og.options[OPTION_IFTT].sval.length()>7) { // key size is at least 8
    DEBUG_PRINTLN(" Sending IFTTT Notification");
    http.begin("http://maker.ifttt.com/trigger/opengarage/with/key/"+og.options[OPTION_IFTT].sval);
    http.addHeader("Content-Type", "application/json");
    http.POST("{\"value1\":\""+s+"\"}");
    String payload = http.getString();
    http.end();
    if(payload.indexOf("Congratulations") >= 0) {
      DEBUG_PRINTLN(" Successfully updated IFTTT");
    }else{
      DEBUG_PRINT(" Error from IFTTT: ");
      DEBUG_PRINTLN(payload);
    }
  }

  //Mqtt notification
  if(og.options[OPTION_MQTT].sval.length()>8) {
    if (mqttclient.connected()) {
        DEBUG_PRINTLN(" Sending MQTT Notification");
        mqttclient.publish(og.options[OPTION_NAME].sval + "/OUT/NOTIFY",s); 
    }
  }
}

void process_dynamics(byte event) {
  static bool automationclose_triggered=false;
  byte ato = og.options[OPTION_ATO].ival;
  byte atob = og.options[OPTION_ATOB].ival;
  byte noto = og.options[OPTION_NOTO].ival;
  if(!ato && !atob && !noto) {
    justopen_timestamp = 0;
    return;
  }
  if(event == DOOR_STATUS_JUST_OPENED) {
    justopen_timestamp = curr_utc_time; // record time stamp
    if (noto & OG_NOTIFY_DO)
      { perform_notify(og.options[OPTION_NAME].sval + " just OPENED!");}
    
    //If the door is set to auto close at a certain hour, ensure if manually opened it doesn't autoshut
    if( (curr_utc_hour == og.options[OPTION_ATIB].ival) && (!automationclose_triggered) ){
      DEBUG_PRINTLN(" Door opened during automation hour, set to not auto-close ");
      automationclose_triggered=true;
    }

  } else if (event == DOOR_STATUS_JUST_CLOSED) {
    justopen_timestamp = 0; // reset time stamp
    if (noto & OG_NOTIFY_DC)
      { perform_notify(og.options[OPTION_NAME].sval + " just CLOSED!");}

  } else if (event == DOOR_STATUS_REMAIN_OPEN) {
    if (!justopen_timestamp) justopen_timestamp = curr_utc_time; // record time stamp
    else {
      if(curr_utc_time > justopen_timestamp + (ulong)og.options[OPTION_ATI].ival*60L) {
        // reached timeout, perform action
        if(ato & OG_AUTO_NOTIFY) {
          // send notification
          String s = og.options[OPTION_NAME].sval+" is left open for more than ";
          s+= og.options[OPTION_ATI].ival;
          s+= " minutes.";
          if(ato & OG_AUTO_CLOSE) {
            s+= " It will be auto-closed shortly";
          } else {
            s+= " This is a reminder for you.";
          }
          perform_notify(s);
        }
        if(ato & OG_AUTO_CLOSE) {
          // auto close door
          // alarm is mandatory in auto-close
          if(!og.options[OPTION_ALM].ival) { og.set_alarm(OG_ALM_5); }
          else { og.set_alarm(); }
        }
        justopen_timestamp = 0;
      }
      
      if(( curr_utc_hour == og.options[OPTION_ATIB].ival) && (!automationclose_triggered)) {
        // still open past time, perform action
        DEBUG_PRINTLN("Door is open at specified close time and automation not yet triggered: ");
        automationclose_triggered=true;
        if(atob & OG_AUTO_NOTIFY) {
          // send notification
          String s = og.options[OPTION_NAME].sval+" is open after ";
          s+= og.options[OPTION_ATIB].ival;
          s+= " UTC. Current hour:";
          s+= curr_utc_hour;
          if(atob & OG_AUTO_CLOSE) {
            s+= " It will be auto-closed shortly";
          } else {
            s+= " This is a reminder for you.";
          }
          perform_notify(s);
        }
        if(atob & OG_AUTO_CLOSE) {
          // auto close door
          // alarm is mandatory in auto-close
          if(!og.options[OPTION_ALM].ival) { og.set_alarm(OG_ALM_5); }
          else { 
            og.set_alarm(); 
          }
        }
        justopen_timestamp = 0;
      }
      else if ((curr_utc_hour > og.options[OPTION_ATIB].ival) && (automationclose_triggered))
      {
        DEBUG_PRINTLN("Unlocking automation close function");
        automationclose_triggered=false; //Unlock the hour after the setting
      }
    }
  } else {
    justopen_timestamp = 0;
  }
}

void check_status() {
  static ulong checkstatus_timeout = 0;
  static ulong checkstatus_report_timeout = 0; 
  if((curr_utc_time > checkstatus_timeout) || (checkstatus_timeout == 0))  { //also check on first boot
    og.set_led(HIGH);
    aux_ticker.once_ms(25, og.set_led, (byte)LOW);
    uint threshold = og.options[OPTION_DTH].ival;
    uint vthreshold = og.options[OPTION_VTH].ival;
    if ((og.options[OPTION_MNT].ival == OG_MNT_SIDE) || (og.options[OPTION_MNT].ival == OG_MNT_CEILING)){
      //sensor is ultrasonic
      distance = og.read_distance();
      door_status = (distance>threshold)?0:1; 
      if (og.options[OPTION_MNT].ival == OG_MNT_SIDE){
       door_status = 1-door_status;  // reverse logic for side mount
       vehicle_status = 3;}
      else {
        if (vthreshold >0) {
          if (!door_status) {
            vdistance = distance;
            vehicle_status = ((vdistance>threshold) && (vdistance <=vthreshold))?1:0;
          }else{vehicle_status = 2;}
        }else {vehicle_status = 3;}
      }
    }else if (og.options[OPTION_MNT].ival == OG_SWITCH_LOW){
      vehicle_status= 3;
      if (og.get_switch() == LOW){
        //DEBUG_PRINTLN("Low Mount Switch reads LOW, setting distance to high value (indicating closed)");
        door_status =0; 
        distance = threshold + 20;
      }
      else{
        //DEBUG_PRINTLN("Low Mount Switch reads HIGH, setting distance to low value (indicating open)");
        door_status =1; 
        distance = threshold - 20;
      }
    }else if (og.options[OPTION_MNT].ival == OG_SWITCH_HIGH){
      vehicle_status= 3;
      if (og.get_switch() == LOW){
        //DEBUG_PRINTLN("High Mount Switch reads LOW, setting distance to low value (indicating open)");
        door_status =1; 
        distance = threshold - 20;
      }
      else{
        //DEBUG_PRINTLN("High Mount Switch reads HIGH, setting distance to high value (indicating closed)");
        door_status =0; 
        distance = threshold + 20;
      }
    }
    read_cnt = (read_cnt+1)%100;
    // get temperature readings
    og.read_TH_sensor(tempC, humid);
    if (checkstatus_timeout == 0){
      DEBUG_PRINTLN(F("First time checking status don't trigger a status change, set full history to current value"));
      if (door_status) { door_status_hist = B11111111; }
      else { door_status_hist = B00000000; }
    }else{
       door_status_hist = (door_status_hist<<1) | door_status;
    }
    //DEBUG_PRINT(F("Histogram value:"));
    //DEBUG_PRINTLN(door_status_hist);
    //DEBUG_PRINT(F("Vehicle Status:"));
    //DEBUG_PRINTLN(vehicle_status);
    byte event = check_door_status_hist();

    //Upon change
    if(event == DOOR_STATUS_JUST_OPENED || event == DOOR_STATUS_JUST_CLOSED) {
      // write log record
      DEBUG_PRINTLN(" Update Local Log"); 
      LogStruct l;
      l.tstamp = curr_utc_time;
      l.status = door_status;
      l.dist = distance;
      og.write_log(l);

#if 0
      //Debug Beep (only if sound is enabled)
      if(og.options[OPTION_ALM].ival){
        og.play_note(1000);
        delay(500);
        og.play_note(0);
      }
      DEBUG_PRINT(curr_utc_time);
      if(event == DOOR_STATUS_JUST_OPENED)  {	
        DEBUG_PRINTLN(F(" Sending State Change event to connected systems, value: DOOR_STATUS_JUST_OPENED")); }
      else if(event == DOOR_STATUS_JUST_CLOSED) {	
        DEBUG_PRINTLN(F(" Sending State Change event to connected systems, value: DOOR_STATUS_JUST_CLOSED")); }
#endif
      
      // Blynk notification
#if 0
      byte ato = og.options[OPTION_ATO].ival;
      if(curr_cloud_access_en && Blynk.connected() && ato) {
        //The official firmware only sends these notifications on ato enabled (which seems a somewhat unrelated function)
        //Maintain backwards compat and use same logic
        DEBUG_PRINTLN(F(" Notify Blynk with text notification"));
        if(event == DOOR_STATUS_JUST_OPENED)  {	
          Blynk.notify(og.options[OPTION_NAME].sval + " just opened!");}
        else if(event == DOOR_STATUS_JUST_CLOSED) {	
          Blynk.notify(og.options[OPTION_NAME].sval + " just closed!");}
      }

      // IFTTT notification
      if(og.options[OPTION_IFTT].sval.length()>7) { // key size is at least 8
        DEBUG_PRINTLN(F(" Notify IFTTT (State Change)")); 
        http.begin("http://maker.ifttt.com/trigger/opengarage/with/key/"+og.options[OPTION_IFTT].sval);
        http.addHeader("Content-Type", "application/json");
        http.POST("{\"value1\":\""+String(event,DEC)+"\"}");
        String payload = http.getString();
        http.end();
        if(payload.indexOf("Congratulations") >= 0) {
          DEBUG_PRINTLN(F("  Successfully updated IFTTT"));
        }else{
          DEBUG_PRINT(F("  ERROR from IFTTT: "));
          DEBUG_PRINTLN(payload);
        }
      }

      //Mqtt notification
      if(og.options[OPTION_MQTT].sval.length()>8) {
        if (mqttclient.connected()) {
          DEBUG_PRINTLN(F(" Update MQTT (State Change)"));
          mqttclient.publish(og.options[OPTION_NAME].sval + "/OUT/CHANGE",String(event,DEC)); 
        }
      }
#endif
    } //End state change updates

    //Send current status only on change and longer interval
    if ((curr_utc_time >checkstatus_report_timeout) || (event == DOOR_STATUS_JUST_OPENED || event == DOOR_STATUS_JUST_CLOSED) ){
#if 0
      DEBUG_PRINT(curr_utc_time);
      if(event == DOOR_STATUS_REMAIN_OPEN)  {	
        DEBUG_PRINTLN(F(" Sending State Refresh to connected systems, value: OPEN")); }
      else if(event == DOOR_STATUS_REMAIN_CLOSED) {	
        DEBUG_PRINTLN(F(" Sending State Refresh to connected systems, value: CLOSED")); }
#endif
      
      //IFTTT only recieves state change events not ongoing status

      //Mqtt update
      if((og.options[OPTION_MQTT].sval.length()>8) && (mqttclient.connected())) {
        DEBUG_PRINTLN(F(" Update MQTT (State Refresh)"));
        if(door_status == DOOR_STATUS_REMAIN_OPEN)  {						// MQTT: If door open...
          mqttclient.publish(og.options[OPTION_NAME].sval + "/OUT/STATE","OPEN");
          mqttclient.publish(og.options[OPTION_NAME].sval,"Open"); //Support existing mqtt code
          //DEBUG_PRINTLN(curr_utc_time + " Sending MQTT State otification: OPEN");
        } 
        else if(door_status == DOOR_STATUS_REMAIN_CLOSED) {					// MQTT: If door closed...
          mqttclient.publish(og.options[OPTION_NAME].sval + "/OUT/STATE","CLOSED");
          mqttclient.publish(og.options[OPTION_NAME].sval,"Closed"); //Support existing mqtt code
          //DEBUG_PRINTLN(curr_utc_time + " Sending MQTT State Notification: CLOSED");
        }
      }
      // Send status report every 15 seconds: we don't need to send updates frequently if there is no status change.
      checkstatus_report_timeout= curr_utc_time + 15; 
    }
    
    // Process dynamics: automation and notifications
    // report status to Blynk
    if(curr_cloud_access_en && Blynk.connected()) {
      DEBUG_PRINTLN(F(" Update Blynk (State Refresh)"));
      
      static uint old_distance = 0;
      static byte old_door_status = 0xff, old_vehicle_status = 0xff;
      static String old_ip = "";
      if(distance != old_distance) {  Blynk.virtualWrite(BLYNK_PIN_DIST, distance); old_distance = distance; }
      if(door_status != old_door_status) { (door_status) ? blynk_door.on() : blynk_door.off(); old_door_status = door_status; }
      if(vehicle_status != old_vehicle_status) { (vehicle_status==1) ? blynk_car.on() : blynk_car.off(); old_vehicle_status = vehicle_status; }
      if(old_ip != get_ip()) { Blynk.virtualWrite(BLYNK_PIN_IP, get_ip()); old_ip = get_ip(); }

      // report json strings to Blynk
      /* comment this section out as the features are not fully ready yet
      String json;
      static String old_json = "";
      sta_controller_fill_json(json);
      if(old_json != json) { Blynk.virtualWrite(BLYNK_PIN_JC, json); old_json = json; }
      
      if(og.get_dirty_bit(DIRTY_BIT_JO)) {
        sta_options_fill_json(json);
        Blynk.virtualWrite(BLYNK_PIN_JO, json);
        og.set_dirty_bit(DIRTY_BIT_JO, 0);
      }
      
      if(og.get_dirty_bit(DIRTY_BIT_JL)) {
        sta_logs_fill_json(json);
        Blynk.virtualWrite(BLYNK_PIN_JL, json);
        og.set_dirty_bit(DIRTY_BIT_JL, 0);
      }
      */
    }
    
    process_dynamics(event);
    checkstatus_timeout = curr_utc_time + og.options[OPTION_RIV].ival;
    
  }
}

void time_keeping() {
  static bool configured = false;
  static ulong prev_millis = 0;
  static ulong time_keeping_timeout = 0;

  if(!configured) {
    DEBUG_PRINTLN(F("Set time server"));
    configTime(0, 0, "time.google.com", "pool.ntp.org", NULL);
    configured = true;
  }

  if(!curr_utc_time || (curr_utc_time > time_keeping_timeout)) {
    ulong gt = time(nullptr);
    if(gt<978307200L) {
      // if we didn't get response, re-try after 2 seconds
      time_keeping_timeout = curr_utc_time + 2;
    } else {
      curr_utc_time = gt;
      curr_utc_hour = (curr_utc_time % 86400)/3600;
      DEBUG_PRINT(F("Updated time from NTP: "));
      DEBUG_PRINT(curr_utc_time);
      DEBUG_PRINT(" Hour: ");
      DEBUG_PRINTLN(curr_utc_hour);
      // if we got a response, re-try after TIME_SYNC_TIMEOUT seconds
      time_keeping_timeout = curr_utc_time + TIME_SYNC_TIMEOUT;
      prev_millis = millis();
    }
  }
  while(millis() - prev_millis >= 1000) {
    curr_utc_time ++;
    curr_utc_hour = (curr_utc_time % 86400)/3600;
    prev_millis += 1000;
  }
}

void process_alarm() {
  if(!og.alarm) return;
  static ulong prev_half_sec = 0;
  ulong curr_half_sec = millis()/500;
  if(curr_half_sec != prev_half_sec) {  
    prev_half_sec = curr_half_sec;
    if(prev_half_sec % 2 == 0) {
      og.play_note(ALARM_FREQ);
    } else {
      og.play_note(0);
    }
    og.alarm--;
    if(og.alarm==0) {
      og.play_note(0);
      og.click_relay();
    }
  }
}


void do_loop() {

  static ulong connecting_timeout;
  
  switch(og.state) {
  case OG_STATE_INITIAL:
    if(curr_mode == OG_MOD_AP) {
      scanned_ssids = scan_network();
      String ap_ssid = get_ap_ssid();
      start_network_ap(ap_ssid.c_str(), NULL);
      delay(500);
      dns->setErrorReplyCode(DNSReplyCode::NoError);
      dns->start(53, "*", WiFi.softAPIP());
      server->on("/",   on_home);    
      server->on("/js", on_ap_scan);
      server->on("/cc", on_ap_change_config);
      server->on("/jt", on_ap_try_connect);
      server->on("/db", on_ap_debug);      
      server->on("/update", HTTP_GET, on_ap_update);
      server->on("/update", HTTP_POST, on_ap_upload_fin, on_ap_upload);      
      server->on("/resetall",on_reset_all);
      server->onNotFound(on_home);
      server->begin();
      DEBUG_PRINTLN(F("Web Server endpoints (AP mode) registered"));
      og.state = OG_STATE_CONNECTED;
      DEBUG_PRINTLN(WiFi.softAPIP());
      connecting_timeout = 0;
    } else {
      led_blink_ms = LED_SLOW_BLINK;
      DEBUG_PRINT(F("Attempting to connect to SSID: "));
      DEBUG_PRINT(og.options[OPTION_SSID1].sval.c_str());
      DEBUG_PRINT(F(" or "));
      DEBUG_PRINTLN(og.options[OPTION_SSID2].sval.c_str());
      WiFi.mode(WIFI_STA);
      start_network_sta(og.options[OPTION_SSID1].sval.c_str(), og.options[OPTION_PASS1].sval.c_str(), og.options[OPTION_SSID2].sval.c_str(), og.options[OPTION_PASS2].sval.c_str());
      og.config_ip();
      og.state = OG_STATE_CONNECTING;
      connecting_timeout = millis() + 60000;
    }
    break;

  case OG_STATE_TRY_CONNECT:
    led_blink_ms = LED_SLOW_BLINK;
    DEBUG_PRINT(F("Attempting to connect to SSID: "));
    DEBUG_PRINT(og.options[OPTION_SSID1].sval.c_str());
    DEBUG_PRINT(F(" or "));
    DEBUG_PRINTLN(og.options[OPTION_SSID2].sval.c_str());
    start_network_sta_with_ap(og.options[OPTION_SSID1].sval.c_str(), og.options[OPTION_PASS1].sval.c_str(), og.options[OPTION_SSID2].sval.c_str(), og.options[OPTION_PASS2].sval.c_str());
    wifi_run(); // First wifi_run needed for initual setup.
    og.config_ip();
    og.state = OG_STATE_CONNECTED;
    break;
    
  case OG_STATE_CONNECTING:
    if(wifi_run() == WL_CONNECTED) {
      DEBUG_PRINT(F("Wireless connected, IP: "));
      DEBUG_PRINTLN(WiFi.localIP());

      server->on("/", on_home);
      server->on("/jc", on_sta_controller);
      server->on("/jo", on_sta_options);
      server->on("/jl", on_sta_logs);
      server->on("/vo", on_sta_view_options);
      server->on("/vl", on_sta_view_logs);
      server->on("/cc", on_sta_change_controller);
      server->on("/co", on_sta_change_options);
      server->on("/db", on_sta_debug);
      server->on("/update", HTTP_GET, on_sta_update);
      server->on("/update", HTTP_POST, on_sta_upload_fin, on_sta_upload);
      server->on("/clearlog", on_clear_log);
      server->on("/resetall",on_reset_all);
      server->begin();
      DEBUG_PRINTLN(F("Web Server endpoints (STA mode) registered"));

      // use ap ssid as mdns name
      if(MDNS.begin(get_ap_ssid().c_str(), WiFi.localIP())) {
        DEBUG_PRINTLN(F("MDNS registered"));
        DEBUG_PRINTLN(get_ap_ssid().c_str());
        //MDNS.addService("http", "tcp", og.options[OPTION_HTP].ival);
				//DEBUG_PRINTLN(og.options[OPTION_HTP].ival);
      }

      if(curr_cloud_access_en) {
        Blynk.config(og.options[OPTION_AUTH].sval.c_str(), og.options[OPTION_BDMN].sval.c_str(), (uint16_t) og.options[OPTION_BPRT].ival); // use the config function
        Blynk.connect();
        DEBUG_PRINTLN(F("Blynk Connected"));
      }
      if(og.options[OPTION_MQTT].sval.length()>8) {
        mqtt_connect_subscibe();
        DEBUG_PRINTLN(F("MQTT Connected"));
      }
      led_blink_ms = 0;
      og.set_led(LOW);
      og.state = OG_STATE_CONNECTED;
      connecting_timeout = 0;
    } else {
      if(millis() > connecting_timeout) {
        DEBUG_PRINTLN(F("Wifi Connecting timeout, restart"));
        og.restart();
      }
    }
    break;

  case OG_STATE_RESET:
    og.state = OG_STATE_INITIAL;
    og.options_reset();
    og.log_reset();
    og.restart();
    break;
    
  case OG_STATE_WAIT_RESTART:
    if(dns) dns->processNextRequest();  
    if(server) server->handleClient();    
    break;
    
  case OG_STATE_CONNECTED: //THIS IS THE MAIN LOOP
    if(curr_mode == OG_MOD_AP) {
      dns->processNextRequest();
      server->handleClient();
      check_status_ap();
      connecting_timeout = 0;
      if(og.options[OPTION_MOD].ival == OG_MOD_STA) {
        // already in STA mode, waiting to reboot
        break;
      }
      if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
        DEBUG_PRINTLN(F("STA connected, updating option file"));
        og.options[OPTION_MOD].ival = OG_MOD_STA;
        og.options_save();
        og.play_startup_tune();
        //restart_ticker.once_ms(10000, og.restart);
        restart_in(10000);
      }
      
    } else {
      if(wifi_run() == WL_CONNECTED) {
      	//MDNS.update();
        time_keeping();
        check_status(); //This checks the door, sends info to services and processes the automation rules
        server->handleClient();

        if(curr_cloud_access_en)
          Blynk.run();
        //Handle MQTT
        if(og.options[OPTION_MQTT].sval.length()>8) {
          if (!mqttclient.connected()) {
            mqtt_connect_subscibe();
          }
          else {mqttclient.loop();} //Processes MQTT Pings/keep alives
        }
        connecting_timeout = 0;
      } else {
        //og.state = OG_STATE_INITIAL;
        if(!connecting_timeout) {
          DEBUG_PRINTLN(F("State is CONNECTED but WiFi is disconnected, start timeout counter."));
          connecting_timeout = millis()+60000;
        }
        else if(millis() > connecting_timeout) {
          DEBUG_PRINTLN(F("timeout reached, reboot"));
          og.restart();
        }
      }
    }
    break;
  }

  //Nework independent functions, handle events like reset even when not connected
  process_ui();
  if(og.alarm)
    process_alarm();
}

BLYNK_WRITE(BLYNK_PIN_RELAY) {
  DEBUG_PRINTLN(F("Received Blynk generated button request"));
  if(!og.options[OPTION_ALM].ival) {
    // if alarm is disabled, trigger right away
    if(param.asInt()) {
      og.set_relay(HIGH);
    } else {
      og.set_relay(LOW);
    }
  } else {
    // otherwise, set alarm
    if(param.asInt()) {
      if(!og.options[OPTION_ALM].ival) {
        og.set_alarm(OG_ALM_5);
      } else if(og.options[OPTION_AOO].ival && !door_status) {
      	og.click_relay();
      } else {
        og.set_alarm();
      }    
    }  
  }
}

BLYNK_WRITE(BLYNK_PIN_CC) {
  /* comment this section out as the features are not fully ready yet
  DEBUG_PRINTLN(F("Received Blynk cc request"));
  DEBUG_PRINTLN(param.asStr());
  sta_change_controller_main(param.asStr());
  */
}

BLYNK_WRITE(BLYNK_PIN_CO) {
	/* comment this section out as the features are not fully ready yet
  DEBUG_PRINTLN(F("Received Blynk co request"));
  DEBUG_PRINTLN(param.asStr());  
  sta_change_options_main(param.asStr());
  */
}
