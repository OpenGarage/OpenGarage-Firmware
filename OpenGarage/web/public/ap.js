
/**
 * @typedef {Object} ApDebug
* @property {number} fwv - Firmware version (read-only)
* @property {number} dist - Distance value (cm)
*/

/**
 * @typedef {Object} ApSSID
* @property {string[]} ssids - Avaible SSIDs
* @property {string[]} rssis - Corresponding RSSI values
*/

/**
 * @typedef {Object} ApIP
* @property {number} ip - IP Address
*/

import { initalize, loading, garageFetch, sendCommand, updateTitle, updateVersion, successToast, infoToast, errorToast } from "./common.js";
initalize(location.href, true);
updateTitle("Open Garage");

const table = document.getElementById("wifi-table");

const wifiName = document.getElementById("wifi-name");
const wifiPassword = document.getElementById("wifi-password");
const wifiHostname = document.getElementById("wifi-hostname");

const cloudSelectNone = document.getElementById("cloud-none");
const cloudSelectBlynk = document.getElementById("cloud-blynk");
const cloudSelectOTC = document.getElementById("cloud-otc");

const cloudToken = document.getElementById("cloud-token");
const cloudServer = document.getElementById("cloud-server");
const cloudPort = document.getElementById("cloud-port");

const form = document.getElementById("form");
const submitButton = document.getElementById("submit-button");
const goToButton = document.getElementById("go-to-button");

let wifiInterval = -1;
let tryConnectInterval = -1;

async function tryConnect() {
    const res = await garageFetch("jt");
    /** @type {ApIP} */
    const data = await res.json();
    if (data.ip == 0) {
        return;
    }

    clearInterval(tryConnectInterval);
    const ip = `${data.ip % 256}.${(data.ip >>> 8) % 256}.${(data.ip >>> 16) % 256}.${(data.ip >>> 24) % 256}`;
    successToast(`Connected! Device IP is ${ip}. Switch to ${wifiName.value} and connect.`);
    submitButton.classList.add("hidden");
    goToButton.classList.remove("hidden");
    goToButton.innerText = `Go To ${ip}`;
    goToButton.type = "button";
    goToButton.href = `http://${ip}/`;
}

async function submit() {
    const params = [
        `ssid=${encodeURIComponent(wifiName.value)}`,
        `pass=${encodeURIComponent(wifiPassword.value)}`,
        `host=${encodeURIComponent(wifiHostname.value)}`,
    ];

    if (cloudSelectBlynk.checked || cloudSelectOTC.checked) {
        params.push(`cld=${cloudSelectBlynk.checked ? "blynk" : "otc"}`);
        params.push(`auth=${cloudToken.value}`);
        params.push(`bdmn=${cloudServer.value}`);
        params.push(`bprt=${cloudPort.value}`);
    }

    let res = await garageFetch("cc", params);
    let data = await res.json();
    
    if (data.result == 1) {
        clearInterval(wifiInterval);
        submitButton.disabled = true;
        wifiName.disabled = true;
        submitButton.innerText = "Connecting...";
        infoToast("Connecting to " + wifiName.value);
        setInterval(tryConnect, 1000);
    } else {
        errorToast(`Error connecting to network (${data.result})`);
    }
}

form.addEventListener("submit", (e) => {
    e.preventDefault();
    submit();
});


cloudSelectNone.addEventListener("input", () => {
    cloudToken.disabled = true;
    cloudServer.disabled = true;
    cloudPort.disabled = true;
});

cloudSelectBlynk.addEventListener("input", () => {
    cloudToken.disabled = false;
    cloudServer.disabled = false;
    cloudPort.disabled = false;
    cloudServer.value = "blynk.openthings.io";
    cloudPort.value = 8080;
});

cloudSelectOTC.addEventListener("input", () => {
    cloudToken.disabled = false;
    cloudServer.disabled = false;
    cloudPort.disabled = false;
    cloudServer.value = "ws.cloud.openthings.io";
    cloudPort.value = 80;
});

/**
 * Adds row to WiFi network table
 * @param {string} ssid SSID for network
 * @param {number} rssi RSSI value
 */
function createRow(ssid, rssi) {
    const row = document.createElement("tr");

    const radioColumn = document.createElement("th");
    const radioButton = document.createElement("input");
    radioButton.type = "radio";
    radioButton.name = "wifi-radio";
    radioButton.className = "radio";
    radioButton.addEventListener("input", () => {
        wifiName.value = ssid;
    });

    radioColumn.append(radioButton)
    row.appendChild(radioColumn);

    const ssidColumn = document.createElement("td");
    ssidColumn.innerText = ssid;
    row.appendChild(ssidColumn);

    const strengthColumn = document.createElement("td");
    strengthColumn.innerText = rssi > -71 ? "Ok" : rssi > -81 ? "Weak" : "Poor";
    row.appendChild(strengthColumn);

    const powerColumn = document.createElement("td");
    powerColumn.innerText = `(${rssi} dBm)`;
    row.appendChild(powerColumn);

    table.appendChild(row);
}

async function getFirmware() {
    const res = await garageFetch("db");
    /** @type {ApDebug} */
    const data = await res.json();
    updateVersion(data.fwv);
}

getFirmware();

async function updateData() {
    const res = await garageFetch("js");
    /** @type {ApSSID} */
    const data = await res.json();
    table.innerHTML = "";
    data.ssids.forEach((ssid, i) => {
        createRow(ssid, Number.parseInt(data.rssis[i]));
    })
}

wifiInterval = setInterval(updateData, 1000);