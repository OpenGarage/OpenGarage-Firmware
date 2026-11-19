# Core regression tests

Keep a test public when it guards a lasting controller, authentication, settings
compatibility, or release guarantee. Use synthetic fixtures. Keep bench scripts,
device captures, local QA and measurements private. Git ignores new files in
this directory by default so adding another core test is deliberate.

Run the commands below from the repository root on a host with Python 3, its
`cryptography` package, GCC/G++, OpenSSL and its development headers. Build the
firmware with PlatformIO first to install the pinned ESP8266 SDK and ArduinoJson.

### Door control and stored settings

These checks cover distance readings, Security+ 2.0 identity persistence, offline
door automation, interrupted settings writes and migration from older releases.
The [fixture notes](fixtures/README.md) identify the source layouts.

```sh
set -e
for name in distance_consensus secplus2_identity config_store; do
    g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I. "tests/test_$name.cpp" -o "/tmp/og-test-$name"
    "/tmp/og-test-$name"
done
python3 tests/test_offline_automation.py
python3 tests/test_config_options.py
```

### Device keys and firmware updates

These tests check command and setup authorization, per-request credential
clearing, upload admission, and verification of signed releases.

```sh
set -e
for name in device_key ap_authorization update_header_reset update_security firmware_service firmware_release; do
    python3 "tests/test_$name.py"
done
```

### Secure cloud trust

These checks verify the generated trust anchors and the certificate policy used
when preparing a release.

```sh
set -e
python3 tests/test_wss_flash_store.py
python3 tests/test_wss_catalog_release.py
```
