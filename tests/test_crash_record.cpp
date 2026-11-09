#include "../OpenGarage/crash_record.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace og_crash;

static Record handler_record() {
  Record r{};
  r.crash_reason = RESET_EXCEPTION;
  r.exccause = 28; r.epc1 = 0x40212345; r.excvaddr = 0x10;
  r.sp = 0x3ffffd00; r.stack_end = 0x3fffffb0;
  r.trace_count = 2; r.trace[0] = 0x40201234; r.trace[1] = 0x40105678;
  seal(r);
  return r;
}

int main() {
  // RTC layout: user memory only (the SDK writes reset info at 0x60001100 on
  // every reset), after the core's 128-byte eboot command block.
  assert(RTC_BASE == 0x60001280);
  assert(RTC_BASE + (RTC_RECORD_OFFSET + sizeof(Record) / 4) * 4 <= 0x60001400);

  // Code-address filter and trace collection.
  assert(is_code_address(0x40100000) && is_code_address(0x4010fffc));
  assert(is_code_address(0x40201010) && is_code_address(0x402fffff));
  assert(!is_code_address(0x3ffe8000) && !is_code_address(0x40110000));
  assert(!is_code_address(0x40200000) && !is_code_address(0x40300000) && !is_code_address(0));
  uint32_t stack[TRACE_MAX + 20];
  for (size_t i = 0; i < sizeof(stack) / 4; ++i) stack[i] = (i % 2) ? 0x40201100 + i : 0x3fff0000;
  uint32_t trace[TRACE_MAX];
  assert(collect_trace(stack, 6, trace) == 3 && trace[0] == 0x40201101 && trace[2] == 0x40201105);
  for (size_t i = 0; i < sizeof(stack) / 4; ++i) stack[i] = 0x40201100;
  assert(collect_trace(stack, sizeof(stack) / 4, trace) == TRACE_MAX);

  // Checksums reject corruption and power-on garbage.
  Record r = handler_record();
  assert(valid(r));
  Record bad = r; bad.epc1 ^= 1; assert(!valid(bad));
  bad = r; bad.trace_count = TRACE_MAX + 1; seal(bad); assert(!valid(bad));
  Record garbage; memset(&garbage, 0xa5, sizeof(garbage)); assert(!valid(garbage));
  Breadcrumb crumb = make_crumb(STAGE_CLOUD, 148106, 20576);
  assert(valid(crumb));
  Breadcrumb bc = crumb; bc.uptime_s++; assert(!valid(bc));
  bc = crumb; bc.tag = 0x12345608; assert(!valid(bc));
  Breadcrumb zero{}; assert(!valid(zero));

  // Exception boot: boot registers win, trace and crumb are merged.
  BootRegisters ex{RESET_EXCEPTION, 29, 0x40299999, 1, 2, 0x44, 3};
  Record out;
  assert(build_boot_record(ex, &r, &crumb, 7, out));
  assert(valid(out) && out.seq == 7 && out.boot_reason == RESET_EXCEPTION);
  assert(out.crash_reason == RESET_EXCEPTION && out.exccause == 29 && out.epc1 == 0x40299999);
  assert(out.excvaddr == 0x44 && out.trace_count == 2 && out.trace[1] == 0x40105678);
  assert(out.stage == STAGE_CLOUD && out.uptime_s == 148106 && out.free_heap == 20576);

  // Hardware watchdog: no handler ran; registers and crumb still recorded.
  BootRegisters wdt{RESET_HW_WDT, 4, 0x40100abc, 0, 0, 0, 0};
  assert(build_boot_record(wdt, &garbage, &crumb, 1, out));
  assert(out.crash_reason == UINT32_MAX && out.trace_count == 0 && out.epc1 == 0x40100abc);
  assert(out.stage == STAGE_CLOUD);
  assert(build_boot_record(wdt, nullptr, nullptr, 1, out));
  assert(out.stage == STAGE_NONE && out.uptime_s == UINT32_MAX);

  // Abort can reboot as a software restart: the handler record is kept, and the
  // stale exception registers of a normal restart are not copied.
  BootRegisters soft{RESET_SOFT_RESTART, 0, 0x40201111, 0, 0, 0, 0};
  Record abort_rec = handler_record(); abort_rec.crash_reason = RESET_ABORT; seal(abort_rec);
  assert(build_boot_record(soft, &abort_rec, &crumb, 2, out));
  assert(out.crash_reason == RESET_ABORT && out.epc1 == 0x40212345);
  assert(!build_boot_record(soft, &garbage, &crumb, 2, out));

  // Power-on and deep-sleep boots are not crashes.
  assert(!build_boot_record(BootRegisters{RESET_POWER_ON, 0, 0, 0, 0, 0, 0}, nullptr, &crumb, 1, out));
  assert(!build_boot_record(BootRegisters{RESET_DEEP_SLEEP, 0, 0, 0, 0, 0, 0}, nullptr, &crumb, 1, out));
  // External reset counts only when this firmware left a breadcrumb.
  BootRegisters ext{RESET_EXTERNAL, 0, 0, 0, 0, 0, 0};
  assert(!build_boot_record(ext, nullptr, &zero, 1, out));
  assert(build_boot_record(ext, nullptr, &crumb, 1, out) && out.uptime_s == 148106);

  // History ring keeps the newest four; index 0 is the newest.
  History h{};
  assert(next_seq(h) == 1 && newest(h, 0) == nullptr);
  for (uint32_t i = 0; i < 6; ++i) {
    assert(build_boot_record(ex, &r, &crumb, next_seq(h), out));
    push(h, out);
  }
  assert(h.count == 6 && next_seq(h) == 7);
  assert(newest(h, 0)->seq == 6 && newest(h, 3)->seq == 3 && newest(h, 4) == nullptr);

  // Stage timer attributes elapsed time to the stage that was running.
  StageTimer t;
  t.mark(STAGE_LOOP_START, 100);
  t.mark(STAGE_CLOUD, 150);
  t.mark(STAGE_MQTT, 650150);
  t.mark(STAGE_BETWEEN_LOOPS, 650200);
  t.mark(STAGE_LOOP_START, 650300);
  assert(t.max_us(STAGE_LOOP_START) == 50 && t.max_us(STAGE_CLOUD) == 650000);
  assert(t.max_us(STAGE_MQTT) == 50 && t.max_us(STAGE_BETWEEN_LOOPS) == 100);
  t.mark(STAGE_ALARM, UINT32_MAX - 50);
  t.mark(STAGE_CLOUD, 50);  // micros() wrap
  assert(t.max_us(STAGE_ALARM) == 101);
  assert(t.current() == STAGE_CLOUD && t.max_us(STAGE_COUNT) == 0);
  assert(!strcmp(stage_name(STAGE_BETWEEN_LOOPS), "between_loops") && !strcmp(stage_name(200), "unknown"));

  std::puts("Crash record: checksum, merge rules, history ring and stage timing passed");
}
