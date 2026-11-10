// NTP source selection, DHCP renewal reconciliation and source attribution, against a
// fake of the lwIP SNTP behavior time_sync.h documents.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../OpenGarage/time_sync.h"

using og_time::Plan;
using og_time::SLOTS;
using og_time::Slot;
using og_time::SyncState;

static const char *const defaults[SLOTS] = {"time.google.com", "time.cloudflare.com", "time.windows.com"};
static const uint32_t A = 0x0501a8c0u, B = 0x0601a8c0u, C = 0x0701a8c0u;  // 192.168.1.5/6/7

// lwIP's SNTP slots and DHCP hook, as modelled in time_sync.h.
struct FakeSntp {
  Slot slots[SLOTS];
  uint8_t reach[SLOTS] = {0, 0, 0};
  bool dhcp_mode = true;            // on by default in this core
  FakeSntp() { for (auto &s : slots) s.clear(); }
  void lease(uint8_t n, const uint32_t *servers) {   // dhcp_set_ntp_servers()
    if (!dhcp_mode || !n) return;
    for (uint8_t i = 0; i < SLOTS; ++i) {
      if (i < n) slots[i].set_ip(servers[i]);
      else slots[i].clear();
    }
  }
  void apply(const Plan &p) {        // apply_ntp_plan(): stop (clears history), mode, servers, init
    dhcp_mode = p.dhcp_updates;
    for (uint8_t i = 0; i < SLOTS; ++i) { slots[i] = p.slots[i]; reach[i] = 0; }
  }
  void poll(uint8_t i, bool answered) { reach[i] = uint8_t(reach[i] << 1) | (answered ? 1 : 0); }
};

// The firmware loop: first configuration, then periodic observe + reconcile.
struct Device {
  FakeSntp sntp;
  SyncState state;
  const char *configured = "";
  uint32_t now = 0;
  void boot() {
    state.capture_dhcp(sntp.slots);
    Plan p = og_time::choose(configured, state.dhcp_boot, defaults);
    sntp.apply(p); state.applied_plan(p, now); state.baseline(sntp.slots, sntp.reach);
  }
  void sample(uint32_t advance_ms = 250) {
    now += advance_ms;
    state.observe(sntp.slots, sntp.reach);
    Plan p;
    if (state.reconcile(configured, sntp.slots, defaults, now, 60000, p)) {
      sntp.apply(p); state.applied_plan(p, now); state.baseline(sntp.slots, sntp.reach);
    }
  }
  void settle() { for (int i = 0; i < 4; ++i) sample(60000); }
};

static bool slot_is(const Slot &s, const char *name) { return s.name[0] && strcmp(s.name, name) == 0; }
static bool slot_is(const Slot &s, uint32_t ip) { return s.dhcp() && s.ip == ip; }

static void selection() {
  const uint32_t none[SLOTS] = {0, 0, 0}, one[SLOTS] = {A, 0, 0}, two[SLOTS] = {A, B, 0}, three[SLOTS] = {A, B, C};
  Plan p = og_time::choose("ntp.lan", one, defaults);
  assert(!p.dhcp_updates && slot_is(p.slots[0], "ntp.lan") && slot_is(p.slots[1], defaults[0]) && slot_is(p.slots[2], defaults[1]));
  p = og_time::choose("Time.Cloudflare.com", none, defaults);   // not listed twice
  assert(slot_is(p.slots[0], "Time.Cloudflare.com") && slot_is(p.slots[1], defaults[0]) && slot_is(p.slots[2], defaults[2]));
  char host64[65]; memset(host64, 'h', 60); memcpy(host64 + 60, ".lan", 5);   // 64 characters
  assert(strlen(host64) == 64);
  p = og_time::choose(host64, none, defaults);
  assert(!p.dhcp_updates && slot_is(p.slots[0], host64));                    // installed whole
  char host65[66]; memset(host65, 'h', 61); memcpy(host65 + 61, ".lan", 5);
  p = og_time::choose(host65, one, defaults);                                // never shortened
  assert(p.dhcp_updates && slot_is(p.slots[0], A) && slot_is(p.slots[1], defaults[0]));
  p = og_time::choose("", none, defaults);
  assert(p.dhcp_updates && slot_is(p.slots[0], defaults[0]) && slot_is(p.slots[2], defaults[2]));
  p = og_time::choose("", one, defaults);
  assert(slot_is(p.slots[0], A) && slot_is(p.slots[1], defaults[0]) && slot_is(p.slots[2], defaults[1]));
  p = og_time::choose("", two, defaults);
  assert(slot_is(p.slots[0], A) && slot_is(p.slots[1], B) && slot_is(p.slots[2], defaults[0]));
  p = og_time::choose("", three, defaults);   // deliberate: no Internet fallback left
  assert(slot_is(p.slots[0], A) && slot_is(p.slots[1], B) && slot_is(p.slots[2], C));
  const uint32_t gap[SLOTS] = {0, B, 0};
  p = og_time::choose("", gap, defaults);
  assert(slot_is(p.slots[0], B) && slot_is(p.slots[1], defaults[0]));
}

static void renewals() {
  // No configured server: each lease is merged, and OG's defaults refill the cleared slots.
  for (uint8_t n = 1; n <= 3; ++n) {
    Device d; d.boot(); d.settle();
    assert(slot_is(d.sntp.slots[0], defaults[0]) && d.sntp.dhcp_mode);
    const uint32_t servers[SLOTS] = {A, B, C};
    d.sntp.lease(n, servers);                   // clears every slot after n
    d.settle();
    for (uint8_t i = 0; i < n; ++i) assert(slot_is(d.sntp.slots[i], servers[i]));
    for (uint8_t i = n; i < SLOTS; ++i) assert(slot_is(d.sntp.slots[i], defaults[i - n]));
    const uint32_t changed[SLOTS] = {C, 0, 0};  // the advertised list changes
    d.sntp.lease(1, changed);
    d.settle();
    assert(slot_is(d.sntp.slots[0], C) && slot_is(d.sntp.slots[1], defaults[0]) && slot_is(d.sntp.slots[2], defaults[1]));
    d.sntp.lease(0, changed);                   // a lease without option 42 changes nothing
    d.settle();
    assert(slot_is(d.sntp.slots[0], C));
  }
  // Boot with an advertised server: it is kept, with the defaults behind it.
  { Device d; const uint32_t s[1] = {A}; d.sntp.lease(1, s); d.boot(); d.settle();
    assert(slot_is(d.sntp.slots[0], A) && slot_is(d.sntp.slots[1], defaults[0]));
    assert(d.state.dhcp_boot[0] == A && !d.state.dhcp_boot[1]); }
  // Configured server: DHCP updates are off, so renewals never displace it.
  { Device d; d.configured = "ntp.lan"; const uint32_t s[SLOTS] = {A, B, C}; d.sntp.lease(1, s); d.boot();
    assert(!d.sntp.dhcp_mode && slot_is(d.sntp.slots[0], "ntp.lan") && d.state.dhcp_boot[0] == A);
    for (uint8_t n = 1; n <= 3; ++n) { d.sntp.lease(n, s); d.settle(); }
    assert(slot_is(d.sntp.slots[0], "ntp.lan") && slot_is(d.sntp.slots[1], defaults[0]) && slot_is(d.sntp.slots[2], defaults[1]));
    // Clearing the option at runtime re-enables DHCP updates; the next lease is merged again.
    d.configured = ""; d.settle();
    assert(d.sntp.dhcp_mode && slot_is(d.sntp.slots[0], defaults[0]));
    d.sntp.lease(1, s); d.settle();
    assert(slot_is(d.sntp.slots[0], A) && slot_is(d.sntp.slots[1], defaults[0]));
    // Setting it again turns updates off before installing it.
    d.configured = "ntp2.lan"; d.settle();
    assert(!d.sntp.dhcp_mode && slot_is(d.sntp.slots[0], "ntp2.lan")); }
  // A stable configuration is never re-applied, and re-applies are rate-limited.
  { Device d; d.boot(); const uint32_t before = d.state.applies;
    for (int i = 0; i < 1000; ++i) d.sample();
    assert(d.state.applies == before);
    const uint32_t s[1] = {A};
    d.sntp.lease(1, s); d.sample(); d.sample();
    assert(d.state.applies <= before + 1);
    d.sntp.lease(1, s); for (int i = 0; i < 100; ++i) d.sample();   // 25 s: within the interval
    assert(d.state.applies <= before + 1);
    d.settle(); assert(slot_is(d.sntp.slots[1], defaults[0])); }
}

// One poll of slot i, answered or not; a sync fires the callback before the next sample.
static void sync_from(Device &d, uint8_t slot, uint32_t uptime_s) {
  d.sntp.poll(slot, true); d.state.on_sync(uptime_s); d.sample();
}

static void attribution() {
  // The first reply can arrive before the first sample after installing the list.
  { Device d; d.boot();
    sync_from(d, 0, 1);
    assert(strcmp(d.state.last_source, defaults[0]) == 0); }
  { Device d; d.boot(); d.sample();
    sync_from(d, 0, 10);
    assert(strcmp(d.state.last_source, defaults[0]) == 0);
    for (int i = 0; i < 10; ++i) sync_from(d, 0, 20 + i);   // steady 0xFF: still slot 0
    assert(d.sntp.reach[0] == 0xff && strcmp(d.state.last_source, defaults[0]) == 0); }
  // Codex's case: slot 0 answered earlier (bit 0 still set, no new poll after a DNS failure),
  // then slot 1 answers. The changed history identifies slot 1, not slot 0.
  { Device d; d.boot(); d.sample();
    sync_from(d, 0, 10);
    sync_from(d, 1, 20);
    assert(strcmp(d.state.last_source, defaults[1]) == 0); }
  // Ambiguous: two slots changed with bit 0 set in one sample.
  { Device d; d.boot(); d.sample();
    d.sntp.poll(0, true); d.sntp.poll(1, true); d.state.on_sync(5); d.sample();
    assert(strcmp(d.state.last_source, "unknown") == 0); }
  // A slot whose server just changed keeps lwIP's old history: not trusted.
  { Device d; d.boot(); d.sample();
    sync_from(d, 0, 10);
    const uint32_t s[1] = {A}; d.sntp.lease(1, s);            // slot 0 is now A, history 0x01
    d.state.on_sync(11); d.sample();
    assert(strcmp(d.state.last_source, "unknown") == 0); }
  // Codex's case: DHCP servers A and B have both answered; a renewal replaces A with C
  // (B unchanged), then C answers. C's identity changed, so B's stale bit must not win.
  { Device d; const uint32_t ab[2] = {A, B}; d.sntp.lease(2, ab); d.boot(); d.sample();
    sync_from(d, 0, 10); sync_from(d, 1, 20);
    assert(d.sntp.reach[0] == 1 && d.sntp.reach[1] == 1 && strcmp(d.state.last_source, "192.168.1.6") == 0);
    const uint32_t cb[2] = {C, B}; d.sntp.lease(2, cb);
    assert(slot_is(d.sntp.slots[0], C) && slot_is(d.sntp.slots[1], B));
    d.sntp.poll(0, true); assert(d.sntp.reach[0] == 3);
    d.state.on_sync(30); d.sample();
    assert(strcmp(d.state.last_source, "unknown") == 0);
    sync_from(d, 0, 40);                     // once C's identity is settled, it is attributed
    assert(strcmp(d.state.last_source, "192.168.1.7") == 0); }
  // A DHCP slot is reported by address.
  { Device d; const uint32_t s[1] = {A}; d.sntp.lease(1, s); d.boot(); d.sample();
    sync_from(d, 0, 10);
    assert(strcmp(d.state.last_source, "192.168.1.5") == 0); }
}

static void diagnostics() {
  char text[20];
  og_time::format_ip4(A, text, sizeof text); assert(strcmp(text, "192.168.1.5") == 0);
  og_time::format_ip4(0xffffffffu, text, sizeof text); assert(strcmp(text, "255.255.255.255") == 0);
  SyncState s;
  assert(s.sync_age_s(100) == -1);
  s.on_sync(100);
  assert(s.sync_age_s(100) == 0 && s.sync_age_s(160) == 60);
  assert(s.sync_age_s(100 + 60u * 86400u) == 60L * 86400L);   // beyond millis()'s 49.7 days
  Slot named; named.set_name(nullptr); assert(named.empty());
  char longname[100]; memset(longname, 'a', sizeof longname - 1); longname[99] = 0;
  assert(!named.set_name(longname) && named.empty());   // too long: refused, not truncated
  Slot dhcp; dhcp.set_ip(A); assert(dhcp.dhcp() && dhcp.identity() == A && named.identity() != A);
}

int main() {
  selection();
  renewals();
  attribution();
  diagnostics();
  printf("Time sync: selection, renewals with 1-3 advertised servers, configured-server priority, "
         "rate-limited reconcile, ambiguous attribution and diagnostics passed\n");
}
