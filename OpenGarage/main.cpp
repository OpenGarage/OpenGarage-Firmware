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
#include <PubSubClient.h>
#include <OpenThingsFramework.h>
#include <Request.h>
#include <Response.h>

#include "pitches.h"
#include "OpenGarage.h"
#include "espconnect.h"

OpenGarage og;
OTF::OpenThingsFramework *otf = NULL;
ESP8266WebServer *updateServer = NULL;
DNSServer *dns = NULL;

WidgetLED blynk_door(BLYNK_PIN_DOOR);
WidgetLED blynk_car(BLYNK_PIN_CAR);

static Ticker aux_ticker;
static Ticker ip_ticker;
static Ticker restart_ticker;

static WiFiClient wificlient;
static WiFiClient httpclient;
PubSubClient mqttclient(wificlient);
String mqtt_topic;
String mqtt_id;

static String scanned_ssids;
static byte read_cnt = 0;
static uint distance = 0;
static byte sn2_value = 0;
static float tempC = 0;
static float humid = 0;
static byte door_status = 0; //0: closed, 1: open
static int vehicle_status = OG_VEH_ABSENT;
static uint led_blink_ms = LED_FAST_BLINK;
static ulong justopen_timestamp = 0;
static byte curr_mode;
extern bool fullbuffer;
// this is one byte storing the door status histogram
// maximum 8 bits
static byte door_status_hist = 0;
static ulong curr_utc_time = 0;
static ulong curr_utc_hour= 0;
static HTTPClient http;

void do_setup();

void otf_send_html_P(OTF::Response &res, const __FlashStringHelper *content) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("text/html"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*")); // from esp8266 2.4 this has to be sent explicitly
	res.writeHeader(F("Content-Length"), strlen_P((char *) content));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyChunk((char *) "%s", content);
	DEBUG_PRINT(strlen_P((char *) content));
	DEBUG_PRINTLN(F(" bytes sent."));
}

void otf_send_json(OTF::Response &res, String json) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("application/json"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*")); // from esp8266 2.4 this has to be sent explicitly
	res.writeHeader(F("Content-Length"), json.length());
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyChunk((char *) "%s",json.c_str());
}

void otf_send_result(OTF::Response &res, byte code, const char *item = NULL) {
	String json = F("{\"result\":");
	json += code;
	if (!item) item = "";
	json += F(",\"item\":\"");
	json += item;
	json += F("\"");
	json += F("}");
	otf_send_json(res, json);
}

void updateserver_send_result(byte code, const char* item = NULL) {
	String json = F("{\"result\":");
	json += code;
	if (!item) item = "";
	json += F(",\"item\":\"");
	json += item;
	json += F("\"");
	json += F("}");
	updateServer->sendHeader("Access-Control-Allow-Origin", "*"); // from esp8266 2.4 this has to be sent explicitly
	updateServer->send(200, "application/json", json);
}

// this is simplified version to check if MQTT / NTP server is valid domain name
bool valid_url(String s) {
	return (s.length()>0 && s[0]!='-' && s.indexOf('.')>0);
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

void on_home(const OTF::Request &req, OTF::Response &res)
{
	if(curr_mode == OG_MOD_AP) {
		otf_send_html_P(res, (const __FlashStringHelper *) ap_home_html);
	} else {
		otf_send_html_P(res, (const __FlashStringHelper *) sta_home_html);
	}
}

void on_sta_view_options(const OTF::Request &req, OTF::Response &res) {
	otf_send_html_P(res, (const __FlashStringHelper *) sta_options_html);
}

void on_sta_view_logs(const OTF::Request &req, OTF::Response &res) {
	otf_send_html_P(res, (const __FlashStringHelper *) sta_logs_html);
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
	IPAddress _ip = WiFi.localIP();
	String ip = String(_ip[0]);
	ip += ".";
	ip += _ip[1];
	ip += ".";
	ip += _ip[2];
	ip += ".";
	ip += _ip[3];
	return ip;
}

void sta_controller_fill_json(String& json, bool fullversion=true) {
	json = "";
	json += F("{\"dist\":");
	json += distance;
	if(og.options[OPTION_SN2].ival>OG_SN2_NONE) {
		json += F(",\"sn2\":");
		json += sn2_value;
	}
	json += F(",\"door\":");
	json += door_status;
	json += F(",\"vehicle\":");
	json += vehicle_status;
	json += F(",\"rcnt\":");
	json += read_cnt;
	if(fullversion) {
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
		json += F(",\"cld\":");
		byte cld = og.options[OPTION_CLD].ival;
		json += cld;
		if(cld>CLD_NONE) {
			json += F(",\"clds\":");
			if(cld==CLD_BLYNK) json += (Blynk.connected()?1:0);
			else if(cld==CLD_OTC) json += (otf->getCloudStatus());
			else json += "0";
		}
	}
	if(og.options[OPTION_TSN].ival) {
		json += F(",\"temp\":");
		json += tempC;
		json += F(",\"humid\":");
		json += humid;
	}
	json += F("}");
}

void on_sta_controller(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;
	String json;
	sta_controller_fill_json(json);
	otf_send_json(res, json);
}

void on_sta_debug(const OTF::Request &req, OTF::Response &res) {
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
	json += F("\",\"mqtt_topic\":\"");
	json += mqtt_topic;
	json += F("\",\"devip\":\"");
	json += WiFi.localIP().toString();
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
	json += F(",\"flash_size\":");
	json += (uint32_t)ESP.getFlashChipRealSize();
	json += F("}");
	otf_send_json(res, json);
}

void sta_logs_fill_json(String& json, OTF::Response &res) {
	json = "";
	json += F("{\"name\":\"");
	json += og.options[OPTION_NAME].sval;
	json += F("\",\"time\":");
	json += curr_utc_time;
	json += F(",\"ncols\":");
	json += og.options[OPTION_SN2].ival>OG_SN2_NONE ? 4 : 3;
	json += F(",\"logs\":[");
	if(!og.read_log_start()) {
		json += F("]}");
		//otf_send_json(res, json);
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
		if(og.options[OPTION_SN2].ival>OG_SN2_NONE) {
		  json += F(",");
		  json += l.sn2;
		}
		json += F("],");
	}
	og.read_log_end();
	json.remove(json.length()-1); // remove the extra ,
	json += F("]}");
}

void on_sta_logs(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;
	String json;
	sta_logs_fill_json(json, res);
	otf_send_json(res, json);
}

bool verify_device_key(const OTF::Request &req) {
	if(req.isCloudRequest()){ // no need for dkey if this is coming from cloud connection
		return true;
	}
	const char *dkey = req.getQueryParameter("dkey");
	if(dkey != NULL && strcmp(dkey, og.options[OPTION_DKEY].sval.c_str()) == 0)
		return true;
	return false;
}

void on_reset_all(const OTF::Request &req, OTF::Response &res){
	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}

	og.state = OG_STATE_RESET;
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void on_clear_log(const OTF::Request &req, OTF::Response &res) {
	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}
	og.log_reset();
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void sta_change_controller_main(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;

	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}

	bool click = req.getQueryParameter("click");
	bool close = req.getQueryParameter("close");
	bool open = req.getQueryParameter("open");

	if(click || close || open) {
		DEBUG_PRINTLN(F("Received button request (click, close, or open)"));
		otf_send_result(res, HTML_SUCCESS, nullptr);
		//1 is open
		if ((close && door_status) ||
				(open && !door_status) ||
				(click)) {
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
	} else if(req.getQueryParameter("reboot") != NULL) {
		otf_send_result(res, HTML_SUCCESS, nullptr);
		//restart_ticker.once_ms(1000, og.restart);
		restart_in(1000);
	} else if(req.getQueryParameter("apmode") != NULL) {
		otf_send_result(res, HTML_SUCCESS, nullptr);
		og.reset_to_ap();
	} else {
		otf_send_result(res, HTML_NOT_PERMITTED, nullptr);
	}

}

void on_sta_change_controller(const OTF::Request &req, OTF::Response &res) {
	sta_change_controller_main(req, res);
}

void sta_change_options_main(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;

	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}

	byte i;
	OptionStruct *o = og.options;

	byte usi = 0;
	// FIRST ROUND: check option validity
	// do not save option values yet
	for(i=0;i<NUM_OPTIONS;i++,o++) {
		const char *key = o->name.c_str();
		// these options cannot be modified here
		if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID ||
			i==OPTION_PASS || i==OPTION_DKEY)
			continue;
		
		if(o->max) {  // integer options
			char *sval = req.getQueryParameter(key);
			if(sval != NULL) {
				uint ival = String(sval).toInt();
				if(ival>o->max) {	// check max limit
				  otf_send_result(res, HTML_DATA_OUTOFBOUND, key);
				  return;
				}
				// check min limit
				if(i==OPTION_DRI && ival < 50) {
					otf_send_result(res, HTML_DATA_OUTOFBOUND, key);
					return;
				}
				if(i==OPTION_LSZ && ival < 20) {
					// minimal log size is 20
					otf_send_result(res, HTML_DATA_OUTOFBOUND, key);
					return;
				}
				if(i==OPTION_CDT && ival < 50) {
					// click delay time should be at least 50 ms
					otf_send_result(res, HTML_DATA_OUTOFBOUND, key);
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
	const char* _dvip = "dvip";
	const char* _gwip = "gwip";
	const char* _subn = "subn";
	const char* _dns1 = "dns1";

	String dvip = req.getQueryParameter(_dvip);
	String gwip = req.getQueryParameter(_gwip);
	String subn = req.getQueryParameter(_subn);
	String dns1 = req.getQueryParameter(_dns1);

	if(usi) {
		if(dvip != NULL) {
			if(gwip != NULL) {
				// check validity of IP address
				IPAddress ip;
				if(!ip.fromString(dvip)) { otf_send_result(res, HTML_DATA_FORMATERROR, _dvip); return;}
				if(!ip.fromString(gwip)) { otf_send_result(res, HTML_DATA_FORMATERROR, _gwip); return;}
				if(subn != NULL) {
					if(!ip.fromString(subn)) {
						otf_send_result(res, HTML_DATA_FORMATERROR, _subn);
						return;
					}
				}
				if(dns1 != NULL) {
					if(!ip.fromString(dns1)) {
						otf_send_result(res, HTML_DATA_FORMATERROR, _dns1);
						return;
					}
				}
			} else {
				otf_send_result(res, HTML_DATA_MISSING, _gwip);
				return;
			}
		} else {
			otf_send_result(res, HTML_DATA_MISSING, _dvip);
			return;
		}
	}
	// Check device key change
	const char* _nkey = "nkey";
	const char* _ckey = "ckey";

	char* nkey = req.getQueryParameter(_nkey);
	char* ckey = req.getQueryParameter(_ckey);

	if(nkey != NULL) {
		if(ckey != NULL) {
			if(strcmp(nkey,ckey)!=0) {
				otf_send_result(res, HTML_MISMATCH, _ckey);
				return;
			}
		} else {
			otf_send_result(res, HTML_DATA_MISSING, _ckey);
			return;
		}
	}

	// SECOND ROUND: change option values
	o = og.options;
	for(i=0;i<NUM_OPTIONS;i++,o++) {
		const char *key = o->name.c_str();
		// these options cannot be modified here
		if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID ||
			i==OPTION_PASS || i==OPTION_DKEY)
			continue;

		char *sval = req.getQueryParameter(key);
		if(sval != NULL) {
			if (o->max) {  // integer options
				uint ival = String(sval).toInt();
				o->ival = ival;
			} else {
				o->sval = String(sval);
			}
		}
	}

	if(usi) {
		og.options[OPTION_DVIP].sval = dvip;
		og.options[OPTION_GWIP].sval = gwip;
		if(subn != NULL) {
			og.options[OPTION_SUBN].sval = subn;
		}
		if(dns1 != NULL) {
			og.options[OPTION_DNS1].sval = dns1;
		}
	}

	if(nkey != NULL) {
			og.options[OPTION_DKEY].sval = nkey;
	}

	og.options_save();
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void on_sta_change_options(const OTF::Request &req, OTF::Response &res) {
	sta_change_options_main(req, res);
}

void sta_options_fill_json(String& json) {
	json = "{";
	OptionStruct *o = og.options;
	for(byte i=0;i<NUM_OPTIONS;i++,o++) {
		if(!o->max) {
			if(i==OPTION_PASS || i==OPTION_DKEY || i==OPTION_MQPW) { // do not output password or device key or MQTT password
				continue;
			} else {
				json += F("\"");
				json += o->name;
				json += F("\":");
				json += F("\"");
				// fill in default string values for certain options
				if(o->sval.length()==0) {
					switch(i) {
					case OPTION_MQTP:
						json += og.options[OPTION_NAME].sval;
						break;
					case OPTION_HOST:
						json += get_ap_ssid();
						break;
					//case OPTION_NTP1: // leave NPT1 empty if using default
					//json += DEFAULT_NTP1;
					//break;
					}
				} else {
					json += o->sval;
				}
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

void on_sta_options(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;
	String json;
	sta_options_fill_json(json);
	otf_send_json(res, json);
}

void on_ap_scan(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_STA) return;
	otf_send_json(res, scanned_ssids);
}

void on_ap_change_config(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_STA) return;
	char *ssid = req.getQueryParameter("ssid");
	if(ssid!=NULL&&strlen(ssid)!=0) {
		og.options[OPTION_SSID].sval = ssid;
		og.options[OPTION_PASS].sval = req.getQueryParameter("pass");
		// if cloud token is provided, save it
		char *cld = req.getQueryParameter("cld");
		char *auth = req.getQueryParameter("auth");
		if(cld!=NULL&&strlen(cld)!=0&&auth!=NULL&&strlen(auth)!=0) {
			if(strcmp(cld, "blynk")==0 || strcmp(cld, "otc")==0) {
				char *bdmn = req.getQueryParameter("bdmn");
				char *bprt = req.getQueryParameter("bprt");
				if(strcmp(cld, "blynk")==0) {
					og.options[OPTION_BDMN].sval=(bdmn==NULL||strlen(bdmn)==0)?DEFAULT_BLYNK_DMN:bdmn;
					og.options[OPTION_BPRT].ival=(bprt==NULL||strlen(bprt)==0)?DEFAULT_BLYNK_PRT:String(bprt).toInt();
					og.options[OPTION_CLD].ival = CLOUD_BLYNK;
				} else {
					og.options[OPTION_BDMN].sval=(bdmn==NULL||strlen(bdmn)==0)?DEFAULT_OTC_DMN:bdmn;
					og.options[OPTION_BPRT].ival=(bprt==NULL||strlen(bprt)==0)?DEFAULT_OTC_PRT:String(bprt).toInt();
					og.options[OPTION_CLD].ival = CLOUD_OTC;
				}
				og.options[OPTION_AUTH].sval = auth;
				og.options_save();
				DEBUG_PRINTLN(og.options[OPTION_CLD].ival);
				DEBUG_PRINTLN(og.options[OPTION_AUTH].sval);
				DEBUG_PRINTLN(og.options[OPTION_BDMN].sval);
				DEBUG_PRINTLN(og.options[OPTION_BPRT].ival);
			} else {
				otf_send_result(res, HTML_DATA_OUTOFBOUND, "cld");
				return;
			}
		}
		otf_send_result(res, HTML_SUCCESS, nullptr);
		og.state = OG_STATE_TRY_CONNECT;
	} else {
		otf_send_result(res, HTML_DATA_MISSING, "ssid");
	}
}

void on_ap_try_connect(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_STA) return;
	String json = "{";
	json += F("\"ip\":");
	json += (WiFi.status() == WL_CONNECTED) ? (uint32_t)WiFi.localIP() : 0;
	json += F("}");
	otf_send_json(res, json);
	if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
		DEBUG_PRINTLN(F("IP received by client, restart."));
		restart_in(1000);
	}
}

void on_ap_debug(const OTF::Request &req, OTF::Response &res) {
	String json = "";
	json += F("{");
	json += F("\"dist\":");
	json += og.read_distance();
	json += F(",\"fwv\":");
	json += og.options[OPTION_FWV].ival;
	json += F("}");
	otf_send_json(res, json);
}

// MQTT callback to read "Button" requests
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
	payload[length]=0;
	String Payload((char*)payload);
	String Topic(topic);
	/*DEBUG_PRINT("MQTT Message Received: ");
	DEBUG_PRINT(Topic);
	DEBUG_PRINT(" Data: ");
	DEBUG_PRINTLN(Payload);*/

	//Accept button on any topic for backwards compat with existing code - use IN messages below if possible
	if (Payload=="Button") {
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
	if (Topic==(mqtt_topic+"/IN/STATE")){
		DEBUG_PRINT(F("MQTT IN Message detected, check data for action, Data:"));
		DEBUG_PRINTLN(Payload);
		if ( (Payload == "close" && door_status) || (Payload == "open" && !door_status) || Payload == "click") {
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
		}else if ( (Payload=="close" && !door_status) || (Payload=="open" && door_status) ){
			DEBUG_PRINTLN(F("Command request not valid, door already in requested state"));
		}
		else {
		  DEBUG_PRINT(F("Unrecognized MQTT data/command:"));
		}
	}
}

void do_setup()
{
	Serial.begin(115200);
	if(otf) {
		delete otf;
		otf = NULL;
	}
	if(updateServer) {
		delete updateServer;
		updateServer = NULL;
	}
	WiFi.persistent(false); // turn off persistent, fixing flash crashing issue
	og.begin();
	og.options_setup();
	og.init_sensors();
	if(og.get_mode() == OG_MOD_AP) og.play_startup_tune();
	curr_mode = og.get_mode();
	if(!otf) {
		const String otfDeviceKey = og.options[OPTION_AUTH].sval;

		if((og.options[OPTION_CLD].ival==CLD_OTC) && (otfDeviceKey.length() >= 32)) {
			// Initialize with remote connection if a device key was specified.
			otf = new OTF::OpenThingsFramework(og.options[OPTION_HTP].ival, og.options[OPTION_BDMN].sval, og.options[OPTION_BPRT].ival, otfDeviceKey, false);
			DEBUG_PRINTLN(F("Started OTF with remote connection"));
		} else {
			// Initialize just the local server if no device key was specified.
			otf = new OTF::OpenThingsFramework(og.options[OPTION_HTP].ival);
			DEBUG_PRINTLN(F("Started OTF with just local connection"));
		}
		if(curr_mode == OG_MOD_AP) dns = new DNSServer();
		DEBUG_PRINT(F("server started @ "));
		DEBUG_PRINTLN(og.options[OPTION_HTP].ival);
	}
	if(!updateServer) {
		updateServer = new ESP8266WebServer(8080);
		DEBUG_PRINT(F("update server started"));
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

void on_sta_update(const OTF::Request &req, OTF::Response &res) {
	if(req.isCloudRequest()) otf_send_result(res, HTML_NOT_PERMITTED, "fw update");
	else otf_send_html_P(res, (const __FlashStringHelper *) sta_update_html);
}

void on_ap_update(const OTF::Request &req, OTF::Response &res) {
	otf_send_html_P(res, (const __FlashStringHelper *) ap_update_html);
}

void on_sta_upload_fin() {

	// Verify the device key.
	if(!(updateServer->hasArg("dkey") && (updateServer->arg("dkey") == og.options[OPTION_DKEY].sval))) {
		updateserver_send_result(HTML_UNAUTHORIZED);
		Update.end(false); // Update.reset(); FAB
		return;
	}

	// finish update and check error
	if(!Update.end(true) || Update.hasError()) {
		updateserver_send_result(HTML_UPLOAD_FAILED);
		DEBUG_PRINTLN(F("OTA update failed: "));
		Update.printError(Serial);
		return;
	}

	updateserver_send_result(HTML_SUCCESS);
	//restart_ticker.once_ms(1000, og.restart);
	restart_in(1000);
}

void on_ap_upload_fin() { on_sta_upload_fin(); }

void on_sta_upload() {
	HTTPUpload& upload = updateServer->upload();
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
	HTTPUpload& upload = updateServer->upload();
	if(upload.status == UPLOAD_FILE_START){
		DEBUG_PRINTLN(F("prepare to upload: "));
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

void check_status_ap() {
		static ulong cs_timeout = 0;
		if(millis() > cs_timeout) {
			Serial.print(og.read_distance());
			Serial.print("/");
			Serial.println(OG_FWV);
			cs_timeout = millis() + 2000;
		}
}

bool mqtt_connect_subscribe() {
	static ulong mqtt_subscribe_timeout = 0;
	if(curr_utc_time > mqtt_subscribe_timeout) {
		if (!mqttclient.connected()) {
			DEBUG_PRINT(F("MQTT Not connected- (Re)connect MQTT"));
			boolean ret;
			if(og.options[OPTION_MQUR].sval.length()>0) { // if MQTT user name is defined
				DEBUG_PRINT(F(" (authenticated)"));
				ret = mqttclient.connect(mqtt_id.c_str(), og.options[OPTION_MQUR].sval.c_str(), og.options[OPTION_MQPW].sval.c_str(), (mqtt_topic+"/OUT/STATUS").c_str(), 1, true, "offline");
			}
			else {
				DEBUG_PRINT(F(" [anonymous]"));      
				ret = mqttclient.connect(mqtt_id.c_str(), (mqtt_topic+"/OUT/STATUS").c_str(), 1, true, "offline");
			}
			if(ret) {
				mqttclient.subscribe(mqtt_topic.c_str());
				mqttclient.subscribe((mqtt_topic +"/IN/#").c_str());
				mqttclient.publish((mqtt_topic+"/OUT/STATUS").c_str(), "online", true);
				DEBUG_PRINTLN(F("......Success, Subscribed to MQTT Topic"));
				mqtt_subscribe_timeout = curr_utc_time + 5; // if successful, don't check for 5 seconds
				return true;
			}else {
				DEBUG_PRINTLN(F("......Failed to Connect to MQTT"));
				mqtt_subscribe_timeout = curr_utc_time + 60; // if unsuccessful, try again in 60 seconds
				return false;
			}
		}
	}
	return false;
}

void perform_notify(String s) {
	DEBUG_PRINT(F("Sending Notify to connected systems, value:"));
	DEBUG_PRINTLN(s);
	// Blynk notification
	if(og.options[OPTION_CLD].ival==CLD_BLYNK && Blynk.connected()) {
		DEBUG_PRINTLN(F(" Blynk Notify"));
		Blynk.notify(s);
	}

	// IFTTT notification
	if(og.options[OPTION_IFTT].sval.length()>7) { // key size is at least 8
		DEBUG_PRINTLN(" Sending IFTTT Notification");
		http.begin(httpclient, "http://maker.ifttt.com/trigger/opengarage/with/key/"+og.options[OPTION_IFTT].sval);
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

	if(og.options[OPTION_MQEN].ival>0 && valid_url(og.options[OPTION_MQTT].sval)) {
		if (mqttclient.connected()) {
		    DEBUG_PRINTLN(" Sending MQTT Notification");
		    mqttclient.publish((mqtt_topic + "/OUT/NOTIFY").c_str(),s.c_str()); 
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
		if (noto & OG_NOTIFY_DO) { perform_notify(og.options[OPTION_NAME].sval + " just OPENED!");}
		
		//If the door is set to auto close at a certain hour, ensure if manually opened it doesn't autoshut
		if( (curr_utc_hour == og.options[OPTION_ATIB].ival) && (!automationclose_triggered) ){
			DEBUG_PRINTLN(" Door opened during automation hour, set to not auto-close ");
			automationclose_triggered=true;
		}

	} else if (event == DOOR_STATUS_JUST_CLOSED) {
		justopen_timestamp = 0; // reset time stamp
		if (noto & OG_NOTIFY_DC) { perform_notify(og.options[OPTION_NAME].sval + " just CLOSED!");}

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
		
		// Read SN1 -- ultrasonic sensor
		uint dth = og.options[OPTION_DTH].ival;
		uint vth = og.options[OPTION_VTH].ival;
		bool sn1_status;
		distance = og.read_distance();
		if((distance==0 || distance>500 || !fullbuffer) && og.options[OPTION_SNO].ival!=OG_SNO_2ONLY) {
			// invalid distance value or non full buffer, return immediately except if using SN2 only
			DEBUG_PRINTLN(F("invalid distance or non-full buffer"));
			checkstatus_timeout = curr_utc_time + og.options[OPTION_RIV].ival;
			return; 
		}
		
		sn1_status = (distance>dth)?0:1;
		if(og.options[OPTION_SN1].ival == OG_SN1_SIDE) {
			sn1_status = 1-sn1_status; // reverse logic for side mount
			// for side-mount, we can't decide vehicle status
			vehicle_status = OG_VEH_NOTAVAIL;
		} else {
		  if (vth>0) {
				if(!sn1_status) {
					// if vehicle distance threshold is defined and door is closed (i.e. not blocking view of vehicle)
					// vehicle status can be determined by checking if distance is within bracket [dth, vth]
					vehicle_status = ((distance>dth) && (distance <=vth)) ? OG_VEH_PRESENT:OG_VEH_ABSENT;
				} else { vehicle_status = OG_VEH_UNKNOWN; }	// door is open, blocking view of vehicle
			} else {vehicle_status = OG_VEH_NOTAVAIL;} // vth undefined
		}
		
		// Read SN2 -- optional switch sensor
		sn2_value = og.get_switch();
		byte sn2_status = 0;
		if(og.options[OPTION_SN2].ival == OG_SN2_NC) {	// if SN2 is normally closed type
			sn2_status = sn2_value;
		} else if(og.options[OPTION_SN2].ival == OG_SN2_NO) {	// if SN2 is normally open type
			sn2_status = 1-sn2_value;
		}

		// Process Sensor Logic
		if(og.options[OPTION_SN2].ival==OG_SN2_NONE || og.options[OPTION_SNO].ival==OG_SNO_1ONLY) {
			// if SN2 not installed or logic is SN1 only
			door_status = sn1_status;
		} else if(og.options[OPTION_SNO].ival==OG_SNO_2ONLY) {
			door_status = sn2_status;
		} else if(og.options[OPTION_SNO].ival==OG_SNO_AND) {
			door_status = sn1_status && sn2_status;
		} else if(og.options[OPTION_SNO].ival==OG_SNO_OR) {
			door_status = sn1_status || sn2_status;
		}
		
		// get temperature readings
		og.read_TH_sensor(tempC, humid);
		read_cnt = (read_cnt+1)%100;    
		
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
			l.sn2 = 255;	// use 255 to indicate invalid value
			if(og.options[OPTION_SN2].ival>OG_SN2_NONE) l.sn2 = sn2_value;
			og.write_log(l);

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

			//Mqtt update
			if(og.options[OPTION_MQEN].ival>0 && valid_url(og.options[OPTION_MQTT].sval) && (mqttclient.connected())) {
				DEBUG_PRINTLN(F(" Update MQTT (State Refresh)"));
				if(door_status == DOOR_STATUS_REMAIN_OPEN)  {						// MQTT: If door open...
					mqttclient.publish((mqtt_topic + "/OUT/STATE").c_str(),"OPEN");
					mqttclient.publish(mqtt_topic.c_str(),"Open"); //Support existing mqtt code
					//DEBUG_PRINTLN(curr_utc_time + " Sending MQTT State otification: OPEN");
				} 
				else if(door_status == DOOR_STATUS_REMAIN_CLOSED) {					// MQTT: If door closed...
					mqttclient.publish((mqtt_topic + "/OUT/STATE").c_str(),"CLOSED");
					mqttclient.publish(mqtt_topic.c_str(),"Closed"); //Support existing mqtt code
					//DEBUG_PRINTLN(curr_utc_time + " Sending MQTT State Notification: CLOSED");
				}
				String msg;
				sta_controller_fill_json(msg, false);
				mqttclient.publish((mqtt_topic + "/OUT/JSON").c_str(),msg.c_str());
			}
			// Send status report every 15 seconds: we don't need to send updates frequently if there is no status change.
			checkstatus_report_timeout= curr_utc_time + 15; 
		}
		
		// Process dynamics: automation and notifications
		// report status to Blynk
		if(og.options[OPTION_CLD].ival==CLD_BLYNK && Blynk.connected()) {
			DEBUG_PRINTLN(F(" Update Blynk (State Refresh)"));
			
			static uint old_distance = 0;
			static byte old_door_status = 0xff, old_vehicle_status = 0xff;
			static String old_ip = "";
			static float old_tempC = -100;
			static float old_humid = -100;
			
			// to reduce traffic, only send updated values
			if(distance != old_distance) {  Blynk.virtualWrite(BLYNK_PIN_DIST, distance); old_distance = distance; }
			if(door_status != old_door_status) { (door_status) ? blynk_door.on() : blynk_door.off(); old_door_status = door_status; }
			if(vehicle_status != old_vehicle_status) { (vehicle_status==1) ? blynk_car.on() : blynk_car.off(); old_vehicle_status = vehicle_status; }
			if(old_ip != get_ip()) { Blynk.virtualWrite(BLYNK_PIN_IP, get_ip()); old_ip = get_ip(); }
			// hack to simulate temp humid changes
			if(old_tempC != tempC) { Blynk.virtualWrite(BLYNK_PIN_TEMP, tempC); old_tempC = tempC; }
			if(old_humid != humid) { Blynk.virtualWrite(BLYNK_PIN_HUMID,humid); old_humid = humid; }
		}
		
		process_dynamics(event);
		checkstatus_timeout = curr_utc_time + og.options[OPTION_RIV].ival;
		
	}
}

void time_keeping() {
	static byte ntp_state = OG_NTP_CONFIGURE;
	static ulong prev_millis = 0;
	static ulong ntp_timeout = 0;
	static ulong state_transition_time = 0;
	static uint state_transition_delay = 0;

	ulong current_millis = millis();

	while(current_millis - prev_millis >= 1000) {
		curr_utc_time ++;
		curr_utc_hour = (curr_utc_time % 86400)/3600;
		prev_millis += 1000;
	}

	if(current_millis - state_transition_time < state_transition_delay){
		return;
	}

	state_transition_time = current_millis;
	state_transition_delay = 0;

	switch(ntp_state) {
		case OG_NTP_CONFIGURE: {
			String ntp_server = og.options[OPTION_NTP1].sval;
			if(ntp_server == F("0.0.0.0")){
				DEBUG_PRINTLN("Skipping NTP Configuration");
				state_transition_delay = TIME_SYNC_REFRESH;
				break;
			}
			DEBUG_PRINT(F("NTP1: "));
			if(valid_url(ntp_server)) {
				DEBUG_PRINTLN(ntp_server);
				configTime(0, 0, ntp_server.c_str(), DEFAULT_NTP1, DEFAULT_NTP2);
			} else {
				DEBUG_PRINTLN(DEFAULT_NTP1);
				configTime(0, 0, DEFAULT_NTP1, DEFAULT_NTP2, DEFAULT_NTP3);
			}
			DEBUG_PRINTLN(F("NTP Configured"));
			ntp_state = OG_NTP_UPDATE_TIME;
			state_transition_delay = TIME_SYNC_CONFIG_DELAY;
			break;
		}
		case OG_NTP_UPDATE_TIME: {
			DEBUG_PRINTLN(F("Updating Time"));
			ntp_timeout = current_millis + TIME_SYNC_TIMEOUT;
			ntp_state = OG_NTP_CALL_NTP;
			break;
		}
		case OG_NTP_CALL_NTP: {
			DEBUG_PRINTLN(F("Calling NTP Server"));
			ulong gt = time(nullptr);
			if(gt < TIME_SYNC_ERROR_DATE){
				DEBUG_PRINT(F("NTP Server Error: "));
				// the result of ntp time failed, if we haven't reached the timeout window yet, wait 2 seconds, then try again, once we timeout, wait 60 seconds before trying again.
				if(current_millis > ntp_timeout ){
					DEBUG_PRINTLN(F("TIMEOUT"));
					ntp_state = OG_NTP_UPDATE_TIME;
					state_transition_delay = TIME_SYNC_TIMEOUT_DELAY;
				} else {
					DEBUG_PRINTLN(F("WAIT/RETRY"));
					ntp_state = OG_NTP_CALL_NTP;
					state_transition_delay = TIME_SYNC_RETRY_DELAY;
				}
			}
			else{
				curr_utc_time = gt;
				curr_utc_hour = (curr_utc_time % 86400)/3600;
				DEBUG_PRINT(F("Updated time from NTP: "));
				DEBUG_PRINT(curr_utc_time);
				DEBUG_PRINT(" Hour: ");
				DEBUG_PRINTLN(curr_utc_hour);

				prev_millis = current_millis;

				// if we got a response, re-try after TIME_SYNC_REFRESH milliseconds
				ntp_state = OG_NTP_UPDATE_TIME;
				state_transition_delay = TIME_SYNC_REFRESH;
			}
			break;
		}
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
			otf->on("/",   on_home);
			otf->on("/js", on_ap_scan);
			otf->on("/cc", on_ap_change_config);
			otf->on("/jt", on_ap_try_connect);
			otf->on("/db", on_ap_debug);
			// FIXME get update ap updates working.
			otf->on("/update", on_ap_update, OTF::HTTP_GET);
			updateServer->on("/update", HTTP_POST, on_ap_upload_fin, on_ap_upload);
			otf->on("/resetall",on_reset_all);
			otf->onMissingPage(on_home);
			updateServer->begin();
			DEBUG_PRINTLN(F("Web Server endpoints (AP mode) registered"));
			og.state = OG_STATE_CONNECTED;
			DEBUG_PRINTLN(WiFi.softAPIP());
			connecting_timeout = 0;
		} else {
			led_blink_ms = LED_SLOW_BLINK;
			DEBUG_PRINT(F("Attempting to connect to SSID: "));
			DEBUG_PRINTLN(og.options[OPTION_SSID].sval.c_str());
			WiFi.mode(WIFI_STA);
			start_network_sta(og.options[OPTION_SSID].sval.c_str(), og.options[OPTION_PASS].sval.c_str());
			og.config_ip();
			og.state = OG_STATE_CONNECTING;
			connecting_timeout = millis() + 60000;
		}
		break;

	case OG_STATE_TRY_CONNECT:
		led_blink_ms = LED_SLOW_BLINK;
		DEBUG_PRINT(F("Attempting to connect to SSID: "));
		DEBUG_PRINTLN(og.options[OPTION_SSID].sval.c_str());
		start_network_sta_with_ap(og.options[OPTION_SSID].sval.c_str(), og.options[OPTION_PASS].sval.c_str());
		og.config_ip();
		og.state = OG_STATE_CONNECTED;
		break;
		
	case OG_STATE_CONNECTING:
		if(WiFi.status() == WL_CONNECTED) {
			DEBUG_PRINT(F("Wireless connected, IP: "));
			DEBUG_PRINTLN(WiFi.localIP());

			time_keeping();
			otf->on("/", on_home);
			otf->on("/jc", on_sta_controller);
			otf->on("/jo", on_sta_options);
			otf->on("/jl", on_sta_logs);
			otf->on("/vo", on_sta_view_options);
			otf->on("/vl", on_sta_view_logs);
			otf->on("/cc", on_sta_change_controller);
			otf->on("/co", on_sta_change_options);
			otf->on("/db", on_sta_debug);
			// FIXME get sta updates working.
			otf->on("/update", on_sta_update, OTF::HTTP_GET);
			updateServer->on("/update", HTTP_POST, on_sta_upload_fin, on_sta_upload);
			otf->on("/clearlog", on_clear_log);
			otf->on("/resetall",on_reset_all);
			updateServer->begin();
			DEBUG_PRINTLN(F("Web Server endpoints (STA mode) registered"));

			// request mDNS host name
			String host = og.options[OPTION_HOST].sval;
			if(host.length()==0) host=get_ap_ssid(); // if undefined, AP name as host
			if(MDNS.begin(host.c_str(), WiFi.localIP())) {
				DEBUG_PRINT(F("MDNS registered: "));
				DEBUG_PRINT(host);
				DEBUG_PRINTLN(F(".local"));
				
				MDNS.addService("http", "tcp", og.options[OPTION_HTP].ival);
				//DEBUG_PRINTLN(og.options[OPTION_HTP].ival);
			}

			if(og.options[OPTION_CLD].ival==CLD_BLYNK) {
				DEBUG_PRINTLN(F("Attempt to connect to Blynk:"));
				DEBUG_PRINT(og.options[OPTION_BDMN].sval);
				DEBUG_PRINT(":");
				DEBUG_PRINTLN(og.options[OPTION_BPRT].ival);
				Blynk.config(og.options[OPTION_AUTH].sval.c_str(), og.options[OPTION_BDMN].sval.c_str(), (uint16_t) og.options[OPTION_BPRT].ival); // use the config function
				if(Blynk.connect()) {DEBUG_PRINTLN(F("Blynk connected"));}
				else {DEBUG_PRINTLN(F("Blynk failed!"));}
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
		if(otf) otf->loop();
		if(updateServer) updateServer->handleClient();
		break;
		
	case OG_STATE_CONNECTED: //THIS IS THE MAIN LOOP
		if(curr_mode == OG_MOD_AP) {
			dns->processNextRequest();
			otf->loop();
			updateServer->handleClient();
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
		if(WiFi.status() == WL_CONNECTED) {
			MDNS.update();
			time_keeping();
			check_status(); //This checks the door, sends info to services and processes the automation rules
			otf->loop();
			updateServer->handleClient();

			if(og.options[OPTION_CLD].ival==CLD_BLYNK)
			  Blynk.run();

			//Handle MQTT
			if(og.options[OPTION_MQEN].ival>0 && valid_url(og.options[OPTION_MQTT].sval)) { // if enabled and mqtt server looks valid
				if (!mqttclient.connected()) {
					mqtt_id = get_ap_ssid();
					mqtt_topic = og.options[OPTION_MQTP].sval;
					if(mqtt_topic.length()==0) mqtt_topic = og.options[OPTION_NAME].sval;
					mqttclient.setServer(og.options[OPTION_MQTT].sval.c_str(), og.options[OPTION_MQPT].ival);
					mqttclient.setCallback(mqtt_callback); 		
					mqtt_connect_subscribe();
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

