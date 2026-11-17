/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/ESP32-DIV
   ________________________________________ */

#ifndef wifiscanner_h
#define wifiscanner_h

#include <Arduino.h>

#include "display_config.h"
#include "input.h"
#include <SPI.h>

void wifiscannerSetup();
void wifiscannerLoop();

#endif
