## Frequently Asked Questions

!!! note
    This page covers the stock OpenGarage firmware. For technical support, visit the [Support Portal](https://support.openthings.io) or the [Community Forums](https://opengarage.io/forums/).

### General Questions

**Q: What is OpenGarage?**
<br>
OpenGarage is an **open-source, WiFi-enabled smart** garage door controller that works with most existing garage opener systems. It provides:

* **Remote Access**: Check and control your garage door and opener light from anywhere.
* **Advanced Features**: Intuitive web interface, event log, automation rules, and native support for Security+ 2.0/1.0.
* **Flexible Integration**: Cloud connectivity (optional), MQTT, IFTTT, a well-documented HTTP API.

**Q: What types of garage openers are compatible with OpenGarage?**
<br>

* **v2.3+** directly supports Security+ 2.0 and 1.0, as well as dry-contact 12VDC systems.
* **v2.2** supports traditional dry-contact, AC and higher-voltage systems.
* Security+ 3.0 (white learn button) requires a “sacrifice remote” as an adapter; see the [DIY Security+ 3.0 Adapter](guides/diy-security-adapter.md) guide.

See the [product page](https://opensprinkler.com/product/opengarage/) for help choosing hardware and the [user manual](manual/index.html) for wiring instructions.

**Q: Who is OpenGarage for?**
<br>
Homeowners, business owners, and makers who want remote status and control of their garage doors, with an easy-to-install, low-cost, feature-rich solution.

**Q: Does it require a cloud connection?**
<br>
No. OpenGarage is local-first. By default it connects only to your local WiFi network and does NOT rely on the Internet. Cloud connection is optional; it is one way to enable remote access.

**Q: Is there a subscription fee for cloud access?**
<br>
No. OpenThings Cloud (OTC) access for OpenGarage is free.

**Q: Where can I find documents and source code?**

* [User Manual](manual/index.html)
* [API Reference](api/index.html)
* [Video Tutorials](https://openthings.freshdesk.com/support/solutions/articles/5000717106-video-introduction-to-opengarage)
* [GitHub Repository](https://github.com/opengarage)

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
The package includes one OpenGarage controller with a 3D-printed enclosure, a USB power cable, and a 2-wire cable. A USB power adapter is an optional add-on.

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
Firmware can be updated through the [built-in web interface](index.md#firmware-update-instructions). In the unlikely event the device becomes unresponsive (e.g. by uploading incorrect or corrupted firmware), you can recover it by reflashing the firmware using a [USB-serial programmer](https://opensprinkler.com/product/usb-programmer/).

**Q: Will firmware update erase my settings?**
<br>
No. OpenGarage preserves your WiFi credentials, settings, and logs during a normal firmware update. Upgrading from an old firmware (prior to 1.2.0) to the latest is an exception because the flash layout changed; see the [firmware update warning](https://opengarage.github.io/OpenGarage-Firmware/#troubleshooting).

**Q: How can I integrate it with Home Assistant or other smart platforms?**
<br>
Home Assistant has a built-in OpenGarage integration for the stock firmware over your local network. For a more complete set of Home Assistant entities, including Security+ light and remote lock controls on compatible hardware, consider the alternative [ESPHome-based firmware](https://github.com/OpenGarage/OpenGarage-ESPHome). Stock firmware also supports MQTT, IFTTT and the [HTTP API](api/index.html).

**Q: What is the main difference between hardware v2.2 and v2.3+?**
<br>
v2.3+ can communicate directly with Security+ 2.0 and 1.0 systems (by LiftMaster, Chamberlain, and Craftsman brands). This allows for detailed status reporting (e.g., *opening*, *closing*, *stopped*) and control of the opener's light and lock. It additionally uses the built-in distance sensor to detect and report vehicle presence.

v2.2 uses the distance sensor (or an optional wired switch sensor) for door and vehicle statuses.

**Q: Tell me more about Security+ and how v2.3+ handles it.**
<br>
We have a [detailed blog post](https://opensprinkler.com/introducing-opengarage-2-3/) explaining the technology.

**Q: My OpenGarage fails to join or stay connected to my WiFi network.**

1. **Check the mode:** Is the unit still in **AP** (Access Point) mode (fast-blinking LED)? If so, follow [WiFi Configuration](manual/index.html#step-2-wifi-configuration) in the user manual.
2. **Changed router/SSID/password recently?** Follow the [Reset WiFi](manual/index.html#step-4-button-actions) instructions.
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

**Q: Can I still use Blynk?**
<br>
Blynk support was removed in **firmware 1.2.6**, and the OpenThings Blynk server will be retired soon. Use [OpenThings Cloud (OTC)](guides/remote-access-otc.md) for remote access with stock firmware. A Blynk token cannot be used as an OTC token.

---
