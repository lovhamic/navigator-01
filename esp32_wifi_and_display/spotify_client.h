
//Spotify client · H
// spotify_client.h
// Spotify OAuth + "now playing" client for the ESP32.
//
// Flow: the web app opens /spotify/login, which 302s the browser to Spotify's
// authorize page. Spotify requires the redirect_uri to be HTTPS, but this
// device only speaks plain http on a LAN address, so the browser is sent to
// an HTTPS bounce page instead (SPOTIFY_REDIRECT_URI). That page picks the
// ?code=... out of the URL and forwards it to this device's
// /spotify/callback endpoint. We then swap the code for an access+refresh
// token pair, persist the refresh token in NVS Preferences so the device
// stays signed in across reboots, and poll the API for the current track.
//
// Consumed by:
//   esp32_wifi_and_display.ino  -> spotifyNow, spotifyPollNowPlaying(),
//                                  spotifyLoadRefreshToken(),
//                                  spotifyEstimatedProgressMs()
//   web_ui.h                    -> SPOTIFY_CLIENT_ID, SPOTIFY_SCOPES,
//                                  SPOTIFY_REDIRECT_URI, spotifyUrlEncode(),
//                                  spotifyExchangeCode(), spotifyIsAuthed(),
//                                  spotifyClearAuth()
#pragma once
 
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
 
// ---- Spotify app credentials (create an app on developer.spotify.com) ----
// MUST EDIT: replace these two with your app's Client ID and Client Secret.
// Uses an OAuth code exchange on the device itself (confidential client),
// so no PKCE or extra code_challenge plumbing is needed in the web handlers.
const char* SPOTIFY_CLIENT_ID     = "YOUR_SPOTIFY_CLIENT_ID";
const char* SPOTIFY_CLIENT_SECRET = "YOUR_SPOTIFY_CLIENT_SECRET";
const char* SPOTIFY_SCOPES        = "user-read-currently-playing user-read-playback-state";
 
// Spotify only allows HTTPS redirect URIs, and it must exactly match the one
// registered in the Spotify developer dashboard. This is an HTTPS bounce page
// (hosted on your GitHub Pages) rather than the ESP32's own address - the
// phone browser can never reach the device's plain-http LAN IP after leaving
// Spotify's domain, so Spotify returns the auth code to this HTTPS page,
// which then forwards it to the device's /spotify/callback.
const char* SPOTIFY_REDIRECT_URI = "https://<YOUR_GITHUB_USERNAME>.github.io/<YOUR_REPO_NAME>/spotify-bounce/";
 
#define SPOTIFY_AUTH_HOST      "accounts.spotify.com"
#define SPOTIFY_API_HOST       "api.spotify.com"
#define SPOTIFY_TOKEN_PATH     "/api/token"
#define SPOTIFY_CURRENT_PATH   "/v1/me/player/currently-playing"
 
// Refresh the access token a minute early so we never slip out between polls.
#define SPOTIFY_TOKEN_SKEW_MS  60000
#define SPOTIFY_FETCH_TIMEOUT_MS 8000
 
// Spotify's TLS chain: *.spotify.com leaf -> DigiCert Global G2 TLS RSA
// SHA256 2020 CA1 (intermediate) -> DigiCert Global Root G2 (root). Both
// certs are concatenated into ONE string below (mbedTLS can parse multiple
// PEM blocks out of a single buffer) because WiFiClientSecure::setCACert()
// just stores a pointer and does not accumulate across repeated calls -
// calling it once per cert would silently overwrite with only the last one,
// leaving no trusted root and breaking the handshake. Refresh if Spotify
// ever rotates CAs.
const char* SPOTIFY_ROOT_CA PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIE9DCCA9ygAwIBAgIQCF+UwC2Fe+jMFP9T7aI+KjANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0yMDA5MjQwMDAwMDBaFw0zMDA5MjMyMzU5NTlaMFkxCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxMzAxBgNVBAMTKkRpZ2lDZXJ0IEdsb2Jh
bCBHMiBUTFMgUlNBIFNIQTI1NiAyMDIwIENBMTCCASIwDQYJKoZIhvcNAQEBBQAD
ggEPADCCAQoCggEBAMz3EGJPprtjb+2QUlbFbSd7ehJWivH0+dbn4Y+9lavyYEEV
cNsSAPonCrVXOFt9slGTcZUOakGUWzUb+nv6u8W+JDD+Vu/E832X4xT1FE3LpxDy
FuqrIvAxIhFhaZAmunjZlx/jfWardUSVc8is/+9dCopZQ+GssjoP80j812s3wWPc
3kbW20X+fSP9kOhRBx5Ro1/tSUZUfyyIxfQTnJcVPAPooTncaQwywa8WV0yUR0J8
osicfebUTVSvQpmowQTCd5zWSOTOEeAqgJnwQ3DPP3Zr0UxJqyRewg2C/Uaoq2yT
zGJSQnWS+Jr6Xl6ysGHlHx+5fwmY6D36g39HaaECAwEAAaOCAa4wggGqMB0GA1Ud
DgQWBBR0hYDAZsffN97PvSk3qgMdvu3NFzAfBgNVHSMEGDAWgBROIlQgGJXm427m
D/r6uRLtBhePOTAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEG
CCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8CAQAwdgYIKwYBBQUHAQEEajBoMCQG
CCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wQAYIKwYBBQUHMAKG
NGh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RH
Mi5jcnQwewYDVR0fBHQwcjA3oDWgM4YxaHR0cDovL2NybDMuZGlnaWNlcnQuY29t
L0RpZ2lDZXJ0R2xvYmFsUm9vdEcyLmNybDA3oDWgM4YxaHR0cDovL2NybDQuZGln
aWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsUm9vdEcyLmNybDAwBgNVHSAEKTAnMAcG
BWeBDAEBMAgGBmeBDAECATAIBgZngQwBAgIwCAYGZ4EMAQIDMA0GCSqGSIb3DQEB
CwUAA4IBAQB1i8A8W+//cFxrivUh76wx5kM9gK/XVakew44YbHnT96xC34+IxZ20
dfPJCP2K/lHz8p0gGgQ1zvi2QXmv/8yWXpTTmh1wLqIxi/ulzH9W3xc3l7/BjUOG
q4xmfrnti/EPjLXUVa9ciZ7gpyptsqNjMhg7y961n4OzEQGsIA2QlxK3KZw1tdeR
Du9Ab21cO72h2fviyy52QNI6uyy/FgvqvQNbTpg6Ku0FUAcVkzxzOZGUWkgOxtNK
Aa9mObm9QjQc2wgD80D8EuiuPKuK1ftyeWSm4w5VsTuVP61gM2eKrLanXPDtWlIb
1GHhJRLmB7WqlLLwKPZhJl5VHPgB63dx
-----END CERTIFICATE-----
  )CERT";
 
// ---- Token / session state ----
Preferences spotifyPrefs;
String spotifyAccessToken;
String spotifyRefreshToken;
unsigned long spotifyTokenExpiresAtMs = 0;
 
// Latest snapshot of the Spotify player, shared with the render code.
struct SpotifyNow {
  bool authed = false;            // saved refresh token present (signed in)
  bool valid = false;             // last poll returned an active player
  bool isPlaying = false;
  String title;
  String artist;
  unsigned long durationMs = 0;
  unsigned long progressMs = 0;
  unsigned long pollAtMs = 0;     // millis() when this snapshot was taken
};
SpotifyNow spotifyNow;
 
// ---- TLS helper: one secure client, CA bundle configured once ----
WiFiClientSecure* spotifyClient() {
  static WiFiClientSecure client;
  static bool caSet = false;
  if (!caSet) {
    client.setCACert(SPOTIFY_ROOT_CA);
    caSet = true;
  }
  return &client;
}
 
// ---- Tiny typed-JSON helpers (no ArduinoJson dependency) ----
static size_t spotifyFindKey(const String& json, const char* key) {
  return json.indexOf("\"" + String(key) + "\":");
}
 
static String spotifyJsonString(const String& json, const char* key, bool last = false) {
  String needle = "\"" + String(key) + "\":";
  size_t pos = last ? json.lastIndexOf(needle) : json.indexOf(needle);
  if (pos == (size_t)-1) return "";
  pos += needle.length();
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (pos >= json.length() || json[pos] != '"') return "";
  pos++;
  String out;
  while (pos < json.length()) {
    char c = json[pos];
    if (c == '\\' && pos + 1 < json.length()) { out += json[pos + 1]; pos += 2; }
    else if (c == '"') break;
    else out += c;
    pos++;
  }
  return out;
}
 
static long spotifyJsonLong(const String& json, const char* key) {
  size_t pos = spotifyFindKey(json, key);
  if (pos == (size_t)-1) return 0;
  pos += String("\"" + String(key) + "\":").length();
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  return atol(json.c_str() + pos);
}
 
static bool spotifyJsonBool(const String& json, const char* key) {
  size_t pos = spotifyFindKey(json, key);
  if (pos == (size_t)-1) return false;
  pos += String("\"" + String(key) + "\":").length();
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  return json.startsWith(String("true"), pos);
}
 
// Returns the value of a `"key":{...}` object, braces included.
static String spotifyJsonObject(const String& json, const char* key) {
  size_t pos = spotifyFindKey(json, key);
  if (pos == (size_t)-1) return "";
  pos += String("\"" + String(key) + "\":").length();
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (pos >= json.length() || json[pos] != '{') return "";
  int depth = 0;
  size_t i = pos;
  for (; i < json.length(); i++) {
    if (json[i] == '{') depth++;
    else if (json[i] == '}') { depth--; if (depth == 0) break; }
  }
  return json.substring(pos, i + 1);
}
 
// Returns the first `{...}` element of a `"key":[{...}, ...]` array.
static String spotifyJsonArrayFirstObject(const String& json, const char* key) {
  size_t pos = spotifyFindKey(json, key);
  if (pos == (size_t)-1) return "";
  pos += String("\"" + String(key) + "\":").length();
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (pos >= json.length() || json[pos] != '[') return "";
  pos++;
  while (pos < json.length() && json[pos] != '{') pos++;
  if (pos >= json.length()) return "";
  int depth = 0;
  size_t i = pos;
  for (; i < json.length(); i++) {
    if (json[i] == '{') depth++;
    else if (json[i] == '}') { depth--; if (depth == 0) break; }
  }
  return json.substring(pos, i + 1);
}
 
// ---- URL encoding for query strings / form bodies ----
String spotifyUrlEncode(const String& s) {
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      out += hex;
    }
  }
  return out;
}
 
// ---- Storage of the refresh token in NVS ----
void spotifySaveRefreshToken(const String& token) {
  spotifyPrefs.begin("spotify", false);
  spotifyPrefs.putString("refresh", token);
  spotifyPrefs.end();
}
 
void spotifyLoadRefreshToken() {
  spotifyPrefs.begin("spotify", true);
  spotifyRefreshToken = spotifyPrefs.getString("refresh", "");
  spotifyPrefs.end();
  spotifyNow.authed = spotifyRefreshToken.length() > 0;
  if (spotifyNow.authed) {
    Serial.println("Spotify: found saved refresh token");
  }
}
 
// ---- Low-level HTTPS helpers ----
bool spotifyHttpsPost(const char* host, const String& path, const String& body,
                      String& outResponse, int& outStatus) {
  WiFiClientSecure* c = spotifyClient();
  HTTPClient http;
  http.setConnectTimeout(SPOTIFY_FETCH_TIMEOUT_MS);
  http.setTimeout(SPOTIFY_FETCH_TIMEOUT_MS);
  Serial.println("Spotify: free heap before POST: " + String(ESP.getFreeHeap()));
  if (!http.begin(*c, "https://" + String(host) + path)) {
    Serial.println("Spotify: http.begin() failed (bad URL?)");
    http.end();
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("User-Agent", "ESP32-Spotify/1.0");
  outStatus = http.POST(body);
  if (outStatus > 0) {
    outResponse = http.getString();
  } else {
    Serial.println("Spotify: POST failed, code " + String(outStatus) +
                    " (" + http.errorToString(outStatus) + "), free heap: " +
                    String(ESP.getFreeHeap()));
  }
  http.end();
  return outStatus > 0;
}
 
bool spotifyHttpsGet(const char* host, const String& path, const String& bearer,
                     String& outResponse, int& outStatus) {
  WiFiClientSecure* c = spotifyClient();
  HTTPClient http;
  http.setConnectTimeout(SPOTIFY_FETCH_TIMEOUT_MS);
  http.setTimeout(SPOTIFY_FETCH_TIMEOUT_MS);
  if (!http.begin(*c, "https://" + String(host) + path)) {
    http.end();
    return false;
  }
  http.addHeader("Authorization", "Bearer " + bearer);
  http.addHeader("User-Agent", "ESP32-Spotify/1.0");
  outStatus = http.GET();
  if (outStatus > 0) outResponse = http.getString();
  http.end();
  return outStatus > 0;
}
 
// ---- OAuth ----
// Swaps the authorization code (forwarded from the bounce page) for an access
// token + refresh token. Called from handleSpotifyCallback().
bool spotifyExchangeCode(const String& code) {
  if (code.length() == 0) return false;
 
  String body = "grant_type=authorization_code";
  body += "&code=" + spotifyUrlEncode(code);
  body += "&redirect_uri=" + spotifyUrlEncode(String(SPOTIFY_REDIRECT_URI));
  body += "&client_id=" + String(SPOTIFY_CLIENT_ID);
  body += "&client_secret=" + String(SPOTIFY_CLIENT_SECRET);
 
  String resp;
  int status;
  if (!spotifyHttpsPost(SPOTIFY_AUTH_HOST, SPOTIFY_TOKEN_PATH, body, resp, status)) {
    Serial.println("Spotify: token exchange request failed");
    return false;
  }
  if (status != 200) {
    Serial.println("Spotify: token exchange failed HTTP " + String(status) + ": " + resp);
    return false;
  }
 
  spotifyAccessToken = spotifyJsonString(resp, "access_token");
  String newRefresh = spotifyJsonString(resp, "refresh_token");
  long expiresIn = spotifyJsonLong(resp, "expires_in");
 
  if (spotifyAccessToken.length() == 0 && newRefresh.length() == 0) {
    Serial.println("Spotify: no tokens in exchange response: " + resp);
    return false;
  }
 
  // The refresh token is only issued once per authorization; keep the old
  // one if Spotify didn't echo a new one back.
  if (newRefresh.length() > 0) {
    spotifyRefreshToken = newRefresh;
    spotifySaveRefreshToken(newRefresh);
    Serial.println("Spotify: refresh token saved");
  }
  spotifyTokenExpiresAtMs = expiresIn > 0
    ? millis() + expiresIn * 1000 - SPOTIFY_TOKEN_SKEW_MS
    : 0;
  spotifyNow.authed = true;
  Serial.println("Spotify: connected");
  return true;
}
 
bool spotifyRefreshAccessToken() {
  if (spotifyRefreshToken.length() == 0) return false;
 
  String body = "grant_type=refresh_token";
  body += "&refresh_token=" + spotifyUrlEncode(spotifyRefreshToken);
  body += "&client_id=" + String(SPOTIFY_CLIENT_ID);
  body += "&client_secret=" + String(SPOTIFY_CLIENT_SECRET);
 
  String resp;
  int status;
  if (!spotifyHttpsPost(SPOTIFY_AUTH_HOST, SPOTIFY_TOKEN_PATH, body, resp, status)) {
    Serial.println("Spotify: token refresh request failed");
    return false;
  }
  if (status != 200) {
    Serial.println("Spotify: token refresh failed HTTP " + String(status) + ": " + resp);
    return false;
  }
 
  spotifyAccessToken = spotifyJsonString(resp, "access_token");
  long expiresIn = spotifyJsonLong(resp, "expires_in");
  String newRefresh = spotifyJsonString(resp, "refresh_token");
  if (newRefresh.length() > 0 && newRefresh != spotifyRefreshToken) {
    spotifyRefreshToken = newRefresh;
    spotifySaveRefreshToken(newRefresh);
  }
  spotifyTokenExpiresAtMs = expiresIn > 0
    ? millis() + expiresIn * 1000 - SPOTIFY_TOKEN_SKEW_MS
    : 0;
  return spotifyAccessToken.length() > 0;
}
 
// Returns true while a usable (not-yet-expired) access token is cached,
// refreshing it via the stored refresh token when necessary.
bool spotifyEnsureAccessToken() {
  if (!spotifyNow.authed) return false;
  if (spotifyAccessToken.length() > 0 && millis() < spotifyTokenExpiresAtMs) return true;
  return spotifyRefreshAccessToken();
}
 
bool spotifyIsAuthed() {
  return spotifyNow.authed;
}
 
void spotifyClearAuth() {
  spotifyAccessToken = "";
  spotifyRefreshToken = "";
  spotifyTokenExpiresAtMs = 0;
  spotifyNow.authed = false;
  spotifyNow.valid = false;
  spotifyPrefs.begin("spotify", false);
  spotifyPrefs.remove("refresh");
  spotifyPrefs.end();
  Serial.println("Spotify: signed out");
}
 
// Smooth runtime progress: snapshot progress_ms at poll time, then add the
// elapsed time since. Pauses when the track is not playing.
unsigned long spotifyEstimatedProgressMs() {
  if (!spotifyNow.valid) return 0;
  unsigned long p = spotifyNow.progressMs;
  if (spotifyNow.isPlaying) p += millis() - spotifyNow.pollAtMs;
  return p;
}
 
void spotifyPollNowPlaying() {
  if (!spotifyEnsureAccessToken()) {
    spotifyNow.valid = false;
    return;
  }
 
  String resp;
  int status;
  if (!spotifyHttpsGet(SPOTIFY_API_HOST, SPOTIFY_CURRENT_PATH, spotifyAccessToken, resp, status)) {
    return; // transient network error; try again next poll
  }
 
  // 401 = access token revoked/rotated behind our back - force a refresh
  // (which also bumps authed=false if the saved refresh token is dead) and
  // retry the request once.
  if (status == 401) {
    if (!spotifyRefreshAccessToken()) {
      spotifyNow.authed = false;
      spotifyNow.valid = false;
      return;
    }
    if (!spotifyHttpsGet(SPOTIFY_API_HOST, SPOTIFY_CURRENT_PATH, spotifyAccessToken, resp, status)) {
      return;
    }
  }
 
  if (status == 204 || resp.length() == 0) {
    spotifyNow.valid = false; // nothing playing
    return;
  }
  if (status != 200) {
    Serial.println("Spotify: player fetch failed HTTP " + String(status));
    return;
  }
 
  spotifyNow.progressMs = (unsigned long)spotifyJsonLong(resp, "progress_ms");
  spotifyNow.isPlaying = spotifyJsonBool(resp, "is_playing");
 
  String item = spotifyJsonObject(resp, "item");
  if (item.length() > 0) {
    // The track "name" is the last one in the item object (album/artists
    // carry earlier "name" keys), so grab the last match.
    spotifyNow.title = spotifyJsonString(item, "name", true);
    spotifyNow.durationMs = (unsigned long)spotifyJsonLong(item, "duration_ms");
    String artist = spotifyJsonArrayFirstObject(item, "artists");
    if (artist.length() > 0) {
      spotifyNow.artist = spotifyJsonString(artist, "name", true);
    }
  }
 
  spotifyNow.valid = true;
  spotifyNow.pollAtMs = millis();
}
