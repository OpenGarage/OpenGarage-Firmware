const openGarageRoot = "http://192.168.12.206/";
export async function garageFetch(path, ...params) {
    return await fetch(openGarageRoot + path + "?" + params.join("&"));
}

export async function sendCommand(path, key, ...params) {
    params.push('dkey=' + encodeURIComponent(key));
    let res = await garageFetch(path, params);

    let data = await res.json();
    if (data.result != undefined) {
        switch (data.result) {
            case 1: //Success
                break;
            case 2: //Invalid device key
                errorToast("Invalid device key");
                break;
            //Ignore other cases
            default:
                break;
        }
    }
}

export function loading(...elements) {
    elements.forEach((element) => {
        element.innerHTML = `<span class="loading loading-dots loading-sm"></span>`;
    });
}

const drawerContent = document.getElementById("drawer-content");

const toastContainer = document.createElement("div");
toastContainer.className = "toast toast-bottom toast-end";
drawerContent.prepend(toastContainer);

const toast = document.createElement("div");
toast.className = "hidden";
toastContainer.appendChild(toast);

const toastMessage = document.createElement("span");
toast.appendChild(toastMessage);

const navbar = document.createElement("div");
navbar.className = "navbar bg-base-100 shadow-sm";
drawerContent.prepend(navbar);

const navbarStart = document.createElement("div");
navbarStart.className = "navbar-start";
navbar.appendChild(navbarStart);

const menu = document.createElement("label");
menu.setAttribute("for", "og-drawer");
menu.tabIndex = "0";
menu.role = "button";
menu.className = "btn btn-ghost btn-circle drawer-button";
navbarStart.appendChild(menu);

const menuIcon = document.createElementNS("http://www.w3.org/2000/svg", "svg");
menuIcon.setAttribute("class", "h-5 w-5");
menuIcon.setAttribute("fill", "none");
menuIcon.setAttribute("viewBox", "0 0 24 24");
menuIcon.setAttribute("stroke", "currentColor");
menu.appendChild(menuIcon);

const menuPath = document.createElementNS("http://www.w3.org/2000/svg", "path");
menuPath.setAttribute("stroke-linecap", "round");
menuPath.setAttribute("stroke-linejoin", "round");
menuPath.setAttribute("stroke-width", "2");
menuPath.setAttribute("d", "M4 6h16M4 12h16M4 18h7");
menuIcon.appendChild(menuPath);

const navbarCenter = document.createElement("div");
navbarCenter.className = "navbar-center";
navbar.appendChild(navbarCenter);

const title = document.createElement("a");
title.className = "btn btn-ghost text-xl";
title.innerText = "Open Garage: Loading";
navbarCenter.appendChild(title);

const navbarEnd = document.createElement("div");
navbarEnd.className = "navbar-end";
navbar.appendChild(navbarEnd);

const themeSwap = document.createElement("label");
themeSwap.className = "swap swap-rotate";
navbarEnd.appendChild(themeSwap);

const themeSelector = document.createElement("input");
themeSelector.type = "checkbox";
themeSwap.appendChild(themeSelector);

const sunIcon = document.createElementNS("http://www.w3.org/2000/svg", "svg");
sunIcon.setAttribute("class", "swap-off h-8 w-8 fill-current");
sunIcon.setAttribute("viewBox", "0 0 24 24");
themeSwap.appendChild(sunIcon);

const sunPath = document.createElementNS("http://www.w3.org/2000/svg", "path");
sunPath.setAttribute("d", "M5.64,17l-.71.71a1,1,0,0,0,0,1.41,1,1,0,0,0,1.41,0l.71-.71A1,1,0,0,0,5.64,17ZM5,12a1,1,0,0,0-1-1H3a1,1,0,0,0,0,2H4A1,1,0,0,0,5,12Zm7-7a1,1,0,0,0,1-1V3a1,1,0,0,0-2,0V4A1,1,0,0,0,12,5ZM5.64,7.05a1,1,0,0,0,.7.29,1,1,0,0,0,.71-.29,1,1,0,0,0,0-1.41l-.71-.71A1,1,0,0,0,4.93,6.34Zm12,.29a1,1,0,0,0,.7-.29l.71-.71a1,1,0,1,0-1.41-1.41L17,5.64a1,1,0,0,0,0,1.41A1,1,0,0,0,17.66,7.34ZM21,11H20a1,1,0,0,0,0,2h1a1,1,0,0,0,0-2Zm-9,8a1,1,0,0,0-1,1v1a1,1,0,0,0,2,0V20A1,1,0,0,0,12,19ZM18.36,17A1,1,0,0,0,17,18.36l.71.71a1,1,0,0,0,1.41,0,1,1,0,0,0,0-1.41ZM12,6.5A5.5,5.5,0,1,0,17.5,12,5.51,5.51,0,0,0,12,6.5Zm0,9A3.5,3.5,0,1,1,15.5,12,3.5,3.5,0,0,1,12,15.5Z");
sunIcon.appendChild(sunPath);

const moonIcon = document.createElementNS("http://www.w3.org/2000/svg", "svg");
moonIcon.setAttribute("class", "swap-on h-8 w-8 fill-current");
moonIcon.setAttribute("viewBox", "0 0 24 24");
themeSwap.appendChild(moonIcon);

const moonPath = document.createElementNS("http://www.w3.org/2000/svg", "path");
moonPath.setAttribute("d", "M21.64,13a1,1,0,0,0-1.05-.14,8.05,8.05,0,0,1-3.37.73A8.15,8.15,0,0,1,9.08,5.49a8.59,8.59,0,0,1,.25-2A1,1,0,0,0,8,2.36,10.14,10.14,0,1,0,22,14.05,1,1,0,0,0,21.64,13Zm-9.5,6.69A8.14,8.14,0,0,1,7.08,5.22v.27A10.15,10.15,0,0,0,17.22,15.63a9.79,9.79,0,0,0,2.1-.22A8.11,8.11,0,0,1,12.14,19.73Z");
moonIcon.appendChild(moonPath);

themeSelector.checked = (localStorage.getItem("darkMode") || (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? "true" : "false")) == "true";
document.documentElement.setAttribute("data-theme", themeSelector.checked ? "dark" : "light");
themeSelector.addEventListener("input", () => {
    localStorage.setItem("darkMode", themeSelector.checked);
    document.documentElement.setAttribute("data-theme", themeSelector.checked ? "dark" : "light");
});


const drawerInner = document.getElementById("drawer-inner");

const drawerList = document.createElement("ul");
drawerList.className = "menu text-base-content w-80 p-4 flex flex-col";
drawerInner.appendChild(drawerList);

const drawerSpacer = document.createElement("div");
drawerSpacer.className = "flex-1";
drawerInner.appendChild(drawerSpacer);

const drawerVersion = document.createElement("div");
drawerVersion.className = "flex gap-2 p-2";
drawerInner.appendChild(drawerVersion);

const versionTitle = document.createElement("span");
versionTitle.innerText = "Version:";
drawerVersion.appendChild(versionTitle);

const versionNumber = document.createElement("span");
loading(versionNumber);
drawerVersion.appendChild(versionNumber);

function createDrawerMenuItem(name, href) {
    const listItem = document.createElement("li");
    drawerList.appendChild(listItem);

    const link = document.createElement("a");
    link.href = href;
    listItem.appendChild(link);
    link.innerText = name;
}

createDrawerMenuItem("Home", "/");
createDrawerMenuItem("Options", "vo/");
createDrawerMenuItem("View Log", "/vl");
createDrawerMenuItem("Firmware Update", "/update");
createDrawerMenuItem("User Manual", "/https://github.com/OpenGarage/OpenGarage-Firmware/tree/master/docs");

document.body.classList.remove("hidden");

function getDigits(n, count) {
    return new Array(count).fill(undefined).map(() => {
        const r = n % 10;
        n = Math.floor(n / 10);
        return r;
    });
}

export function updateTitle(t) {
    title.innerText = t;
}

export function updateVersion(fwv) {
    const versionDigits = getDigits(fwv, 3);
    versionNumber.innerText = versionDigits[2] + "." + versionDigits[1] + "." + versionDigits[0];
}

let toastTimeout;
function showToast(level, message) {
    clearTimeout(toastTimeout);
    let toastClass = "alert ";
    switch (level) {
        case 0:
            toastClass += "alert-info";
            break;
        case 0:
            toastClass += "alert-success";
            break;
        case 2:
            toastClass += "alert-warning";
            break;
        case 3:
            toastClass += "alert-error";
            break;
    }
    toast.className = toastClass;

    toastMessage.innerText = message;

    toastTimeout = setTimeout(() => {
        toast.className = "hidden";
    }, 2000);
}

export const infoToast = (message) => showToast(0, message);
export const successToast = (message) => showToast(1, message);
export const warningToast = (message) => showToast(2, message);
export const errorToast = (message) => showToast(3, message);