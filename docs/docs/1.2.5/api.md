## Firmware 1.2.5 API Reference

###1. Overview
* Existing endpoints that require a device key use `dkey=xxx`; the new regeneration endpoint uses a request body instead. At factory reset, the **default device key** is `opendoor`.
* The device's IP address is referred to as `devip`. For most commands, parameters are optional and the order of parameters does not matter.
* Return values are all formatted in JSON, for example: `{"result":1}`. Below is the list of error codes:

|Code|Meaning|
|:---|:------|
|`1` |Success|
|`2` |Unauthorized (e.g. missing device key or device key is incorrect)|
|`3` |Mismatch (e.g. new device key and confirmation key do not match)|
|`16`|Data Missing (e.g. missing required parameters)|
|`17`|Out of Range (e.g. value exceeds the acceptable range)|
|`18`|Data Format Error (e.g. provided data does not match required format)|
|`32`|Page Not Found (e.g. page not found or requested file missing)|
|`48`|Not Permitted (e.g. cannot operate on the requested station)|
|`64`|Upload failed (e.g. OTA firmware update failed)|

<br>
#### Changes from 1.2.4

* <code>/jc</code>: adds status-validity fields and protocol-specific status/fault information. Clients should check validity before using retained light, lock or obstruction values.
* <code>/db?verbose=1</code>: adds device-health, Security+ identity and communication diagnostics.
* <code>POST /secplus2/regenerate-id</code>: new endpoint to regenerate the Security+ 2.0 client ID and restart, preserving other settings.

<span class="hl">New or modified APIs in firmware 1.2.5 are highlighted in green.</span>

---

###2. Get Controller Variables `/jc`
**Usage**: `http://devip/jc`

|Variable |Explanation |
|:--------|:---------- |
|`dist`   |Distance sensor value (unit: cm)|
|`sn2`    |Switch sensor value (present only if switch sensor is enabled)|
|<a id="jc_door"></a>`door`   |Door status (`0:closed; 1:open; 2:stopped (partially open); 3:closing; 4:opening; 5:unknown`)|
|`vehicle`|Vehicle status (`0:vehicle not detected; 1:detected; 2:unknown`)|
|`rcnt`   |Read count (increments every time sensor values are updated)|
|`fwv`    |Firmware version|
|`name`   |Device name|
|`mac`    |MAC address|
|`cid`    |WiFi chip ID|
|`rssi`   |WiFi signal strength (dBm)|
|`cld`    |Cloud option (`0:none; 1:Blynk; 2:OTC`)|
|`clds`   |Cloud connection status<br>• For Blynk: `0:disconnected; 1:connected` <br>• For OTC: `0:not enabled; 1:connecting; 2:disconnected; 3:connected`|
|`temp`   |temperature reading (Celsius), only if T/H sensor is enabled|
|`humid`  |humidity reading (relative percentage), only if T/H sensor is enabled|
|`secv`   |Security+ version (`2:v2.0; 1:v1.0; 0:None`)|
|`has_swrx`|Support for software RX (required for native support of Security+ 2.0/1.0)|
|`light`  |Light status (`0:off; 1:on`)|
|`lock`   |Remote lock status|
|`obstruct`|Obstruction sensor status|
|`nopenings`|Number of times the door has opened (available only if `secv=2`)|
|`pemu`   |Panel emulator status (`0:inactive; 1:active; 2:detecting`, available only if `secv=1`)|
|<span class="hl"><code>door_valid</code></span>|<span class="hl">Door status validity: 0 unknown/stale, 1 valid (Security+ 1.0/2.0).</span>|
|<span class="hl"><code>light_lock_valid</code></span>|<span class="hl">Validity of light and remote-lock status: 0 unknown/stale, 1 valid (Security+ 1.0/2.0).</span>|
|<span class="hl"><code>obstruct_valid</code></span>|<span class="hl">Obstruction status validity: 0 unknown/stale, 1 valid (Security+ 1.0/2.0).</span>|
|<span class="hl"><code>panel_status</code></span>|<span class="hl">Panel detection/emulation description (Security+ 1.0 only).</span>|
|<span class="hl"><code>command_status</code></span>|<span class="hl">Command progress/outcome description (Security+ 1.0 only).</span>|
|<span class="hl"><code>recovery_status</code></span>|<span class="hl">Communication recovery description (Security+ 2.0 only); not a transmission acknowledgement.</span>|
|<span class="hl"><code>control_fault</code></span>|<span class="hl">0 no latched fault; 1 uncertain actuation failure has inhibited controls (Security+ 2.0 only).</span>|

<br><span class="hl">These new fields are present only on Security+-capable hardware with the corresponding protocol selected. Expired door status uses <code>door=5</code>; light, lock and obstruction retain their last-known Boolean values for compatibility. Check the validity fields before treating them as current. Diagnostic text is not a stable enumeration.</span>

---

###3. Change Controller Variables `/cc`

**Usage**: `http://devip/cc?dkey=xxx&click=1`

Send one action per request; supported parameters are listed below.

|Parameters |Meaning |
|:----------|:---------- |
|`dkey`     |Device key (at factory reset it is `opendoor`)|
|`click`    |Toggle door|
|`close`    |Trigger door close action|
|`open`     |Trigger door open action|
|`reboot`   |Reboot the controller|
|`apmode`   |Reset the controller to WiFi AP mode (for reconfiguring WiFi setting)|
|`light`    |Toggle light. Supported only for Security+ 2.0/1.0. The only accepted value currently is `toggle`.|
|`lock`     |Toggle remote lock. Supported only for Security+ 2.0/1.0 The only accepted value currently is `toggle`.|

<br>**Examples:**

* `http://devip/cc?dkey=xxx&click=1`: toggle door
* `http://devip/cc?dkey=xxx&close=1`: request door close
* `http://devip/cc?dkey=xxx&reboot=1`: reboot device
* `http://devip/cc?dkey=xxx&light=toggle`: toggle light

---

###4. Get Options `/jo`
**Usage**: `http://devip/jo`

<u>Underlined</u> values indicate factory default values.

|Variable |Explanation|
|:---------|:------------|
| `fwv` | Firmware version (read-only) |
| `secv` | Security+ version (`0:None; 1:1.0; 2:2.0`); native Security+ requires v2.3+ hardware |
|<span class="hl"><code>pem</code></span>|<span class="hl">Security+ 1.0 panel emulation (<code>0:Auto</code>, default; <code>1:Disable</code>). Only configurable on v2.3+ hardware with Security+ 1.0 selected.</span>|
|<span class="hl"><code>secplus2_client_id</code></span>|<span class="hl">Read-only Security+ 2.0 client ID (hexadecimal), or <code>Unavailable</code> if identity initialization failed.</span>|
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
| `htp` | HTTP port (default is `80`) |
| `cdt` | Button click time (unit: `ms`, default is `1000`) |
| `dri` | Distance reading interval (unit: `ms`, default is `500`) |
| `sfi` | Sensor filtering method (<code>0:median; <u>1:consensus</u></code>). <span class="hl">Consensus now averages the tightest five of seven readings if their spread meets <code>cmr</code>; otherwise the last accepted distance is retained.</span> |
| `cmr` | Consensus margin for the consensus method (unit: `cm`, default is `10`) |
| `sto` | Sensor timeout handling option (<code><u>0:ignore</u>; 1:cap to maximum value</code>) |
| `ati` | Automation rule A time (unit: `minutes`): detect if the door is left open for longer than `ati` minutes |
| `ato` | Automation rule A option (`bit 0:notify; bit 1:auto-close`) |
| `atib` | Automation rule B time (unit: `UTC hour`, detect if the door is left open after hour `atib:00 UTC`) |
| `atob` | Automation rule B option (`bit 0:notify; bit 1:auto-close`) |
| `noto` | Choose notification events (`bit 0:door OPEN event; bit 1:door CLOSED event; bit 2:door STOPPED event`) |
| `usi` | Use static IP (<code><u>0:use DHCP</u>; 1:use static IP</code>). |
| `dvip`,<br>`gwip`,<br>`subn`,<br>`dns1` | Customizing device IP, gateway IP, subnet mask, and DNS IP in static IP mode. Effective only when `usi=1`|
|<a id="jo_cld"></a>`cld` | Cloud connection type (<code><u>0:none</u>; 1:Blynk; 2:OTC</code>) |
| `auth` | Cloud authorization token |
| `bdmn` | Cloud server domain name (refer to [cloud connection](#10-cloud-connection) for details) |
| `bprt` | Cloud server port (refer to [cloud connection](#10-cloud-connection) for details) |
| `name` | Device name (default is `My OpenGarage`) |
| `iftt` | IFTTT Maker channel token |
| `ssid` | SSID of the WiFi network this device is connected to |
| `mqen` | MQTT enable (<code><u>0:disabled</u>; 1:enabled</code>). Note: this option preserves other mqtt option values when temporarily disabling MQTT. |
| `mqtt` | MQTT server url (IP or domain name both allowed) |
| `mqpt` | MQTT port (default is `1883`) |
| `mqur` | MQTT server user name (if authentication is required) |
| `mqpw` | MQTT password (this variable is NOT shown in the result of `/jo`, but you can change it using the `/co` command in the [section below](#5-change-options-co).) |
| `mqtp` | MQTT topic (optional, if left empty it will use the Device name as the topic) |
| `emen` | Email enable (<code><u>0:disabled</u>; 1:enabled</code>) |
| `smtp` | SMTP server name (default is `smtp.gmail.com`) |
| `sprt` | SMTP server port (default is `465`) |
| `send` | Email sender (for authentication with the SMTP server) |
| `apwd` | Email app password (for authentication with the SMTP server) |
| `recp` | Recipient email (for receiving notification, may or may not be the same as the sender) |
| `ntp1` | NTP server url (optional, if customizing NTP server) |
| `host` | Custom Host name (used for the mDNS feature). For example, if `host=my_og`, you can access the device via its mDNS name `my_og.local/`) |

---

###5. Change Options `/co`

**Usage**: <code>http://devip/co?**dkey**=xxx&**nkey**=xxx&**ckey**=xxx&**opn**=opv...</code>

<span class="hl">Set <code>pem=0</code> for automatic Security+ 1.0 panel detection/emulation or <code>pem=1</code> to disable emulation. The device accepts this option only on Security+-capable hardware when Security+ 1.0 is selected.</span>

* `opn` is a writable option name from the table above, and `opv` is its new value.
* `fwv` and `ssid` cannot be changed through `/co`; use WiFi setup to change the network.
* The device key `dkey` may be changed only by providing both `nkey` (new key) and `ckey` (confirm key) and they must match exactly.

<br>**Examples:**

* `devip/co?dkey=xxx&nkey=abc&ckey=abc`: set device key to `abc`
* `devip/co?dkey=xxx&dth=75`: set door's distance threshold to `75cm`
* `devip/co?dkey=xxx&auth=0123456789abcdef`: set the cloud authentication token
* `devip/co?dkey=xxx&htp=8080&riv=2`: set HTTP port to `8080`, and set the status-check interval option to `2` seconds (the raw ultrasonic sampling interval is `dri`, in milliseconds)
* `devip/co?dkey=xxx&ati=5&ato=3`: set automation rule A time to `5` minutes and its option to `0b11` (i.e. auto-notify and auto-close if the door is left open for `>5 minutes`).
---

###6. Get Logs `/jl`
**Usage**: `http://devip/jl`

| Variable | Explanation |
|:---------|:------------|
| `name`   | Device name |
| `time`   | Device time (UTC epoch time) |
| `starttime`| Time when the device is powered on (UTC epoch time) |
| `ncols`  | Number of columns (`3` or `4` depending on if `sn2_value` is present in the log data)|
| `logs`   | Log data: an array of log entries, each in the format of `[time_stamp, door_status, distance_value, sn2_value]`. Note that `sn2_value` is only available if `sn2` is enabled.|

---

###7. Clear Logs `/clearlog`

**Usage**: <code>http://devip/clearlog?**dkey**=xxx</code>

Clears the entire log data.

---

###8. Factory Reset `/resetall`

**Usage**: <code>http://devip/resetall?**dkey**=xxx</code>

Resets the controller back to factory default settings. WiFi will be restored to AP (Access Point) mode.

<span class="hl">Factory reset also generates a new Security+ 2.0 client ID on the next setup. Reboot, OTA update and WiFi-only reset preserve the identity.

---

###9. MQTT

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
|`OGTOPIC/OUT/JSON`  | Published on door-state events and periodically (approximately every 15 seconds), with the same controller variables as [`/jc`](#2-get-controller-variables-jc). <span class="hl">Includes the new Security+ validity and diagnostic fields; use this JSON rather than the limited text state to identify Unknown.</span> |

**Subscribed Message**:

The controller subscribes to topic `OGTOPIC/IN/STATE` and accepts the following payload messages:

* `click`: Toggle door
* `close`: Trigger door close action
* `open` : Trigger door open action
* `togglelight`: Toggle light. Supported only for Security+ 2.0/1.0
* `togglelock`: Toggle remote lock. Supported only for Security+ 2.0/1.0

---

###10. Cloud Connection

The cloud connection type is defined by the [`cld` option](#jo_cld). Two types are supported: Blynk and OTC.

####Blynk
As the Blynk team officially ended support for their legacy app and server, we provide a replicated Blynk server at [`openthings.io`](https://openthings.io). This allows existing users of the legacy app to continue connecting.

* Default server domain name (`bdmn`): `blynk.openthings.io`
* Default server port (`bprt`): `8080`. Change these accordingly if using your own Blynk server.

Once connected, you can use the **Blynk Legacy HTTP API** to retrieve data or trigger garage door actions. The base URL is:

* Secure (HTTPS): `https://blynk.openthings.io:9443/token`
* Non-secure (HTTP): `https://blynk.openthings.io:8080/token`

For example:

* `https://blynk.openthings.io:9443/token/project` returns the entire project in JSON, where `token` is the 32-character authorization token.
* `https://blynk.openthings.io:9443/token/get/V?` returns the value of pin `V?`, where:
    * `V0`: binary door indicator
    * `V2`: complete door status (see [`door` variable](#jc_door))
    * `V3`: distance sensor value
    * `V4`: vehicle status
    * `V6`: temperature sensor value
    * `V7`: humidity sensor value
    * `V8`: opener light state/control
    * `V9`: remote lock state/control
* `https://blynk.openthings.io:9443/token/update/V1?value=1` updates pin `V1` with value `1`, triggering a door click.

<span class="hl">Blynk light/lock state-setting requests are ignored while the corresponding opener status is invalid. Existing Blynk indicators may still show last-known values; use <code>/jc</code> for validity information.</span>

####OTC (OpenThings Token)

OTC is based on our own [OpenThingsFramework (OTF) proxy](https://github.com/openthingsio).

* Default server domain name (`bdmn`) is `ws.cloud.openthings.io`
* Default server port (`bprt`) is: `80`. Change these accordingly if using your own OTC server.

Once connected, you can use the same **OpenGarage API** (this document) via the OTC cloud. The base URL is `https://cloud.openthings.io/forward/v1/token`. For example:

* `https://cloud.openthings.io/forward/v1/token/` returns the controller homepage, where `token` is the 32-character OTC token.
* `https://cloud.openthings.io/forward/v1/token/jc` returns [controller variables](#2-get-controller-variables-jc).
* `https://cloud.openthings.io/forward/v1/token/jl` returns [log data](#6-get-logs-jl).
* <u>**NOTE**</u>: Existing endpoints use the OTC token in place of the device key (`dkey`). <span class="hl">The new <code>POST /secplus2/regenerate-id</code> endpoint is an exception: it requires the current device key in the request body even over OTC.</span>

---

### 11. Debug `/db` {.hl}

**Usage:** `http://devip/db`

Returns the compact debug information.

| Field | Meaning |
|:------|:--------|
| `fwv` | Firmware version, e.g. 125 for 1.2.5 |
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

### 12. Regenerate Sec+ 2.0 Client ID {.hl}

**Method:** `POST /secplus2/regenerate-id`

Send the current device key as the **raw UTF-8 request body**, not a URL parameter or JSON object. This endpoint requires the device key even over OTC.

The device must have Security+ 2.0 saved, be connected, and have no pending warning, command or release cleanup. This is a maintenance operation: it saves a new identity and restarts OG, preserving WiFi and other settings.

- Success: `{"result":1,"client_id":"0x…","message":"…"}`.
- Incorrect device key: `{"result":2}`.
- Wrong mode, busy device or storage failure: `{"result":0,"message":"…"}`.

After storage, OG automatically restarts and uses the new key.
