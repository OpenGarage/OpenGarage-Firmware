# OpenGarage secure-cloud integration candidate

Vendored upstream WebSockets 2.7.3. UPSTREAM-SHA256SUMS records the original src
files. Changes are enabled only by OTF_SECURE_CLOUD, for pinned ESP8266 core3.1.2:

- Verified TLS1.2 with a mandatory flash trust catalog; no insecure fallback.
- Boot-reserved external RX storage and retained6656-byte TLS stack; preflight
  guards refuse unavailable resources before the reset-prone core allocations.
- A DRAM trust workspace exists only during synchronous connect, then is freed.
  Abandoned claims reset only after the old TLS client is destroyed.
- Session parameters allocated only for a secure client, reused across reconnects,
  cleared on begin/endpoint changes, and freed after client teardown. No session
  secrets are printed or exposed by the UI/API.
- Explicit10000-ms shared DNS/TCP/TLS budget through `connectWithTimeout()`.
  A late completion is refused; synchronous SDK/crypto steps can overrun before
  returning. This is a cooperative budget, not a hard wall-clock cutoff.

The application decides whether secure cloud is active at boot. Cloud-off, AP
and plain-WS selections do not reserve RX/retained stack. Plain WS stays connected
during email and uses the original SMTP guard. Secure mode pauses OTF, invalidates
queued cloud work without replay, sends email, then reconnects with backoff.

No bench endpoints, fake tokens, forced11g mode, edge-capture ISR or handshake
history is included. Keep the public catalog fingerprint manifest and core
ownership patch in review; final hardware regression/soak is still required.

The paired OTF opt-in checks OTF_VERIFIED_WSS_TRANSPORT=1, exported by this
reviewed transport. Upstream WebSockets does not advertise it, preventing an
accidental secure-cloud build with an unreviewed/insecure backend. This marker
is a build compatibility check, not a substitute for source/dependency review.
