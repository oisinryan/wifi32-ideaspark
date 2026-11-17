/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/ESP32-DIV
   ________________________________________  */

#include "display_config.h"  // onboard ST7789 170x320 panel (LovyanGFX)
#include "input.h"            // single BOOT-button gesture decoder
#include "webdash.h"           // web dashboard over home WiFi

#include "HCScreen.h"
#include "packetmonitor.h"
#include "detector.h"
#include "wifiscanner.h"
#include "beaconspam.h"
#include "packetstormer.h"

#include "icons.h"   // Different icons and logos displayed

#define MAIN_MENU 0
#define SUB_MENU 1

String main_menu[] = {"1.Wifi Analyzer", "2.Packet Monitor", "3.Beacon Spam", "4.Deauth Detector", "5.Packet Stormer"};
uint8_t main_menu_cnt = 5;

volatile int whichMenu = 0;
String lastPath = "";

HCScreen screen = HCScreen(tft);

String selection = screen.getSelection();
int8_t selectionIndex = screen.getSelectionIndex();

// Function pointers for setup and loop functions
void (*setupFunctions[])(void) = {wifiscannerSetup, packetmonitorSetup, beaconspamSetup, detectorSetup, stormerSetup};
void (*loopFunctions[])(void) = {wifiscannerLoop, packetmonitorLoop, beaconspamLoop, detectorLoop, stormerLoop};

const uint16_t ORANGE = 0xba6525;
// const uint16_t ORANGE = 0xff9514;
const uint16_t WHITE = 0xf0f0f0;
const uint16_t PURPLE = 0x7b21db;
const uint16_t DARKPURPLE = 0x6614d9;

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);

  pinMode(TFT_PIN_BLK, OUTPUT);
  digitalWrite(TFT_PIN_BLK, HIGH);   // backlight on

  tft.fillScreen(ST7735_BLACK);

  tft.drawBitmap(0, 0, sypherBitmap, 120, 120, 0xffffff);

  tft.setTextWrap(false);
  tft.setCursor(10, 110);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.println("ESP32 HackOne");
  tft.setCursor(10, 120);
  tft.setTextSize(2);
  tft.println("Sypher0");
  tft.setCursor(45, 140);
  tft.setTextSize(1);
  tft.println("Alpha Version");
  tft.setCursor(2, 160);
  tft.println("BOOT: tap=next  2x=select  hold=back");

  delay(3000);

  tft.fillScreen(ST7735_BLACK);

  screen.setMenu(main_menu, main_menu_cnt);
  screen.setTitle("[ESP32 HackOne]");
  screen.setLineHeight(12);

  inputSetup();
  webdashSetup();
}

void loop() {
  inputPoll();
  webdashPoll();

  String selection = screen.getSelection();
  int8_t selectionIndex = screen.getSelectionIndex();

  if (inputNext()) {
    screen.selectNext();
  }

  if (inputBack()) {
    screen.setMenu(main_menu, main_menu_cnt);
    whichMenu = MAIN_MENU;
  }

  if (inputSelect()) {
    if (selection == "Back") {
      screen.setMenu(main_menu, main_menu_cnt);
      whichMenu = MAIN_MENU;
    }

    if (selectionIndex >= 0 && selectionIndex < main_menu_cnt) {
      // Hand the single WiFi radio over to the feature — the dashboard
      // goes offline for as long as this runs. Push one last status line
      // explaining why before the connection drops.
      webdashAnnounceHandoff(main_menu[selectionIndex].c_str());
      webdashReleaseRadio();

      // Execute the selected setup and loop functions
      setupFunctions[selectionIndex]();
      while (selection == main_menu[selectionIndex]) {
        inputPoll();
        loopFunctions[selectionIndex]();
        if (inputBack()) {
          screen.setMenu(main_menu, main_menu_cnt);
          break;
        }
      }

      // Back at the main menu — reconnect and bring the dashboard back.
      webdashReclaimRadio();
    }
  }
}
