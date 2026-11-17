/* ____________________________
   Packet Stormer — connection-resilience stress tester.

   Four flood modes, cycled with the "[change mode]" list entry:
     - SSID Flood : raw 802.11 Authentication frames from randomised fake
                    MACs at a chosen nearby AP's BSSID/channel. No WiFi
                    credentials needed — works at Layer 2 against any AP
                    the scan can see. Only point this at your own AP:
                    it will disrupt whoever it's aimed at.
     - TCP Flood  : rapid-fire real TCP connect/close against a LAN IP
     - UDP Flood  : rapid-fire UDP datagrams against a LAN IP
     - HTTP Flood : rapid-fire real HTTP GET requests against a LAN IP

   The IP-based modes need the ESP32 joined to the home WiFi (same
   credentials as the web dashboard, from secrets.h) and discover
   candidate targets with a quick TCP port-80 sweep of the local subnet
   — a best-effort "what's alive" probe, not a proper ARP scan.
   ________________________________________ */

#ifndef packetstormer_h
#define packetstormer_h

#include <Arduino.h>

#include "display_config.h"
#include "input.h"
#include <SPI.h>

void stormerSetup();
void stormerLoop();

#endif
