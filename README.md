# 💊 Smart Medicine Reminder Box (Smart MedBox)

An IoT-powered automated medicine reminder box and telemetry system built using **ESP32**, **HC-SR04 Ultrasonic Sensor**, **16x2 I2C LCD Display**, and a **Real-Time Web Application** supporting cross-network remote cloud synchronization over **MQTT**.

---

## 🌟 Key Features

### 📟 ESP32 Hardware Firmware
* **Ultrasonic Pill Intake Detection**: HC-SR04 ultrasonic distance sensor detects when the box lid is opened ($<15\text{ cm}$) to automatically verify and record dosage intake.
* **16x2 I2C LCD Status Display**: Displays live NTP clock, upcoming medicine countdown marquee, and network status.
* **Single Button Multi-Control**:
  * **Single Click**: Toggles LCD screen to display the ESP32 IP address.
  * **Hold 3 Seconds**: Clears NVS flash memory and resets Wi-Fi configuration.
* **Visual & Audible Alerts**: Active buzzer alert and dual-LED status indicators (Red = Alert / Ringing, Green = Medicine Taken / Success).
* **NTP Time Sync**: Automatic time synchronization (+5:30 IST default with pool.ntp.org).
* **AP Setup Mode Fallback**: Soft AP (`MedBox-Setup` at `192.168.4.1`) for easy Wi-Fi configuration when no Wi-Fi credentials are stored.
* **Dual-Broker MQTT Cloud Sync**: Connects to `broker.emqx.io` and `broker.hivemq.com` for cross-network communication across different Wi-Fi/cellular networks.

---

### 💻 Modern Web Application Dashboard
* **Glassmorphism UI**: Premium dark mode UI built with vanilla HTML5, CSS3, and JavaScript.
* **Cross-Network Cloud Sync**: Uses **Secure WebSockets (`wss://broker.emqx.io:8084/mqtt`)** to communicate with the ESP32 from anywhere in the world (compatible with Debian VPS / remote server hosting over HTTPS).
* **Real-time Telemetry Grid**: Displays lid open/closed state, ultrasonic distance, active alarm status, RSSI signal strength, and daily intake statistics.
* **Schedule Manager**: Add, edit, toggle, and delete medicine timers synced to ESP32 NVS memory.
* **Remote Hardware Control**: Trigger hardware buzzer & LED tests or manually mark doses as taken from the web dashboard.
* **Intake History Log**: Keeps a persistent activity log of taken doses.

---

## 🛠️ Hardware Requirements

| Component | Pin / Connection | Description |
| :--- | :--- | :--- |
| **ESP32 Development Board** | Main Controller | Microcontroller with Wi-Fi & Bluetooth |
| **HC-SR04 Ultrasonic Sensor** | Trig: `GPIO 12`, Echo: `GPIO 14` | Detects box lid open/closed state |
| **I2C 16x2 LCD Display** | SDA: `GPIO 21`, SCL: `GPIO 22` | Displays time, marquee, and IP info |
| **Active Buzzer** | `GPIO 25` | Sound alert for medicine alarms |
| **Red LED** | `GPIO 26` | Alarm ringing indicator |
| **Green LED** | `GPIO 27` | Dosage taken success indicator |
| **Push Button** | `GPIO 13` (Internal Pullup) | Mode toggle & hold-to-reset button |

---

## 📁 Repository Structure

```text
medicinebox/
├── standalone_cloudflare_esp32.ino # Standalone ESP32 C++ firmware (NO LCD/Ultrasonic/Hardware required)
├── medicinebox.ino                 # Complete ESP32 C++ firmware with hardware sensors & display
├── index.html                      # Web Dashboard HTML layout & modals
├── app.js                          # Frontend JavaScript, Cloudflare Tunnel & MQTT state logic
├── styles.css                      # Vanilla CSS design tokens & glassmorphism theme
├── setup_custom_domain_tunnel.bat  # Automated Cloudflare Tunnel ID creator for mediback.ugsidharth.in
└── README.md                       # Project documentation
```

---

## 🚀 Quick Setup & Deployment Guide

### 1. Flashing the ESP32 Firmware
* **Option A (No Hardware Connected - Recommended)**: Open [`standalone_cloudflare_esp32.ino`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/standalone_cloudflare_esp32.ino) in **Arduino IDE**. No external libraries required (uses builtin `WiFi`, `WebServer`, `Preferences`). Select board **ESP32 Dev Module** and click **Upload**.
* **Option B (Full Hardware Box)**: Open [`medicinebox.ino`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/medicinebox.ino) in **Arduino IDE**. Install `LiquidCrystal_I2C` and `PubSubClient`, then click **Upload**.

### 2. Wi-Fi Configuration
* If the ESP32 has no saved Wi-Fi credentials, it starts in AP mode (**`MedBox-Setup`**).
* Connect your phone/laptop to **`MedBox-Setup`** and open `http://192.168.4.1`.
* Select your Wi-Fi network, enter the password, and click **Save & Connect Wi-Fi**.

### 3. Web Dashboard Hosting (Debian OS / Web Server)
1. Copy `index.html`, `app.js`, and `styles.css` to your web root (e.g. `/var/www/html/` on Debian OS):
   ```bash
   scp index.html app.js styles.css root@YOUR_SERVER_IP:/var/www/html/
   ```
2. Open your website (e.g. `https://medicinebox.ugsidharth.in`).
3. Click **⚙️ Config** and verify **`☁️ Cloud Sync`** mode is active.

### 4. ⚡ Cloudflare Tunnel (Secure HTTPS Remote Access)
1. Run [`cloudflared_runner.bat`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/cloudflared_runner.bat) or run:
   ```bash
   cloudflared tunnel --url http://YOUR_ESP32_IP:80
   ```
2. Copy the generated `https://xxxx.trycloudflare.com` URL.
3. Open the Web Dashboard, click **⚙️ Config**, select **`⚡ Cloudflare Tunnel`**, paste your URL, and click **Save Settings**.
4. For detailed custom domain setup, read [`CLOUDFLARE_TUNNEL_GUIDE.md`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/CLOUDFLARE_TUNNEL_GUIDE.md).

---

## 📡 MQTT Cloud Architecture

| Direction | MQTT Topic | Description |
| :--- | :--- | :--- |
| **ESP32 $\rightarrow$ Dashboard** | `ug_sidharth/medbox/telemetry` | JSON telemetry payload (distance, box state, LEDs, active alarm) |
| **Dashboard $\rightarrow$ ESP32** | `ug_sidharth/medbox/commands` | Control commands (`take`, `test_alarm`) |
| **Dashboard $\leftrightarrow$ ESP32** | `ug_sidharth/medbox/alarms` | Synchronized JSON array of medicine timers |

---

## 📄 License
This project is open-source and available under the **MIT License**.
