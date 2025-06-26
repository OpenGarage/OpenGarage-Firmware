
import { initalize, garageFetch, updateTitle, updateVersion } from "./common.js";
initalize(new URL(location.href + "/..").href);
const tableBody = document.getElementById("table-body");
const sn2Head = document.getElementById("sn2-head");

const currentTime = document.getElementById("current-time");
const startTime = document.getElementById("start-time");
const uptime = document.getElementById("uptime");
const recordCount = document.getElementById("record-count");

function timeDigit(num) {
    return num.toLocaleString('en-US', { minimumIntegerDigits: 2, useGrouping: false });
}

async function updateData() {
    const res = await garageFetch("jl");
    const data = await res.json();

    updateTitle(data.name);
    updateVersion(data.fwv);

    tableBody.innerHTML = "";

    if (data.ncols > 3) {
        sn2Head.classList.remove("hidden");
    } else {
        sn2Head.classList.add("hidden");
    }

    currentTime.innerText = new Date(data.time * 1000).toLocaleString();
    startTime.innerText = new Date(data.starttime * 1000).toLocaleString();
    const delta = data.time - data.starttime;
    const seconds = delta % 60;
    const minutes = Math.floor((delta / 60) % 3600);
    const hours = Math.floor((delta / 3600) % 86400);
    const days = Math.floor(delta / 86400);
    uptime.innerText = days + ":" + timeDigit(hours) + ":" + timeDigit(minutes) + ":" + timeDigit(seconds);
    recordCount.innerText = data.logs.length;

    for (let i = data.logs.length - 1; i >= 0; i--) {
        const log = data.logs[i];
        const row = document.createElement("tr");
        tableBody.appendChild(row);

        const header = document.createElement("th");
        header.innerText = data.logs.length - i;
        row.append(header);

        const statusCell = document.createElement("td");
        row.append(statusCell);

        const statusBadge = document.createElement("div");
        statusBadge.className = "badge";
        switch (log[1]) { // Door status
            case 0:
                statusBadge.innerText = "Closed";
                statusBadge.classList.add("badge-success");
                break;
            case 1:
                statusBadge.innerText = "Opened";
                statusBadge.classList.add("badge-error");
                break;
            default:
                statusBadge.innerText = "UNKNOWN"
                statusBadge.classList.add("badge-neutral");
                break;
        }
        statusCell.appendChild(statusBadge);

        let col = [];
        col.push(new Date(log[0] * 1000).toLocaleString());
        col.push(log[2] + " cm");

        if (data.ncols > 3) {
            switch (log[3]) {
                case 0:
                    col.push("Low");
                    break;
                case 1:
                    col.push("High");
                    break;
                default:
                    col.push("-");
                    break;
            }
        }

        col.forEach((e) => {
            const cell = document.createElement("td");
            cell.innerText = e;
            row.append(cell);
        });
    }
}

updateData();

setInterval(updateData, 5000);
