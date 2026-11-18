"""Options "mqrt" (retain settled door state on MQTT) and "mgrd" (ignore commands while moving)."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/main.cpp').read_text()
defines = (root / 'OpenGarage/defines.h').read_text()
table = (root / 'OpenGarage/OpenGarage.cpp').read_text()
page = (root / 'OpenGarage/html/sta_options.html').read_text()

# Both options exist, come last in enum and table, are 0/1 flags and default to off (current behaviour).
after_wfr = defines.split('OPTION_WFR')[1].split('NUM_OPTIONS')[0]
assert re.findall(r'OPTION_\w+', after_wfr) == ['OPTION_MQRT', 'OPTION_MGRD'], after_wfr
rows = re.findall(r'^\s*\{"(\w+)",\s*(\d+),\s*(\d+),', table, re.M)
assert rows[-2:] == [('mqrt', '0', '1'), ('mgrd', '0', '1')], rows[-2:]

# --- MQTT retain -------------------------------------------------------------------------------
# Door state goes through the helper with the right "settled" flag; nothing else publishes to the
# state topics except the empty retained message that deletes the retained copy.
raw = re.findall(r'mqttclient\.publish\(([^;]*)\);', source)
direct = [r for r in raw if '/OUT/STATE' in r and not r.endswith('"", true')]
assert not direct, direct
for state, settled in (('OPEN', 'true'), ('CLOSED', 'true'), ('STOPPED', 'true'), ('OPENING', 'false'), ('CLOSING', 'false')):
    assert f'mqtt_publish_state("{state}", {settled});' in source, state
legacy = re.findall(r'mqttclient\.publish\(mqtt_topic\.c_str\(\), ([^)]*)\)', source)
assert all(not l.endswith('true') for l in legacy), legacy  # the plain topic is never retained

helper = re.search(r'static bool mqtt_publish_state\(.*?^}', source, re.M | re.S).group()
stub = r'''
#include <string>
#include <cassert>
#include <iostream>
using String = std::string;
struct Opt { int ival = 0; };
struct OG { Opt options[64]; } og;
enum { OPTION_MQRT = 5 };
String mqtt_topic = "door";
struct Client {
  std::string topic, payload; bool retained = false; int calls = 0;
  bool publish(const char* t, const char* p, bool r = false) { topic = t; payload = p; retained = r; ++calls; return true; }
} mqttclient;
''' + helper + r'''
int main() {
  og.options[OPTION_MQRT].ival = 0;
  mqtt_publish_state("OPEN", true);   assert(mqttclient.topic == "door/OUT/STATE" && !mqttclient.retained);
  og.options[OPTION_MQRT].ival = 1;
  mqtt_publish_state("CLOSED", true); assert(mqttclient.payload == "CLOSED" && mqttclient.retained);
  mqtt_publish_state("OPENING", false); assert(mqttclient.payload == "OPENING" && !mqttclient.retained);
  assert(mqttclient.calls == 3);
  std::cout << "ok\n";
}
'''
with tempfile.TemporaryDirectory() as d:
    cpp = Path(d) / 't.cpp'; cpp.write_text(stub)
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(Path(d) / 't')], check=True)
    assert subprocess.run([str(Path(d) / 't')], check=True, capture_output=True, text=True).stdout.strip() == 'ok'

# Turning retain or MQTT off deletes the retained copy of the state topic only.
clear = source.split('uint old_mqrt', 1)[1]
assert 'old_mqrt > 0 && (og.options[OPTION_MQRT].ival == 0 || og.options[OPTION_MQEN].ival == 0)' in clear
assert 'mqttclient.publish((mqtt_topic + "/OUT/STATE").c_str(), "", true);' in clear

# --- move guard --------------------------------------------------------------------------------
# Enforced only when the option is on, never for the device's own button (force_alarm_off).
assert 'if (!force_alarm_off && og.options[OPTION_MGRD].ival > 0 && move_guard.blocked(millis()))' in source
# Armed only when the option is on, from the one helper used by both places that send the command.
assert 'if (og.options[OPTION_MGRD].ival > 0) move_guard.arm(millis(), door_status);' in source
assert source.count('door_command_sent(') == 3, source.count('door_command_sent(')  # definition + immediate + alarm countdown
assert re.search(r'door_command_sent\(og\.alarm_action\);\s*switch \(og\.options\[OPTION_SECV\]', source)
# A command that cannot move the door (already open/opening, closed/closing) neither arms nor announces.
assert re.search(r'action == ACTION_OPEN && \(door_status == DOOR_STATUS_OPEN \|\| door_status == DOOR_STATUS_OPENING\)', source)
assert re.search(r'action == ACTION_CLOSE && \(door_status == DOOR_STATUS_CLOSED \|\| door_status == DOOR_STATUS_CLOSING\)', source)
# While a move is pending the regular status update repeats OPENING/CLOSING instead of the stale settled
# state; the sensor's confirmation (JUST_*) ends it, and a second command while pending says nothing.
assert 'pending_move.start(millis(), transition_state, transition_legacy);' in source
assert re.search(r'if \(pending_move\.active\(millis\(\)\)\) \{ pending_move\.clear\(\); return; \}', source)
assert re.search(r'DOOR_EVENT_JUST_OPENED \|\| event == DOOR_EVENT_JUST_CLOSED \|\| event == DOOR_EVENT_JUST_STOPPED\) pending_move\.clear\(\);', source)
pub = source[source.index('const char *moving = pending_move.state(millis());'):]
assert pub.index('if(moving && (event == DOOR_EVENT_REMAIN_OPEN') < pub.index('mqtt_publish_state("OPEN", true);')  # checked first
assert 'mqtt_publish_state(moving, false);' in pub  # never retained
assert 'OG_FWV     128' in defines
# Every status reading is fed to the guard, so it ends on open, closed or stopped.
assert re.search(r'move_guard\.observe\(millis\(\), door_status, door_status==DOOR_STATUS_OPENING \|\| door_status==DOOR_STATUS_CLOSING,\s*door_status==DOOR_STATUS_OPEN \|\| door_status==DOOR_STATUS_CLOSED \|\| door_status==DOOR_STATUS_STOPPED\)', source)
assert 'OG_MOVE_GUARD_MS 45000' in defines

# --- settings page -----------------------------------------------------------------------------
for key in ('mqrt', 'mgrd'):
    assert f"id='{key}'" in page and f"comm+='&{key}='+" in page and f"if(jd.{key}>0) cbt('{key}');" in page, key
print('door options tests passed')
