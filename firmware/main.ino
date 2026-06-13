/**
 * @file main.ino
 * @brief CargoPulse v2.0 — Main Entry Point & State Machine
 * 
 * Implements a 5-state machine for cold chain monitoring:
 *   IDLE → SAMPLING → BREACH_ALERT → NFC_SERVE → SLEEP
 * 
 * Target hardware: ESP32-C3-MINI-1
 * 
 * Required libraries (install via Arduino Library Manager):
 *   - RadioLib (https://github.com/jgromes/RadioLib)        — SX1262 LoRa
 *   - TinyGPS++ (https://github.com/mikalhart/TinyGPSPlus)  — GPS parsing
 *   - GxEPD2 (https://github.com/ZinggJM/GxEPD2)           — e-ink display
 *   - SparkFun ATECC608A Arduino Library                     — Hardware crypto
 *   - Wire (built-in)                                        — I2C
 *   - SPI (built-in)                                         — SPI bus
 * 
 * Based on: polesskiy-dev/iot-risk-data-logger-nfc-samd21 (MIT)
 * CargoPulse v2.0 modifications: hydra-eng — 2026
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include "config.h"
#include "sensors.h"
#include "logger.h"
#include "alert.h"
#include "gps.h"
#include "display.h"
#include "crypto.h"

// =============================================================================
// GLOBAL STATE
// =============================================================================
static DeviceState_t currentState = STATE_IDLE;
static SensorData_t  sensorData   = {0};
static GPSData_t     gpsData      = {0};
static uint32_t      lastSampleMs = 0;
static bool          tamperArmed  = false;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
void handleIdle();
void handleSampling();
void handleBreachAlert(BreachType_t breach);
void handleNFCServe();
void handleSleep();
void checkTamper();
void setRGB(bool r, bool g, bool b);

// =============================================================================
// SETUP — Initialize all peripherals
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500); // Allow serial to settle
    Serial.println("[CargoPulse v2.0] Booting...");
    Serial.println("[CargoPulse] Device ID: " DEVICE_ID " | FW: " FW_VERSION);

    // --- GPIO setup ---
    pinMode(PIN_RGB_R,   OUTPUT);
    pinMode(PIN_RGB_G,   OUTPUT);
    pinMode(PIN_RGB_B,   OUTPUT);
    pinMode(PIN_BUZZER,  OUTPUT);
    pinMode(PIN_BAT_LED, OUTPUT);
    pinMode(PIN_TAMPER,  INPUT_PULLUP); // Tamper: pulled HIGH, GND when lid closed

    setRGB(true, true, false); // Yellow = booting

    // --- I2C bus init ---
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000); // 400kHz fast mode

    // --- SPI bus init ---
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI);

    // --- Peripheral init ---
    if (!initSensors()) {
        Serial.println("[ERROR] Sensor init failed — check I2C wiring");
        setRGB(true, false, false); // Red = error
        // TODO: [HARDWARE-TEST] Replace with recoverable error in production
    }

    if (!initLogger()) {
        Serial.println("[ERROR] Flash logger init failed — check SPI/CS");
        // TODO: [HARDWARE-TEST] Handle flash init failure gracefully
    }

    if (!initGPS()) {
        Serial.println("[WARN] GPS init failed — location will be unavailable");
        // GPS failure is non-fatal; alerts still sent without coordinates
    }

    if (!initAlert()) {
        Serial.println("[ERROR] LoRa SX1262 init failed — no alerts will be sent");
        // TODO: [HARDWARE-TEST] Verify LoRa module connections
    }

    if (!initDisplay()) {
        Serial.println("[WARN] e-ink display init failed — display offline");
    }

    if (!initCrypto()) {
        Serial.println("[ERROR] ATECC608A init failed — payloads will be unsigned");
        // TODO: [HARDWARE-TEST] Verify ATECC608A provisioning
    }

    // Arm tamper detection
    tamperArmed = (digitalRead(PIN_TAMPER) == LOW); // Lid closed at boot = armed
    Serial.print("[TAMPER] Lid state at boot: ");
    Serial.println(tamperArmed ? "CLOSED (armed)" : "OPEN");

    setRGB(false, true, false); // Green = ready
    delay(1000);
    setRGB(false, false, false); // Off

    Serial.println("[CargoPulse] Init complete — entering state machine");
    currentState = STATE_IDLE;
}

// =============================================================================
// LOOP — Run state machine
// =============================================================================
void loop() {
    // Always check tamper on every loop iteration
    checkTamper();

    switch (currentState) {
        case STATE_IDLE:
            handleIdle();
            break;
        case STATE_SAMPLING:
            handleSampling();
            break;
        case STATE_NFC_SERVE:
            handleNFCServe();
            break;
        case STATE_SLEEP:
            handleSleep();
            break;
        default:
            currentState = STATE_IDLE;
            break;
    }
}

// =============================================================================
// STATE HANDLERS
// =============================================================================

/**
 * @brief IDLE state — wait for sample interval or NFC poll
 * Normally in deep sleep; wake via RTC timer every LOG_INTERVAL_SEC
 */
void handleIdle() {
    uint32_t now = millis();
    if (now - lastSampleMs >= (LOG_INTERVAL_SEC * 1000UL)) {
        Serial.println("[STATE] IDLE → SAMPLING");
        currentState = STATE_SAMPLING;
    }

    // TODO: [HARDWARE-TEST] Implement ESP32 deep sleep with RTC wakeup:
    // esp_sleep_enable_timer_wakeup(LOG_INTERVAL_SEC * 1000000ULL);
    // esp_deep_sleep_start();
    // Currently using millis() polling for simulation compatibility

    // Check for NFC field present (ST25DV interrupt — GPIO poll or interrupt)
    // TODO: [HARDWARE-TEST] Wire ST25DV GPO pin to ESP32 interrupt GPIO
    delay(100);
}

/**
 * @brief SAMPLING state — read all sensors, log, check thresholds
 */
void handleSampling() {
    Serial.println("[STATE] SAMPLING");
    lastSampleMs = millis();

    // Read environmental sensors
    sensorData = readSHT40();
    Serial.printf("[SENSOR] Temp: %.2f°C | Humidity: %.1f%%\n",
                  sensorData.temp_c, sensorData.humidity_pct);

    // Read accelerometer
    ShockData_t shock = readADXL345();
    sensorData.peak_g = shock.peak_g;
    Serial.printf("[SENSOR] Shock: %.2fg peak\n", shock.peak_g);

    // Read battery state
    BatteryData_t bat = readBattery();
    sensorData.voltage_mv = bat.voltage_mv;
    sensorData.soc_pct    = bat.soc_pct;
    Serial.printf("[BATTERY] %.0f mV | %d%% SOC\n", bat.voltage_mv, bat.soc_pct);

    // Check battery critical
    if (bat.soc_pct <= BATTERY_CRIT_PCT) {
        Serial.println("[POWER] Battery critical — entering SLEEP");
        currentState = STATE_SLEEP;
        return;
    }

    // Update battery LED
    digitalWrite(PIN_BAT_LED, bat.soc_pct > BATTERY_LOW_PCT ? HIGH : LOW);

    // Get GPS location
    gpsData = getLocation();
    if (gpsData.fix_quality > 0) {
        Serial.printf("[GPS] Fix: %.6f, %.6f (quality=%d)\n",
                      gpsData.lat, gpsData.lon, gpsData.fix_quality);
    } else {
        Serial.println("[GPS] No fix — using last known location");
    }

    // Write log entry to NOR flash
    uint32_t timestamp = millis() / 1000; // TODO: Replace with RTC timestamp
    uint8_t  flags     = (gpsData.fix_quality > 0) ? 0x01 : 0x00;
    writeLogEntry(timestamp, sensorData.temp_c, sensorData.humidity_pct,
                  shock.peak_g, gpsData.lat, gpsData.lon, flags);

    // Update display
    StatusType_t displayStatus = STATUS_OK;

    // Check thresholds
    BreachType_t breach = checkThresholds(sensorData);
    if (breach != BREACH_NONE) {
        displayStatus = STATUS_BREACH;
        Serial.printf("[BREACH] Type=%d — transitioning to BREACH_ALERT\n", breach);
        handleBreachAlert(breach); // Handle inline before returning
        currentState = STATE_IDLE; // Return to IDLE after alert
    } else {
        currentState = STATE_IDLE;
    }

    updateDisplay(sensorData.temp_c, sensorData.humidity_pct,
                  bat.soc_pct, displayStatus);
}

/**
 * @brief BREACH_ALERT state — sign payload, transmit LoRa, alert driver
 * @param breach The type of breach detected
 */
void handleBreachAlert(BreachType_t breach) {
    Serial.println("[STATE] BREACH_ALERT");

    // Visual + audio alert for driver
    setRGB(true, false, false); // RED
    digitalWrite(PIN_BUZZER, HIGH);

    // Build JSON payload
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"device_id\":\"%s\","
        "\"breach_type\":%d,"
        "\"temp_c\":%.2f,"
        "\"humidity\":%.1f,"
        "\"shock_g\":%.2f,"
        "\"lat\":%.6f,"
        "\"lon\":%.6f,"
        "\"timestamp\":%lu,"
        "\"fw\":\"%s\"}",
        DEVICE_ID, (int)breach,
        sensorData.temp_c, sensorData.humidity_pct, sensorData.peak_g,
        gpsData.lat, gpsData.lon,
        millis() / 1000,
        FW_VERSION
    );

    // Sign payload with ATECC608A
    uint8_t signature[64] = {0};
    bool signed_ok = signPayload((const uint8_t*)payload, strlen(payload), signature);
    if (signed_ok) {
        Serial.print("[CRYPTO] Payload signed: 0x");
        for (int i = 0; i < 4; i++) Serial.printf("%02X", signature[i]);
        Serial.println("...");
    } else {
        Serial.println("[CRYPTO] Signing failed — sending unsigned alert");
    }

    // Transmit LoRa alert
    bool lora_sent = sendLoRaAlert(breach, sensorData.temp_c,
                                   gpsData.lat, gpsData.lon);
    Serial.println(lora_sent ? "[LORA] Alert sent successfully" :
                               "[LORA] Transmission failed");

    // Buzzer pattern: 3 short beeps
    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_BUZZER, HIGH);
        delay(200);
        digitalWrite(PIN_BUZZER, LOW);
        delay(100);
    }

    delay(2000); // Hold red for 2s
    setRGB(false, false, false);
}

/**
 * @brief NFC_SERVE state — respond to NFC reader with log data
 */
void handleNFCServe() {
    Serial.println("[STATE] NFC_SERVE — waiting for NFC read");
    setRGB(false, false, true); // Blue = NFC active

    // TODO: [HARDWARE-TEST] Implement ST25DV NDEF message write with last N entries
    // Wire.beginTransmission(ADDR_ST25DV);
    // ... write NDEF records ...
    delay(5000); // Hold NFC state for 5s

    setRGB(false, false, false);
    currentState = STATE_IDLE;
}

/**
 * @brief SLEEP state — extreme power save when battery critical
 */
void handleSleep() {
    Serial.println("[STATE] SLEEP — battery critical, hibernating");
    setRGB(false, false, false);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_BAT_LED, LOW);

    // Final display update showing battery empty
    updateDisplay(sensorData.temp_c, sensorData.humidity_pct, 0, STATUS_SLEEP);

    // TODO: [HARDWARE-TEST] Enter ESP32-C3 deep sleep indefinitely
    // esp_deep_sleep_start();

    // Simulation fallback:
    while (true) {
        delay(60000);
    }
}

// =============================================================================
// TAMPER DETECTION
// =============================================================================
/**
 * @brief Check tamper circuit on every loop.
 * Logic: Lid CLOSED = GPIO0 LOW (trace connects to GND)
 *        Lid OPEN   = GPIO0 HIGH (trace breaks, pulled up to 3.3V)
 */
void checkTamper() {
    if (!tamperArmed) return;

    if (digitalRead(PIN_TAMPER) == TAMPER_OPEN) {
        Serial.println("[TAMPER] ⚠ LID OPENED — logging tamper event");
        tamperArmed = false; // Prevent repeat alerts

        // Log tamper as a breach
        uint32_t ts = millis() / 1000;
        writeLogEntry(ts, sensorData.temp_c, sensorData.humidity_pct,
                      0.0f, gpsData.lat, gpsData.lon, 0x80); // 0x80 = tamper flag

        // Alert via LoRa
        handleBreachAlert(BREACH_TAMPER);
        currentState = STATE_IDLE;
    }
}

// =============================================================================
// RGB LED HELPER
// =============================================================================
void setRGB(bool r, bool g, bool b) {
    digitalWrite(PIN_RGB_R, r ? HIGH : LOW);
    digitalWrite(PIN_RGB_G, g ? HIGH : LOW);
    digitalWrite(PIN_RGB_B, b ? HIGH : LOW);
}
