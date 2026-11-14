// navigation.h
// The ESP32 has no GPS of its own, so it doesn't talk to Google Maps
// directly. Instead, a companion page (nav_companion.html - hosted
// separately over HTTPS, since browsers require a secure context for
// GPS) runs the actual navigation using Google Maps' Directions Service,
// figures out the next turn + distance, and POSTs that here every few
// seconds. This file just stores whatever it was last told and expires
// it if updates stop coming in (e.g. you closed the companion page).
#pragma once

String navDirection = "";  // "left","right","slight-left","slight-right",
                            // "sharp-left","sharp-right","straight","uturn","arrive"
float navMeters = 0;
unsigned long navLastUpdateMs = 0;
#define NAV_STALE_MS 15000UL // no update in this long -> treat as "not navigating"

bool navIsActive() {
  return navDirection.length() > 0 && (millis() - navLastUpdateMs) < NAV_STALE_MS;
}

void navSet(const String &dir, float meters) {
  navDirection = dir;
  navMeters = meters;
  navLastUpdateMs = millis();
}

void navClear() {
  navDirection = "";
  navMeters = 0;
  navLastUpdateMs = 0;
}
