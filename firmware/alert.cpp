/**
 * @file alert.cpp
 * @brief Cryo Sentinel — Breach Detection & LoRa Alert Implementation
 *
 * SX1262 driver via RadioLib (https://github.com/jgromes/RadioLib)
 * Configure India 865 MHz ISM band, SF9, BW125kHz, 14dBm.
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

    // Build Cayenne LPP (Low Power Payload) buffer for LoRaWAN compliance
    uint8_t payload[64];
    int len = 0;

    // 1. Temperature (Channel 1, Type 103 (0x67), 0.1°C signed)
    int16_t t_val = (int16_t)(temp_c * 10);
    payload[len++] = 1;
    payload[len++] = 0x67;
    payload[len++] = (t_val >> 8) & 0xFF;
    payload[len++] = t_val & 0xFF;

    // 2. GPS Location (Channel 2, Type 136 (0x88), 0.0001 signed)
    int32_t lat_val = (int32_t)(lat * 10000);
    int32_t lon_val = (int32_t)(lon * 10000);
    int32_t alt_val = 0;
    payload[len++] = 2;
    payload[len++] = 0x88;
    payload[len++] = (lat_val >> 16) & 0xFF;
    payload[len++] = (lat_val >> 8) & 0xFF;
    payload[len++] = lat_val & 0xFF;
    payload[len++] = (lon_val >> 16) & 0xFF;
    payload[len++] = (lon_val >> 8) & 0xFF;
    payload[len++] = lon_val & 0xFF;
    payload[len++] = (alt_val >> 16) & 0xFF;
    payload[len++] = (alt_val >> 8) & 0xFF;
    payload[len++] = alt_val & 0xFF;

    // 3. Digital Input for Breach Type (Channel 3, Type 0)
    payload[len++] = 3;
    payload[len++] = 0x00;
    payload[len++] = (uint8_t)breach_type;

    Serial.printf("[LORA] Transmitting %d bytes LPP payload\n", len);

    // Transmit packet (blocking until TX complete or timeout)
    int state = lora.transmit(payload, len);

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
