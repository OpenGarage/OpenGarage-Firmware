## Firmware 1.2.8 API Reference

### 1. Overview
* Existing endpoints that require a device key use `dkey=xxx`; the regeneration endpoint uses a request body and OTA uses a header instead. At factory reset, the **default device key** is `opendoor`.
* The device's IP address is referred to as `devip`. For most commands, parameters are optional and the order of parameters does not matter.
* Return values are all formatted in JSON, for example: `{"result":1}`. Below is the list of error codes:

|Code|Meaning|
|:---|:------|
|`0` |Operation failed (used by `/s2id_gen`; see its `message`)|
|`1` |Success|
|`2` |Unauthorized (e.g. missing device key or device key is incorrect)|
|`3` |Mismatch (e.g. new device key and confirmation key do not match)|
|`16`|Data Missing (e.g. missing required parameters)|
|`17`|Out of Range (e.g. value exceeds the acceptable range)|
|`18`|Data Format Error (e.g. provided data does not match required format)|
|`32`|Page Not Found (e.g. page not found or requested file missing)|
|`48`|Not Permitted (e.g. unsupported controller action)|
|`64`|Upload failed (e.g. OTA firmware update failed)|

Some error replies include an `item` field naming the rejected parameter or explaining the failure.

<br>
#### What's New in This Firmware (1.2.8)

* `mqrt` retains the settled door state on MQTT: `OGTOPIC/OUT/STATE` is published with the `retain` flag for `OPEN`, `CLOSED` and `STOPPED` (off by default).
* `mgrd` ignores door commands from OpenGarage's own interfaces while the door is still moving because of an earlier command (off by default).
* `OGTOPIC/OUT/STATE` also reports `OPENING` and `CLOSING`. They are published as soon as a command that should move the door is sent, and keep being reported until the sensor confirms the new state.
* `OPEN`, `CLOSED` and `STOPPED` are published on `OGTOPIC/OUT/STATE` as soon as the change is detected, not only on the next 15 second update.

<span class="hl">New or modified API contracts in firmware 1.2.8 are highlighted in green.</span>

---

### 2. Get Controller Variables `/jc`
**Usage**: `http://devip/jc`

|Variable |Explanation |
|:--------|:---------- |
|`dist`   |Distance sensor value (unit: cm)|
|`sn2`    |Switch sensor value (present only if switch sensor is enabled)|
|<a id="jc_door"></a>`door`   |Door status (`0:closed; 1:open; 2:stopped (partially open); 3:closing; 4:opening; 5:unknown`)|
|`vehicle`|Vehicle status (`0:vehicle not detected; 1:detected; 2:unknown; 3:not available`)|
|`rcnt`   |Read count (increments every time sensor values are updated)|
|`fwv`    |Firmware version|
|`name`   |Device name|
|`mac`    |MAC address|
|`cid`    |WiFi chip ID|
|`rssi`   |WiFi signal strength (dBm)|
| <code>cld</code> | Cloud option (<code>0:none; 1:retired Blynk (inactive); 2:OTC</code>) |
| <code>clds</code> | Cloud connection status (present only when <code>cld &gt; 0</code>); retired Blynk reports 0. For OTC: <code>0:not enabled; 1:unable to connect (displayed as connecting); 2:disconnected; 3:connected</code> |
|`temp`   |temperature reading (Celsius), only if T/H sensor is enabled|
|`humid`  |humidity reading (relative percentage), only if T/H sensor is enabled|
|`secv`   |Security+ version (`2:v2.0; 1:v1.0; 0:None`)|
|`has_swrx`|Support for software RX (required for native support of Security+ 2.0/1.0)|
|`light`  |Light status (`0:off; 1:on`)|
|`lock`   |Remote lock status|
|`obstruct`|Obstruction sensor status|
|`nopenings`|Number of times the door has opened (available only if `secv=2`)|
|`pemu`   |Panel emulator status (`0:inactive; 1:active; 2:detecting`, available only if `secv=1`)|
|<code>door_valid</code>|Door status validity: 0 unknown/stale, 1 valid (Security+ 1.0/2.0).|
|<code>light_lock_valid</code>|Validity of light and remote-lock status: 0 unknown/stale, 1 valid (Security+ 1.0/2.0).|
|<code>obstruct_valid</code>|Obstruction status validity: 0 unknown/stale, 1 valid (Security+ 1.0/2.0).|
|<code>panel_status</code>|Panel detection/emulation description (Security+ 1.0 only).|
|<code>command_status</code>|Command progress/outcome description (Security+ 1.0 only).|
|<code>recovery_status</code>|Communication recovery description (Security+ 2.0 only); not a transmission acknowledgement.|
|<code>control_fault</code>|0 no latched fault; 1 uncertain actuation failure has inhibited controls (Security+ 2.0 only).|

<br>These fields are present only on Security+-capable hardware with the corresponding protocol selected. Expired door status uses <code>door=5</code>; light, lock and obstruction retain their last-known Boolean values for compatibility. Check the validity fields before treating them as current. Diagnostic text is not a stable enumeration.

---

### 3. Change Controller Variables `/cc`

**Usage**: `http://devip/cc?dkey=xxx&click=1`

Send one action per request; supported parameters are listed below. Result `1` means the request was accepted, not that the physical action completed: state or maintenance guards may prevent execution, and light/lock values other than `toggle` do nothing.

|Parameters |Meaning |
|:----------|:---------- |
|`dkey`     |Device key (at factory reset it is `opendoor`)|
|`click`    |Toggle door|
|`close`    |Trigger door close action|
|`open`     |Trigger door open action|
|`reboot`   |Reboot the controller|
|`apmode`   |Reset the controller to WiFi AP mode (for reconfiguring WiFi setting)|
|`light`    |Toggle light. Supported only for Security+ 2.0/1.0. The only accepted value currently is `toggle`.|
|`lock`     |Toggle remote lock. Supported only for Security+ 2.0/1.0. The only accepted value currently is `toggle`.|

In dry-contact and Security+ 1.0 modes, `open` and `close` issue a toggle when the requested direction is valid for the reported door state. Only Security+ 2.0 sends directional Open and Close commands.

<br>**Examples:**

* `http://devip/cc?dkey=xxx&click=1`: toggle door
* `http://devip/cc?dkey=xxx&close=1`: request door close
* `http://devip/cc?dkey=xxx&reboot=1`: reboot device
* `http://devip/cc?dkey=xxx&light=toggle`: toggle light

---

### 4. Get Options `/jo`
**Usage**: `http://devip/jo`

<u>Underlined</u> values indicate factory default values.

|Variable |Explanation|
|:---------|:------------|
| `fwv` | Firmware version (read-only) |
| `has_swrx` | Native Security+ hardware capability (read-only, 0 or 1) |
| `mod` | Saved WiFi mode (read-only here): `169` AP, `42` station |
| `bas` | Status LED blink limit in status-read cycles (`0`: unlimited; maximum `99`) |
| `secv` | Security+ version (`0:None; 1:1.0; 2:2.0`); native Security+ requires v2.3+ hardware |
|<code>pem</code>|Security+ 1.0 panel emulation (<code>0:Auto</code>, default; <code>1:Disable</code>). Only configurable on v2.3+ hardware with Security+ 1.0 selected.|
|<code>s2id</code>|Stored Security+ 2.0 client ID (hexadecimal; may appear even when another protocol is selected), or <code>Unavailable</code> if identity initialization fails.|
| `sn1` | Distance sensor (`sn1`) mounting type (<code><u>0:ceiling mount</u>; 1:side mount</code>)|
| `sn2` | Switch sensor (`sn2`) type (<code><u>0:none</u>; 1:normally closed; 2:normally open</code>) |
| `sno` | Sensor logic: door 'open' status is determined by: <code><u>0:use sn1 only</u>; 1:sn2 only; 2:sn1 AND sn2; 3:sn1 OR sn2</code>. This option has no effect for Security+ 2.0/1.0|
| `dth` | Door distance threshold (unit: `cm`, used to detect if the door is open). This option has no effect for Security+ 2.0/1.0|
| `vth` | Vehicle distance threshold (unit: `cm`, used to detect if a vehicle is present) |
| `riv` | Status and sensor check interval (unit: `second`, default is `1`) |
| `alm` | Sound alarm: <code>0:no alarm; <u>1:5-second alarm</u>; 2:10-second alarm</code>|
| `aoo` | Disable alarm on opening (<code><u>0:no</u>; 1:yes, i.e. alarm disabled</code>) |
| `lsz` | Log size (e.g. `50` means the controller keeps the most recent `50` records) |
| `tsn` | Temperature/humidity sensor type (<code><u>0:none</u>; 2:DHT11; 3:DHT22; 4:DS18B20</code>). Note that the previous `AM2320` type is no longer supported due to GPIO pin conflict with OpenGarage v2.3+ |
| `htp` | HTTP port (default `80`, range `80`–`65535`); <code>8080</code> is reserved for firmware updates and rejected with result <code>17</code> |
| `cdt` | Dry-contact relay pulse duration (unit: `ms`, default is `1000`) |
| `dri` | Distance reading interval (unit: `ms`, default is `500`) |
| `sfi` | Sensor filtering method (<code>0:median; <u>1:consensus</u></code>). Consensus averages the tightest five of seven readings if their spread meets <code>cmr</code>; otherwise the last accepted distance is retained. |
| `cmr` | Consensus margin for the consensus method (unit: `cm`, default is `10`) |
| `sto` | Sensor timeout handling option (<code><u>0:ignore</u>; 1:cap to maximum value</code>) |
| `ati` | Automation rule A time (unit: `minutes`): detect if the door is left open for longer than `ati` minutes |
| `ato` | Automation rule A option (`bit 0:notify; bit 1:auto-close`) |
| `atib` | Automation rule B time (unit: `UTC hour`, detect if the door is left open after hour `atib:00 UTC`) |
| `atob` | Automation rule B option (`bit 0:notify; bit 1:auto-close`) |
| `noto` | Choose notification events (`bit 0:door OPEN event; bit 1:door CLOSED event; bit 2:door STOPPED event`) |
| `usi` | Use static IP (<code><u>0:use DHCP</u>; 1:use static IP</code>). |
| `dvip`,<br>`gwip`,<br>`subn`,<br>`dns1` | Customizing device IP, gateway IP, subnet mask, and DNS IP in static IP mode. Effective only when `usi=1`|
| <a id="jo_cld"></a><code>cld</code> | Cloud connection type (<code><u>0:none</u>; 2:OTC; legacy 1 is inactive</code>) |
| <code>auth_set</code> | 1 if a cloud authorization token is stored, otherwise 0. The secret itself is not returned; write it through <code>/co</code>. |
| `bdmn` | Cloud server domain name (refer to [cloud connection](#10-cloud-connection) for details) |
| `bprt` | Cloud server port (refer to [cloud connection](#10-cloud-connection) for details) |
| `name` | Device name (default is `My OpenGarage`); values longer than 64 UTF-8 bytes are rejected with result <code>17</code> (<code>item</code> names the field) |
| <code>iftt_set</code> | 1 if an IFTTT Maker channel token is stored, otherwise 0. The secret itself is not returned; write it through <code>/co</code>. |
| <code>wfr</code> | WiFi AP selection: <code>1</code> Automatic (default), <code>0</code> Off. Off retains automatic reconnection without AP selection or roaming. |
| `ssid` | SSID of the WiFi network this device is connected to |
| `mqen` | MQTT enable (<code><u>0:disabled</u>; 1:enabled</code>). Note: this option preserves other mqtt option values when temporarily disabling MQTT. |
| `mqtt` | MQTT server url (IP or domain name both allowed) |
| `mqpt` | MQTT port (default is `1883`) |
| `mqur` | MQTT server user name (if authentication is required) |
| `mqpw` | MQTT password (this variable is NOT shown in the result of `/jo`, but you can change it using the `/co` command in the [section below](#5-change-options-co).) |
| `mqtp` | MQTT topic (optional, if left empty it will use the Device name as the topic); values longer than 64 UTF-8 bytes are rejected with result <code>17</code> (<code>item</code> names the field) |
| <span class="hl"><code>mqrt</code></span> | <span class="hl">Retain door state (<code><u>0:no</u>; 1:yes</code>). When `1`, <code>OGTOPIC/OUT/STATE</code> is published with the MQTT <code>retain</code> flag for the settled states <code>OPEN</code>, <code>CLOSED</code> and <code>STOPPED</code>, so a client that subscribes later receives the last settled state at once. <code>OPENING</code> and <code>CLOSING</code> are never retained, and neither is the plain <code>OGTOPIC</code> copy (the device subscribes to it). Setting <code>mqrt=0</code> or <code>mqen=0</code> deletes the retained copy while the device is connected to the broker. The retained state is not marked stale if the device goes offline; check <code>OGTOPIC/OUT/STATUS</code>.</span> |
| <span class="hl"><code>mgrd</code></span> | <span class="hl">Ignore door commands while the door is moving (<code><u>0:no</u>; 1:yes</code>). When `1`, a door command sent through OpenGarage (HTTP, MQTT or the cloud) is ignored while the door is still moving because of an earlier one. It ends when the door reports <code>OPEN</code>, <code>CLOSED</code> or <code>STOPPED</code> after moving, or after 45 seconds. The command is still acknowledged with <code>{"result":1}</code>. A hardware remote and the OpenGarage button are not affected.</span> |
| `emen` | [Email enable](../guides/email-notifications.md) (<code><u>0:disabled</u>; 1:enabled</code>) |
| `smtp` | SMTP server name (default is `smtp.gmail.com`) |
| `sprt` | SMTP server port (default is `465`) |
| `send` | Email sender (for authentication with the SMTP server) |
| <code>apwd_set</code> | 1 if an email app password is stored, otherwise 0. The secret itself is not returned; write it through <code>/co</code>. |
| `recp` | Recipient email (for receiving notification, may or may not be the same as the sender) |
| `ntp1` | NTP server url (optional, if customizing NTP server) |
| `host` | Custom Host name (used for the mDNS feature). For example, if `host=my_og`, you can access the device via its mDNS name `my_og.local/`) |

---

### 5. Change Options `/co`

**Usage**: <code>http://devip/co?**dkey**=xxx&**nkey**=xxx&**ckey**=xxx&**opn**=opv...</code>

* `opn` is a writable option name from the table above, and `opv` is its new value. The read-only `s2id` field cannot be changed through `/co`; use [POST `/s2id_gen`](#12-regenerate-sec-20-client-id) to regenerate it.
* `pem` is writable only on v2.3+ hardware with Security+ 1.0 selected (`0:Auto`, `1:Disable`).
* <code>wfr=1</code> enables automatic AP selection/roaming (default); <code>wfr=0</code> disables it without disabling reconnection or offline operation.
* `fwv` and `ssid` cannot be changed through `/co`; use WiFi setup to change the network.
* The device key `dkey` may be changed only by providing both `nkey` (new key) and `ckey` (confirm key) and they must match exactly.

#### Secret fields and cloud options {.hl}

For `auth`, `apwd`, `iftt` and `mqpw`, omitted or blank values keep the stored secret; nonempty values replace it. The `/jo` presence flags (`auth_set`, `apwd_set`, `iftt_set`) are read-only.
{: .hl}

To erase a secret, send `clear_auth=1`, `clear_apwd=1`, `clear_iftt=1` or `clear_mqpw=1`. A clear flag other than `1`, or combining clearing and replacement, returns result `18`. Disable OTC before or when clearing its token.
{: .hl}

Saved Blynk settings (`cld=1`) are inactive; `/co` accepts only `cld=0` (None) or `cld=2` (OTC), rejecting `cld=1` with result `17`. When OTC is enabled, every options save requires a stored or supplied OTC token of at least 32 characters; otherwise it returns result `17`, even for unrelated changes.
{: .hl}

<br>**Examples:**

* `devip/co?dkey=xxx&nkey=abc&ckey=abc`: set device key to `abc`
* `devip/co?dkey=xxx&dth=75`: set door's distance threshold to `75cm`
* `devip/co?dkey=xxx&cld=2&auth=OT0123456789abcdef0123456789abcd&bdmn=ws.cloud.openthings.io&bprt=80`: configure OTC with a new token (replace the example token with yours)
* `devip/co?dkey=xxx&htp=8081&riv=2`: set HTTP port to `8081` (port `8080` is reserved for firmware updates), and set the status-check interval option to `2` seconds (the raw ultrasonic sampling interval is `dri`, in milliseconds)
* `devip/co?dkey=xxx&ati=5&ato=3`: set automation rule A time to `5` minutes and its option to `0b11` (i.e. auto-notify and auto-close if the door is left open for `>5 minutes`).
Duration-based automation uses elapsed uptime and continues without WiFi. Time-of-day automation waits for clock synchronization after boot, then runs at most once per UTC day. Manually opening the door during the configured hour skips that day's scheduled close.

---

### 6. Get Logs `/jl`
**Usage**: `http://devip/jl`

| Variable | Explanation |
|:---------|:------------|
| `name`   | Device name |
| `time`   | Device time (UTC epoch time), or 0 before time synchronization |
| <code>uptime</code> | Device uptime in seconds, independent of time synchronization |
| `starttime`| Estimated boot time (UTC epoch time), or 0 before time synchronization |
| `ncols`  | Number of columns (`3` or `4` depending on if `sn2_value` is present in the log data)|
| `logs`   | Log data: an array of log entries, each in the format of `[time_stamp, door_status, distance_value, sn2_value]`. A positive <code>time_stamp</code> below 1577836800 represents an unsynchronized record (uptime seconds plus one), not a calendar date. Zero is reserved for empty log slots. Note that `sn2_value` is only available if `sn2` is enabled.|

---

### 7. Clear Logs `/clearlog`

**Usage**: <code>http://devip/clearlog?**dkey**=xxx</code>

Clears the entire log data.

---

### 8. Factory Reset `/resetall`

**Usage**: <code>http://devip/resetall?**dkey**=xxx</code>

Resets the controller back to factory default settings. WiFi will be restored to AP (Access Point) mode.

Factory reset also generates a new Security+ 2.0 client ID on the next setup. Reboot, OTA update and WiFi-only reset preserve the identity.

---

### 9. MQTT

To use MQTT features:

* Enable MQTT (set `mqen=1`) and provide server parameters (server url `mqtt`, port `mqpt`, user name `mqur` and password `mqpw` if using authentication).
* You may define a custom MQTT topic (`mqtp`). If left empty, the device name will be used as the topic. In the example below, the topic is referred to as `OGTOPIC`.
* Topics use the configured prefix exactly: `OGTOPIC/OUT/JSON` has no leading slash unless the prefix itself starts with one.
* To disable MQTT, set `mqen=0`, which preserves the other values, allowing you to easily switch between enabling and disabling MQTT without the loss of other parameters.

| Published Message | Explanation |
|:------------------|:------------|
|`OGTOPIC/OUT/NOTIFY`| Published upon changes in door status, including just `OPENED`, just `CLOSED`, or just `STOPPED`. |
|`OGTOPIC/OUT/STATUS`| Report device online/offline status. |
|`OGTOPIC/OUT/STATE` | <span class="hl">Reports the current door state: `OPEN`, `CLOSED`, `STOPPED`, `OPENING` or `CLOSING`. Published as soon as a door action that should move the door is sent (`OPENING` or `CLOSING`), as soon as a state change is detected, and every 15 seconds. The same value is published in title case (`Open`, `Closed`, `Stopped`, `Opening`, `Closing`) on `OGTOPIC` for existing integrations. On a door with no Security+ link the sensor lags the real move, so after a command sent through OpenGarage the 15 second updates keep reporting `OPENING` or `CLOSING` until the sensor confirms the new state (45 seconds at most). The settled states are retained when `mqrt=1`.</span> |
|`OGTOPIC/OUT/JSON`  | Published on door-state events and periodically (approximately every 15 seconds), with the same controller variables as [`/jc`](#2-get-controller-variables-jc). Includes Security+ validity and diagnostic fields; use this JSON rather than the limited text state to identify Unknown. |

<br>**Subscribed Message**:

Publish commands with the MQTT <code>retain</code> flag disabled (<code>retain: false</code>). Broker-retained commands received on subscription/reconnection are ignored, including the legacy <code>Button</code> command. Clear any retained command previously stored on your broker. A newly published command delivered to an already subscribed client normally has its <code>retain</code> bit cleared by the broker; this protection targets stored-message replay.

The controller subscribes to topic `OGTOPIC/IN/STATE` and accepts the following payload messages:

* `click`: Toggle door
* `close`: Trigger door close action
* `open` : Trigger door open action
* `togglelight`: Toggle light. Supported only for Security+ 2.0/1.0
* `togglelock`: Toggle remote lock. Supported only for Security+ 2.0/1.0

The `open` and `close` payloads have the same mode-dependent behavior as the [HTTP `/cc` actions](#3-change-controller-variables-cc).

---

### 10. Cloud Connection

The cloud connection type is defined by the [`cld` option](#jo_cld). OTC is the only supported cloud service in 1.2.8. Local HTTP and MQTT remain available without cloud.

#### OTC (OpenThings Token)

OTC is based on our own [OpenThingsFramework (OTF) proxy](https://github.com/openthingsio). For token creation and device setup, see [Remote Access with OTC](../guides/remote-access-otc.md).

* Default server domain name (`bdmn`) is `ws.cloud.openthings.io`
* Default server port (`bprt`) is: `80`. Change these accordingly if using your own OTC server.

Once connected, you can use the same **OpenGarage API** (this document) via the OTC cloud. The base URL is `https://cloud.openthings.io/forward/v1/token`. For example:

* `https://cloud.openthings.io/forward/v1/token/` returns the controller homepage, where `token` is the 32-character OTC token.
* `https://cloud.openthings.io/forward/v1/token/jc` returns [controller variables](#2-get-controller-variables-jc).
* `https://cloud.openthings.io/forward/v1/token/jl` returns [log data](#6-get-logs-jl).
* <u>**NOTE**</u>: Existing endpoints use the OTC token in place of the device key (`dkey`). The <code>POST /s2id_gen</code> endpoint is an exception: it requires the current device key in the request body even over OTC.

---

### 11. Debug `/db`

**Usage:** `http://devip/db`

Returns compact debug information. AP mode returns only `dist`, `fwv` and `has_swrx`; station mode returns the other fields below instead of `dist`.

| Field | Meaning |
|:------|:--------|
| `fwv` | Firmware version, e.g. 128 for 1.2.8 |
| `has_swrx` | Native Security+ hardware capability (0 or 1) |
| `dist` | Distance reading in centimeters |
| `rcnt` | Sensor/status read count |
| `name` | Device name |
| `mac` | Device MAC address |
| `mqtt_topic` | Runtime MQTT topic prefix |
| `devip` | Device IP address |
| `cid` | ESP8266 chip ID (not the Security+ client ID) |
| `rssi` | WiFi signal strength in dBm |
| `bssid` | Connected access point's MAC address |
| `build` | Firmware compilation date |
| `Freeheap` | Legacy free heap in bytes, cast to 16 bits; retained for compatibility |
| `flash_size` | Physical flash size in bytes |

---

### 12. Regenerate Sec+ 2.0 Client ID

**Method:** `POST /s2id_gen`

Send the current device key as the **raw UTF-8 request body**, not a URL parameter or JSON object. This endpoint requires the device key even over OTC.

The device must have Security+ 2.0 saved, be connected, and have no pending warning, command or release cleanup. This is a maintenance operation: it saves a new identity and restarts OG, preserving WiFi and other settings.

- Success: `{"result":1,"client_id":"0x…","message":"…"}`.
- Incorrect device key: `{"result":2}`.
- Wrong mode, busy device or storage failure: `{"result":0,"message":"…"}`.

After storage, OG automatically restarts and uses the new client ID.

---

### 13. Firmware Upload {.hl}

**Method:** `POST http://devip:8080/update` (local network only, in station or AP mode).

Send exactly one firmware file as a multipart form field named `file`. The key is checked in the `X-OG-Device-Key: hex:<encoded-key>` header before any body is read, where `<encoded-key>` is the lowercase hexadecimal encoding of the current device key's UTF-8 bytes. This is encoding, not encryption; use a trusted local network.
{: .hl}

The browser's Firmware Update page handles this automatically and displays upload progress. Scripts that put `dkey` only in a multipart field must be updated. This contract applies to the firmware currently receiving the upload: the first upgrade from 1.2.5 uses its old uploader, with `dkey` in the multipart form or query string.
{: .hl}

* **HTTP 401**, result `2`: missing/incorrect key; the body is not processed.
    {: .hl}
* **HTTP 409**, result `64`: device busy (warning, pending Security+ command/restart, within five seconds of a door action, or an update already running). The body is not processed; wait before retrying.
    {: .hl}
* **HTTP 200**, result `2`: the key failed the final authorization check.
    {: .hl}
* **HTTP 200**, result `64`: upload failed.
    {: .hl}
* **HTTP 200**, result `1`: upload succeeded; the device restarts.
    {: .hl}

Malformed or stalled requests may close without a JSON reply. A lost reply or timeout does not establish that installation failed; check the device before retrying. Check both HTTP status and JSON result, not an upload-progress value of 100%.
{: .hl}

Use an uncompressed ESP8266 image with a `.bin` filename, at most 1,044,464 bytes; gzip images are not accepted. Sensing and automation pause during the upload.
{: .hl}

Example using Bash, Python 3 and curl (enter the device's current key):

```sh
read -rsp 'Device key: ' OG_KEY; echo
OG_KEY_HEX=$(printf '%s' "$OG_KEY" | python3 -c 'import sys; print(sys.stdin.buffer.read().hex())')
curl --max-time 150 --config - --form 'file=@og_1.2.8.bin' http://devip:8080/update <<EOF
header = "X-OG-Device-Key: hex:$OG_KEY_HEX"
EOF
unset OG_KEY OG_KEY_HEX
```

Replace `devip` with the local IP; in AP mode use `192.168.4.1`. Firmware upload is not supported over OTC.
{: .hl}
