"""Exercise the actual lazy-buffer helper's allocation failure/retry behavior."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'OpenGarage/main.cpp').read_text();start=s.index('static bool ensure_mqtt_buffer()');end=s.index('\n}',start)+2
helper=s[start:end]
program='''#include <cassert>
struct Client { bool allow=true; unsigned size=256,calls=0;
 bool setBufferSize(unsigned n){++calls;if(!allow)return false;size=n;return true;} } mqttclient;
bool mqtt_buffer_ready=false;
'''+helper+'''
int main(){
 assert(mqttclient.size==256 && mqttclient.calls==0);
 mqttclient.allow=false;assert(!ensure_mqtt_buffer());assert(!mqtt_buffer_ready && mqttclient.size==256);
 mqttclient.allow=true;assert(ensure_mqtt_buffer());assert(mqtt_buffer_ready && mqttclient.size==2048);
 unsigned calls=mqttclient.calls;assert(ensure_mqtt_buffer());assert(mqttclient.calls==calls);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++11',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Lazy MQTT allocation failure/retry and reuse: PASS')
