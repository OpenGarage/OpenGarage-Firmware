# Stock firmware release bundles

The official updater host is https://firmware.opengarage.io. Its initial stable
catalog will contain only1.2.6 once released. Older firmware remains available
for manual upload. The live catalog is intentionally empty during development.

`firmware_release.py` prepares and verifies signed immutable bundles from an
already built, tested ESP8266 image. It does not rebuild, upload or publish files.
No build number is used. A changed public release needs a new firmware version.
The current target is `og-esp8266-4m-v1` (the d1_mini4m1m layout). Future C3 targets
must be added when their actual flash layout and image validation are defined;
the metadata supports multiple targets but no unfinished C3 image is accepted.

Keep the P-256 private key outside the repository and synced directories, with
an offline backup. Only OpenGarage/firmware_update_public_key.h is public.

```sh
python3 tools/firmware_release.py prepare \
  --private-key ~/.local/share/opengarage-signing/opengarage-firmware-p256.pem \
  --output-dir /tmp/og-release/v1 \
  --esp8266 /path/to/tested/firmware.bin --version 126
python3 tools/firmware_release.py verify --output-dir /tmp/og-release/v1
```

Do not prepare/sign1.2.6 as a stable release until its updater and tests are
complete. The operator must confirm the image was built from the reviewed source
with the production public key and correct linker layout: a generic ESP image
header does not prove product identity, key inclusion or SPIFFS layout.

Preparation refuses an existing release ID and verifies existing catalog entries
before adding a release. Tests use disposable keys and artificial images:

```sh
python3 tests/test_firmware_release.py
```

Publication tooling remains a follow-up: preserve existing verified entries,
refuse changed bytes under published IDs, upload release directories first and
atomically replace the catalog last. Verify public signatures, hashes, size and
CORS afterward. Never upload the private key. GitHub and the update host must
serve the same tested image bytes.

## Development testing without publishing a release

Keep test bundles under an ignored local directory, separate from the production
release tree. In an isolated test browser, intercept only GET requests to the
catalog, descriptor, signature and image URLs and supply that bundle. Leave all
controller requests real: its signature, target, checksum and authentication
checks still apply. Reinstalling the current version requires explicit confirmation.
No catalog override or signature bypass is included in shipping firmware.

After testing, close the isolated browser and remove the mock bundle. Keep test
results and the identified candidate binary separately. Never copy that catalog
or release directory to the public server. Prepare the final release from the
final tested artifact in a new directory; published release IDs remain immutable.
