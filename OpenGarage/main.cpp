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

#include "device_key_policy.h"
#include "setup_window.h"
#include <memory>
#include <new>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <OpenThingsFramework.h>
#include <Request.h>
#include <Response.h>

#include "json_escape.h"
#include "etag_match.h"
#include "form_params.h"
#include "checked_json.h"
#include "update_auth.h"
#include "firmware_update.h"
#include "local_time.h"
#include "network_policy.h"
#include "smtp_exchange.h"
#include "pitches.h"
#include "OpenGarage.h"
#include "espconnect.h"

OpenGarage og;
OTF::OpenThingsFramework *otf = NULL;
ESP8266WebServer *updateServer = NULL;
DNSServer *dns = NULL;

static Ticker aux_ticker;
static Ticker ip_ticker;
static Ticker restart_ticker;
static uint32_t setup_started_ms=0;
static bool setup_expired=false;


static WiFiClient wificlient;
static WiFiClient httpclient;
PubSubClient mqttclient(wificlient);
String mqtt_topic;
String mqtt_id;
static uint32_t mqtt_json_failures = 0;
static uint32_t mqtt_json_bytes = 0;
static bool mqtt_buffer_ready = false;
// Retain the small default buffer until MQTT is enabled; reuse after growth.
static bool ensure_mqtt_buffer() {
	if (!mqtt_buffer_ready) mqtt_buffer_ready = mqttclient.setBufferSize(2048);
	return mqtt_buffer_ready;
}
static uint32_t mqtt_retained_dropped = 0;
static og_network::MqttRecovery mqtt_retry;
static og_network::Backoff email_retry(60000, 600000), ifttt_retry(60000, 600000);
static String pending_notification;
static uint32_t notification_ms = 0, notification_dropped = 0;
static uint32_t email_failures = 0;
static char email_last[96] = "Not attempted";
static uint32_t local_action_ms = 0;
static bool local_action_seen = false;
static uint32_t wifi_door_event_ms = 0;
static bool wifi_door_event_seen = false;
static bool update_authorized = false, update_failed = false, update_started = false;
static bool update_file_seen = false;

static String scanned_ssids;
static byte read_cnt = 0;
static uint distance = 0;
static byte sn2_value = 0;
static float tempC = 0;
static float humid = 0;
static byte door_status = DOOR_STATUS_UNKNOWN; //door_status enum
static byte last_door_status = DOOR_STATUS_UNKNOWN;
static byte secplus_door_status = DOOR_STATUS_UNKNOWN;
static byte door_status_hist = 0;
static bool light_status = 0;
static bool lock_status = 0;
static bool obstruction_status = 0;
static uint16_t opening_count = 0;
static int vehicle_status = OG_VEH_ABSENT;
static uint led_blink_ms = LED_FAST_BLINK;
static og_local::OpenDuration open_duration;
static og_local::DailyRule daily_rule;
static og_local::Clock local_clock;
static byte curr_mode;
extern bool fullbuffer;
// this is one byte storing the door status histogram
// maximum 8 bits
static ulong start_utc_time = 0;
static ulong curr_utc_time = 0;
static ulong curr_utc_hour= 0;
static HTTPClient http;
static bool light_blink_enabled = true;
// security+ objects
SecPlus1::Garage secplus1_garage(PIN_SW_RX, PIN_SW_TX);
#include "secplus2_identity_store.h"
#include "body_device_key.h"
#include "health_stats.h"
#include "audio_timing.h"
#include "crash_diagnostics.h"
struct BuzzerSink {
	void note(unsigned frequency, unsigned duration) { tone(PIN_BUZZER, frequency, duration); }
	void silence() { noTone(PIN_BUZZER); }
};
static BuzzerSink buzzer;
static og_audio::Warning<BuzzerSink> warning_audio(buzzer);
static og_audio::Melody<BuzzerSink> startup_audio(buzzer);
static bool ip_audio_active=false, setup_reboot_pending=false;
static byte ip_note=0, ip_digit=0;

void cancel_diagnostic_audio() {
	startup_audio.cancel();
	ip_ticker.detach();
	if (ip_audio_active) buzzer.silence();
	ip_audio_active=false; ip_note=ip_digit=0;
}
void start_tune(og_audio::Tune tune) {
	if (!og.alarm && !ip_audio_active) startup_audio.start(millis(),tune);
}
static HealthStats health_stats;
struct LoopMeasurement {
	uint32_t started=micros();
	LoopMeasurement() { health_stats.enter(millis()); }
	~LoopMeasurement() { health_stats.finish(started,micros(),ESP.getFreeHeap()); }
};
static uint32_t secplus2_client_id = 0;
static bool secplus2_identity_ready = false;
static bool identity_restart_pending = false;
SecPlus2::Garage secplus2_garage(0, PIN_SW_RX, PIN_SW_TX);

void do_setup();

void otf_send_html_P(OTF::Response &res, const __FlashStringHelper *content) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("text/html"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*")); // from esp8266 2.4 this has to be sent explicitly
	res.writeHeader(F("Content-Length"), strlen_P((char *) content));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(content, strlen_P((char*)content));
	DEBUG_PRINT(strlen_P((char *) content));
	DEBUG_PRINTLN(F(" bytes sent."));
}

void otf_send_compressed_P(OTF::Response& res, const __FlashStringHelper* content, size_t len) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("text/html; charset=utf-8"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*")); // from esp8266 2.4 this has to be sent explicitly
	res.writeHeader(F("Content-Length"), len);
	res.writeHeader(F("Vary"), F("Accept-Encoding"));
	res.writeHeader(F("Content-Encoding"), F("gzip"));
	res.writeHeader(F("Cache-Control"), F("no-cache"));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(content, len);
	DEBUG_PRINT(len);
	DEBUG_PRINTLN(F(" bytes sent."));
}

// Station UI page with browser caching. `etag` is the page's quoted content ID from htmls.h: the
// browser may reuse its copy for 5 minutes, then revalidates and gets a bodyless 304 while the page
// is unchanged. A firmware update changes the ID; the UI also compares it with "ui" in /jc and
// reloads itself. OTF lowercases header names. Its local server keeps If-None-Match only from the
// OTF revision below onward; 0.2.6 dropped it, so only OTC-forwarded requests could revalidate.
// Callers register these routes for GET only, so a 304 never answers another method.
void otf_send_cached_page_P(const OTF::Request &req, OTF::Response &res, const __FlashStringHelper* content, size_t len, const __FlashStringHelper* etag) {
	char tag[24];
	strncpy_P(tag, (PGM_P)etag, sizeof(tag) - 1);
	tag[sizeof(tag) - 1] = 0;
	const char *inm = req.getHeader(F("if-none-match"));
	const bool unchanged = inm && etag_list_matches(inm, tag);
	res.writeStatus(unchanged ? 304 : 200, unchanged ? F("Not Modified") : F("OK"));
	res.writeHeader(F("ETag"), etag);
	res.writeHeader(F("Cache-Control"), F("private, max-age=300"));
	res.writeHeader(F("Vary"), F("Accept-Encoding"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	if (unchanged) {
		res.writeHeader(F("Connection"), F("close"));
		// End the header block. A 304 has no body, and no Content-Length: if present it would
		// have to describe the 200 representation (RFC 9110 8.6).
		res.writeBodyData("", 0);
		return;
	}
	res.writeHeader(F("Content-Type"), F("text/html; charset=utf-8"));
	res.writeHeader(F("Content-Length"), len);
	res.writeHeader(F("Content-Encoding"), F("gzip"));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(content, len);
}

void otf_send_json(OTF::Response &res, const String &json) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("application/json"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*")); // from esp8266 2.4 this has to be sent explicitly
	res.writeHeader(F("Content-Length"), json.length());
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(json.c_str(), json.length());
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

template<typename Message>
static void updateserver_send_result_impl(byte code, Message item) {
	String json = F("{\"result\":");
	json += code;
	json += F(",\"item\":\"");
	if (item) json += item;
	json += F("\"}");
	updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
	updateServer->send(200, "application/json", json);
}

void updateserver_send_result(byte code, const char* item = NULL) {
	updateserver_send_result_impl(code, item);
}
void updateserver_send_result(byte code, const __FlashStringHelper* item) {
	updateserver_send_result_impl(code, item);
}

// this is simplified version to check if MQTT / NTP server is valid domain name
bool valid_url(String s) {
	return (s.length()>0 && s[0]!='-' && s.indexOf('.')>0);
}

String ipString;

static void append_age(DebugJson &json, uint32_t age) {
	if (age==UINT32_MAX) json += F("-1");
	else json += age;
}

static void append_health_json(DebugJson &json) {
	json += F(",\"active_security_protocol\":"); json += og.options[OPTION_SECV].ival;
	json += F(",\"free_heap\":"); json += ESP.getFreeHeap();
	json += F(",\"largest_free_heap_block\":"); json += ESP.getMaxFreeBlockSize();
	json += F(",\"build_time\":\""); json += F(__TIME__); json += F("\"");
	json += F(",\"otf_revision\":\"7ecee5a8a8d6614b5e005f51fcb636128ad88640\"");
	json += F(",\"reset_reason\":\""); json += ESP.getResetReason(); json += F("\"");
	json += F(",\"reset_reason_code\":"); json += ESP.getResetInfoPtr()->reason;
	json += F(",\"uptime_s\":"); json += health_stats.uptime_s();
	json += F(",\"min_free_heap\":");
	if (health_stats.min_heap()==UINT32_MAX) json += ESP.getFreeHeap();
	else json += health_stats.min_heap();
	json += F(",\"max_loop_us\":"); json += health_stats.max_loop_us();
	// Untouched continuation-stack watermark since boot (bytes), not current
	// free stack or the separate SDK/interrupt stack. Never repaint during soak.
	json += F(",\"min_free_cont_stack\":"); json += ESP.getFreeContStack();
	if (!og.has_swrx) return;
	if (og.options[OPTION_SECV].ival == 1) {
		json += F(",\"secplus1_parity_errors\":"); json += secplus1_garage.get_parity_errors();
		json += F(",\"secplus1_invalid_frames\":"); json += secplus1_garage.get_invalid_frames();
		json += F(",\"secplus1_rx_overflows\":"); json += secplus1_garage.get_overflows();
		json += F(",\"secplus1_tx_deferrals\":"); json += secplus1_garage.get_tx_deferrals();
		json += F(",\"secplus1_expired_commands\":"); json += secplus1_garage.get_expired_commands();
		json += F(",\"secplus1_door_age_ms\":"); append_age(json,secplus1_garage.get_door_age());
		json += F(",\"secplus1_light_lock_age_ms\":"); append_age(json,secplus1_garage.get_light_lock_age());
#ifdef OG_SEC1_DIAGNOSTICS
		const auto &gap=secplus1_garage.diagnostic_gap();
		const auto &gd=gap.diagnostic;
		const auto &td=secplus1_garage.diagnostic;
		json += F(",\"secplus1_diag\":{\"exchanges\":"); json += gd.exchanges;
		json += F(",\"resets\":"); json += gd.resets;
		json += F(",\"bad_latency\":"); json += gd.bad_latency;
		json += F(",\"zero_latency\":"); json += gd.zero_latency;
		json += F(",\"bad_interval\":"); json += gd.bad_interval;
		json += F(",\"last_latency_ms\":"); json += gd.latency;
		json += F(",\"last_interval_ms\":"); json += gd.interval;
		json += F(",\"learned_count\":"); json += gap.learned_count();
		json += F(",\"max_learned_count\":"); json += gd.max_count;
		json += F(",\"minimum_ms\":"); json += gap.learned_minimum();
		json += F(",\"maximum_ms\":"); json += gap.learned_maximum();
		json += F(",\"reply_age_ms\":"); append_age(json,gap.reply_age(millis()));
		json += F(",\"loops\":"); json += td.loops;
		json += F(",\"gap_ready_loops\":"); json += td.gap_ready_loops;
		json += F(",\"last_block\":"); json += td.last_block;
		json += F(",\"blocked\":[");
		for(unsigned i=0;i<9;++i) { if(i) json += ','; json += td.blocked[i]; }
		json += F("]}");
#endif
	} else if (og.options[OPTION_SECV].ival == 2) {
		json += F(",\"secplus2_status_age_ms\":"); append_age(json,secplus2_garage.get_status_age());
		json += F(",\"secplus2_query_failures\":"); json += secplus2_garage.get_query_failures();
		json += F(",\"secplus2_action_write_failures\":"); json += secplus2_garage.get_write_failures();
		json += F(",\"secplus2_tx_deferrals\":"); json += secplus2_garage.get_tx_deferrals();
		json += F(",\"secplus2_expired_commands\":"); json += secplus2_garage.get_expired_commands();
		json += F(",\"secplus2_control_fault\":"); json += secplus2_garage.controls_faulted() ? 1 : 0;
		json += F(",\"secplus2_recovery\":\""); json += secplus2_garage.recovery_status(); json += F("\"");
	}
}

void report_ip() {
	static uint notes[] = {NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4};
	if (!ip_audio_active || og.alarm) return;
	byte &note=ip_note, &digit=ip_digit;

	if(digit == ipString.length()) { // play ending note
		og.play_note(NOTE_C6); digit++; note=0;
		ip_ticker.once_ms(1000, report_ip);
		return;
	} else if(digit == ipString.length()+1) { // end
		og.play_note(0); note=0; digit=0;
		ip_audio_active=false;
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

static bool setup_room(uint32_t required) {
 return curr_mode != OG_MOD_AP || (!setup_expired && og_setup::may_start(millis(),setup_started_ms,required));
}

void restart_in(uint32_t ms) {
	if(og.state != OG_STATE_WAIT_RESTART) {
		og.state = OG_STATE_WAIT_RESTART;
		DEBUG_PRINTLN(F("Prepare to restart..."));
		restart_ticker.once_ms(ms, og.restart);
	}
}

// Both the provisioning poll and the normal loop can notice association first.
// Persist once, play the complete success tune, then restart to enter STA mode.
static bool setup_save_failed=false;
bool complete_ap_setup() {
	if(!setup_room(0) || !og.options[OPTION_KSET].ival)return false;
	if(setup_save_failed)return false;
	if (setup_reboot_pending || og.options[OPTION_MOD].ival==OG_MOD_STA) return true;
	og.options[OPTION_MOD].ival=OG_MOD_STA;
	if(!og.options_save()) {og.options[OPTION_MOD].ival=OG_MOD_AP;setup_save_failed=true;return false;}
	setup_reboot_pending=true;
	start_tune(og_audio::Tune::SETUP_SUCCESS);
	return true;
}

static void send_sta_ui(const OTF::Request &req, OTF::Response &res) {
	otf_send_cached_page_P(req, res, (const __FlashStringHelper*)sta_ui_html_gz, sta_ui_html_gz_len, F("\"" STA_UI_HTML_GZ_ID "\""));
}

void on_home(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) {
		otf_send_compressed_P(res, (const __FlashStringHelper*)ap_home_html_gz, ap_home_html_gz_len);
	} else {
		send_sta_ui(req, res);
	}
}

// Home, Options and Log are one page (sta_ui.html); it opens the view that matches the path.
void on_sta_view_options(const OTF::Request &req, OTF::Response &res) { send_sta_ui(req, res); }
void on_sta_view_logs(const OTF::Request &req, OTF::Response &res) { send_sta_ui(req, res); }

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

void sta_controller_fill_json(String& json) {
	json.reserve(STRING_RESERVE_SIZE);
	json = "";
	json += F("{\"dist\":");
	json += distance;
	if(og.options[OPTION_SN2].ival>OG_SN2_NONE) {
		json += F(",\"sn2\":");
		json += sn2_value;
	}
	json += F(",\"secv\":");
	json += og.options[OPTION_SECV].ival;
	json += F(",\"door\":");
	json += door_status;
	json += F(",\"vehicle\":");
	json += vehicle_status;
	json += F(",\"rcnt\":");
	json += read_cnt;
json += F(",\"fwv\":");
	json += og.options[OPTION_FWV].ival;
	json += F(",\"ui\":\"" STA_UI_HTML_GZ_ID "\"");  // station UI content ID: an open page reloads when it changes
	json += F(",\"has_swrx\":");
	json += og.has_swrx;
	if(og.has_swrx) {
		if(og.options[OPTION_SECV].ival>=1) {
			json += F(",\"light\":");
			json += light_status;
			json += F(",\"lock\":");
			json += lock_status;
			json += F(",\"obstruct\":");
			json += obstruction_status;
			if(og.options[OPTION_SECV].ival==2) {
				const bool valid=secplus2_garage.status_valid();
				json += F(",\"door_valid\":"); json += valid ? 1 : 0;
				json += F(",\"light_lock_valid\":"); json += valid ? 1 : 0;
				json += F(",\"obstruct_valid\":"); json += valid ? 1 : 0;
				json += F(",\"recovery_status\":\"");
				json += secplus2_garage.recovery_status(); json += F("\"");
				json += F(",\"control_fault\":"); json += secplus2_garage.controls_faulted() ? 1 : 0;
				json += F(",\"nopenings\":");
				json += opening_count;
			}
			if(og.options[OPTION_SECV].ival==1) {
				json += F(",\"door_valid\":");
				json += secplus1_garage.door_valid() ? 1 : 0;
				json += F(",\"light_lock_valid\":");
				json += secplus1_garage.light_lock_valid() ? 1 : 0;
				json += F(",\"obstruct_valid\":");
				json += secplus1_garage.obstruction_valid() ? 1 : 0;
				json += F(",\"panel_status\":\"");
				json += secplus1_garage.get_panel_description();
				json += F("\",\"command_status\":\"");
				json += secplus1_garage.get_action_result();
				json += F("\"");
				json += F(",\"pemu\":");
				json += secplus1_garage.get_panel_emu_status();
			}
		}
	}
	json += F(",\"name\":\"");
	append_json_escaped(json, og.options[OPTION_NAME].sval);
	json += F("\",\"mac\":\"");
	json += get_mac();
	json += F("\",\"cid\":");
	json += ESP.getChipId();
	json += F(",\"rssi\":");
	json += (int16_t)WiFi.RSSI();
	json += F(",\"cld\":");
	byte cld = og.options[OPTION_CLD].ival;
	json += cld;
	if(cld>CLOUD_NONE) {
		json += F(",\"clds\":");
		if(cld==CLOUD_OTC) json += (otf->getCloudStatus());
		else json += "0";
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

static void append_debug_details(const OTF::Request &req, DebugJson &json) {
	const char *verbose = req.getQueryParameter("verbose");
	if (!verbose || strcmp(verbose, "1") != 0) return;
	if (og.has_swrx && og.options[OPTION_SECV].ival == 2) {
		json += F(",\"secplus2_client_id\":\"0x");
		json += String(secplus2_client_id, HEX);
		json += F("\"");
		json += F(",\"secplus2_identity_ready\":");
		json += secplus2_identity_ready ? F("true") : F("false");
	}
	append_health_json(json);
	json += F(",\"time_valid\":"); json += local_clock.valid ? 1 : 0;
	json += F(",\"mqtt_json_failures\":"); json += mqtt_json_failures;
	json += F(",\"mqtt_json_bytes\":"); json += mqtt_json_bytes;
	json += F(",\"mqtt_buffer_ready\":"); json += mqtt_buffer_ready ? 1 : 0;
	json += F(",\"mqtt_retained_dropped\":"); json += mqtt_retained_dropped;
	json += F(",\"notification_dropped\":"); json += notification_dropped;
	json += F(",\"email_failures\":"); json += email_failures;
	json += F(",\"email_last\":\""); append_json_escaped(json,String(email_last)); json += F("\"");
	json += F(",\"wifi_recovery\":\""); json += station_recovery.status(); json += F("\"");
	json += F(",\"wifi_attempts\":"); json += station_recovery.attempts();
	json += F(",\"wifi_timeouts\":"); json += station_recovery.timeouts();
	json += F(",\"wifi_disconnects\":"); json += station_recovery.disconnects();
	json += F(",\"wifi_retry_in_ms\":"); json += station_recovery.retry_in_ms(millis());
	json += F(",\"wifi_disconnect_reason\":"); json += station_disconnect_reason;
	json += F(",\"wifi_selection\":\""); json += station_selection_status(); json += F("\"");
	json += F(",\"wifi_scans\":"); json += station_selection.scans();
	json += F(",\"wifi_roams\":"); json += station_selection.roams();
	json += F(",\"wifi_smoothed_rssi\":"); json += station_selection.smoothed();
	json += F(",\"wifi_channel\":"); json += WiFi.channel();
	json += F(",\"wifi_phy\":"); json += int(WiFi.getPhyMode());
	json += F(",\"config_status\":"); json += og.config_status;
	json += F(",\"config_locked\":"); json += og.config_locked?1:0;
	const char *crash = req.getQueryParameter("crash");
	og_crash::append_json(json, crash ? (unsigned)atoi(crash) : 0);
}

static void send_debug_json(OTF::Response &res, DebugJson &json) {
    if (json.good()) { otf_send_json(res, json); return; }
    json.discard();
    res.writeStatus(503);
    res.writeHeader(F("Content-Type"), F("application/json"));
    res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
    res.writeHeader(F("Connection"), F("close"));
    res.writeHeader(F("Content-Length"), sizeof("{\"result\":48,\"item\":\"Not enough memory for diagnostics\"}")-1);
    res.writeBodyData(F("{\"result\":48,\"item\":\"Not enough memory for diagnostics\"}"),
        sizeof("{\"result\":48,\"item\":\"Not enough memory for diagnostics\"}")-1);
}

void on_sta_debug(const OTF::Request &req, OTF::Response &res) {
	DebugJson json;
	json.reserve(STRING_RESERVE_SIZE);
	json += F("{");
	json += F("\"rcnt\":");
	json += read_cnt;
	json += F(",\"fwv\":");
	json += og.options[OPTION_FWV].ival;
	json += F(",\"name\":\"");
	append_json_escaped(json, og.options[OPTION_NAME].sval);
	json += F("\",\"mac\":\"");
	json += get_mac();
	json += F("\",\"mqtt_topic\":\"");
	append_json_escaped(json, mqtt_topic);
	json += F("\",\"devip\":\"");
	json += WiFi.localIP().toString();
	json += F("\",\"cid\":");
	json += ESP.getChipId();
	append_debug_details(req, json);
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
	json += F(",\"has_swrx\":");
	json += og.has_swrx;
	json += F("}");
	send_debug_json(res, json);
}

void on_sta_logs(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("application/json"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	// One pass over the log: local HTTP ends at connection close; OTC ends at
	// its response-stream terminator. No full JSON String or counting pass.
	res.writeHeader(F("Connection"), F("close"));
	if (!res.isValid()) return;

	struct EscapedOutput {
		OTF::Response &response;
		void operator+=(const char *s) { response.writeBodyData(s, strlen(s)); }
		void operator+=(char c) { response.writeBodyData(&c, 1); }
	} escaped{res};
	res.writeBodyData(F("{\"name\":\""), 9);
	append_json_escaped(escaped, og.options[OPTION_NAME].sval);
	const bool extra_column = og.options[OPTION_SN2].ival > OG_SN2_NONE;
	const uint count = og.options[OPTION_LSZ].ival;
	// Worst-case metadata is 81 bytes with three UINT32_MAX timestamps.
	// Reuse this fixed buffer for each row, including corrupt maximum fields.
	char chunk[112];
	snprintf_P(chunk, sizeof(chunk), PSTR("\",\"starttime\":%lu,\"time\":%lu,\"uptime\":%lu,\"ncols\":%u,\"logs\":["),
		(unsigned long)start_utc_time, (unsigned long)curr_utc_time,
		(unsigned long)uint32_t(local_clock.uptime_ms / 1000), extra_column ? 4U : 3U);
	res.writeBodyData(chunk, strlen(chunk));

	struct LogReader {
		bool opened;
		~LogReader() { if (opened) og.read_log_end(); }
	} reader{og.read_log_start()};
	bool first = true;
	LogStruct l;
	if (reader.opened) {
		for (uint i=0; i<count && res.isValid(); ++i) {
			if (!og.read_log_next(l)) break;
			if (!l.tstamp) continue;
			if (extra_column)
				snprintf_P(chunk, sizeof(chunk), PSTR("%s[%lu,%u,%u,%u]"), first ? "" : ",",
					(unsigned long)l.tstamp, l.status, l.dist, (unsigned)l.sn2);
			else
				snprintf_P(chunk, sizeof(chunk), PSTR("%s[%lu,%u,%u]"), first ? "" : ",",
					(unsigned long)l.tstamp, l.status, l.dist);
			res.writeBodyData(chunk, strlen(chunk));
			first = false;
		}
	}
	res.writeBodyData(F("]}"), 2);
}

// The device key is required on every path, OTC included. (Before 1.2.7 an OTC request was
// trusted without it, following Blynk's token-only model; the OTC token alone no longer
// authorizes changes, matching OpenSprinkler.)
static bool device_key_matches(const char *dkey) {
	return !og.config_locked && dkey != NULL && strcmp(dkey, og.options[OPTION_DKEY].sval.c_str()) == 0;
}

bool verify_device_key(const OTF::Request &req) {
	return device_key_matches(req.getQueryParameter("dkey"));
}

void on_reset_all(const OTF::Request &req, OTF::Response &res){
	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}

	if(!og.options_reset()){otf_send_result(res, HTML_UPLOAD_FAILED, "save");return;}
	og.state = OG_STATE_RESET;
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

// POST only. Keep credentials out of URLs; the key is required over OTC too, as everywhere.
void on_regenerate_secplus2_id(const OTF::Request &req, OTF::Response &res) {
	if (og.config_locked || !body_device_key_matches(req.getBody(), req.getBodyLength(), og.options[OPTION_DKEY].sval.c_str())) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}
	if (og.options[OPTION_SECV].ival != 2) {
		otf_send_json(res, F("{\"result\":0,\"message\":\"Select and save Security+ 2.0 first.\"}"));
		return;
	}
	if (identity_restart_pending || og.state != OG_STATE_CONNECTED || og.alarm || secplus2_garage.command_pending()) {
		otf_send_json(res, F("{\"result\":0,\"message\":\"Device busy. Wait for the pending action to finish, then retry.\"}"));
		return;
	}
	// No pending actuation: stop the old identity before touching storage. Inhibit
	// all further work until reboot, including on a failed/uncertain flash write.
	identity_restart_pending = true;
	secplus2_garage.stop();
	Secplus2IdentityStore store;
	uint32_t replacement = 0;
	const bool saved = og_identity::load(store, []() -> uint32_t { return ESP.random(); }, true, replacement);
	if (saved) {
		String json = F("{\"result\":1,\"client_id\":\"0x");
		json += String(replacement, HEX);
		json += F("\",\"message\":\"New client ID saved. Restarting; WiFi and other settings are unchanged.\"}");
		otf_send_json(res, json);
	} else {
		otf_send_json(res, F("{\"result\":0,\"message\":\"Could not verify the new ID. Restarting; check identity diagnostics before using controls.\"}"));
	}
	restart_in(1000);
}

void on_clear_log(const OTF::Request &req, OTF::Response &res) {
	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}
	og.log_reset();
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void secplus_update_door(SecPlusCommon::DoorStatus door_state) {
	switch (door_state) {
		case SecPlusCommon::DoorStatus::OPEN:
			secplus_door_status = DOOR_STATUS_OPEN;
			break;
		case SecPlusCommon::DoorStatus::CLOSED:
			secplus_door_status = DOOR_STATUS_CLOSED;
			break;
		case SecPlusCommon::DoorStatus::STOPPED:
			secplus_door_status = DOOR_STATUS_STOPPED;
			break;
		case SecPlusCommon::DoorStatus::OPENING:
			secplus_door_status = DOOR_STATUS_OPENING;
			break;
		case SecPlusCommon::DoorStatus::CLOSING:
			secplus_door_status = DOOR_STATUS_CLOSING;
			break;
		default:
			secplus_door_status = DOOR_STATUS_UNKNOWN;
	}
}

void secplus1_state_callback(SecPlus1::state_struct_t state) {
	secplus_update_door(state.door_state);
	light_status = state.light_state;
	lock_status = state.lock_state;
	obstruction_status = state.obstruction_state;
}

void secplus2_state_callback(SecPlus2::state_struct_t state) {
	secplus_update_door(state.door_state);
	light_status = state.light_state;
	lock_status = state.lock_state;
	obstruction_status = state.obstruction_state;
	opening_count = state.openings;
}

int run_auto_detect() {
	secplus2_garage.stop();
	secplus1_garage.stop(); // finish any possibly dispatched Sec+ 1.0 release tail
	// Try Sec+ 2.0 first
	if (secplus2_garage.detect()) {
		return 2;
	}
	// If that fails, try Sec+ 1.0
	if (secplus1_garage.detect()) {
		return 1;
	}
	return 0; // Detected nothing
}

// Restart the configured protocol as at boot. Both protocols share one software serial port and
// each detect() re-creates it for itself, so after detection it belongs to whichever protocol was
// tried last (Security+ 1.0): a Security+ 2.0 unit would stop monitoring until reboot. With no
// protocol, release the port and return its TX pin (the relay pin) to relay-low.
static void restart_configured_protocol() {
	secplus2_garage.stop();
	secplus1_garage.stop();
	switch (og.options[OPTION_SECV].ival) {
		case 2:
			secplus2_garage.begin();
			secplus2_garage.reset_state();
			secplus_door_status = DOOR_STATUS_UNKNOWN;
			secplus2_garage.enable_callback(secplus2_state_callback);
			break;
		case 1:
			secplus1_garage.begin();
			secplus1_garage.reset_state();
			secplus_door_status = DOOR_STATUS_UNKNOWN;
			secplus1_garage.enable_callback(secplus1_state_callback);
			break;
		default:
			SecPlusCommon::delete_sw_serial();
			digitalWrite(PIN_RELAY, LOW);
			pinMode(PIN_RELAY, OUTPUT);
	}
}

static bool auto_detect_busy();

// Security+ 2.0 probing can move a Security+ 1.0 door. Only first-use AP
// detection is keyless; established units require their existing key in every mode.
void on_auto_detect(const OTF::Request &req, OTF::Response &res) {
	if (og.config_locked || ((curr_mode != OG_MOD_AP || og.options[OPTION_KSET].ival) && !verify_device_key(req))) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}
	if(!setup_room(og_setup::SETUP_RUNWAY_MS)) {otf_send_result(res, HTML_NOT_PERMITTED, "setup_timeout");return;}
	if (auto_detect_busy()) {
		otf_send_result(res, HTML_NOT_PERMITTED, "busy");
		return;
	}
	int detected_version = run_auto_detect();
	restart_configured_protocol();
	String json = "{\"version\": " + String(detected_version) + "}";
	otf_send_json(res, json);
}

static void hold_network_delivery() {
	local_action_seen = true; local_action_ms = millis();
}
static bool network_delivery_busy() {
	// Keep blocking connection attempts/notifications out of the short melody.
	return startup_audio.active() || og.alarm || update_started || identity_restart_pending || setup_reboot_pending ||
		secplus2_garage.command_pending() ||
		(local_action_seen && uint32_t(millis()-local_action_ms)<5000);
}

// Detection stops both protocols (discarding unsent door actions) and blocks the loop for up to
// ~12.5 s, so it is refused while anything time-critical is in progress: a warning countdown, a
// pending or recent door command (Security+ 1.0 exposes no pending flag; the 5 s window covers it),
// door motion, a firmware update or a scheduled restart.
static bool auto_detect_busy() {
	return network_delivery_busy() || firmware_update.busy() || og.state == OG_STATE_WAIT_RESTART ||
		door_status == DOOR_STATUS_OPENING || door_status == DOOR_STATUS_CLOSING;
}

void performDoorAction(uint8_t action, bool force_alarm_off = false) {
	if(og.config_locked)return;
	if (firmware_update.busy() || firmware_update.phase()==FirmwareUpdatePhase::Success) return;
	if (identity_restart_pending) return;
	// Check if the requested action is valid based on the current door state
	bool isValidAction = false;
	if((og.options[OPTION_SECV].ival == 2) || // For Sec+ 2.0, open/close commands are always valid
		(action == ACTION_TOGGLE) || // toggle action is always valid
		(action == ACTION_OPEN && (door_status == DOOR_STATUS_CLOSED || door_status == DOOR_STATUS_CLOSING)) || // can only open from CLOSED or CLOSING states
		(action == ACTION_CLOSE && (door_status == DOOR_STATUS_OPEN || door_status == DOOR_STATUS_STOPPED))) {
			isValidAction = true;
	}

	if (!isValidAction) {
		DEBUG_PRINTLN(F("Requested command not valid, or door already in requested state"));
		return;
	}
	cancel_diagnostic_audio();
	hold_network_delivery();

	// This is the core logic for deciding whether to trigger the alarm or the door directly.
	bool shouldTriggerAlarm = true;
	if (force_alarm_off || // override is on
		(!og.options[OPTION_ALM].ival) || // alarm not enabled
		(og.options[OPTION_AOO].ival && (door_status==DOOR_STATUS_CLOSED || door_status==DOOR_STATUS_CLOSING || door_status==DOOR_STATUS_OPENING))) { shouldTriggerAlarm = false;
	}

	if (shouldTriggerAlarm) { // if using alarm
		if (og.options[OPTION_SECV].ival == 2) {
			og.set_alarm(0, action);
		} else {
			og.set_alarm();
		}
		return;
	}

	// no alarm
	switch (og.options[OPTION_SECV].ival) {
		case 2: // SecPlus 2.0
			if (action == ACTION_OPEN) secplus2_garage.open_door();
			else if (action == ACTION_CLOSE) secplus2_garage.close_door();
			else secplus2_garage.toggle_door();
			break;
		case 1: // SecPlus 1.0
			secplus1_garage.toggle_door();
			break;
		default: // No SecPlus (standard relay)
			og.click_relay();
			break;
	}
}

void performLightAction(uint8_t action) {
	if(og.config_locked)return;
	if (firmware_update.busy() || firmware_update.phase()==FirmwareUpdatePhase::Success) return;
	hold_network_delivery();
	switch(action) {
		case ACTION_TOGGLE:
			switch (og.options[OPTION_SECV].ival) {
				case 2: // SecPlus 2
					secplus2_garage.toggle_light();
					break;
				case 1: // SecPlus 1
					secplus1_garage.toggle_light();
					break;
				default: // No secplus
					DEBUG_PRINTLN(F("Command request not valid, light requires secplus"));
			}
			break;
		case ACTION_OPEN: // TODO
		case ACTION_CLOSE:
			break;
	}
}

void performLockAction(uint8_t action) {
	if(og.config_locked)return;
	if (firmware_update.busy() || firmware_update.phase()==FirmwareUpdatePhase::Success) return;
	hold_network_delivery();
	switch(action) {
		case ACTION_TOGGLE:
			switch (og.options[OPTION_SECV].ival) {
				case 2: // SecPlus 2
					secplus2_garage.toggle_lock();
					break;
				case 1: // SecPlus 1
					secplus1_garage.toggle_lock();
					break;
				default: // No secplus
					DEBUG_PRINTLN(F("Command request not valid, light requires secplus"));
			}
			break;
		case ACTION_OPEN: // TODO
		case ACTION_CLOSE:
			break;
	}
}
void sta_change_controller_main(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;

	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}

	uint8_t action = 255;
	if((bool)req.getQueryParameter("click")) { action = ACTION_TOGGLE; }
	else if((bool)req.getQueryParameter("close")) { action = ACTION_CLOSE; }
	else if((bool)req.getQueryParameter("open")) { action = ACTION_OPEN; }

	char* light = req.getQueryParameter("light");
	char* lock = req.getQueryParameter("lock");

	if(action < 255) {
		otf_send_result(res, HTML_SUCCESS, nullptr);
		performDoorAction(action);
		return;
	}

	if(light) {
		otf_send_result(res, HTML_SUCCESS, nullptr);
		if(strcmp(light, "toggle")==0) {
			performLightAction(ACTION_TOGGLE);
		}
		return;
	}

	if(lock) {
		otf_send_result(res, HTML_SUCCESS, nullptr);
		if(strcmp(lock, "toggle")==0) {
			performLockAction(ACTION_TOGGLE);
		}
		return;
	}

	if(req.getQueryParameter("reboot") != NULL) {
		otf_send_result(res, HTML_SUCCESS, nullptr);
		restart_in(1000);
	} else if(req.getQueryParameter("apmode") != NULL) {
		og.options[OPTION_MOD].ival=OG_MOD_AP;
		if(!og.options_save()){otf_send_result(res, HTML_UPLOAD_FAILED, "save");return;}
		otf_send_result(res, HTML_SUCCESS, nullptr);
		restart_in(1000);
	} else {
		otf_send_result(res, HTML_NOT_PERMITTED, nullptr);
	}
}

void on_sta_change_controller(const OTF::Request &req, OTF::Response &res) {
	sta_change_controller_main(req, res);
}

// Parameters of one /co request. GET (and any method other than POST, as before) reads the URL
// query; POST reads only its application/x-www-form-urlencoded body, which has no request-line
// limit and keeps secrets out of URLs. Both are decoded with the same rules (form_params.h).
struct OptionParams {
	const OTF::Request &request;
	const og_form::Form *form;
	char *getQueryParameter(const char *key) const {
		return form ? form->get(key) : request.getQueryParameter(key);
	}
};

bool verify_device_key(const OptionParams &params) {
	return device_key_matches(params.getQueryParameter("dkey"));
}

void sta_change_options_main(const OptionParams &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;

	if(!verify_device_key(req)) {
		otf_send_result(res, HTML_UNAUTHORIZED, nullptr);
		return;
	}

	byte i;
	OptionStruct *o = og.options;

	byte usi = 0;
	const char *cloud = req.getQueryParameter("cld");
	if (cloud && strcmp(cloud, "0") != 0 && strcmp(cloud, "2") != 0) {
		otf_send_result(res, HTML_DATA_OUTOFBOUND, "cld: Blynk support retired"); return;
	}
	for (const char *field : {"auth", "apwd", "iftt", "mqpw"}) {
		const String clear_name = String("clear_") + field;
		const char *clear = req.getQueryParameter(clear_name.c_str());
		const char *value = req.getQueryParameter(field);
		if (clear && (strcmp(clear, "1") != 0 || (value && *value))) {
			otf_send_result(res, HTML_DATA_FORMATERROR, field); return;
		}
	}
	const char *new_auth = req.getQueryParameter("auth");
	const bool otc_selected = cloud ? strcmp(cloud, "2") == 0 : og.options[OPTION_CLD].ival == CLOUD_OTC;
	if (otc_selected && (req.getQueryParameter("clear_auth") ||
		(new_auth && *new_auth ? strlen(new_auth) < 32 :
		 og.options[OPTION_CLD].ival != CLOUD_OTC || og.options[OPTION_AUTH].sval.length() < 32))) {
		otf_send_result(res, HTML_DATA_OUTOFBOUND, "auth: OTC requires a token"); return;
	}
	// Match UI bounds for MQTT text, SMTP and NTP server settings (UTF-8 bytes).
	for (const char *field : {"name", "mqtp", "smtp", "send", "apwd", "recp", "ntp1"}) {
		const char *value = req.getQueryParameter(field);
		if (value && strlen(value) > 64) { otf_send_result(res, HTML_DATA_OUTOFBOUND, field); return; }
	}
    for(const char *field : {"name","smtp","send","recp"}) {
        const char *value=req.getQueryParameter(field);
        if(value && !og_smtp::header_safe(value)) {otf_send_result(res,HTML_DATA_FORMATERROR,field);return;}
    }
	const char *http_port = req.getQueryParameter("htp");
	if (http_port && String(http_port).toInt() == 8080) {
		otf_send_result(res, HTML_DATA_OUTOFBOUND, "htp: port 8080 is reserved for firmware updates"); return;
	}
	char *pem_value = req.getQueryParameter("pem");
	if (pem_value != NULL) {
		const char *requested_secv = req.getQueryParameter("secv");
		const bool secplus1_selected = requested_secv ? strcmp(requested_secv, "1") == 0 : og.options[OPTION_SECV].ival == 1;
		if (!og.has_swrx || !secplus1_selected ||
			(strcmp(pem_value, "0") != 0 && strcmp(pem_value, "1") != 0)) {
			otf_send_result(res, HTML_DATA_OUTOFBOUND, "pem");
			return;
		}
	}
	// FIRST ROUND: check option validity
	// do not save option values yet
	for(i=0;i<NUM_OPTIONS;i++,o++) {
		const char *key = o->name;
		// these options cannot be modified here
		if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID ||
			i==OPTION_PASS || i==OPTION_DKEY || i==OPTION_KSET)
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
				if(i==OPTION_RIV && ival < 1) {
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
		if(!og_key::allowed_change(og.options[OPTION_DKEY].sval.c_str(),nkey)) {
			otf_send_result(res, HTML_DATA_OUTOFBOUND, _nkey);return;
		}
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
	uint old_secv = og.options[OPTION_SECV].ival;
	uint old_pem = og.options[OPTION_PEM].ival;
	o = og.options;
	for(i=0;i<NUM_OPTIONS;i++,o++) {
		const char *key = o->name;
		// these options cannot be modified here
		if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID ||
			i==OPTION_PASS || i==OPTION_DKEY || i==OPTION_KSET)
			continue;

		char *sval = req.getQueryParameter(key);
		if(sval != NULL) {
			if (o->max) {  // integer options
				uint ival = String(sval).toInt();
				o->ival = ival;
			} else {
				// Secret fields are write-only. Blank means preserve; explicit clear
				// flags below remove them without round-tripping stored credentials.
				if (*sval || (i!=OPTION_APWD && i!=OPTION_AUTH && i!=OPTION_IFTT && i!=OPTION_MQPW))
					o->sval = String(sval);
			}
		}
	}

	for (byte field : {byte(OPTION_AUTH), byte(OPTION_APWD), byte(OPTION_IFTT), byte(OPTION_MQPW)}) {
		const String clear_name = String("clear_") + og.options[field].name;
		if (req.getQueryParameter(clear_name.c_str())) og.options[field].sval = "";
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
			if(og_key::valid_new(nkey))og.options[OPTION_KSET].ival=1;
	}

	if(!og.options_save()) {
		otf_send_result(res, HTML_UPLOAD_FAILED, "save"); return;
	}

	uint new_secv = og.options[OPTION_SECV].ival;
	secplus1_garage.set_panel_emulation_disabled(og.options[OPTION_PEM].ival != 0);
	if(old_secv != new_secv) { // sec+ version changed
		if (old_secv == 2) secplus2_garage.stop();
		if (old_secv == 1) secplus1_garage.stop();
		switch(new_secv) {
			case 2:
				secplus2_garage.begin();
				secplus2_garage.reset_state();
				secplus_door_status = DOOR_STATUS_UNKNOWN;
				secplus2_garage.enable_callback(secplus2_state_callback);
				break;
			case 1:
				secplus1_garage.begin();
				secplus1_garage.reset_state();
				secplus_door_status = DOOR_STATUS_UNKNOWN;
				secplus1_garage.enable_callback(secplus1_state_callback);
				break;
		}
	} else if (new_secv == 1 && old_pem != og.options[OPTION_PEM].ival) {
		secplus1_garage.reset_state();
		secplus_door_status = DOOR_STATUS_UNKNOWN;
	}

	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void on_sta_change_options(const OTF::Request &req, OTF::Response &res) {
	sta_change_options_main(OptionParams{req, nullptr}, res);
}

void on_sta_change_options_post(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;
	og_form::Form form;
	switch (form.parse(req.getBody(), req.getBodyLength())) {
		case og_form::Form::OK: break;
		case og_form::Form::TOO_MANY: otf_send_result(res, HTML_DATA_OUTOFBOUND, "body: too many fields"); return;
		case og_form::Form::NO_MEMORY: otf_send_result(res, HTML_NOT_PERMITTED, "body: not enough memory"); return;
		default: otf_send_result(res, HTML_DATA_FORMATERROR, "body"); return;
	}
	sta_change_options_main(OptionParams{req, &form}, res);
}

void sta_options_fill_json(String& json) {
	json.reserve(STRING_RESERVE_SIZE*2); // reserve capacity to reduce the overhead of String's +=
	json = "{";
	OptionStruct *o = og.options;
	for(byte i=0;i<NUM_OPTIONS;i++,o++) {
		if(!o->max) {
			if(i==OPTION_PASS || i==OPTION_DKEY || i==OPTION_MQPW ||
			   i==OPTION_APWD || i==OPTION_AUTH || i==OPTION_IFTT) { // Stored secrets are write-only
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
						append_json_escaped(json, og.options[OPTION_NAME].sval);
						break;
					case OPTION_HOST:
						append_json_escaped(json, get_ap_ssid());
						break;
					//case OPTION_NTP1: // leave NPT1 empty if using default
					//json += DEFAULT_NTP1;
					//break;
					}
				} else {
					append_json_escaped(json, o->sval);
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
	json += F("\"key_weak\":"); json += og_key::valid_new(og.options[OPTION_DKEY].sval.c_str()) ? 0 : 1;
	json += F(",\"auth_set\":"); json += og.options[OPTION_AUTH].sval.length() ? 1 : 0;
	json += F(",\"apwd_set\":"); json += og.options[OPTION_APWD].sval.length() ? 1 : 0;
	json += F(",\"iftt_set\":"); json += og.options[OPTION_IFTT].sval.length() ? 1 : 0;
	json += F(",");
	// append has_swrx variable
	json += F("\"has_swrx\":");
	json += og.has_swrx;
	json += F(",\"s2id\":\"");
	if (secplus2_identity_ready) {
		json += F("0x");
		json += String(secplus2_client_id, HEX);
	} else {
		json += F("Unavailable");
	}
	json += F("\"");
	json += F("}");
}

void on_sta_options(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) return;
	String json;
	sta_options_fill_json(json);
	otf_send_json(res, json);
}

// First-use credential establishment is independent of WiFi. A retry after a
// lost successful response is allowed only with the exact already-saved key.
void on_ap_set_key(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode != OG_MOD_AP || og.config_locked) {otf_send_result(res, HTML_NOT_PERMITTED, "config");return;}
	if(!setup_room(og_setup::SETUP_RUNWAY_MS)) {otf_send_result(res, HTML_NOT_PERMITTED, "setup_timeout");return;}
	const char *body=req.getBody();
	const size_t length=req.getBodyLength();
	if(og.options[OPTION_KSET].ival) {
		otf_send_result(res, body_device_key_matches(body,length,og.options[OPTION_DKEY].sval.c_str()) ? HTML_SUCCESS : HTML_UNAUTHORIZED, nullptr);return;
	}
	if(!body || length<10 || length>64 || memchr(body,0,length)) {otf_send_result(res, HTML_DATA_OUTOFBOUND, "nkey");return;}
	char key[65];memcpy(key,body,length);key[length]=0;
	if(!og_key::valid_new(key)) {otf_send_result(res, HTML_DATA_OUTOFBOUND, "nkey");return;}
	if(!og.options[OPTION_DKEY].sval.reserve(length)) {otf_send_result(res, HTML_UPLOAD_FAILED, "save");return;}
	og.options[OPTION_DKEY].sval=key;og.options[OPTION_KSET].ival=1;
	if(!og.options_save()) {otf_send_result(res, HTML_UPLOAD_FAILED, "save");return;}
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void on_ap_setup_status(const OTF::Request &, OTF::Response &res) {
 String json=F("{\"remaining_s\":");
 json+=setup_expired ? 0 : og_setup::remaining(millis(),setup_started_ms)/1000;
 json+=F("}");otf_send_json(res,json);
}

void on_ap_scan(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_STA) return;
	// key_required is the final single-digit field in this cached scan response.
	scanned_ssids.setCharAt(scanned_ssids.length()-2, og.options[OPTION_KSET].ival ? '1' : '0');
	otf_send_json(res, scanned_ssids);
}

void on_ap_change_config(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_STA) return;
	if(og.config_locked) {otf_send_result(res, HTML_NOT_PERMITTED, "config");return;}
	if(!og.options[OPTION_KSET].ival) {otf_send_result(res, HTML_NOT_PERMITTED, "key");return;}
	if(og.options[OPTION_KSET].ival && !verify_device_key(req)) {otf_send_result(res, HTML_UNAUTHORIZED, nullptr);return;}
	if(!setup_room(og_setup::SETUP_RUNWAY_MS)) {otf_send_result(res, HTML_NOT_PERMITTED, "setup_timeout");return;}
	const char *cloud = req.getQueryParameter("cld");
	if (cloud && strcmp(cloud, "otc") != 0 && strcmp(cloud, "none") != 0) {
		otf_send_result(res, HTML_DATA_OUTOFBOUND, "cld: Blynk support retired"); return;
	}
	char *ssid = req.getQueryParameter("ssid");
	if(ssid!=NULL&&strlen(ssid)!=0) {
		og.options[OPTION_SSID].sval = ssid;
		og.options[OPTION_PASS].sval = req.getQueryParameter("pass");
		og.options[OPTION_HOST].sval = req.getQueryParameter("host");
		// if cloud token is provided, save it
		char *cld = req.getQueryParameter("cld");
		char *auth = req.getQueryParameter("auth");
		if(cld!=NULL&&strlen(cld)!=0&&auth!=NULL&&strlen(auth)!=0) {
			if(strcmp(cld, "otc")==0) {
				char *bdmn = req.getQueryParameter("bdmn");
				char *bprt = req.getQueryParameter("bprt");
					og.options[OPTION_BDMN].sval=(bdmn==NULL||strlen(bdmn)==0)?DEFAULT_OTC_DMN:bdmn;
					og.options[OPTION_BPRT].ival=(bprt==NULL||strlen(bprt)==0)?DEFAULT_OTC_PRT:String(bprt).toInt();
					og.options[OPTION_CLD].ival = CLOUD_OTC;
				og.options[OPTION_AUTH].sval = auth;
				DEBUG_PRINTLN(og.options[OPTION_CLD].ival);

				DEBUG_PRINTLN(og.options[OPTION_BDMN].sval);
				DEBUG_PRINTLN(og.options[OPTION_BPRT].ival);
			} else {
				otf_send_result(res, HTML_DATA_OUTOFBOUND, "cld");
				return;
			}
		}
		// if secplus is provided
		char *secv = req.getQueryParameter("secv");
		if(og.has_swrx && secv != NULL && strlen(secv)!=0) {
			uint8_t v = String(secv).toInt();
			if(v>2) v=2;
			og.options[OPTION_SECV].ival=v;
		}
		if(!og.options_save()){otf_send_result(res, HTML_UPLOAD_FAILED, "save");return;}
		otf_send_result(res, HTML_SUCCESS, nullptr);
		setup_save_failed=false;
		og.state = OG_STATE_TRY_CONNECT;
	} else {
		otf_send_result(res, HTML_DATA_MISSING, "ssid");
	}
}

void on_ap_try_connect(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_STA) return;
	if(og.config_locked) {otf_send_result(res, HTML_NOT_PERMITTED, "config");return;}
	if(!og.options[OPTION_KSET].ival) {otf_send_result(res, HTML_NOT_PERMITTED, "key");return;}
	if(og.options[OPTION_KSET].ival && !verify_device_key(req)) {otf_send_result(res, HTML_UNAUTHORIZED, nullptr);return;}
	if(!setup_room(og_setup::SETUP_RUNWAY_MS)) {otf_send_result(res, HTML_NOT_PERMITTED, "setup_timeout");return;}
	if(WiFi.status()==WL_CONNECTED && WiFi.localIP() && !complete_ap_setup()) {otf_send_result(res, HTML_UPLOAD_FAILED, "save");return;}
	String json = "{";
	json += F("\"ip\":");
	json += (WiFi.status() == WL_CONNECTED) ? (uint32_t)WiFi.localIP() : 0;
	json += F("}");
	otf_send_json(res, json);
}

void on_ap_debug(const OTF::Request &req, OTF::Response &res) {
	DebugJson json;
    json.reserve(STRING_RESERVE_SIZE);
	json += F("{");
	json += F("\"dist\":");
	json += og.read_distance();
	json += F(",\"fwv\":");
	json += og.options[OPTION_FWV].ival;
	json += F(",\"has_swrx\":");
	json += og.has_swrx;
	append_debug_details(req, json);
	json += F("}");
	send_debug_json(res, json);
}

// MQTT callback to read "Button" requests
// PubSubClient's parser waits for missing bytes. Only enter it with a whole
// packet buffered; a stalled partial packet is discarded without waiting here.
static bool mqtt_frame_ready() {
	static bool partial = false;
	static uint32_t partial_since = 0;
	const size_t available = wificlient.available();
	if (!available) { partial = false; return true; }
	uint8_t header[5];
	const size_t count = wificlient.peekBytes(header, available < 5 ? available : 5);
	uint32_t remaining = 0, multiplier = 1;
	for (size_t i=1; i<count; ++i) {
		remaining += (header[i]&127)*multiplier;
		if (!(header[i]&128)) {
			const size_t total = i+1+remaining;
			if (total > mqttclient.getBufferSize()) { wificlient.stop(); partial=false; return false; }
			if (available >= total) { partial=false; return true; }
			break;
		}
		if (i==4) { wificlient.stop(); partial=false; return false; }
		multiplier *= 128;
	}
	if (!partial) { partial=true; partial_since=millis(); }
	else if (uint32_t(millis()-partial_since)>=1000) { wificlient.stop(); partial=false; }
	return false;
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
	if (mqttclient.incomingRetained()) { ++mqtt_retained_dropped; return; }
	const String incoming(topic);
	const bool state_topic = incoming == mqtt_topic + "/IN/STATE";
	const bool legacy_topic = incoming == mqtt_topic || incoming.startsWith(mqtt_topic + "/IN/");
	switch (og_network::command(payload, length, false, state_topic, legacy_topic)) {
		case og_network::Command::TOGGLE: performDoorAction(ACTION_TOGGLE); break;
		case og_network::Command::OPEN: performDoorAction(ACTION_OPEN); break;
		case og_network::Command::CLOSE: performDoorAction(ACTION_CLOSE); break;
		case og_network::Command::LIGHT: performLightAction(ACTION_TOGGLE); break;
		case og_network::Command::LOCK: performLockAction(ACTION_TOGGLE); break;
		default: break;
	}
}

void do_setup() {
	Serial.begin(115200);
	DEBUG_PRINTLN("started.");
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
	og_crash::begin();
	Secplus2IdentityStore identity_store;
	secplus2_identity_ready = og_identity::load(identity_store,
		[]() -> uint32_t { return ESP.random(); },
		!FILESYS.exists(CONFIG_FNAME), secplus2_client_id);
	if (secplus2_identity_ready) {
		secplus2_identity_ready = secplus2_garage.set_client_id(secplus2_client_id);
	}
	Serial.print(F("Security+ 2.0 client ID: 0x"));
	Serial.println(secplus2_client_id, HEX);
	if (!secplus2_identity_ready) Serial.println(F("Security+ 2.0 disabled: identity storage unavailable or invalid"));
	og.options_setup();
	if (og.options[OPTION_MQEN].ival) ensure_mqtt_buffer();
	mqttclient.setSocketTimeout(1);
	wificlient.setTimeout(1000);
	httpclient.setTimeout(5000);
	secplus1_garage.set_panel_emulation_disabled(og.options[OPTION_PEM].ival != 0);
	og.init_sensors();
	curr_mode = og.get_mode();

	// initialize secplus objects and enable callbacks
	switch(og.options[OPTION_SECV].ival) {
		case 2:
			secplus2_garage.begin();
			secplus2_garage.reset_state();
			secplus2_garage.enable_callback(secplus2_state_callback);
			break;
		case 1:
			secplus1_garage.begin();
			secplus1_garage.reset_state();
			secplus1_garage.enable_callback(secplus1_state_callback);
			break;
	}

	if(!otf) {
		const String otfDeviceKey = og.options[OPTION_AUTH].sval;

		if((og.options[OPTION_CLD].ival==CLOUD_OTC) && (otfDeviceKey.length() >= 32)) {
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
		updateServer->collectHeaders("X-OG-Device-Key", "X-OG-Upload-Token", "X-OG-Release-Id", "X-OG-Signature", "X-OG-Allow-Downgrade");
		DEBUG_PRINT(F("update server started"));
	}
	led_blink_ms = LED_FAST_BLINK;
}

void process_ui() {
	// process button
	static ulong button_down_time = 0;
	static byte button_hold_stage = 0;
	if(og.get_button() == LOW) {
		if(!button_down_time) {
			button_down_time = millis();
		} else {
			ulong curr = millis();
			long diff = (long)(curr - button_down_time);
			// Announce only the newly reached action; never replay missed stages
			// after a busy loop. Actions themselves still happen on release.
			byte stage = diff > BUTTON_FACRESET_TIMEOUT ? 3 :
				diff > BUTTON_APRESET_TIMEOUT ? 2 : diff > BUTTON_REPORTIP_TIMEOUT ? 1 : 0;
			if (stage > button_hold_stage) {
				button_hold_stage = stage;
				// Safety warnings and an active IP report take precedence. Do not
				// restart diagnostic audio while firmware is being written.
				if (!update_started && !setup_reboot_pending)
					start_tune(stage == 1 ? og_audio::Tune::BUTTON_IP :
						stage == 2 ? og_audio::Tune::BUTTON_WIFI : og_audio::Tune::BUTTON_FACTORY);
			}
			if(diff > BUTTON_FACRESET_TIMEOUT) {
				led_blink_ms = 0;
				og.set_led(LOW);
			} else if(diff > BUTTON_APRESET_TIMEOUT) {
				led_blink_ms = 0;
				og.set_led(HIGH);
			}
		}
	} else {
		if (button_down_time > 0) {
			ulong curr = millis();
			long diff = (long)(curr - button_down_time);
			if(diff > BUTTON_FACRESET_TIMEOUT) {
				og.state = OG_STATE_RESET;
			} else if(diff > BUTTON_APRESET_TIMEOUT) {
				og.reset_to_ap();
			} else if(diff > BUTTON_REPORTIP_TIMEOUT) {
				// report IP
				if (!og.alarm) {
					cancel_diagnostic_audio();
					ipString = get_ip();
					ipString.replace(".", ". ");
					ip_audio_active=true;
					report_ip();
				}
			} else if(curr > button_down_time + 50) {
				performDoorAction(ACTION_TOGGLE, true); // no alarm since manual operation
			}
			button_down_time = 0;
			button_hold_stage = 0;
		}
	}
	// process led
	static ulong led_toggle_timeout = 0;
	if(led_blink_ms) {
		if((long)(millis() - led_toggle_timeout) > 0) {
			// toggle led
			og.set_led(1-og.get_led());
			led_toggle_timeout = millis() + led_blink_ms;
		}
	}
}

void on_update(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) {
		otf_send_compressed_P(res, (const __FlashStringHelper*)ap_update_html_gz, ap_update_html_gz_len);
	} else {
		if(req.isCloudRequest()) {
			otf_send_result(res, HTML_NOT_PERMITTED, "fw update");
			return;
		}
		otf_send_compressed_P(res, (const __FlashStringHelper*)sta_update_html_gz, sta_update_html_gz_len);
	}
}

// All update entry points share the same service and early authorization.
static std::unique_ptr<char[]> prepare_body;
static constexpr size_t prepare_capacity = 1024;
static size_t prepare_length = 0;
static bool prepare_seen = false, prepare_ok = false;
static String upload_credential;
static bool upload_signed = false;

static bool updater_busy() {
 // Existing five-second local-action hold covers pending Sec+1 requests/releases.
 return !setup_room(og_setup::UPDATE_RUNWAY_MS) || og.state == OG_STATE_WAIT_RESTART || network_delivery_busy() || firmware_update.busy() ||
   firmware_update.phase() == FirmwareUpdatePhase::Success;
}
static bool updater_key_ok() {
 return !og.config_locked && update_key_matches(updateServer->header("X-OG-Device-Key").c_str(),
                           og.options[OPTION_DKEY].sval.c_str());
}
// Header-stage admission never consumes a token or changes updater state.
static bool updater_body_authorized() {
 if (updateServer->method() != HTTP_POST) return false;
 const String &path = updateServer->uri();
 bool authorized;
 if (path == "/update" || path == "/update/prepare") authorized=updater_key_ok();
 else if (path == "/update/upload")
  authorized=!og.config_locked && firmware_update.token_valid(updateServer->header("X-OG-Upload-Token").c_str());
 else return false;
 if (authorized && !setup_room(og_setup::UPDATE_RUNWAY_MS)) {
  updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  updateServer->sendHeader(F("Cache-Control"), F("no-store"));
  updateServer->sendHeader(F("Connection"), F("close"));
  updateServer->send(403, "application/json", F("{\"result\":48,\"item\":\"Setup window ending; reopen setup with a 5-second button hold\"}"));
  return false;
 }
 if (authorized && path=="/update" && !og.options[OPTION_KSET].ival) {
  updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  updateServer->sendHeader(F("Cache-Control"), F("no-store"));
  updateServer->sendHeader(F("Connection"), F("close"));
  updateServer->send(403, "application/json", F("{\"result\":48,\"item\":\"Set a device key in setup before uploading firmware\"}"));
  return false;
 }
 if (!authorized || updater_busy()) {
  // Explicit pre-body refusal: this request has not written or installed anything.
  updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  updateServer->sendHeader(F("Cache-Control"), F("no-store"));
  updateServer->sendHeader(F("Connection"), F("close"));
  updateServer->send(authorized ? 409 : 401, "application/json",
    authorized ? F("{\"result\":64,\"item\":\"Device busy; wait a few seconds and try again\"}") :
                 F("{\"result\":2,\"item\":\"Check the device key or prepare a new upload token\"}"));
  return false;
 }
 return true;
}
static void cancel_firmware_upload() {
 if (update_started) firmware_update.abort_upload();
 update_started = update_authorized = false;
 upload_credential = "";
}
static void quiesce_for_update() {
 if (og.options[OPTION_SECV].ival == 2) secplus2_garage.finish_release();
 if (og.options[OPTION_SECV].ival == 1) secplus1_garage.finish_release();
 cancel_diagnostic_audio();
 if (curr_mode == OG_MOD_STA) {
  mqttclient.disconnect(); WiFiUDP::stopAll();
 }
 // OTF0.2.5 has no disconnect API. Close its sockets (and pending local clients)
 // without deleting the framework or changing its registered routes.
 WiFiClient::stopAllExcept(&updateServer->client());
 updateServer->client().keepAlive(5,2,3);
}

void on_firmware_upload_fin() {
 const bool authenticated = setup_room(0) && update_authorized &&
  (upload_signed ? upload_credential == updateServer->header("X-OG-Upload-Token") : (og.options[OPTION_KSET].ival && updater_key_ok()));
 bool ok = false;
 if (authenticated && !update_failed && update_file_seen)
  ok = upload_signed ? firmware_update.finish_verified() : firmware_update.finish_manual();
 if (!ok) cancel_firmware_upload();
 update_file_seen = update_started = update_authorized = false;
 upload_credential = "";
 if (ok) updateserver_send_result(HTML_SUCCESS, F("Update complete; rebooting"));
 else updateserver_send_result(authenticated ? HTML_UPLOAD_FAILED : HTML_UNAUTHORIZED, firmware_update.message());
 // Service schedules the reboot; no second independent timer.
}

void on_update_options() {
 updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
 updateServer->sendHeader(F("Access-Control-Max-Age"), F("10000"));
 updateServer->sendHeader(F("Access-Control-Allow-Methods"), F("POST,GET,OPTIONS"));
 updateServer->sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type, X-OG-Device-Key, X-OG-Upload-Token, X-OG-Release-Id, X-OG-Signature, X-OG-Allow-Downgrade"));
 updateServer->send(200, "text/plain", "");
}

void on_firmware_upload() {
 HTTPUpload& upload = updateServer->upload();
 if(!setup_room(0)) {cancel_firmware_upload();update_failed=true;updateServer->client().stop();return;}
 if (upload.status == UPLOAD_FILE_START) {
  if (update_file_seen) {
   firmware_update.reject("Only one firmware file is allowed"); update_failed=true;
   updateServer->client().stop(); return;
  }
  update_file_seen=true; update_failed=false;
  upload_signed=updateServer->uri()=="/update/upload";
  upload_credential=updateServer->header("X-OG-Upload-Token");
  update_authorized=upload_signed ? firmware_update.token_valid(upload_credential.c_str()) : updater_key_ok();
  if (!update_authorized || (!upload_signed && !og.options[OPTION_KSET].ival) || updater_busy()) {
   update_failed=true;updateServer->client().stop();return;
  }
  const bool begun=upload_signed ? firmware_update.begin_verified(upload.filename.c_str(),upload_credential.c_str()) :
                                  firmware_update.begin_manual_file(upload.filename.c_str());
  if (!begun) {update_failed=true;updateServer->client().stop();return;}
  update_started=true;quiesce_for_update();
 } else if (upload.status == UPLOAD_FILE_WRITE) {
  if (!update_authorized || update_failed || !update_started || !upload.currentSize) return;
  bool ok=upload_signed ? firmware_update.write_verified(upload.buf,upload.currentSize) :
                         firmware_update.write_manual(upload.buf,upload.currentSize);
  if (!ok) {update_failed=true;updateServer->client().stop();}
 } else if (upload.status == UPLOAD_FILE_ABORTED) {
  cancel_firmware_upload();update_file_seen=false;update_failed=true;
 }
 delay(0);
}

static void on_prepare_upload() {
 HTTPUpload &upload=updateServer->upload();
 if (upload.status==UPLOAD_FILE_START) {
  if (prepare_seen) {prepare_ok=false;updateServer->client().stop();return;}
  prepare_seen=true;prepare_length=0;prepare_ok=updater_key_ok() && !updater_busy();
  if (prepare_ok) prepare_body.reset(new (std::nothrow) char[prepare_capacity]);
  if (!prepare_ok) {updateServer->client().stop();return;}
  // On allocation refusal, consume the bounded multipart request and reply in the final callback.
  // Closing here could reset TCP before the browser receives an error.
  if (!prepare_body) {prepare_ok=false;return;}
 } else if (upload.status==UPLOAD_FILE_WRITE) {
  if (!prepare_ok) return;
  if (upload.currentSize>prepare_capacity-prepare_length) {
   prepare_ok=false;updateServer->client().stop();return;
  }
  memcpy(prepare_body.get()+prepare_length,upload.buf,upload.currentSize);prepare_length+=upload.currentSize;
 } else if (upload.status==UPLOAD_FILE_ABORTED) {
  prepare_seen=prepare_ok=false;prepare_length=0;
 }
}
static void on_prepare_fin() {
 if (prepare_seen && !prepare_body && updater_key_ok() && !updater_busy()) {
  prepare_seen=prepare_ok=false;prepare_length=0;
  updateserver_send_result(HTML_UPLOAD_FAILED, F("Not enough contiguous memory to verify release; retry later"));return;
 }
 bool ok=prepare_seen && prepare_ok && prepare_length && updater_key_ok() && !updater_busy();
 prepare_seen=prepare_ok=false;
 char token[17]={};
 String id=updateServer->header("X-OG-Release-Id");
 String sig=updateServer->header("X-OG-Signature");
 if (ok && (id.length()>31 || sig.length()>144)) ok=false;
 const bool attempted=ok;
 if (ok) ok=firmware_update.prepare_verified(reinterpret_cast<uint8_t*>(prepare_body.get()),prepare_length,
     sig.c_str(),id.c_str(),updateServer->header("X-OG-Allow-Downgrade")=="1",token,og.options[OPTION_KSET].ival ? 126 : 127);
 prepare_length=0;
 if (!ok) {
  if (attempted) updateserver_send_result(HTML_UPLOAD_FAILED, firmware_update.message());
  else updateserver_send_result(HTML_UPLOAD_FAILED, F("Preparation rejected; check key, release and device status"));
  return;
 }
 String json=F("{\"result\":1,\"upload_token\":\"");json+=token;json+=F("\"}");
 updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
 updateServer->sendHeader(F("Cache-Control"), F("no-store"));
 updateServer->send(200,"application/json",json);
}
// The vendored parser completes or aborts multipart parsing within handleClient().
// Scope cleanup also covers disconnects that bypass the upload/final callbacks.
static void service_update_server() {
 updateServer->handleClient();
 prepare_body.reset();
 prepare_seen=prepare_ok=false;prepare_length=0;
}
static void on_update_info() {
 String json=F("{\"product\":\"opengarage-stock\",\"target\":\"");json+=firmware_update.target();
 json+=F("\",\"version\":");json+=OG_FWV;
 json+=F(",\"base_url\":\"https://firmware.opengarage.io\",\"file_extension\":\".bin\",\"configured\":true}");
 updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));updateServer->sendHeader(F("Cache-Control"), F("no-store"));
 updateServer->send(200,"application/json",json);
}
static void on_update_status() {
 String json=F("{\"phase\":\"");json+=firmware_update.phase_name();
 json+=F("\",\"message\":\"");append_json_escaped(json,String(firmware_update.message()));
 json+=F("\",\"bytes_done\":");json+=firmware_update.bytes_done();
 json+=F(",\"bytes_total\":");json+=firmware_update.bytes_total();json+='}';
 updateServer->sendHeader(F("Access-Control-Allow-Origin"), F("*"));updateServer->sendHeader(F("Cache-Control"), F("no-store"));
 updateServer->send(200,"application/json",json);
}
static void register_update_routes() {
 updateServer->onBodyAuthorization(updater_body_authorized);
 updateServer->on("/update",HTTP_POST,on_firmware_upload_fin,on_firmware_upload);
 updateServer->on("/update/upload",HTTP_POST,on_firmware_upload_fin,on_firmware_upload);
 updateServer->on("/update/prepare",HTTP_POST,on_prepare_fin,on_prepare_upload);
 updateServer->on("/update/info",HTTP_GET,on_update_info);
 updateServer->on("/update/status",HTTP_GET,on_update_status);
 for(const char *path : {"/update","/update/upload","/update/prepare","/update/info","/update/status"})
  updateServer->on(path,HTTP_OPTIONS,on_update_options);
}

void check_status_ap() {
	static ulong cs_timeout = 0;
	if((long)(millis() - cs_timeout) > 0) {
		Serial.print(og.read_distance());
		Serial.print("/");
		Serial.println(OG_FWV);
		if(og.has_swrx) {
			Serial.println(F("secplus"));
		}
		cs_timeout = millis() + 2000;
	}
}

bool mqtt_connect_subscribe() {
	if (!network_delivery_busy() && mqtt_retry.ready(millis())) {
		if (!ensure_mqtt_buffer()) { mqtt_retry.attempt_result(millis(), false); return false; }
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
				mqtt_retry.attempt_result(millis(), true);
				return true;
			}else {
				DEBUG_PRINTLN(F("......Failed to Connect to MQTT"));
				mqtt_retry.attempt_result(millis(), false);
				return false;
			}
		}
	}
	return false;
}

bool iftttNotify(String s){
	http.setTimeout(5000);
	DEBUG_PRINTLN(" Sending IFTTT Notification");
	http.begin(httpclient, "http://maker.ifttt.com/trigger/opengarage/with/key/"+og.options[OPTION_IFTT].sval);
	http.addHeader("Content-Type", "application/json");
	const int code = http.POST("{\"value1\":\""+s+"\"}");
	String payload = http.getString();
	http.end();
	const bool ok = code == 200 && payload.indexOf("Congratulations") >= 0;
	if(ok) {
		DEBUG_PRINTLN(" Successfully updated IFTTT");
	}else{
		DEBUG_PRINT(" Error from IFTTT: ");
		DEBUG_PRINTLN(payload);
	}
	return ok;
}

static void __attribute__((noinline)) prepare_email_headers(EMailSender::EMailMessage &email_message) {
    if(local_clock.valid) {
        time_t utc=curr_utc_time;struct tm stamp;char date[48];
        gmtime_r(&utc,&stamp);
        static const char days[] PROGMEM="SunMonTueWedThuFriSat";
        static const char months[] PROGMEM="JanFebMarAprMayJunJulAugSepOctNovDec";
        char day[4]={},month[4]={};memcpy_P(day,days+3*stamp.tm_wday,3);memcpy_P(month,months+3*stamp.tm_mon,3);
        snprintf_P(date,sizeof(date),PSTR("%s, %02d %s %04d %02d:%02d:%02d +0000"),day,stamp.tm_mday,month,
            stamp.tm_year+1900,stamp.tm_hour,stamp.tm_min,stamp.tm_sec);
        email_message.date=date;
    }
    static const uint32_t mailBoot=ESP.random();static uint32_t mailSequence=0;
    char id[80];snprintf_P(id,sizeof(id),PSTR("%06lx.%08lx.%lu@opengarage.local"),
        (unsigned long)ESP.getChipId(),(unsigned long)mailBoot,(unsigned long)++mailSequence);
    email_message.messageId=id;

}

static og_smtp::Delivery record_email_result(og_smtp::Delivery result, const char *stage, const char *code) {
    if (result != og_smtp::Delivery::SENT) ++email_failures;
    // Only local stage descriptions and numeric codes, never server text/credentials.
    snprintf_P(email_last,sizeof(email_last),PSTR("%s: %s (%s)"),
        result==og_smtp::Delivery::SENT ? "Sent" : result==og_smtp::Delivery::REFUSED ? "Refused" : "Failed",stage,code);
    return result;
}

og_smtp::Delivery emailNotify(String s){
	DEBUG_PRINTLN(" Sending EMail notification");
	DEBUG_PRINTLN(GET_FREE_HEAP);
	EMailSender::EMailMessage email_message;
    email_message.mime="text/plain";
	email_message.subject = og.options[OPTION_NAME].sval.c_str();
	email_message.message = s;
    prepare_email_headers(email_message);
	const char *email_host = og.options[OPTION_SMTP].sval.c_str();
	const char *email_pword = og.options[OPTION_APWD].sval.c_str();
	const char *email_sender = og.options[OPTION_SEND].sval.c_str();
	const char *email_recip = og.options[OPTION_RECP].sval.c_str();
	unsigned int email_port = og.options[OPTION_SPRT].ival;
	if(email_host && email_pword && email_sender && email_recip){
		EMailSender emailSend(email_sender, email_pword);
		emailSend.setSMTPServer(email_host);
		emailSend.setSMTPPort(email_port);
		if (!emailSend.readyToSend()) return record_email_result(og_smtp::Delivery::REFUSED,"Settings allocation failed","2");
		DEBUG_PRINTLN(GET_FREE_HEAP);
		EMailSender::Response resp = emailSend.send(email_recip, email_message);
        DEBUG_PRINT("SMTP result: ");DEBUG_PRINTLN(resp.desc);
		return record_email_result(resp.status ? og_smtp::Delivery::SENT :
            emailSend.networkAttempted() ? og_smtp::Delivery::FAILED : og_smtp::Delivery::REFUSED,
            resp.desc.c_str(),resp.code.c_str());
	}
	return record_email_result(og_smtp::Delivery::REFUSED,"Missing settings","2");
}

void mqttNotify(String s){
	if (mqttclient.connected()) {
		DEBUG_PRINTLN(" Sending MQTT Notification");
		mqttclient.publish((mqtt_topic + "/OUT/NOTIFY").c_str(),s.c_str());
	}
}

void perform_notify(String s) {
	// One short-lived delivery slot, never an offline/reconnect replay queue.
	if (WiFi.status() != WL_CONNECTED || !WiFi.localIP()) return;
	if (pending_notification.length()) ++notification_dropped;
	pending_notification = s; notification_ms = millis();
}

void service_notifications() {
	if (!pending_notification.length()) return;
	if (WiFi.status() != WL_CONNECTED || !WiFi.localIP() ||
	    uint32_t(millis()-notification_ms)>30000) {
		pending_notification=""; ++notification_dropped; return;
	}
	if (network_delivery_busy()) return;
	String message = pending_notification; pending_notification="";
	if (og.options[OPTION_MQEN].ival) mqttNotify(message);
	if (og.options[OPTION_IFTT].sval.length()>7 && ifttt_retry.ready(millis())) {
		if (iftttNotify(message)) ifttt_retry.success(); else ifttt_retry.failure(millis());
	}
	if (WiFi.status()==WL_CONNECTED && og.options[OPTION_EMEN].ival && email_retry.ready(millis())) {
		const auto result=emailNotify(message);
        if (result==og_smtp::Delivery::SENT) email_retry.success();
        else if (result==og_smtp::Delivery::FAILED) email_retry.failure(millis());
        // Local preflight refusal sends nothing: do not penalize the next event.
	}
}

void service_cloud() {
	const bool online = WiFi.status()==WL_CONNECTED && WiFi.localIP();
	const bool connected = otf->getCloudStatus()==OTF::CONNECTED;
	// OTF owns jittered reconnect backoff and resets it only after a healthy
	// session. Keep established sessions live during warnings; defer new
	// connection work while local actions are busy. Local HTTP is always served.
	otf->loop(online && (connected || !network_delivery_busy()));
}

void process_dynamics(byte event) {
	const uint32_t now = millis();
	byte ato = og.options[OPTION_ATO].ival;
	byte atob = og.options[OPTION_ATOB].ival;
	byte noto = og.options[OPTION_NOTO].ival;
	if(!ato && !atob && !noto) {
		open_duration.reset();
		return;
	}
	if(event == DOOR_EVENT_JUST_OPENED) {
		open_duration.start(now); // record time stamp
		if (noto & OG_NOTIFY_DO) { perform_notify(og.options[OPTION_NAME].sval + F(" just OPENED!"));}

		// If the door is set to auto close at a certain hour, ensure if manually opened it doesn't autoshut
		if( daily_rule.eligible(local_clock.valid, curr_utc_time, og.options[OPTION_ATIB].ival) ){
			DEBUG_PRINTLN(" Door opened during automation hour, set to not auto-close ");
			daily_rule.mark(curr_utc_time);
		}

	} else if (event == DOOR_EVENT_JUST_CLOSED) {
		open_duration.reset(); // reset time stamp
		if (noto & OG_NOTIFY_DC) { perform_notify(og.options[OPTION_NAME].sval + F(" just CLOSED!"));}

	} else if (event == DOOR_EVENT_JUST_STOPPED) {
		open_duration.reset(); // reset time stamp
		if (noto & OG_NOTIFY_DS) { perform_notify(og.options[OPTION_NAME].sval + F(" just STOPPED!"));}

	}	else if (event == DOOR_EVENT_REMAIN_OPEN) {
		{
			if(ato && open_duration.expired(now, uint32_t(og.options[OPTION_ATI].ival) * 60000UL)) {
				// reached timeout, perform action
				if(ato & OG_AUTO_NOTIFY) {
					// send notification
					String s = og.options[OPTION_NAME].sval+F(" is left open for more than ");
					s+= og.options[OPTION_ATI].ival;
					s+= F(" minutes.");
					if(ato & OG_AUTO_CLOSE) {
						s+= F(" Auto-close has been requested");
					} else {
						s+= F(" This is a reminder for you.");
					}
					perform_notify(s);
				}
				if(ato & OG_AUTO_CLOSE) {
					// auto close door
					// alarm is mandatory in auto-close
					if(!og.options[OPTION_ALM].ival) { og.set_alarm(OG_ALM_5); }
					else { og.set_alarm(); }
				}
				open_duration.reset();
			}

			if(atob && daily_rule.eligible(local_clock.valid, curr_utc_time, og.options[OPTION_ATIB].ival)) {
				// still open past time, perform action
				DEBUG_PRINTLN("Door is open at specified close time and automation not yet triggered: ");
				daily_rule.mark(curr_utc_time);
				if(atob & OG_AUTO_NOTIFY) {
					// send notification
					String s = og.options[OPTION_NAME].sval+F(" is open after ");
					s+= og.options[OPTION_ATIB].ival;
					s+= F(" UTC. Current hour:");
					s+= curr_utc_hour;
					if(atob & OG_AUTO_CLOSE) {
						s+= F(" Auto-close has been requested");
					} else {
						s+= F(" This is a reminder for you.");
					}
					perform_notify(s);
				}
				if(atob & OG_AUTO_CLOSE) {
					// auto close door
					// alarm is mandatory in auto-close
					if(!og.options[OPTION_ALM].ival) { og.set_alarm(OG_ALM_5); }
					else { og.set_alarm(); }
				}
				open_duration.reset();
			}
		}
	} else {
		open_duration.reset();
	}
}

byte check_door_event() {
	// perform pattern matching of door status histogram
	// and return the corresponding results
	const byte allones = (1<<DOOR_STATUS_HIST_K)-1;       // 0b1111
	const byte lowones = (1<<(DOOR_STATUS_HIST_K/2))-1; // 0b0011
	const byte highones= lowones << (DOOR_STATUS_HIST_K/2); // 0b1100

	if (!og.options[OPTION_SECV].ival) { // non security+
		byte _hist = door_status_hist & allones;  // get the lowest K bits
		if(_hist == 0) return DOOR_EVENT_REMAIN_CLOSED;
		if(_hist == allones) return DOOR_EVENT_REMAIN_OPEN;
		if(_hist == lowones) return DOOR_EVENT_JUST_OPENED;
		if(_hist == highones) return DOOR_EVENT_JUST_CLOSED;
		return DOOR_EVENT_NONE;
	} else { // security+
		if(door_status<DOOR_STATUS_UNKNOWN && last_door_status<DOOR_STATUS_UNKNOWN) {
			if(door_status == last_door_status) {
				switch(door_status) {
					case DOOR_STATUS_CLOSED:
						return DOOR_EVENT_REMAIN_CLOSED;
					case DOOR_STATUS_OPEN:
						return DOOR_EVENT_REMAIN_OPEN;
					case DOOR_STATUS_STOPPED:
						return DOOR_EVENT_REMAIN_STOPPED;
					case DOOR_STATUS_CLOSING:
						return DOOR_EVENT_STILL_CLOSING;
					case DOOR_STATUS_OPENING:
						return DOOR_EVENT_STILL_OPENING;
				}
			} else {
				switch(door_status) {
					case DOOR_STATUS_CLOSED:
						return DOOR_EVENT_JUST_CLOSED;
					case DOOR_STATUS_OPEN:
						return DOOR_EVENT_JUST_OPENED;
					case DOOR_STATUS_STOPPED:
						return DOOR_EVENT_JUST_STOPPED;
					case DOOR_STATUS_CLOSING:
						return DOOR_EVENT_START_CLOSING;
					case DOOR_STATUS_OPENING:
						return DOOR_EVENT_START_OPENING;
				}
			}
		}
		return DOOR_EVENT_NONE;
	}
}

void check_status() {
	static og_local::Interval status_interval;
	static bool history_initialized = false;

	if(status_interval.due(millis(), uint32_t(og.options[OPTION_RIV].ival) * 1000UL))  { //also check on first boot
		last_door_status = door_status; // save the current status to last_door_status
		if(light_blink_enabled) {
			og.set_led(HIGH);
			aux_ticker.once_ms(OG_LIGHT_BLINK_TIME, og.set_led, (byte)LOW);
		}

		// Read SN1 -- ultrasonic sensor
		uint dth = og.options[OPTION_DTH].ival;
		uint vth = og.options[OPTION_VTH].ival;
		bool sn1_status;
		distance = og.read_distance();
		if((distance==0 || distance>500 || !fullbuffer) && og.options[OPTION_SNO].ival!=OG_SNO_2ONLY) {
			// invalid distance value or non full buffer, return immediately except if using SN2 only
			DEBUG_PRINTLN(F("invalid distance or non-full buffer"));
			return;
		}

		sn1_status = (distance>dth)?0:1;
		if(og.options[OPTION_SN1].ival == OG_SN1_SIDE) {
			sn1_status = 1-sn1_status; // reverse logic for side mount
			// for side-mount, we can't decide vehicle status
			vehicle_status = OG_VEH_NOTAVAIL;
		} else {
			vehicle_status = OG_VEH_NOTAVAIL;
		  if (vth>0) { // if vehicle distance threshold is defined
				if(og.options[OPTION_SECV].ival>0 || og.options[OPTION_SNO].ival==OG_SNO_2ONLY) {
					// if door status is not determined using distance sensor (either using security+ or using SN2 only)
					// vehicle status can be deduced by checking if distance is less than vth
					vehicle_status = (distance <=vth) ? OG_VEH_PRESENT:OG_VEH_ABSENT;
				} else if (!sn1_status) {
					// if door is currently closed, vehicle status can be deduced by checking if distance is within bracket [dth, vth]
					vehicle_status = ((distance>dth) && (distance <=vth)) ? OG_VEH_PRESENT:OG_VEH_ABSENT;
				} else {
					// otherwise, door status is determined by distance sensor and door is open, blocking its view
					// so we can't deduce vehicle status
					vehicle_status = OG_VEH_UNKNOWN;
				}
			}
		}

		// Read SN2 -- optional switch sensor
		sn2_value = og.get_switch();
		byte sn2_status = 0;
		if(og.options[OPTION_SN2].ival == OG_SN2_NC) {	// if SN2 is normally closed type
			sn2_status = sn2_value;
		} else if(og.options[OPTION_SN2].ival == OG_SN2_NO) {	// if SN2 is normally open type
			sn2_status = 1-sn2_value;
		}

		switch (og.options[OPTION_SECV].ival) {
			case 1: // SecPlus 1
			case 2: // SecPlus 2
				// Handled by the callback function
				door_status = secplus_door_status;
				break;
			default: // No secplus
				// Process Sensor Logic
				bool status = false;
				if(og.options[OPTION_SN2].ival==OG_SN2_NONE || og.options[OPTION_SNO].ival==OG_SNO_1ONLY) {
					// if SN2 not installed or logic is SN1 only
					status = sn1_status;
				} else if(og.options[OPTION_SNO].ival==OG_SNO_2ONLY) {
					status = sn2_status;
				} else if(og.options[OPTION_SNO].ival==OG_SNO_AND) {
					status = sn1_status && sn2_status;
				} else if(og.options[OPTION_SNO].ival==OG_SNO_OR) {
					status = sn1_status || sn2_status;
				}
				door_status = status ? DOOR_STATUS_OPEN : DOOR_STATUS_CLOSED;
				break;
		}

		if (!history_initialized){
			history_initialized = true;
			DEBUG_PRINTLN(F("First time checking status don't trigger a status change, set full history to current value"));
			if (door_status) { door_status_hist = B11111111; }
			else { door_status_hist = B00000000; }
			last_door_status = door_status;
		} else {
			door_status_hist = (door_status_hist<<1) | door_status;
		}

		// get temperature readings
		og.read_TH_sensor(tempC, humid);

		read_cnt = (read_cnt+1)%100;

		// once the light is disabled, quit blinking until a restart or a reset to 0.
		byte blink_limit = og.options[OPTION_BAS].ival;
		bool current_light_blink_enabled = light_blink_enabled;
		light_blink_enabled = blink_limit == OG_LIGHT_BLINK_FOREVER || (light_blink_enabled && read_cnt <= blink_limit);

		// do a long blink to notify that we are turning the light off
		if(current_light_blink_enabled && !light_blink_enabled){
			og.set_led(HIGH);
			aux_ticker.once_ms(OG_LIGHT_BLINK_NOTIFY, og.set_led, (byte)LOW);
		}

		byte event = check_door_event();

		// Log door status changes (only record opened, closed, stopped status changes, as the other statuses are transient)
		if(event == DOOR_EVENT_JUST_OPENED || event == DOOR_EVENT_JUST_CLOSED || event == DOOR_EVENT_JUST_STOPPED) {
			wifi_door_event_seen = true; wifi_door_event_ms = millis();
			// write log record
			DEBUG_PRINTLN(" Update Local Log");
			LogStruct l;
			// Zero is the empty-slot marker in the existing binary log format.
			l.tstamp = local_clock.valid ? curr_utc_time :
				uint32_t(local_clock.uptime_ms / 1000) + 1;
			l.status = door_status;
			l.dist = distance;
			l.sn2 = 255;	// use 255 to indicate invalid value
			if(og.options[OPTION_SN2].ival>OG_SN2_NONE) l.sn2 = sn2_value;
			og.write_log(l);

		} //End state change updates

		static og_local::Interval mqtt_status_interval;
		// Send current status on change, or on status_report interval
		if (mqtt_status_interval.due(millis(), 15000) ||
				(event == DOOR_EVENT_JUST_OPENED || event == DOOR_EVENT_JUST_CLOSED || event == DOOR_EVENT_JUST_STOPPED || event == DOOR_EVENT_START_OPENING || event == DOOR_EVENT_START_CLOSING) ){
			// Mqtt update
			if(WiFi.status() == WL_CONNECTED && og.options[OPTION_MQEN].ival>0 && valid_url(og.options[OPTION_MQTT].sval) && mqttclient.connected()) {
				//DEBUG_PRINTLN(F(" Update MQTT (State Refresh)"));
				if(event == DOOR_EVENT_REMAIN_OPEN) {
					mqttclient.publish((mqtt_topic + "/OUT/STATE").c_str(),"OPEN");
					mqttclient.publish(mqtt_topic.c_str(),"Open"); //Support existing mqtt code
				} else if(event == DOOR_EVENT_REMAIN_CLOSED) {					// MQTT: If door closed...
					mqttclient.publish((mqtt_topic + "/OUT/STATE").c_str(),"CLOSED");
					mqttclient.publish(mqtt_topic.c_str(),"Closed"); //Support existing mqtt code
				} else if (event == DOOR_EVENT_REMAIN_STOPPED) {					// MQTT: If door closed...
					mqttclient.publish((mqtt_topic + "/OUT/STATE").c_str(),"STOPPED");
					mqttclient.publish(mqtt_topic.c_str(),"Stopped"); //Support existing mqtt code
				}
				String msg;
				sta_controller_fill_json(msg);
				mqtt_json_bytes = msg.length();
				const String topic = mqtt_topic + "/OUT/JSON";
				if (!mqtt_buffer_ready || msg.length() + topic.length() + 7 > mqttclient.getBufferSize() ||
					!mqttclient.publish(topic.c_str(), msg.c_str())) ++mqtt_json_failures;
			}
			// Send status report every 15 seconds: we don't need to send updates frequently if there is no status change.
			mqtt_status_interval.last = millis(); mqtt_status_interval.started = true;
		}

		// Process dynamics: automation and notifications
		process_dynamics(event);
	}
}

void time_keeping() {
	static bool configured = false;
	if (!configured && WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
		// lwIP keeps the name pointers it is given, and an options save can reallocate the ntp1
		// String, so pass a stable copy. /co bounds ntp1 to 64 bytes; a longer value saved earlier
		// is not shortened (that would query a different server): the defaults are used instead.
		static char ntp1[65];
		const String &server = og.options[OPTION_NTP1].sval;
		if (valid_url(server) && server.length() < sizeof(ntp1)) {
			memcpy(ntp1, server.c_str(), server.length() + 1);
			configTime(0, 0, ntp1, DEFAULT_NTP1, DEFAULT_NTP2);
		} else configTime(0, 0, DEFAULT_NTP1, DEFAULT_NTP2, DEFAULT_NTP3);
		configured = true;
	}
	const time_t observed = time(nullptr);
	local_clock.tick(millis(), observed >= og_local::Clock::MIN_UTC ? uint32_t(observed) : 0);
	curr_utc_time = local_clock.utc;
	curr_utc_hour = (curr_utc_time % 86400UL) / 3600UL;
	if (local_clock.valid && !start_utc_time)
		start_utc_time = curr_utc_time - uint32_t(local_clock.uptime_ms / 1000);
}

void process_alarm() {
	static uint32_t sequence=0;
	static bool running=false;
	if (!og.alarm) { warning_audio.cancel(); running=false; return; }
	if (!running || sequence!=og.alarm_sequence) {
		cancel_diagnostic_audio();
		sequence=og.alarm_sequence; running=true;
		warning_audio.start(millis(),uint32_t(og.alarm-1)*500);
		return;
	}
	if (warning_audio.tick(millis())) {
		hold_network_delivery();
		og.alarm=0; running=false;
			switch (og.options[OPTION_SECV].ival) {
				case 2: // SecPlus 2
					switch (og.alarm_action) {
						case 1: // Close
							secplus2_garage.close_door();
							break;
						case 2: // Open
							secplus2_garage.open_door();
							break;
						default: // Toggle
							secplus2_garage.toggle_door();
					}
					break;
				case 1: // SecPlus 1
					secplus1_garage.toggle_door();
					break;
				default: // No secplus
					og.click_relay();
			}
	}
}

void do_loop() {
	og_crash::loop_start(health_stats.uptime_s());
	og_crash::mark(og_crash::STAGE_FW_UPDATE);
	firmware_update.loop();
	LoopMeasurement loop_measurement;
	static uint32_t sta_address = 0;
	static bool station_tune_played = false;
	if (curr_mode == OG_MOD_STA && (og.state == OG_STATE_CONNECTING || og.state == OG_STATE_CONNECTED)) {
		og_crash::mark(og_crash::STAGE_TIME);
		time_keeping();
		og_crash::mark(og_crash::STAGE_CHECK_STATUS);
		check_status();
		og_crash::mark(og_crash::STAGE_STATION);
		const bool motion_busy = door_status == DOOR_STATUS_OPENING || door_status == DOOR_STATUS_CLOSING;
		service_station_network(og.options[OPTION_SSID].sval.c_str(), og.options[OPTION_PASS].sval.c_str(), network_delivery_busy(), motion_busy, og.options[OPTION_WFR].ival != 0,
			(local_action_seen && uint32_t(millis()-local_action_ms)<30000) ||
			(wifi_door_event_seen && uint32_t(millis()-wifi_door_event_ms)<30000));

	}
	if(curr_mode==OG_MOD_AP && !setup_expired && og_setup::remaining(millis(),setup_started_ms)==0)setup_expired=true;
	if(curr_mode==OG_MOD_AP && setup_expired && og.state!=OG_STATE_AP_EXPIRED && og.state!=OG_STATE_RESET && og.state!=OG_STATE_WAIT_RESTART) {
		WiFi.mode(WIFI_OFF);dns->stop();updateServer->stop();
		firmware_update.abort_upload();
		og.state=OG_STATE_AP_EXPIRED;led_blink_ms=2000;
	}
	og_crash::mark(og_crash::STAGE_STATE);
	switch(og.state) {
	case OG_STATE_INITIAL:
		if(curr_mode == OG_MOD_AP) {
			scanned_ssids = scan_network();
			scanned_ssids += F(",\"has_swrx\":");
			scanned_ssids += og.has_swrx;
			scanned_ssids += F(",\"secv\":");
			scanned_ssids += og.options[OPTION_SECV].ival;
			scanned_ssids += F(",\"key_required\":");
			scanned_ssids += og.options[OPTION_KSET].ival ? 1 : 0;
			scanned_ssids += F("}");
			String ap_ssid = get_ap_ssid();
			setup_started_ms=millis();setup_expired=false;
			start_network_ap(ap_ssid.c_str(), NULL);
			delay(500);
			dns->setErrorReplyCode(DNSReplyCode::NoError);
			dns->start(53, "*", WiFi.softAPIP());
			otf->on("/",   on_home);
			otf->on("/js", on_ap_scan);
			otf->on("/setup_status", on_ap_setup_status, OTF::HTTP_GET);
			otf->on("/cc", on_ap_change_config);
			otf->on("/jt", on_ap_try_connect);
			otf->on("/setup_key", on_ap_set_key, OTF::HTTP_POST);
			otf->on("/db", on_ap_debug);
			otf->on("/ad", on_auto_detect);
			// FIXME get update ap updates working.
			otf->on("/update", on_update, OTF::HTTP_GET);
			register_update_routes();
			otf->on("/resetall",on_reset_all);
			otf->onMissingPage(on_home);
			updateServer->begin();
			DEBUG_PRINTLN(F("Web Server endpoints (AP mode) registered"));
			og.state = OG_STATE_CONNECTED;
			DEBUG_PRINTLN(WiFi.softAPIP());
			start_tune(og_audio::Tune::AP);
		} else {
			led_blink_ms = LED_SLOW_BLINK;
			DEBUG_PRINT(F("Attempting to connect to SSID: "));
			DEBUG_PRINTLN(og.options[OPTION_SSID].sval.c_str());

			String host = og.options[OPTION_HOST].sval;
			const char* hostname = host.length() == 0 ? NULL : host.c_str(); // use the hostname provided if one is specified

			otf->on("/", on_home, OTF::HTTP_GET);
			otf->on("/jc", on_sta_controller);
			otf->on("/jo", on_sta_options);
			otf->on("/jl", on_sta_logs);
			otf->on("/ad", on_auto_detect);
			otf->on("/vo", on_sta_view_options, OTF::HTTP_GET);
			otf->on("/vl", on_sta_view_logs, OTF::HTTP_GET);
			otf->on("/cc", on_sta_change_controller);
			otf->on("/co", on_sta_change_options);  // GET as documented; any method but POST, as before
			otf->on("/co", on_sta_change_options_post, OTF::HTTP_POST);
			otf->on("/db", on_sta_debug);
			// FIXME get sta updates working.
			otf->on("/update", on_update, OTF::HTTP_GET);
			register_update_routes();
			otf->on("/clearlog", on_clear_log);
			otf->on("/s2id_gen", on_regenerate_secplus2_id, OTF::HTTP_POST);
			otf->on("/resetall",on_reset_all);
			updateServer->begin();
			DEBUG_PRINTLN(F("Web Server endpoints (STA mode) registered"));
			start_network_sta(og.options[OPTION_SSID].sval.c_str(), og.options[OPTION_PASS].sval.c_str(), hostname);
			og.config_ip();
			og.state = OG_STATE_CONNECTING;
		}
		break;

	case OG_STATE_AP_EXPIRED:
		// Radio stays off; process_ui below still handles the physical reset button.
		break;

	case OG_STATE_TRY_CONNECT:
		{
			led_blink_ms = LED_SLOW_BLINK;
			DEBUG_PRINT(F("Attempting to connect to SSID: "));
			DEBUG_PRINTLN(og.options[OPTION_SSID].sval.c_str());

			String host = og.options[OPTION_HOST].sval;
			const char* hostname = host.length() == 0 ? NULL : host.c_str(); // use the hostname provided if one is specified

			start_network_sta_with_ap(og.options[OPTION_SSID].sval.c_str(), og.options[OPTION_PASS].sval.c_str(), hostname);
			og.config_ip();
			og.state = OG_STATE_CONNECTED;
			break;
		}

	case OG_STATE_CONNECTING:
		if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
			DEBUG_PRINT(F("Wireless connected, IP: "));
			DEBUG_PRINTLN(WiFi.localIP());

			sta_address = uint32_t(WiFi.localIP());
			MDNS.close();
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

			led_blink_ms = 0;
			og.set_led(LOW);
			og.state = OG_STATE_CONNECTED;
			if (!station_tune_played) { start_tune(og_audio::Tune::STATION); station_tune_played = true; }
		}
		break;

	case OG_STATE_RESET:
		og.state = OG_STATE_INITIAL;
		if(!og.options_reset()) {og.state=OG_STATE_CONNECTED;break;}
		og.log_reset();
		og.restart();
		break;

	case OG_STATE_WAIT_RESTART:
		if(dns) dns->processNextRequest();
		if(otf) otf->loop();
		if(updateServer) service_update_server();
		break;

	case OG_STATE_CONNECTED: //THIS IS THE MAIN LOOP
		if(curr_mode == OG_MOD_AP) {
			dns->processNextRequest();
			otf->loop();
			service_update_server();
			check_status_ap();
			if(og.options[OPTION_MOD].ival == OG_MOD_STA) {
				// already in STA mode, waiting to reboot
				break;
			}
			if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
				DEBUG_PRINTLN(F("STA connected, updating option file"));
				complete_ap_setup();
			}

		} else {
			if(WiFi.status() == WL_CONNECTED) {
				if (!WiFi.localIP() || uint32_t(WiFi.localIP()) != sta_address) {
					og.state = OG_STATE_CONNECTING; break;
				}
				og_crash::mark(og_crash::STAGE_MDNS);
				MDNS.update();
				og_crash::mark(og_crash::STAGE_CLOUD);
				service_cloud();
				og_crash::mark(og_crash::STAGE_UPDATE_SERVER);
				service_update_server();

				//Handle MQTT
				og_crash::mark(og_crash::STAGE_MQTT);
				if(og.options[OPTION_MQEN].ival>0 && valid_url(og.options[OPTION_MQTT].sval)) { // if enabled and mqtt server looks valid
					const bool mqtt_connected = mqttclient.connected();
					mqtt_retry.observe(millis(), mqtt_connected);
					if (!mqtt_connected) {
						mqtt_id = get_ap_ssid();
						mqtt_topic = og.options[OPTION_MQTP].sval;
						if(mqtt_topic.length()==0) mqtt_topic = og.options[OPTION_NAME].sval;
						mqttclient.setServer(og.options[OPTION_MQTT].sval.c_str(), og.options[OPTION_MQPT].ival);
						mqttclient.setCallback(mqtt_callback);
						mqtt_connect_subscribe();
					}
					else if (mqtt_frame_ready()) {mqttclient.loop();} //Processes MQTT Pings/keep alives
				}
			} else {
				og.state = OG_STATE_CONNECTING;
				led_blink_ms = LED_SLOW_BLINK;
				DEBUG_PRINTLN(F("WiFi connection lost."));
			}
		}
		break;
	}

	// secplus process loop
	if (identity_restart_pending) return;
	og_crash::mark(og_crash::STAGE_SECPLUS);
	switch (og.options[OPTION_SECV].ival) {
		case 2: // SecPlus 2
			secplus2_garage.loop();
			break;
		case 1: // SecPlus 1
			secplus1_garage.loop();
			break;
	}

	og_crash::mark(og_crash::STAGE_UI);
	process_ui();

	og_crash::mark(og_crash::STAGE_ALARM);
	process_alarm();
	og_crash::mark(og_crash::STAGE_NOTIFY);
	if (curr_mode == OG_MOD_STA) service_notifications();
	og_crash::mark(og_crash::STAGE_AUDIO);
	if (!og.alarm && !ip_audio_active) startup_audio.tick(millis());
	if (setup_reboot_pending && !startup_audio.active() && !og.alarm) {
		setup_reboot_pending=false;
		restart_in(1000);
	}
}
