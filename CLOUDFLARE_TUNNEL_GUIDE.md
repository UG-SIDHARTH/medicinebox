# ⚡ Cloudflare Tunnel Setup Guide for ESP32 Smart MedBox

Cloudflare Tunnel (`cloudflared`) creates a secure, encrypted outbound connection between your ESP32 local Web Server and Cloudflare's global edge network. This allows you to remotely monitor and control your Smart MedBox from anywhere in the world over HTTPS **without exposing open ports on your router, setting up dynamic DNS, or needing a public IP**.

---

## 🚀 Quick Setup Methods

### Method 1: Using the Automated Windows Script (`cloudflared_runner.bat`)

1. Double-click [`cloudflared_runner.bat`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/cloudflared_runner.bat) in the project root directory.
2. Enter your ESP32 local IP address (e.g., `192.168.1.100` or `192.168.4.1`).
3. The script will automatically download `cloudflared.exe` (if not already installed) and launch a quick tunnel:
   ```text
   +-----------------------------------------------------------------------------------+
   | Your quick Tunnel has been created! Visit it at:                                  |
   | https://random-subdomain.trycloudflare.com                                       |
   +-----------------------------------------------------------------------------------+
   ```
4. Copy the generated `https://xxxx.trycloudflare.com` URL.
5. Open your Web Dashboard (`index.html`), click **⚙️ Config**, select **`⚡ Cloudflare Tunnel`**, paste the URL, and click **Save Settings**.

---

### Method 2: Manual Terminal Command (Windows / Linux / macOS / Raspberry Pi)

1. Install `cloudflared`:
   * **Windows**: `winget install --id Cloudflare.cloudflared`
   * **macOS**: `brew install cloudflared`
   * **Debian / Ubuntu / Raspberry Pi**:
     ```bash
     sudo mkdir -p /etc/apt/keyrings
     curl -fsSL https://pkg.cloudflare.com/cloudflare-main.gpg | sudo tee /etc/apt/keyrings/cloudflare-main.gpg >/dev/null
     echo "deb [signed-by=/etc/apt/keyrings/cloudflare-main.gpg] https://pkg.cloudflare.com/cloudflared jammy main" | sudo tee /etc/apt/sources.list.d/cloudflared.list
     sudo apt-get update && sudo apt-get install cloudflared
     ```
2. Start the temporary Quick Tunnel pointing to your ESP32 IP:
   ```bash
   cloudflared tunnel --url http://192.168.1.100:80
   ```
3. Copy the `https://xxxx.trycloudflare.com` URL into the Web Dashboard settings.

---

### Method 3: Permanent Named Tunnel ID for `mediback.ugsidharth.in`

To bind your permanent Cloudflare Tunnel ID directly to **`mediback.ugsidharth.in`**:

#### Option A: Using the Automated Custom Domain Launcher Script (`setup_custom_domain_tunnel.bat`)

1. Double-click [`setup_custom_domain_tunnel.bat`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/setup_custom_domain_tunnel.bat).
2. It opens your browser to log into your Cloudflare account (`ugsidharth.in`).
3. Press Enter to use the defaults (Tunnel name: `esp32-medbox`, Subdomain: `mediback.ugsidharth.in`).
4. Enter your ESP32 local IP address (e.g. `192.168.1.100` or `192.168.4.1`).
5. The script automatically generates your **Tunnel ID**, creates a DNS CNAME record for `mediback.ugsidharth.in` on Cloudflare, builds `config.yml`, and starts your tunnel!

#### Option B: Manual Command Breakdown for `mediback.ugsidharth.in`

1. **Authenticate `cloudflared` with Cloudflare**:
   ```bash
   cloudflared tunnel login
   ```
2. **Create a named Tunnel (generates a unique Tunnel ID / UUID)**:
   ```bash
   cloudflared tunnel create esp32-medbox
   ```
   *Output:*
   ```text
   Created tunnel esp32-medbox with id 8f4a19b2-3c7d-4e5f-9a1b-0c1d2e3f4a5b
   ```
3. **Route DNS record for `mediback.ugsidharth.in`**:
   ```bash
   cloudflared tunnel route dns esp32-medbox mediback.ugsidharth.in
   ```
4. **Create `config.yml`**:
   ```yaml
   tunnel: 8f4a19b2-3c7d-4e5f-9a1b-0c1d2e3f4a5b
   credentials-file: C:\Users\Lenovo\.cloudflared\8f4a19b2-3c7d-4e5f-9a1b-0c1d2e3f4a5b.json

   ingress:
     - hostname: mediback.ugsidharth.in
       service: http://192.168.1.100:80
     - service: http_status:404
   ```
5. **Run the Tunnel**:
   ```bash
   cloudflared tunnel run esp32-medbox
   ```
6. In the Web App dashboard, set **Connection Mode** to **Cloudflare Tunnel** and URL to `https://mediback.ugsidharth.in`.

---

## 🔒 Security & CORS Support in ESP32 Firmware

The ESP32 C++ firmware ([`medicinebox.ino`](file:///c:/Users/Lenovo/Downloads/medicine_reminder_box/medicinebox/medicinebox.ino)) includes:
* **Preflight CORS OPTIONS Handling**: Responds to browser preflight checks sent through Cloudflare edge proxying.
* **Cloudflare Edge Headers**: Accepts `CF-Connecting-IP`, `Cf-Access-Jwt-Assertion`, and custom auth headers.
* **Health Check API**: Endpoint `/api/tunnel` returns JSON device status and network readiness.
