#pragma once
#include <Arduino.h>
#include "checked_json.h"
#include "crash_record.h"

// Records why the previous boot ended (exception, watchdog, external reset),
// where its loop was, and candidate return addresses for decoding against the
// build's ELF. See crash_record.h for the stored format.
namespace og_crash {

// Once per boot, after the filesystem is mounted and before the loop runs.
void begin();
// Main-loop breadcrumbs. Cheap: two or four RTC register writes.
void loop_start(uint32_t uptime_s);
void mark(Stage stage);
// Appends ,"crash_count":..,"crash_new":..,"crash":{..}|null,"stage_max_us":{..}
// index 0 is the newest stored record.
void append_json(DebugJson &json, unsigned index);

}  // namespace og_crash
