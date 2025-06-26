
/**
 * @typedef {Object} OpenGarageConfig
* @property {string} fwv - Firmware version (read-only)
* @property {0 | 1} sn1 - Sensor 1 mounting type (0: ceiling mount, 1: side mount)
* @property {0 | 1 | 2} sn2 - Sensor 2 type (0: none, 1: normally closed, 2: normally open)
* @property {0 | 1 | 2 | 3} sno - Sensor logic (0: sn1 only, 1: sn2 only, 2: both, 3: either)
* @property {number} dth - Door distance threshold in cm
* @property {number} vth - Vehicle distance threshold in cm
* @property {number} riv - Status report interval in seconds (default: 5)
* @property {0 | 1 | 2} alm - Alarm type (0: none, 1: 5s, 2: 10s)
* @property {0 | 1} aoo - Alarm on opening (0: no, 1: yes)
* @property {number} lsz - Log size (e.g. 50 records)
* @property {0 | 1 | 2 | 3 | 4} tsn - Temp/humidity sensor type (0: none, 1: AM2320, etc.)
* @property {number} htp - HTTP port (default: 80)
* @property {number} cdt - Click delay time in ms (default: 1000)
* @property {number} dri - Distance read interval in ms (default: 500)
* @property {0 | 1} sfi - Sensor filtering (0: median, 1: consensus)
* @property {number} cmr - Consensus margin in cm (default: 10)
* @property {0 | 1} sto - Sensor timeout (0: ignore, 1: cap to max)
* @property {number} ati - Automation A: open time limit in minutes
* @property {number} ato - Automation A: bit 0 notify, bit 1 auto-close
* @property {number} atib - Automation B: UTC hour
* @property {number} atob - Automation B: bit 0 notify, bit 1 auto-close
* @property {number} noto - Notification options (bit 0: open, bit 1: close)
* @property {0 | 1} usi - Use static IP (0: DHCP, 1: Static)
* @property {string} [dvip] - Device IP (if usi=1)
* @property {string} [gwip] - Gateway IP (if usi=1)
* @property {string} [subn] - Subnet mask (if usi=1)
* @property {string} [dns1] - DNS server IP (if usi=1)
* @property {0 | 1 | 2} cld - Cloud connection type (0: none, 1: Blynk, 2: OTC)
* @property {string} auth - Cloud auth token
* @property {string} bdmn - Cloud domain
* @property {number} bprt - Cloud port
* @property {string} name - Device name
* @property {string} iftt - IFTTT maker token
* @property {string} ssid - Connected WiFi SSID
* @property {0 | 1} mqen - MQTT enabled (1: yes)
* @property {string} mqtt - MQTT broker address
* @property {number} mqpt - MQTT port
* @property {string} [mqur] - MQTT username (optional)
* @property {string} [mqpw] - MQTT password (not sent over API)
* @property {string} [mqtp] - MQTT topic (default: device name)
* @property {boolean} emen - Email enabled
* @property {string} smtp - SMTP server
* @property {number} sprt - SMTP port
* @property {string} send - Email sender address
* @property {string} apwd - App password
* @property {string} recp - Email recipient
* @property {string} [ntp1] - NTP server (optional)
* @property {string} host - mDNS hostname (e.g. 'myog' = myog.local)
 */

/**
 * @typedef {Object} OpenGarageUpdate
* @property {0 | 1} sn1 - Sensor 1 mounting type (0: ceiling mount, 1: side mount)
* @property {0 | 1 | 2} sn2 - Sensor 2 type (0: none, 1: normally closed, 2: normally open)
* @property {0 | 1 | 2 | 3} sno - Sensor logic (0: sn1 only, 1: sn2 only, 2: both, 3: either)
* @property {number} dth - Door distance threshold in cm
* @property {number} vth - Vehicle distance threshold in cm
* @property {number} riv - Status report interval in seconds (default: 5)
* @property {0 | 1 | 2} alm - Alarm type (0: none, 1: 5s, 2: 10s)
* @property {0 | 1} aoo - Alarm on opening (0: no, 1: yes)
* @property {number} lsz - Log size (e.g. 50 records)
* @property {0 | 1 | 2 | 3 | 4} tsn - Temp/humidity sensor type (0: none, 1: AM2320, etc.)
* @property {number} htp - HTTP port (default: 80)
* @property {number} cdt - Click delay time in ms (default: 1000)
* @property {number} dri - Distance read interval in ms (default: 500)
* @property {0 | 1} sfi - Sensor filtering (0: median, 1: consensus)
* @property {number} cmr - Consensus margin in cm (default: 10)
* @property {0 | 1} sto - Sensor timeout (0: ignore, 1: cap to max)
* @property {number} ati - Automation A: open time limit in minutes
* @property {number} ato - Automation A: bit 0 notify, bit 1 auto-close
* @property {number} atib - Automation B: UTC hour
* @property {number} atob - Automation B: bit 0 notify, bit 1 auto-close
* @property {number} noto - Notification options (bit 0: open, bit 1: close)
* @property {0 | 1} usi - Use static IP (0: DHCP, 1: Static)
* @property {string} [dvip] - Device IP (if usi=1)
* @property {string} [gwip] - Gateway IP (if usi=1)
* @property {string} [subn] - Subnet mask (if usi=1)
* @property {string} [dns1] - DNS server IP (if usi=1)
* @property {0 | 1 | 2} cld - Cloud connection type (0: none, 1: Blynk, 2: OTC)
* @property {string} auth - Cloud auth token
* @property {string} bdmn - Cloud domain
* @property {number} bprt - Cloud port
* @property {string} name - Device name
* @property {string} iftt - IFTTT maker token
* @property {string} ssid - Connected WiFi SSID
* @property {0 | 1} mqen - MQTT enabled (1: yes)
* @property {string} mqtt - MQTT broker address
* @property {number} mqpt - MQTT port
* @property {string} [mqur] - MQTT username (optional)
* @property {string} [mqpw] - MQTT password (not sent over API)
* @property {string} [mqtp] - MQTT topic (default: device name)
* @property {boolean} emen - Email enabled
* @property {string} smtp - SMTP server
* @property {number} sprt - SMTP port
* @property {string} send - Email sender address
* @property {string} apwd - App password
* @property {string} recp - Email recipient
* @property {string} [ntp1] - NTP server (optional)
* @property {string} host - mDNS hostname (e.g. 'myog' = myog.local)
* @property {string} nkey - New device key
* @property {string} ckey - Confirm new device key
 */

import { initalize, garageFetch, sendCommand, updateTitle, updateVersion } from "./common.js";
// initalize(new URL(location.href + "/..").href);
// TODO;
initalize(new URL("http://192.168.12.130/vo" + "/..").href);
const nameInput = document.getElementById("name-input");
const sensorMount = document.getElementById("sensor-mount");
const secplusVersion = document.getElementById("secplus-select");
const doorThreshold = document.getElementById("door-threshold");
const carThreshold = document.getElementById("car-threshold");
const statusCheck = document.getElementById("status-check");
const clickTime = document.getElementById("click-time");
const switchSensor = document.getElementById("switch-sensor");
const sensorLogic = document.getElementById("sensor-logic");
const thSensor = document.getElementById("th-sensor");
const soundAlarm = document.getElementById("alarm-time");
const openAlarm = document.getElementById("alarm-open");
const logSize = document.getElementById("log-size");

const enableCloud = document.getElementById("enable-cloud");
const cloudTypeBlynk = document.getElementById("cloud-type-blynk");
const cloudTypeOTC = document.getElementById("cloud-type-otc");
const cloudToken = document.getElementById("cloud-token");
const cloudServer = document.getElementById("cloud-server");
const cloudPort = document.getElementById("cloud-port");
const enableMqtt = document.getElementById("enable-mqtt");
const mqttServer = document.getElementById("mqtt-server");
const mqttPort = document.getElementById("mqtt-port");
const mqttUsername = document.getElementById("mqtt-username");
const mqttPassword = document.getElementById("mqtt-password");
const mqttTopic = document.getElementById("mqtt-topic");
const enableEmail = document.getElementById("enable-email");
const smtpServer = document.getElementById("smtp-server");
const smtpPort = document.getElementById("smtp-port");
const smtpSender = document.getElementById("smtp-sender-email");
const smtpPassword = document.getElementById("smtp-password");
const smtpRecipient = document.getElementById("smtp-recipient-email");
const notifyOnDoorOpen = document.getElementById("notification-door-open");
const notifyOnDoorClose = document.getElementById("notification-door-close");
const iftttKey = document.getElementById("ifttt-key");
const openLongerTime = document.getElementById("open-longer-time");
const openLongerNotification = document.getElementById("open-longer-notification");
const openLongerClose = document.getElementById("open-longer-close");
const openAfterTime = document.getElementById("open-after-time");
const openAfterNotification = document.getElementById("open-after-notification");
const openAfterClose = document.getElementById("open-after-close");

const readInterval = document.getElementById("read-interval");
const sensorFilter = document.getElementById("sensor-filter");
const consensusMarginLabel = document.getElementById("consensus-margin-label");
const consensusMargin = document.getElementById("consensus-margin");
const distanceTimeout = document.getElementById("distance-timeout");
const httpPort = document.getElementById("http-port");
const hostname = document.getElementById("hostname");
const useCustomNTP = document.getElementById("use-custom-ntp");
const ntpServer = document.getElementById("ntp-server");
const useStaticIP = document.getElementById("use-static-ip");
const deviceIP = document.getElementById("device-ip");
const gatewayIP = document.getElementById("gateway-ip");
const subnet = document.getElementById("subnet");
const dns = document.getElementById("dns");
const changeDeviceKey = document.getElementById("change-device-key");
const newKey = document.getElementById("new-key");
const confirmNewKey = document.getElementById("confirm-new-key");


switchSensor.addEventListener("input", () => {
    sensorLogic.disabled = switchSensor.value == 0;
});

sensorFilter.addEventListener("input", () => {
    consensusMarginLabel.hidden = sensorFilter.value == 0;
});

const keyInput = document.getElementById("key-input");
keyInput.value = localStorage.getItem("keyInput");
keyInput.addEventListener("input", () => {
    localStorage.setItem("keyInput", keyInput.value);
});

async function updateData() {
    const res = await garageFetch("jo");
    /** @type {OpenGarageConfig} */
    const data = await res.json();

    updateTitle(data.name);
    updateVersion(data.fwv);

    nameInput.disabled = false;
    nameInput.value = data.name;

    sensorMount.disabled = false;
    sensorMount.value = data.sn1;

    secplusVersion.disabled = false;
    secplusVersion.value = data.secv;

    doorThreshold.disabled = false;
    doorThreshold.value = data.dth;

    carThreshold.disabled = false;
    carThreshold.value = data.vth;

    statusCheck.disabled = false;
    statusCheck.value = data.riv;

    clickTime.disabled = false;
    clickTime.value = data.cdt;

    switchSensor.disabled = false;
    switchSensor.value = data.sn2;

    sensorLogic.disabled = switchSensor.value == 0;
    sensorLogic.value = data.sno;

    thSensor.disabled = false;
    thSensor.value = data.tsn;

    soundAlarm.disabled = false;
    soundAlarm.value = data.alm;

    openAlarm.disabled = false;
    openAlarm.value = data.aoo;

    logSize.disabled = false;
    logSize.value = data.lsz;

    enableCloud.disabled = false;
    enableCloud.value = data.cld == 0;

    cloudTypeBlynk.disabled = false;
    cloudTypeBlynk.value = data.cld == 1;

    cloudTypeOTC.disabled = false;
    cloudTypeOTC.value = data.cld == 2;

    cloudToken.disabled = false;
    cloudToken.value = data.auth;

    cloudServer.disabled = false;
    cloudServer.value = data.bdmn;

    cloudPort.disabled = false;
    cloudPort.value = data.bprt;

    enableMqtt.disabled = false;
    enableMqtt.value = data.mqen;

    mqttServer.disabled = false;
    mqttServer.value = data.mqtt;

    mqttPort.disabled = false;
    mqttPort.value = data.mqpt;

    mqttUsername.disabled = false;
    mqttUsername.value = data.mqur || "";

    mqttPassword.disabled = false;
    mqttPassword.value = "";

    mqttTopic.disabled = false;
    mqttTopic.value = data.mqtp || "";

    enableEmail.disabled = false;
    enableEmail.value = data.emen;

    smtpServer.disabled = false;
    smtpServer.value = data.smtp;

    smtpPort.disabled = false;
    smtpPort.value = data.sprt;

    smtpSender.disabled = false;
    smtpSender.value = data.send;

    smtpPassword.disabled = false;
    smtpPassword.value = data.apwd;

    smtpRecipient.disabled = false;
    smtpRecipient.value = data.recp;

    notifyOnDoorOpen.disabled = false;
    notifyOnDoorOpen.value = (data.noto & 1) == 1;

    notifyOnDoorClose.disabled = false;
    notifyOnDoorClose.value = (data.noto & 2) == 1;

    iftttKey.disabled = false;
    iftttKey.value = data.iftt;

    openLongerTime.disabled = false;
    openLongerTime.value = data.ati;

    openLongerNotification.disabled = false;
    openLongerNotification.value = (data.ato & 1) == 1;

    openLongerClose.disabled = false;
    openLongerClose.value = (data.ato & 2) == 1;

    openAfterTime.disabled = false;
    openAfterTime.value = data.atib;

    openAfterNotification.disabled = false;
    openAfterNotification.value = (data.atob & 1) == 1;

    openAfterClose.disabled = false;
    openAfterClose.value = (data.atob & 2) == 1;

    readInterval.disabled = false;
    readInterval.value = data.dri;

    sensorFilter.disabled = false;
    sensorFilter.value = data.sfi;

    consensusMarginLabel.hidden = sensorFilter.value == 0;

    consensusMargin.disabled = false;
    consensusMargin.value = data.cmr;

    distanceTimeout.disabled = false;
    distanceTimeout.value = data.sto;

    httpPort.disabled = false;
    httpPort.value = data.htp;

    hostname.disabled = false;
    hostname.value = data.host;

    useCustomNTP.disabled = false;
    useCustomNTP.value = data.ntp1 != "";

    ntpServer.disabled = false;
    ntpServer.value = data.ntp1;

    useStaticIP.disabled = false;
    useStaticIP.value = data.usi;

    deviceIP.disabled = false;
    deviceIP.value = data.dvip || "";

    gatewayIP.disabled = false;
    gatewayIP.value = data.gwip || "";

    subnet.disabled = false;
    subnet.value = data.subn || "";

    subnet.disabled = false;
    subnet.value = data.subn || "";

    dns.disabled = false;
    dns.value = data.dns1 || "";

    changeDeviceKey.disabled = false;
    changeDeviceKey.value = false;

    newKey.disabled = false;
    newKey.value = "";

    confirmNewKey.disabled = false;
    confirmNewKey.value = "";
}

updateData();

async function updateSettings() {
    /** @type {OpenGarageUpdate} */
    const data = {};
    data.name = nameInput.value;
    data.sn1 = sensorMount.value;
    data.secv = secplusVersion.value;
    data.dth = doorThreshold.value;
    data.vth = carThreshold.value;
    data.riv = statusCheck.value;
    data.cdt = clickTime.value;
    data.sn2 = switchSensor.value;
    data.sno = sensorLogic.value;
    data.tsn = thSensor.value;
    data.alm = soundAlarm.value;
    data.aoo = openAlarm.value;
    data.lsz = logSize.value;
    data.cld = enableCloud.checked ? 0 : cloudTypeBlynk.chcked ? 1 : 2;
    data.auth = cloudToken.value;
    data.bdmn = cloudServer.value;
    data.bprt = cloudPort.value;
    data.mqen = enableMqtt.value;
    data.mqtt = mqttServer.value;
    data.mqpt = mqttPort.value;
    data.mqur = mqttUsername.value;
    if (mqttPassword != "") {
        data.mqpw = mqttPassword.value;
    }
    data.mqtp = mqttTopic.value;
    data.emen = enableEmail.value;
    data.smtp = smtpServer.value;
    data.sprt = smtpPort.value;
    data.send = smtpSender.value;
    data.apwd = smtpPassword.value;
    data.recp = smtpRecipient.value;
    data.noto = notifyOnDoorClose.checked ? 0b10 : 0 | notifyOnDoorOpen.checked ? 0b1 : 0;
    data.iftt = iftttKey.value;
    data.ati = openLongerTime.value;
    data.ato = openLongerClose.checked ? 0b10 : 0 | openLongerNotification.checked ? 0b1 : 0;
    data.atib = openAfterTime.value;
    data.ato = openAfterClose.checked ? 0b10 : 0 | openAfterNotification.checked ? 0b1 : 0;
    data.dri = readInterval.value;
    data.sfi = sensorFilter.value;
    data.cmr = consensusMargin.value;
    data.sto = distanceTimeout.value;
    data.htp = httpPort.value;
    data.host = hostname.value;
    data.ntp1 = useCustomNTP.chcked ? ntpServer.value : "";
    data.usi = useStaticIP.value;
    data.dvip = deviceIP.value;
    data.gwip = gatewayIP.value;
    data.subn = subnet.value;
    data.subn = subnet.value;
    data.dns1 = dns.value;
    if (changeDeviceKey.checked) {
        data.nkey = newKey.value;
        data.ckey = confirmNewKey.value;
    }

    const params = [];

    for (const [key, value] of Object.entries(data)) {
        params.push(`${key}=${encodeURIComponent(value)}`);
    }

    const res = await garageFetch("co", `dkey=${encodeURIComponent(keyInput.value)}`, ...params);
    const resData = await res.json();
    switch (resData.result) {
        case 1:
            successToast("Settings saved.");
            updateData();
            break;
        case 2:
            errorToast("Check device key and try again.");
            break;
        case 3:
            errorToast("New device key doesn't match confirmation key.");
            break;
        case 16:
            errorToast(`Missing data (${resData.item}).`);
            break;
        case 17:
            errorToast(`Out of range (${resData.item}).`);
            break;
        case 18:
            errorToast(`Format error (${resData.item}).`);
            break;
        default:
            errorToast("Saving failed.");
            break;
    }
}

document.getElementById("save-button").addEventListener("click", () => {
    updateSettings();
});