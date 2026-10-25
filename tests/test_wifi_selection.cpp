#include "../OpenGarage/wifi_selection.h"
#include "../OpenGarage/wifi_manager.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using og_wifi::Selection;
using og_wifi::AccessPoint;
using Scan=Selection::Scan;
AccessPoint ap(uint8_t id,int rssi,int channel=1) {
  AccessPoint a;a.valid=true;a.bssid[0]=id;a.rssi=rssi;a.channel=channel;return a;
}
int main() {
  for (uint32_t start : {0U,0xffff8000U}) {
    Selection s;auto old=ap(1,-80), good=ap(2,-60,6), weak=ap(3,-76);
    s.begin(Scan::RECOVERY);s.offer(start,weak,nullptr);s.offer(start,good,nullptr);
    assert(!s.finish(start,-127) && s.target.same(good.bssid));
    s.failed(start);assert(s.plain_next && !s.target.valid);
    s.begin(Scan::RECOVERY);s.offer(start+1,good,nullptr);s.offer(start+1,weak,nullptr);
    s.finish(start+1,-127);assert(s.target.same(weak.bssid));
    s.begin(Scan::RECOVERY);s.offer(start+1800000,good,nullptr);
    s.finish(start+1800000,-127);assert(s.target.same(good.bssid));
    s.reset();s.connected(start);
    // Healthy means no scan, however much time elapses.
    s.sample(start,-50);s.sample(start+3600000,-50);
    assert(s.due(start+3600000,true)==Scan::NONE);
    s.reset();s.connected(start);s.sample(start,-80);
    assert(s.due(start+59999,true)==Scan::NONE);
    assert(s.due(start+60000,false)==Scan::NONE);
    assert(s.due(start+60000,true)==Scan::SEARCH);
    s.begin(Scan::SEARCH);s.offer(start+60000,old,old.bssid);s.offer(start+60000,good,old.bssid);
    assert(!s.finish(start+60000,-80));
    assert(s.due(start+64999,true)==Scan::NONE);
    assert(s.due(start+65000,true)==Scan::CONFIRM && s.confirm_channel()==6);
    s.begin(Scan::CONFIRM);s.offer(start+65000,good,old.bssid);
    assert(s.finish(start+65000,-80));
    s.connected(start+65001);assert(s.roams()==1);
    s.sample(start+65001,-80);
    assert(s.due(start+665000,true)==Scan::NONE);
    assert(s.due(start+665001,true)==Scan::SEARCH);
    // Failed roam doesn't count as a success when fallback connects.
    s.begin(Scan::SEARCH);s.offer(start+665001,good,old.bssid);s.finish(start+665001,-80);
    s.begin(Scan::CONFIRM);s.offer(start+670001,good,old.bssid);assert(s.finish(start+670001,-80));
    s.failed(start+685001);s.connected(start+690001);assert(s.roams()==1);
    // Missing/marginal candidate backs off; latest live strength prevents a
    // false advantage in confirmation when the original AP wasn't scanned.
    s.reset();s.connected(start);s.sample(start,-80);
    s.begin(Scan::SEARCH);s.offer(start+60000,weak,old.bssid);
    assert(!s.finish(start+60000,-80));
    assert(s.due(start+179999,true)==Scan::NONE);
    assert(s.due(start+180000,true)==Scan::SEARCH);
    s.begin(Scan::SEARCH);s.offer(start+180000,good,old.bssid);s.finish(start+180000,-80);
    s.begin(Scan::CONFIRM);s.offer(start+185000,good,old.bssid);
    assert(!s.finish(start+185000,-65));
    // Busy past the confirmation expiry must discard the target.
    s.reset();s.connected(start);s.sample(start,-80);
    s.begin(Scan::SEARCH);s.offer(start+60000,good,old.bssid);s.finish(start+60000,-80);
    assert(s.due(start+76000,true)==Scan::NONE && !s.target.valid);
    // Off mode follows the old recovery path; scans only precede attempts.
    og_wifi::Recovery r;r.start(start);
    assert(r.step(start,false,false,false,true)==og_wifi::Recovery::Action::SCAN);
    assert(r.step(start+1,false,false,false,true)==og_wifi::Recovery::Action::NONE);
    r.scan_finished(start+2000);
    assert(r.step(start+2000,false,true,false,true)==og_wifi::Recovery::Action::NONE);
    assert(r.step(start+2001,false,false,false,true)==og_wifi::Recovery::Action::CONNECT);
  }
  puts("WiFi selection, confirmation, cooldown, fallback, dwell and rollover: PASS");
}
