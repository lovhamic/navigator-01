// clock_fonts.h
// Font choices for the digital clock screen. All fonts here are built
// into the U8g2 library already (no extra files needed) - selecting a
// font just changes which one u8g2.setFont() points at.
//
// Time fonts are big, for the HH:MM readout.
// Date fonts are small, for the date line below it.
// Both are selectable independently from the web app.
#pragma once

// ---------- TIME (big) fonts ----------
#define TIME_FONT_COUNT 7

const uint8_t* timeFonts[TIME_FONT_COUNT] = {
  u8g2_font_logisoso32_tf,   // 0: big, modern, rounded digits
  u8g2_font_fub30_tf,        // 1: Furore - bold, wide digits
  u8g2_font_helvB24_tf,      // 2: Helvetica Bold
  u8g2_font_ncenB24_tf,      // 3: New Century Schoolbook Bold
  u8g2_font_courB24_tf,      // 4: Courier Bold (monospace)
  u8g2_font_timB24_tf,       // 5: Times Bold
  u8g2_font_crox5hb_tf       // 6: Crox - heavy bold
};

const char* timeFontNames[TIME_FONT_COUNT] = {
  "Logisoso (big)",
  "Furore Bold",
  "Helvetica Bold",
  "Century Bold",
  "Courier Bold",
  "Times Bold",
  "Crox Heavy"
};

// ---------- DATE (small) fonts ----------
#define DATE_FONT_COUNT 7

const uint8_t* dateFonts[DATE_FONT_COUNT] = {
  u8g2_font_6x10_tf,         // 0: compact pixel font
  u8g2_font_7x13_tf,         // 1: slightly larger pixel font
  u8g2_font_9x15_tf,         // 2: roomier pixel font
  u8g2_font_helvR10_tf,      // 3: Helvetica Regular
  u8g2_font_ncenR10_tf,      // 4: New Century Schoolbook Regular
  u8g2_font_courR10_tf,      // 5: Courier Regular (monospace)
  u8g2_font_timR10_tf        // 6: Times Regular
};

const char* dateFontNames[DATE_FONT_COUNT] = {
  "6x10 Pixel",
  "7x13 Pixel",
  "9x15 Pixel",
  "Helvetica",
  "Century",
  "Courier",
  "Times"
};
