# Focused native tests

From the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/test_distance_consensus.cpp -o /tmp/og-test-distance-consensus
/tmp/og-test-distance-consensus
g++ -std=c++11 -Wall -Wextra -Werror tests/test_secplus2_identity.cpp -o /tmp/og-test-secplus2-identity
/tmp/og-test-secplus2-identity
g++ -std=c++11 -Wall -Wextra -Werror tests/test_health_stats.cpp -o /tmp/og-test-health-stats
/tmp/og-test-health-stats
g++ -std=c++11 -Wall -Wextra -Werror tests/test_body_device_key.cpp -o /tmp/og-test-body-key
/tmp/og-test-body-key
node tests/test_identity_button.mjs
node tests/test_pending_switches.mjs
node tests/test_cloud_options.mjs
g++ -std=c++11 -Wall -Wextra -Werror tests/test_local_time.cpp -o /tmp/og-test-local-time
/tmp/og-test-local-time
python3 tests/test_offline_automation.py
python3 tests/test_mqtt_packets.py
python3 tests/test_network_delivery.py
g++ -std=c++11 -Wall -Wextra -Werror tests/test_wifi_manager.cpp -o /tmp/og-test-wifi
/tmp/og-test-wifi
g++ -std=c++11 -Wall -Wextra -Werror tests/test_wifi_selection.cpp -o /tmp/og-test-selection
/tmp/og-test-selection
python3 tests/test_wifi_adapter.py
python3 tests/test_debug_response.py
python3 tests/test_cached_page.py
python3 tests/test_update_security.py
python3 tests/test_update_parser.py
python3 tests/test_update_header_reset.py
python3 tests/test_firmware_release.py
python3 tests/test_firmware_service.py
node tests/test_updater_page.cjs
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I. tests/test_config_store.cpp -o /tmp/og-test-config
/tmp/og-test-config
python3 tests/test_config_options.py
python3 tests/test_config_setup.py
python3 tests/test_options_security.py
python3 tests/test_options_post.py
python3 tests/test_device_key.py
python3 tests/test_ntp_pointer.py
g++ -std=c++11 -Wall -Wextra -Werror tests/test_audio_timing.cpp -o /tmp/og-test-audio
/tmp/og-test-audio
python3 tests/test_button_audio.py
g++ -std=c++11 -Wall -Wextra -Werror tests/test_crash_record.cpp -o /tmp/og-test-crash-record
/tmp/og-test-crash-record
```

These test production distance-consensus, health, identity lifecycle and exact POST-body authentication helpers without Arduino dependencies. The Node test checks the Security+ option visibility, exclusive Panel Emulate selection, and client-ID button's JavaScript, request body, cancellation and placement.
They do not replace sensor checks on hardware.

The upload test runs the production callbacks against observable flash/network
stubs, including rejected authentication, cancellation, multiple files, write
failures and retry. The options test exercises production serialization and save
logic for write-only secrets and Blynk-to-OTC migration, and checks escaped MQTT
status payloads against the configured packet capacity. A real browser/device
upload and broker subscription remain necessary integration checks.

The station pages (`sta_home`, `sta_options`, `sta_logs`) no longer use jQuery
Mobile. Shared CSS, JavaScript and images live in `OpenGarage/html/_*` partials,
which `compress_htmls.mjs` inlines into each page through `<!--#include ...-->`.
The light/lock switches are native checkboxes, so the pending-state unit test
covers their whole lifecycle; the earlier jQuery Mobile slider test was removed.

The local-time tests cover rollover, NTP corrections and daily automation. The
offline automation harness executes the production automation function with
controlled time and outputs, including boot before synchronization, the hour-23
rule and the manual-open exception. Hardware offline/reconnect checks are still
required; these tests do not establish network-call latency.

MQTT packet tests compile the vendored parser against a fake transport with
AddressSanitizer/UBSan (leak detection disabled for sandbox compatibility). They
cover retain metadata for QoS0/1 across reconnects, callbacks that publish, bounded
command payloads, partial-frame deadlines, and retry backoff. Network delivery
tests execute the real notification/cloud scheduling functions with fake services.
They do not prove TLS, DNS or SMTP responsiveness on hardware.

WiFi recovery tests cover initial connection, failed association/DHCP deadlines,
healthy and short-lived sessions, capped retry backoff, local-action gating and
uptime rollover. The adapter harness executes the production SDK-call wrapper;
AP provisioning keeps its separate trial-connect path. The selection suite covers ranking, candidate confirmation, failed-target
cooldown, scan backoff, dwell, hidden-network fallback and rollover. The adapter
checks asynchronous scans, credential/security filtering and clearing the BSSID
lock on fallback. These tests do not establish real radio or mesh behavior.
The WiFi recovery suite also covers the 60-second stale-movement deferral cap,
while preserving separate warning/command/upload/restart gates. This cap applies
to offline recovery, not permission to roam away from a healthy busy connection.

Updater service host tests require OpenSSL development headers/libcrypto and the
pinned ArduinoJson7.1.0 dependency (run the PlatformIO build first). They execute
production service logic with host crypto/flash adapters, not ESP8266 BearSSL
assembly. Parser tests execute its byte/line readers and pre-body admission gate; complete socket/multipart
behavior and memory/latency still require the bench.

Release-note rendering uses the same limited, allowlisted GitHub HTML subset as
OpenSprinkler, with plain-text fallback. Run the browser test with Playwright
available on `NODE_PATH` and Chromium selected by `BROWSER_PATH`:

```sh
node tests/test_updater_notes.cjs
UPDATER_GENERATED=1 node tests/test_updater_notes.cjs
```

All requests are intercepted. The test covers formatting, inert embedded resources,
unsafe links, input/depth/node limits, fallback, and mobile/desktop layout. The
second command checks the compressed page embedded in `htmls.h`.

`python3 tests/test_email_lifetime.py` checks actual EMailSender allocation code
with AddressSanitizer/LeakSanitizer, including injected string-copy failures.
Run outside a ptrace-based sandbox because LeakSanitizer cannot operate there.

`python3 tests/test_email_encoding.py` checks the production Base64 encoder
against reference vectors and oversized credentials with ASan/UBSan. The options
security suite also covers 64-byte SMTP field limits and atomic rejection.

SMTP notification exchange tests (scripted peer; no real email or device):

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/test_smtp_exchange.cpp -o /tmp/og-smtp-exchange
/tmp/og-smtp-exchange
python3 tests/test_smtp_probe.py
```

These exercise the production bounded parser, multiline responses, UTF-8
encoding, dot stuffing, acceptance/rejection, shared deadline, and memory gate.
The MFLN test executes the bounded probe against a fake transport. The probe is
adapted from pinned ESP8266 core 3.1.2 (LGPL notice retained in smtp_probe.h).
Real TLS/provider timing and the stock Sec+ 2.0 memory gate still need hardware
validation; these tests do not establish universal server compatibility.

SMTP review follow-up: `test_network_delivery.py` also checks that local email
refusals update the failure counter/last result without starting retry backoff;
`test_debug_response.py` covers their verbose diagnostic serialization. SMTP peer
cases include DATA rejection codes, long UTF-8 subjects, quoted-printable soft
breaks with dot stuffing, and lone-CR normalization. The provisional memory gate
still needs a stock Security+ 2.0 send and final-image hardware stack measurements.

The diagnostic response test also injects 2,400 failed reservations/appends into
AP/station compact/verbose JSON builders. It requires complete JSON or the fixed
HTTP 503 reply. CheckedJsonBuffer must remain the type through diagnostic
serialization helpers; convert to the base String only after good() succeeds.
The JSON sender takes a const reference, avoiding a second full response copy.

The network-delivery suite drives the real startup melody and production busy
policy through all 560 ms, checking deferred connection/notification work and
continued local/established-cloud serving, completion and cancellation.

RAM batch 1: `python3 tests/test_lazy_mqtt.py` checks deferred MQTT buffer growth, allocation failure/retry and reuse. `test_update_security.py` also checks request-scope preparation-buffer cleanup after completed and interrupted requests.

`python3 tests/test_log_reader.py
python3 tests/test_log_stream.py` compiles the real log-reader opener and checks short headers, invalid indices, failed opens and replacement of an existing reader.

`python3 tests/test_smtp_build_policy.py` checks the actual bounded-only SMTP admission guard, including rejection of multiple recipients, CC/BCC, unauthenticated, SASL/CRAM and HTML modes before network work. The ESP8266 firmware build additionally verifies the selected compile-time path.

Updater header reset: test_update_header_reset.py compiles the real request reset loop against ESP8266-style length-only clear semantics and the production key comparator. Covers sequential valid, missing and wrong credentials, plus clearing upload-token and release headers. Hardware overlap checks supplement this host test.

The log-stream test compiles the production /jl handler with real OTF local/cloud streaming at the configured buffer size. It checks empty/1/400 records, both column layouts, zero slots, truncated reads, all control-byte escapes, maximum numeric fields, response-allocation refusal, failed network writes, and reader cleanup. Bodies retain the API format; HTTP uses Connection: close instead of precomputing Content-Length.


WiFi compatibility: `python3 tests/test_wifi_adapter.py` executes the adapter
with both normal (11n enabled) and 11g compatibility settings, checking PHY mode
at every connection including recovery after the radio resets. Options security
tests also cover setting/omitting/clearing `wfg` and its JSON representation.

Settings persistence tests execute the file transaction with interrupted writes,
short writes, invalid CRC/length, rename/remove/open failures and reset interruption.
The real options code is exercised against the same fake filesystem for released-layout
migration, rollback, reserved string storage, and corrupt-file lockout.
`fixtures/README.md` records the release sources for independent migration fixtures;
production has no per-version field list. Tests also cover unknown versions, malformed
legacy lines and torn non-key tails, invalid numbers, missing/duplicate/truncated keys and a future payload in the frozen v1 envelope. Setup completion and updater refusal are checked separately.
These model file API behavior, not SPIFFS flash-page corruption or physical power cuts.

Device-key foundation: `python3 tests/test_key_policy.py` checks new-key boundaries,
printable ASCII, unchanged legacy credentials, browser/server parity and the real
/co validation and marker update. `test_config_options.py` also covers marker
migration (including already-1.2.7 files and WiFi-only reset), fresh defaults,
invalid/duplicate markers, factory reset and failed key-plus-marker save rollback.
`test_options_security.py` verifies that /co cannot write the internal marker.

Configured AP authorization: `python3 tests/test_ap_authorization.py` executes
the actual /cc and /jt handlers with missing, empty, wrong and valid existing keys,
including opendoor and Unicode legacy keys. Refusals must leave options, connection
state and completion untouched. `node tests/test_ap_page.mjs` checks the key prompt,
URL encoding, polling snapshot and retry UI. Scanned SSIDs render as text.
`test_update_security.py` runs its upload/prepare callback suite in both AP and STA.

First-use key tests extend test_ap_authorization.py with the real POST handler:
invalid/NUL/oversize bodies, save rollback, identical-key idempotent retry,
established-key replacement refusal, non-AP refusal, live scan metadata and
WiFi setup gates. test_ap_page.mjs covers generation, validation, POST save and
retaining a chosen key after a lost response. test_config_setup.py checks that
first-use completion cannot save station mode or schedule a reboot.

First-use upload gate: test_update_security.py verifies early manual refusal and
callback checks with kset=0 in both AP/STA, plus the signed version floor passed
to the service. test_firmware_service.py signs 126 and 127 fixture descriptors
and proves the floor rejects the former despite allow-downgrade. Established
units retain floor126. Updater-page tests cover deliberate retry after HTTP403.

`python3 tests/test_setup_window.py` executes the setup deadline policy, timer
loop shutdown and cleanup with fakes, including millis rollover,
admission cutoffs and preserved setup mode/settings for all credential states.
Hardware must still verify real loop-delay/radio shutdown, interrupted
upload bootability, button recovery and one actual 30-minute expiry.
