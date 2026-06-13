/**
 * @file sensors.cpp
 * @brief CargoPulse v2.0 — Sensor Driver Implementations
 *
 * Implements drivers for:
 *   - Sensirion SHT40 (I2C) — Temperature & Humidity
 *   - ADXL345 (SPI) — 3-axis accelerometer, ±16g
 *   - MAX17048 (I2C) — LiPo fuel gauge
 */

#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <SPI.h>

// =============================================================================
// SHT40 CONSTANTS (Sensirion SHT40 — I2C address 0x44)
// Datasheet: https://sensirion.com/media/documents/33C09C07/620C2294/Datasheet_SHT4x.pdf
// =============================================================================
#define SHT40_CMD_MEASURE_HIGH   0xFD  // High repeatability measurement command
#define SHT40_MEASURE_DELAY_MS   10    // ~10ms for high precision measurement

// =============================================================================
// ADXL345 REGISTER MAP (SPI, CS=GPIO5)
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf
// =============================================================================
#define ADXL345_REG_DEVID        0x00  // Device ID — should read 0xE5
#define ADXL345_REG_BW_RATE      0x2C  // Data rate and power mode
#define ADXL345_REG_POWER_CTL    0x2D  // Power-saving features
#define ADXL345_REG_DATA_FORMAT  0x31  // Data format register
#define ADXL345_REG_DATAX0       0x32  // X-axis data LSB (auto-increments to X1, Y0, Y1, Z0, Z1)
#define ADXL345_READ_BIT         0x80  // Set MSB for read operations
#define ADXL345_MB_BIT           0x40  // Set bit 6 for multi-byte reads
#define ADXL345_SCALE_16G        0x0B  // ±16g range, full resolution
#define ADXL345_SCALE_FACTOR     0.004f // 4mg per LSB at ±16g full resolution

// =============================================================================
// MAX17048 REGISTERS (I2C address 0x36)
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/max17048-max17049.pdf
// =============================================================================
#define MAX17048_REG_VCELL       0x02  // Battery voltage (2 bytes, 78.125µV/bit)
#define MAX17048_REG_SOC         0x04  // State of charge (high byte = %, low byte = 1/256%)
#define MAX17048_VCELL_MULT      0.000078125f // 78.125µV per LSB

// =============================================================================
// MODULE-LEVEL STATE
// =============================================================================
static bool  sht40_ok    = false;
static bool  adxl345_ok  = false;
static bool  max17048_ok = false;

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

/**
 * @brief Read 2 bytes from an I2C device register
 */
static uint16_t i2c_read16(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    Wire.requestFrom(addr, (uint8_t)2);
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    return ((uint16_t)hi << 8) | lo;
}

/**
 * @brief CRC8 checksum for SHT40 data validation
 * Polynomial: 0x31, Initial value: 0xFF
 */
static uint8_t sht40_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

/**
 * @brief ADXL345 SPI write register
 */
static void adxl_write_reg(uint8_t reg, uint8_t val) {
    digitalWrite(PIN_ADXL_CS, LOW);
    SPI.transfer(reg & 0x3F); // Write: bit7=0, bit6=0
    SPI.transfer(val);
    digitalWrite(PIN_ADXL_CS, HIGH);
}

/**
 * @brief ADXL345 SPI read register(s)
 */
static void adxl_read_regs(uint8_t reg, uint8_t *buf, uint8_t len) {
    uint8_t cmd = ADXL345_READ_BIT | (len > 1 ? ADXL345_MB_BIT : 0) | (reg & 0x3F);
    digitalWrite(PIN_ADXL_CS, LOW);
    SPI.transfer(cmd);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = SPI.transfer(0x00);
    }
    digitalWrite(PIN_ADXL_CS, HIGH);
}

// =============================================================================
// PUBLIC IMPLEMENTATIONS
// =============================================================================

bool initSensors() {
    bool all_ok = true;

    // --- SHT40 probe ---
    Wire.beginTransmission(ADDR_SHT40);
    if (Wire.endTransmission() == 0) {
        sht40_ok = true;
        Serial.println("[SENSOR] SHT40 found at 0x44");
    } else {
        Serial.println("[SENSOR] SHT40 NOT found — check I2C");
        all_ok = false;
    }

    // --- ADXL345 init ---
    pinMode(PIN_ADXL_CS, OUTPUT);
    digitalWrite(PIN_ADXL_CS, HIGH);
    delay(10);

    uint8_t devid = 0;
    adxl_read_regs(ADXL345_REG_DEVID, &devid, 1);
    if (devid == 0xE5) {
        // Configure ADXL345:
        adxl_write_reg(ADXL345_REG_DATA_FORMAT, ADXL345_SCALE_16G); // ±16g, full res
        adxl_write_reg(ADXL345_REG_BW_RATE, 0x0A);                  // 100 Hz ODR
        adxl_write_reg(ADXL345_REG_POWER_CTL, 0x08);                // Measure mode
        adxl345_ok = true;
        Serial.println("[SENSOR] ADXL345 init OK (±16g, 100Hz)");
    } else {
        Serial.printf("[SENSOR] ADXL345 NOT found — DEVID=0x%02X (expected 0xE5)\n", devid);
        // TODO: [HARDWARE-TEST] Check SPI wiring and CS pin
        all_ok = false;
    }

    // --- MAX17048 probe ---
    Wire.beginTransmission(ADDR_MAX17048);
    if (Wire.endTransmission() == 0) {
        max17048_ok = true;
        Serial.println("[SENSOR] MAX17048 found at 0x36");
    } else {
        Serial.println("[SENSOR] MAX17048 NOT found — check I2C/power");
        all_ok = false;
    }

    return all_ok;
}

SensorData_t readSHT40() {
    SensorData_t result = {0};

    if (!sht40_ok) {
        // Return simulated data for testing when hardware absent
        result.temp_c       = 4.0f + (millis() / 10000.0f); // Simulated rising temp
        result.humidity_pct = 65.0f;
        return result;
    }

    // Send high-precision measurement command
    Wire.beginTransmission(ADDR_SHT40);
    Wire.write(SHT40_CMD_MEASURE_HIGH);
    Wire.endTransmission();
    delay(SHT40_MEASURE_DELAY_MS);

    // Read 6 bytes: T_MSB, T_LSB, T_CRC, RH_MSB, RH_LSB, RH_CRC
    Wire.requestFrom(ADDR_SHT40, (uint8_t)6);
    uint8_t raw[6];
    for (int i = 0; i < 6; i++) raw[i] = Wire.read();

    // Verify CRC
    if (sht40_crc8(raw, 2) != raw[2] || sht40_crc8(raw + 3, 2) != raw[5]) {
        Serial.println("[SENSOR] SHT40 CRC error — discarding reading");
        return result;
    }

    // Convert raw values (per SHT40 datasheet Table 5)
    uint16_t t_raw  = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t rh_raw = ((uint16_t)raw[3] << 8) | raw[4];

    result.temp_c       = -45.0f + 175.0f * ((float)t_raw  / 65535.0f);
    result.humidity_pct =  -6.0f + 125.0f * ((float)rh_raw / 65535.0f);

    // Clamp humidity 0-100%
    result.humidity_pct = constrain(result.humidity_pct, 0.0f, 100.0f);

    return result;
}

ShockData_t readADXL345() {
    ShockData_t result = {0};

    if (!adxl345_ok) {
        result.peak_g = 0.0f;
        return result;
    }

    // Read 6 bytes starting from DATAX0 (X0, X1, Y0, Y1, Z0, Z1)
    uint8_t buf[6];
    adxl_read_regs(ADXL345_REG_DATAX0, buf, 6);

    // Combine bytes into signed 16-bit values (little-endian)
    result.x_raw = (int16_t)((buf[1] << 8) | buf[0]);
    result.y_raw = (int16_t)((buf[3] << 8) | buf[2]);
    result.z_raw = (int16_t)((buf[5] << 8) | buf[4]);

    // Convert to g-force using 4mg/LSB scale factor
    result.x_g = result.x_raw * ADXL345_SCALE_FACTOR;
    result.y_g = result.y_raw * ADXL345_SCALE_FACTOR;
    result.z_g = result.z_raw * ADXL345_SCALE_FACTOR;

    // Compute resultant vector magnitude (excluding gravity on Z)
    // Peak g = sqrt(x² + y² + (z-1g)²) to remove static gravity component
    float x2 = result.x_g * result.x_g;
    float y2 = result.y_g * result.y_g;
    float z_dyn = result.z_g - 1.0f; // Subtract 1g gravity
    result.peak_g = sqrt(x2 + y2 + z_dyn * z_dyn);

    return result;
}

BatteryData_t readBattery() {
    BatteryData_t result = {3700.0f, 80, false};

    if (!max17048_ok) return result;

    // Read VCELL register (12-bit ADC, 78.125µV/LSB, MSB first)
    uint16_t vcell_raw = i2c_read16(ADDR_MAX17048, MAX17048_REG_VCELL);
    result.voltage_mv = (vcell_raw >> 4) * MAX17048_VCELL_MULT * 1000.0f; // Convert to mV

    // Read SOC register (high byte = integer %, low byte = 1/256%)
    uint16_t soc_raw = i2c_read16(ADDR_MAX17048, MAX17048_REG_SOC);
    result.soc_pct = (uint8_t)(soc_raw >> 8);

    // Charging detection: voltage rising + TP4056 CHRG pin (TODO: wire CHRG to GPIO)
    // For now, assume not charging (external detection to be added)
    result.is_charging = false;

    return result;
}
