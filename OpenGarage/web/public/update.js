import { initalize, successToast, errorToast, infoToast, updateTitle, updateVersion, garageFetch } from "./common.js";
import { ap } from "./mode.js";

initalize(new URL(location.href + "/..").href, ap);

async function getFirmwareAP() {
    const res = await garageFetch("db");
    /** @type {ApDebug} */
    const data = await res.json();
    updateVersion(data.fwv);
}

async function getDebugSTA() {
    const res = await garageFetch("db");
    const data = await res.json();
    updateVersion(data.fwv);
    updateTitle(data.name);
}

if (ap) {
    updateTitle("Open Garage");
    getFirmwareAP();
} else {
    getDebugSTA();
}

const form = document.getElementById("update-form");
const submitButton = document.getElementById("submit-button");

let submitting = false;

async function sendUpdate(formData) {
    let postURL = new URL(window.location);
    postURL.port = 8080;

    submitButton.disabled = true;
    infoToast("Uploading...");
    const res = await fetch(postURL, {
        method: "POST",
        body: formData,
    });
    submitButton.disabled = false;
    submitting = false;

    const data = await res.json();
    switch (data.result) {
        case 1:
            successToast("Update is successful. Rebooting. Please wait...");
            setTimeout(() => {
                location.reload();
            }, 10000);
            break;
        case 2:
            errorToast("Check device key and try again.");
            break;
        default:
            errorToast("Update failed.");
            break;
    }
}

form.addEventListener("submit", (event) => {
    event.preventDefault();
    if (submitting) {
        return;
    }

    submitting = true;
    const formData = new FormData(form);
    sendUpdate(formData);
    return false;
});