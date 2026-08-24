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

namespace {
const int ROAMING_MARGIN_DB = 8;
const unsigned long ROAMING_SCAN_INTERVAL_MS = 300000;

bool find_strongest_ap(const char *ssid, int &channel, uint8_t *bssid, int &rssi) {
	int count = WiFi.scanNetworks(false, true);
	int strongest = -127;
	for (int i = 0; i < count; i++) {
		if (WiFi.SSID(i) != ssid || WiFi.RSSI(i) <= strongest) continue;
		strongest = WiFi.RSSI(i);
		channel = WiFi.channel(i);
		memcpy(bssid, WiFi.BSSID(i), 6);
	}
	WiFi.scanDelete();
	if (strongest == -127) return false;
	rssi = strongest;
	return true;
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
		wirelessinfo += WiFi.SSID(i);
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

void maintain_wifi_connection(const char *ssid, const char *pass) {
	static unsigned long last_scan = 0;
	static bool has_scanned = false;
	if (!ssid || !pass || WiFi.status() != WL_CONNECTED) return;
	if (has_scanned && millis() - last_scan < ROAMING_SCAN_INTERVAL_MS) return;
	last_scan = millis();
	has_scanned = true;

	int channel;
	int strongest_rssi;
	uint8_t strongest_bssid[6];
	if (!find_strongest_ap(ssid, channel, strongest_bssid, strongest_rssi)) return;
	if (strongest_rssi <= WiFi.RSSI() + ROAMING_MARGIN_DB) return;
	if (memcmp(strongest_bssid, WiFi.BSSID(), 6) == 0) return;

	DEBUG_PRINT(F("Roaming to stronger access point (RSSI: "));
	DEBUG_PRINT(strongest_rssi);
	DEBUG_PRINTLN(F(")"));
	WiFi.begin(ssid, pass, channel, strongest_bssid, true);
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
	WiFi.begin(ssid, pass);
	WiFi.setSleep(false); // work-around for ARP issue: disable sleep mode
	WiFi.setOutputPower(20.5);
	WiFi.setAutoReconnect(true); // enable auto reconnect
}

void start_network_sta_with_ap(const char *ssid, const char *pass, const char *hostname) {
	start_network_sta(ssid, pass, hostname, false);
}

void start_network_sta(const char *ssid, const char *pass, const char *hostname) {
	start_network_sta(ssid, pass, hostname, true);
}

