/**
 * @file config.h
 * @brief CargoPulse v2.0 — Pin Definitions & Threshold Configuration
 * 
 * All GPIO assignments for ESP32-C3-MINI-1 module.
 * All operational thresholds for cold chain monitoring.
 */

#pragma once
#include <Arduino.h>

// =============================================================================
// DEVICE IDENTITY
// =============================================================================
#define DEVICE_ID           "CP-001"
#define FW_VERSION          "2.0.0"
#define HW_VERSION          "v2.0"

// =============================================================================
// I2C BUS PINS (SDA / SCL)
// =============================================================================
#define PIN_I2C_SDA         8   // GPIO8 — SDA for SHT40, ATECC608A, ST25DV, MAX17048
#define PIN_I2C_SCL         9   // GPIO9 — SCL for I2C bus

// =============================================================================
// SPI BUS PINS (shared: ADXL345, SX1262 LoRa, e-ink display)
// =============================================================================
#define PIN_SPI_MOSI        6   // GPIO6 — MOSI
#define PIN_SPI_MISO        2   // GPIO2 — MISO
#define PIN_SPI_CLK         4   // GPIO4 — CLK

// =============================================================================
// ADXL345 ACCELEROMETER (SPI)
// =============================================================================
#define PIN_ADXL_CS         5   // GPIO5 — Chip Select for ADXL345

// =============================================================================
// SX1262 LORA MODULE (SPI)
// =============================================================================
#define PIN_LORA_CS         7   // GPIO7  — LoRa Chip Select
#define PIN_LORA_BUSY       3   // GPIO3  — LoRa BUSY
#define PIN_LORA_DIO1       10  // GPIO10 — LoRa DIO1 (interrupt)
#define PIN_LORA_RST        1   // GPIO1  — LoRa Reset

// =============================================================================
// GPS MODULE (UART — u-blox MAX-M10S)
// =============================================================================
#define PIN_GPS_TX          21  // GPIO21 — GPS TX (ESP32 RX)
#define PIN_GPS_RX          20  // GPIO20 — GPS RX (ESP32 TX)
#define GPS_UART_BAUD       9600

// =============================================================================
// E-INK DISPLAY (SPI — 1.54" 200×200 FPC connector)
// =============================================================================
#define PIN_EINK_CS         19  // GPIO19 — e-ink Chip Select
#define PIN_EINK_DC         18  // GPIO18 — e-ink Data/Command
#define PIN_EINK_RST        17  // GPIO17 — e-ink Reset
#define PIN_EINK_BUSY       16  // GPIO16 — e-ink Busy

// =============================================================================
// POWER & BATTERY
// =============================================================================
#define PIN_BAT_LED         15  // GPIO15 — Battery indicator LED (GPIO → 330Ω → LED → GND)

// =============================================================================
// RGB LED INDICATORS (common cathode — active HIGH)
// =============================================================================
#define PIN_RGB_R           12  // GPIO12 — Red   channel (→ 330Ω → LED)
#define PIN_RGB_G           13  // GPIO13 — Green channel (→ 330Ω → LED)
#define PIN_RGB_B           14  // GPIO14 — Blue  channel (→ 330Ω → LED)

// =============================================================================
// BUZZER (NPN transistor drive)
// =============================================================================
#define PIN_BUZZER          11  // GPIO11 — Buzzer (GPIO → NPN base → buzzer → 3.3V)

// =============================================================================
// TAMPER DETECTION
// =============================================================================
// GPIO0 pulled HIGH via 10kΩ to 3.3V
// Conductive lid trace connects GPIO0 to GND when lid is CLOSED
// When lid is REMOVED: trace breaks, GPIO0 pulled HIGH → tamper detected
#define PIN_TAMPER          0   // GPIO0 — Tamper detection input (INPUT_PULLUP)
#define TAMPER_OPEN         HIGH  // Lid removed = GPIO reads HIGH (trace broken)

// =============================================================================
// I2C DEVICE ADDRESSES
// =============================================================================
#define ADDR_SHT40          0x44  // Sensirion SHT40 Temp/Humidity
#define ADDR_ATECC608A      0x60  // Microchip ATECC608A Hardware Crypto
#define ADDR_ST25DV         0x53  // ST ST25DV04K NFC tag (user memory)
#define ADDR_MAX17048       0x36  // Maxim MAX17048 Fuel Gauge

// =============================================================================
// COLD CHAIN THRESHOLDS
// =============================================================================
#define TEMP_MAX_C          8.0f  // Upper temperature limit (pharmaceutical cold chain)
#define TEMP_MIN_C          2.0f  // Lower temperature limit (freeze protection)
#define HUMIDITY_MAX_PCT    85.0f // Max relative humidity
#define HUMIDITY_MIN_PCT    15.0f // Min relative humidity
#define SHOCK_THRESHOLD_G   3.0f  // Shock alert threshold in g-force

// =============================================================================
// LORA CONFIGURATION (India 865 MHz band — In-building & outdoor logistics)
// =============================================================================
#define LORA_FREQUENCY      865.0f   // MHz — India SubGHz ISM band
#define LORA_BANDWIDTH      125.0f   // kHz
#define LORA_SPREADING      9        // SF9 — balance range vs airtime
#define LORA_CODING_RATE    7        // 4/7 coding rate
#define LORA_TX_POWER       14       // dBm (legal limit for India)
#define LORA_SYNC_WORD      0x12     // Private network sync word

// =============================================================================
// LOGGING CONFIGURATION
// =============================================================================
#define LOG_INTERVAL_SEC    300      // 5-minute sampling interval
#define LOG_MAX_ENTRIES     8192     // Max entries in circular buffer (4MB flash / ~512 bytes/entry)
#define LOG_MAGIC_BYTE      0xCF     // Flash log header magic

// =============================================================================
// POWER MANAGEMENT
// =============================================================================
#define BATTERY_LOW_PCT     15       // Low battery warning threshold
#define BATTERY_CRIT_PCT    5        // Critical — enter SLEEP state
#define GPS_WARMUP_MS       30000    // 30s GPS warmup before fix attempt
#define GPS_TIMEOUT_MS      60000    // 60s timeout for GPS fix

// =============================================================================
// STATE MACHINE STATES
// =============================================================================
typedef enum {
    STATE_IDLE          = 0,
    STATE_SAMPLING      = 1,
    STATE_BREACH_ALERT  = 2,
    STATE_NFC_SERVE     = 3,
    STATE_SLEEP         = 4
} DeviceState_t;

// =============================================================================
// BREACH TYPES
// =============================================================================
typedef enum {
    BREACH_NONE         = 0,
    BREACH_TEMP_HIGH    = 1,   // Temperature exceeded TEMP_MAX_C
    BREACH_TEMP_LOW     = 2,   // Temperature below TEMP_MIN_C
    BREACH_HUMIDITY     = 3,   // Humidity out of range
    BREACH_SHOCK        = 4,   // Shock exceeded SHOCK_THRESHOLD_G
    BREACH_TAMPER       = 5,   // Lid opened / tamper detected
    BREACH_COMBINED     = 6    // Multiple simultaneous breaches
} BreachType_t;
