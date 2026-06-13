/**
 * @file alert.h
 * @brief Cryo Sentinel — Breach Detection & LoRa Alert Header
 *
 * Threshold checking and LoRa transmission via SX1262 module.
 * Uses RadioLib for SX1262 driver (MIT License).
 * Library: https://github.com/jgromes/RadioLib
 */

#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensors.h"

// =============================================================================
// LORA PAYLOAD STRUCTURE
// =============================================================================
/** JSON LoRa alert payload (max 255 bytes for SF9 at 865MHz) */
typedef struct {
    char device_id[16];    ///< Device identifier
    uint8_t breach_type;   ///< BreachType_t value
    float temp_c;          ///< Temperature at breach
    float humidity_pct;    ///< Humidity at breach
    float lat;             ///< GPS latitude
    float lon;             ///< GPS longitude
    uint32_t timestamp;    ///< Unix timestamp
    uint8_t signature[8];  ///< First 8 bytes of ATECC608A signature (truncated for airtime)
} LoRaPayload_t;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize SX1262 LoRa module
 * @return true if module responds and is configured correctly
 * @note Configures 865.0 MHz, SF9, BW125, CR4/7, 14dBm TX power
 */
bool initAlert();

/**
 * @brief Check sensor readings against all configured thresholds
 * @param data  Current sensor reading
 * @return BREACH_NONE if all in range, otherwise the first breach type detected
 */
BreachType_t checkThresholds(const SensorData_t &data);

/**
 * @brief Transmit LoRa breach alert
 * @param breach_type  Type of breach (BreachType_t)
 * @param temp_c       Current temperature
 * @param lat          GPS latitude
 * @param lon          GPS longitude
 * @return true if transmission acknowledged or sent (no ACK in LoRaWAN raw mode)
 *
 * Payload format (JSON, max 200 bytes):
 * {"id":"CP-001","bt":1,"t":8.2,"h":65.0,"lat":28.6139,"lon":77.2090,"ts":1234567,"s":"A3F2..."}
 */
bool sendLoRaAlert(BreachType_t breach_type, float temp_c, float lat, float lon);

/**
 * @brief Get last LoRa RSSI value (for diagnostic purposes)
 * @return RSSI in dBm, or 0 if not available
 */
int16_t getLastRSSI();

/**
 * @brief Check if LoRa module is currently busy (transmitting)
 * @return true if BUSY pin is HIGH
 */
bool isLoRaBusy();
