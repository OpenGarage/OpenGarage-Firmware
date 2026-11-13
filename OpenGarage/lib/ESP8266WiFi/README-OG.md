# Pinned ESP8266WiFi integration candidate

Copied from the pinned ESP8266 Arduino core 3.1.2 used by PlatformIO
espressif8266 4.2.1. Installed core files are untouched. UPSTREAM-SHA256SUMS
records the original src tree. reserved-rx.patch contains the three changed
upstream files; ExternalReceiveBuffer.h is the added ownership helper; OGCoreVersion.h rejects
cores other than 3.1.2, including builds without the reservation flag.

Only OG_WSS_RESERVED_RX enables the API and extra context pointer. Ordinary
clients retain the original allocation path. Storage owner must outlive the
context. The API configures borrowing; connect claims it exclusively and
_freeSSL releases the claim without freeing the caller's memory. A failed
handshake may retain its context until stop/destruction, matching the original
core lifecycle. No implicit heap fallback for a configured but unavailable
reservation. Active contexts cannot change storage. Changing requested buffer
size is rechecked at connect time. Single-threaded ESP8266 use only.

The ownership patch passed the experimental failure/reconnect tests and a
24-hour WSS/email soak. It is now ported unchanged into the 1.2.7 integration
candidate, which still needs its own hardware regression. The installed
framework is not modified. Tests exercise the real setter/allocation/cleanup.

Release wipes the exact capacity before dropping ownership, including the final
partial word. Volatile aligned word stores avoid IRAM byte-access overhead and
prevent optimization from discarding the clear. Nonowners never wipe data.
The version guard detects semantic core-version changes; it does not fingerprint
custom modifications carrying the same version number.
