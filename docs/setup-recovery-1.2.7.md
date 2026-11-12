# Setup WiFi and recovery in firmware 1.2.7

The open setup WiFi is set to close after 30 minutes per setup session.
Connecting a phone, keeping the page open or submitting requests does not extend
that window. Some operations are refused shortly before it ends so they have
time to finish. Shutdown occurs when the main loop next runs; a request still
finishing can briefly delay it.

If the device connects while the page reports that little setup time remains,
it can still restart on your home WiFi. Switch your phone back to that network
and try accessing the device there.

At expiry, WiFi shuts down and the device stays in setup mode, even if a network
is saved. Expiration does not save settings, restart the device or switch to
station mode. The key and saved WiFi settings remain intact. Only successful
WiFi setup makes the normal transition to station mode.

To correct WiFi settings, hold the device button for about **5 seconds**, then
release it **before 10 seconds**. Rejoin its setup WiFi and enter the existing
device key. This WiFi-only reset preserves the key; a factory reset is not needed.
Holding the button longer than 10 seconds instead requests a factory reset.

While holding the button, short beeps mark the available action:

- One beep after 0.8 seconds: release to hear the IP address.
- Two quick beeps after 4.5 seconds: release to reopen setup WiFi.
- Three quick beeps after 9.5 seconds: release to factory-reset.

Each marker sounds once per hold; the action happens only on release. Markers
are suppressed during a door warning, an IP announcement or an active firmware
upload. Release after the two-beep marker, before the three-beep marker, for a
WiFi-only reset.

Expiration leaves WiFi off and the LED toggling
every two seconds. The same button gesture reopens setup.
