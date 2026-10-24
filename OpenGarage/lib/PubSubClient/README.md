# PubSubClient 2.8 — OpenGarage retain metadata patch

Source: https://github.com/knolleary/pubsubclient/tree/v2.8/src
License: MIT (LICENSE.txt). Only the two required source files are included.

Local change: snapshot the incoming PUBLISH retain bit before invoking either
QoS callback and expose it through incomingRetained(). The existing callback API
and upstream protocol behavior are preserved. The snapshot survives outgoing
publishes made inside a callback. OpenGarage rejects retained actuator commands
so reconnecting cannot execute a broker-stored command.

Do not replace this with unmodified upstream 2.8 without preserving this check.
