import { initalize, loading, garageFetch, sendCommand, updateTitle, updateVersion } from "./common.js";
initalize(location.href);
import { partial_svg, open_svg, closed_svg, car_svg, arrow_up_svg, arrow_down_svg } from "./images.js";
const door = document.getElementById("door-status");
const doorAction = document.getElementById("door-action");

const lightLock = document.getElementById("light-lock");
const lightAction = document.getElementById("light-action");
const lockAction = document.getElementById("lock-action");

const vehicle = document.getElementById("vehicle-status");
const distance = document.getElementById("distance-value");
const readCount = document.getElementById("read-count");
const sensor2Value = document.getElementById("sn2-value");
const temperatureValue = document.getElementById("temperature-value");
const humidityValue = document.getElementById("humidity-value");
const wifiSignal = document.getElementById("wifi-signal");
const cloudStatus = document.getElementById("cloud-status");

const doorImg = document.getElementById("door-pic");
const arrowImg = document.getElementById("arrow-pic");
const carImg = document.getElementById("car-pic");

const keyInput = document.getElementById("key-input");
keyInput.value = localStorage.getItem("keyInput");
keyInput.addEventListener("input", () => {
    localStorage.setItem("keyInput", keyInput.value);
});

lightLock.classList.add("hidden");

loading(door,
    doorAction,
    lightAction,
    lockAction,
    vehicle,
    distance,
    readCount,
    sensor2Value,
    temperatureValue,
    humidityValue,
    wifiSignal,
    cloudStatus);

document.getElementById("reboot-button").addEventListener("click", () => {
    sendCommand("cc", keyInput.value, "reboot=1");
});

document.getElementById("reset-wifi").addEventListener("click", () => {
    sendCommand("cc", keyInput.value, "apmode=1");
});

document.getElementById("clear-log").addEventListener("click", () => {
    sendCommand("clearlog", keyInput.value);
});

document.getElementById("door-action").addEventListener("click", () => {
    sendCommand("cc", keyInput.value, "click=1");
    setTimeout(updateData, 500);
});

document.getElementById("light-action").addEventListener("click", () => {
    sendCommand("cc", keyInput.value, "light=toggle");
    setTimeout(updateData, 100);
});

document.getElementById("lock-action").addEventListener("click", () => {
    sendCommand("cc", keyInput.value, "lock=toggle");
    setTimeout(updateData, 100);
});

carImg.innerHTML = car_svg;

async function updateData() {
    const res = await garageFetch("jc");
    const data = await res.json();

    let doorStatusText;
    let doorStatusColor;

    switch (data.door) {
        case 0: // Closed
            doorStatusText = "Closed";
            doorAction.innerText = "Open Door";
            doorStatusColor = "text-success";
            doorImg.innerHTML = closed_svg;
            arrowImg.innerHTML = "";
            break;
        case 1: // Open
            doorStatusText = "Open";
            doorAction.innerText = "Close Door";
            doorStatusColor = "text-error";
            doorImg.innerHTML = open_svg;
            arrowImg.innerHTML = "";
            break;
        case 3: // Stopped
            doorStatusText = "Stopped";
            doorAction.innerText = "Toggle Door";
            doorStatusColor = "text-error";
            doorImg.innerHTML = partial_svg;
            arrowImg.innerHTML = "";
            break;
        case 4: // Closing
            doorStatusText = "Closing";
            doorAction.innerText = "Open Door";
            doorStatusColor = "text-success";
            doorImg.innerHTML = partial_svg;
            arrowImg.innerHTML = arrow_down_svg;
            break;
        case 5: // Opening
            doorStatusText = "Opening";
            doorAction.innerText = "Stop Door";
            doorStatusColor = "text-error";
            doorImg.innerHTML = partial_svg;
            arrowImg.innerHTML = arrow_up_svg;
            break;
        case 2: // UNKNOWN / UNSET
        default:
            doorStatusText = "Unknown";
            doorAction.innerText = "Unknown";
            doorStatusColor = "text-error";
            doorImg.innerHTML = "";
            arrowImg.innerHTML = "";
            break;
    }

    if (data.light == undefined || data.lock == undefined) {
        lightLock.classList.add("hidden");
    } else {
        lightLock.classList.remove("hidden");
        if (data.light) {
            lightAction.innerText = `Turn Off Light`;
            lightAction.className = "btn btn-warning";
        } else {
            lightAction.innerText = `Turn On Light`;
            lightAction.className = "btn btn-info";
        }

        if (data.lock) {
            lockAction.innerText = `Unlock Remotes`;
            lockAction.className = "btn btn-error";
        } else {
            lockAction.innerText = `Lock Remotes`;
            lockAction.className = "btn btn-info";
        }
    }

    let vehicleStatusText;

    switch (data.vehicle) {
        default:
            carImg.className = "w-full h-auto col-start-1 row-start-1 hidden";
            vehicleStatusText = "Absent";
            break;
        case 1:
            carImg.className = "w-full h-auto col-start-1 row-start-1 fill-base-content";
            vehicleStatusText = "Detected";
            break;
        case 2:
            carImg.className = "w-full h-auto col-start-1 row-start-1 fill-none stroke-info stroke-10";
            vehicleStatusText = "Unavailable";
            break;
    }

    updateTitle(data.name);
    updateVersion(data.fwv);

    door.innerText = doorStatusText;
    door.className = doorStatusColor;
    vehicle.innerText = vehicleStatusText;
    distance.innerText = data.dist + " (cm)";
    readCount.innerText = data.rcnt;

    if (data.sn2 == undefined) {
        sensor2Value.parentElement.classList.add("hidden");
    } else {
        sensor2Value.parentElement.classList.remove("hidden");
        sensor2Value.innerText = data.sn2 ? "High" : "Low";
    }

    if (data.temp == undefined) {
        temperatureValue.parentElement.classList.add("hidden");
    } else {
        temperatureValue.parentElement.classList.remove("hidden");
        temperatureValue.innerText = `${data.temp.toFixed(1)} \u00B0C (${((data.temp * 9 / 5) + 32).toFixed(1)} \u00B0F)`;
    }

    if (data.humidity == undefined) {
        humidityValue.parentElement.classList.add("hidden");
    } else {
        humidityValue.parentElement.classList.remove("hidden");
        humidityValue.innerText = data.humidity.toFixed(1) + "%";
    }

    wifiSignal.innerText = (data.rssi > -71 ? 'Good' : (data.rssi > -81 ? 'Weak' : 'Poor')) + ' (' + data.rssi + ' dBm)';
    let cloudStatusText;
    switch (data.cld) {
        default:
            cloudStatusText = "None";
            break;
        case 1: //Blynk
            cloudStatusText = "Blynk " + ["(disconnected)", "(connected)"][data.clds];
            break;
        case 2: //OTC
            cloudStatusText = "OTC " + ['(not enabled)', '(connecting...)', '(disconnected)', '(connected)'][data.clds];
            break;
    }

    cloudStatus.innerText = cloudStatusText;
}

updateData();

// setInterval(updateData, 1000);