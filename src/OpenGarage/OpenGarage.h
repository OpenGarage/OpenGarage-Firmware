/* OpenGarage Firmware
 *
 * OpenGarage library header file
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

#ifndef _OPENGARAGE_H
#define _OPENGARAGE_H

#include <Arduino.h>
#include <FS.h>
#include <WiFi.h>
#include <Ticker.h>
#include <Esp.h>
#include <esp32-hal-gpio.h>

#include "defines.h"

struct OptionStruct {
    String name;
    byte ival;
    uint max;
    String sval;
};

struct LogStruct {
    ulong tstamp; // time stamp
    uint status;  // door status
    uint dist;    // distance
};

class OpenGarage {
public:
    static OptionStruct options[];
    static byte state;
    static int alarm;
    static byte led_reverse;

    static void begin();

    static void options_setup();

    static void options_load();

    static void options_save();

    static void options_reset();

    static void restart() { ESP.restart(); } //digitalWrite(PIN_RESET, LOW); }
    static uint read_distance(); // centimeter
    static byte get_mode() { return options[OPTION_MOD].ival; }

    static int get_button() { return digitalRead(PIN_BUTTON); }

    static int get_switch() { return digitalRead(PIN_SWITCH); }

    static byte get_led() { return static_cast<byte>(led_reverse ? (!digitalRead(PIN_LED)) : digitalRead(PIN_LED)); }

    static bool get_cloud_access_en();

    static void set_led(byte status) { digitalWrite(PIN_LED, static_cast<uint8_t>(led_reverse ? (!status) : status)); }

    static void set_relay(byte status) { digitalWrite(PIN_RELAY, status); }

    static void click_relay() {
        set_relay(HIGH);
        delay(options[OPTION_CDT].ival);
        set_relay(LOW);
    }

    static int find_option(String name);

    static void log_reset();

    static void write_log(const LogStruct &data);

    static bool read_log_start();

    static bool read_log_next(LogStruct &data);

    static bool read_log_end();

    static void play_note(uint freq);

    static void set_alarm(byte ov = 0) { // ov = override value
        if (ov) alarm = ov * 10 + 1;
        else alarm = options[OPTION_ALM].ival * 10 + 1;
    }

    static void reset_to_ap() {
        options[OPTION_MOD].ival = OG_MOD_AP;
        options_save();
        restart();
    }

    static void config_ip();

    static void play_startup_tune();

private:
    static File log_file;

};

#endif  // _OPENGARAGE_H_
