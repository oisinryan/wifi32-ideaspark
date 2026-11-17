/* ____________________________
   Single-button gesture decoder.

   The original WIFI32 hardware wires 4 momentary buttons (Up/Down/Select/
   Back) to GPIO21/22/25/26. The ideaspark board this is running on instead
   has only the onboard BOOT button (GPIO0) — pressing it at runtime is
   harmless, it's only a strapping pin at boot (see rf-scanner.ino).

   One button, three gestures, standing in for the four original buttons:
     short press  -> Next   (cycle: menu item / channel / list entry)
     double press -> Select (enter menu item / toggle a feature on-off)
     long press   -> Back   (exit the current feature to the main menu)
   ________________________________________ */

#ifndef input_h
#define input_h

#include <Arduino.h>

void inputSetup();   // call once from setup()
void inputPoll();    // call once per loop iteration to refresh gesture state

// One-shot: true only for the inputPoll() cycle in which the gesture fired.
// Reading it again without an intervening inputPoll() call still returns
// the same value, so a feature's own inner loop may call inputPoll() itself
// and check these afterwards.
bool inputNext();
bool inputSelect();
bool inputBack();

// Lets another input source (the web dashboard's buttons) fire the same
// gestures as the physical button. Safe to call from an HTTP handler that
// runs inline with loop() (as webdash.cpp's does) — not interrupt-safe.
void inputInjectNext();
void inputInjectSelect();
void inputInjectBack();

#endif
