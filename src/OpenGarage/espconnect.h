/* OpenGarage Firmware
 *
 * ESPConnect header file
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

#ifndef _ESP_CONNECT_H
#define _ESP_CONNECT_H

#define HTML_SUCCESS           0x01
#define HTML_UNAUTHORIZED      0x02
#define HTML_MISMATCH          0x03
#define HTML_DATA_MISSING      0x10
#define HTML_DATA_OUTOFBOUND   0x11
#define HTML_DATA_FORMATERROR  0x12
#define HTML_NOT_PERMITTED     0x30
#define HTML_UPLOAD_FAILED     0x40

#include <WiFi.h>
#include <mdns.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include "defines.h"
#include "htmls.h"

String scan_network();

void start_network_ap(const char *ssid, const char *pass);

void start_network_sta(const char *ssid, const char *pass);

void start_network_sta_with_ap(const char *ssid, const char *pass);

#endif
