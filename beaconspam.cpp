/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/ESP32-DIV
   ________________________________________ */

#include <Arduino.h>
#include "beaconspam.h"
#include "icons.h"
#include "webdash.h"
#include <esp_wifi.h>

const uint16_t ORANGE = 0xfbe4;
const uint16_t BLACK = 0;

String alfa = "1234567890qwertyuiopasdfghjkklzxcvbnm QWERTYUIOPASDFGHJKLZXCVBNM_";
uint8_t spamchannel = 1;
bool spam = false;

// Beacon Packet buffer
uint8_t packet[128] = {0x80, 0x00, 0x00, 0x00,
                      /*4*/   0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                      /*10*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                      /*16*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                      /*22*/  0xc0, 0x6c,
                      /*24*/  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,
                      /*32*/  0x64, 0x00,
                      /*34*/  0x01, 0x04,
                      /* SSID */
                      /*36*/  0x00, 0x06, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72,
                      0x01, 0x08, 0x82, 0x84,
                      0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01,
                      /*56*/  0x04};

void spammer() {
  // Randomize channel
  esp_wifi_set_channel(spamchannel, WIFI_SECOND_CHAN_NONE);

  // Randomize SRC MAC
  for (int i = 10; i <= 21; i++) {
    packet[i] = random(256);
  }

  // Randomize SSID
  for (int i = 38; i <= 43; i++) {
    packet[i] = alfa[random(65)];
  }

  packet[56] = spamchannel;

  // Send the packet
  esp_wifi_80211_tx(WIFI_IF_STA, packet, 57, false);

  delay(1);
}

void beaconspamSetup() {

  tft.fillScreen(ST7735_BLACK);

  // Set WiFi mode to NULL mode (monitor mode)
  esp_wifi_set_mode(WIFI_MODE_NULL);

  // Disconnect from any previous network
  esp_wifi_disconnect();

  // Initialize the ESP-IDF event loop. ESP_ERR_INVALID_STATE just means the
  // web dashboard (or another feature, earlier this boot) already created it.
  esp_err_t ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE("setup", "Error creating event loop: %d", ret);
    return;
  }

  // Initialize and configure Wi-Fi driver with default configuration.
  // ESP_ERR_WIFI_INIT_STATE means it's already initialized — also fine.
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
    ESP_LOGE("setup", "Error initializing Wi-Fi: %d", ret);
    return;
  }

  // Start the Wi-Fi driver
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE("setup", "Error starting Wi-Fi: %d", ret);
    return;
  }

  // Configure the Wi-Fi driver for promiscuous mode
  ret = esp_wifi_set_promiscuous(true);
  if (ret != ESP_OK) {
    ESP_LOGE("setup", "Error enabling promiscuous mode: %d", ret);
    return;
  }

  tft.fillScreen(ST7735_BLACK);
}

void beaconspamLoop() {
  // Single BOOT-button stand-in for the original pressBt01 (channel++) and
  // pressBt02 (toggle spam) interrupt handlers.
  if (inputNext()) {
    spamchannel = (spamchannel < 14) ? spamchannel + 1 : 1;
  }
  if (inputSelect()) {
    spam = !spam;
  }

  String dashLines[2] = {
    "channel: " + String(spamchannel),
    String("status: ") + (spam ? "ENABLED" : "disabled")
  };
  dashSetStatus("Beacon Spam", dashLines, 2);

  tft.fillRect(70, 0, 80, 20, ST7735_BLACK);
  tft.setTextWrap(false);
  tft.setTextColor(ORANGE);
  tft.setTextSize(1);

  tft.setCursor(2, 2);
  tft.print("channel:");

  tft.setCursor(105, 2);
  tft.print("[");
  tft.print(spamchannel);
  tft.println("]");

  tft.setCursor(2, 13);
  tft.print("progress:");

  tft.setCursor(80, 13);
  tft.print("disabled");

  delay(500);

  if (spam) {
    tft.fillRect(70, 10, 80, 10, ST7735_BLACK);
    tft.setTextColor(ORANGE);
    tft.setCursor(85, 13);
    tft.print("enabled");

    tft.fillRect(0, 45, 128, 150, ST7735_BLACK);

    tft.setCursor(2, 45);
    tft.print("beginning");

    for (int i = 0; i < 5; i++) {
      tft.print(".");
      delay(500);
    }
    delay(1000);

    tft.setCursor(2, 60);
    tft.print(" set channel");
    tft.print(" [");
    tft.print(spamchannel);
    tft.print("]");
    delay(700);

    tft.setCursor(2, 75);
    tft.print(" SSID generated");
    delay(300);

    tft.setCursor(2, 90);
    tft.print(" Randomize SRC MAC");
    delay(500);

    tft.setCursor(2, 110);
    tft.print(" start broadcast");
    delay(500);
  }

  while (spam) {
    spammer();
    inputPoll();
    if (inputSelect()) spam = false;        // toggle off, stay on this screen
    if (inputBack())   { spam = false; return; }  // exit to main menu
  }
}
