// Nonblocking warning and melodies adapted from OpenGarage-ESPHome.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

namespace og_audio {
// Sink supplies note(frequency, duration_ms) and silence(). All tones are finite.
template<class Sink> class Warning {
 public:
  explicit Warning(Sink &sink): sink_(sink) {}
  void start(uint32_t now, uint32_t duration) {
    cancel(); active_=true; last_=now; count_=1;
    required_=(duration+999)/1000;
    if (!required_) required_=1;
    sink_.note(1000,500);
  }
  bool tick(uint32_t now) {
    if (!active_ || uint32_t(now-last_)<1000) return false;
    // Never catch up by compressing or skipping audible bursts after a stall.
    if (count_<required_) { ++count_; last_=now; sink_.note(1000,500); return false; }
    cancel(); return true;
  }
  void cancel() { if(active_) sink_.silence(); active_=false; }
 private:
  Sink &sink_;
  uint32_t last_=0, count_=0, required_=0;
  bool active_=false;
};

enum class Tune { AP, STATION, SETUP_SUCCESS };
template<class Sink> class Melody {
 public:
  explicit Melody(Sink &sink): sink_(sink) {}
  void start(uint32_t now, Tune tune) {
    cancel(); tune_=tune; index_=0; gap_=false; active_=true;
    at_=now; wait_=0; tick(now);
  }
  void cancel() { if(active_) sink_.silence(); active_=false; }
  bool active() const { return active_; }
  void tick(uint32_t now) {
    if (!active_ || uint32_t(now-at_)<wait_) return;
    at_=now;
    const unsigned count=tune_==Tune::SETUP_SUCCESS?6:3;
    if (gap_) {
      sink_.silence();
      if (++index_==count) { active_=false; return; }
      gap_=false; wait_=40; return;
    }
    static constexpr unsigned ap[]={262,330,392};
    static constexpr unsigned station[]={330,392,523};
    static constexpr unsigned success[]={196,262,330,392,330,392};
    static constexpr unsigned beats[]={1,1,1,2,1,4};
    const auto *notes=tune_==Tune::AP?ap:tune_==Tune::STATION?station:success;
    wait_=120*(tune_==Tune::SETUP_SUCCESS?beats[index_]:tune_==Tune::STATION&&index_==2?2:1);
    sink_.note(notes[index_],wait_); gap_=true;
  }
 private:
  Sink &sink_;
  uint32_t at_=0,wait_=0;
  unsigned index_=0;
  Tune tune_=Tune::AP;
  bool active_=false,gap_=false;
};
}
