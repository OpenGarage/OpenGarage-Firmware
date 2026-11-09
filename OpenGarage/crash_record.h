#pragma once
#include <stddef.h>
#include <stdint.h>

// Reset diagnostics kept in RTC user memory, which survives every reset except
// power loss. The crash handler saves registers and likely return addresses;
// the main loop keeps a breadcrumb so a watchdog or manual reset still shows
// where the previous boot was. Pure data and rules, testable on the host.
namespace og_crash {

enum Stage : uint8_t {
  STAGE_NONE = 0, STAGE_LOOP_START, STAGE_FW_UPDATE, STAGE_TIME, STAGE_CHECK_STATUS,
  STAGE_STATION, STAGE_STATE, STAGE_MDNS, STAGE_CLOUD, STAGE_UPDATE_SERVER, STAGE_MQTT,
  STAGE_SECPLUS, STAGE_UI, STAGE_ALARM, STAGE_NOTIFY, STAGE_AUDIO, STAGE_TCP_CLEANUP,
  STAGE_BETWEEN_LOOPS,  // returned from loop(); the core and SDK run until the next one
  STAGE_COUNT
};

inline const char *stage_name(uint32_t stage) {
  static const char *const names[STAGE_COUNT] = {
    "none", "loop_start", "fw_update", "time", "check_status", "station", "state",
    "mdns", "cloud", "update_server", "mqtt", "secplus", "ui", "alarm", "notify",
    "audio", "tcp_cleanup", "between_loops"};
  return stage < STAGE_COUNT ? names[stage] : "unknown";
}

// SDK reset reasons, plus the core's codes for abort/panic and stack smashing.
enum : uint32_t {
  RESET_POWER_ON = 0, RESET_HW_WDT = 1, RESET_EXCEPTION = 2, RESET_SOFT_WDT = 3,
  RESET_SOFT_RESTART = 4, RESET_DEEP_SLEEP = 5, RESET_EXTERNAL = 6,
  RESET_STACK_SMASH = 253, RESET_ABORT = 254
};

static const uint32_t RECORD_MAGIC = 0x4f474352;  // "OGCR"
static const uint32_t CRUMB_MAGIC = 0x4f474200;   // low byte holds the stage
static const size_t TRACE_MAX = 40;

// RTC memory map: 0x60001100-0x600011ff is SDK system data (the reset info the
// SDK writes on every reset lives at its start), and user memory is the 512 bytes
// at 0x60001200. The core's eboot command (OTA) uses the first 32 user words,
// so this layout starts at user word 32 (0x60001280).
static const uintptr_t RTC_USER_MEMORY = 0x60001200;
static const size_t RTC_USER_WORDS = 128;
static const size_t RTC_EBOOT_WORDS = 32;
static const uintptr_t RTC_BASE = RTC_USER_MEMORY + RTC_EBOOT_WORDS * 4;
// Word offsets from RTC_BASE.
static const size_t RTC_CRUMB_WORDS = 4;
static const size_t RTC_RECORD_OFFSET = RTC_CRUMB_WORDS;
static const size_t RTC_USER_WORDS_AVAILABLE = RTC_USER_WORDS - RTC_EBOOT_WORDS;

struct Breadcrumb {
  uint32_t tag;       // CRUMB_MAGIC | stage
  uint32_t uptime_s;
  uint32_t free_heap;
  uint32_t check;     // ~uptime_s ^ tag
};

struct Record {
  uint32_t magic;
  uint32_t seq;          // increases across stored records
  uint32_t boot_reason;  // reason reported by the boot that saved this record
  uint32_t crash_reason; // reason seen by the crash handler; UINT32_MAX if none ran
  uint32_t exccause, epc1, epc2, epc3, excvaddr, depc;
  uint32_t sp, stack_end;
  uint32_t uptime_s;     // previous boot's uptime at its last loop breadcrumb
  uint32_t stage;        // previous boot's loop stage
  uint32_t free_heap;    // free heap at the previous boot's last loop start
  uint32_t fail_alloc_addr, fail_alloc_size;
  uint32_t trace_count;
  uint32_t trace[TRACE_MAX];
  uint32_t checksum;
};

static_assert(sizeof(Breadcrumb) == RTC_CRUMB_WORDS * 4, "breadcrumb layout");
static_assert(RTC_RECORD_OFFSET + sizeof(Record) / 4 <= RTC_USER_WORDS_AVAILABLE,
              "record must fit below the eboot command block");

inline uint32_t checksum_words(const uint32_t *w, size_t n) {
  uint32_t h = 2166136261u;  // FNV-1a over words
  for (size_t i = 0; i < n; ++i) { h ^= w[i]; h *= 16777619u; }
  return h;
}

inline uint32_t record_checksum(const Record &r) {
  return checksum_words(reinterpret_cast<const uint32_t *>(&r), sizeof(Record) / 4 - 1);
}

inline void seal(Record &r) { r.magic = RECORD_MAGIC; r.checksum = record_checksum(r); }

inline bool valid(const Record &r) {
  return r.magic == RECORD_MAGIC && r.trace_count <= TRACE_MAX && r.checksum == record_checksum(r);
}

inline Breadcrumb make_crumb(uint32_t stage, uint32_t uptime_s, uint32_t free_heap) {
  Breadcrumb b{CRUMB_MAGIC | (stage & 0xff), uptime_s, free_heap, 0};
  b.check = ~b.uptime_s ^ b.tag;
  return b;
}

inline bool valid(const Breadcrumb &b) {
  return (b.tag & 0xffffff00u) == CRUMB_MAGIC && b.check == (~b.uptime_s ^ b.tag);
}

// Instruction RAM or mapped flash code on the ESP8266 (4M1M layout).
inline bool is_code_address(uint32_t v) {
  return (v >= 0x40100000u && v < 0x40110000u) || (v >= 0x40201010u && v < 0x40300000u);
}

// Words on the stack that point into code are probably return addresses. They
// include stale frames, so treat them as candidates, nearest the fault first.
inline uint32_t collect_trace(const uint32_t *stack, size_t words, uint32_t *out) {
  uint32_t n = 0;
  for (size_t i = 0; i < words && n < TRACE_MAX; ++i)
    if (is_code_address(stack[i])) out[n++] = stack[i];
  return n;
}

// An external reset (reset button, serial adapter) is recorded only when this
// firmware's breadcrumb shows it was running, so a reflash is not a "crash".
inline bool reason_worth_recording(uint32_t boot_reason, bool crumb_valid) {
  return boot_reason == RESET_HW_WDT || boot_reason == RESET_EXCEPTION ||
         boot_reason == RESET_SOFT_WDT || (boot_reason == RESET_EXTERNAL && crumb_valid);
}

struct BootRegisters {
  uint32_t reason, exccause, epc1, epc2, epc3, excvaddr, depc;
};

// Decide at boot whether the previous boot ended abnormally and, if so, build
// the record to keep. The SDK's boot registers are authoritative; the crash
// handler adds the stack trace; the breadcrumb adds stage and uptime. A valid
// handler record is kept even after a software restart, because abort and
// panic can reboot that way.
inline bool build_boot_record(const BootRegisters &boot, const Record *saved,
                              const Breadcrumb *crumb, uint32_t seq, Record &out) {
  const bool have_saved = saved && valid(*saved);
  const bool have_crumb = crumb && valid(*crumb);
  if (!have_saved && !reason_worth_recording(boot.reason, have_crumb)) return false;
  Record r{};
  if (have_saved) r = *saved;
  else r.crash_reason = UINT32_MAX;
  r.seq = seq;
  r.boot_reason = boot.reason;
  if (boot.reason == RESET_EXCEPTION || boot.reason == RESET_SOFT_WDT || boot.reason == RESET_HW_WDT) {
    r.exccause = boot.exccause; r.epc1 = boot.epc1; r.epc2 = boot.epc2;
    r.epc3 = boot.epc3; r.excvaddr = boot.excvaddr; r.depc = boot.depc;
  }
  if (have_crumb) {
    r.stage = crumb->tag & 0xff;
    r.uptime_s = crumb->uptime_s;
    r.free_heap = crumb->free_heap;
  } else {
    r.stage = STAGE_NONE; r.uptime_s = UINT32_MAX; r.free_heap = 0;
  }
  seal(r);
  out = r;
  return true;
}

// Stored history: a fixed ring of the most recent records.
static const size_t HISTORY_MAX = 4;
struct History {
  Record records[HISTORY_MAX];
  uint32_t count;  // total ever stored; the newest is at (count-1) % HISTORY_MAX
};

inline uint32_t next_seq(const History &h) {
  if (!h.count) return 1;
  const Record &newest = h.records[(h.count - 1) % HISTORY_MAX];
  return valid(newest) ? newest.seq + 1 : h.count + 1;
}

inline void push(History &h, const Record &r) {
  h.records[h.count % HISTORY_MAX] = r;
  ++h.count;
}

// index 0 is the newest; returns nullptr when absent.
inline const Record *newest(const History &h, size_t index) {
  if (index >= HISTORY_MAX || index >= h.count) return nullptr;
  const Record &r = h.records[(h.count - 1 - index) % HISTORY_MAX];
  return valid(r) ? &r : nullptr;
}

// Per-stage worst-case duration, so a slow loop can be attributed.
class StageTimer {
 public:
  void mark(uint32_t stage, uint32_t now_us) {
    if (current_ < STAGE_COUNT && started_) {
      const uint32_t elapsed = now_us - last_us_;
      if (elapsed > max_us_[current_]) max_us_[current_] = elapsed;
    }
    current_ = stage < STAGE_COUNT ? stage : uint32_t(STAGE_NONE);
    last_us_ = now_us;
    started_ = true;
  }
  uint32_t max_us(uint32_t stage) const { return stage < STAGE_COUNT ? max_us_[stage] : 0; }
  uint32_t current() const { return current_; }
 private:
  uint32_t max_us_[STAGE_COUNT] = {};
  uint32_t current_ = STAGE_NONE, last_us_ = 0;
  bool started_ = false;
};

}  // namespace og_crash
