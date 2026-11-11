# Legacy configuration fixtures

These are synthetic settings with field names/order taken from actual released
source layouts, independent of the production validator:

- `config-1.2.0.txt`: tag 1.2.0, commit `51a666c`.
- `config-1.2.2.txt`: commit `b1e1f8c`.
- `config-1.2.3.txt`: tag 1.2.3, commit `324fac6`.

Values are test data, not user settings or a claim about every release default.
They preserve station mode, an SSID, and a non-default key. The first two layouts
have no email fields; the third has no `bas`. Firmware versions did not uniquely
identify layouts. These fixtures reproduced the old validator's lockout before
its per-version masks were removed. The C++ harness now loads and migrates them.
