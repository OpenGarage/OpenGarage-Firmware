# Secure OTC connections (1.2.7 development)

Secure WebSocket (WSS) encrypts the device's connection to OTC and verifies the
server's certificate. Secure cloud requires a working time source: the device
must obtain the current time through NTP after each restart before it connects.
Allow NTP requests and replies through your network, or configure a reachable
local NTP server in the device's options. A router is not necessarily an NTP
server.

While time is unavailable, the homepage shows **OTC — waiting for time**. Local
access and local control remain available. Check the configured NTP server and
your network's NTP restrictions if this message persists. Upgrading a device
that used plain WebSocket on the default OTC server enables secure cloud, so
networks that block NTP may lose remote access until this is resolved locally.
The firmware does not use its build date as the current time and does not
automatically fall back to an unencrypted connection.

The cloud settings offer **Disable secure WebSocket** as an explicit opt-out.
Save and reboot after changing it. Plain WebSocket does not provide transport
encryption. For the default OTC service, the firmware selects the appropriate
server and port. With **Custom**, configure the server and port yourself; the
checkbox selects security, and the port number does not select it.

During an email notification, a secure cloud connection is temporarily closed
and reconnects afterward to keep memory available for email. Plain WebSocket
connections are left connected. Reconnecting may briefly delay the main loop,
including physical-button handling. Local network service can also pause while
email or TLS connection work runs.
