#include "input.h"

static constexpr int      BTN_PIN      = 0;   // BOOT button, idle HIGH, pressed LOW
static constexpr uint32_t DEBOUNCE_MS  = 25;
static constexpr uint32_t LONG_MS      = 600;
static constexpr uint32_t DOUBLE_MS    = 250;

static bool     rawPrev       = true;   // HIGH = released
static uint32_t lastChange    = 0;
static uint32_t pressStart    = 0;
static uint32_t releaseTime   = 0;
static bool     longFired     = false;
static bool     awaitingDbl   = false;

static bool evNext = false, evSelect = false, evBack = false;

void inputSetup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  rawPrev = digitalRead(BTN_PIN);
}

void inputPoll() {
  evNext = evSelect = evBack = false;

  bool raw = digitalRead(BTN_PIN);
  uint32_t now = millis();

  if (raw != rawPrev && (now - lastChange) > DEBOUNCE_MS) {
    lastChange = now;
    if (raw == LOW) {
      // just pressed
      pressStart = now;
      longFired = false;
    } else {
      // just released
      if (!longFired) {
        if (awaitingDbl && (now - releaseTime) < DOUBLE_MS) {
          evSelect = true;
          awaitingDbl = false;
        } else {
          awaitingDbl = true;
          releaseTime = now;
        }
      }
    }
    rawPrev = raw;
  }

  // long-press fires once, immediately, while still held
  if (raw == LOW && !longFired && (now - pressStart) > LONG_MS) {
    longFired = true;
    awaitingDbl = false;
    evBack = true;
  }

  // a single press is only confirmed once the double-press window elapses
  // with no second press
  if (awaitingDbl && raw == HIGH && (now - releaseTime) > DOUBLE_MS) {
    awaitingDbl = false;
    evNext = true;
  }
}

bool inputNext()   { return evNext; }
bool inputSelect() { return evSelect; }
bool inputBack()   { return evBack; }

void inputInjectNext()   { evNext = true; }
void inputInjectSelect() { evSelect = true; }
void inputInjectBack()   { evBack = true; }
