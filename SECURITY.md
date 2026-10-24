# Reporting security issues

Please report suspected security vulnerabilities privately to
[support@openthings.io](mailto:support@openthings.io) before opening a public issue.
Include the hardware and firmware versions, reproduction steps, and the impact
you observed. Do not include real device keys, cloud tokens or email passwords.

For ordinary bugs and feature requests, use GitHub Issues.

Keep OpenGarage's local HTTP interface on a trusted network; do not expose it
directly to the Internet. Change the default device key. The local HTTP API does
not encrypt credentials in transit.
