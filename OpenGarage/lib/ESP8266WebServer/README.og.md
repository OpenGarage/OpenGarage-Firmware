# OpenGarage bounded updater parser

Source: ESP8266 Arduino core3.1.2, supplied by espressif8266@4.2.1.
Original LGPL notices are retained. Used only by the port8080 updater; OTF's
port80 parser is separate. Local library takes precedence over the core copy.

Changes:5s total request-header deadline,8KiB aggregate header cap,1024-byte
line cap,10s upload-byte idle deadline,120s overall request limit,70-byte
multipart boundary cap,3MiB multipart body admission cap. Non-multipart bodies
are rejected before any body read. A required onBodyAuthorization callback runs
after headers and before multipart parsing; the OG callback checks POST, route,
key/token and busy state without consuming a token or changing service state.
Only multipart file parts are accepted (existing OG pages send credentials in
headers). Unexpected EOF aborts safely, even before a file was allocated.
All time differences use unsigned subtraction. A timeout stops the TCP client;
normal application-loop service resumes after parser cleanup. This is still a
synchronous parser: it does not promise continuous sensing during an upload.

Keep these changes small and propose the same reliability fix for OpenSprinkler.
When upgrading the core, rebase and rerun parser/upload tests. Do not patch the
shared PlatformIO installation; other projects must remain unaffected.
