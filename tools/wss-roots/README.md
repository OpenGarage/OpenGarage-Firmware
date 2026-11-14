# Candidate WSS roots and release maintenance

These are public certificates copied from the fingerprint-checked candidate
inventory. `manifest.json` retains provenance and explicitly marks shipping
approval false. This is a candidate catalog, not an approved release policy.
Private keys and controller credentials are not included.

Regenerate from the experiment root:

```
python3 tools/generate_wss_store.py --roots tools/wss-roots --manifest tools/wss-roots/manifest.json --output OpenGarage/lib/WebSockets/src/OGWssPublicAnchors.h
python3 tests/test_wss_flash_store.py
python3 tools/check_wss_catalog.py --check-candidate
python3 tests/test_wss_catalog_release.py
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

The November 14 review selects 11 roots: GTS R1/R3/R4, ISRG X1/X2,
SSL.com's two legacy and two 2022 TLS roots, and Sectigo E46/R46.
GTS R2 was removed because its websites trust bit was removed in
[NSS 3.123](https://bugzilla.mozilla.org/show_bug.cgi?id=2020137). A certificate
still being present in NSS, or having a matching fingerprint, does not mean it
is trusted for TLS.

`nss-audit.json` records exact DER matches and server-auth trust from a pinned
official NSS commit. The full snapshot is review evidence, not firmware data.
Download the immutable URL in that file and reproduce it offline with:

```
python3 tools/check_wss_catalog.py --check-candidate --nss /path/to/certdata.txt
```

`release-policy.json` binds the manifest metadata to the reviewed source, a
maximum 90-day review window, and known TLS retirement dates. USERTrust RSA/ECC
were omitted at initial release because they are
[scheduled to lose Mozilla websites trust April 15, 2027](https://bugzilla.mozilla.org/show_bug.cgi?id=2017182).
They remain TLS-trusted in this snapshot; their exclusion is an intentional
maintenance decision. A custom server whose chain requires USERTrust must
renew or configure a chain to a supported root. A chain through Sectigo R46/E46
can use those roots directly without trusting USERTrust.
SSL.com's legacy pair are [scheduled by December 31, 2028](https://bugzilla.mozilla.org/show_bug.cgi?id=2032170).
The release checker refuses roots within 90 days of those dates or certificate
expiry. These are release-maintenance gates, not runtime distrust dates; already
installed firmware needs an update to remove a root. Keeping SSL.com's legacy
pair preserves Cloudflare-chain compatibility, but requires timely removal.

Watch Let's Encrypt's ISRG Root YE/YR inclusion in Mozilla and its chain rollout.
The current YE/YR fixtures rely on cross-signs to X1/X2. Review adding their own
anchors before servers stop sending those cross-signs; do not add unreviewed
roots solely because a new hierarchy exists.

Before approving a release:

1. Fetch a current official NSS snapshot and review its TLS trust bits and any
   distrust-after constraints. Recreate the audit, pin the commit/hash, and check
   [additional Mozilla trust changes](https://wiki.mozilla.org/CA/Additional_Trust_Changes)
   and vendor/browser incident advisories. Do not just advance the review dates.
2. Review [Cloudflare's current CAs](https://developers.cloudflare.com/ssl/reference/certificate-authorities/)
   and [backup-certificate policy](https://developers.cloudflare.com/ssl/edge-certificates/backup-certificates/).
   Test supported chain fixtures and the real endpoint. This selected store is
   not a promise to cover every present/future Cloudflare chain.
3. Revisit scheduled retirements, update the PEMs/manifest/policy together,
   regenerate the header, run both store tests and measure the final image.
4. Obtain review of the final root list and complete the exact-image hardware
   timing/full-handshake check. Only then mark the manifest and **every root**
   explicitly `shipping_approved: true`, recompute its roots metadata hash and
   bind the policy to it. Missing approval, false, strings or numeric values
   never count as approval. Run the checker without `--check-candidate`.

The 11-root selection is approved in the owner/Claude review. Shipping approval
is still false while its final-image hardware checks await the .103 soak's end.

Firmware release preparation for version 127 or later runs the same check before
reading a private key or creating output. It has no network dependency and adds
no device RAM. The audit is evidence rather than a cryptographic attestation;
reviewers still own the policy decision. Build/signing source provenance and
the exact-image performance gate remain separate requirements.

The device does not fetch CRLs, OCSP, OneCRL or CRLite. NSS inclusion alone
therefore does not reproduce browser verification. The recent
[eMudhra intermediate compromise](https://www.sectigo.com/resource-library/advisory-on-compromised-emudhra-intermediate-certificates)
was reviewed; do not add its roots or the legacy AAA Certificate Services root
to this catalog. This review is not an exhaustive proof that every possible
subordinate chain is safe. A relevant new incident requires prompt investigation
and, where needed, a signed firmware update that removes the affected trust.

Roots are updated only through signed firmware; there is no independent root
download mechanism. If the service moves to an unsupported CA, local access and
local updates remain available. The browser downloads a release and uploads it
to the device, so recovery does not depend on the device's TLS trust working.
Never automatically fall back to plain WebSocket.
