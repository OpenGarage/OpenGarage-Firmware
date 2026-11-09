#include "crash_diagnostics.h"
#include <FS.h>
#include <new>
#include <user_interface.h>
#include "defines.h"

extern "C" {
extern void *umm_last_fail_alloc_addr;
extern int umm_last_fail_alloc_size;
}

namespace og_crash {
namespace {

volatile uint32_t *const RTC_USER = reinterpret_cast<volatile uint32_t *>(RTC_BASE);
const char HISTORY_FNAME[] = "/crashes.dat";

Record last_;           // newest stored record; also scratch for the crash handler
bool have_last_ = false, new_this_boot_ = false;
uint32_t stored_count_ = 0;
uint32_t uptime_s_ = 0, free_heap_ = 0;
StageTimer timer_;

void read_rtc(size_t offset, void *dst, size_t bytes) {
  uint32_t *w = static_cast<uint32_t *>(dst);
  for (size_t i = 0; i < bytes / 4; ++i) w[i] = RTC_USER[offset + i];
}

void write_rtc(size_t offset, const void *src, size_t bytes) {
  const uint32_t *w = static_cast<const uint32_t *>(src);
  for (size_t i = 0; i < bytes / 4; ++i) RTC_USER[offset + i] = w[i];
}

void write_crumb(uint32_t stage) {
  const Breadcrumb b = make_crumb(stage, uptime_s_, free_heap_);
  write_rtc(0, &b, sizeof(b));
}

bool load_history(History &h) {
  memset(&h, 0, sizeof(h));
  File f = FILESYS.open(HISTORY_FNAME, "r");
  if (!f) return false;
  const bool ok = f.size() == sizeof(h) && f.read(reinterpret_cast<uint8_t *>(&h), sizeof(h)) == sizeof(h);
  f.close();
  if (!ok) memset(&h, 0, sizeof(h));
  return ok;
}

bool save_history(const History &h) {
  File f = FILESYS.open(HISTORY_FNAME, "w");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t *>(&h), sizeof(h)) == sizeof(h);
  f.close();
  return ok;
}

void append_hex(DebugJson &json, uint32_t v) {
  char buf[11];
  snprintf(buf, sizeof(buf), "0x%08x", (unsigned)v);
  json += buf;
}

void append_hex_field(DebugJson &json, const __FlashStringHelper *name, uint32_t v) {
  json += F(",\""); json += name; json += F("\":\""); append_hex(json, v); json += '"';
}

}  // namespace

void begin() {
  const rst_info *ri = ESP.getResetInfoPtr();
  const BootRegisters boot{ri->reason, ri->exccause, ri->epc1, ri->epc2, ri->epc3, ri->excvaddr, ri->depc};
  Record saved; read_rtc(RTC_RECORD_OFFSET, &saved, sizeof(saved));
  Breadcrumb crumb; read_rtc(0, &crumb, sizeof(crumb));

  // The history is about 1 KB: keep it off the 4 KB loop stack.
  History *h = new (std::nothrow) History;
  if (h) {
    load_history(*h);
    Record rec;
    if (build_boot_record(boot, &saved, &crumb, next_seq(*h), rec)) {
      push(*h, rec);
      new_this_boot_ = save_history(*h);
    }
    stored_count_ = h->count;
    const Record *n = newest(*h, 0);
    if (n) { last_ = *n; have_last_ = true; }
    delete h;
  }
  // Consume the handler record and breadcrumb so the next boot starts clean.
  RTC_USER[RTC_RECORD_OFFSET] = 0;
  RTC_USER[0] = 0;
}

void loop_start(uint32_t uptime_s) {
  timer_.mark(STAGE_LOOP_START, micros());
  uptime_s_ = uptime_s;
  free_heap_ = ESP.getFreeHeap();
  write_crumb(STAGE_LOOP_START);
}

void mark(Stage stage) {
  timer_.mark(stage, micros());
  write_crumb(stage);
}

void append_json(DebugJson &json, unsigned index) {
  json += F(",\"crash_count\":"); json += stored_count_;
  json += F(",\"crash_new\":"); json += new_this_boot_ ? 1 : 0;
  json += F(",\"crash\":");
  Record r;
  bool found = false;
  if (index == 0) { found = have_last_; if (found) r = last_; }
  else if (index < HISTORY_MAX) {
    History *h = new (std::nothrow) History;
    if (h) {
      load_history(*h);
      const Record *n = newest(*h, index);
      if (n) { r = *n; found = true; }
      delete h;
    }
  }
  if (!found) json += F("null");
  else {
    json += F("{\"index\":"); json += index;
    json += F(",\"seq\":"); json += r.seq;
    json += F(",\"boot_reason\":"); json += r.boot_reason;
    json += F(",\"crash_reason\":");
    if (r.crash_reason == UINT32_MAX) json += F("-1"); else json += r.crash_reason;
    json += F(",\"exccause\":"); json += r.exccause;
    append_hex_field(json, F("epc1"), r.epc1);
    append_hex_field(json, F("epc2"), r.epc2);
    append_hex_field(json, F("epc3"), r.epc3);
    append_hex_field(json, F("excvaddr"), r.excvaddr);
    append_hex_field(json, F("depc"), r.depc);
    append_hex_field(json, F("sp"), r.sp);
    append_hex_field(json, F("stack_end"), r.stack_end);
    json += F(",\"uptime_s\":");
    if (r.uptime_s == UINT32_MAX) json += F("-1"); else json += r.uptime_s;
    json += F(",\"stage\":\""); json += stage_name(r.stage); json += '"';
    json += F(",\"free_heap\":"); json += r.free_heap;
    append_hex_field(json, F("fail_alloc_addr"), r.fail_alloc_addr);
    json += F(",\"fail_alloc_size\":"); json += r.fail_alloc_size;
    json += F(",\"trace\":\"");
    for (uint32_t i = 0; i < r.trace_count && i < TRACE_MAX; ++i) {
      if (i) json += ' ';
      append_hex(json, r.trace[i]);
    }
    json += F("\"}");
  }
  json += F(",\"stage_max_us\":{");
  for (uint32_t s = STAGE_LOOP_START; s < STAGE_COUNT; ++s) {
    if (s != STAGE_LOOP_START) json += ',';
    json += '"'; json += stage_name(s); json += F("\":"); json += timer_.max_us(s);
  }
  json += '}';
}

}  // namespace og_crash

// Called by the core's postmortem handler after an exception, software
// watchdog, abort or panic, just before restart. The core has already printed
// the full stack to the serial port; this keeps a compact copy in RTC memory.
// It must not allocate.
extern "C" void custom_crash_callback(struct rst_info *ri, uint32_t stack, uint32_t stack_end) {
  using namespace og_crash;
  Record &r = last_;
  memset(&r, 0, sizeof(r));
  r.crash_reason = ri->reason;
  r.exccause = ri->exccause; r.epc1 = ri->epc1; r.epc2 = ri->epc2;
  r.epc3 = ri->epc3; r.excvaddr = ri->excvaddr; r.depc = ri->depc;
  r.sp = stack; r.stack_end = stack_end;
  r.stage = timer_.current(); r.uptime_s = uptime_s_; r.free_heap = free_heap_;
  r.fail_alloc_addr = reinterpret_cast<uint32_t>(umm_last_fail_alloc_addr);
  r.fail_alloc_size = static_cast<uint32_t>(umm_last_fail_alloc_size);
  size_t words = 0;
  if ((stack & 3) == 0 && stack_end > stack && stack >= 0x3ffe8000u && stack_end <= 0x40000000u)
    words = (stack_end - stack) / 4;
  if (words > 1024) words = 1024;
  r.trace_count = collect_trace(reinterpret_cast<const uint32_t *>(stack), words, r.trace);
  seal(r);
  write_rtc(RTC_RECORD_OFFSET, &r, sizeof(r));
}
