#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <pgmspace.h>

// Full Glassmorphism MedBox Web App HTML Layout
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart MedBox • Telemetry Dashboard</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@400;500;600;700&family=Plus+Jakarta+Sans:wght@400;500;600;700&display=swap" rel="stylesheet">
  <link rel="stylesheet" href="/styles.css">
</head>
<body>
  <div class="app-container">
    <header class="app-header glass-panel">
      <div class="brand">
        <div class="brand-icon">💊</div>
        <div>
          <h1>Smart MedBox</h1>
          <p class="subtitle">Cloudflare Remote Monitor • mediback.ugsidharth.in</p>
        </div>
      </div>
      <div class="header-status">
        <div class="connection-badge" id="connectionBadge">
          <span class="status-dot"></span>
          <span id="connectionText">Connecting...</span>
        </div>
        <div class="header-clock">
          <span id="headerTime">--:--:--</span>
          <small id="headerDate">--- --, ----</small>
        </div>
        <button class="btn btn-secondary btn-icon-only" onclick="openConfigModal()" title="Config">⚙️</button>
      </div>
    </header>

    <main class="dashboard-grid">
      <section class="next-pill-card glass-panel highlight-border">
        <div class="card-header">
          <span class="badge badge-emerald" id="nextPillTag">⚡ Next Scheduled Dose</span>
          <span class="pill-dot" id="nextPillDot"></span>
        </div>
        <div class="next-pill-content">
          <h2 id="nextMedName">Loading Alarms...</h2>
          <p id="nextMedDosage">Syncing with ESP32...</p>
          <div class="next-time-display">
            <span class="time-tag" id="nextMedTime">⏰ --:--</span>
          </div>
        </div>
        <div class="countdown-wrapper">
          <span class="countdown-label">Time Remaining Until Next Dose</span>
          <div class="countdown-timer">
            <div class="cd-unit"><strong id="cdHours">00</strong><label>Hours</label></div>
            <span class="cd-colon">:</span>
            <div class="cd-unit"><strong id="cdMins">00</strong><label>Mins</label></div>
            <span class="cd-colon">:</span>
            <div class="cd-unit"><strong id="cdSecs">00</strong><label>Secs</label></div>
          </div>
        </div>
      </section>

      <section class="timers-section glass-panel">
        <div class="section-header">
          <h2>Medicine Schedules</h2>
          <button class="btn btn-primary" onclick="openAddModal()">+ Add Timer</button>
        </div>
        <div class="timers-grid" id="timersGrid"></div>
      </section>

      <section class="hardware-section glass-panel">
        <div class="section-header">
          <h2>ESP32 Telemetry & Active Hardware</h2>
          <div class="action-buttons">
            <button class="btn btn-emerald btn-sm" onclick="triggerMarkTaken()">✓ Record Intake</button>
            <button class="btn btn-secondary btn-sm" onclick="triggerTestBuzzer()">🔔 Test Alert</button>
          </div>
        </div>
        <div class="hw-grid">
          <div class="hw-item" id="hwDistance">
            <span class="hw-icon">📏</span>
            <div class="hw-info">
              <label>Ultrasonic Sensor</label>
              <strong id="distanceValue">-- cm</strong>
            </div>
            <div class="sensor-bar"><div class="sensor-fill" id="distanceBar" style="width: 0%;"></div></div>
          </div>
          <div class="hw-item" id="hwBoxState">
            <span class="hw-icon">📦</span>
            <div class="hw-info">
              <label>Lid Detection</label>
              <strong id="boxStateBadge">Checking Lid...</strong>
            </div>
          </div>
          <div class="hw-item" id="hwBuzzer">
            <span class="hw-icon">🔊</span>
            <div class="hw-info">
              <label>Active Buzzer (GPIO 25)</label>
              <strong id="buzzerStateText">OFF</strong>
            </div>
          </div>
          <div class="hw-item" id="hwRedLed">
            <span class="hw-icon">🔴</span>
            <div class="hw-info">
              <label>Red LED (Alert)</label>
              <strong id="redLedText">OFF</strong>
            </div>
          </div>
          <div class="hw-item" id="hwGreenLed">
            <span class="hw-icon">🟢</span>
            <div class="hw-info">
              <label>Green LED (Success)</label>
              <strong id="greenLedText">OFF</strong>
            </div>
          </div>
          <div class="hw-item" id="hwRssi">
            <span class="hw-icon">📡</span>
            <div class="hw-info">
              <label>WiFi Signal</label>
              <strong id="rssiText">-- dBm</strong>
            </div>
          </div>
        </div>
        <div class="intake-summary">
          <div class="intake-text">
            <span>Today's Intake Progress</span>
            <strong id="intakeRatio">0 / 0 Taken</strong>
          </div>
          <div class="progress-bar-bg">
            <div class="progress-bar-fill emerald" id="intakeBar" style="width: 0%;"></div>
          </div>
        </div>
      </section>

      <section class="history-section glass-panel">
        <div class="section-header">
          <h2>Activity & Dosage Log</h2>
          <button class="btn btn-secondary btn-sm" onclick="clearHistoryLog()">Clear Log</button>
        </div>
        <div class="table-wrapper">
          <table class="history-table">
            <thead>
              <tr>
                <th>Timestamp</th>
                <th>Medicine Name</th>
                <th>Scheduled Time</th>
                <th>Method</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody id="historyTableBody"></tbody>
          </table>
        </div>
      </section>
    </main>
  </div>

  <div class="modal-backdrop" id="alarmModal">
    <div class="modal-content glass-panel">
      <div class="modal-header">
        <h3 id="modalTitle">Add Medicine Timer</h3>
        <button class="close-btn" onclick="closeAlarmModal()">×</button>
      </div>
      <form id="alarmForm" onsubmit="saveAlarmSubmit(event)">
        <input type="hidden" id="formAlarmId" value="0" />
        <div class="form-group">
          <label>Medicine Name</label>
          <input type="text" id="formName" required placeholder="e.g. Paracetamol / BP Medicine" />
        </div>
        <div class="form-row">
          <div class="form-group">
            <label>Alarm Hour (0-23)</label>
            <input type="number" id="formHour" min="0" max="23" required placeholder="8" />
          </div>
          <div class="form-group">
            <label>Alarm Minute (0-59)</label>
            <input type="number" id="formMinute" min="0" max="59" required placeholder="00" />
          </div>
        </div>
        <div class="form-group">
          <label>Dosage & Instructions</label>
          <input type="text" id="formDosage" placeholder="e.g. 1 Tablet after food" />
        </div>
        <div class="form-group">
          <label>Pill Color Identifier</label>
          <div class="color-options">
            <label class="color-choice"><input type="radio" name="formColor" value="#3b82f6" checked /><span class="color-swatch" style="background:#3b82f6;"></span> Blue</label>
            <label class="color-choice"><input type="radio" name="formColor" value="#10b981" /><span class="color-swatch" style="background:#10b981;"></span> Green</label>
            <label class="color-choice"><input type="radio" name="formColor" value="#f59e0b" /><span class="color-swatch" style="background:#f59e0b;"></span> Amber</label>
            <label class="color-choice"><input type="radio" name="formColor" value="#ec4899" /><span class="color-swatch" style="background:#ec4899;"></span> Pink</label>
            <label class="color-choice"><input type="radio" name="formColor" value="#8b5cf6" /><span class="color-swatch" style="background:#8b5cf6;"></span> Purple</label>
          </div>
        </div>
        <div class="modal-footer">
          <button type="button" class="btn btn-secondary" onclick="closeAlarmModal()">Cancel</button>
          <button type="submit" class="btn btn-primary">Save Timer</button>
        </div>
      </form>
    </div>
  </div>

  <div class="modal-backdrop" id="configModal">
    <div class="modal-content glass-panel">
      <div class="modal-header">
        <h3>ESP32 Configuration</h3>
        <button class="close-btn" onclick="closeConfigModal()">×</button>
      </div>
      <form id="configForm" onsubmit="saveConfigSubmit(event)">
        <div class="form-group">
          <label>Connection Mode</label>
          <select id="cfgMode" onchange="toggleModeInputs()">
            <option value="tunnel">⚡ Cloudflare Tunnel (https://mediback.ugsidharth.in)</option>
            <option value="cloud">☁️ Cloud Sync (MQTT Broker)</option>
            <option value="esp32">📶 Local ESP32 IP Mode</option>
            <option value="demo">🎮 Simulation Mode</option>
          </select>
        </div>
        <div class="form-group" id="tunnelUrlGroup">
          <label>⚡ Cloudflare Tunnel URL</label>
          <input type="text" id="cfgTunnelUrl" placeholder="https://mediback.ugsidharth.in" />
        </div>
        <div class="form-group" id="espIpGroup">
          <label>ESP32 IP Address</label>
          <input type="text" id="cfgEspIp" placeholder="192.168.4.1" />
        </div>
        <div class="modal-footer">
          <button type="button" class="btn btn-secondary" onclick="closeConfigModal()">Close</button>
          <button type="submit" class="btn btn-emerald">Save Settings</button>
        </div>
      </form>
    </div>
  </div>

  <div class="toast-container" id="toastContainer"></div>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/mqtt/4.3.7/mqtt.min.js"></script>
  <script src="/app.js"></script>
</body>
</html>
)rawliteral";

#endif
