# 🛡️ Home Security System with Telegram Alerts

An advanced, production-grade IoT home security solution powered by the **ESP32** microcontroller. This system continuously monitors environmental variables to safeguard spaces against unauthorized motion, entry point breaches, and fire outbreaks. It features a dual-action alert mechanism: instantaneous localized audio-visual feedback (OLED + Buzzer) and global, secure push notifications pushed directly to your smartphone via **Telegram**.

---

## 🚀 Features

* **Instant Cloud Telemetry:** Sends low-latency, secure HTTPS alerts to your private Telegram channel or bot chat.
* **Triple Hazard Multi-Tasking:** Simultaneous monitoring for Fire (Flame sensor), Intruders (PIR motion sensor), and Breached Openings (Magnetic Reed switch).
* **Non-Blocking Architecture:** Built entirely without rigid `delay()` functions, utilizing asynchronous `millis()` structures to ensure the CPU never misses a sensor pulse.
* **Intelligent Network Recovery:** Employs an exponential backoff algorithm to smoothly attempt Wi-Fi reconnection during router outages without freezing the system loop.
* **Signal Debouncing:** Advanced software filtering on the door sensor to eradicate false alarms triggered by mechanical vibrations or electrical noise.
* **Rate-Limited Flood Protection:** Structured cooldown windows prevent the hardware from spamming your phone with repetitive notifications for a single continuous event.

---

## 🛠️ Hardware Requirements

| Component | Purpose | Connection (ESP32 GPIO) |
| :--- | :--- | :--- |
| **ESP32 Dev Kit** | Central Controller & Wi-Fi Node | Main SoC |
| **PIR Motion Sensor** | Registers spatial movement | `GPIO 13` |
| **Magnetic Reed Switch** | Monitors Door Open/Closed status | `GPIO 14` (Internal Pull-up) |
| **Flame Sensor** | Detects IR/Light wavelengths of fire | `GPIO 12` |
| **Active Buzzer** | High-decibel local audio alarm | `GPIO 15` |
| **SSD1306 OLED (128x64)**| On-site status visualizer | `I2C Pins` (`SDA/SCL`) |

---

## 💻 Software & Environment

This project is engineered using **PlatformIO** inside **VS Code** for robust dependency tracking and compilation. 

### Required Libraries:
* `Adafruit GFX Library` (Core graphics layer)
* `Adafruit SSD1306` (Hardware-specific driver for the OLED display)
* `WiFi` & `WiFiClientSecure` (Built into the ESP32 Arduino Core)

---

## ⚙️ Setup & Installation

### 1. Telegram Bot Configuration
1. Open Telegram and search for `@BotFather`.
2. Send `/newbot` and follow the prompts to create a bot. Copy your **HTTP API Bot Token**.
3. Search for `@userinfobot` on Telegram and message it to find your unique **Chat ID**.

### 2. Project Setup (PlatformIO)
1. Clone this repository or copy the code to your local machine.
2. Open the project folder in **VS Code** (ensure the PlatformIO extension is installed).
3. Open your project's configuration file `platformio.ini` and append the required libraries under your environment:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306

// --- USER CONFIGURATION ---
const char* WIFI_SSID = "YOUR_WIFI_NAME";         // Your Wi-Fi Router SSID
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";     // Your Wi-Fi Router Password
const String BOT_TOKEN = "YOUR_BOT_TOKEN";        // Insert your Telegram Bot Token
const String CHAT_ID = "YOUR_CHAT_ID";            // Insert your Telegram Chat ID
