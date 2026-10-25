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
python3 tests/test_update_security.py
python3 tests/test_options_security.py
g++ -std=c++11 -Wall -Wextra -Werror tests/test_audio_timing.cpp -o /tmp/og-test-audio
/tmp/og-test-audio
```

These test production distance-consensus, health, identity lifecycle and exact POST-body authentication helpers without Arduino dependencies. The Node test checks the Security+ option visibility, exclusive Panel Emulate selection, and client-ID button's JavaScript, request body, cancellation and placement.
They do not replace sensor checks on hardware.

The upload test runs the production callbacks against observable flash/network
stubs, including rejected authentication, cancellation, multiple files, write
failures and retry. The options test exercises production serialization and save
logic for write-only secrets and Blynk-to-OTC migration, and checks escaped MQTT
status payloads against the configured packet capacity. A real browser/device
upload and broker subscription remain necessary integration checks.

For the real-widget slider regression, supply local copies of the exact
`jquery-1.9.1.min.js` and `jquery.mobile-1.3.1.js` assets referenced by the page:

```sh
node tests/test_slider_gestures.cjs /path/to/jquery-1.9.1.min.js /path/to/jquery.mobile-1.3.1.js
```

This optional browser test uses `/opt/google/chrome/chrome`, a temporary profile,
and a loopback-only simulated page. All commands are mocked; no device is contacted.
It exercises jQuery Mobile's actual drag/disable lifecycle, which the smaller
pending-state unit test does not model.

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
