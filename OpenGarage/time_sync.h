#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// NTP source selection and diagnostics, kept free of SDK calls so it can be
// host-tested. main.cpp reads lwIP's SNTP slots into Slot values, applies Plans
// and feeds observations back.
//
// lwIP behavior this models (ESP8266 core 3.1.2, lwIP2):
//  - 3 slots (SNTP_MAX_SERVERS). A slot holds a host name (set by OG) or a bare
//    IPv4 address (set by DHCP option 42); polls rotate to the next slot on failure.
//  - DHCP updates are on by default. A lease with n>0 NTP servers installs them in
//    slots 0..n-1 as bare addresses and clears the remaining slots; a lease with
//    none changes nothing.
//  - Per-slot reachability: shifted left when a poll is sent, bit 0 set when it is
//    answered. Changing a slot's server (OG or DHCP) keeps its history; sntp_stop()
//    clears every slot's history.
namespace og_time {

static const uint8_t SLOTS = 3;
static const size_t MAX_NAME = 64;    // the UI and /co limit for the NTP server option

struct Slot {
  char name[MAX_NAME + 1];  // host name or address text OG installed ("" for a DHCP slot)
  uint32_t ip;        // resolved or DHCP-installed IPv4, lwIP byte order (0 = none)

  bool empty() const { return !name[0] && !ip; }
  bool dhcp() const { return !name[0] && ip; }
  // Named slots are compared by name (their resolved address may change).
  bool same(const Slot &o) const { return name[0] || o.name[0] ? strcmp(name, o.name) == 0 : ip == o.ip; }
  // Installs `n` only if it fits whole: a shortened host name would query a different server.
  bool set_name(const char *n) {
    ip = 0;
    if (!n || strlen(n) > MAX_NAME) { name[0] = 0; return false; }
    memcpy(name, n, strlen(n) + 1);
    return true;
  }
  void set_ip(uint32_t a) { name[0] = 0; ip = a; }
  void clear() { name[0] = 0; ip = 0; }
  uint32_t identity() const {        // FNV-1a of the name, or the bare address
    if (!name[0]) return ip;
    uint32_t h = 2166136261u;
    for (const char *p = name; *p; ++p) { h ^= uint8_t(*p); h *= 16777619u; }
    return h | 1u;                   // never 0, never equal to "empty"
  }
};

// lwIP keeps IPv4 addresses in network byte order: on the little-endian ESP8266
// the first octet is the lowest byte of the u32.
inline void format_ip4(uint32_t addr, char *out, size_t size) {
  snprintf(out, size, "%u.%u.%u.%u", unsigned(addr & 0xff), unsigned((addr >> 8) & 0xff),
           unsigned((addr >> 16) & 0xff), unsigned(addr >> 24));
}
inline void slot_text(const Slot &s, char *out, size_t size) {
  if (s.name[0]) snprintf(out, size, "%s", s.name);
  else if (s.ip) format_ip4(s.ip, out, size);
  else if (size) *out = 0;
}

// The SNTP configuration OG installs.
struct Plan {
  bool dhcp_updates;    // allow DHCP leases to install advertised servers
  Slot slots[SLOTS];
};

// Order: the configured server; else DHCP-advertised servers; then Internet
// defaults in whatever slots remain. With a configured server, DHCP updates are
// off so a renewal can never displace it. Three advertised servers fill every
// slot, leaving no Internet fallback: a deliberate network setting is respected.
inline Plan choose(const char *configured, const uint32_t advertised[SLOTS], const char *const defaults[SLOTS]) {
  Plan p;
  for (uint8_t i = 0; i < SLOTS; ++i) p.slots[i].clear();
  uint8_t n = 0, d = 0;
  // A configured value too long to install whole (saved before /co bounded it) is ignored.
  if (configured && strlen(configured) > MAX_NAME) configured = "";
  if (configured && *configured) {
    p.dhcp_updates = false;
    p.slots[n++].set_name(configured);
  } else {
    p.dhcp_updates = true;
    for (uint8_t i = 0; i < SLOTS && n < SLOTS; ++i)
      if (advertised[i]) p.slots[n++].set_ip(advertised[i]);
  }
  for (; n < SLOTS && d < SLOTS; ++d) {
    // A configured server that is also a default is not listed twice.
    if (configured && *configured && strcasecmp(configured, defaults[d]) == 0) continue;
    p.slots[n++].set_name(defaults[d]);
  }
  return p;
}

// DHCP-installed servers currently in the slots (DHCP never sets names).
inline void advertised_in(const Slot cur[SLOTS], uint32_t out[SLOTS]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SLOTS; ++i) out[i] = 0;
  for (uint8_t i = 0; i < SLOTS; ++i) if (cur[i].dhcp()) out[n++] = cur[i].ip;
}

inline bool same_slots(const Slot a[SLOTS], const Slot b[SLOTS]) {
  for (uint8_t i = 0; i < SLOTS; ++i) if (!a[i].same(b[i])) return false;
  return true;
}

struct SyncState {
  // Boot snapshot: what DHCP advertised before OG installed its first list.
  uint32_t dhcp_boot[SLOTS] = {0, 0, 0};
  bool dhcp_captured = false;

  bool synced = false;
  uint32_t last_sync_s = 0;            // OG uptime (seconds) at the last SNTP sync
  char last_source[64] = "";           // slot text, or "unknown" when ambiguous
  bool source_pending = false;

  uint32_t prev_id[SLOTS] = {0, 0, 0};
  uint8_t prev_reach[SLOTS] = {0, 0, 0};
  bool have_prev = false;

  bool dhcp_updates = true;            // as last applied
  uint32_t last_apply_ms = 0;
  bool applied = false;
  uint32_t applies = 0;

  void capture_dhcp(const Slot cur[SLOTS]) {
    advertised_in(cur, dhcp_boot);
    dhcp_captured = true;
  }

  // From the SNTP time-set callback; attribution happens on the next observe().
  void on_sync(uint32_t uptime_s) {
    synced = true;
    last_sync_s = uptime_s;
    source_pending = true;
  }

  long sync_age_s(uint32_t uptime_s) const { return synced ? long(uptime_s - last_sync_s) : -1; }

  // Which slot answered the latest sync, or -1 when the evidence is ambiguous.
  // A reply changes its slot's history since the previous sample (the send shifts
  // it, the reply sets bit 0); a slot that answers every poll stays 0xFF, so when
  // nothing changed, a single slot with bit 0 set and an unchanged identity is the
  // source. A slot whose server changed (a DHCP renewal keeps lwIP's old history) may
  // be the one that answered, so if it shows bit 0 the result is ambiguous.
  int attribute(const Slot cur[SLOTS], const uint8_t reach[SLOTS]) const {
    if (!have_prev) return -1;
    int changed = -1, changed_count = 0, steady = -1, steady_count = 0;
    for (uint8_t i = 0; i < SLOTS; ++i) {
      if (cur[i].empty() || !(reach[i] & 1)) continue;
      if (cur[i].identity() != prev_id[i]) return -1;
      if (reach[i] != prev_reach[i]) { changed = i; ++changed_count; }
      else { steady = i; ++steady_count; }
    }
    if (changed_count == 1) return changed;
    if (changed_count == 0 && steady_count == 1) return steady;
    return -1;
  }

  // Record the slots without attributing anything: call right after installing a
  // plan, so a reply that arrives before the next sample can still be attributed.
  void baseline(const Slot cur[SLOTS], const uint8_t reach[SLOTS]) {
    for (uint8_t i = 0; i < SLOTS; ++i) { prev_id[i] = cur[i].identity(); prev_reach[i] = reach[i]; }
    have_prev = true;
  }

  // Call regularly (every few hundred ms is enough: polls are >= 15 s apart).
  void observe(const Slot cur[SLOTS], const uint8_t reach[SLOTS]) {
    if (source_pending) {
      const int slot = attribute(cur, reach);
      if (slot >= 0) slot_text(cur[slot], last_source, sizeof(last_source));
      else snprintf(last_source, sizeof(last_source), "unknown");
      source_pending = false;
    }
    baseline(cur, reach);
  }

  // The plan for the current configured server and the slots lwIP now holds, if
  // they differ (e.g. a renewal installed one advertised server and cleared OG's
  // Internet defaults, or the configured server changed). At most one re-apply per
  // `min_interval_ms`, so a disagreement with lwIP can never become an apply loop.
  bool reconcile(const char *configured, const Slot cur[SLOTS], const char *const defaults[SLOTS],
                 uint32_t now_ms, uint32_t min_interval_ms, Plan &plan) const {
    uint32_t adv[SLOTS];
    advertised_in(cur, adv);
    plan = choose(configured, adv, defaults);
    if (same_slots(cur, plan.slots) && plan.dhcp_updates == dhcp_updates) return false;
    return !applied || uint32_t(now_ms - last_apply_ms) >= min_interval_ms;
  }
  void applied_plan(const Plan &plan, uint32_t now_ms) {
    dhcp_updates = plan.dhcp_updates;
    last_apply_ms = now_ms;
    applied = true;
    ++applies;
  }
};

}  // namespace og_time
