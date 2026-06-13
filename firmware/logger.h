/**
 * @file logger.h
 * @brief CargoPulse v2.0 — SPI NOR Flash Circular Log Buffer Header
 *
 * Provides persistent logging to 4MB SPI NOR flash using a
 * circular buffer with wear leveling via sequential write pointer.
 *
 * Log entry format (32 bytes per entry):
 *   [4B timestamp][4B temp float][4B humidity float]
 *   [4B shock float][4B lat float][4B lon float]
 *   [4B flags+reserved]
 *
 * Based on: polesskiy-dev/iot-risk-data-logger-nfc-samd21 (MIT)
 * CargoPulse v2.0 modifications: hydra-eng — 2026
 */

#pragma once
#include <Arduino.h>

// =============================================================================
// LOG ENTRY STRUCTURE (32 bytes — aligned for SPI flash page writes)
// =============================================================================
typedef struct __attribute__((packed)) {
    uint32_t timestamp;     ///< Unix timestamp (seconds since boot or RTC epoch)
    float    temp_c;        ///< Temperature in degrees Celsius
    float    humidity_pct;  ///< Relative humidity percentage
    float    shock_g;       ///< Peak shock in g-force
    float    lat;           ///< GPS latitude (0.0 if no fix)
    float    lon;           ///< GPS longitude (0.0 if no fix)
    uint8_t  flags;         ///< Bit flags: b0=GPS_fix, b7=tamper, b6=breach
    uint8_t  breach_type;   ///< BreachType_t value
    uint8_t  soc_pct;       ///< Battery state of charge at time of log
    uint8_t  crc8;          ///< CRC8 of bytes 0-30 for integrity check
} LogEntry_t;

// =============================================================================
// FLAG DEFINITIONS
// =============================================================================
#define LOG_FLAG_GPS_FIX    0x01  ///< GPS fix was valid at time of log
#define LOG_FLAG_BREACH     0x02  ///< This entry was a breach event
#define LOG_FLAG_TAMPER     0x80  ///< This entry was a tamper event

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize SPI NOR flash and mount log partition
 * @return true if flash is accessible and log header is valid
 * @note Formats flash if header magic is invalid (first boot)
 */
bool initLogger();

/**
 * @brief Write a new log entry to the circular buffer
 * @param timestamp  Unix timestamp
 * @param temp_c     Temperature in °C
 * @param humidity   Relative humidity %
 * @param shock_g    Peak shock in g
 * @param lat        GPS latitude
 * @param lon        GPS longitude
 * @param flags      Log flags (see LOG_FLAG_* defines)
 * @return true if write was successful
 */
bool writeLogEntry(uint32_t timestamp, float temp_c, float humidity,
                   float shock_g, float lat, float lon, uint8_t flags);

/**
 * @brief Read a log entry by index
 * @param index   Entry index (0 = oldest, getLogCount()-1 = newest)
 * @param entry   Pointer to LogEntry_t to populate
 * @return true if entry exists and CRC is valid
 */
bool readLogEntry(uint32_t index, LogEntry_t *entry);

/**
 * @brief Get total number of valid log entries
 * @return Entry count (0 to LOG_MAX_ENTRIES)
 */
uint32_t getLogCount();

/**
 * @brief Erase all log entries (reset write pointer to 0)
 * @note This erases the entire log partition — use with caution
 */
void clearLog();

/**
 * @brief Get the flash usage percentage
 * @return Percentage 0-100
 */
uint8_t getFlashUsagePct();
