/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/ESP32-DIV
   ________________________________________ */

#ifndef detector_h
#define detector_h

#include <Arduino.h>

#include "display_config.h"
#include "input.h"
#include <SPI.h>

void detectorSetup();
void detectorLoop();

#endif
