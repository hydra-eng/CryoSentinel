/**
 * @file gps.h
 * @brief CargoPulse v2.0 — GPS Interface Header (u-blox MAX-M10S)
 *
 * UART-based GPS via TinyGPS++ library.
 * Library: https://github.com/mikalhart/TinyGPSPlus
 *
 * Based on: polesskiy-dev/iot-risk-data-logger-nfc-samd21 (MIT)
 * CargoPulse v2.0 modifications: hydra-eng — 2026
 */

#pragma once
#include <Arduino.h>

// =============================================================================
// GPS DATA STRUCTURE
// =============================================================================
typedef struct {
    double   lat;           ///< Latitude in decimal degrees (positive = North)
    double   lon;           ///< Longitude in decimal degrees (positive = East)
    double   alt_m;         ///< Altitude in meters above sea level
    float    hdop;          ///< Horizontal dilution of precision
    uint8_t  satellites;    ///< Number of satellites tracked
    uint8_t  fix_quality;   ///< 0=no fix, 1=GPS, 2=DGPS
    uint32_t utc_time;      ///< UTC time as HHMMSS integer
} GPSData_t;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize GPS UART and power up MAX-M10S
 * @return true if UART begins successfully
 * @note GPS requires up to 30s cold start for first fix
 */
bool initGPS();

/**
 * @brief Attempt to get current GPS location
 * @return GPSData_t with current fix — check fix_quality > 0 for valid data
 * @note Blocks up to GPS_TIMEOUT_MS waiting for valid NMEA sentences
 */
GPSData_t getLocation();

/**
 * @brief Power down GPS module to save battery
 * @note Uses u-blox UBX-RXM-PMREQ command over UART
 * @note Backup battery on GPS module retains almanac and RTC
 */
void powerDownGPS();

/**
 * @brief Wake GPS module from backup power mode
 */
void wakeGPS();

/**
 * @brief Feed NMEA data from UART to TinyGPS++ parser
 * @note Call regularly in loop for continuous tracking
 */
void feedGPS();

/**
 * @brief Get last known location without waiting for new fix
 * @return Last valid GPSData_t (may be stale)
 */
GPSData_t getLastKnownLocation();
