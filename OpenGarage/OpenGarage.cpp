/* OpenGarage Firmware
 *
 * OpenGarage library
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

#include "OpenGarage.h"
#include "distance_consensus.h"
#include "config_store.h"

byte  OpenGarage::state = OG_STATE_INITIAL;
File  OpenGarage::log_file;
byte  OpenGarage::alarm = 0;
uint32_t OpenGarage::alarm_sequence = 0;
byte  OpenGarage::alarm_action = 0;
byte  OpenGarage::led_reverse = 0;
byte  OpenGarage::has_swrx = 0;
Ticker ud_ticker;

uint8_t OpenGarage::config_status = uint8_t(og_config::Status::NORMAL);
bool OpenGarage::config_locked = false;
static const char* log_fname = LOG_FNAME;

DallasTemperature* OpenGarage::ds18b20 = NULL;
DHTesp* OpenGarage::dht = NULL;
extern OpenGarage og;
/* Options name, default integer value, max value, default string value
 * Integer options don't have string value
 * String options don't have integer or max value
 */
OptionStruct OpenGarage::options[] = {
	{"fwv", OG_FWV,      255, ""},
	{"sn1", OG_SN1_CEILING,1, ""},
	{"sn2", OG_SN2_NONE,   2, ""},
	{"sno", OG_SNO_1ONLY,  3, ""},
	{"secv",0,             2, ""},
	{"dth", 50,        65535, ""},
	{"vth", 150,       65535, ""},
	{"riv", 1,            30, ""},
	{"alm", OG_ALM_5,      2, ""},
	{"aoo", 0,             1, ""},
	{"lsz", DEFAULT_LOG_SIZE,400,""},
	{"tsn", OG_TSN_NONE, 255, ""},
	{"htp", 80,        65535, ""},
	{"cdt", 1000,       5000, ""},
	{"dri", 500,        3000, ""},
	{"sfi", OG_SFI_CONSENSUS,1,""},
	{"cmr", 10,          100, ""},
	{"sto", 0,             1, ""},
	{"mod", OG_MOD_AP,   255, ""},
	{"ati", 30,          720, ""},
	{"ato", OG_AUTO_NONE,255, ""},
	{"atib", 3,           24, ""},
	{"atob", OG_AUTO_NONE,255, ""},
	{"noto", OG_NOTIFY_DO|OG_NOTIFY_DC,255, ""},
	{"bas", OG_LIGHT_BLINK_FOREVER, OG_LIGHT_BLINK_MAX, ""},
	{"usi", 0,             1, ""},
	{"ssid", 0, 0, ""},  // string options have 0 max value
	{"pass", 0, 0, ""},
	{"cld",  0, 2, ""},
	{"auth", 0, 0, ""},
	{"bdmn", 0, 0, DEFAULT_OTC_DMN},
	{"bprt", DEFAULT_OTC_PRT,65535, ""},
	{"dkey", 0, 0, DEFAULT_DKEY},
	{"name", 0, 0, DEFAULT_NAME},
	{"iftt", 0, 0, ""},
	{"mqen", 0, 1, ""},
	{"mqtt", 0, 0, ""},
	{"mqpt", 1883, 65535, ""},
	{"mqur", 0, 0, ""},
	{"mqpw", 0, 0, ""},
	{"mqtp", 0, 0, ""},
	{"emen", 0, 1, ""},
	{"smtp", 0, 0, DEFAULT_SMTP_SERVER},
	{"sprt", DEFAULT_SMTP_PORT, 65535, ""},
	{"send", 0, 0, ""},
	{"apwd", 0, 0, ""},
	{"recp", 0, 0, ""},
	{"dvip", 0, 0, ""},
	{"gwip", 0, 0, ""},
	{"subn", 0, 0, "255.255.255.0"},
	{"dns1", 0, 0, "8.8.8.8"},
	{"ntp1", 0, 0, ""},
	{"host", 0, 0, ""},
	{"pem", 0, 1, ""},
	{"wfr", 1, 1, ""},
	{"wfg", 0, 1, ""},
};

/* Variables and functions for handling Ultrasonic Distance sensor */
#define KAVG 7  // k average
volatile uint32_t ud_start = 0;
volatile byte ud_i = 0;
volatile boolean fullbuffer = false;
volatile uint32_t ud_buffer[KAVG];
volatile boolean triggered = false;
static const uint32_t UD_TIMEOUT_US = 26000L;  // ~4.5 m round-trip

// start trigger signal
void ud_start_trigger() {
	// Check if the previous trigger timed out (the triggered flag was never cleared by the ISR).
	noInterrupts();
	if (triggered) {
		DEBUG_PRINTLN("a timeout occurred");
		// A timeout occurred because the ECHO pin never went LOW.
		// Check if the user wants to cap the value or ignore it.
		if (og.options[OPTION_STO].ival != 0) {
			// Cap the duration to the maximum value.
			ud_buffer[ud_i] = UD_TIMEOUT_US;
			// Advance the circular buffer.
			ud_i = (ud_i + 1) % KAVG;
			if (ud_i == 0) fullbuffer = true;
		}
	}
	interrupts();

	digitalWrite(PIN_TRIG, LOW);
	delayMicroseconds(2);
	digitalWrite(PIN_TRIG, HIGH);
	delayMicroseconds(20);
	triggered = true;
	digitalWrite(PIN_TRIG, LOW);
}

IRAM_ATTR void ud_isr() {
	if(!triggered) return;

	// ECHO pin went from low to high
	if(digitalRead(PIN_ECHO)==HIGH) {
		ud_start = micros();  // record start time
	} else {
		// ECHO pin went from high to low
		triggered = false;
		ud_buffer[ud_i] = micros() - ud_start; // calculate elapsed time
		if(ud_buffer[ud_i]>UD_TIMEOUT_US) {
			// timedout
			if(og.options[OPTION_STO].ival==0) {
				// ignore
				return;
			} else {
				// cap to max
		  	ud_buffer[ud_i]=UD_TIMEOUT_US;
			}
		} else {
			ud_i = (ud_i+1)%KAVG; // circular buffer
		  if(ud_i==0) fullbuffer=true;
		}
	}
}

void ud_ticker_cb() {
	ud_start_trigger();
}

void OpenGarage::begin() {
	digitalWrite(PIN_BUZZER, LOW);
	pinMode(PIN_BUZZER, OUTPUT);

	digitalWrite(PIN_RELAY, LOW);
	pinMode(PIN_RELAY, OUTPUT);

	has_swrx = 0;
	pinMode(PIN_SWRX_DETECT, INPUT_PULLUP);
	if(digitalRead(PIN_SWRX_DETECT) == 0) {
		digitalWrite(PIN_SW_RX, INPUT); // software rx exists, set it up
		has_swrx = 1;
	}

	// detect LED logic
	pinMode(PIN_LED, INPUT);
	// use median filtering to detect led logic
	byte nl=0, nh=0;
	for(byte i=0;i<KAVG;i++) {
		if(digitalRead(PIN_LED)==HIGH) nh++;
		else nl++;
		delay(50);
	}
	if(nh>nl) { // if we get more HIGH readings
		led_reverse = 1;  // if no external LED connected, reverse logic
		//DEBUG_PRINTLN(F("reverse logic"));
	} else {
		//DEBUG_PRINTLN(F("normal logic"));
	}

	pinMode(PIN_LED, OUTPUT);
	set_led(LOW);

	digitalWrite(PIN_TRIG, HIGH);
	pinMode(PIN_TRIG, OUTPUT);

	pinMode(PIN_ECHO, INPUT);
	pinMode(PIN_BUTTON, INPUT_PULLUP);

	pinMode(PIN_SWITCH, INPUT_PULLUP);

	state = OG_STATE_INITIAL;

	if(!FILESYS.begin()) {
		DEBUG_PRINTLN(F("failed to mount file system!"));
	}
}

// Consume one physical line, including malformed legacy lines. Allocation/read
// failure is fatal; malformed lines can be skipped without interpreting fragments.
enum class ConfigRecord { VALID, SKIP, INCOMPLETE, ERROR };
static ConfigRecord config_record(File &file, String &name, String &value) {
	name=""; value="";
	bool colon=false, malformed=false;
	while(file.available()) {
		int c=file.read();
		if(c<0)return ConfigRecord::ERROR;
		if(c=='\n') {
			value.trim();
			return colon && name.length() && !malformed ? ConfigRecord::VALID : ConfigRecord::SKIP;
		}
		if(malformed)continue;
		if(c==0){malformed=true;continue;}
		if(!colon) {
			if(c==':'){colon=true;continue;}
			if(c=='\r'||name.length()>=16){malformed=true;continue;}
			if(!name.concat(char(c)))return ConfigRecord::ERROR;
		} else {
			if(value.length()>=2049){malformed=true;continue;}
			if(!value.concat(char(c)))return ConfigRecord::ERROR;
		}
	}
	return ConfigRecord::INCOMPLETE; // EOF, distinct from a read/allocation failure.
}
static bool config_number(const String &value, unsigned maximum, unsigned long &number) {
	char *end=nullptr;
	number=strtoul(value.c_str(),&end,10);
	return value.length() && value[0]!='-' && end && !*end && number<=maximum;
}
static bool valid_legacy_config(File &file) {
	if(file.size()>16384)return false;
	bool first=true, key_seen=false;
	String name,value;
	while(file.available()) {
		auto record=config_record(file,name,value);
		if(record==ConfigRecord::ERROR)return false;
		if(first && (record!=ConfigRecord::VALID || name!="fwv"))return false;
		first=false;
		// A damaged/newer envelope must never downgrade to legacy validation.
		if(name=="ogcfg" || name=="ogcrc")return false;
		if(name=="dkey") {
			if(key_seen || record!=ConfigRecord::VALID || !value.length())return false;
			key_seen=true;
		}
	}
	// FWV did not change for every schema change. Recover the credential rather
	// than requiring a version-specific option list. Missing fields use defaults.
	// A torn non-key tail is ignored; a torn key was rejected above.
	return !first && key_seen;
}
static_assert(NUM_OPTIONS<64,"Expand config validation presence mask");

void OpenGarage::options_setup() {
 auto r=og_config::recover(FILESYS,valid_legacy_config);
 config_status=uint8_t(r.status);
 if(r.status==og_config::Status::FRESH) { options_save(); return; }
 if(!r.path || !options_load()) {
  config_locked=true;config_status=uint8_t(og_config::Status::CORRUPT);
  options[OPTION_MOD].ival=OG_MOD_AP;return;
 }
 config_status=uint8_t(r.status);
 if(r.kind==og_config::Kind::LEGACY || options[OPTION_FWV].ival!=OG_FWV) {
  options[OPTION_FWV].ival=OG_FWV;
  if(options_save() && r.kind==og_config::Kind::LEGACY) config_status=uint8_t(og_config::Status::MIGRATED);
 }
}

bool OpenGarage::options_reset() {
 if(!og_config::reset(FILESYS)){config_status=uint8_t(og_config::Status::SAVE_FAILED);return false;}
 config_locked=false;return true;
}

void OpenGarage::log_reset() {
	if(!FILESYS.remove(log_fname)) {
		DEBUG_PRINTLN(F("failed to remove log file"));
		return;
	}else{DEBUG_PRINTLN(F("Removed log file"));}
	DEBUG_PRINTLN(F("ok"));
}

int OpenGarage::find_option(String name) {
	for(byte i=0;i<NUM_OPTIONS;i++) {
		if(name == options[i].name) {
		  return i;
		}
	}
	return -1;
}

// Flash-resident defaults for missing/invalid old fields. Tests compare both
// tables with options[]. Strings reuse the literals already used by options[].
static const uint16_t config_int_defaults[] PROGMEM = {
	OG_FWV, OG_SN1_CEILING, OG_SN2_NONE, OG_SNO_1ONLY, 0, 50, 150, 1,
	OG_ALM_5, 0, DEFAULT_LOG_SIZE, OG_TSN_NONE, 80, 1000, 500, OG_SFI_CONSENSUS,
	10, 0, OG_MOD_AP, 30, OG_AUTO_NONE, 3, OG_AUTO_NONE, OG_NOTIFY_DO|OG_NOTIFY_DC,
	OG_LIGHT_BLINK_FOREVER, 0, 0, 0, 0, 0, 0, DEFAULT_OTC_PRT,
	0, 0, 0, 0, 0, 1883, 0, 0,
	0, 0, 0, DEFAULT_SMTP_PORT, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 0
};
static const char *const config_string_defaults[] PROGMEM = {
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "", DEFAULT_OTC_DMN, "",
	DEFAULT_DKEY, DEFAULT_NAME, "", "", "", "", "", "",
	"", "", DEFAULT_SMTP_SERVER, "", "", "", "", "",
	"", "255.255.255.0", "8.8.8.8", "", "", "", "", ""
};
static_assert(sizeof(config_int_defaults)/sizeof(config_int_defaults[0])==NUM_OPTIONS,"Update config defaults");
static_assert(sizeof(config_string_defaults)/sizeof(config_string_defaults[0])==NUM_OPTIONS,"Update config defaults");
static bool config_defaults_missing(uint64_t seen,bool apply) {
	for(unsigned i=0;i<NUM_OPTIONS;++i) {
		if(seen&(uint64_t(1)<<i))continue;
		auto &o=OpenGarage::options[i];
		if(o.max) {
			if(apply)o.ival=pgm_read_word(&config_int_defaults[i]);
		} else {
			const char *value=reinterpret_cast<const char *>(pgm_read_ptr(&config_string_defaults[i]));
			if(!o.sval.reserve(strlen(value)))return false;
			if(apply)o.sval=value;
		}
	}
	return true;
}

bool OpenGarage::options_load() {
 auto r=og_config::recover(FILESYS,valid_legacy_config);
 if(!r.path)return false;
 uint64_t seen=0;
 // Reserve before changing values, so low-memory reload cannot partly replace keys.
 for(unsigned pass=0;pass<2;++pass) {
  File f=FILESYS.open(r.path,"r");if(!f)return false;
  String name,value;
  if(pass && !config_defaults_missing(seen,true)){f.close();return false;}
  while(f.available()) {
   auto record=config_record(f,name,value);
   if(record==ConfigRecord::ERROR || (record!=ConfigRecord::VALID && r.kind!=og_config::Kind::LEGACY)){f.close();return false;}
   if(record!=ConfigRecord::VALID)continue;
   int idx=find_option(name);if(idx<0)continue;
   unsigned long number=0;
   if(options[idx].max && !config_number(value,options[idx].max,number))continue;
   if(!pass)seen|=uint64_t(1)<<idx;
   if(!options[idx].max && !options[idx].sval.reserve(value.length())){f.close();return false;}
   if(pass) {
    if(options[idx].max)options[idx].ival=number;
    else options[idx].sval=value;
   }
  }
  f.close();
  if(!pass && !config_defaults_missing(seen,false))return false;
 }
 return true;
}

bool OpenGarage::options_save() {
 if(config_locked)return false;
 bool ok=og_config::save(FILESYS,valid_legacy_config,[](auto &out) {
  for(byte i=0;i<NUM_OPTIONS;++i) {
   const auto &o=options[i];
   // The legacy-readable record format cannot represent embedded line endings.
   if(!o.max && (o.sval.indexOf('\r')>=0 || o.sval.indexOf('\n')>=0 || o.sval.length()>2048 || strlen(o.sval.c_str())!=o.sval.length()))return false;
   if(!out.text(o.name)||!out.text(":")||!(o.max?out.number(o.ival):out.write(o.sval.c_str(),o.sval.length()))||!out.text("\r\n"))return false;
  }
  return true;
 });
 if(!ok) {
  config_status=uint8_t(og_config::Status::SAVE_FAILED);
  // Reload the authoritative durable values before replying. Usually these
  // are the old settings; recovery may select a verified pending replacement.
  if(!options_load()) {DEBUG_PRINTLN(F("Settings reload failed; configuration locked"));config_locked=true;options[OPTION_MOD].ival=OG_MOD_AP;options[OPTION_ATO].ival=0;options[OPTION_ATOB].ival=0;reset_alarm();}
 }
 if(ok && config_status==uint8_t(og_config::Status::SAVE_FAILED))config_status=uint8_t(og_config::Status::NORMAL);
 return ok;
}

uint OpenGarage::read_distance() {
	static uint last_returned;
	uint32_t buf[KAVG];
	if(!fullbuffer) {
		last_returned = (ud_i>0)? (uint)(ud_buffer[ud_i-1]*0.01716f) : 0;
		return last_returned;
	}
	for(byte i=0;i<KAVG;i++) {
		buf[i] = ud_buffer[i];
	}

	// noise filtering methods
	if(options[OPTION_SFI].ival == OG_SFI_MEDIAN) {
		// partial sorting of buf to perform median filtering
		byte out, in;
		for(out=1; out<=KAVG/2; out++){
			uint32_t temp = buf[out];
			in = out;
			while(in>0 && buf[in-1]>temp) {
				buf[in] = buf[in-1];
				in--;
			}
			buf[in] = temp;
		}
		last_returned = (uint)(buf[KAVG/2]*0.01716f);  // 34320 cm / 2 / 10^6 s
		return last_returned;
	} else {
		// Consensus of the tightest five out of seven; tolerate two outliers.
		// calculate margin
		uint32_t margin = (float)options[OPTION_CMR].ival/0.01716f;
		margin = (margin<60)?60:margin;
		uint32_t mean;
		if(og_filter::tightest_five(buf, margin, mean)) {
			last_returned = mean*0.01716f;
		}
		return last_returned;
	}
}

void OpenGarage::init_sensors() {
	// set up distance sensors
	ud_ticker.attach_ms(options[OPTION_DRI].ival, ud_ticker_cb);
	attachInterrupt(PIN_ECHO, ud_isr, CHANGE);

	switch(options[OPTION_TSN].ival) {
	case OG_TSN_DHT11:
		dht = new DHTesp();
		dht->setup(PIN_TH, DHTesp::DHT11);
		break;
	case OG_TSN_DHT22:
		dht = new DHTesp();
		dht->setup(PIN_TH, DHTesp::DHT22);
		break;
	case OG_TSN_DS18B20:
		OneWire *oneWire = new OneWire(PIN_TH);
		ds18b20 = new DallasTemperature(oneWire);
		ds18b20->begin();
		break;
	}
}

void OpenGarage::read_TH_sensor(float& C, float& H) {
	float v;
	switch(options[OPTION_TSN].ival) {
	case OG_TSN_DHT11:
	case OG_TSN_DHT22:
		if(dht) {
			TempAndHumidity th = dht->getTempAndHumidity();
			v = th.temperature;
			if(!isnan(v)) C=v;
			v = th.humidity;
			if(!isnan(v)) H=v;
		}
		break;

	case OG_TSN_DS18B20:
		if(ds18b20) {
			ds18b20->requestTemperatures();
			v=ds18b20->getTempCByIndex(0);
			if(!isnan(v)) C=v;
		}
		break;
	}
}

void OpenGarage::write_log(const LogStruct& data) {
	File file;
	uint curr = 0;
	DEBUG_PRINTLN(F("saving log data..."));
	if(!FILESYS.exists(log_fname)) {  // create log file
		file = FILESYS.open(log_fname, "w");
		if(!file) {
			DEBUG_PRINTLN(F("failed"));
			return;
		}
		// fill log file
		uint next = curr+1;
		file.write((const byte*)&next, sizeof(next));
		file.write((const byte*)&data, sizeof(LogStruct));
		LogStruct l;
		l.tstamp = 0;
		for(;next<MAX_LOG_SIZE;next++) {  // pre-fill the log file to maximum size
			file.write((const byte*)&l, sizeof(LogStruct));
		}
	} else {
		file = FILESYS.open(log_fname, "r+");
		if(!file) {
			DEBUG_PRINTLN(F("failed"));
			return;
		}
		file.readBytes((char*)&curr, sizeof(curr));
		uint next = (curr+1) % options[OPTION_LSZ].ival;
		file.seek(0, SeekSet);
		file.write((const byte*)&next, sizeof(next));

		file.seek(sizeof(curr)+curr*sizeof(LogStruct), SeekSet);
		file.write((const byte*)&data, sizeof(LogStruct));
	}
	DEBUG_PRINTLN(F("ok"));
	file.close();
}

bool OpenGarage::read_log_start() {
	if(log_file) log_file.close();
	log_file = FILESYS.open(log_fname, "r");
	if(!log_file) return false;
	uint curr;
	if(log_file.readBytes((char*)&curr, sizeof(curr)) != sizeof(curr)) { log_file.close(); return false; }
	if(curr>=MAX_LOG_SIZE) { log_file.close(); return false; }
	return true;
}

bool OpenGarage::read_log_next(LogStruct& data) {
	if(!log_file) return false;
	if(log_file.readBytes((char*)&data, sizeof(LogStruct)) != sizeof(LogStruct)) return false;
	return true;
}

bool OpenGarage::read_log_end() {
	if(!log_file) return false;
	log_file.close();
	return true;
}

void OpenGarage::play_note(uint freq) {
	if(freq>0) {
		tone(PIN_BUZZER, freq);
	} else {
		noTone(PIN_BUZZER);
	}
}

void OpenGarage::config_ip() {
	if(options[OPTION_USI].ival) {
		IPAddress dvip, gwip, subn, dns1;
		if(dvip.fromString(options[OPTION_DVIP].sval) &&
		   gwip.fromString(options[OPTION_GWIP].sval) &&
		   subn.fromString(options[OPTION_SUBN].sval) &&
		   dns1.fromString(options[OPTION_DNS1].sval)) {
			WiFi.config(dvip, gwip, subn, dns1, gwip);
		}
	}
}
