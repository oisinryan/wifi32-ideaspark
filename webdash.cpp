#include "webdash.h"
#include "input.h"
#include "secrets.h"
#include <WiFi.h>

static const uint32_t IDLE_SCAN_INTERVAL_MS = 15000;
static const uint32_t RECONNECT_RETRY_MS    = 5000;
static const uint32_t CONNECT_TIMEOUT_MS    = 8000;
static const uint32_t SSE_PUSH_INTERVAL_MS  = 1000;

static WiFiServer server(80);
static WiFiClient sseClient;
static bool     radioOwned         = false;
static uint32_t lastIdleScan       = 0;
static uint32_t lastReconnectTry   = 0;
static uint32_t lastPush           = 0;

static String currentTitle = "Main Menu";
static String currentLines[8];
static int    currentLineCount = 0;

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WIFI32 Dashboard</title>
<style>
  body { background:#0b0b10; color:#e8e8e8; font-family: ui-monospace, Menlo, monospace; margin:0; padding:16px; }
  h1 { font-size:16px; color:#ba6525; margin:0 0 4px; }
  #status { font-size:12px; margin-bottom:12px; }
  #status.live { color:#6fd16f; }
  #status.dead { color:#d16f6f; }
  #title { font-size:14px; color:#f0f0f0; margin:8px 0; }
  #lines { list-style:none; padding:0; margin:0 0 16px; font-size:13px; }
  #lines li { padding:4px 0; border-bottom:1px solid #222; white-space:pre-wrap; word-break:break-all; }
  .btns { display:flex; gap:10px; }
  button { flex:1; padding:14px 0; font-size:14px; border:none; border-radius:8px; background:#1b1b24; color:#f0f0f0; }
  button:active { background:#ba6525; }
  #back { background:#2a1414; }
</style></head>
<body>
<h1>ESP32 HackOne</h1>
<div id="status" class="dead">connecting&hellip;</div>
<div id="title">Main Menu</div>
<ul id="lines"></ul>
<div class="btns">
  <button onclick="act('next')">Next</button>
  <button onclick="act('select')">Select</button>
  <button id="back" onclick="act('back')">Back</button>
</div>
<script>
function act(a){ fetch('/api/' + a).catch(function(){}); }
function connect(){
  var es = new EventSource('/events');
  var st = document.getElementById('status');
  es.onopen = function(){ st.textContent = 'live'; st.className = 'live'; };
  es.onerror = function(){ st.textContent = 'reconnecting…'; st.className = 'dead'; };
  es.onmessage = function(e){
    try {
      var d = JSON.parse(e.data);
      document.getElementById('title').textContent = d.title || '';
      var ul = document.getElementById('lines');
      ul.innerHTML = '';
      (d.lines || []).forEach(function(l){
        var li = document.createElement('li');
        li.textContent = l;
        ul.appendChild(li);
      });
    } catch (err) {}
  };
}
connect();
</script>
</body></html>
)HTML";

static String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((unsigned char)c >= 0x20) out += c;  // drop other control bytes
    }
  }
  return out;
}

static String buildStatusJson() {
  String j = "{\"title\":\"" + jsonEscape(currentTitle) + "\",\"lines\":[";
  for (int i = 0; i < currentLineCount; i++) {
    if (i) j += ",";
    j += "\"" + jsonEscape(currentLines[i]) + "\"";
  }
  j += "]}";
  return j;
}

static void pushStatus() {
  if (!(sseClient && sseClient.connected())) return;
  sseClient.print("data: ");
  sseClient.print(buildStatusJson());
  sseClient.print("\n\n");
}

void dashSetStatus(const char* title, const String lines[], int count) {
  currentTitle = title;
  currentLineCount = count < 8 ? count : 8;
  for (int i = 0; i < currentLineCount; i++) currentLines[i] = lines[i];
}

static void handleClient(WiFiClient client) {
  client.setTimeout(2);  // seconds

  String reqLine = client.readStringUntil('\n');
  reqLine.trim();

  // consume the rest of the headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
  }

  int sp1 = reqLine.indexOf(' ');
  int sp2 = (sp1 < 0) ? -1 : reqLine.indexOf(' ', sp1 + 1);
  if (sp1 < 0 || sp2 < 0) {
    client.stop();
    return;
  }
  String path = reqLine.substring(sp1 + 1, sp2);

  if (path == "/events") {
    client.print("HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/event-stream\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: keep-alive\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "\r\n");
    if (sseClient) sseClient.stop();
    sseClient = client;    // adopted — kept open across loop() iterations
    pushStatus();
    return;                // do NOT client.stop() — this connection stays open
  }

  if (path == "/" || path == "/index.html") {
    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
    client.print(DASHBOARD_HTML);
  } else if (path == "/api/next") {
    inputInjectNext();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"ok\":true}");
  } else if (path == "/api/select") {
    inputInjectSelect();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"ok\":true}");
  } else if (path == "/api/back") {
    inputInjectBack();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"ok\":true}");
  } else {
    client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
  }
  client.stop();
}

static void idleScanCycle() {
  dashSetStatus("Main Menu - scanning...", nullptr, 0);
  pushStatus();

  WiFi.disconnect(false, false);
  delay(100);

  int n = WiFi.scanNetworks();
  String lines[8];
  int cnt = 0;
  for (int i = 0; i < n && cnt < 8; i++) {
    lines[cnt++] = WiFi.SSID(i) + "  " + String(WiFi.RSSI(i)) + "dBm  ch" + String(WiFi.channel(i));
  }
  WiFi.scanDelete();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT_MS) {
    delay(50);
  }

  dashSetStatus("Main Menu", lines, cnt);
  pushStatus();
}

void webdashSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[webdash] connecting to ");
  Serial.print(WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[webdash] connected, dashboard at http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[webdash] not connected yet (check secrets.h) — will keep retrying");
  }
  server.begin();
}

void webdashPoll() {
  if (radioOwned) return;   // a feature currently owns the radio

  WiFiClient client = server.accept();
  if (client) handleClient(client);

  if (sseClient) {
    if (sseClient.connected()) {
      if (millis() - lastPush > SSE_PUSH_INTERVAL_MS) {
        pushStatus();
        lastPush = millis();
      }
    } else {
      sseClient.stop();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastIdleScan > IDLE_SCAN_INTERVAL_MS) {
      lastIdleScan = millis();
      idleScanCycle();
    }
  } else if (millis() - lastReconnectTry > RECONNECT_RETRY_MS) {
    lastReconnectTry = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

void webdashAnnounceHandoff(const char* featureName) {
  String lines[3] = {
    String("Radio handed to ") + featureName + ".",
    "This page goes offline until you",
    "return to the Main Menu (hold BOOT)."
  };
  dashSetStatus(featureName, lines, 3);
  pushStatus();
  if (sseClient) sseClient.flush();
  delay(150);   // give the TCP stack a moment to actually get this out
               // before webdashReleaseRadio() tears the connection down
}

void webdashReleaseRadio() {
  if (sseClient) sseClient.stop();
  server.end();
  radioOwned = true;
  WiFi.mode(WIFI_OFF);
  delay(50);
}

void webdashReclaimRadio() {
  radioOwned = false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT_MS) {
    delay(50);
  }
  server.begin();
  dashSetStatus("Main Menu", nullptr, 0);
  lastIdleScan = millis();   // don't scan immediately, give it a beat
}
