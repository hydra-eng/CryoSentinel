/**
 * @file nfc.h
 * @brief Cryo Sentinel — ST25DV Dynamic NFC Tag Driver Header
 *
 * Provides interface to write dynamically updated NDEF records (e.g. current
 * status, GPS coordinates, and temperature readings) into the ST25DV
 * tag EEPROM over I2C. This allows smartphones to scan the device and retrieve
 * logs even if the main battery is dead, utilizing RF energy harvesting.
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Initialize ST25DV dynamic tag over I2C
 * @return true if device responds at address ADDR_ST25DV
 */
bool initNFC();

/**
 * @brief Format and write status details as NDEF Text Record to NFC tag
 * @param temp        Current temperature
 * @param hum         Current humidity percentage
 * @param lat         GPS latitude
 * @param lon         GPS longitude
 * @param status      Status text string (e.g. "OK", "BREACH", "WARNING")
 * @return true if write operation succeeds
 */
bool updateNFCTag(float temp, float hum, float lat, float lon, const char* status);
