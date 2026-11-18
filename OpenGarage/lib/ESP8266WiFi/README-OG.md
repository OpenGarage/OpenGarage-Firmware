# Pinned ESP8266WiFi integration candidate

Copied from the pinned ESP8266 Arduino core 3.1.2 used by PlatformIO
espressif8266 4.2.1. Installed core files are untouched. UPSTREAM-SHA256SUMS
records the original src tree. reserved-rx.patch contains the three changed
upstream files; ExternalReceiveBuffer.h and ConnectionDeadline.h are added helpers; OGCoreVersion.h rejects
cores other than 3.1.2, including builds without the reservation flag.

Only OG_WSS_RESERVED_RX enables the external-buffer API and extra context pointer. Ordinary
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

## Explicit connection deadline

`connectWithTimeout(host, port, milliseconds)` is an added, fresh-client-only
API. The ordinary `connect()`/Stream APIs and SMTP callers retain their existing
behavior. WebSockets uses this API instead of the ineffective outer
`setTimeout()` call. One stack-owned budget covers DNS, TCP and TLS. The TLS
phase receives only the remaining time, applied after the internal reset and
context setup. No timeout field is added to the context or global state.

The budget is cooperative: synchronous SDK/crypto work cannot be preempted.
A completion that returns at/after expiry is refused and the socket is aborted
without a TLS flush. Thus a single synchronous step may overrun wall-clock time;
this is not a hard real-time guarantee. Non-timeout certificate errors remain
readable until the caller stops the client. WebSockets reads the error and then
stops a failed client to release the borrowed buffer promptly. Existing active
clients are refused by this API rather than implicitly flushing an old session.
