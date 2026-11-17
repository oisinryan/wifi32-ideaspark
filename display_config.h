/* ____________________________
   Display driver for the ideaspark ESP32-D0WD-V3 board with the
   INTEGRATED 1.9" ST7789 TFT (170x320) — this board is NOT the
   ESP32 DevKitC + separate wired ST7735 breadboard rig the original
   WIFI32/ESP32-DIV project targets.

   Pins are fixed by the PCB, confirmed against the silkscreen and
   against ~/Documents/ESP32/rf-scanner/rf-scanner.ino, which already
   runs on this exact board:
     RST D4 - SCL D18 - CS D15 - RS D2 - SDA D23 - LEDA D32
   ________________________________________ */

#ifndef display_config_h
#define display_config_h

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

static constexpr int TFT_PIN_SCK  = 18;
static constexpr int TFT_PIN_MOSI = 23;
static constexpr int TFT_PIN_CS   = 15;
static constexpr int TFT_PIN_DC   = 2;
static constexpr int TFT_PIN_RST  = 4;
static constexpr int TFT_PIN_BLK  = 32;   // backlight, active-high

static constexpr int SCR_W = 170;
static constexpr int SCR_H = 320;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
 public:
  LGFX() {
    { auto c = _bus.config();
      c.spi_host    = VSPI_HOST;
      c.spi_mode    = 0;
      c.freq_write  = 40000000;
      c.freq_read   = 16000000;
      c.spi_3wire   = true;
      c.use_lock    = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk    = TFT_PIN_SCK;
      c.pin_mosi    = TFT_PIN_MOSI;
      c.pin_miso    = -1;
      c.pin_dc      = TFT_PIN_DC;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs          = TFT_PIN_CS;
      c.pin_rst         = TFT_PIN_RST;
      c.pin_busy        = -1;
      c.panel_width     = SCR_W;
      c.panel_height    = SCR_H;
      c.offset_x        = 35;   // 170 centred on the 240-wide controller
      c.offset_y        = 0;
      c.offset_rotation = 0;
      c.readable        = false;
      c.invert          = true;
      c.rgb_order       = false;
      c.dlen_16bit      = false;
      c.bus_shared      = false;
      _panel.config(c);
    }
    setPanel(&_panel);
  }
};

extern LGFX tft;

// Adafruit_ST7735 colour-name compatibility shims — the original WIFI32
// sources reference these by name throughout.
#define ST7735_BLACK 0x0000
#define ST7735_WHITE 0xFFFF

#endif
