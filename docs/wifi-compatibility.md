# WiFi compatibility mode (1.2.7 development)

Advanced options include **WiFi compatibility mode (802.11g)**, off by default.
Enable it when troubleshooting WiFi-related resets with particular routers, save,
and reboot. It disables 802.11n capability; it does not automatically switch back
if connection fails. Routers configured to accept only 802.11n may reject it.

The option is a workaround for an observed ESP8266 WiFi watchdog failure, not a
confirmed root-cause fix or guarantee against resets. Normal/off mode enables
802.11n, preserving normal device capabilities.

API: `wfg=0` (normal) or `wfg=1` (compatibility), saved through the existing
key-protected `/co` GET/form POST paths and returned by `/jo`. Omitting it leaves
the saved value unchanged. Existing configurations and factory defaults use 0.
Changes require a reboot. Radio recovery reapplies the mode selected at startup.

Verbose `/db?verbose=1` reports `wifi_phy`: 1=802.11b, 2=802.11g,
3=802.11n enabled. This is the configured radio capability, not the instantaneous
transmission rate. It can differ from saved `wfg` until reboot.

## Downgrading

**Turn compatibility mode off, save, and reboot before downgrading to 1.2.6 if
you want normal 802.11n capability.** A bench downgrade retained 11g, and 1.2.6
has no UI option to turn it off. Returning to 1.2.7 and selecting off/reboot
restores 11n.

Validation: protocol-None bench at production 80 MHz passed on/off/reboot checks.
The downgrade measurement used released 1.2.6 source with only a read-only
`WiFi.getPhyMode()` diagnostic added, not the byte-identical release binary.
Host tests cover both modes through reconnect, AP trial connection, roaming and
scan-timeout radio recovery. Hardware fault-injection of roaming/scan timeout
and an n-only router test are not covered by this check.
