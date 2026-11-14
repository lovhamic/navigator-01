// web_ui.h
// The web control panel: HTML/JS page, WiFi scan/connect endpoints,
// animation-switch endpoints, and clock/font/time endpoints.
//
// Uses globals declared in the main .ino:
//   server, connState, pendingSSID, pendingPassword,
//   connectRequested, connectStartTime, CONNECT_TIMEOUT_MS,
//   activeAnim, lastFrameMs, displayMode,
//   timeFontIndex, dateFontIndex, TIME_FONT_COUNT, DATE_FONT_COUNT,
//   utcOffsetSec, NTP_SERVER_1, NTP_SERVER_2
//   spotifyIsAuthed(), spotifyClearAuth(), spotifyExchangeCode(),
//   SPOTIFY_CLIENT_ID, SPOTIFY_SCOPES, SPOTIFY_REDIRECT_URI,
//   spotifyUrlEncode() (all from spotify_client.h)
//   navSet(), navClear(), navIsActive(), navDirection, navMeters (navigation.h)
//   reminderSetText(), reminderSetDrawingFromHex(), reminderClear(),
//   reminderText (reminder.h)
#pragma once

// ================= WEB UI + HTTP HANDLERS =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Control</title>
  <style>
    body { font-family: Arial, sans-serif; background:#f2f2f2; margin:0; padding:20px; }
    .card { background:#fff; max-width:420px; margin:auto; padding:20px; border-radius:10px; box-shadow:0 2px 8px rgba(0,0,0,0.15); }
    h2 { text-align:center; color:#333; margin-top:0; }
    h3 { color:#333; margin-bottom:8px; }
    button { width:100%; padding:10px; margin-top:10px; border:none; border-radius:6px; background:#2196F3; color:#fff; font-size:16px; cursor:pointer; }
    button:hover { background:#1976D2; }
    .anim-btn.active { background:#4CAF50; }
    #networks { margin-top:15px; }
    .net-item { padding:10px; border:1px solid #ddd; border-radius:6px; margin-bottom:6px; cursor:pointer; display:flex; justify-content:space-between; }
    .net-item:hover { background:#f0f8ff; }
    input[type=password], input[type=text] { width:100%; padding:8px; margin-top:10px; border:1px solid #ccc; border-radius:6px; box-sizing:border-box; }
    #status { margin-top:15px; text-align:center; font-weight:bold; }
    .signal { color:#888; font-size:13px; }
    hr { margin:25px 0; border:none; border-top:1px solid #eee; }
    .hint { color:#888; font-size:12px; margin:4px 0 10px; }
    .field-label { display:block; margin-top:14px; font-size:13px; color:#555; font-weight:bold; }
    select { width:100%; padding:8px; margin-top:6px; border:1px solid #ccc; border-radius:6px; box-sizing:border-box; font-size:15px; background:#fff; }
    input[type=number] { width:100%; padding:8px; margin-top:6px; border:1px solid #ccc; border-radius:6px; box-sizing:border-box; }
  </style>
</head>
<body>
  <div class="card">
    <h2>ESP32 Control Panel</h2>

    <h3>Display Animation</h3>
    <p class="hint">Tap the device's touch button to switch between Animation and Clock. Buttons below only take effect while it's on the Animation screen.</p>
    <button class="anim-btn" id="animBtn0" onclick="setAnim(0)">DasaiMochi</button>
    <button class="anim-btn" id="animBtn1" onclick="setAnim(1)">OLED Animation Maker</button>
    <button class="anim-btn" id="animBtn2" onclick="setAnim(2)">Eyes</button>
    <button class="anim-btn" id="animBtn3" onclick="setAnim(3)">Hands</button>
    <button class="anim-btn" id="animBtn4" onclick="setAnim(4)">Flag</button>
    <button class="anim-btn" id="animBtn5" onclick="setAnim(5)">Orbit</button>
    <button class="anim-btn" id="animBtn6" onclick="setAnim(6)">Compass</button>

    <hr>

    <h3>Clock</h3>
    <p class="hint">Applies whenever the device is on the Clock screen.</p>

    <label class="field-label" for="timeFontSelect">Time font</label>
    <select id="timeFontSelect" onchange="setTimeFont(this.value)"></select>

    <label class="field-label" for="dateFontSelect">Date font</label>
    <select id="dateFontSelect" onchange="setDateFont(this.value)"></select>

    <label class="field-label" for="utcOffsetInput">Time zone (UTC offset, hours)</label>
    <input type="number" id="utcOffsetInput" step="0.25" value="5.5">
    <button onclick="applyUtcOffset()">Apply Time Zone</button>

    <button onclick="syncTimeFromPhone()">Sync Time From Phone</button>
    <div id="clockStatus" class="hint"></div>

    <hr>

    <h3>Spotify</h3>
    <p class="hint">Tap the touch button to cycle Animation → Clock → Spotify. Connect once here and the device will stay signed in.</p>
    <div id="spotifyStatus" class="hint">Checking...</div>
    <button onclick="connectSpotify()">Connect Spotify</button>
    <button onclick="disconnectSpotify()">Disconnect</button>

    <hr>

    <h3>Navigation</h3>
    <p class="hint">
      This screen relays turn-by-turn directions from your phone. GPS + Google Maps
      need a secure (https) page to run in the browser, so directions are computed on
      a separate companion page (nav_companion.html) which you open on your phone while
      driving - it sends updates here automatically. Enter this device's address into
      that page: <b><span id="deviceAddr"></span></b>
    </p>
    <div id="navStatus" class="hint">Checking...</div>
    <button onclick="clearNav()">Clear Navigation</button>

    <hr>

    <h3>Reminder</h3>
    <p class="hint">Type a message or draw something, then send it to the device.</p>

    <label class="field-label" for="reminderTextInput">Message</label>
    <input type="text" id="reminderTextInput" placeholder="e.g. Take the bins out">
    <label style="display:flex; align-items:center; gap:6px; margin-top:8px; font-size:13px; color:#555;">
      <input type="checkbox" id="reminderScrollToggle" checked style="width:auto;">
      Scroll if it doesn't fit on screen
    </label>
    <button onclick="sendReminderText()">Send Message</button>

    <label class="field-label">Or draw something</label>
    <canvas id="reminderCanvas" style="width:100%; aspect-ratio:2/1; border:1px solid #ccc; border-radius:6px; background:#fff; touch-action:none; display:block; margin-top:6px;"></canvas>
    <button onclick="clearCanvas()">Clear Canvas</button>
    <button onclick="sendDrawing()">Send Drawing</button>

    <div id="reminderStatus" class="hint"></div>
    <button onclick="clearReminder()">Clear Reminder</button>

    <hr>

    <h3>WiFi Setup</h3>
    <button onclick="scanNetworks()">Scan for Networks</button>
    <div id="networks"></div>

    <div id="connectForm" style="display:none;">
      <p>Selected: <b id="selectedSSID"></b></p>
      <input type="password" id="password" placeholder="Enter WiFi Password">
      <button onclick="connectWifi()">Connect</button>
    </div>

    <div id="status"></div>
  </div>

<script>
let chosenSSID = "";

// ---- Animation control ----
function setAnim(n) {
  fetch('/setAnim?anim=' + n)
    .then(() => updateAnimButtons(n))
    .catch(() => {});
}

const ANIM_COUNT = 7;

function updateAnimButtons(active) {
  for (let i = 0; i < ANIM_COUNT; i++) {
    document.getElementById('animBtn' + i).classList.toggle('active', active === i);
  }
}

function refreshAnimStatus() {
  fetch('/animStatus')
    .then(res => res.json())
    .then(data => updateAnimButtons(data.anim))
    .catch(() => {});
}

// ---- Clock control ----
// Keep these two lists in sync with the font arrays in clock_fonts.h.
const TIME_FONT_NAMES = ["Logisoso (big)", "Furore Bold", "Helvetica Bold", "Century Bold", "Courier Bold", "Times Bold", "Crox Heavy"];
const DATE_FONT_NAMES = ["6x10 Pixel", "7x13 Pixel", "9x15 Pixel", "Helvetica", "Century", "Courier", "Times"];

function populateFontSelects() {
  const timeSel = document.getElementById('timeFontSelect');
  TIME_FONT_NAMES.forEach((name, i) => {
    const opt = document.createElement('option');
    opt.value = i;
    opt.textContent = name;
    timeSel.appendChild(opt);
  });

  const dateSel = document.getElementById('dateFontSelect');
  DATE_FONT_NAMES.forEach((name, i) => {
    const opt = document.createElement('option');
    opt.value = i;
    opt.textContent = name;
    dateSel.appendChild(opt);
  });
}

function setTimeFont(n) {
  fetch('/setTimeFont?font=' + n).catch(() => {});
}

function setDateFont(n) {
  fetch('/setDateFont?font=' + n).catch(() => {});
}

function refreshFontStatus() {
  fetch('/fontStatus')
    .then(res => res.json())
    .then(data => {
      document.getElementById('timeFontSelect').value = data.timeFont;
      document.getElementById('dateFontSelect').value = data.dateFont;
      document.getElementById('utcOffsetInput').value = data.utcOffset / 3600;
    })
    .catch(() => {});
}

function applyUtcOffset() {
  const hours = parseFloat(document.getElementById('utcOffsetInput').value);
  const seconds = Math.round(hours * 3600);
  fetch('/setUtcOffset?offset=' + seconds)
    .then(() => refreshClockStatus())
    .catch(() => {});
}

function syncTimeFromPhone() {
  const epoch = Math.floor(Date.now() / 1000);
  fetch('/setTime?epoch=' + epoch)
    .then(() => {
      document.getElementById('clockStatus').innerText = "Synced from phone.";
      refreshClockStatus();
    })
    .catch(() => {
      document.getElementById('clockStatus').innerText = "Sync failed.";
    });
}

function refreshClockStatus() {
  fetch('/timeStatus')
    .then(res => res.json())
    .then(data => {
      document.getElementById('clockStatus').innerText = data.set
        ? "Device time: " + data.text
        : "No time set yet - connect to WiFi or tap Sync.";
    })
    .catch(() => {});
}

// ---- Spotify control ----
function connectSpotify() {
  // Opens Spotify's login page in a new tab; this device tab keeps polling
  // /spotify/status until the callback finishes.
  window.open('/spotify/login', '_blank');
  pollSpotifyStatus(0);
}

function disconnectSpotify() {
  fetch('/spotify/logout').then(() => refreshSpotifyStatus()).catch(() => {});
}

function refreshSpotifyStatus() {
  fetch('/spotify/status')
    .then(res => res.json())
    .then(data => {
      document.getElementById('spotifyStatus').innerText =
        data.connected ? "Connected" : "Not connected";
    })
    .catch(() => {});
}

function pollSpotifyStatus(attempt) {
  fetch('/spotify/status')
    .then(res => res.json())
    .then(data => {
      if (data.connected) {
        document.getElementById('spotifyStatus').innerText = "Connected";
      } else if (attempt < 30) {
        document.getElementById('spotifyStatus').innerText = "Waiting for login in the other tab...";
        setTimeout(() => pollSpotifyStatus(attempt + 1), 2000);
      } else {
        document.getElementById('spotifyStatus').innerText = "Not connected";
      }
    })
    .catch(() => {});
}

// ---- Navigation (data itself comes from nav_companion.html on your phone) ----
document.getElementById('deviceAddr').innerText = window.location.host;

function refreshNavStatus() {
  fetch('/nav/status')
    .then(res => res.json())
    .then(data => {
      document.getElementById('navStatus').innerText = data.active
        ? ("Relaying: " + data.direction + ", " + Math.round(data.meters) + " m")
        : "No active navigation";
    })
    .catch(() => {});
}

function clearNav() {
  fetch('/nav/clear').then(() => refreshNavStatus()).catch(() => {});
}

// ---- Reminder: text message ----
function sendReminderText() {
  const text = document.getElementById('reminderTextInput').value;
  const scroll = document.getElementById('reminderScrollToggle').checked ? 1 : 0;
  fetch('/reminder/text', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'text=' + encodeURIComponent(text) + '&scroll=' + scroll
  })
  .then(() => { document.getElementById('reminderStatus').innerText = "Message sent!"; })
  .catch(() => { document.getElementById('reminderStatus').innerText = "Send failed"; });
}

function clearReminder() {
  fetch('/reminder/clear')
    .then(() => { document.getElementById('reminderStatus').innerText = "Cleared"; })
    .catch(() => {});
}

// ---- Reminder: drawing canvas ----
const reminderCanvas = document.getElementById('reminderCanvas');
const reminderCtx = reminderCanvas.getContext('2d');
reminderCanvas.width = 128;
reminderCanvas.height = 64;
reminderCtx.fillStyle = '#fff';
reminderCtx.fillRect(0, 0, 128, 64);
reminderCtx.strokeStyle = '#000';
reminderCtx.lineWidth = 3;
reminderCtx.lineCap = 'round';

let reminderDrawing = false;
let reminderLastX = 0, reminderLastY = 0;

function reminderCanvasPos(e) {
  const rect = reminderCanvas.getBoundingClientRect();
  const scaleX = reminderCanvas.width / rect.width;
  const scaleY = reminderCanvas.height / rect.height;
  const clientX = e.touches ? e.touches[0].clientX : e.clientX;
  const clientY = e.touches ? e.touches[0].clientY : e.clientY;
  return { x: (clientX - rect.left) * scaleX, y: (clientY - rect.top) * scaleY };
}

function reminderStartDraw(e) {
  reminderDrawing = true;
  const p = reminderCanvasPos(e);
  reminderLastX = p.x; reminderLastY = p.y;
  e.preventDefault();
}
function reminderMoveDraw(e) {
  if (!reminderDrawing) return;
  const p = reminderCanvasPos(e);
  reminderCtx.beginPath();
  reminderCtx.moveTo(reminderLastX, reminderLastY);
  reminderCtx.lineTo(p.x, p.y);
  reminderCtx.stroke();
  reminderLastX = p.x; reminderLastY = p.y;
  e.preventDefault();
}
function reminderEndDraw() { reminderDrawing = false; }

reminderCanvas.addEventListener('mousedown', reminderStartDraw);
reminderCanvas.addEventListener('mousemove', reminderMoveDraw);
reminderCanvas.addEventListener('mouseup', reminderEndDraw);
reminderCanvas.addEventListener('mouseleave', reminderEndDraw);
reminderCanvas.addEventListener('touchstart', reminderStartDraw);
reminderCanvas.addEventListener('touchmove', reminderMoveDraw);
reminderCanvas.addEventListener('touchend', reminderEndDraw);

function clearCanvas() {
  reminderCtx.fillStyle = '#fff';
  reminderCtx.fillRect(0, 0, reminderCanvas.width, reminderCanvas.height);
}

function sendDrawing() {
  const img = reminderCtx.getImageData(0, 0, 128, 64).data; // RGBA
  let hex = "";
  for (let y = 0; y < 64; y++) {
    for (let g = 0; g < 16; g++) { // 16 groups of 8px per row (128/8)
      let byte = 0;
      for (let b = 0; b < 8; b++) {
        const x = g * 8 + b;
        const idx = (y * 128 + x) * 4;
        const lum = (img[idx] + img[idx + 1] + img[idx + 2]) / 3;
        const on = lum < 128 ? 1 : 0;      // dark pixel = "on" (matches XBM convention)
        byte |= (on << b);                 // bit0 = leftmost pixel of the group
      }
      hex += byte.toString(16).padStart(2, '0');
    }
  }
  fetch('/reminder/drawing', {
    method: 'POST',
    headers: { 'Content-Type': 'text/plain' },
    body: hex
  })
  .then(() => { document.getElementById('reminderStatus').innerText = "Drawing sent!"; })
  .catch(() => { document.getElementById('reminderStatus').innerText = "Send failed"; });
}

// ---- WiFi scan/connect ----
function scanNetworks() {
  document.getElementById('networks').innerHTML = "Scanning...";
  fetch('/scan')
    .then(res => res.json())
    .then(data => {
      let html = "";
      data.forEach(net => {
        html += `<div class="net-item" onclick="selectNetwork('${net.ssid}')">
                   <span>${net.ssid}</span>
                   <span class="signal">${net.rssi} dBm ${net.secure ? '[Secured]' : ''}</span>
                 </div>`;
      });
      document.getElementById('networks').innerHTML = html || "No networks found";
    })
    .catch(() => {
      document.getElementById('networks').innerHTML = "Scan failed";
    });
}

function selectNetwork(ssid) {
  chosenSSID = ssid;
  document.getElementById('selectedSSID').innerText = ssid;
  document.getElementById('connectForm').style.display = 'block';
}

function connectWifi() {
  const password = document.getElementById('password').value;
  document.getElementById('status').innerText = "Connecting...";

  fetch('/connect', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'ssid=' + encodeURIComponent(chosenSSID) + '&password=' + encodeURIComponent(password)
  })
  .then(() => {
    pollStatus(0);
  })
  .catch(() => {
    document.getElementById('status').innerText = "Connection request failed";
  });
}

function pollStatus(attempt) {
  fetch('/status')
    .then(res => res.json())
    .then(data => {
      if (data.state === "connected") {
        document.getElementById('status').innerText = "Connected! IP: " + data.ip;
      } else if (data.state === "failed") {
        document.getElementById('status').innerText = "Failed to connect. Check password / signal.";
      } else if (attempt < 20) {
        document.getElementById('status').innerText = "Connecting" + ".".repeat((attempt % 4));
        setTimeout(() => pollStatus(attempt + 1), 500);
      } else {
        document.getElementById('status').innerText = "Timed out waiting for connection.";
      }
    })
    .catch(() => {
      document.getElementById('status').innerText = "Lost contact with ESP32 (did it switch networks?)";
    });
}

populateFontSelects();
refreshAnimStatus();
refreshFontStatus();
refreshClockStatus();
refreshSpotifyStatus();
refreshNavStatus();
setInterval(refreshClockStatus, 5000);
setInterval(refreshNavStatus, 3000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
  WiFi.scanDelete();
}

void handleConnect() {
  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    server.send(400, "text/plain", "Missing SSID or password");
    return;
  }

  // Just record the request and return immediately.
  // The actual WiFi.begin() happens in loop() so the web server
  // never blocks and the page can poll /status for real progress.
  pendingSSID = server.arg("ssid");
  pendingPassword = server.arg("password");
  connectRequested = true;
  connState = CONNECTING;

  server.send(200, "text/plain", "Connection attempt queued");
}

void handleStatus() {
  String state;
  switch (connState) {
    case CONNECTING: state = "connecting"; break;
    case CONNECTED:  state = "connected";  break;
    case FAILED:     state = "failed";     break;
    default:         state = "idle";       break;
  }

  String json = "{\"state\":\"" + state + "\"";
  if (connState == CONNECTED) {
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetAnim() {
  if (!server.hasArg("anim")) {
    server.send(400, "text/plain", "Missing anim parameter");
    return;
  }
  int anim = server.arg("anim").toInt();
  if (anim < 0 || anim > 6) {
    server.send(400, "text/plain", "anim must be between 0 and 6");
    return;
  }
  activeAnim = anim;
  lastFrameMs = millis();
  server.send(200, "text/plain", "OK");
}

void handleAnimStatus() {
  String json = "{\"anim\":" + String(activeAnim) + ",\"mode\":" + String(displayMode) + "}";
  server.send(200, "application/json", json);
}

void handleSetTimeFont() {
  if (!server.hasArg("font")) {
    server.send(400, "text/plain", "Missing font parameter");
    return;
  }
  int f = server.arg("font").toInt();
  if (f < 0 || f >= TIME_FONT_COUNT) {
    server.send(400, "text/plain", "font out of range");
    return;
  }
  timeFontIndex = f;
  server.send(200, "text/plain", "OK");
}

void handleSetDateFont() {
  if (!server.hasArg("font")) {
    server.send(400, "text/plain", "Missing font parameter");
    return;
  }
  int f = server.arg("font").toInt();
  if (f < 0 || f >= DATE_FONT_COUNT) {
    server.send(400, "text/plain", "font out of range");
    return;
  }
  dateFontIndex = f;
  server.send(200, "text/plain", "OK");
}

void handleFontStatus() {
  String json = "{\"timeFont\":" + String(timeFontIndex) +
                ",\"dateFont\":" + String(dateFontIndex) +
                ",\"utcOffset\":" + String(utcOffsetSec) + "}";
  server.send(200, "application/json", json);
}

void handleSetUtcOffset() {
  if (!server.hasArg("offset")) {
    server.send(400, "text/plain", "Missing offset parameter");
    return;
  }
  utcOffsetSec = server.arg("offset").toInt();
  configTime(utcOffsetSec, 0, NTP_SERVER_1, NTP_SERVER_2);
  server.send(200, "text/plain", "OK");
}

void handleSyncTime() {
  if (!server.hasArg("epoch")) {
    server.send(400, "text/plain", "Missing epoch parameter");
    return;
  }
  time_t epoch = (time_t) server.arg("epoch").toInt();
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  server.send(200, "text/plain", "OK");
}

void handleTimeStatus() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5)) {
    server.send(200, "application/json", "{\"set\":false}");
    return;
  }
  char buf[40];
  strftime(buf, sizeof(buf), "%H:%M:%S, %a %d %b %Y", &timeinfo);
  String json = "{\"set\":true,\"text\":\"" + String(buf) + "\"}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ---- Spotify handlers ----
void handleSpotifyLogin() {
  String url = "https://accounts.spotify.com/authorize";
  url += "?response_type=code";
  url += "&client_id=" + String(SPOTIFY_CLIENT_ID);
  url += "&scope=" + spotifyUrlEncode(SPOTIFY_SCOPES);
  url += "&redirect_uri=" + spotifyUrlEncode(SPOTIFY_REDIRECT_URI);
  // state = this device's address as seen by the browser, so the bounce page
  // knows where to send the auth code back to (Spotify echoes it untouched).
  url += "&state=" + spotifyUrlEncode(server.hostHeader());
  url += "&show_dialog=true";
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "");
}

void handleSpotifyCallback() {
  if (server.hasArg("error")) {
    server.send(200, "text/html",
      "<html><body>Spotify login was cancelled. You can close this tab.</body></html>");
    return;
  }
  if (!server.hasArg("code")) {
    server.send(400, "text/plain", "Missing code");
    return;
  }
  bool ok = spotifyExchangeCode(server.arg("code"));
  if (ok) {
    server.send(200, "text/html",
      "<html><body><h3>Spotify connected!</h3><p>You can close this tab and go back to the control panel.</p></body></html>");
  } else {
    server.send(200, "text/html",
      "<html><body><h3>Something went wrong.</h3><p>Check the Serial Monitor and try again.</p></body></html>");
  }
}

void handleSpotifyStatus() {
  String json = "{\"connected\":" + String(spotifyIsAuthed() ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleSpotifyLogout() {
  spotifyClearAuth();
  server.send(200, "text/plain", "OK");
}

// ---- Reminder handlers ----
void handleReminderSetText() {
  if (!server.hasArg("text")) {
    server.send(400, "text/plain", "Missing text parameter");
    return;
  }
  bool scroll = true;
  if (server.hasArg("scroll")) {
    scroll = server.arg("scroll") != "0";
  }
  reminderSetText(server.arg("text"), scroll);
  server.send(200, "text/plain", "OK");
}

void handleReminderSetDrawing() {
  // Raw text/plain body lands in the "plain" arg.
  String hex = server.hasArg("plain") ? server.arg("plain") : "";
  if (!reminderSetDrawingFromHex(hex)) {
    server.send(400, "text/plain", "Bad drawing data (expected 2048 hex chars)");
    return;
  }
  server.send(200, "text/plain", "OK");
}

void handleReminderClear() {
  reminderClear();
  server.send(200, "text/plain", "OK");
}

// ---- Navigation handlers ----
// nav_companion.html is hosted elsewhere (https) and calls these from a
// different origin, so we need CORS + Chrome's Private Network Access
// header to let a public/https page reach this device on the local network.
void addNavCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void handleNavOptions() {
  addNavCorsHeaders();
  server.send(204);
}

void handleNavUpdate() {
  addNavCorsHeaders();
  if (!server.hasArg("dir") || !server.hasArg("meters")) {
    server.send(400, "text/plain", "Missing dir or meters parameter");
    return;
  }
  navSet(server.arg("dir"), server.arg("meters").toFloat());
  server.send(200, "text/plain", "OK");
}

void handleNavStatus() {
  addNavCorsHeaders();
  bool active = navIsActive();
  String json = "{\"active\":" + String(active ? "true" : "false");
  if (active) {
    json += ",\"direction\":\"" + navDirection + "\"";
    json += ",\"meters\":" + String(navMeters);
  }
  json += "}";
  server.send(200, "application/json", json);
}

void handleNavClear() {
  addNavCorsHeaders();
  navClear();
  server.send(200, "text/plain", "OK");
}

