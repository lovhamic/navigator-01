# REST API Specification

The ESP32 firmware hosts an asynchronous HTTP server on port 80. It exposes an administrative web interface and a comprehensive set of RESTful endpoints to configure networking, switch display modes, customize fonts, authenticate external services, and stream real-time telemetry.

---

## 1. Network Management Endpoints

### 1.1. Root Dashboard
- Route: `GET /`
- Description: Serves the compiled single-page administrative control panel (`text/html`).
- Response: `200 OK` (HTML document).

### 1.2. WiFi Scan
- Route: `GET /scan`
- Description: Triggers a non-blocking wireless site survey and returns an array of visible SSIDs.
- Response: `200 OK` (`application/json`)
```json
[
  {
    "ssid": "HomeNetwork",
    "rssi": -58,
    "secure": true
  },
  {
    "ssid": "Guest_Open",
    "rssi": -72,
    "secure": false
  }
]
```

### 1.3. WiFi Connect
- Route: `POST /connect`
- Content-Type: `application/x-www-form-urlencoded`
- Request Parameters:
  - `ssid` (string, required): Target access point SSID.
  - `password` (string, required): WPA/WPA2 pre-shared key.
- Description: Queues a non-blocking connection request.
- Response: `200 OK` (`text/plain: Connection attempt queued`) or `400 Bad Request`.

### 1.4. Connection Status
- Route: `GET /status`
- Description: Retrieves current Station mode connection state and assigned IP address.
- Response: `200 OK` (`application/json`)
```json
{
  "state": "connected",
  "ip": "192.168.1.50"
}
```
Possible values for `state`: `"idle"`, `"connecting"`, `"connected"`, `"failed"`.

---

## 2. Animation Control Endpoints

### 2.1. Set Active Animation
- Route: `GET /setAnim`
- Query Parameters:
  - `anim` (integer, required): Animation frame sequence index (0 to 6).
    - `0`: DasaiMochi (Normal Face)
    - `1`: OLED Animation Maker
    - `2`: Eyes
    - `3`: Hands
    - `4`: Flag
    - `5`: Orbit
    - `6`: Compass
- Response: `200 OK` (`text/plain: OK`) or `400 Bad Request`.

### 2.2. Get Animation Status
- Route: `GET /animStatus`
- Description: Returns active animation index and current hardware display mode.
- Response: `200 OK` (`application/json`)
```json
{
  "anim": 0,
  "mode": 0
}
```
Display mode values:
- `0`: Animation
- `1`: Clock
- `2`: Spotify
- `3`: Navigation
- `4`: Reminder

---

## 3. Clock and Time Synchronization Endpoints

### 3.1. Set Time Font
- Route: `GET /setTimeFont`
- Query Parameters:
  - `font` (integer, required): Time font index (0 to 6).
- Response: `200 OK` (`text/plain: OK`) or `400 Bad Request`.

### 3.2. Set Date Font
- Route: `GET /setDateFont`
- Query Parameters:
  - `font` (integer, required): Date font index (0 to 6).
- Response: `200 OK` (`text/plain: OK`) or `400 Bad Request`.

### 3.3. Get Font and Timezone Status
- Route: `GET /fontStatus`
- Response: `200 OK` (`application/json`)
```json
{
  "timeFont": 0,
  "dateFont": 0,
  "utcOffset": 19800
}
```

### 3.4. Set UTC Offset
- Route: `GET /setUtcOffset`
- Query Parameters:
  - `offset` (integer, required): UTC offset in seconds (e.g. `19800` for UTC+5:30, `-18000` for UTC-5).
- Description: Updates timezone offset and immediately kicks SNTP sync.
- Response: `200 OK` (`text/plain: OK`).

### 3.5. Direct Epoch Time Sync
- Route: `GET /setTime`
- Query Parameters:
  - `epoch` (integer, required): Current Unix epoch time in seconds.
- Description: Immediately programs the ESP32 internal RTC clock via `settimeofday`.
- Response: `200 OK` (`text/plain: OK`).

### 3.6. Get Time Status
- Route: `GET /timeStatus`
- Description: Returns human-readable RTC time formatted as `%H:%M:%S, %a %d %b %Y`.
- Response: `200 OK` (`application/json`)
```json
{
  "set": true,
  "text": "14:32:05, Mon 16 Nov 2026"
}
```

---

## 4. Spotify Integration Endpoints

### 4.1. Spotify Login Redirect
- Route: `GET /spotify/login`
- Description: Returns an HTTP 302 Found redirect pointing to Spotify's authorization portal (`https://accounts.spotify.com/authorize`) with required OAuth scopes and the bounce URI.
- Response: `302 Found`.

### 4.2. Spotify OAuth Callback
- Route: `GET /spotify/callback`
- Query Parameters:
  - `code` (string, required): Authorization code received from the bounce proxy.
  - `error` (string, optional): Error identifier if user cancelled login.
- Description: Trades authorization code for access and refresh tokens, storing refresh token into ESP32 NVS flash storage (`Preferences`).
- Response: `200 OK` (Confirmation HTML).

### 4.3. Spotify Authentication Status
- Route: `GET /spotify/status`
- Description: Checks whether a valid refresh token exists in NVS storage.
- Response: `200 OK` (`application/json`)
```json
{
  "connected": true
}
```

### 4.4. Spotify Logout
- Route: `GET /spotify/logout`
- Description: Flushes access token from RAM and purges saved refresh token from NVS storage.
- Response: `200 OK` (`text/plain: OK`).

---

## 5. Navigation Telemetry Endpoints

> Note: All navigation endpoints include Cross-Origin Resource Sharing (CORS) headers (`Access-Control-Allow-Origin: *`) and Chrome Private Network Access headers (`Access-Control-Allow-Private-Network: true`) to allow browser-based companion web apps hosted on HTTPS domains to push telemetry to local network devices.

### 5.1. Navigation Pre-flight
- Route: `OPTIONS /nav/update`
- Description: Pre-flight CORS handler.
- Response: `204 No Content`.

### 5.2. Ingest Navigation Update
- Route: `POST /nav/update`
- Content-Type: `application/x-www-form-urlencoded`
- Request Parameters:
  - `dir` (string, required): Directional maneuver indicator. Supported values:
    - `"straight"`
    - `"slight-right"`
    - `"right"`
    - `"sharp-right"`
    - `"uturn"`
    - `"sharp-left"`
    - `"left"`
    - `"slight-left"`
    - `"arrive"`
  - `meters` (float or integer, required): Distance to the maneuver in meters.
- Description: Updates internal navigation state and resets the 15-second staleness timer.
- Response: `200 OK` (`text/plain: OK`).

### 5.3. Get Navigation Status
- Route: `GET /nav/status`
- Description: Returns active navigation telemetry or inactive flag if stale.
- Response: `200 OK` (`application/json`)
```json
{
  "active": true,
  "direction": "left",
  "meters": 120.0
}
```

### 5.4. Clear Navigation
- Route: `GET /nav/clear`
- Description: Flushes active navigation instruction and resets distance to zero.
- Response: `200 OK` (`text/plain: OK`).

---

## 6. Reminder and Drawing Endpoints

### 6.1. Set Text Reminder
- Route: `POST /reminder/text`
- Content-Type: `application/x-www-form-urlencoded`
- Request Parameters:
  - `text` (string, required): Message text to display.
  - `scroll` (integer, optional): `1` to enable horizontal marquee scroll, `0` for static truncated display. Defaults to `1`.
- Response: `200 OK` (`text/plain: OK`).

### 6.2. Set Drawing Reminder
- Route: `POST /reminder/drawing`
- Content-Type: `text/plain`
- Payload: Raw hexadecimal string containing exactly 2048 characters (representing 1024 bytes of 1-bit monochrome 128x64 XBM bitmap format).
- Description: Parses hexadecimal bitmap and loads it into the display frame buffer.
- Response: `200 OK` (`text/plain: OK`) or `400 Bad Request` if payload length is invalid.

### 6.3. Clear Reminder
- Route: `GET /reminder/clear`
- Description: Clears any active text or drawing reminder.
- Response: `200 OK` (`text/plain: OK`).
