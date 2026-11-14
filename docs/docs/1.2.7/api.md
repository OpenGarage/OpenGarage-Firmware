## Firmware 1.2.7 API Reference

Development draft for firmware 1.2.7, including the approved secure-cloud candidate. Final release validation is still in progress.

### 1. Overview
* Existing endpoints that require a device key use `dkey=xxx`. See [`/co`](#5-change-options-co), [`/s2id_gen`](#12-regenerate-sec-20-client-id), [`/setup_key`](#15-setup-ap-apis) and [firmware uploads](#13-firmware-upload) for endpoint-specific credential requirements.
* The device's IP address is referred to as `devip`. <span class="hl">You can also use a resolvable hostname, for example <code>mygarage.local</code>, in place of the IP address.</span> For most commands, parameters are optional and the order of parameters does not matter.
* API replies normally use JSON, for example: `{"result":1}`. Transport/parser errors may instead be HTTP errors without JSON. Below is the list of API result codes:

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
|`64`|<span class="hl">Upload or settings-save failure; may also report updater admission/verification errors (check <code>item</code> and HTTP status)</span>|

Some error replies include an `item` field naming the rejected parameter or explaining the failure.

<br>
#### What's New in This Firmware (1.2.7)

* <span class="hl">OTC no longer bypasses the device key for control, settings, log clearing or factory reset. Read-only <code>/jc</code>, <code>/jo</code>, <code>/jl</code> and <code>/db</code> remain keyless.</span>
* <span class="hl"><code>/co</code> accepts URL-encoded POST bodies as well as the compatible GET query API. Long LAN GET requests now receive HTTP 414 instead of being silently truncated.</span>
* <span class="hl"><code>wfg</code> enables optional 802.11g compatibility. <code>otcm</code> selects Default/Custom OTC, and <code>nossl</code> selects secure/plain WebSocket.</span>
* <span class="hl"><code>kset</code> and <code>key_weak</code> report key/setup state. New keys follow the first-use rules; existing keys still authenticate unchanged.</span>
* <span class="hl"><code>/ad</code> now requires the key except on a first-use setup AP, refuses busy requests, and restarts the configured protocol after detection.</span>
* <span class="hl">New setup endpoints establish the first key and report the absolute 30-minute setup window. First-use firmware uploads have additional restrictions.</span>
* <span class="hl"><code>/jl</code> streams the same JSON without a Content-Length header. <code>/db?verbose=1</code> adds crash history, storage/radio and secure-cloud diagnostics.</span>

<span class="hl">New or modified API contracts in firmware 1.2.7 are highlighted in green. Existing 1.2.6 changes are no longer highlighted.</span>

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
| <span class="hl"><code>ui</code></span> | <span class="hl">Content ID of the device-served station UI; clients may reload when it changes.</span> |
| <span class="hl"><code>cloud_secure</code></span> | <span class="hl">1 when secure OTC is selected at runtime, otherwise 0; not proof of a connected session.</span> |
| <span class="hl"><code>cloud_tls_state</code></span> | <span class="hl">Secure-cloud state: <code>0:off; 1:ready; 2:insufficient memory; 3:waiting for time; 4:connecting; 5:connected; 6:failed</code>. Use with <code>clds</code>.</span> |
| <span class="hl"><code>time_valid</code></span> | <span class="hl">1 once the device clock has been synchronized, otherwise 0. Secure OTC requires valid current time after reboot.</span> |
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

In station mode, a missing or incorrect key returns `{"result":2}` even when the OTC token is valid. Setup mode uses a different `/cc` contract; see [Setup APIs](#15-setup-ap-apis).
{: .hl}

Send one action per request; supported parameters are listed below. Result `1` means the request was accepted, not that the physical action completed: state or maintenance guards may prevent execution, and light/lock values other than `toggle` do nothing.

|Parameters |Meaning |
|:----------|:---------- |
|`dkey`     |<span class="hl">Current device key, required locally and through OTC. A new/factory-reset unit first establishes its key in setup.</span>|
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
| `bdmn` | <span class="hl">Stored Custom OTC server hostname, up to 64 ASCII characters. Default mode chooses its runtime hostname independently; see [Cloud Connection](#10-cloud-connection).</span> |
| `bprt` | <span class="hl">Stored Custom OTC port, <code>1</code>–<code>65535</code>; the port does not select WS/WSS. Default mode chooses port 443 or 80 independently.</span> |
| <span class="hl"><code>otcm</code></span> | <span class="hl">OTC server selection (<code>0:Default</code>, factory default; <code>1:Custom</code>).</span> |
| <span class="hl"><code>nossl</code></span> | <span class="hl">Disable secure WebSocket (<code>0:secure WSS</code>, factory default; <code>1:plain WS</code>). Independent of <code>bprt</code>; save and reboot.</span> |
| <span class="hl"><code>kset</code></span> | <span class="hl">Read-only setup/key marker: <code>1</code> established, <code>0</code> first-use key step incomplete. Cannot be set directly through <code>/co</code>.</span> |
| <span class="hl"><code>key_weak</code></span> | <span class="hl">Read-only warning: <code>1</code> if the stored key does not meet current new-key rules, otherwise <code>0</code>. Does not prevent existing-key authentication.</span> |
| `name` | Device name (default is `My OpenGarage`); values longer than 64 UTF-8 bytes are rejected with result <code>17</code> (<code>item</code> names the field) |
| <code>iftt_set</code> | 1 if an IFTTT Maker channel token is stored, otherwise 0. The secret itself is not returned; write it through <code>/co</code>. |
| <code>wfr</code> | WiFi AP selection: <code>1</code> Automatic (default), <code>0</code> Off. Off retains automatic reconnection without AP selection or roaming. |
| <span class="hl"><code>wfg</code></span> | <span class="hl">WiFi compatibility (<code>0:normal 802.11n</code>, factory default; <code>1:802.11g</code>). Takes effect after reboot; an 11n-only router may reject 11g. Before downgrading to 1.2.6, turn this off and reboot: the radio setting can survive a downgrade.</span> |
| `ssid` | SSID of the WiFi network this device is connected to |
| `mqen` | MQTT enable (<code><u>0:disabled</u>; 1:enabled</code>). Note: this option preserves other mqtt option values when temporarily disabling MQTT. |
| `mqtt` | MQTT server url (IP or domain name both allowed) |
| `mqpt` | MQTT port (default is `1883`) |
| `mqur` | MQTT server user name (if authentication is required) |
| `mqpw` | MQTT password (this variable is NOT shown in the result of `/jo`, but you can change it using the `/co` command in the [section below](#5-change-options-co).) |
| `mqtp` | MQTT topic (optional, if left empty it will use the Device name as the topic); values longer than 64 UTF-8 bytes are rejected with result <code>17</code> (<code>item</code> names the field) |
| `emen` | [Email enable](../guides/email-notifications.md) (<code><u>0:disabled</u>; 1:enabled</code>) |
| `smtp` | SMTP server name (default is `smtp.gmail.com`) |
| `sprt` | SMTP server port (default is `465`) |
| `send` | Email sender (for authentication with the SMTP server) |
| <code>apwd_set</code> | 1 if an email app password is stored, otherwise 0. The secret itself is not returned; write it through <code>/co</code>. |
| `recp` | Recipient email (for receiving notification, may or may not be the same as the sender) |
| `ntp1` | <span class="hl">Optional NTP server hostname/IP, up to 64 UTF-8 bytes. Longer values are rejected by <code>/co</code>; changing it takes effect after reboot.</span> |
| `host` | Custom Host name (used for the mDNS feature). For example, if `host=my_og`, you can access the device via its mDNS name `my_og.local/`) |

---

### 5. Change Options `/co`

**GET usage**: `http://devip/co?dkey=xxx&opn=opv`

**New:** `POST http://devip/co`, with `Content-Type: application/x-www-form-urlencoded` and the same fields in the body. POST reads **only the body**: a key supplied only in the URL does not authorize it. Validation and result codes are shared with GET. Percent-encode all values, including keys, numbers and IP addresses. In form values, `+` decodes as a space; use `%2B` for a literal plus. Repeated fields use their last value.
{: .hl}


* `opn` is a writable option name from the table above, and `opv` is its new value. The read-only `s2id` field cannot be changed through `/co`; use [POST `/s2id_gen`](#12-regenerate-sec-20-client-id) to regenerate it.
* `pem` is writable only on v2.3+ hardware with Security+ 1.0 selected (`0:Auto`, `1:Disable`).
* <code>wfr=1</code> enables automatic AP selection/roaming (default); <code>wfr=0</code> disables it without disabling reconnection or offline operation.
* `fwv` and `ssid` cannot be changed through `/co`; use WiFi setup to change the network. <span class="hl"><code>kset</code>, <code>key_weak</code> and presence flags are also read-only.</span>
* The device key `dkey` may be changed only by providing both `nkey` (new key) and `ckey` (confirm key) and they must match exactly.

A different new key must be **10–64 printable ASCII characters**, with no leading/trailing spaces. Internal spaces and punctuation are allowed; mixed character classes are not required. Weak or Unicode keys saved by older firmware still authenticate. Resubmitting the exact existing key is allowed without forcing a change. An invalid replacement returns result `17`, item `nkey`.
{: .hl}


#### POST limits and save failures {.hl}

The local server requires Content-Length for a nonempty body and accepts up to 8,192 body bytes; transfer-encoding/chunked request bodies are not supported. The form parser accepts at most 128 ampersand-separated fields (including repeated/empty fields). A malformed escape or raw/decoded NUL returns result `18`, item `body`; excess fields return `17`, item `body: too many fields`; a form allocation refusal returns `48`.
{: .hl}

Overlong LAN request lines receive HTTP 414; with the default OTF buffer the maximum is 1,013 bytes before LF, including CR and the complete method/target/version line. Use POST for long saves. Incomplete/slow local headers can receive HTTP 408 under the shared 1.5-second header deadline; bodies have their own read deadline. Oversized bodies receive HTTP 413. These transport errors need not contain JSON.
{: .hl}

A settings-write failure returns result `64`, item `save`, rather than success. The firmware reloads durable/recoverable settings; read `/jo` back to determine the saved state. If a response is lost, a timeout does not establish whether a save happened. Do not automatically retry a write with GET after a POST timeout.
{: .hl}

The built-in page currently POSTs locally but uses GET through OTC because the production proxy's form-POST response fix has not yet been promoted. Forwarded GETs do not have the device's local request-line limit. Custom proxies must support forwarding POST bodies and replies before clients use POST through them. This compatibility workaround may be removed after production verification.
{: .hl}

#### Secret fields and cloud options

In addition to `name` and `mqtp`, `/co` limits `smtp`, `send`, `apwd`, `recp` and `ntp1` to 64 UTF-8 bytes (`17` if exceeded). Control characters in `name`, `smtp`, `send` or `recp` are refused with `18` to prevent email-header injection.
{: .hl}


For `auth`, `apwd`, `iftt` and `mqpw`, omitted or blank values keep the stored secret; nonempty values replace it. The `/jo` presence flags (`auth_set`, `apwd_set`, `iftt_set`) are read-only.

To erase a secret, send `clear_auth=1`, `clear_apwd=1`, `clear_iftt=1` or `clear_mqpw=1`. A clear flag other than `1`, or combining clearing and replacement, returns result `18`. Disable OTC before or when clearing its token.

Saved Blynk settings (`cld=1`) are inactive; `/co` accepts only `cld=0` (None) or `cld=2` (OTC), rejecting `cld=1` with result `17`. When OTC is enabled, every options save requires a stored or supplied OTC token of at least 32 characters; otherwise it returns result `17`, even for unrelated changes. <span class="hl">When changing from cloud-off to OTC, supply the token again, even if <code>auth_set=1</code>.</span>

<br>**Examples:**

* <span class="hl"><code>devip/co?dkey=xxx&nkey=MyGarageKey123&ckey=MyGarageKey123</code>: set a key meeting the new rules</span>
* `devip/co?dkey=xxx&dth=75`: set door's distance threshold to `75cm`
* <span class="hl"><code>devip/co?dkey=xxx&cld=2&auth=OT0123456789abcdef0123456789abcd&otcm=0&nossl=0</code>: select the default secure OTC service (replace the example token with yours; reboot afterward)</span>
* `devip/co?dkey=xxx&htp=8081&riv=2`: set HTTP port to `8081` (port `8080` is reserved for firmware updates), and set the status-check interval option to `2` seconds (the raw ultrasonic sampling interval is `dri`, in milliseconds)
* `devip/co?dkey=xxx&ati=5&ato=3`: set automation rule A time to `5` minutes and its option to `0b11` (i.e. auto-notify and auto-close if the door is left open for `>5 minutes`).
Duration-based automation uses elapsed uptime and continues without WiFi. Time-of-day automation waits for clock synchronization after boot, then runs at most once per UTC day. Manually opening the door during the configured hour skips that day's scheduled close.

#### POST example {.hl}

```sh
curl --data-urlencode 'dkey=YOUR_CURRENT_KEY' \
     --data-urlencode 'name=My Garage' \
     http://devip/co
```

Replace the placeholder key; curl encodes the fields and sets the form Content-Type and Content-Length. Check the JSON result, then read `/jo` back. HTTP on the LAN is unencrypted regardless of whether the cloud connection uses WSS.
{: .hl}

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

The JSON schema is unchanged, but `/jl` is now streamed **without Content-Length**. Local clients read until connection close; OTC clients let their proxy/HTTP library handle response framing. Do not require Content-Length or assume one read contains the entire JSON response. A transfer failure can leave incomplete JSON; reject it rather than treating it as a complete log.
{: .hl}

---

### 7. Clear Logs `/clearlog`

**Usage**: <code>http://devip/clearlog?**dkey**=xxx</code>

Clears the entire log data. <span class="hl">The device key is required locally and through OTC.</span>

---

### 8. Factory Reset `/resetall`

**Usage**: <code>http://devip/resetall?**dkey**=xxx</code>

Resets the controller back to factory default settings. WiFi will be restored to AP (Access Point) mode.

The device key is required locally and through OTC. A failed settings reset returns result `64`, item `save`. After a successful reset, first-use key enforcement and the 30-minute setup window apply again.
{: .hl}


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
|`OGTOPIC/OUT/STATE` | Published every 15 seconds to report the current state, including `OPEN`, `CLOSED`, `STOPPED`. |
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

The cloud connection type is defined by the [`cld` option](#jo_cld). OTC is the only supported cloud service. Local HTTP and MQTT remain available without cloud.

#### OTC (OpenThings Token)

OTC is based on our own [OpenThingsFramework (OTF) proxy](https://github.com/openthingsio). For token creation and device setup, see [Remote Access with OTC](../guides/remote-access-otc.md).

| Server mode | Secure (`nossl=0`) | Plain (`nossl=1`) |
|:------------|:-------------------|:-----------------|
| <span class="hl">Default (<code>otcm=0</code>)</span> | <span class="hl"><code>ws.openthings.io:443</code></span> | <span class="hl"><code>ws.cloud.openthings.io:80</code></span> |
| <span class="hl">Custom (<code>otcm=1</code>)</span> | <span class="hl">Configured <code>bdmn:bprt</code>, TLS enabled</span> | <span class="hl">Configured <code>bdmn:bprt</code>, TLS disabled</span> |

Default mode needs only the token; stored `bdmn`/`bprt` do not override its runtime endpoint. Custom mode uses the configured host/port. The port has no implied security meaning: only `nossl` selects WS or WSS. Save and reboot after changing cloud endpoint/security settings.
{: .hl}

Upgrade migration enables secure Default mode for a recognized old official default endpoint. Existing custom endpoints retain Custom/plain operation until explicitly changed. For legacy clients omitting `otcm`, editing `bdmn`/`bprt` infers Default only for a recognized legacy default host on port 80; other endpoints select Custom. A legacy change from Default to Custom also chooses plain WS unless `nossl` is explicitly supplied. New clients should always send `otcm` and `nossl` explicitly.
{: .hl}

Secure OTC waits for current time from NTP after each reboot and validates the server hostname and certificate chain. Configure a reachable local NTP server or permit Internet NTP. Custom secure servers must support TLS 1.2 and send intermediates to a root in the firmware catalog; private/self-signed CAs are unsupported. Older USERTrust-dependent chains are intentionally unsupported. Failure never triggers an automatic plain-WS fallback; `nossl=1` is an explicit opt-out that removes transport encryption.
{: .hl}

Secure cloud temporarily disconnects for email and reconnects afterward; plain WS stays connected. TLS/email work can pause local HTTP handling and physical-button polling. A cloud disconnect can interrupt an in-flight request; it is not automatically replayed. After a timeout, check device state before repeating an action.
{: .hl}

Once connected, you can use the same **OpenGarage API** (this document) via the OTC cloud. The base URL is `https://cloud.openthings.io/forward/v1/token`. For example:

* `https://cloud.openthings.io/forward/v1/token/` returns the controller homepage, where `token` is the 32-character OTC token.
* `https://cloud.openthings.io/forward/v1/token/jc` returns [controller variables](#2-get-controller-variables-jc).
* `https://cloud.openthings.io/forward/v1/token/jl` returns [log data](#6-get-logs-jl).
* <span class="hl">**Changed in 1.2.7:** the OTC token authorizes the forwarding path, but does **not** replace the device key. Append <code>?dkey=...</code> to protected GETs, or supply the key in the endpoint's documented body/header. <code>/jc</code>, <code>/jo</code>, <code>/jl</code> and <code>/db</code> remain keyless on the device; OTC still needs its forwarding token. Old token-only app entries need a way to store/send the device key and handle result <code>2</code>.</span>

---

### 11. Debug `/db`

**Usage:** `http://devip/db`

Returns compact debug information. AP mode returns only `dist`, `fwv` and `has_swrx`; station mode returns the other fields below instead of `dist`.

| Field | Meaning |
|:------|:--------|
| `fwv` | Firmware version, e.g. 127 for 1.2.7 |
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

#### Verbose diagnostics

Use `http://devip/db?verbose=1`. This remains keyless in AP and station modes. Compact replies retain the fields above; verbose fields may depend on hardware/protocol. Diagnostic strings are for support and are not stable enumerations. Existing verbose fields include:

| Field/group | Meaning |
|:------------|:--------|
| `active_security_protocol` | Selected protocol (`0`, `1`, `2`) |
| `free_heap`, `largest_free_heap_block`, `min_free_heap` | Current free heap, largest contiguous block, sampled minimum free heap, in bytes |
| `uptime_s`, `max_loop_us`, `min_free_cont_stack` | Extended uptime seconds, longest whole-loop interval in microseconds, untouched continuation-stack minimum in bytes; not a separate TLS stack reading |
| `build_time`, `otf_revision` | Build time and exact OTF source revision |
| `reset_reason`, `reset_reason_code` | SDK reset description/code |
| `time_valid` | Whether the local clock has synchronized |
| `secplus2_client_id`, `secplus2_identity_ready` | Stored protocol identity and readiness (when Sec+ 2.0 selected) |
| `secplus1_*`, `secplus2_*` | Protocol error/deferral/query counters and status ages; unavailable ages are `-1` |
| `mqtt_json_failures`, `mqtt_json_bytes`, `mqtt_retained_dropped` | MQTT serialization diagnostics and dropped retained-command count |
| `mqtt_buffer_ready` | <span class="hl">1 when the full MQTT buffer is ready; 0 also means not allocated while MQTT is off, not necessarily an allocation failure.</span> |
| `notification_dropped`, `email_failures`, `email_last` | Notification/email diagnostics; `email_last` is text, and SMTP acceptance does not prove inbox delivery |
| `wifi_recovery`, `wifi_attempts`, `wifi_timeouts`, `wifi_disconnects`, `wifi_retry_in_ms`, `wifi_disconnect_reason` | Connection recovery status/counters and retry delay |
| `wifi_selection`, `wifi_scans`, `wifi_roams`, `wifi_smoothed_rssi`, `wifi_channel` | AP-selection/roaming diagnostics |
| <span class="hl"><code>cpu_mhz</code></span> | <span class="hl">CPU clock speed; the integrated 1.2.7 candidate uses 160 MHz.</span> |
| <span class="hl"><code>wifi_phy</code></span> | <span class="hl">Configured radio PHY mode: <code>1:11b; 2:11g; 3:11n</code>. Not the negotiated packet rate. Can differ from saved <code>wfg</code> until reboot.</span> |
| <span class="hl"><code>config_status</code></span> | <span class="hl">Settings storage status: <code>0:normal; 1:fresh; 2:migrated; 3:recovered; 4:corrupt; 5:save failed</code>.</span> |
| <span class="hl"><code>config_locked</code></span> | <span class="hl">1 when unrecoverable settings damage has locked configuration/control; 0 otherwise. Inspect the setup/recovery flow locally.</span> |
| <span class="hl"><code>cloud_tls</code></span> | <span class="hl">Secure-cloud object described below.</span> |
| <span class="hl"><code>crash_count, crash_new, crash</code></span> | <span class="hl">Stored reset/crash sequence count, whether a record was stored this boot, and the selected record or <code>null</code>.</span> |
| <span class="hl"><code>stage_max_us</code></span> | <span class="hl">Object mapping loop-stage names to maximum observed stage duration in microseconds; not a sampled stack trace.</span> |

#### Secure-cloud diagnostics {.hl}

`cloud_tls` contains the following fields; `state` uses the same values as `/jc`'s `cloud_tls_state`. These fields describe transport/resource state, not authorization of a particular request.
{: .hl}

| Field | Meaning |
|:------|:--------|
| <span class="hl"><code>enabled, state</code></span> | <span class="hl">Secure mode selection and state code.</span> |
| <span class="hl"><code>allocation_refusals</code></span> | <span class="hl">Secure-client resource refusals since boot; not a count of all failed connections.</span> |
| <span class="hl"><code>last_ssl_error</code></span> | <span class="hl">Last recorded BearSSL error code. Zero alone does not establish success.</span> |
| <span class="hl"><code>stack_bytes, stack_peak</code></span> | <span class="hl">Configured TLS stack size and painted peak TLS-stack usage in bytes; the shared TLS stack can also be used by SMTP.</span> |
| <span class="hl"><code>iram_largest_block</code></span> | <span class="hl">Largest currently available IRAM heap block in bytes; the lifetime RX reservation is already excluded.</span> |
| <span class="hl"><code>catalog_version, catalog_hash</code></span> | <span class="hl">Installed root-catalog version/content hash. Identifies the store, not online revocation status or automatic freshness.</span> |

#### Stored crash records {.hl}

Use `/db?verbose=1&crash=0` for the newest record; indices `1`–`3` select older records from the four-slot history. An unavailable record is `null`. `crash_count` is the stored sequence count, not the number of currently retained slots. `crash_new=1` means this boot added a record, which can include reset information rather than a confirmed firmware fault.
{: .hl}

The record contains `index`, `seq`, `boot_reason`, `crash_reason`, `exccause`, hexadecimal `epc1`/`epc2`/`epc3`/`excvaddr`/`depc`/`sp`/`stack_end`, `uptime_s`, `stage`, `free_heap`, `fail_alloc_addr`, `fail_alloc_size`, and `trace` (space-separated candidate return addresses). Unknown recorded age/reason can be `-1`. Decode addresses using the exact firmware ELF; candidate return addresses are not proof that every named function was active. If the diagnostic JSON cannot be allocated, HTTP 503 with result `48` is returned.
{: .hl}

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

### 13. Firmware Upload

#### Manual upload

**Method:** `POST http://devip:8080/update` (local network only, in station or AP mode).

Send exactly one firmware file as a multipart form field named `file`. The key is checked in the `X-OG-Device-Key: hex:<encoded-key>` header before any body is read, where `<encoded-key>` is the lowercase hexadecimal encoding of the current device key's UTF-8 bytes. This is encoding, not encryption; use a trusted local network.

The browser's Firmware Update page handles this automatically and displays upload progress. Scripts that put `dkey` only in a multipart field must be updated. This contract applies to the firmware currently receiving the upload: the first upgrade from 1.2.5 uses its old uploader, with `dkey` in the multipart form or query string.

* **HTTP 401**, result `2`: missing/incorrect key; the body is not processed.

* **HTTP 409**, result `64`: device busy (warning, pending Security+ command/restart, within five seconds of a door action, or an update already running). The body is not processed; wait before retrying.

* **HTTP 200**, result `2`: the key failed the final authorization check.

* **HTTP 200**, result `64`: upload failed.

* **HTTP 200**, result `1`: upload succeeded; the device restarts.

Malformed or stalled requests may close without a JSON reply. A lost reply or timeout does not establish that installation failed; check the device before retrying. Check both HTTP status and JSON result, not an upload-progress value of 100%.

Use an uncompressed ESP8266 image with a `.bin` filename, at most 1,044,464 bytes; gzip images are not accepted. Sensing and automation pause during the upload.

Example using Bash, Python 3 and curl (enter the device's current key):

```sh
read -rsp 'Device key: ' OG_KEY; echo
OG_KEY_HEX=$(printf '%s' "$OG_KEY" | python3 -c 'import sys; print(sys.stdin.buffer.read().hex())')
curl --max-time 150 --config - --form 'file=@og_1.2.7.bin' http://devip:8080/update <<EOF
header = "X-OG-Device-Key: hex:$OG_KEY_HEX"
EOF
unset OG_KEY OG_KEY_HEX
```

Replace `devip` with the local IP; in AP mode use `192.168.4.1`. Firmware upload is not supported over OTC.

**First-use restriction:** manual (unsigned) upload is refused with HTTP `403`, result `48`, until the setup key has been saved (`kset=1`). Upgraded established units retain manual upload access, including those still using `opendoor`. A wrong/missing key still receives HTTP `401`. In AP mode, new updater work is refused in the final 150 seconds of the setup window (HTTP `403`, result `48`).
{: .hl}

#### Signed release preparation and upload

The built-in one-click updater downloads the signed release through the browser, then uploads it locally. It does not require the device to download firmware over its own TLS connection. All routes below are on port 8080 and are unavailable through OTC.

| Request | Contract |
|:--------|:---------|
| `GET /update/info` | Keyless product/target metadata: `product`, `target`, `version`, `base_url`, `file_extension`, `configured` |
| `GET /update/status` | Keyless status: `phase`, `message`, `bytes_done`, `bytes_total` |
| `POST /update/prepare` | Multipart upload containing the exact release descriptor, at most 1,024 bytes. Headers: `X-OG-Device-Key: hex:…`, `X-OG-Release-Id`, `X-OG-Signature` (hex-encoded DER signature), optional `X-OG-Allow-Downgrade: 1`. Success returns `{"result":1,"upload_token":"…"}`. |
| `POST /update/upload` | Multipart firmware file with header `X-OG-Upload-Token`. Uses the prepared descriptor to check the image size/checksum/target. The token expires after five minutes and is consumed when the upload starts. Prepare again after expiry or failure. |

A first-use unit (`kset=0`) accepts signed releases **1.2.7 or later**, even if `X-OG-Allow-Downgrade: 1` is supplied. Established units retain the 1.2.6 minimum. The minimum is checked against the signed descriptor after signature verification; a client-provided downgrade flag cannot bypass it.
{: .hl}

The updater now clears authorization headers and response framing at the start of every request. After a failed/cancelled upload, later requests must still supply their own key/token. This fixes the stale-key authorization problem in earlier firmware.
{: .hl}

---

### 14. Detect Security+ Protocol `/ad` {.hl}

**Usage:** `http://devip/ad?dkey=xxx`.
{: .hl}

Requires the current device key in station mode (including OTC) and on an established unit in setup mode. Only a first-use/factory-reset setup AP with `kset=0` is keyless. Wrong/missing keys return result `2` before probing or revealing busy state.
{: .hl}

Detection can operate the door, particularly when probing a Security+ 1.0 opener. Run it only when the area is clear. The request blocks for the probe, potentially around 12.5 seconds; do not automatically repeat it after a timeout.
{: .hl}

Success returns `{"version":0}`, `{"version":1}` or `{"version":2}` (`0`: no supported protocol found). Detection does not save `secv`; the configured protocol is restarted afterward, with status temporarily unknown until re-observed. Busy detection returns `{"result":48,"item":"busy"}`. Busy includes warning/door action, motion, pending commands, updates or restarts. Near AP expiry it returns result `48`, item `setup_timeout`.
{: .hl}

---

### 15. Setup AP APIs {.hl}

These routes are for the open setup network at `http://192.168.4.1/`; they are not the station `/cc` or `/co` interfaces. A first-use/factory-reset unit must establish a key before saving WiFi settings or completing setup. An established unit after a WiFi-only reset uses its existing key, including legacy weak/Unicode keys.
{: .hl}

| Request | Contract |
|:--------|:---------|
| <span class="hl"><code>GET /setup_status</code></span> | <span class="hl">Keyless <code>{"remaining_s":N}</code> countdown. Polling does not extend the absolute window.</span> |
| <span class="hl"><code>GET /js</code></span> | <span class="hl">Keyless cached scan result: <code>ssids</code> (SSID strings), <code>rssis</code> (corresponding RSSI strings), <code>has_swrx</code>, <code>secv</code>, <code>key_required</code> (<code>1</code>: existing key required). Treat SSIDs as text, never executable markup.</span> |
| <span class="hl"><code>POST /setup_key</code></span> | <span class="hl">Raw proposed key in the request body, not JSON or form fields. Accepts first-use keys satisfying the new rules; success saves the key/marker together. Once established, only an exact same-key retry returns success; a different key returns <code>2</code>. Invalid new keys return <code>17</code>, item <code>nkey</code>; save failure returns <code>64</code>, item <code>save</code>.</span> |
| <span class="hl"><code>GET /cc</code></span> | <span class="hl">Setup settings in the query: <code>dkey</code>, nonempty <code>ssid</code>, <code>pass</code> (WiFi password), <code>host</code>; optional <code>secv</code>, <code>cld=otc</code>/<code>none</code>, <code>auth</code>, <code>otcm</code>, <code>nossl</code>, <code>bdmn</code>, <code>bprt</code>. Cloud selector here uses strings, unlike station <code>/co</code>. Success returns result <code>1</code> and starts the connection attempt; it does not prove association.</span> |
| <span class="hl"><code>GET /jt?dkey=…</code></span> | <span class="hl">Poll connection/completion status. Returns <code>{"ip":0}</code> until connected, otherwise an integer IPv4 address. A successful completion saves station mode and schedules restart. Requires the saved key; it is not a read-only authorization exception.</span> |
| <span class="hl"><code>GET /ad?dkey=…</code></span> | <span class="hl">Same detection contract as above; only first-use AP detection is keyless.</span> |

Percent-encode every query value. The setup AP is unencrypted: WiFi credentials and the device key can be observed by someone capturing that traffic. `/setup_key` is not an established-unit key-change endpoint; use authenticated station `/co` for changing an existing key.
{: .hl}

Before a first key is saved, setup `/cc` and `/jt` return result `48`, item `key`. Established units require the correct `dkey` for those requests and `/ad`; a wrong key returns `2`. If someone has already set a different first key, enter that key or physically factory-reset the unit before trying again.
{: .hl}

The AP has an absolute **30-minute** window. New key saves, WiFi settings/connection checks and Detect are refused in its final 30 seconds (`48`, item `setup_timeout`). New firmware updates are refused in the final 150 seconds. Expiry turns WiFi off when the main loop next runs; it does not save, reboot or return an established unit to station mode. A successful setup that already scheduled its restart still finishes normally. Hold the button about five seconds and release before the factory-reset threshold to reopen setup. Existing keys/settings survive that WiFi-only reset.
{: .hl}

Firmware updates and `/resetall` on the setup network still require the device key; first-use updater restrictions above also apply. First-use keyless Detect does not make those routes keyless.
{: .hl}

---

### 16. Device-served UI and Caching {.hl}

In station mode, `GET /`, `GET /vo` and `GET /vl` serve the same self-contained, gzip-compressed UI; the initial path selects Home, Options or Log. Switching views in an open page does not download another full page. The UI still retrieves current data from the JSON APIs.
{: .hl}

These page responses include an ETag derived from the content and `Cache-Control: private, max-age=300`. A matching `If-None-Match` returns HTTP 304 with no body, locally or through OTC. `/jc` includes the UI content ID as `ui`, allowing an open page to notice a changed UI after firmware replacement. Page caching does not authorize a control request or change the key requirements above. Setup and firmware-update pages use their own routes and are not covered by this station-page caching contract.
{: .hl}
