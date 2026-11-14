// reminder.h
// A typed message OR a hand-drawn 128x64 picture, sent in from the web
// app and shown on the Reminder screen. Whichever was sent most recently
// is what's displayed.
#pragma once

#define REMINDER_BITMAP_BYTES 1024 // 128 x 64 pixels, 1 bit each = 1024 bytes

enum ReminderShowMode { REMINDER_NONE, REMINDER_TEXT, REMINDER_DRAWING };
ReminderShowMode reminderShowMode = REMINDER_NONE;

String reminderText = "";
bool   reminderScrollEnabled = true;
uint8_t reminderBitmap[REMINDER_BITMAP_BYTES];

void reminderSetText(const String &text, bool scroll) {
  reminderText = text;
  reminderScrollEnabled = scroll;
  reminderShowMode = REMINDER_TEXT;
}

int reminderHexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

// Fills reminderBitmap from a hex string sent by the browser: 2 hex
// characters per byte, REMINDER_BITMAP_BYTES*2 = 2048 characters total.
// Byte layout matches XBM (what u8g2.drawXBMP expects): 16 bytes per row
// (128px / 8), bit 0 of each byte = its leftmost pixel.
bool reminderSetDrawingFromHex(const String &hex) {
  if ((int)hex.length() < REMINDER_BITMAP_BYTES * 2) return false;
  for (int i = 0; i < REMINDER_BITMAP_BYTES; i++) {
    int hi = reminderHexNibble(hex[i * 2]);
    int lo = reminderHexNibble(hex[i * 2 + 1]);
    reminderBitmap[i] = (uint8_t)((hi << 4) | lo);
  }
  reminderShowMode = REMINDER_DRAWING;
  return true;
}

void reminderClear() {
  reminderShowMode = REMINDER_NONE;
  reminderText = "";
}
