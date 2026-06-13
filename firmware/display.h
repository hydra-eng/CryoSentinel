/**
 * @file display.h
 * @brief Cryo Sentinel — e-ink Display Interface Header
 *
 * Controls a 1.54" 200×200 e-ink display via GxEPD2 library.
 * Connected via 8-pin FPC connector to ESP32-C3 SPI bus.
 *
 * Library: GxEPD2 by Jean-Marc Zingg
 * Install: Arduino Library Manager → "GxEPD2"
 * GitHub: https://github.com/ZinggJM/GxEPD2
 */

#pragma once
#include <Arduino.h>
#include "sensors.h"  // For StatusType_t

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize e-ink display controller
 * @return true if display responds and initial screen is drawn
 * @note First init performs full refresh (takes ~2s for e-ink)
 */
bool initDisplay();

/**
 * @brief Update display with current readings
 * @param temp_c        Current temperature in °C
 * @param humidity_pct  Current relative humidity %
 * @param soc_pct       Battery state of charge (0-100%)
 * @param status        Device status (OK/WARNING/BREACH/SLEEP)
 *
 * Display layout (200×200 px):
 * ┌──────────────────────┐
 * │  Cryo Sentinel       │
 * │  ────────────────    │
 * │  🌡 Temp: 7.4°C     │
 * │  💧 Hum:  63%        │
 * │  🔋 Bat:  85%        │
 * │  ────────────────    │
 * │  ✅ STATUS: OK       │
 * └──────────────────────┘
 */
void updateDisplay(float temp_c, float humidity_pct,
                   uint8_t soc_pct, StatusType_t status);

/**
 * @brief Show breach alert on display (large warning)
 * @param breach_type  Type of breach detected
 * @param temp_c       Temperature at breach time
 */
void showBreachScreen(BreachType_t breach_type, float temp_c);

/**
 * @brief Clear display to white (full refresh)
 */
void clearDisplay();

/**
 * @brief Put display into deep sleep mode (zero power draw)
 * @note Display retains last image while in sleep — e-ink bistable property
 */
void sleepDisplay();
