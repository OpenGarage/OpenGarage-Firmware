# Regenerating the Security+ 2.0 client ID

In this development build, open **Options → Advanced → Regenerate Security+ 2.0 Client ID**.
Enter the Device Key and confirm the restart. Save any other edited settings first.
Security+ 2.0 must already be selected and saved.

This replaces only the stored client ID; WiFi and other saved settings are preserved.
The new hexadecimal ID ends in `2908`. OpenGarage restarts and begins synchronization
with the opener; regeneration does not issue a door command. Check `secplus2_client_id`
in `/db` after reconnecting. Use this for troubleshooting, not routine operation.

The local API is `POST /secplus2/regenerate-id`, with the Device Key in the
raw UTF-8 request body (`Content-Type: text/plain;charset=UTF-8`), not the URL.
Do not form-encode the key or append a newline. GET cannot regenerate an ID.
The JSON `result` is 1 on success, 2 for an incorrect/missing key, or 0 for
a rejected request or storage error; `message` explains the outcome and success
also returns `client_id`. Pending warnings, queued commands and release sequences
block regeneration. If a storage write cannot be verified, the device restarts
with controls inhibited until restart; inspect identity diagnostics afterward.
If the HTTP connection is lost, check `/db` before retrying: a missing response
does not prove the operation failed.

Existing read-only endpoints remain GET. Older state-changing GET endpoints are
unchanged for compatibility; migrating those is separate from this addition.
