#include "../OpenGarage/audio_timing.h"
#include <cassert>
#include <cstdio>
#include <vector>
struct Note { unsigned frequency, duration; };
struct Sink {
  std::vector<Note> notes;
  unsigned stops=0;
  void note(unsigned f,unsigned d) { notes.push_back({f,d}); }
  void silence() { ++stops; }
};
int main() {
  // Every possible millisecond phase against the old uptime-aligned clock.
  for (unsigned phase=0;phase<1000;++phase) {
    for (unsigned duration : {5000U,10000U}) {
      Sink s; og_audio::Warning<Sink> w(s);
      const uint32_t start=10000+phase;
      w.start(start,duration);
      assert(s.notes.size()==1 && s.notes[0].duration==500);
      for(unsigned t=0;t<duration;++t) assert(!w.tick(start+t));
      assert(w.tick(start+duration));
      assert(s.notes.size()==duration/1000 && s.stops==1);
      for(auto n:s.notes) assert(n.frequency==1000 && n.duration==500);
      assert(!w.tick(start+duration+1000)); // no second dispatch or terminal beep
    }
  }
  Sink s; og_audio::Warning<Sink> w(s);
  w.start(0xfffffff0U,5000);
  for(unsigned n=1;n<5;++n) assert(!w.tick(uint32_t(0xfffffff0U+n*1000)));
  assert(w.tick(uint32_t(0xfffffff0U+5000)));
  w.start(0,5000); auto count=s.notes.size();
  assert(!w.tick(9000) && s.notes.size()==count+1); // no catch-up burst train
  assert(!w.tick(9001) && s.notes.size()==count+1);
  w.cancel(); assert(!w.tick(20000));
  w.start(21000,5000); w.start(21250,5000); // renewed warning gets a full period
  assert(!w.tick(21500));
  for(unsigned n=1;n<5;++n) assert(!w.tick(21250+n*1000));
  assert(w.tick(26250));

  for(auto tune:{og_audio::Tune::AP,og_audio::Tune::STATION,og_audio::Tune::SETUP_SUCCESS}) {
    Sink m; og_audio::Melody<Sink> melody(m);
    melody.start(0xfffffff0U,tune);
    for(unsigned t=0;t<3000;++t) melody.tick(uint32_t(0xfffffff0U+t));
    assert(!melody.active());
    const std::vector<unsigned> notes=tune==og_audio::Tune::AP?std::vector<unsigned>{262,330,392}:
      tune==og_audio::Tune::STATION?std::vector<unsigned>{330,392,523}:std::vector<unsigned>{196,262,330,392,330,392};
    const std::vector<unsigned> durations=tune==og_audio::Tune::AP?std::vector<unsigned>{120,120,120}:
      tune==og_audio::Tune::STATION?std::vector<unsigned>{120,120,240}:std::vector<unsigned>{120,120,120,240,120,480};
    assert(m.notes.size()==notes.size());
    for(unsigned i=0;i<notes.size();++i) { assert(m.notes[i].frequency==notes[i]); assert(m.notes[i].duration==durations[i]); }
    melody.start(0,tune); melody.cancel(); auto size=m.notes.size();
    melody.tick(3000); assert(m.notes.size()==size && !melody.active());
  }
  std::puts("Audio: 5/10-second warnings across 1000 start phases, stall/restart/cancel/wrap and all ESPHome melody notes/beats passed");
}
