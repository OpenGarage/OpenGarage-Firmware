import { initalize, garageFetch, sendCommand, updateTitle, updateVersion } from "./common.js";
        initalize(new URL("http://192.168.0.104/update" + "/..").href);

        const form = document.getElementById("update-form");

        async function sendUpdate(formData) {
            // let postURL = new URL(window.location);
            let postURL = new URL("http://192.168.0.104/update");
            postURL.port = 8080;

            const res = await fetch(postURL, {
            method: "POST",
            body: formData,
            });
           
            console.log(res);
        }

        form.addEventListener("submit", (event) => {
            event.preventDefault();
            const formData = new FormData(form);
            sendUpdate(formData);
            return false;
        });