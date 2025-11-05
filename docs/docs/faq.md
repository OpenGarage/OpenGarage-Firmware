## Frequently Asked Questions

!!! note
    This page covers the most common questions. For technical support, visit the [Support Portal](https://support.openthings.io) or the [Community Forums](https://opengarage.io/forums/).

### General Questions

**Q: What is OpenGarage?**
<br>
OpenGarage is an **open-source, WiFi-enabled smart** garage door controller that works with most existing garage opener system. It provides:

* **Remote Access**: Check and control your garage door and opener light from anywhere.
* **Advanced Features**: Intuitive web interface, event log, automation rules, and native support for Security+ 2.0/1.0.
* **Flexible Integration**: Cloud connectivity (optional), MQTT, IFTTT, a well-documented HTTP API.

**Q: What types of garage openers are compatible with OpenGarage?**
<br>
It works with most openers that come with a wall-button terminal. It supports both traditional, dry-contact openers, and modern Security+ 2.0 or 1.0 systems.

**Q: Who is OpenGarage for?**
<br>
Homeowners, business owners, makers who want remote status and control of their garage doors, with an easy-to-install, low-cost, feature-rich solution.

**Q: Does it require a cloud connection?**
<br>
No. By default it connects only to your local WiFi network and does NOT rely on the Internet. Cloud connection is optional and only needed for remote access.

**Q: Is there a subscription fee for cloud access?**
<br>
No. Cloud access is completely free.

**Q: Where can I find documents and source code?**

* [User Manual](https://opengarage.github.io/OpenGarage-Firmware/1.2.4/manual/)
* [API Reference](https://opengarage.github.io/OpenGarage-Firmware/1.2.4/api/)
* [Video Tutorials](https://openthings.freshdesk.com/support/solutions/articles/5000717106-video-introduction-to-opengarage)
* [Github Repository](https://github.com/opengarage)

---

### Purchase and Installation

**Q: Where can I buy it?**
<br>
You can purchase it directly from the [OpenGarage product page](https://opensprinkler.com/product/opengarage/)

**Q: How long does it take to ship? What are the terms and conditions?**
<br>
Please check our [shipping policy and other terms](https://opensprinkler.com/terms-and-conditions/)

**Q: What's included in the package?**
<br>
The package includes one OpenGarage controller with a 3D printed enclosure, screws, a USB power cable, and a 2-wire cable. A USB power adapter is an optional add-on.

**Q: How is it powered? What's the power consumption?**
<br>
OpenGarage is powered by a **5V USB** adapter rated for **at least 1A**. The power consumption is **0.35~0.5W**.

**Q: Does it have built-in wireless?**
<br>
Yes, OpenGarage has built-in 2.4GHz Wi-Fi (based on ESP8266).

**Q: Does it have built-in wired Ethernet?**
<br>
No, it doesn't.

---

### Technical Questions

**Q: How do I update its firmware?**
<br>
Firmware can be updated through the [built-in web interface](https://opengarage.github.io/OpenGarage-Firmware/#firmware-update-instructions). In the unlikely event the device becomes unresponsive (e.g. by uploading an incorrect or corrupted firmware), you can recover it by reflashing the firmware using a [USB-serial programmer](https://opensprinkler.com/product/usb-programmer/).

**Q: Will firmware update erase my settings?**
<br>
No. OpenGarage preserves your WiFi credentials, settings, and logs during a firmware update. Settings are only erased if you perform a manual factory reset.

**Q: How can I integrate it with Home Assistant or other smart platforms?**
<br>
See the [Integration Guide](https://opengarage.github.io/OpenGarage-Firmware/1.2.4/manual/#step-6-browser-mobile-app-and-home-assistant-integration)

**Q: What is the main difference between hardware v2.2 and v2.3+?**
<br>
v2.3+ can communicate directly with Security+ 2.0 and 1.0 systems (commonly used by LiftMaster, Chamberlain, and Craftsman brands). This allows for detailed status reporting (e.g., *opening*, *closing*, *stopped (partially open)*) and control of the opener's light and lock. Earlier hardware versions (v2.2 and below) rely solely on the distance sensor for detecting door status.

**Q: Tell me more about Security+ and how v2.3+ handles it.**
<br>
We have a [detailed blog post](https://opensprinkler.com/introducing-opengarage-2-3/) explaining the technology.

**Q: My OpenGarage fails to join or stay connected to my WiFi network.**

1. **Check the mode:** Is the unit still in **AP** (Access Point) mode (fast-blinking LED, twice a second)? If so, follow [WiFi Configuration](1.2.4/manual.md/#step-2-wifi-configuration) in the user manual.
2. **Changed router/SSID/password recently?** Follow the [Reset WiFi](1.2.4/manual.md/#step-4-button-actions) instructions. 
3. **Router compatibility checklist:**
    * **2.4 GHz only:** The controller's ESP8266 chip is compatible with 2.4 GHz only. Make sure your router has 2.4 GHz enabled and uses a different SSID than 5 GHz.
    * **Security:** Use **WPA2-PSK (AES)** or WPA2/WPA mixed. **Avoid WPA3-only** and **Enterprise**.
    * **Radio mode:** Set 2.4 GHz to Wi-Fi mode to **b/g/n (no ax)**.
    * **Channeling:** **20 MHz** width; prefer channels **1/6/11**.
    * **Band steering:** **Disable** band steering / Smart Connect.
    * **PMF/802.11w** (if applicable): Set to **Optional (not Required)**.
    * **Isolation/filters:** **Disable AP isolation**, **MAC filtering**, and **captive portal** requirements.
    * Turn on **legacy mode** on 2.4 GHz, **disable WMM and airtime fairness**.
    * Ensure the controller isn’t blocked by firewall rules.
4. **Additional troubleshooting:** If problems persist, there may be an ad-hoc compatibility issue between ESP8266 and your router. Try:
    * A different WiFi network (e.g. your phone's 2.4 GHz hotspot) to see if it works.
    * Look up your router model + `ESP8266` for known issues and fixes. For example, if your router is Asus BE7200, search (or use generative AI) `ESP8266 issues Asus BE7200`.

---
