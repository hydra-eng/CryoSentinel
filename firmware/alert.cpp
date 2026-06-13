/**
 * @file alert.cpp
 * @brief CargoPulse v2.0 — Breach Detection & LoRa Alert Implementation
 *
 * SX1262 driver via RadioLib (https://github.com/jgromes/RadioLib)
 * Configure India 865 MHz ISM band, SF9, BW125kHz, 14dBm.
 *
 * Based on: polesskiy-dev/iot-risk-data-logger-nfc-samd21 (MIT)
 * CargoPulse v2.0 modifications: hydra-eng — 2026
 */

#include "alert.h"
#include "config.h"
#include <SPI.h>

// RadioLib SX1262 driver
// Install: Arduino Library Manager → search "RadioLib" by Jan Gromes
#include <RadioLib.h>

// =============================================================================
// RADIOLIB MODULE INSTANTIATION
// SX1262(CS, DIO1, RESET, BUSY, SPI)
// =============================================================================
SX1262 lora = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

static bool  lora_ok      = false;
static int16_t last_rssi  = 0;

// =============================================================================
// INIT
// =============================================================================
bool initAlert() {
    Serial.print("[LORA] Initializing SX1262 at ");
    Serial.print(LORA_FREQUENCY);
    Serial.println(" MHz (India 865 band)...");

    // TODO: [HARDWARE-TEST] Verify SPI bus is already initialized (SPI.begin called in setup)
    int state = lora.begin(
        LORA_FREQUENCY,     // Carrier frequency (MHz)
        LORA_BANDWIDTH,     // Bandwidth (kHz)
        LORA_SPREADING,     // Spreading factor (SF9)
        LORA_CODING_RATE,   // Coding rate (4/7)
        LORA_SYNC_WORD,     // Sync word (private network)
        LORA_TX_POWER,      // TX power (dBm)
        8,                  // Preamble length (symbols)
        1.6f                // TCXO voltage (SX1262 module typically uses 1.6V TCXO)
    );

    if (state == RADIOLIB_ERR_NONE) {
        lora_ok = true;
        // Configure DIO2 as RF switch (required for most SX1262 modules)
        lora.setDio2AsRfSwitch(true);
        Serial.println("[LORA] SX1262 ready");
        return true;
    } else {
        Serial.printf("[LORA] SX1262 init failed — RadioLib error %d\n", state);
        Serial.println("[LORA] Check: CS=GPIO7, DIO1=GPIO10, RST=GPIO1, BUSY=GPIO3");
        // TODO: [HARDWARE-TEST] Verify 50-ohm antenna connected to U.FL
        return false;
    }
}

// =============================================================================
// THRESHOLD CHECKING
// =============================================================================
BreachType_t checkThresholds(const SensorData_t &data) {
    // Check temperature (cold chain pharmaceutical: 2°C to 8°C)
    if (data.temp_c > TEMP_MAX_C) {
        Serial.printf("[BREACH] Temperature HIGH: %.2f°C > %.1f°C\n",
                      data.temp_c, TEMP_MAX_C);
        return BREACH_TEMP_HIGH;
    }
    if (data.temp_c < TEMP_MIN_C) {
        Serial.printf("[BREACH] Temperature LOW: %.2f°C < %.1f°C\n",
                      data.temp_c, TEMP_MIN_C);
        return BREACH_TEMP_LOW;
    }

    // Check humidity
    if (data.humidity_pct > HUMIDITY_MAX_PCT || data.humidity_pct < HUMIDITY_MIN_PCT) {
        Serial.printf("[BREACH] Humidity out of range: %.1f%%\n", data.humidity_pct);
        return BREACH_HUMIDITY;
    }

    // Check shock
    if (data.peak_g > SHOCK_THRESHOLD_G) {
        Serial.printf("[BREACH] Shock threshold exceeded: %.2fg > %.1fg\n",
                      data.peak_g, SHOCK_THRESHOLD_G);
        return BREACH_SHOCK;
    }

    return BREACH_NONE;
}

// =============================================================================
// LORA ALERT TRANSMISSION
// =============================================================================
bool sendLoRaAlert(BreachType_t breach_type, float temp_c, float lat, float lon) {
    if (!lora_ok) {
        Serial.println("[LORA] Module not initialized — alert not sent");
        return false;
    }

    if (isLoRaBusy()) {
        Serial.println("[LORA] Module busy — waiting...");
        delay(500);
    }

    // Build compact JSON payload (keep under 200 bytes for SF9 airtime)
    char payload[256];
    const char* breach_str;
    switch (breach_type) {
        case BREACH_TEMP_HIGH: breach_str = "TEMP_HIGH"; break;
        case BREACH_TEMP_LOW:  breach_str = "TEMP_LOW";  break;
        case BREACH_HUMIDITY:  breach_str = "HUMIDITY";  break;
        case BREACH_SHOCK:     breach_str = "SHOCK";     break;
        case BREACH_TAMPER:    breach_str = "TAMPER";    break;
        default:               breach_str = "UNKNOWN";   break;
    }

    int len = snprintf(payload, sizeof(payload),
        "{\"id\":\"%s\","
        "\"bt\":\"%s\","
        "\"t\":%.2f,"
        "\"lat\":%.5f,"
        "\"lon\":%.5f,"
        "\"ts\":%lu}",
        DEVICE_ID,
        breach_str,
        temp_c,
        lat, lon,
        millis() / 1000UL
    );

    Serial.printf("[LORA] Transmitting %d bytes: %s\n", len, payload);

    // Transmit packet (blocking until TX complete or timeout)
    int state = lora.transmit((uint8_t*)payload, len);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] Packet sent successfully");
        return true;
    } else {
        Serial.printf("[LORA] Transmit failed — error %d\n", state);
        // TODO: [HARDWARE-TEST] Check antenna connection and TX power setting
        return false;
    }
}

int16_t getLastRSSI() {
    if (!lora_ok) return 0;
    return (int16_t)lora.getRSSI();
}

bool isLoRaBusy() {
    return (digitalRead(PIN_LORA_BUSY) == HIGH);
}
