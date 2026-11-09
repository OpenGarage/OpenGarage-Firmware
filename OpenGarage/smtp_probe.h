/*
  WiFiClientBearSSL- SSL client/server for esp8266 using BearSSL libraries
  - Mostly compatible with Arduino WiFi shield library and standard
    WiFiClient/ServerSecure (except for certificate handling).

  Copyright (c) 2018 Earle F. Philhower, III

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


// Adapted for OpenGarage: shared read deadline and exact MFLN extension check.
// Original: ESP8266 Arduino core 3.1.2, WiFiClientSecureBearSSL.cpp.
// License text: licenses/ESP8266-SMTP-Probe-LGPL-2.1.txt
#pragma once
#include <WiFiClientSecure.h>
namespace og_smtp {
  static const uint16_t suites_P[] PROGMEM = {
#ifndef BEARSSL_SSL_BASIC
    BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_CCM,
    BR_TLS_ECDHE_ECDSA_WITH_AES_256_CCM,
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8,
    BR_TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8,
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    BR_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    BR_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    BR_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    BR_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    BR_TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
    BR_TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,
    BR_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
    BR_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256,
    BR_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
    BR_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384,
    BR_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    BR_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    BR_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    BR_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    BR_TLS_RSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_RSA_WITH_AES_256_GCM_SHA384,
    BR_TLS_RSA_WITH_AES_128_CCM,
    BR_TLS_RSA_WITH_AES_256_CCM,
    BR_TLS_RSA_WITH_AES_128_CCM_8,
    BR_TLS_RSA_WITH_AES_256_CCM_8,
#endif
    BR_TLS_RSA_WITH_AES_128_CBC_SHA256,
    BR_TLS_RSA_WITH_AES_256_CBC_SHA256,
    BR_TLS_RSA_WITH_AES_128_CBC_SHA,
    BR_TLS_RSA_WITH_AES_256_CBC_SHA,
#ifndef BEARSSL_SSL_BASIC
    BR_TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    BR_TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    BR_TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    BR_TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
    BR_TLS_RSA_WITH_3DES_EDE_CBC_SHA
#endif
  };

// Derived from pinned ESP8266 core 3.1.2; same cipher offer and wire probe.
// Reads share a 5-second elapsed budget starting before TCP connect; a slow
// peer cannot renew it per byte. TCP and alert writes retain transport timeouts.
class BoundedProbe : public WiFiClient {
    uint32_t start=millis();
public:
    size_t readBytes(uint8_t *dst,size_t n) {
        size_t used=0;
        while(used<n && uint32_t(millis()-start)<5000) {
            if(available()){int ch=read();if(ch>=0)dst[used++]=ch;}
            else if(!connected())break;
            else yield();
        }
        return used;
    }
};
static bool _SendAbort(WiFiClient& probe, bool supportsLen) {
  // If we're still connected, send the appropriate notice that
  // we're aborting the handshake per RFCs.
  static const uint8_t clientAbort_P[] PROGMEM = {
    0x15 /*alert*/, 0x03, 0x03 /*TLS 1.2*/, 0x00, 0x02,
    1, 90 /* warning: user_cancelled */
  };
  static const uint8_t clientClose_P[] PROGMEM = {
    0x15 /*alert*/, 0x03, 0x03 /*TLS 1.2*/, 0x00, 0x02,
    1, 0 /* warning: close_notify */
  };
  if (probe.connected()) {
    uint8_t msg[sizeof(clientAbort_P)];
    memcpy_P(msg, clientAbort_P, sizeof(clientAbort_P));
    probe.write(msg, sizeof(clientAbort_P));
    memcpy_P(msg, clientClose_P, sizeof(clientClose_P));
    probe.write(msg, sizeof(clientClose_P));
  }
  return supportsLen;
}
inline bool probeMfln(IPAddress ip, uint16_t port, uint16_t len) {
  // Hardcoded TLS 1.2 packets used throughout
  static const uint8_t clientHelloHead_P[] PROGMEM = {
    0x16, 0x03, 0x03, 0x00, 0, // TLS header, change last 2 bytes to len
    0x01, 0x00, 0x00, 0, // Last 3 bytes == length
    0x03, 0x03, // Proto version TLS 1.2
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // Random (gmtime + rand[28])
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x00, // Session ID
  };
  // Followed by our cipher-suite, generated on-the-fly
  //    0x00, 0x02, // cipher suite len
  //      0xc0, 0x13, // BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
  static const uint8_t clientHelloTail_P[] PROGMEM = {
    0x01, 0x00, // No compression
    0x00, 26 + 14 + 6 +  5, // Extension length
    0x00, 0x0d, 0x00, 0x16, 0x00, 0x14, 0x04, 0x03, 0x03, 0x03, 0x05, 0x03,
          0x06, 0x03, 0x02, 0x03, 0x04, 0x01, 0x03, 0x01, 0x05, 0x01, 0x06,
    0x01, 0x02, 0x01, // Supported signature algorithms
    0x00, 0x0a, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19,
          0x00, 0x1d, // Supported groups
    0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, // Supported EC formats
    0x00, 0x01, // Max Frag Len
    0x00, 0x01, // len of MaxFragLen
  };
  // Followed by a 1-byte MFLN size requesst
  //          0x04 // 2^12 = 4K
  uint8_t mfl;

  switch (len) {
    case 512: mfl = 1; break;
    case 1024: mfl = 2; break;
    case 2048: mfl = 3; break;
    case 4096: mfl = 4; break;
    default: return false; // Invalid size
  }
  int ttlLen = sizeof(clientHelloHead_P) + (2 + sizeof(suites_P)) + (sizeof(clientHelloTail_P) + 1);
  uint8_t *clientHello = new (std::nothrow) uint8_t[ttlLen];
  if (!clientHello) {

    return false;
  }
  memcpy_P(clientHello, clientHelloHead_P, sizeof(clientHelloHead_P));
  clientHello[sizeof(clientHelloHead_P) + 0] = sizeof(suites_P) >> 8;   // MSB byte len
  clientHello[sizeof(clientHelloHead_P) + 1] = sizeof(suites_P) & 0xff; // LSB byte len
  for (size_t i = 0; i < sizeof(suites_P) / sizeof(suites_P[0]); i++) {
    uint16_t flip = pgm_read_word(&suites_P[i]);
    // Swap to network byte order
    flip = ((flip >> 8) & 0xff) | ((flip & 0xff) << 8);
    memcpy(clientHello + sizeof(clientHelloHead_P) + 2 + 2 * i, &flip, 2);
  }
  memcpy_P(clientHello + sizeof(clientHelloHead_P) + 2 + sizeof(suites_P), clientHelloTail_P, sizeof(clientHelloTail_P));
  clientHello[sizeof(clientHelloHead_P) + 2 + sizeof(suites_P) + sizeof(clientHelloTail_P)] = mfl;

  // Fix up TLS fragment length
  clientHello[3] = (ttlLen - 5) >> 8;
  clientHello[4] = (ttlLen - 5) & 0xff;
  // Fix up ClientHello message length
  clientHello[7] = (ttlLen - 5 - 4) >> 8;
  clientHello[8] = (ttlLen - 5 - 4) & 0xff;

  BoundedProbe probe;
  probe.setTimeout(5000);
  probe.connect(ip, port);
  if (!probe.connected()) {
    delete[] clientHello;

    return false;
  }

  int ret = probe.write(clientHello, ttlLen);
  delete[] clientHello; // We're done w/the hello message
  if (!probe.connected() || (ret != ttlLen)) {

    return false;
  }

  uint8_t junkMfl[1];
  bool supportsLen = false;
  uint8_t fragResp[5];
  int fragLen;
  uint8_t hand[4];
  int handLen;
  uint8_t protoVer[2];
  uint8_t rand[32];
  uint8_t sessionLen;
  uint8_t cipher[2];
  uint8_t comp;
  uint8_t extBytes[2];
  uint16_t extLen;

  ret = probe.readBytes(fragResp, 5);
  if (!probe.connected() || (ret != 5) || (fragResp[0] != 0x16) || (fragResp[1] != 0x03) || (fragResp[2] != 0x03)) {
    // Short read, not a HANDSHAKE or not TLS 1.2, so it's not supported
    return _SendAbort(probe, supportsLen);
  }
  fragLen = (fragResp[3] << 8) | fragResp[4];
  if (fragLen < 4 + 2 + 32 + 1 + 2 + 1) {
    // Too short to have an extension
    return _SendAbort(probe, supportsLen);
  }

  ret = probe.readBytes(hand, 4);
  fragLen -= ret;
  if ((ret != 4) || (hand[0] != 2)) {
    // Short read or not server_hello
    return _SendAbort(probe, supportsLen);
  }
  handLen = (hand[1] << 16) | (hand[2] << 8) | hand[3];
  if (handLen != fragLen) {
    // Got some weird mismatch, this is invalid
    return _SendAbort(probe, supportsLen);
  }

  ret = probe.readBytes(protoVer, 2);
  handLen -= ret;
  if ((ret != 2) || (protoVer[0] != 0x03) || (protoVer[1] != 0x03)) {
    // Short read or not tls 1.2, so can't do MFLN
    return _SendAbort(probe, supportsLen);
  }

  ret = probe.readBytes(rand, 32);
  handLen -= ret;
  if (ret != 32) {
    // short read of random data
    return _SendAbort(probe, supportsLen);
  }

  ret = probe.readBytes(&sessionLen, 1);
  handLen -= ret;
  if ((ret != 1) || (sessionLen > 32)) {
    // short read of session len or invalid size
    return _SendAbort(probe, supportsLen);
  }
  if (sessionLen) {
    ret = probe.readBytes(rand, sessionLen);
    handLen -= ret;
    if (ret != sessionLen) {
      // short session id read
      return _SendAbort(probe, supportsLen);
    }
  }

  ret = probe.readBytes(cipher, 2);
  handLen -= ret;
  if (ret != 2) {
    // Short read...we don't check the cipher here
    return _SendAbort(probe, supportsLen);
  }

  ret = probe.readBytes(&comp, 1);
  handLen -= ret;
  if ((ret != 1) || comp != 0) {
    // short read or invalid compression
    return _SendAbort(probe, supportsLen);
  }

  ret = probe.readBytes(extBytes, 2);
  handLen -= ret;
  if (ret != 2) return _SendAbort(probe, supportsLen);
  extLen = extBytes[1] | (extBytes[0]<<8);
  if ((extLen == 0) || (ret != 2)) {
    return _SendAbort(probe, supportsLen);
  }

  while (handLen > 0) {
    // Parse each extension and look for MFLN
    uint8_t typeBytes[2];
    ret = probe.readBytes(typeBytes, 2);
    handLen -= 2;
    if ((ret != 2) || (handLen <= 0) ) {
      return _SendAbort(probe, supportsLen);
    }
    uint8_t lenBytes[2];
    ret = probe.readBytes(lenBytes, 2);
    handLen -= 2;
    if (ret != 2) return _SendAbort(probe, supportsLen);
    uint16_t extLen = lenBytes[1] | (lenBytes[0]<<8);
    if ((ret != 2) || (handLen <= 0) || (extLen > 32) || (extLen > handLen) ) {
      return _SendAbort(probe, supportsLen);
    }
    if ((typeBytes[0]==0x00) && (typeBytes[1]==0x01)) { // MFLN extension!
      // If present and 1-byte in length, it's supported
      if (extLen != 1 || probe.readBytes(junkMfl, 1) != 1) { _SendAbort(probe, false); return false; }
      return _SendAbort(probe, junkMfl[0] == mfl);
    }
    // Skip the extension, move to next one
    uint8_t junk[32];
    ret = probe.readBytes(junk, extLen);
    handLen -= extLen;
    if (ret != extLen) {
      return _SendAbort(probe, supportsLen);
    }
  }
  return _SendAbort(probe, supportsLen);
}
}
