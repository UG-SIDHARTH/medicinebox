/*
  ===========================================================================
  Smart Medicine Reminder Box - ESP32 Firmware
  ===========================================================================
  Features:
  - Multi-alarm scheduling stored in Preferences (NVS storage)
  - RESTful JSON API for modern Web Application control
  - CORS support for cross-origin local web server development
  - Automatic WiFi & NTP Time Synchronization (+5:30 IST default)
  - Access Point (AP) mode fallback (MedBox-Setup @ 192.168.4.1)
  - HC-SR04 Ultrasonic Sensor (<15cm detection for medicine intake)
  - I2C 16x2 LCD display with smooth status marquee
  - Hardware button hold-to-reset WiFi memory function
  - Active buzzer & dual LED (Red/Green) notification states
  ===========================================================================
*/

#include "time.h"
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

// ==========================================
// 1. HARDWARE PINS & PERIPHERALS
// ==========================================
const int buzzerPin = 25;
const int redLedPin = 26;
const int greenLedPin = 27;
const int trigPin = 12;
const int echoPin = 14;
const int buttonPin = 13;

LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences preferences;
WebServer server(80);

// ==========================================
// 2. DATA STRUCTURES & CONSTANTS
// ==========================================
#define MAX_ALARMS 10

struct Alarm {
  int id;
  char name[32];
  int hour;
  int minute;
  char dosage[32];
  char color[10];
  bool active;
  bool triggeredToday;
};

Alarm alarms[MAX_ALARMS];
int alarmCount = 0;
int nextAlarmId = 1;
int takenCountToday = 0;

// WiFi & System Config (Locked to GMT+5:30 IST: 5h 30m = 19800 seconds)
String savedSSID = "";
String savedPass = "";
const long gmtOffset_sec = 19800; // GMT+5:30 IST Offset (19,800 seconds)
const int daylightOffset_sec = 0;
const char *ntpServer = "pool.ntp.org";

// System State
bool setupMode = false;
bool isAlarmActive = false;
int activeAlarmIndex = -1;
unsigned long lastDistanceCheck = 0;
long currentDistanceCm = 999;
bool greenLedState = false;
bool redLedState = false;
bool buzzerState = false;

// MQTT Cloud Config for medicinebox.ugsidharth.in
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_topic_telemetry = "ug_sidharth/medbox/telemetry";
const char *mqtt_topic_commands = "ug_sidharth/medbox/commands";
const char *mqtt_topic_alarms = "ug_sidharth/medbox/alarms";
unsigned long lastMqttPublish = 0;

// Display & Button Timing
unsigned long lastScrollTime = 0;
int scrollIndex = 0;
unsigned long buttonPressStartTime = 0;
bool buttonIsPressed = false;
bool showIpOnLcd = false;
const int OPEN_DISTANCE_THRESHOLD_CM = 15;

// ==========================================
// 3. HELPER FUNCTIONS & SENSOR READING
// ==========================================
long readUltrasonicDistanceCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  if (duration == 0)
    return 999; // Out of range / timeout
  return duration * 0.034 / 2;
}

void setCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods",
                    "GET, POST, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers",
                    "Content-Type, Authorization");
}

void saveAlarmsToNVS() {
  preferences.putInt("count", alarmCount);
  preferences.putInt("nextId", nextAlarmId);
  for (int i = 0; i < alarmCount; i++) {
    char key[16];
    snprintf(key, sizeof(key), "alm_%d", i);
    preferences.putBytes(key, &alarms[i], sizeof(Alarm));
  }
}

void loadAlarmsFromNVS() {
  alarmCount = preferences.getInt("count", 0);
  nextAlarmId = preferences.getInt("nextId", 1);
  if (alarmCount > MAX_ALARMS)
    alarmCount = MAX_ALARMS;

  for (int i = 0; i < alarmCount; i++) {
    char key[16];
    snprintf(key, sizeof(key), "alm_%d", i);
    preferences.getBytes(key, &alarms[i], sizeof(Alarm));
    alarms[i].triggeredToday = false;
  }

  // If no alarms exist, load a default demo alarm
  if (alarmCount == 0) {
    alarms[0].id = 1;
    strncpy(alarms[0].name, "Morning Medicine", 32);
    alarms[0].hour = 8;
    alarms[0].minute = 0;
    strncpy(alarms[0].dosage, "1 Tablet after food", 32);
    strncpy(alarms[0].color, "#3b82f6", 10);
    alarms[0].active = true;
    alarms[0].triggeredToday = false;
    alarmCount = 1;
    nextAlarmId = 2;
    saveAlarmsToNVS();
  }
}

// ==========================================
// 4. API ENDPOINTS HANDLERS
// ==========================================

void handleCORSPreflight() {
  setCORSHeaders();
  server.send(204);
}

void handleGetStatus() {
  setCORSHeaders();
  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char timeStr[16] = "00:00:00";
  char dateStr[16] = "1970-01-01";
  int hour = 0, minute = 0, second = 0;

  if (hasTime) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour,
             timeinfo.tm_min, timeinfo.tm_sec);
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
  }

  currentDistanceCm = readUltrasonicDistanceCm();
  bool boxOpen =
      (currentDistanceCm > 0 && currentDistanceCm < OPEN_DISTANCE_THRESHOLD_CM);

  String activeAlarmName = "";
  if (isAlarmActive && activeAlarmIndex >= 0 && activeAlarmIndex < alarmCount) {
    activeAlarmName = String(alarms[activeAlarmIndex].name);
  }

  String json = "{";
  json += "\"wifiConnected\":" +
          String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ip\":\"" +
          (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString()
                                         : WiFi.softAPIP().toString()) +
          "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"time\":\"" + String(timeStr) + "\",";
  json += "\"date\":\"" + String(dateStr) + "\",";
  json += "\"hour\":" + String(hour) + ",";
  json += "\"minute\":" + String(minute) + ",";
  json += "\"second\":" + String(second) + ",";
  json += "\"distance\":" + String(currentDistanceCm) + ",";
  json += "\"boxOpen\":" + String(boxOpen ? "true" : "false") + ",";
  json += "\"isAlarmActive\":" + String(isAlarmActive ? "true" : "false") + ",";
  json += "\"activeAlarmName\":\"" + activeAlarmName + "\",";
  json +=
      "\"greenLed\":" + String(digitalRead(greenLedPin) ? "true" : "false") +
      ",";
  json += "\"redLed\":" + String(redLedState ? "true" : "false") + ",";
  json += "\"buzzer\":" + String(buzzerState ? "true" : "false") + ",";
  json += "\"setupMode\":" + String(setupMode ? "true" : "false") + ",";
  json += "\"alarmCount\":" + String(alarmCount) + ",";
  json += "\"takenCount\":" + String(takenCountToday);
  json += "}";

  server.send(200, "application/json", json);
}

void handleGetAlarms() {
  setCORSHeaders();
  String json = "[";
  for (int i = 0; i < alarmCount; i++) {
    if (i > 0)
      json += ",";
    json += "{";
    json += "\"id\":" + String(alarms[i].id) + ",";
    json += "\"name\":\"" + String(alarms[i].name) + "\",";
    json += "\"hour\":" + String(alarms[i].hour) + ",";
    json += "\"minute\":" + String(alarms[i].minute) + ",";
    json += "\"dosage\":\"" + String(alarms[i].dosage) + "\",";
    json += "\"color\":\"" + String(alarms[i].color) + "\",";
    json += "\"active\":" + String(alarms[i].active ? "true" : "false");
    json += "}";
  }
  json += "]";

  server.send(200, "application/json", json);
}

void handleSaveAlarm() {
  setCORSHeaders();
  if (!server.hasArg("plain")) {
    if (server.hasArg("name") && server.hasArg("hour") &&
        server.hasArg("minute")) {
      int id = server.hasArg("id") ? server.arg("id").toInt() : 0;
      int existingIdx = -1;
      if (id > 0) {
        for (int i = 0; i < alarmCount; i++) {
          if (alarms[i].id == id) {
            existingIdx = i;
            break;
          }
        }
      }

      int idx = (existingIdx >= 0) ? existingIdx : alarmCount;
      if (idx >= MAX_ALARMS) {
        server.send(400, "application/json",
                    "{\"error\":\"Max alarms limit reached\"}");
        return;
      }

      if (existingIdx < 0) {
        alarms[idx].id = nextAlarmId++;
        alarmCount++;
      }

      strncpy(alarms[idx].name, server.arg("name").c_str(), 32);
      alarms[idx].hour = server.arg("hour").toInt();
      alarms[idx].minute = server.arg("minute").toInt();
      strncpy(alarms[idx].dosage,
              server.hasArg("dosage") ? server.arg("dosage").c_str() : "1 Dose",
              32);
      strncpy(alarms[idx].color,
              server.hasArg("color") ? server.arg("color").c_str() : "#3b82f6",
              10);
      alarms[idx].active =
          server.hasArg("active")
              ? (server.arg("active") == "true" || server.arg("active") == "1")
              : true;
      alarms[idx].triggeredToday = false;

      saveAlarmsToNVS();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }

  String body = server.arg("plain");
  if (body.length() > 0) {
    int idVal = 0;
    if (body.indexOf("\"id\":") != -1) {
      int idStart = body.indexOf("\"id\":") + 5;
      idVal = body.substring(idStart, body.indexOf(",", idStart)).toInt();
    }

    String nameVal = "Medicine";
    if (body.indexOf("\"name\":\"") != -1) {
      int s = body.indexOf("\"name\":\"") + 8;
      nameVal = body.substring(s, body.indexOf("\"", s));
    }

    int hourVal = 8;
    if (body.indexOf("\"hour\":") != -1) {
      int s = body.indexOf("\"hour\":") + 7;
      hourVal = body.substring(s, body.indexOf(",", s)).toInt();
    }

    int minVal = 0;
    if (body.indexOf("\"minute\":") != -1) {
      int s = body.indexOf("\"minute\":") + 9;
      minVal = body.substring(s, body.indexOf(",", s)).toInt();
    }

    String dosageVal = "1 Tablet";
    if (body.indexOf("\"dosage\":\"") != -1) {
      int s = body.indexOf("\"dosage\":\"") + 10;
      dosageVal = body.substring(s, body.indexOf("\"", s));
    }

    String colorVal = "#3b82f6";
    if (body.indexOf("\"color\":\"") != -1) {
      int s = body.indexOf("\"color\":\"") + 9;
      colorVal = body.substring(s, body.indexOf("\"", s));
    }

    bool activeVal = (body.indexOf("\"active\":false") == -1);

    int existingIdx = -1;
    if (idVal > 0) {
      for (int i = 0; i < alarmCount; i++) {
        if (alarms[i].id == idVal) {
          existingIdx = i;
          break;
        }
      }
    }

    int idx = (existingIdx >= 0) ? existingIdx : alarmCount;
    if (idx >= MAX_ALARMS) {
      server.send(400, "application/json",
                  "{\"error\":\"Max alarms limit reached\"}");
      return;
    }

    if (existingIdx < 0) {
      alarms[idx].id = nextAlarmId++;
      alarmCount++;
    }

    strncpy(alarms[idx].name, nameVal.c_str(), 32);
    alarms[idx].hour = hourVal;
    alarms[idx].minute = minVal;
    strncpy(alarms[idx].dosage, dosageVal.c_str(), 32);
    strncpy(alarms[idx].color, colorVal.c_str(), 10);
    alarms[idx].active = activeVal;
    alarms[idx].triggeredToday = false;

    saveAlarmsToNVS();
    server.send(200, "application/json", "{\"success\":true}");
    return;
  }

  server.send(400, "application/json", "{\"error\":\"Invalid arguments\"}");
}

void handleDeleteAlarm() {
  setCORSHeaders();
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    int foundIdx = -1;
    for (int i = 0; i < alarmCount; i++) {
      if (alarms[i].id == id) {
        foundIdx = i;
        break;
      }
    }

    if (foundIdx >= 0) {
      for (int i = foundIdx; i < alarmCount - 1; i++) {
        alarms[i] = alarms[i + 1];
      }
      alarmCount--;
      saveAlarmsToNVS();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  server.send(400, "application/json", "{\"error\":\"Alarm ID not found\"}");
}

void handleTakeMedicine() {
  setCORSHeaders();
  isAlarmActive = false;
  activeAlarmIndex = -1;
  noTone(buzzerPin);
  buzzerState = false;
  redLedState = false;
  digitalWrite(redLedPin, LOW);

  digitalWrite(greenLedPin, HIGH);
  takenCountToday++;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Medicine Taken!");
  lcd.setCursor(0, 1);
  lcd.print("Great job!");

  server.send(200, "application/json",
              "{\"success\":true,\"message\":\"Medicine intake recorded\"}");
}

void handleTestAlarm() {
  setCORSHeaders();
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, HIGH);
  tone(buzzerPin, 1000);
  delay(1000);
  noTone(buzzerPin);
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);

  server.send(200, "application/json",
              "{\"success\":true,\"message\":\"Test triggered\"}");
}

void handleSaveConfig() {
  setCORSHeaders();
  String ssid = "";
  String pass = "";

  if (server.hasArg("ssid"))
    ssid = server.arg("ssid");
  if (server.hasArg("pass"))
    pass = server.arg("pass");

  if (ssid != "") {
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);

    String responseHtml = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Saving Wi-Fi...</title>
  <style>
    body { font-family: system-ui, sans-serif; background: #0f172a; color: #f8fafc; text-align: center; padding: 40px 20px; }
    .card { background: #1e293b; border-radius: 16px; padding: 30px; max-width: 450px; margin: 0 auto; box-shadow: 0 4px 16px rgba(0,0,0,0.4); }
    h2 { color: #38bdf8; }
    p { color: #94a3b8; }
  </style>
</head>
<body>
  <div class="card">
    <h2>✓ Settings Saved!</h2>
    <p>Connecting to Wi-Fi network: <strong>)rawliteral" +
                          ssid + R"rawliteral(</strong></p>
    <p>Restarting Smart MedBox in 2 seconds...</p>
  </div>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", responseHtml);
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html",
                "<h3>Error: Please select a Wi-Fi network</h3>");
  }
}

void handleScanWiFi() {
  setCORSHeaders();
  // Fast active scan: 80ms per channel (under 1 second total!)
  int n = WiFi.scanNetworks(false, false, false, 80);
  String json = "[";
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      if (i > 0)
        json += ",";
      json += "{";
      json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i));
      json += "}";
    }
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleRoot() {
  setCORSHeaders();
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart MedBox Setup</title>
  <style>
    body { font-family: system-ui, sans-serif; background: #0f172a; color: #f8fafc; padding: 20px; text-align: center; }
    .card { background: #1e293b; border-radius: 16px; padding: 24px; margin: 15px auto; max-width: 480px; box-shadow: 0 4px 16px rgba(0,0,0,0.4); text-align: left; }
    h1 { color: #38bdf8; font-size: 1.8rem; margin-bottom: 5px; text-align: center; }
    p.subtitle { color: #94a3b8; font-size: 0.9rem; text-align: center; margin-bottom: 20px; }
    .status { font-size: 1.05rem; text-align: center; margin: 10px 0; color: #4ade80; background: rgba(74,222,128,0.1); padding: 8px; border-radius: 8px; }
    .form-group { margin-bottom: 16px; }
    label { display: block; font-weight: 600; font-size: 0.85rem; color: #cbd5e1; margin-bottom: 6px; }
    input[type="text"], input[type="password"], select { width: 100%; padding: 12px; background: #0f172a; border: 1px solid #334155; border-radius: 8px; color: #fff; font-size: 1rem; box-sizing: border-box; }
    select option { background: #0f172a; color: #fff; }
    .divider-text { text-align: center; color: #64748b; font-size: 0.8rem; margin: 10px 0; font-weight: 600; }
    .btn { background: #0284c7; color: white; border: none; padding: 12px 20px; border-radius: 8px; font-weight: bold; cursor: pointer; width: 100%; font-size: 1rem; margin-top: 10px; }
    .btn:hover { background: #0369a1; }
    .btn-secondary { background: #334155; margin-top: 8px; }
    .btn-secondary:hover { background: #475569; }
    .btn-danger { background: #e11d48; margin-top: 8px; }
    .btn-danger:hover { background: #be123c; }
    .row-btns { display: flex; gap: 10px; margin-top: 15px; }
    .row-btns .btn { width: 50%; }
    hr { border: 0; height: 1px; background: #334155; margin: 20px 0; }
  </style>
</head>
<body>
  <div class="card">
    <h1>💊 Smart MedBox</h1>
    <p class="subtitle">Wi-Fi Setup & Device Management</p>
    <div id="time" class="status">Loading ESP32 Status...</div>

    <form action="/save" method="POST">
      <div class="form-group">
        <label>📶 Select Scanned Wi-Fi Network:</label>
        <select id="ssidSelect" onchange="if(this.value) document.getElementById('ssidInput').value = this.value;">
          <option value="">⏳ Scanning (Fast <1s)...</option>
        </select>
      </div>

      <div class="form-group">
        <label>✍️ Wi-Fi Network Name (SSID):</label>
        <input type="text" name="ssid" id="ssidInput" placeholder="e.g. UG_SIDHARTH" required>
      </div>

      <div class="form-group">
        <label>🔑 Wi-Fi Password:</label>
        <input type="password" name="pass" placeholder="Enter Wi-Fi Password" required>
      </div>

      <input type="submit" class="btn" value="Save & Connect Wi-Fi">
    </form>

    <hr>

    <div class="row-btns">
      <button class="btn btn-secondary" onclick="fetch('/api/test-alarm', {method:'POST'})">Test Alert</button>
      <button class="btn btn-danger" onclick="fetch('/api/take', {method:'POST'})">Mark Taken</button>
    </div>
  </div>

  <script>
    function loadWifiNetworks() {
      const select = document.getElementById('ssidSelect');
      const input = document.getElementById('ssidInput');
      fetch('/api/scan-wifi')
        .then(r => r.json())
        .then(networks => {
          if (!networks || networks.length === 0) {
            select.innerHTML = '<option value="">No Wi-Fi networks found</option>';
          } else {
            select.innerHTML = '<option value="">-- Choose Scanned Network --</option>' + 
              networks.map(n => `<option value="${n.ssid}">${n.ssid} (${n.rssi} dBm)</option>`).join('');
          }
        })
        .catch(() => {
          select.innerHTML = '<option value="">Scan failed - Type name below</option>';
        });
    }

    loadWifiNetworks();

    setInterval(() => {
      fetch('/api/status').then(r=>r.json()).then(d=>{
        document.getElementById('time').innerText = 'ESP32 Time: ' + d.time + ' | ' + (d.wifiConnected ? 'WiFi Connected' : 'AP Setup Mode');
      }).catch(()=>{});
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void publishStatusMQTT() {
  if (!mqttClient.connected()) return;

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);
  char timeStr[16] = "00:00:00";
  char dateStr[16] = "1970-01-01";
  int hour = 0, minute = 0, second = 0;

  if (hasTime) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
  }

  bool boxOpen = (currentDistanceCm > 0 && currentDistanceCm < OPEN_DISTANCE_THRESHOLD_CM);
  String activeAlarmName = "";
  if (isAlarmActive && activeAlarmIndex >= 0 && activeAlarmIndex < alarmCount) {
    activeAlarmName = String(alarms[activeAlarmIndex].name);
  }

  String json = "{";
  json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"time\":\"" + String(timeStr) + "\",";
  json += "\"date\":\"" + String(dateStr) + "\",";
  json += "\"hour\":" + String(hour) + ",";
  json += "\"minute\":" + String(minute) + ",";
  json += "\"second\":" + String(second) + ",";
  json += "\"distance\":" + String(currentDistanceCm) + ",";
  json += "\"boxOpen\":" + String(boxOpen ? "true" : "false") + ",";
  json += "\"isAlarmActive\":" + String(isAlarmActive ? "true" : "false") + ",";
  json += "\"activeAlarmName\":\"" + activeAlarmName + "\",";
  json += "\"greenLed\":" + String(digitalRead(greenLedPin) ? "true" : "false") + ",";
  json += "\"redLed\":" + String(redLedState ? "true" : "false") + ",";
  json += "\"buzzer\":" + String(buzzerState ? "true" : "false") + ",";
  json += "\"setupMode\":" + String(setupMode ? "true" : "false") + ",";
  json += "\"alarmCount\":" + String(alarmCount) + ",";
  json += "\"takenCount\":" + String(takenCountToday);
  json += "}";

  mqttClient.publish(mqtt_topic_telemetry, json.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == mqtt_topic_commands) {
    if (message.indexOf("take") != -1) {
      isAlarmActive = false;
      activeAlarmIndex = -1;
      noTone(buzzerPin);
      buzzerState = false;
      redLedState = false;
      digitalWrite(redLedPin, LOW);
      digitalWrite(greenLedPin, HIGH);
      takenCountToday++;
    } else if (message.indexOf("test_alarm") != -1) {
      digitalWrite(redLedPin, HIGH);
      digitalWrite(greenLedPin, HIGH);
      tone(buzzerPin, 1000);
      delay(1000);
      noTone(buzzerPin);
      digitalWrite(redLedPin, LOW);
      digitalWrite(greenLedPin, LOW);
    }
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  static unsigned long lastReconnectAttempt = 0;
  if (millis() - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = millis();
    String clientId = "ESP32MedBox-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      mqttClient.subscribe(mqtt_topic_commands);
      mqttClient.subscribe(mqtt_topic_alarms);
    }
  }
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(buzzerPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  noTone(buzzerPin);
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart MedBox");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  preferences.begin("medbox", false);
  savedSSID = preferences.getString("ssid", "");
  savedPass = preferences.getString("pass", "");

  loadAlarmsFromNVS();

  if (savedSSID == "") {
    setupMode = true;
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi:");
    lcd.setCursor(0, 1);
    lcd.print(savedSSID.substring(0, 16));

    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
      setupMode = true;
    }
  }

  if (setupMode) {
    Serial.println("Starting Soft AP Mode...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("MedBox-Setup");
    IPAddress apIP = WiFi.softAPIP();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AP: MedBox-Setup");
    lcd.setCursor(0, 1);
    lcd.print(apIP.toString());
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000);
    lcd.clear();
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSaveConfig);
  server.on("/api/scan-wifi", HTTP_GET, handleScanWiFi);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/alarms", HTTP_GET, handleGetAlarms);
  server.on("/api/alarms", HTTP_POST, handleSaveAlarm);
  server.on("/api/alarms", HTTP_DELETE, handleDeleteAlarm);
  server.on("/api/take", HTTP_POST, handleTakeMedicine);
  server.on("/api/test-alarm", HTTP_POST, handleTestAlarm);
  server.on("/api/config", HTTP_POST, handleSaveConfig);

  server.on("/api/scan-wifi", HTTP_OPTIONS, handleCORSPreflight);
  server.on("/api/status", HTTP_OPTIONS, handleCORSPreflight);
  server.on("/api/alarms", HTTP_OPTIONS, handleCORSPreflight);
  server.on("/api/take", HTTP_OPTIONS, handleCORSPreflight);
  server.on("/api/test-alarm", HTTP_OPTIONS, handleCORSPreflight);
  server.on("/api/config", HTTP_OPTIONS, handleCORSPreflight);

  server.begin();
  Serial.println("HTTP Server Started.");

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

// ==========================================
// 6. MAIN LOOP
// ==========================================
void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();

    if (millis() - lastMqttPublish > 1000) {
      lastMqttPublish = millis();
      publishStatusMQTT();
    }
  }

  // --- HARDWARE BUTTON LOGIC (Single Click = Toggle IP View, Hold 3s = Memory
  // Reset) ---
  if (digitalRead(buttonPin) == LOW) {
    if (!buttonIsPressed) {
      buttonIsPressed = true;
      buttonPressStartTime = millis();
    }

    unsigned long pressDuration = millis() - buttonPressStartTime;

    if (pressDuration > 3000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Memory Cleared!");
      lcd.setCursor(0, 1);
      lcd.print("Resetting...");
      preferences.clear();
      delay(2000);
      ESP.restart();
    }
  } else {
    if (buttonIsPressed) {
      buttonIsPressed = false;
      unsigned long pressDuration = millis() - buttonPressStartTime;
      if (pressDuration < 3000) {
        // Toggle IP Address display mode on single click
        showIpOnLcd = !showIpOnLcd;
        lcd.clear();
      }
    }
  }

  struct tm timeinfo;
  bool timeSynced = getLocalTime(&timeinfo);

  if (timeSynced) {
    int currentHour = timeinfo.tm_hour;
    int currentMin = timeinfo.tm_min;
    int currentSec = timeinfo.tm_sec;

    if (currentHour == 0 && currentMin == 0 && currentSec == 0) {
      for (int i = 0; i < alarmCount; i++) {
        alarms[i].triggeredToday = false;
      }
      takenCountToday = 0;
    }

    if (!isAlarmActive) {
      for (int i = 0; i < alarmCount; i++) {
        if (alarms[i].active && !alarms[i].triggeredToday) {
          if (alarms[i].hour == currentHour && alarms[i].minute == currentMin) {
            isAlarmActive = true;
            activeAlarmIndex = i;
            alarms[i].triggeredToday = true;
            digitalWrite(greenLedPin, LOW);
            break;
          }
        }
      }
    }

    if (!isAlarmActive && millis() - lastScrollTime > 400) {
      lastScrollTime = millis();

      if (showIpOnLcd) {
        // Mode 1: Display IP Address on LCD
        lcd.setCursor(0, 0);
        lcd.print("IP Address:     ");
        lcd.setCursor(0, 1);
        String ipStr = (WiFi.status() == WL_CONNECTED)
                           ? WiFi.localIP().toString()
                           : WiFi.softAPIP().toString();
        char ipBuf[17];
        snprintf(ipBuf, sizeof(ipBuf), "%-16.16s", ipStr.c_str());
        lcd.print(ipBuf);
      } else {
        // Mode 2: Display Normal Time & Medicine Scrolling Marquee
        char marqueeBuffer[40];

        String nextMedInfo = "No Active Alarms";
        int minDiff = 99999;
        for (int i = 0; i < alarmCount; i++) {
          if (alarms[i].active) {
            int alarmTotalMin = alarms[i].hour * 60 + alarms[i].minute;
            int currTotalMin = currentHour * 60 + currentMin;
            int diff = alarmTotalMin - currTotalMin;
            if (diff < 0)
              diff += 1440;
            if (diff < minDiff) {
              minDiff = diff;
              char buf[32];
              snprintf(buf, sizeof(buf), "%s @ %02d:%02d", alarms[i].name,
                       alarms[i].hour, alarms[i].minute);
              nextMedInfo = String(buf);
            }
          }
        }

        snprintf(marqueeBuffer, sizeof(marqueeBuffer),
                 " Time %02d:%02d:%02d | Next: %s   ", currentHour, currentMin,
                 currentSec, nextMedInfo.c_str());

        String scrollStr = String(marqueeBuffer);
        if (scrollIndex > scrollStr.length() - 16)
          scrollIndex = 0;

        lcd.setCursor(0, 0);
        lcd.print(scrollStr.substring(scrollIndex, scrollIndex + 16));
        scrollIndex++;

        lcd.setCursor(0, 1);
        char statusLine[17];
        if (WiFi.status() == WL_CONNECTED) {
          snprintf(statusLine, sizeof(statusLine), "MedBox OK: %02d:%02d",
                   currentHour, currentMin);
        } else {
          snprintf(statusLine, sizeof(statusLine), "AP: 192.168.4.1  ");
        }
        lcd.print(statusLine);
      }
    }
  }

  if (isAlarmActive) {
    if (millis() - lastDistanceCheck > 200) {
      lastDistanceCheck = millis();
      currentDistanceCm = readUltrasonicDistanceCm();
    }

    if (currentDistanceCm > 0 &&
        currentDistanceCm < OPEN_DISTANCE_THRESHOLD_CM) {
      isAlarmActive = false;
      noTone(buzzerPin);
      buzzerState = false;
      redLedState = false;
      digitalWrite(redLedPin, LOW);
      digitalWrite(greenLedPin, HIGH);
      takenCountToday++;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Medicine Taken!");
      lcd.setCursor(0, 1);
      if (activeAlarmIndex >= 0) {
        lcd.print(alarms[activeAlarmIndex].name);
      } else {
        lcd.print("Well done!");
      }
      delay(3000);
      digitalWrite(greenLedPin, LOW);
      lcd.clear();
      activeAlarmIndex = -1;
    } else {
      static unsigned long lastBlink = 0;
      if (millis() - lastBlink > 300) {
        lastBlink = millis();
        redLedState = !redLedState;
        buzzerState = redLedState;
        digitalWrite(redLedPin, redLedState ? HIGH : LOW);
        if (buzzerState) {
          tone(buzzerPin, 1000);
        } else {
          noTone(buzzerPin);
        }
      }

      lcd.setCursor(0, 0);
      lcd.print("!! TAKE MEDS !! ");
      lcd.setCursor(0, 1);
      if (activeAlarmIndex >= 0) {
        char buf[17];
        snprintf(buf, sizeof(buf), "%-16.16s", alarms[activeAlarmIndex].name);
        lcd.print(buf);
      } else {
        lcd.print("Open MedBox lid ");
      }
    }
  }
}
