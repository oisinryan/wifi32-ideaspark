/* ____________________________
   Web dashboard — reachable over the home WiFi (STA mode) at this
   device's IP, port 80. Shows whichever feature is currently active
   (mirrored from the TFT) and lets you drive it with the same
   tap/double-tap/hold gestures as the physical BOOT button.

   Hardware reality check: this board has ONE WiFi radio. The dashboard
   can only be reachable while nothing else has claimed the radio for
   exclusive use (channel-hopping, promiscuous sniffing, beacon spam).
   When you enter a feature from the menu, the radio is handed to it —
   the dashboard goes offline for that duration and the browser page
   auto-retries until it's handed back (when you press Back / hold the
   button on the device).

   While idle at the main menu, the dashboard itself periodically does a
   short disconnect -> scan -> reconnect cycle (same radio-sharing
   pattern) to report nearby networks.
   ________________________________________ */

#ifndef webdash_h
#define webdash_h

#include <Arduino.h>

void webdashSetup();   // call once from setup(), after inputSetup()
void webdashPoll();    // call once per loop() iteration — cheap, non-blocking

// Called by WIFI32.ino's menu dispatcher around a feature's setup()/loop():
// hands the radio to a feature (dropping the dashboard's WiFi connection)
// and reclaims it afterwards (reconnecting to home WiFi).
// webdashAnnounceHandoff pushes one last status line explaining the outage
// to any connected browser before the connection drops — call it right
// before webdashReleaseRadio().
void webdashAnnounceHandoff(const char* featureName);
void webdashReleaseRadio();
void webdashReclaimRadio();

// Called by each feature to publish what it's currently showing, so the
// dashboard page mirrors the TFT. `lines` is a small array of short status
// strings; at most 8 are used.
void dashSetStatus(const char* title, const String lines[], int count);

#endif
