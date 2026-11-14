#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

// ================= WIFI MANAGER SETTINGS =================
const char* AP_SSID = "ESP32-Setup";   // Name of the WiFi network ESP32 creates
const char* AP_PASSWORD = "12345678";  // Must be at least 8 characters

WebServer server(80);

// ---------- Non-blocking WiFi connection state ----------
enum ConnState { IDLE, CONNECTING, CONNECTED, FAILED };
ConnState connState = IDLE;
String pendingSSID, pendingPassword;
bool connectRequested = false;
unsigned long connectStartTime = 0;
const unsigned long CONNECT_TIMEOUT_MS = 15000; // 15 seconds

#define PIN_SDA 21
#define PIN_SCL 22

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE,
  PIN_SCL,
  PIN_SDA
);



// ================= DISPLAY / ANIMATION FRAMES =================
#include "normal_frames.h"   // normalFrames[], NORMAL_FRAME_COUNT
#include "maker_frames.h"    // makerFrames[], MAKER_FRAME_COUNT
#include "eyes_frames.h"     // eyesFrames[], EYES_FRAME_COUNT
#include "hands_frames.h"    // handsFrames[], HANDS_FRAME_COUNT
#include "flag_frames.h"     // flagFrames[], FLAG_FRAME_COUNT
#include "orbit_frames.h"    // orbitFrames[], ORBIT_FRAME_COUNT
#include "compass_frames.h"  // compassFrames[], COMPASS_FRAME_COUNT

// STATE
#define TOUCH_PIN                 33
#define MOCHI_FRAME_RATE_MS       40
#define MAKER_FRAME_RATE_MS       83
#define EYES_FRAME_RATE_MS        450
#define HANDS_FRAME_RATE_MS       303
#define FLAG_FRAME_RATE_MS        365
#define ORBIT_FRAME_RATE_MS       386
#define COMPASS_FRAME_RATE_MS     221
#define CLOCK_REFRESH_RATE_MS     1000
#define TOUCH_DEBOUNCE_MS         50
#define NUM_ANIMATIONS            7

bool lastTouchRead  = LOW;
bool touchPressed   = false;
unsigned long lastTouchChangeMs = 0;

int  normalFrame    = 0;
int  makerFrame     = 0;
int  eyesFrame      = 0;
int  handsFrame     = 0;
int  flagFrame      = 0;
int  orbitFrame     = 0;
int  compassFrame   = 0;

// 0 = DasaiMochi, 1 = OLED Animation Maker, 2 = Eyes,
// 3 = Hands, 4 = Flag, 5 = Orbit, 6 = Compass
// Selected only from the web app.
int  activeAnim     = 0;
unsigned long lastFrameMs = 0;

// ================= DISPLAY MODE (ANIMATION / CLOCK / SPOTIFY / NAV / REMINDER) =================
// Switched ONLY by the physical touch button. The app cannot change this.
#define MODE_ANIMATION 0
#define MODE_CLOCK     1
#define MODE_SPOTIFY   2
#define MODE_NAV       3
#define MODE_REMINDER  4
int displayMode = MODE_ANIMATION;

// ================= CLOCK / FONTS =================
// Selected only from the web app.
#include "clock_fonts.h"   // timeFonts[], dateFonts[], *_COUNT, *_NAMES[]

int  timeFontIndex = 0;
int  dateFontIndex = 0;

// Timezone offset in seconds, applied on top of UTC. Default: IST (UTC+5:30).
long utcOffsetSec = 19800;
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.nist.gov";

// ================= SPOTIFY =================
#include "spotify_client.h" // spotifyNow, spotifyPollNowPlaying(), spotifyIsAuthed(), etc.

#define SPOTIFY_POLL_INTERVAL_MS   3000  // how often to hit the Spotify API
#define SPOTIFY_RENDER_INTERVAL_MS 100   // how often to redraw (for smooth scrolling)
unsigned long lastSpotifyPollMs = 0;
unsigned long lastSpotifyRenderMs = 0;

// title scroll state (shared helper, one state struct per screen that scrolls)
struct ScrollState {
  String text = "";
  int offsetPx = 0;
  unsigned long lastMs = 0;
};
ScrollState spotifyScroll;
ScrollState reminderScroll;
#define SCROLL_STEP_PX     1
#define SCROLL_INTERVAL_MS 60
#define SCROLL_GAP_PX      20

// ================= NAVIGATION =================
#include "navigation.h" // navDirection, navMeters, navIsActive(), navSet(), navClear()
#define NAV_RENDER_INTERVAL_MS 300
unsigned long lastNavRenderMs = 0;

// ================= REMINDER =================
#include "reminder.h" // reminderText, reminderBitmap[], reminderSetText(), etc.
#define REMINDER_RENDER_INTERVAL_MS 100
unsigned long lastReminderRenderMs = 0;

// ================= WEB CONTROL PANEL (HTML + HTTP handlers) =================
#include "web_ui.h"

void readTouch() {
  bool reading = digitalRead(TOUCH_PIN) == HIGH;
  if (reading != lastTouchRead) {
    lastTouchChangeMs = millis();
  }
  if ((millis() - lastTouchChangeMs) > TOUCH_DEBOUNCE_MS) {
    if (reading && !touchPressed) {
      // Touch button cycles Animation -> Clock -> Spotify -> Navigation ->
      // Reminder -> Animation. Content for each screen is set from the web app.
      displayMode++;
      if (displayMode > MODE_REMINDER) displayMode = MODE_ANIMATION;
      lastFrameMs = 0;        // force an immediate redraw of the new window
      lastSpotifyPollMs = 0;  // poll Spotify right away if we just switched to it
    }
    touchPressed = reading;
  }
  lastTouchRead = reading;
}

// Draws the current time (big font) and date (small font, below) centered
// on the screen. Falls back to a placeholder if no time has been set yet
// (no WiFi/NTP sync and no manual sync from the phone).
void drawClock() {
  struct tm timeinfo;
  bool haveTime = getLocalTime(&timeinfo, 5);

  char timeBuf[16];
  char dateBuf[32];

  u8g2.setFont(timeFonts[timeFontIndex]);
  if (haveTime) {
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
  } else {
    strcpy(timeBuf, "--:--");
  }
  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr((128 - tw) / 2, 40, timeBuf);

  u8g2.setFont(dateFonts[dateFontIndex]);
  if (haveTime) {
    strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y", &timeinfo);
  } else {
    strcpy(dateBuf, "Set time via app");
  }
  int dw = u8g2.getStrWidth(dateBuf);
  u8g2.drawStr((128 - dw) / 2, 58, dateBuf);
}

// A small generic "8th note" icon drawn with primitives (~10x12px) so we
// don't need to bundle a bitmap copy of Spotify's trademarked logo. Swap
// this out for your own XBM bitmap if you'd rather draw something else.
void drawMusicNoteIcon(int x, int y) {
  u8g2.drawDisc(x, y + 9, 3);
  u8g2.drawVLine(x + 3, y, 10);
  u8g2.drawLine(x + 3, y, x + 8, y + 3);
  u8g2.drawLine(x + 3, y + 3, x + 8, y + 6);
}

// Draws `text` on one line. If it fits within areaW it's centered; if not,
// it scrolls left continuously with a gap before it loops back around.
// `st` holds per-screen scroll position so Spotify and Reminder don't
// interfere with each other. Resets automatically whenever text changes.
void drawScrollingLine(ScrollState &st, const String &text, int y, int areaW) {
  int tw = u8g2.getStrWidth(text.c_str());

  if (tw <= areaW) {
    u8g2.drawStr((128 - tw) / 2, y, text.c_str());
    return;
  }

  if (text != st.text) {
    st.text = text;
    st.offsetPx = 0;
    st.lastMs = millis();
  }

  unsigned long now = millis();
  if (now - st.lastMs >= SCROLL_INTERVAL_MS) {
    st.lastMs = now;
    st.offsetPx += SCROLL_STEP_PX;
    if (st.offsetPx >= tw + SCROLL_GAP_PX) {
      st.offsetPx = 0;
    }
  }

  u8g2.setClipWindow(0, y - 11, 128, y + 3);
  int x1 = -st.offsetPx;
  int x2 = x1 + tw + SCROLL_GAP_PX;
  u8g2.drawStr(x1, y, text.c_str());
  u8g2.drawStr(x2, y, text.c_str());
  u8g2.setMaxClipWindow();
}

// Spotify "now playing" screen: title (scrolling), artist, progress bar,
// elapsed/remaining time, and a small icon + label up top.
void drawSpotify() {
  u8g2.setFont(u8g2_font_6x10_tf);

  if (!spotifyNow.authed) {
    u8g2.drawStr(2, 20, "Spotify not linked");
    u8g2.drawStr(2, 34, "Open the web app and");
    u8g2.drawStr(2, 46, "tap 'Connect Spotify'");
    return;
  }

  drawMusicNoteIcon(2, 1);
  u8g2.drawStr(16, 10, "SPOTIFY");

  if (!spotifyNow.valid) {
    u8g2.drawStr(2, 36, "Nothing playing");
    return;
  }

  u8g2.setFont(u8g2_font_7x13_tf);
  drawScrollingLine(spotifyScroll, spotifyNow.title, 26, 124);

  u8g2.setFont(u8g2_font_6x10_tf);
  int aw = u8g2.getStrWidth(spotifyNow.artist.c_str());
  if (aw > 124) {
    String a = spotifyNow.artist;
    while (a.length() > 3 && u8g2.getStrWidth((a + "...").c_str()) > 124) {
      a.remove(a.length() - 1);
    }
    a += "...";
    u8g2.drawStr(2, 40, a.c_str());
  } else {
    u8g2.drawStr((128 - aw) / 2, 40, spotifyNow.artist.c_str());
  }

  unsigned long prog = spotifyEstimatedProgressMs();
  unsigned long dur  = spotifyNow.durationMs > 0 ? spotifyNow.durationMs : 1;

  int barX = 4, barY = 48, barW = 120, barH = 5;
  u8g2.drawFrame(barX, barY, barW, barH);
  int fillW = (int)((float)(barW - 2) * prog / dur);
  if (fillW > 0) u8g2.drawBox(barX + 1, barY + 1, fillW, barH - 2);

  unsigned long remainMs = (dur > prog) ? (dur - prog) : 0;
  char elapsedBuf[8], remainBuf[8];
  snprintf(elapsedBuf, sizeof(elapsedBuf), "%lu:%02lu", prog / 60000, (prog / 1000) % 60);
  snprintf(remainBuf, sizeof(remainBuf), "-%lu:%02lu", remainMs / 60000, (remainMs / 1000) % 60);

  u8g2.drawStr(2, 62, elapsedBuf);
  int rw = u8g2.getStrWidth(remainBuf);
  u8g2.drawStr(126 - rw, 62, remainBuf);

  if (!spotifyNow.isPlaying) {
    u8g2.drawStr(48, 62, "PAUSED");
  }
}

// Rotates point (x,y) around (cx,cy) by angleDeg (0 = up, clockwise positive).
void navRotatePoint(float x, float y, float angleDeg, int cx, int cy, int &outX, int &outY) {
  float rad = angleDeg * PI / 180.0;
  float rx = x * cos(rad) - y * sin(rad);
  float ry = x * sin(rad) + y * cos(rad);
  outX = cx + (int)round(rx);
  outY = cy + (int)round(ry);
}

// Draws a big arrow centered at (cx,cy), pointing angleDeg clockwise from up.
void drawArrow(int cx, int cy, float angleDeg) {
  int tailX, tailY, neckX, neckY, tipX, tipY, wingLX, wingLY, wingRX, wingRY;
  navRotatePoint(0, 14, angleDeg, cx, cy, tailX, tailY);
  navRotatePoint(0, -6, angleDeg, cx, cy, neckX, neckY);
  u8g2.drawLine(tailX, tailY, neckX, neckY);

  navRotatePoint(0, -16, angleDeg, cx, cy, tipX, tipY);
  navRotatePoint(-8, -2, angleDeg, cx, cy, wingLX, wingLY);
  navRotatePoint(8, -2, angleDeg, cx, cy, wingRX, wingRY);
  u8g2.drawTriangle(tipX, tipY, wingLX, wingLY, wingRX, wingRY);
}

float navAngleForDirection(const String &dir) {
  if (dir == "straight")     return 0;
  if (dir == "slight-right") return 30;
  if (dir == "right")        return 90;
  if (dir == "sharp-right")  return 135;
  if (dir == "uturn")        return 180;
  if (dir == "sharp-left")   return -135;
  if (dir == "left")         return -90;
  if (dir == "slight-left")  return -30;
  return 0;
}

// Navigation screen: just an arrow + distance to the next turn, relayed
// in from the phone (see navigation.h for why the device doesn't fetch
// this itself).
void drawNav() {
  u8g2.setFont(u8g2_font_6x10_tf);

  if (!navIsActive()) {
    u8g2.drawStr(2, 20, "No active navigation");
    u8g2.drawStr(2, 34, "Start navigating from");
    u8g2.drawStr(2, 46, "the companion page");
    return;
  }

  if (navDirection == "arrive") {
    u8g2.setFont(u8g2_font_helvB10_tf);
    const char* msg = "ARRIVED";
    int w = u8g2.getStrWidth(msg);
    u8g2.drawStr((128 - w) / 2, 38, msg);
    return;
  }

  drawArrow(64, 28, navAngleForDirection(navDirection));

  char distBuf[16];
  if (navMeters >= 1000) {
    snprintf(distBuf, sizeof(distBuf), "%.1f km", navMeters / 1000.0);
  } else {
    snprintf(distBuf, sizeof(distBuf), "%d m", (int)navMeters);
  }
  u8g2.setFont(u8g2_font_helvB10_tf);
  int dw = u8g2.getStrWidth(distBuf);
  u8g2.drawStr((128 - dw) / 2, 58, distBuf);
}

// Reminder screen: shows whichever was sent most recently - a typed
// message (scrolling if it doesn't fit, unless scrolling was turned off)
// or a hand-drawn picture from the web app's canvas.
void drawReminder() {
  if (reminderShowMode == REMINDER_DRAWING) {
    u8g2.drawXBMP(0, 0, 128, 64, reminderBitmap);
    return;
  }

  if (reminderShowMode == REMINDER_TEXT && reminderText.length() > 0) {
    u8g2.setFont(u8g2_font_7x13_tf);
    if (reminderScrollEnabled) {
      drawScrollingLine(reminderScroll, reminderText, 36, 124);
    } else {
      String t = reminderText;
      while (t.length() > 3 && u8g2.getStrWidth((t + "...").c_str()) > 124) {
        t.remove(t.length() - 1);
      }
      if (t.length() < reminderText.length()) t += "...";
      int tw = u8g2.getStrWidth(t.c_str());
      u8g2.drawStr((128 - tw) / 2, 36, t.c_str());
    }
    return;
  }

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 34, "No reminder set");
}

void setup(void) {
  Wire.begin(PIN_SDA, PIN_SCL);
  u8g2.begin();
  pinMode(TOUCH_PIN, INPUT);

  Serial.begin(115200);
  delay(500);

  // Start in AP+STA mode so the config page stays reachable
  // even after connecting to a home/office network.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println();
  Serial.println("Access Point started");
  Serial.print("AP SSID: "); Serial.println(AP_SSID);
  Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP());

  // Sets the local UTC offset used to render the clock, and starts an
  // SNTP sync attempt (will succeed once the STA side has real internet).
  configTime(utcOffsetSec, 0, NTP_SERVER_1, NTP_SERVER_2);

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/status", handleStatus);
  server.on("/setAnim", handleSetAnim);
  server.on("/animStatus", handleAnimStatus);
  server.on("/setTimeFont", handleSetTimeFont);
  server.on("/setDateFont", handleSetDateFont);
  server.on("/fontStatus", handleFontStatus);
  server.on("/setUtcOffset", handleSetUtcOffset);
  server.on("/setTime", handleSyncTime);
  server.on("/timeStatus", handleTimeStatus);
  server.on("/spotify/login", handleSpotifyLogin);
  server.on("/spotify/callback", handleSpotifyCallback);
  server.on("/spotify/status", handleSpotifyStatus);
  server.on("/spotify/logout", handleSpotifyLogout);
  server.on("/reminder/text", HTTP_POST, handleReminderSetText);
  server.on("/reminder/drawing", HTTP_POST, handleReminderSetDrawing);
  server.on("/reminder/clear", handleReminderClear);
  server.on("/nav/update", HTTP_POST, handleNavUpdate);
  server.on("/nav/update", HTTP_OPTIONS, handleNavOptions);
  server.on("/nav/status", handleNavStatus);
  server.on("/nav/clear", handleNavClear);
  server.onNotFound(handleNotFound);

  // Load any previously-saved Spotify refresh token so we're still
  // "logged in" after a reboot / power cycle.
  spotifyLoadRefreshToken();

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  // A connect request came in from the web page: kick it off (non-blocking)
  if (connectRequested) {
    connectRequested = false;
    Serial.println("Connecting to: " + pendingSSID);
    WiFi.disconnect(true, true);
    delay(200);
    WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
    connectStartTime = millis();
    connState = CONNECTING;
  }

  // While a connection attempt is in progress, check status without blocking
  if (connState == CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      connState = CONNECTED;
      Serial.println("Connected! IP address: " + WiFi.localIP().toString());
      // Real internet is available now - (re)kick NTP so the clock syncs.
      configTime(utcOffsetSec, 0, NTP_SERVER_1, NTP_SERVER_2);
    } else if (millis() - connectStartTime > CONNECT_TIMEOUT_MS) {
      connState = FAILED;
      Serial.println("Failed to connect (timeout).");
    }
  }

  readTouch();

  unsigned long now = millis();

  if (displayMode == MODE_CLOCK) {
    if (now - lastFrameMs >= CLOCK_REFRESH_RATE_MS) {
      lastFrameMs = now;
      u8g2.clearBuffer();
      drawClock();
      u8g2.sendBuffer();
    }
    return;
  }

  if (displayMode == MODE_SPOTIFY) {
    // Poll the Spotify API only occasionally (network calls are slow and
    // rate-limited); redraw more often so the scroll/progress bar are smooth.
    if (now - lastSpotifyPollMs >= SPOTIFY_POLL_INTERVAL_MS) {
      lastSpotifyPollMs = now;
      spotifyPollNowPlaying();
    }
    if (now - lastSpotifyRenderMs >= SPOTIFY_RENDER_INTERVAL_MS) {
      lastSpotifyRenderMs = now;
      u8g2.clearBuffer();
      drawSpotify();
      u8g2.sendBuffer();
    }
    return;
  }

  if (displayMode == MODE_NAV) {
    // Nothing to poll here - the phone pushes updates in via /nav/update.
    if (now - lastNavRenderMs >= NAV_RENDER_INTERVAL_MS) {
      lastNavRenderMs = now;
      u8g2.clearBuffer();
      drawNav();
      u8g2.sendBuffer();
    }
    return;
  }

  if (displayMode == MODE_REMINDER) {
    if (now - lastReminderRenderMs >= REMINDER_RENDER_INTERVAL_MS) {
      lastReminderRenderMs = now;
      u8g2.clearBuffer();
      drawReminder();
      u8g2.sendBuffer();
    }
    return;
  }

  unsigned long rate;
  switch (activeAnim) {
    case 0:  rate = MOCHI_FRAME_RATE_MS;   break;
    case 1:  rate = MAKER_FRAME_RATE_MS;   break;
    case 2:  rate = EYES_FRAME_RATE_MS;    break;
    case 3:  rate = HANDS_FRAME_RATE_MS;   break;
    case 4:  rate = FLAG_FRAME_RATE_MS;    break;
    case 5:  rate = ORBIT_FRAME_RATE_MS;   break;
    case 6:  rate = COMPASS_FRAME_RATE_MS; break;
    default: rate = MOCHI_FRAME_RATE_MS;   break;
  }

  if (now - lastFrameMs >= rate) {
    lastFrameMs = now;
    u8g2.clearBuffer();
    switch (activeAnim) {
      case 0:
        normalFrame++;
        if (normalFrame >= NORMAL_FRAME_COUNT) normalFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, normalFrames[normalFrame]);
        break;
      case 1:
        makerFrame++;
        if (makerFrame >= MAKER_FRAME_COUNT) makerFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, makerFrames[makerFrame]);
        break;
      case 2:
        eyesFrame++;
        if (eyesFrame >= EYES_FRAME_COUNT) eyesFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, eyesFrames[eyesFrame]);
        break;
      case 3:
        handsFrame++;
        if (handsFrame >= HANDS_FRAME_COUNT) handsFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, handsFrames[handsFrame]);
        break;
      case 4:
        flagFrame++;
        if (flagFrame >= FLAG_FRAME_COUNT) flagFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, flagFrames[flagFrame]);
        break;
      case 5:
        orbitFrame++;
        if (orbitFrame >= ORBIT_FRAME_COUNT) orbitFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, orbitFrames[orbitFrame]);
        break;
      case 6:
        compassFrame++;
        if (compassFrame >= COMPASS_FRAME_COUNT) compassFrame = 0;
        u8g2.drawXBMP(0, 0, 128, 64, compassFrames[compassFrame]);
        break;
    }
    u8g2.sendBuffer();
  }
}
