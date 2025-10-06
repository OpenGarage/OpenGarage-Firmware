# Archived Manuals and API References

These documents are for older versions of the OpenGarage firmware and are provided in their original PDF format. For the latest documentation, please use the versioned navigation at the top of the page.

---

### Firmware 1.2.3
* [User Manual (PDF)](assets/pdfs/OGManual1.2.3.pdf)
* [API Reference (PDF)](assets/pdfs/OGAPI1.2.3.pdf)

---

### Firmware 1.2.0
* [User Manual (PDF)](assets/pdfs/OGManual1.2.0.pdf)
* [API Reference (PDF)](assets/pdfs/OGAPI1.2.0.pdf)

---

### Firmware 1.1.2
* [User Manual (PDF)](assets/pdfs/OGManual1.1.2.pdf)
* [API Reference (PDF)](assets/pdfs/OGAPI1.1.2.pdf)

---

### Firmware 1.1.0
* [User Manual (PDF)](assets/pdfs/OGManual1.1.0.pdf)
* [API Reference (PDF)](assets/pdfs/OGAPI1.1.0.pdf)

---

### Firmware 1.0.7
* [User Manual (PDF)](assets/pdfs/OGManual1.0.7.pdf)
* [API Reference (PDF)](assets/pdfs/OGAPI1.0.7.pdf)

---

### Other Docs
* [Firmware 1.0.9 API](assets/pdfs/OGAPI1.0.9.pdf)
* [Firmware 1.0.6 API](assets/pdfs/OGAPI1.0.6.pdf)
* [Firmware 1.0.5 API](assets/pdfs/OGAPI1.0.5.pdf)
* [Firmware 1.0.4 API](assets/pdfs/OGAPI1.0.4.pdf)

---

### Blynk Legacy App

!!! info
    The instructions below apply only to the Blynk **legacy app** and will **NOT** work with the new Blynk IoT app.

The Blynk **legacy app** is discontinued and no longer available in the official app stores. On Android phones, you may still find the legacy `.apk` from third-party websites and install it manually. To use it:

* Follow the [Blynk (Legacy) App Support Article](https://openthings.freshdesk.com/a/solutions/articles/5000872170) to request a Blynk account on the `openthings.io` cloud server.

* Launch the app, sign in with your account credentials, tap the **QR code** icon, and scan the appropriate QR code below. This creates a project in the app, and the Blynk token will be emailed to you (you can copy the token manually from the Project Settings in the app).

##### **v1.3**
* Requires firmware `1.2.4` or newer
* Door status LED uses distinct colors for each state
* Added light and lock toggles
![Blynk App v1.3](assets/blynk_qrs/og_blynk_1.3.png){: .center_wider}

|Virtual pin |Used for|
|:--------  -|:------------|
|`V0`| Door status (binary) |
|`V1`| Trigger relay |
|`V2`| Door status (complete: including stopped, opening, closing) |
|`V3`| Distance value |
|`V4`| Car status |
|`V6`| Temperature (if temperature sensor is enabled) |
|`V7`| Humidity (if humidity sensor is enabled) |
|`V8`| Toggle light |
|`V9`| Toggle remote lock |
<p></p>

##### **v1.2**
* Requires firmware `1.1.2` or newer
* Added temperature and humidity history (if the sensors are enabled)
![Blynk App v1.2](assets/blynk_qrs/og_blynk_1.2.png){: .center}
<p></p>

##### **v1.1**
* Requires firmware `1.0.9` or newer
* Added Car Status LED
![Blynk App v1.1](assets/blynk_qrs/og_blynk_1.1.png){: .center}

<p></p>

##### **v1.0**
* The original version
![Blynk App v1.0](assets/blynk_qrs/og_blynk_1.0.png){: .center}
---
