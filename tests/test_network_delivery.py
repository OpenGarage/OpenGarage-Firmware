"""Compile production notification/cloud scheduling against fake network services."""
from pathlib import Path
import subprocess,tempfile
source=Path('OpenGarage/main.cpp').read_text()
policy=source[source.index('static bool network_delivery_busy() {'):source.index('\n}',source.index('static bool network_delivery_busy() {'))+2]
record=source[source.index('static og_smtp::Delivery record_email_result'):source.index('static bool email_settings_ready()')]
functions=source[source.index('void perform_notify(String s) {'):source.index('void process_dynamics(')]
stub=r'''
#include "OpenGarage/network_policy.h"
#include "OpenGarage/audio_timing.h"
#include "OpenGarage/smtp_exchange.h"
#include <string>
#include <cassert>
#include <cstdio>
#define PSTR(x) x
#define snprintf_P snprintf
using String=std::string;
uint32_t now=0;uint32_t millis(){return now;}
const int WL_CONNECTED=3;
struct {bool online=true;int status(){return online?3:6;}unsigned localIP(){return online?1:0;}} WiFi;
struct AudioSink {void note(unsigned,unsigned){}void silence(){};} sink;
og_audio::Melody<AudioSink> startup_audio(sink);
bool update_started=false,identity_restart_pending=false,setup_reboot_pending=false,local_action_seen=false;
uint32_t local_action_ms=0;
struct {bool command_pending(){return false;}}secplus2_garage;
enum {OPTION_MQEN,OPTION_IFTT,OPTION_EMEN};
#define LOW 0
#define DOOR_STATUS_OPENING 4
#define DOOR_STATUS_CLOSING 3
struct Option {unsigned ival=0;String sval;};struct {Option options[3];bool alarm=false;int button=1;int get_button(){return button;}}og;
int door_status=0;bool secure_cloud_enabled=false,smtp_cloud_paused=false;uint32_t smtp_finished_ms=0;
struct {bool valid=true;} local_clock;
namespace og_tls {struct S{bool retained=true;};inline S &status(){static S s;return s;}}
''' + policy + r'''
String pending_notification;uint32_t notification_ms=0,notification_dropped=0;
og_network::Backoff email_retry(60000,600000),ifttt_retry(60000,600000);
unsigned mqtt_sent=0,email_sent=0,ifttt_sent=0;bool email_ok=false,ifttt_ok=false;bool email_refused=false;
void mqttNotify(String){++mqtt_sent;}
bool iftttNotify(String){++ifttt_sent;return ifttt_ok;}
unsigned email_failures=0;char email_last[96]="Not attempted";
'''+record+r'''
og_smtp::Delivery emailNotify(String){++email_sent;return record_email_result(email_ok ? og_smtp::Delivery::SENT : email_refused ? og_smtp::Delivery::REFUSED : og_smtp::Delivery::FAILED,email_refused?"Not enough memory for SMTP":email_ok?"accepted":"recipient",email_ok?"250":"2");}
namespace OTF {enum {DISCONNECTED=2,CONNECTED=3};}
struct Cloud {int status=2;unsigned local=0,cloud=0;int getCloudStatus(){return status;}void loop(bool allow){++local;if(allow)++cloud;}} cloud;
Cloud *otf=&cloud;
'''
main=r'''
int main(){
 og.options[OPTION_MQEN].ival=1;og.options[OPTION_EMEN].ival=1;og.options[OPTION_IFTT].sval="123456789";
 og.alarm=true;perform_notify("warning");service_notifications();assert(mqtt_sent==0 && email_sent==0);
 WiFi.online=false;service_notifications();assert(pending_notification.empty());WiFi.online=true;og.alarm=false;
 service_notifications();assert(mqtt_sent==0); // no reconnect replay
 perform_notify("old");now=30001;service_notifications();assert(mqtt_sent==0);
 perform_notify("first");perform_notify("second");assert(notification_dropped==3);
 service_notifications();assert(mqtt_sent==1 && email_sent==1 && ifttt_sent==1);
 perform_notify("next");service_notifications();assert(mqtt_sent==2 && email_sent==1 && ifttt_sent==1);
 now+=60000;email_ok=ifttt_ok=true;perform_notify("recovered");service_notifications();assert(email_sent==2);
 perform_notify("success");service_notifications();assert(email_sent==3);
 now=0xfffffff0U;perform_notify("wrap");now=20;service_notifications();assert(mqtt_sent==5);
 og.alarm=true;service_cloud();assert(cloud.local==1 && cloud.cloud==0);
 og.alarm=false;service_cloud();assert(cloud.cloud==1);service_cloud();assert(cloud.cloud==2); // handshake must continue
 now+=5000;service_cloud();assert(cloud.cloud==3);service_cloud();assert(cloud.cloud==4); // OTF owns pacing
 cloud.status=OTF::CONNECTED;service_cloud();service_cloud();assert(cloud.cloud==6);
 og.alarm=true;service_cloud();assert(cloud.cloud==7); // established cloud stays live during alarm
 cloud.status=OTF::DISCONNECTED;service_cloud();assert(cloud.cloud==7); // reconnect still waits
 WiFi.online=false;service_cloud();assert(cloud.cloud==7);
 og.alarm=false;service_cloud();assert(cloud.cloud==7); // offline even when idle
 WiFi.online=true;service_cloud();assert(cloud.cloud==8);

 email_retry.success();email_ok=false;email_refused=true;
 perform_notify("low memory");service_notifications();unsigned before=email_sent;
 assert(email_retry.ready(now));assert(email_failures==2);
 assert(std::string(email_last)=="Refused: Not enough memory for SMTP (2)");
 email_refused=false;email_ok=true;perform_notify("next event");service_notifications();assert(email_sent==before+1);assert(email_failures==2);
 assert(std::string(email_last)=="Sent: accepted (250)");

 // WSS alone adds button/motion/time/resource deferral; established sessions stay serviced.
 secure_cloud_enabled=true;cloud.status=OTF::DISCONNECTED;unsigned wss_before=cloud.cloud;
 og.button=LOW;service_cloud();assert(cloud.cloud==wss_before);og.button=1;
 door_status=DOOR_STATUS_OPENING;service_cloud();assert(cloud.cloud==wss_before);door_status=0;
 local_clock.valid=false;service_cloud();assert(cloud.cloud==wss_before);local_clock.valid=true;
 og_tls::status().retained=false;service_cloud();assert(cloud.cloud==wss_before);og_tls::status().retained=true;
 service_cloud();assert(cloud.cloud==wss_before+1);
 secure_cloud_enabled=false;

 // Actual melody holds new cloud connects and notification delivery until all
 // notes finish, including the final40ms processing transition. Local HTTP and
 // already-established cloud polling continue.
 now=100000;email_retry.success();ifttt_retry.success();email_ok=true;email_refused=false;
 og.alarm=false;WiFi.online=true;cloud.status=OTF::DISCONNECTED;
 unsigned mails=email_sent, polls=cloud.cloud,local=cloud.local;
 startup_audio.start(now,og_audio::Tune::STATION);perform_notify("startup");
 for(unsigned elapsed=0;elapsed<560;++elapsed) {
  now=100000+elapsed;
  assert(network_delivery_busy());service_cloud();service_notifications();
  assert(email_sent==mails && cloud.cloud==polls);
  startup_audio.tick(now);
 }
 assert(startup_audio.active());
 cloud.status=OTF::CONNECTED;service_cloud();assert(cloud.cloud==polls+1);
 assert(cloud.local>local); // local serving wasn't paused
 cloud.status=OTF::DISCONNECTED;now=100560;startup_audio.tick(now);
 assert(!startup_audio.active() && !network_delivery_busy());
 service_cloud();service_notifications();assert(cloud.cloud==polls+2 && email_sent==mails+1);
 startup_audio.start(now,og_audio::Tune::AP);assert(network_delivery_busy());
 startup_audio.cancel();assert(!network_delivery_busy()); // explicit cancellation releases gate
 puts("notification ordering, expiry, offline discard and cloud backoff: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stub+functions+main)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I.',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
