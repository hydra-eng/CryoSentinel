/**
 * @file sensors.h
 * @brief CargoPulse v2.0 — Sensor Interface Header
 *
 * Provides data structures and function declarations for:
 *   - SHT40 temperature/humidity sensor (I2C 0x44)
 *   - ADXL345 3-axis accelerometer (SPI, CS=GPIO5)
 *   - MAX17048 fuel gauge (I2C 0x36)
 *
 * Based on: polesskiy-dev/iot-risk-data-logger-nfc-samd21 (MIT)
 * CargoPulse v2.0 modifications: hydra-eng — 2026
 */

#pragma once
#include <Arduino.h>

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/** Environmental sensor reading from SHT40 */
typedef struct {
    float    temp_c;        ///< Temperature in degrees Celsius
    float    humidity_pct;  ///< Relative humidity in percent
    float    peak_g;        ///< Peak shock in g-force (populated by readADXL345)
    float    voltage_mv;    ///< Battery voltage in millivolts
    uint8_t  soc_pct;       ///< State of charge 0-100%
} SensorData_t;

/** 3-axis accelerometer reading */
typedef struct {
    int16_t  x_raw;    ///< Raw X-axis value from ADXL345
    int16_t  y_raw;    ///< Raw Y-axis value from ADXL345
    int16_t  z_raw;    ///< Raw Z-axis value from ADXL345
    float    x_g;      ///< X-axis in g-force
    float    y_g;      ///< Y-axis in g-force
    float    z_g;      ///< Z-axis in g-force
    float    peak_g;   ///< Peak resultant vector magnitude
} ShockData_t;

/** Battery state from MAX17048 */
typedef struct {
    float   voltage_mv;  ///< Battery voltage in mV
    uint8_t soc_pct;     ///< State of charge (0-100%)
    bool    is_charging; ///< True if TP4056 is actively charging
} BatteryData_t;

// =============================================================================
// STATUS TYPE FOR DISPLAY
// =============================================================================
typedef enum {
    STATUS_OK      = 0,
    STATUS_WARNING = 1,
    STATUS_BREACH  = 2,
    STATUS_SLEEP   = 3
} StatusType_t;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize all sensor peripherals (SHT40, ADXL345, MAX17048)
 * @return true if all sensors respond correctly, false if any fails
 */
bool initSensors();

/**
 * @brief Read temperature and humidity from SHT40
 * @return SensorData_t with temp_c and humidity_pct populated
 * @note Uses high-precision mode (repeatability=HIGH, ~10ms measurement time)
 */
SensorData_t readSHT40();

/**
 * @brief Read 3-axis acceleration from ADXL345
 * @return ShockData_t with raw values, g-force values, and peak_g
 * @note Configured for ±16g range, 100Hz output data rate
 */
ShockData_t readADXL345();

/**
 * @brief Read battery voltage and state of charge from MAX17048
 * @return BatteryData_t with voltage_mv, soc_pct, and is_charging
 */
BatteryData_t readBattery();
