/* ==========================================================================
   SMART MEDBOX - APPLICATION LOGIC & CLOUD DOMAIN SYNC
   ========================================================================== */

// --- Global State ---
let config = {
  mode: localStorage.getItem('medbox_mode') || 'esp32',
  espIp: localStorage.getItem('medbox_esp_ip') || '192.168.31.190',
  pollInterval: 1500
};

let alarms = [
  { id: 1, name: "Morning BP Tablet", hour: 8, minute: 0, dosage: "1 Tablet after food", color: "#3b82f6", active: true },
  { id: 2, name: "Afternoon Antibiotic", hour: 13, minute: 30, dosage: "1 Capsule", color: "#10b981", active: true },
  { id: 3, name: "Evening Vitamin D", hour: 20, minute: 0, dosage: "2 Softgels", color: "#f59e0b", active: true }
];

let telemetry = {
  wifiConnected: true,
  ip: "192.168.4.1",
  rssi: -62,
  time: "08:00:00",
  date: "2026-07-28",
  distance: 35.4,
  boxOpen: false,
  isAlarmActive: false,
  activeAlarmName: "",
  greenLed: false,
  redLed: false,
  buzzer: false,
  takenCount: 1
};

let historyLog = JSON.parse(localStorage.getItem('medbox_history')) || [
  { timestamp: "08:02:15 AM", medicine: "Morning BP Tablet", time: "08:00 AM", method: "Ultrasonic Lid Open", status: "Taken" }
];

let pollTimer = null;
let clockTimer = null;

// --- Initialize App ---
document.addEventListener('DOMContentLoaded', () => {
  loadConfigToForm();
  renderAlarms();
  renderHistoryLog();
  startClock();
  initNetworkConnection();
});

// --- Clock & Countdown logic ---
function startClock() {
  if (clockTimer) clearInterval(clockTimer);
  clockTimer = setInterval(() => {
    const now = new Date();
    document.getElementById('headerTime').innerText = now.toLocaleTimeString();
    document.getElementById('headerDate').innerText = now.toLocaleDateString();

    updateCountdown();
  }, 1000);
}

function updateCountdown() {
  const now = new Date();
  const currentTotalSec = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds();

  let nextAlarm = null;
  let minDiffSec = Infinity;

  const activeAlarms = alarms.filter(a => a.active);
  if (activeAlarms.length === 0) {
    document.getElementById('nextMedName').innerText = "No Active Alarms";
    document.getElementById('nextMedDosage').innerText = "Add a timer to get started";
    document.getElementById('nextMedTime').innerText = "⏰ --:--";
    document.getElementById('cdHours').innerText = "00";
    document.getElementById('cdMins').innerText = "00";
    document.getElementById('cdSecs').innerText = "00";
    return;
  }

  activeAlarms.forEach(a => {
    const alarmTotalSec = a.hour * 3600 + a.minute * 60;
    let diffSec = alarmTotalSec - currentTotalSec;
    if (diffSec <= 0) diffSec += 86400;

    if (diffSec < minDiffSec) {
      minDiffSec = diffSec;
      nextAlarm = a;
    }
  });

  if (nextAlarm) {
    document.getElementById('nextMedName').innerText = nextAlarm.name;
    document.getElementById('nextMedDosage').innerText = nextAlarm.dosage || "1 Dose";
    document.getElementById('nextMedTime').innerText = `⏰ ${format12Hour(nextAlarm.hour, nextAlarm.minute)}`;

    const dot = document.getElementById('nextPillDot');
    const badge = document.getElementById('nextPillTag');
    if (dot) dot.style.background = nextAlarm.color;
    if (badge) {
      badge.style.background = `${nextAlarm.color}25`;
      badge.style.color = nextAlarm.color;
    }

    const hours = Math.floor(minDiffSec / 3600);
    const mins = Math.floor((minDiffSec % 3600) / 60);
    const secs = minDiffSec % 60;

    document.getElementById('cdHours').innerText = String(hours).padStart(2, '0');
    document.getElementById('cdMins').innerText = String(mins).padStart(2, '0');
    document.getElementById('cdSecs').innerText = String(secs).padStart(2, '0');
  }
}

function format12Hour(h, m) {
  const period = h >= 12 ? 'PM' : 'AM';
  const hour12 = h % 12 || 12;
  const minStr = String(m).padStart(2, '0');
  return `${hour12}:${minStr} ${period}`;
}

// --- Network Connection Logic ---
function initNetworkConnection() {
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }

  config.mode = localStorage.getItem('medbox_mode') || 'esp32';
  config.espIp = localStorage.getItem('medbox_esp_ip') || '192.168.31.190';

  if (config.mode === 'esp32') {
    initLocalEsp32Mode();
  } else {
    updateConnectionBadge(true, "🎮 Simulation Mode");
  }
}

function initLocalEsp32Mode() {
  updateConnectionBadge(true, `Connecting ${config.espIp}...`);
  fetchStatus();
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = setInterval(fetchStatus, config.pollInterval || 1500);
}

let fetchFailCount = 0;

async function fetchStatus() {
  if (config.mode !== 'esp32' || window.location.protocol === 'https:') return;
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 3000);

    const res = await fetch(`http://${config.espIp}/api/status`, {
      mode: 'cors',
      signal: controller.signal
    });
    clearTimeout(timeoutId);

    if (res.ok) {
      fetchFailCount = 0;
      const data = await res.json();
      telemetry = data;
      updateTelemetryUI(data);
      updateConnectionBadge(true, `Connected (${config.espIp})`);
      fetchAlarmsFromESP();
    } else {
      updateConnectionBadge(false, "ESP32 Unreachable");
    }
  } catch (err) {
    fetchFailCount++;
    updateConnectionBadge(false, `Offline (${config.espIp} Timed Out)`);
  }
}

async function fetchAlarmsFromESP() {
  if (config.mode !== 'esp32' || window.location.protocol === 'https:') return;
  try {
    const res = await fetch(`http://${config.espIp}/api/alarms`, { mode: 'cors' });
    if (res.ok) {
      const data = await res.json();
      alarms = data;
      renderAlarms();
    }
  } catch (e) {
    console.error("Failed to fetch alarms:", e);
  }
}

function updateConnectionBadge(isConnected, label) {
  const badge = document.getElementById('connectionBadge');
  const text = document.getElementById('connectionText');
  
  let cls = 'demo-mode';
  if (config.mode === 'cloud' || config.mode === 'esp32' || config.mode === 'tunnel') {
    cls = isConnected ? 'connected' : 'disconnected';
  }
  
  if (badge) badge.className = 'connection-badge ' + cls;
  if (text) text.innerText = label;
}

function updateTelemetryUI(data) {
  const distVal = document.getElementById('distanceValue');
  const distBar = document.getElementById('distanceBar');
  const stateBadge = document.getElementById('boxStateBadge');

  if (distVal) distVal.innerText = `${(data.distance || 0).toFixed(1)} cm`;
  if (distBar) {
    const pct = Math.min(100, Math.max(5, ((data.distance || 0) / 50) * 100));
    distBar.style.width = `${pct}%`;
  }
  if (stateBadge) {
    if (data.boxOpen || (data.distance > 0 && data.distance < 15)) {
      stateBadge.innerText = "👐 Box Lid OPEN (< 15cm) - Pill Taken Detection!";
      stateBadge.style.color = "#10b981";
    } else {
      stateBadge.innerText = "🔒 Box Lid Closed (> 15cm)";
      stateBadge.style.color = "#94a3b8";
    }
  }

  const buzzerText = document.getElementById('buzzerStateText');
  const redLedText = document.getElementById('redLedText');
  const greenLedText = document.getElementById('greenLedText');
  const rssiText = document.getElementById('rssiText');

  if (buzzerText) {
    if (data.buzzer || data.isAlarmActive) {
      buzzerText.innerText = "ACTIVE RINGING 🔊 (GPIO 25)";
      buzzerText.style.color = "#f43f5e";
    } else {
      buzzerText.innerText = "OFF (Active High)";
      buzzerText.style.color = "#94a3b8";
    }
  }
  if (redLedText) redLedText.innerText = data.redLed || data.isAlarmActive ? "FLASHING 🔴" : "OFF";
  if (greenLedText) greenLedText.innerText = data.greenLed ? "SOLID ON 🟢" : "OFF";
  if (rssiText) rssiText.innerText = `${data.rssi || -60} dBm`;

  if (data.time && document.getElementById('headerTime')) {
    document.getElementById('headerTime').innerText = data.time;
  }
  if (data.date && document.getElementById('headerDate')) {
    document.getElementById('headerDate').innerText = data.date;
  }
  if (data.nextMedName && data.nextMedName !== "No Alarms") {
    const nextNameEl = document.getElementById('nextMedName');
    const nextTimeEl = document.getElementById('nextMedTime');
    if (nextNameEl) nextNameEl.innerText = data.nextMedName;
    if (nextTimeEl) nextTimeEl.innerText = `⏰ ${data.nextMedTime}`;
  }

  const totalAlarms = alarms.length;
  const takenCount = data.takenCount || 0;
  document.getElementById('intakeRatio').innerText = `${takenCount} / ${totalAlarms} Taken`;
  const intakePct = totalAlarms > 0 ? Math.min(100, (takenCount / totalAlarms) * 100) : 0;
  document.getElementById('intakeBar').style.width = `${intakePct}%`;
}

// --- Render Alarms Grid ---
function renderAlarms() {
  const container = document.getElementById('timersGrid');
  if (!container) return;

  if (alarms.length === 0) {
    container.innerHTML = `<p style="color: var(--text-muted); text-align:center; padding: 20px;">No medicine alarms set. Click "+ Add Timer" to create one.</p>`;
    return;
  }

  container.innerHTML = alarms.map(a => `
    <div class="timer-card">
      <div class="timer-left">
        <div class="pill-color-indicator" style="background: ${a.color || '#3b82f6'};"></div>
        <div class="timer-details">
          <h4>${escapeHtml(a.name)}</h4>
          <p>${escapeHtml(a.dosage || '1 Dose')}</p>
        </div>
      </div>
      <div class="timer-right">
        <div class="timer-time">${format12Hour(a.hour, a.minute)}</div>
        <label class="switch">
          <input type="checkbox" ${a.active ? 'checked' : ''} onchange="toggleAlarmActive(${a.id}, this.checked)">
          <span class="slider"></span>
        </label>
        <button class="icon-btn" onclick="openEditModal(${a.id})" title="Edit">✏️</button>
        <button class="icon-btn" onclick="deleteAlarm(${a.id})" title="Delete">🗑️</button>
      </div>
    </div>
  `).join('');

  updateCountdown();
}

function escapeHtml(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// --- Actions & API Invocation ---
async function toggleAlarmActive(id, active) {
  const alarm = alarms.find(a => a.id === id);
  if (alarm) {
    alarm.active = active;
    if (config.mode === 'esp32') {
      await sendAlarmToESP(alarm);
    }
    showToast(`Timer "${alarm.name}" ${active ? 'enabled' : 'disabled'}`, 'info');
    updateCountdown();
  }
}

async function deleteAlarm(id) {
  if (!confirm("Are you sure you want to delete this medicine timer?")) return;

  alarms = alarms.filter(a => a.id !== id);
  if (config.mode === 'esp32') {
    try {
      await fetch(`http://${config.espIp}/api/alarms?id=${id}`, { method: 'DELETE', mode: 'cors' });
    } catch (e) {
      console.error("Delete failed:", e);
    }
  }
  renderAlarms();
  showToast("Medicine timer deleted", 'info');
}

async function triggerMarkTaken() {
  const now = new Date();
  const timeStr = now.toLocaleTimeString();

  const nextMedName = document.getElementById('nextMedName').innerText;

  historyLog.unshift({
    timestamp: timeStr,
    medicine: nextMedName !== 'No Active Alarms' ? nextMedName : 'General Medicine',
    time: now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
    method: "Dashboard Button",
    status: "Taken"
  });
  saveHistoryLog();
  renderHistoryLog();

  telemetry.greenLed = true;
  telemetry.isAlarmActive = false;
  telemetry.takenCount = (telemetry.takenCount || 0) + 1;
  updateTelemetryUI(telemetry);

  setTimeout(() => {
    telemetry.greenLed = false;
    updateTelemetryUI(telemetry);
  }, 3000);

  if (config.mode === 'esp32') {
    try {
      await fetch(`http://${config.espIp}/api/take`, { method: 'POST', mode: 'cors' });
    } catch (e) { console.error(e); }
  }

  showToast("✓ Medicine intake recorded!", 'success');
}

function playBrowserAudioBeep(durationMs = 1000) {
  try {
    const AudioCtx = window.AudioContext || window.webkitAudioContext;
    if (!AudioCtx) return;
    const ctx = new AudioCtx();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();

    osc.type = 'square';
    osc.frequency.setValueAtTime(2700, ctx.currentTime);

    gain.gain.setValueAtTime(0.15, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + (durationMs / 1000));

    osc.connect(gain);
    gain.connect(ctx.destination);

    osc.start();
    osc.stop(ctx.currentTime + (durationMs / 1000));
  } catch (e) {}
}

async function triggerTestBuzzer() {
  showToast("🔔 Active Buzzer (GPIO 25) & LED Test Triggered", 'info');
  playBrowserAudioBeep(1000);
  telemetry.redLed = true;
  telemetry.greenLed = true;
  telemetry.buzzer = true;
  updateTelemetryUI(telemetry);

  setTimeout(() => {
    telemetry.redLed = false;
    telemetry.greenLed = false;
    telemetry.buzzer = false;
    updateTelemetryUI(telemetry);
  }, 1000);

  if (config.mode === 'esp32') {
    try {
      await fetch(`http://${config.espIp}/api/test-alarm`, { method: 'POST', mode: 'cors' });
    } catch (e) { console.error(e); }
  }
}

// --- Alarm Modal Management ---
function openAddModal() {
  document.getElementById('modalTitle').innerText = "Add Medicine Timer";
  document.getElementById('formAlarmId').value = "0";
  document.getElementById('formName').value = "";
  document.getElementById('formHour').value = "";
  document.getElementById('formMinute').value = "";
  document.getElementById('formDosage').value = "";
  document.getElementById('alarmModal').classList.add('active');
}

function openEditModal(id) {
  const alarm = alarms.find(a => a.id === id);
  if (!alarm) return;

  document.getElementById('modalTitle').innerText = "Edit Medicine Timer";
  document.getElementById('formAlarmId').value = alarm.id;
  document.getElementById('formName').value = alarm.name;
  document.getElementById('formHour').value = alarm.hour;
  document.getElementById('formMinute').value = alarm.minute;
  document.getElementById('formDosage').value = alarm.dosage || "";

  const radios = document.getElementsByName('formColor');
  for (let r of radios) {
    if (r.value === alarm.color) r.checked = true;
  }

  document.getElementById('alarmModal').classList.add('active');
}

function closeAlarmModal() {
  document.getElementById('alarmModal').classList.remove('active');
}

async function saveAlarmSubmit(event) {
  event.preventDefault();
  const id = parseInt(document.getElementById('formAlarmId').value) || 0;
  const name = document.getElementById('formName').value.trim();
  const hour = parseInt(document.getElementById('formHour').value);
  const minute = parseInt(document.getElementById('formMinute').value);
  const dosage = document.getElementById('formDosage').value.trim() || "1 Dose";
  const color = document.querySelector('input[name="formColor"]:checked').value;

  if (isNaN(hour) || hour < 0 || hour > 23 || isNaN(minute) || minute < 0 || minute > 59) {
    showToast("Please enter valid hour (0-23) and minute (0-59)", "error");
    return;
  }

  let alarm;
  if (id > 0) {
    alarm = alarms.find(a => a.id === id);
    if (alarm) {
      alarm.name = name;
      alarm.hour = hour;
      alarm.minute = minute;
      alarm.dosage = dosage;
      alarm.color = color;
    }
  } else {
    const newId = alarms.length > 0 ? Math.max(...alarms.map(a => a.id)) + 1 : 1;
    alarm = { id: newId, name, hour, minute, dosage, color, active: true };
    alarms.push(alarm);
  }

  if (config.mode === 'cloud') {
    publishMqttMessage(MQTT_TOPIC_ALARMS, alarms);
  } else if (config.mode === 'tunnel' || config.mode === 'esp32') {
    await sendAlarmToESP(alarm);
  }

  closeAlarmModal();
  renderAlarms();
  showToast(id > 0 ? "Timer updated successfully!" : "New medicine timer created!", "success");
}

async function sendAlarmToESP(alarm) {
  const formData = new URLSearchParams();
  formData.append('id', alarm.id);
  formData.append('name', alarm.name);
  formData.append('hour', alarm.hour);
  formData.append('minute', alarm.minute);
  formData.append('dosage', alarm.dosage);
  formData.append('color', alarm.color);
  formData.append('active', alarm.active ? '1' : '0');

  let targetUrl = '';
  if (config.mode === 'tunnel' && config.cleanTunnelUrl) {
    targetUrl = `${config.cleanTunnelUrl}/api/alarms`;
  } else if (config.mode === 'esp32' && window.location.protocol !== 'https:') {
    targetUrl = `http://${config.espIp}/api/alarms`;
  } else {
    return;
  }

  try {
    await fetch(targetUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formData,
      mode: 'cors'
    });
  } catch (e) {
    console.error("Failed to send alarm to ESP32:", e);
  }
}

// --- History Log Management ---
function renderHistoryLog() {
  const tbody = document.getElementById('historyTableBody');
  if (!tbody) return;

  if (historyLog.length === 0) {
    tbody.innerHTML = `<tr><td colspan="5" style="text-align:center; color: var(--text-muted);">No activity recorded yet today.</td></tr>`;
    return;
  }

  tbody.innerHTML = historyLog.map(item => `
    <tr>
      <td>${item.timestamp}</td>
      <td><strong>${escapeHtml(item.medicine)}</strong></td>
      <td>${item.time}</td>
      <td>${item.method}</td>
      <td><span class="status-tag ${item.status.toLowerCase()}">${item.status}</span></td>
    </tr>
  `).join('');
}

function saveHistoryLog() {
  localStorage.setItem('medbox_history', JSON.stringify(historyLog.slice(0, 50)));
}

function clearHistoryLog() {
  historyLog = [];
  saveHistoryLog();
  renderHistoryLog();
  showToast("History log cleared", "info");
}

// --- Config Modal Management ---
function openConfigModal() {
  document.getElementById('configModal').classList.add('active');
}

function closeConfigModal() {
  document.getElementById('configModal').classList.remove('active');
}

function loadConfigToForm() {
  const modeEl = document.getElementById('cfgMode');
  const ipEl = document.getElementById('cfgEspIp');

  if (modeEl) modeEl.value = config.mode || 'cloud';
  if (ipEl) ipEl.value = config.espIp || '192.168.31.190';
  toggleModeInputs();
}

function toggleModeInputs() {
  const modeEl = document.getElementById('cfgMode');
  const ipGroup = document.getElementById('espIpGroup');
  const mode = modeEl ? modeEl.value : 'cloud';

  if (ipGroup) ipGroup.style.display = (mode === 'esp32') ? 'block' : 'none';
}

async function saveConfigSubmit(e) {
  if (e && e.preventDefault) e.preventDefault();
  const mode = document.getElementById('cfgMode').value;
  const espIp = document.getElementById('cfgEspIp').value.trim();

  config.mode = mode;
  config.espIp = espIp || '192.168.31.190';

  localStorage.setItem('medbox_mode', mode);
  localStorage.setItem('medbox_esp_ip', config.espIp);

  closeConfigModal();
  initNetworkConnection();
  showToast(`Settings Saved (${mode.toUpperCase()} mode)`, "success");
}

// --- Toast Utilities ---
function showToast(message, type = 'info') {
  const container = document.getElementById('toastContainer');
  if (!container) return;

  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  toast.innerText = message;

  container.appendChild(toast);

  setTimeout(() => {
    toast.style.opacity = '0';
    toast.style.transform = 'translateX(100%)';
    setTimeout(() => toast.remove(), 300);
  }, 3000);
}
