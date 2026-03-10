#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {

constexpr uint32_t kDefaultBaud = 115200;
constexpr uint16_t kTcpPort = 9001;
constexpr uint32_t kStaConnectTimeoutMs = 15000;

// Pins can be set per PlatformIO environment via BRIDGE_RX_PIN/BRIDGE_TX_PIN.
#ifndef BRIDGE_RX_PIN
#define BRIDGE_RX_PIN 20
#endif

#ifndef BRIDGE_TX_PIN
#define BRIDGE_TX_PIN 21
#endif

constexpr int kBridgeRxPin = BRIDGE_RX_PIN;
constexpr int kBridgeTxPin = BRIDGE_TX_PIN;

enum class WifiMode : uint8_t { AP = 0, STA = 1 };

struct BridgeConfig {
  uint32_t baudRate;
  WifiMode wifiMode;
  String deviceName;
  String staSsid;
  String staPassword;
  String apSsid;
  String apPassword;
};

BridgeConfig gConfig;
Preferences gPrefs;
WebServer gWeb(80);
WiFiServer gTcpServer(kTcpPort);
WiFiClient gTcpClient;

bool gRestartScheduled = false;
uint32_t gRestartAt = 0;

String htmlEscape(const String &in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    const char c = in[i];
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

String truncateTo(const String &value, size_t maxLen) {
  if (value.length() <= maxLen) {
    return value;
  }
  return value.substring(0, maxLen);
}

void loadConfig() {
  gPrefs.begin("bridge", true);

  gConfig.baudRate = gPrefs.getULong("baud", kDefaultBaud);
  gConfig.wifiMode = static_cast<WifiMode>(gPrefs.getUChar("wmode", 0));

  gConfig.deviceName = gPrefs.getString("devname", "MobisquirtBridge");
  gConfig.staSsid = gPrefs.getString("stassid", "");
  gConfig.staPassword = gPrefs.getString("stapass", "");
  gConfig.apSsid = gPrefs.getString("apssid", "MobisquirtBridge");
  gConfig.apPassword = gPrefs.getString("appass", "mobisquirt123");

  gPrefs.end();

  if (gConfig.baudRate < 1200 || gConfig.baudRate > 2000000) {
    gConfig.baudRate = kDefaultBaud;
  }

  if (gConfig.wifiMode != WifiMode::AP && gConfig.wifiMode != WifiMode::STA) {
    gConfig.wifiMode = WifiMode::AP;
  }

  gConfig.deviceName = truncateTo(gConfig.deviceName, 32);
  gConfig.staSsid = truncateTo(gConfig.staSsid, 32);
  gConfig.staPassword = truncateTo(gConfig.staPassword, 63);
  gConfig.apSsid = truncateTo(gConfig.apSsid, 32);
  gConfig.apPassword = truncateTo(gConfig.apPassword, 63);

  if (gConfig.deviceName.isEmpty()) {
    gConfig.deviceName = "MobisquirtBridge";
  }
  if (gConfig.apSsid.isEmpty()) {
    gConfig.apSsid = "MobisquirtBridge";
  }
}

void saveConfig(const BridgeConfig &cfg) {
  gPrefs.begin("bridge", false);
  gPrefs.putULong("baud", cfg.baudRate);
  gPrefs.putUChar("wmode", static_cast<uint8_t>(cfg.wifiMode));
  gPrefs.putString("devname", cfg.deviceName);
  gPrefs.putString("stassid", cfg.staSsid);
  gPrefs.putString("stapass", cfg.staPassword);
  gPrefs.putString("apssid", cfg.apSsid);
  gPrefs.putString("appass", cfg.apPassword);
  gPrefs.end();
}

void scheduleRestart(uint32_t delayMs) {
  gRestartScheduled = true;
  gRestartAt = millis() + delayMs;
}

bool startWifiSta() {
  if (gConfig.staSsid.isEmpty()) {
    Serial.println("STA mode requested but SSID is empty.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(gConfig.deviceName.c_str());
  WiFi.begin(gConfig.staSsid.c_str(), gConfig.staPassword.c_str());

  Serial.printf("Connecting to STA SSID '%s'...\n", gConfig.staSsid.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kStaConnectTimeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("STA connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("STA connect timeout.");
  return false;
}

void startWifiAp() {
  WiFi.mode(WIFI_AP);
  WiFi.setHostname(gConfig.deviceName.c_str());

  bool ok = false;
  if (gConfig.apPassword.isEmpty()) {
    ok = WiFi.softAP(gConfig.apSsid.c_str());
  } else {
    ok = WiFi.softAP(gConfig.apSsid.c_str(), gConfig.apPassword.c_str());
  }

  if (!ok) {
    Serial.println("Failed to start AP, retrying with open AP.");
    WiFi.softAP(gConfig.apSsid.c_str());
  }

  Serial.printf("AP started. SSID: %s IP: %s\n", gConfig.apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

void setupWifi() {
  WiFi.persistent(false);
  WiFi.disconnect(false, false);
  delay(100);

  if (gConfig.wifiMode == WifiMode::STA && startWifiSta()) {
    return;
  }

  if (gConfig.wifiMode == WifiMode::STA) {
    Serial.println("Falling back to AP mode.");
  }
  startWifiAp();
}

String currentIp() {
  if (WiFi.getMode() == WIFI_MODE_AP) {
    return WiFi.softAPIP().toString();
  }
  return WiFi.localIP().toString();
}

String modeString() {
  return (gConfig.wifiMode == WifiMode::AP) ? "ap" : "sta";
}

String renderPage() {
  String html;
  html.reserve(4096);

  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Mobisquirt Bridge Config</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#f2f5f8;margin:0;padding:20px;}";
  html += ".card{max-width:720px;margin:0 auto;background:#fff;padding:20px;border-radius:12px;box-shadow:0 10px 28px rgba(0,0,0,.08);}";
  html += "h1{margin-top:0;}label{display:block;margin-top:12px;font-weight:600;}";
  html += "input,select{width:100%;padding:10px;margin-top:6px;border:1px solid #ccd2da;border-radius:8px;box-sizing:border-box;}";
  html += ".hint{color:#5e6875;font-size:.9rem;}button{margin-top:18px;padding:12px 18px;border:0;border-radius:8px;background:#0067b8;color:#fff;font-weight:700;cursor:pointer;}";
  html += ".status{background:#edf6ff;border:1px solid #c8e1ff;border-radius:8px;padding:10px;margin-top:12px;}";
  html += "</style></head><body><div class='card'>";

  html += "<h1>Mobisquirt Bridge</h1>";
  html += "<p class='hint'>Configure UART baud and Wi-Fi mode (AP or client STA). Saving reboots the device.</p>";
  html += "<div class='status'><strong>Current IP:</strong> " + htmlEscape(currentIp()) + "<br><strong>TCP Port:</strong> " + String(kTcpPort) + "</div>";

  html += "<form method='POST' action='/save'>";

  html += "<label for='devname'>Device Name</label>";
  html += "<input id='devname' name='devname' maxlength='32' value='" + htmlEscape(gConfig.deviceName) + "'>";

  html += "<label for='baud'>UART Baud Rate</label>";
  html += "<input id='baud' name='baud' type='number' min='1200' max='2000000' value='" + String(gConfig.baudRate) + "'>";

  html += "<label for='mode'>Wi-Fi Mode</label>";
  html += "<select id='mode' name='mode'>";
  html += "<option value='ap'" + String(modeString() == "ap" ? " selected" : "") + ">Access Point (AP)</option>";
  html += "<option value='sta'" + String(modeString() == "sta" ? " selected" : "") + ">Client (STA)</option>";
  html += "</select>";

  html += "<label for='apssid'>AP SSID</label>";
  html += "<input id='apssid' name='apssid' maxlength='32' value='" + htmlEscape(gConfig.apSsid) + "'>";

  html += "<label for='appass'>AP Password (8-63 chars, leave blank for open AP)</label>";
  html += "<input id='appass' name='appass' maxlength='63' value='" + htmlEscape(gConfig.apPassword) + "'>";

  html += "<label for='stassid'>STA SSID</label>";
  html += "<input id='stassid' name='stassid' maxlength='32' value='" + htmlEscape(gConfig.staSsid) + "'>";

  html += "<label for='stapass'>STA Password</label>";
  html += "<input id='stapass' type='password' name='stapass' maxlength='63' value='" + htmlEscape(gConfig.staPassword) + "'>";

  html += "<button type='submit'>Save and Reboot</button>";
  html += "</form></div></body></html>";

  return html;
}

void handleRoot() { gWeb.send(200, "text/html", renderPage()); }

void handleStatus() {
  String body;
  body.reserve(256);
  body += "mode=";
  body += modeString();
  body += "\nip=";
  body += currentIp();
  body += "\nbaud=";
  body += String(gConfig.baudRate);
  body += "\n";
  gWeb.send(200, "text/plain", body);
}

void handleSave() {
  BridgeConfig next = gConfig;

  next.deviceName = truncateTo(gWeb.arg("devname"), 32);
  if (next.deviceName.isEmpty()) {
    next.deviceName = "MobisquirtBridge";
  }

  const long baud = gWeb.arg("baud").toInt();
  if (baud >= 1200 && baud <= 2000000) {
    next.baudRate = static_cast<uint32_t>(baud);
  }

  const String mode = gWeb.arg("mode");
  next.wifiMode = (mode == "sta") ? WifiMode::STA : WifiMode::AP;

  next.apSsid = truncateTo(gWeb.arg("apssid"), 32);
  next.apPassword = truncateTo(gWeb.arg("appass"), 63);
  next.staSsid = truncateTo(gWeb.arg("stassid"), 32);
  next.staPassword = truncateTo(gWeb.arg("stapass"), 63);

  if (next.apSsid.isEmpty()) {
    next.apSsid = "MobisquirtBridge";
  }

  if (!next.apPassword.isEmpty() && next.apPassword.length() < 8) {
    gWeb.send(400, "text/plain", "AP password must be 8-63 chars, or empty for open AP.");
    return;
  }

  if (next.wifiMode == WifiMode::STA && next.staSsid.isEmpty()) {
    gWeb.send(400, "text/plain", "STA mode requires SSID.");
    return;
  }

  saveConfig(next);

  gWeb.send(200, "text/html",
            "<html><body><h2>Saved</h2><p>Settings stored. Rebooting now...</p></body></html>");
  scheduleRestart(800);
}

void setupWeb() {
  gWeb.on("/", HTTP_GET, handleRoot);
  gWeb.on("/status", HTTP_GET, handleStatus);
  gWeb.on("/save", HTTP_POST, handleSave);
  gWeb.begin();
  Serial.println("Web server started on port 80.");
}

void setupBridge() {
  Serial1.begin(gConfig.baudRate, SERIAL_8N1, kBridgeRxPin, kBridgeTxPin);
  gTcpServer.begin();
  gTcpServer.setNoDelay(true);
  Serial.printf("TCP server started on port %u.\n", kTcpPort);
  Serial.printf("UART bridge on RX=%d TX=%d at %lu baud.\n", kBridgeRxPin, kBridgeTxPin,
                static_cast<unsigned long>(gConfig.baudRate));
}

void serviceTcpBridge() {
  if (!gTcpClient || !gTcpClient.connected()) {
    WiFiClient incoming = gTcpServer.available();
    if (incoming) {
      if (gTcpClient) {
        gTcpClient.stop();
      }
      gTcpClient = incoming;
      gTcpClient.setNoDelay(true);
      Serial.printf("TCP client connected: %s\n", gTcpClient.remoteIP().toString().c_str());
    }
  }

  if (!gTcpClient || !gTcpClient.connected()) {
    return;
  }

  uint8_t buf[256];

  while (gTcpClient.available()) {
    const size_t n = gTcpClient.read(buf, sizeof(buf));
    if (n > 0) {
      Serial1.write(buf, n);
    }
  }

  while (Serial1.available()) {
    const size_t n = Serial1.readBytes(buf, sizeof(buf));
    if (n > 0) {
      gTcpClient.write(buf, n);
    }
  }

  if (!gTcpClient.connected()) {
    gTcpClient.stop();
    Serial.println("TCP client disconnected.");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("MobisquirtBridge booting...");

  loadConfig();
  setupWifi();
  setupWeb();
  setupBridge();

  Serial.printf("Open config UI: http://%s/\n", currentIp().c_str());
}

void loop() {
  gWeb.handleClient();
  serviceTcpBridge();

  if (gRestartScheduled && static_cast<int32_t>(millis() - gRestartAt) >= 0) {
    Serial.println("Restarting...");
    ESP.restart();
  }
}
