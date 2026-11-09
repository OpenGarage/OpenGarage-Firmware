#include "../OpenGarage/smtp_exchange.h"
#include <string>
#include <cassert>
#include <iostream>
struct Clock {uint32_t t=0;uint32_t now(){return t;}void idle(){++t;}};
struct Peer {
 std::string incoming,output;size_t pos=0;bool online=true;Clock* clock=nullptr;unsigned drip=0;bool quitFails=false;
 int available(){return credits && pos<incoming.size();} bool connected(){return online;}
 unsigned credits=1;size_t lineStart=0;bool data=false;std::string pending;
 int read(){if(clock)clock->t+=drip;if(!available())return -1;
  int ch=(unsigned char)incoming[pos++];
  if(ch=='\n'){if(pos-lineStart<4||incoming[lineStart+3]!='-')--credits;lineStart=pos;}
  return ch;
 }
 size_t write(const uint8_t*p,size_t n){if(quitFails && n==4 && !memcmp(p,"QUIT",4))return 0;output.append((const char*)p,n);pending.append((const char*)p,n);
  size_t at;while((at=pending.find("\r\n"))!=std::string::npos){auto line=pending.substr(0,at);pending.erase(0,at+2);
   if(data){if(line=="."){data=false;++credits;}}else{++credits;if(line=="DATA")data=true;}
  }return n;
 }
 void setTimeout(uint32_t){}
};
std::string replies="220-test\r\n220 ready\r\n250-test\r\n250 AUTH LOGIN\r\n334 user\r\n334 pass\r\n235 ok\r\n250 sender\r\n251 recipient\r\n354 data\r\n250 queued\r\n";
og_smtp::Result send(Peer&p,Clock&c,const char* title="Garage",const char*body=".first\nsecond"){
 og_smtp::Exchange<Peer,Clock>x(p,c,c.t);
 return x.send("192.168.1.2","user","pass","a@example.test","b@example.test",title,body,"","boot.1@opengarage.local");
}
int main(){
 assert(og_smtp::memory_ready(22000,12288));assert(!og_smtp::memory_ready(21999,12288));assert(!og_smtp::memory_ready(22000,12287));
 {Clock c;Peer p;p.incoming=replies;auto r=send(p,c);assert(r.accepted);assert(p.output.find("EHLO [192.168.1.2]\r\n")!=std::string::npos);assert(p.output.find("Content-Transfer-Encoding: 7bit\r\n\r\n..first\r\nsecond\r\n.\r\nQUIT\r\n")!=std::string::npos);assert(p.output.find("frontier")==std::string::npos);}
 {Clock c;Peer p;p.incoming=replies;auto r=send(p,c,"Caf\xc3\xa9","Caf\xc3\xa9");assert(r.accepted);assert(p.output.find("Subject: =?UTF-8?B?Q2Fmw6k=?=\r\n")!=std::string::npos);assert(p.output.find("Caf=C3=A9")!=std::string::npos);}
 {Clock c;Peer p;p.incoming=replies;auto at=p.incoming.find("251 recipient");p.incoming.replace(at,13,"550 rejected");auto r=send(p,c);assert(!r.accepted&&r.code==550);assert(p.output.find("DATA\r\n")==std::string::npos);}
 {Clock c;Peer p;p.incoming="220 ready\r\n250 ok\r\n334 u\r\n334 p\r\n235 ok\r\n250 ok\r\n250 ok\r\n354 ok\r\n250 ok\r\n";assert(send(p,c).accepted);}
 for(auto input:{std::string("220 ")+std::string(510,'x')+"\r\n",std::string("220 bad\n"),std::string("220-first\r\n250 wrong\r\n")}){Clock c;Peer p;p.incoming=input;og_smtp::Exchange<Peer,Clock>x(p,c,0);assert(x.reply()<0);}
 {Clock c;Peer p;for(int i=0;i<33;i++)p.incoming+="220-more\r\n";og_smtp::Exchange<Peer,Clock>x(p,c,0);assert(x.reply()==-5);}
 {Clock c;Peer p;og_smtp::Exchange<Peer,Clock>x(p,c,0);assert(x.reply()==-1&&c.t==10000);}
 {Clock c;Peer p;p.clock=&c;p.drip=2000;p.incoming="220 slow stream\r\n";og_smtp::Exchange<Peer,Clock>x(p,c,0);assert(x.reply()==-1&&c.t<=10000);}
 {Clock c;c.t=0xfffffff0;Peer p;og_smtp::Exchange<Peer,Clock>x(p,c,c.t);c.t+=30000;assert(!x.line("test"));}
 {Clock c;Peer p;p.incoming="220 "+std::string(506,'x')+"\r\n";og_smtp::Exchange<Peer,Clock>x(p,c,0);assert(x.reply()==220);}
 {Clock c;Peer p;p.incoming="550 text mentions 250\r\n";og_smtp::Exchange<Peer,Clock>x(p,c,0);assert(x.reply()==550);}
 {Clock c;Peer p;p.clock=&c;p.drip=250;p.incoming=replies;assert(!send(p,c).accepted);assert(c.t>=30000&&c.t<=30250);}
 {Clock c;Peer p;p.incoming=replies;p.quitFails=true;assert(send(p,c).accepted);}
 {Clock c;Peer p;p.incoming="220 ready\r\n502 unsupported\r\n250 ready\r\n334 u\r\n334 p\r\n235 ok\r\n250 ok\r\n250 ok\r\n354 ok\r\n250 queued\r\n";assert(send(p,c).accepted);assert(p.output.find("HELO [192.168.1.2]")!=std::string::npos);}

 {Clock c;Peer p;p.incoming=replies;auto at=p.incoming.find("354 data");p.incoming.replace(at,8,"554 denied");auto r=send(p,c);assert(!r.accepted&&r.code==554&&std::string(r.stage)=="DATA");assert(p.output.find("Subject:")==std::string::npos);}
 {Clock c;Peer p;p.incoming=replies;std::string title;for(int i=0;i<45;++i)title+="\xc3\xa9";
  assert(send(p,c,title.c_str()).accepted);auto a=p.output.find("Subject: "),b=p.output.find("MIME-Version:",a);
  auto headers=p.output.substr(a,b-a);size_t pos=0,end;
  while((end=headers.find("\r\n",pos))!=std::string::npos){assert(end-pos<=76);pos=end+2;}
  assert(headers.find("\r\n =?UTF-8?B?")!=std::string::npos);
 }
 {Clock c;Peer p;p.incoming=replies;std::string body(72,'a');body+=".\xc3\xa9";
  assert(send(p,c,"Garage",body.c_str()).accepted);assert(p.output.find(std::string(72,'a')+"=\r\n..=C3=A9\r\n")!=std::string::npos);
 }
 {Clock c;Peer p;p.incoming=replies;assert(send(p,c,"Garage","a\rb\r.c").accepted);assert(p.output.find("\r\na\r\nb\r\n..c\r\n.\r\n")!=std::string::npos);}
 assert(!og_smtp::header_safe("x\r\nInjected"));assert(!og_smtp::credential_safe(std::string(190,'x').c_str()));
 std::cout<<"SMTP scripted peer: replies, bounds, deadlines, MIME, UTF-8, dot stuffing, rejection and accepted DATA: PASS\n";
}
