from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
mock=r'''
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <string>
#include <bearssl.h>
#define PROGMEM
#define memcpy_P memcpy
#define pgm_read_word(p) (*(p))
static uint32_t ticks=0;static std::string wire;static size_t offset=0;
uint32_t millis(){return ticks;}void yield(){++ticks;}
struct IPAddress{};
struct WiFiClient{
 void setTimeout(unsigned){}bool connect(IPAddress,unsigned){return true;}
 bool connected(){return true;}int available(){return offset<wire.size();}
 int read(){return available()?(unsigned char)wire[offset++]:-1;}
 size_t write(const uint8_t*,size_t n){return n;}
};
'''
program=r'''
#include "smtp_probe.h"
#include <cassert>
int main(){
 for(int requested:{1,2}){
  ticks=0;offset=0;
  std::string body="\x03\x03";body+=std::string(32,'x');body.push_back(0);
  body.push_back(0xc0);body.push_back(0x2f);body.push_back(0);
  body.append("\x00\x05\x00\x01\x00\x01",6);body.push_back(requested);
  std::string handshake;handshake.append("\x02\x00\x00",3);handshake.push_back(body.size());handshake+=body;
  wire.assign("\x16\x03\x03\x00",4);wire.push_back(handshake.size());wire+=handshake;
  assert(og_smtp::probeMfln(IPAddress(),465,512)==(requested==1));
 }
 ticks=0;offset=0;wire.clear();assert(!og_smtp::probeMfln(IPAddress(),465,512));assert(ticks==5000);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'WiFiClientSecure.h').write_text(mock);(p/'t.cpp').write_text(program)
 sdk=Path('/home/ruiwang/.platformio/packages/framework-arduinoespressif8266/tools/sdk/include/bearssl')
 subprocess.run(['g++','-std=c++11','-I',d,'-I',str(sdk),'-I',str(r/'OpenGarage'),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('MFLN probe: matching extension, mismatched extension and silent peer deadline: PASS')
