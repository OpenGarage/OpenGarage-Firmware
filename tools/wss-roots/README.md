# Experimental WSS roots

These are public certificates copied from the independently fingerprint-checked
candidate inventory. `manifest.json` retains provenance and explicitly marks
shipping approval false. This is a test catalog, not an approved release policy.
Private keys and controller credentials are not included.

Regenerate from the experiment root:

```
python3 tools/generate_wss_store.py --roots tools/wss-roots --manifest tools/wss-roots/manifest.json --output OpenGarage/lib/WebSockets/src/OGWssPublicAnchors.h
python3 tests/test_wss_flash_store.py
```

The generator checks pinned DER fingerprints, CA constraints, self-issued roots,
key bounds and conflicting subject hashes. It preserves exact subject DER.
Tests compare the subject bytes and decoded key with the pinned ESP8266 core's
actual BearSSL decoder. Handshake lookup uses flash data and one heap workspace;
there is no certificate parsing or file I/O inside that callback.

Root expiry is not checked by a BearSSL trust anchor. Revisit this allowlist,
root retirement, supported chains and issuing-CA policy for each firmware release.
`trustVersion` and `trustHash` identify the catalog, not freshness or automatic
trust-policy approval. The synthetic fixture catalog is selected only by the
isolated RSA bench flag and must never be used in production.
