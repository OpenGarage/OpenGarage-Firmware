/* OpenGarage Firmware
 *
 * ESPConnect functions
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

#include "espconnect.h"
#include "OpenGarage.h"
#include "json_escape.h"

og_wifi::Recovery station_recovery;
uint8_t station_disconnect_reason = 0;
static WiFiEventHandler station_disconnect_handler;

og_wifi::Selection station_selection;
static og_wifi::Selection::Scan station_scan = og_wifi::Selection::Scan::NONE;
static uint32_t station_scan_ms = 0;
static bool station_was_online = false, station_scan_broken = false;
static const char *station_selection_result = "Idle";

const char *station_selection_status() { return station_selection_result; }

static void begin_station_scan(const char *ssid, og_wifi::Selection::Scan scan) {
	station_scan = scan; station_scan_ms = millis();
	station_selection.begin(scan);
	station_selection_result = scan == og_wifi::Selection::Scan::CONFIRM ? "Confirming candidate" : "Scanning";
	// Never call this during an association/DHCP attempt: the pinned SDK scan
	// path disconnects an in-progress attempt. Connected scans retain the link.
	WiFi.scanNetworks(true, true, scan == og_wifi::Selection::Scan::CONFIRM ? station_selection.confirm_channel() : 0,
		 reinterpret_cast<uint8_t *>(const_cast<char *>(ssid)));
}

void service_station_network(const char *ssid, const char *pass, bool critical_busy, bool motion_busy,
                             bool selection_enabled, bool roam_busy) {
	using Scan = og_wifi::Selection::Scan;
	using Action = og_wifi::Recovery::Action;
	const uint32_t now = millis();
	bool online = WiFi.status() == WL_CONNECTED && WiFi.localIP();
	if (online && !station_was_online) {
		const bool different = station_selection.target.valid && !station_selection.target.same(WiFi.BSSID());
		if (different) station_selection.cancel();
		station_selection.connected(now);
		station_selection_result = different ? "Connected to different AP" : "Connected";
	} else if (!online && station_was_online) station_selection.disconnected();
	station_was_online = online;
	station_selection.sample(now, online ? WiFi.RSSI() : -127);

	if (station_scan != Scan::NONE) {
		const int n = WiFi.scanComplete();
		if (n == WIFI_SCAN_RUNNING && uint32_t(now-station_scan_ms) < 10000) return;
		const Scan completed = station_scan; station_scan = Scan::NONE;
		if (n == WIFI_SCAN_RUNNING) {
			// A missing scan callback must not strand recovery. Reset only the
			// radio and use SSID-only recovery for this boot; scanDelete alone
			// cannot cancel a scan in the pinned core. No MCU reboot/flash write.
			const auto hostname = WiFi.hostname();
			WiFi.mode(WIFI_OFF); WiFi.mode(WIFI_STA);
			WiFi.hostname(hostname.c_str());
			WiFi.setAutoReconnect(false);
			WiFi.setSleep(false);
			WiFi.setOutputPower(20.5);
			// Reapply configured addressing before the fallback connection. A
			// radio reset must not rely on SDK retention of static IP/DNS.
			OpenGarage::config_ip();
			station_scan_broken = true; station_selection.plain_next = true;
			online = false; station_was_online = false;
			station_selection.disconnected(); station_selection.cancel();
			station_selection_result = "Scan timed out; plain SSID recovery";
		} else if (selection_enabled) {
			for (int i=0; i<n; ++i) {
				if (WiFi.SSID(i) != ssid) continue;
				const uint8_t enc = WiFi.encryptionType(i);
				// No downgrade to an open network when a password is configured.
				if (*pass ? (enc != ENC_TYPE_CCMP && enc != ENC_TYPE_TKIP && enc != ENC_TYPE_AUTO) : enc != ENC_TYPE_NONE) continue;
				og_wifi::AccessPoint ap;
				memcpy(ap.bssid, WiFi.BSSID(i), 6); ap.channel = WiFi.channel(i);
				ap.rssi = WiFi.RSSI(i); ap.valid = true;
				station_selection.offer(now, ap, online ? WiFi.BSSID() : nullptr);
			}
			const bool roam = station_selection.finish(now, online ? WiFi.RSSI() : -127);
			station_selection_result = completed == Scan::RECOVERY ? "Scan complete" :
				(roam ? "Candidate confirmed" : "Scan complete; waiting");
			if (completed != Scan::RECOVERY && (!online || critical_busy || motion_busy || roam_busy)) {
				station_selection.no_candidate(now); // discard stale/busy scan results
			} else if (roam) {
				WiFi.disconnect(false, false); online = false; station_was_online = false;
				station_recovery.roam(now); station_selection_result = "Roaming";
			}
		} else station_selection.cancel();
		WiFi.scanDelete();
		if (completed == Scan::RECOVERY) station_recovery.scan_finished(now);
		// A connected scan that lost its link must not carry a roam candidate
		// into offline recovery. Recovery will schedule its own fresh scan.
		if (completed != Scan::RECOVERY && !online && station_recovery.state() != og_wifi::Recovery::State::WAITING)
			station_selection.cancel();
	}

	if (station_scan_broken && WiFi.scanComplete() >= 0) WiFi.scanDelete(); // discard a late callback
	const bool select = selection_enabled && !station_scan_broken;
	const auto action = station_recovery.step(now, online, critical_busy, motion_busy,
		select && !station_selection.plain_next);
	if (action == Action::SCAN) { begin_station_scan(ssid, Scan::RECOVERY); return; }
	if (action == Action::DISCONNECT) {
		station_selection.failed(now);
		station_selection_result = "Attempt expired; retry scheduled";
	}
	if (action == Action::CONNECT || action == Action::DISCONNECT) {
		WiFi.setAutoReconnect(false);
		WiFi.disconnect(false, false);
		if (action == Action::CONNECT) {
			const auto &target = station_selection.target;
			const bool targeted = select && target.valid && !station_selection.plain_next;
			WiFi.begin(ssid, pass, targeted ? target.channel : 0, targeted ? target.bssid : nullptr);
			station_selection.plain_next = false;
			station_selection_result = targeted ? "Trying selected AP" : "Trying SSID fallback";
		}
		return;
	}
	if (!select) {
		if (online) {
			station_selection.cancel();
			station_selection_result = selection_enabled ? "Scanning disabled after timeout" : "AP selection off";
		}
		return;
	}
	if (online) {
		const auto scan = station_selection.due(now, !critical_busy && !motion_busy && !roam_busy);
		if (scan != Scan::NONE) begin_station_scan(ssid, scan);
	}
}


String scan_network() {
	DEBUG_PRINTLN(F("scan network"));
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	byte n = WiFi.scanNetworks();
	String wirelessinfo;
	if (n>32) n = 32; // limit to 32 ssids max
	wirelessinfo.reserve(STRING_RESERVE_SIZE); // pre-reserve memory to reduce the overhead of String's +=
	//Maintain old format of wireless network JSON for mobile app compat
	wirelessinfo = "{\"ssids\":[";
	for(int i=0;i<n;i++) {
		wirelessinfo += "\"";
		append_json_escaped(wirelessinfo, WiFi.SSID(i));
		wirelessinfo += "\"";
		if(i<n-1) wirelessinfo += ",\r\n";
	}
	wirelessinfo += "],";
	wirelessinfo += "\"rssis\":[";
	for(int i=0;i<n;i++) {
		wirelessinfo += "\"";
		wirelessinfo += WiFi.RSSI(i);
		wirelessinfo += "\"";
		if(i<n-1) wirelessinfo += ",\r\n";
	}
	wirelessinfo += "]";
	return wirelessinfo;
}

void start_network_ap(const char *ssid, const char *pass) {
	if(!ssid) return;
	DEBUG_PRINTLN(F("AP mode"));
	if(pass)
		WiFi.softAP(ssid, pass);
	else
		WiFi.softAP(ssid);
	WiFi.mode(WIFI_AP_STA); // start in AP_STA mode
	WiFi.disconnect();  // disconnect from router
}

void start_network_sta(const char *ssid, const char *pass, const char *hostname, bool staonly) {
	if(!ssid || !pass) return;
	DEBUG_PRINTLN(F("Sarting start_network_sta"));
	if(staonly){
		DEBUG_PRINTLN(F("Setting STA mode"));
		//WiFi.mode(WIFI_OFF); //Fix for bug in 2.3 on connect after SoftAP mode
		if(WiFi.getMode() != WIFI_STA)  WiFi.mode(WIFI_STA);
	}else{
		//WiFi.mode(WIFI_OFF);
		if(WiFi.getMode() != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
		DEBUG_PRINTLN(F("Setting to AP+STA mode"));
	}
	if(hostname != NULL) {
		WiFi.hostname(hostname);
	}
	if (staonly) {
		// Configured station mode has one reconnect owner. AP provisioning keeps
		// its existing trial-connect flow and does not run this manager.
		WiFi.setAutoReconnect(false);
		WiFi.disconnect(false, false);
		station_disconnect_handler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event) {
			station_disconnect_reason = event.reason;
		});
		station_recovery.start(millis());
		station_selection.reset(); station_scan = og_wifi::Selection::Scan::NONE;
		station_scan_broken = station_was_online = false;
	} else {
		WiFi.begin(ssid, pass);
		WiFi.setAutoReconnect(true);
	}
	WiFi.setSleep(false); // work-around for ARP issue: disable sleep mode
	WiFi.setOutputPower(20.5);
}

void start_network_sta_with_ap(const char *ssid, const char *pass, const char *hostname) {
	start_network_sta(ssid, pass, hostname, false);
}

void start_network_sta(const char *ssid, const char *pass, const char *hostname) {
	start_network_sta(ssid, pass, hostname, true);
}

