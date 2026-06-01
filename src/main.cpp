#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const String BOT_TOKEN = "YOUR_BOT_TOKEN";
const String CHAT_ID = "YOUR_CHAT_ID";
const char* TELEGRAM_ROOT_CA = "";

#define PIR_PIN 13
#define REED_PIN 14
#define FLAME_PIN 12
#define BUZZER_PIN 15
#define GAS_PIN 4
#define SOUND_PIN 35
#define SOUND_THRESHOLD 2300

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const unsigned long GAS_COOLDOWN = 60000UL;   
const unsigned long SOUND_COOLDOWN = 30000UL; 

unsigned long lastGasMillis = 0;
unsigned long lastSoundMillis = 0;

const unsigned long MOTION_COOLDOWN = 15000UL;
const unsigned long DOOR_COOLDOWN = 10000UL;
const unsigned long FLAME_COOLDOWN = 30000UL;

const unsigned long OLED_INTERVAL = 2000UL;
const unsigned long WIFI_BACKOFF_INITIAL = 2000UL;
const unsigned long WIFI_BACKOFF_MAX = 60000UL;
const unsigned long REED_DEBOUNCE_MS = 50UL;
const int TELEGRAM_RETRIES = 2;
const unsigned long TELEGRAM_TIMEOUT_MS = 6000;
const unsigned long TELEGRAM_RETRY_BACKOFF = 1000;

unsigned long lastMotionMillis = 0;
unsigned long lastDoorEventMillis = 0;
unsigned long lastFlameMillis = 0;

bool flameActive = false;
int prevDoorState = -1;

unsigned long lastOledUpdate = 0;
String oledLine1 = "System Boot";
String oledLine2 = "Starting...";

unsigned long wifiBackoff = WIFI_BACKOFF_INITIAL;
unsigned long lastWifiAttempt = 0;

enum BuzzMode { BUZZ_IDLE, BUZZ_PATTERN };
BuzzMode buzzMode = BUZZ_IDLE;
int buzzTotalPulses = 0;
int buzzPulseIndex = 0;
unsigned long buzzPulseStart = 0;
unsigned long buzzOnMs = 0;
unsigned long buzzOffMs = 0;
bool buzzStateHigh = false;

int reedLastStable = -1;
unsigned long reedLastChange = 0;

String jsonEscape(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') { out += "\\n"; }
    else out += c;
  }
  return out;
}

void startBuzzerPattern(int pulses, unsigned long onMs, unsigned long offMs) {
  buzzTotalPulses = pulses;
  buzzPulseIndex = 0;
  buzzOnMs = onMs;
  buzzOffMs = offMs;
  buzzPulseStart = millis();
  buzzMode = BUZZ_PATTERN;
  buzzStateHigh = true;
  digitalWrite(BUZZER_PIN, HIGH);
}

void handleBuzzer() {
  if (buzzMode == BUZZ_IDLE) return;
  unsigned long now = millis();

  if (buzzStateHigh) {
    if (now - buzzPulseStart >= buzzOnMs) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzStateHigh = false;
      buzzPulseStart = now;
    }
  } else {
    if (now - buzzPulseStart >= buzzOffMs) {
      buzzPulseIndex++;
      if (buzzPulseIndex >= buzzTotalPulses) {
        buzzMode = BUZZ_IDLE;
        digitalWrite(BUZZER_PIN, LOW);
      } else {
        digitalWrite(BUZZER_PIN, HIGH);
        buzzStateHigh = true;
        buzzPulseStart = now;
      }
    }
  }
}

int readDebouncedReed() {
  int v = digitalRead(REED_PIN);
  unsigned long now = millis();

  if (reedLastStable == -1) {
    reedLastStable = v;
    reedLastChange = now;
    return v;
  }

  if (v != reedLastStable) {
    if (now - reedLastChange >= REED_DEBOUNCE_MS) {
      reedLastStable = v;
      reedLastChange = now;
    }
  } else reedLastChange = now;

  return reedLastStable;
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiBackoff = WIFI_BACKOFF_INITIAL;
    return;
  }

  unsigned long now = millis();
  if (now - lastWifiAttempt < wifiBackoff) return;
  lastWifiAttempt = now;

  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (millis() - start < 3000) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiBackoff = WIFI_BACKOFF_INITIAL;
      return;
    }
    delay(100);
  }

  wifiBackoff = min(wifiBackoff * 2, WIFI_BACKOFF_MAX) + random(0, 500);
}

bool sendTelegram(const String &text) {
  if (WiFi.status() != WL_CONNECTED) return false;

  const char* host = "api.telegram.org";
  const int httpsPort = 443;

  for (int attempt = 0; attempt <= TELEGRAM_RETRIES; ++attempt) {
    WiFiClientSecure client;

    if (strlen(TELEGRAM_ROOT_CA) > 10) client.setCACert(TELEGRAM_ROOT_CA);
    else client.setInsecure();

    if (client.connect(host, httpsPort)) {
      String url = "/bot" + BOT_TOKEN + "/sendMessage";
      String payload = "{\"chat_id\":\"" + CHAT_ID + "\",\"text\":\"" + jsonEscape(text) + "\"}";

      client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                   "Host: api.telegram.org\r\n" +
                   "User-Agent: ESP32\r\n" +
                   "Content-Type: application/json\r\n" +
                   "Content-Length: " + String(payload.length()) + "\r\n" +
                   "Connection: close\r\n\r\n" + payload);

      String statusLine = client.readStringUntil('\n');

      if (statusLine.length() > 0) {
        int sp = statusLine.indexOf(' ');
        int sp2 = statusLine.indexOf(' ', sp + 1);
        int code = statusLine.substring(sp + 1, sp2).toInt();
        client.stop();
        if (code >= 200 && code < 300) return true;
      }

      client.stop();
    }

    delay(TELEGRAM_RETRY_BACKOFF * (attempt + 1));
  }

  return false;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(FLAME_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(oledLine1);
  display.setTextSize(2);
  display.setCursor(0,20);
  display.println(oledLine2);
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long t0 = millis();
  while (millis() - t0 < 8000 && WiFi.status() != WL_CONNECTED) delay(200);

  startBuzzerPattern(1,120,0);

  if (WiFi.status() == WL_CONNECTED) sendTelegram("System Online");

  prevDoorState = readDebouncedReed();
  lastOledUpdate = millis();
}

void loop() {

int gasValue = digitalRead(GAS_PIN);
if (gasValue == HIGH && (now - lastGasMillis >= GAS_COOLDOWN)) {
    lastGasMillis = now;
    oledLine1 = "CRITICAL ALERT";
    oledLine2 = "GAS LEAK!";
    startBuzzerPattern(3, 500, 500); 
    sendTelegram(" WARNING: Gas Leak Detected!");
}

int soundValue = analogRead(SOUND_PIN);
if (soundValue > SOUND_THRESHOLD && (now - lastSoundMillis >= SOUND_COOLDOWN)) {
    lastSoundMillis = now;
    oledLine1 = "SECURITY ALERT";
    oledLine2 = "LOUD NOISE!";
    startBuzzerPattern(1, 2000, 0); 
    sendTelegram("Alert: High Noise Level Detected!");
}
  unsigned long now = millis();

  ensureWiFi();
  handleBuzzer();

  int motion = digitalRead(PIR_PIN);
  int door = readDebouncedReed();
  int flame = digitalRead(FLAME_PIN);

  bool flameDetected = (flame == LOW);

  if (flameDetected && !flameActive) {
    flameActive = true;
    lastFlameMillis = now;

    oledLine1 = "CRITICAL ALERT";
    oledLine2 = "FIRE!";

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(oledLine1);
    display.setTextSize(2);
    display.setCursor(0,20);
    display.println(oledLine2);
    display.display();

    startBuzzerPattern(1, 8000, 0);
    sendTelegram("FIRE DETECTED");
  }

  if (!flameDetected && flameActive && now - lastFlameMillis >= 2000) {
    flameActive = false;
    sendTelegram("Fire Cleared");
    oledLine1 = "System Active";
    oledLine2 = "Monitoring";
  }

  if (door != prevDoorState) {
    if (now - lastDoorEventMillis >= DOOR_COOLDOWN) {
      lastDoorEventMillis = now;

      if (door == HIGH) {
        oledLine1 = "Security Alert";
        oledLine2 = "DOOR OPEN";
        startBuzzerPattern(1, 8000, 0);
        sendTelegram("Door Opened");
      } else {
        oledLine1 = "Status Update";
        oledLine2 = "DOOR CLOSED";
        sendTelegram("Door Closed");
      }
    }
    prevDoorState = door;
  }

  if (motion == HIGH && now - lastMotionMillis >= MOTION_COOLDOWN) {
    lastMotionMillis = now;
    oledLine1 = "Security Alert";
    oledLine2 = "MOTION";
    startBuzzerPattern(1, 8000, 0);
    sendTelegram("Motion Detected");
  }

  if (now - lastOledUpdate >= OLED_INTERVAL) {
    lastOledUpdate = now;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(oledLine1);
    display.setTextSize(2);
    display.setCursor(0,20);
    display.println(oledLine2);
    display.display();
  }

  delay(10);
}