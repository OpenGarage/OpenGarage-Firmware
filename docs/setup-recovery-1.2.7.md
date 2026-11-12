# Setup WiFi and recovery in firmware 1.2.7

The open setup WiFi is available for at most 30 minutes per setup session.
Connecting a phone, keeping the page open or submitting requests does not extend
that window. Some operations are refused shortly before it ends so they have
time to finish.

If the device connects while the page reports that little setup time remains,
it can still restart on your home WiFi. Switch your phone back to that network
and try accessing the device there.

At expiry, a device with an established key and saved network tries that network
in station mode. If you saved an incorrect WiFi password, it can therefore stay
offline. It does not automatically reopen setup after connection failures.

To correct WiFi settings, hold the device button for about **5 seconds**, then
release it **before 10 seconds**. Rejoin its setup WiFi and enter the existing
device key. This WiFi-only reset preserves the key; a factory reset is not needed.
Holding the button longer than 10 seconds instead requests a factory reset.

Without a saved key/network, expiration leaves WiFi off and the LED toggling
every two seconds. The same button gesture reopens setup.
