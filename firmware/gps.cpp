/**
 * @file gps.cpp
 * @brief Cryo Sentinel — GPS Implementation (u-blox MAX-M10S via TinyGPS++)
 *
 * Library: TinyGPS++ by Mikal Hart
 * Install: Arduino Library Manager → "TinyGPS++"
 * Docs: http://arduiniana.org/libraries/tinygpsplus/
 */

#include "gps.h"
#include "config.h"
#include <TinyGPSPlus.h>

// =============================================================================
// GPS UART (ESP32-C3 Serial1 on GPIO20/21)
// u-blox MAX-M10S default baud: 9600
// =============================================================================
// TinyGPSPlus instance
static TinyGPSPlus gps;

// Last valid fix (cached for use when no current fix)
static GPSData_t lastFix = {0.0, 0.0, 0.0, 99.0, 0, 0, 0};
static bool gps_initialized = false;

// =============================================================================
// INIT
// =============================================================================
bool initGPS() {
    // Initialize UART1 on ESP32-C3 with GPS TX/RX pins
    Serial1.begin(GPS_UART_BAUD, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX);
    delay(100);

    gps_initialized = true;
    Serial.printf("[GPS] u-blox MAX-M10S UART init — TX=GPIO%d, RX=GPIO%d, %d baud\n",
                  PIN_GPS_TX, PIN_GPS_RX, GPS_UART_BAUD);
    Serial.println("[GPS] Waiting for fix (may take up to 30s cold start)...");
    return true;
}

// =============================================================================
// GET LOCATION (blocking with timeout)
// =============================================================================
GPSData_t getLocation() {
    if (!gps_initialized) return lastFix;

    uint32_t start = millis();
    bool fix_obtained = false;

    // Feed NMEA data until fix obtained or timeout
    while (millis() - start < GPS_TIMEOUT_MS) {
        while (Serial1.available()) {
            char c = Serial1.read();
            gps.encode(c); // Feed each character to TinyGPS++ parser
        }

        // Check if we have a valid location fix
        if (gps.location.isValid() && gps.location.isUpdated()) {
            fix_obtained = true;

            lastFix.lat         = gps.location.lat();
            lastFix.lon         = gps.location.lng();
            lastFix.alt_m       = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
            lastFix.hdop        = gps.hdop.isValid()     ? (float)gps.hdop.hdop() : 99.0f;
            lastFix.satellites  = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
            lastFix.fix_quality = 1; // Basic GPS fix

            // Extract UTC time as HHMMSS integer
            if (gps.time.isValid()) {
                lastFix.utc_time = gps.time.hour()   * 10000UL
                                 + gps.time.minute() * 100UL
                                 + gps.time.second();
            }

            Serial.printf("[GPS] Fix: %.6f, %.6f | Alt: %.0fm | HDOP: %.1f | Sats: %d\n",
                          lastFix.lat, lastFix.lon, lastFix.alt_m,
                          lastFix.hdop, lastFix.satellites);
            break;
        }

        // Yield to other tasks during wait
        delay(10);
    }

    if (!fix_obtained) {
        Serial.println("[GPS] Timeout — returning last known location");
        // fix_quality = 0 indicates stale/no fix
    }

    return lastFix;
}

// =============================================================================
// POWER MANAGEMENT (u-blox UBX protocol)
// =============================================================================
void powerDownGPS() {
    // UBX-RXM-PMREQ: Request GPS to enter backup mode (preserves almanac)
    // Command bytes per u-blox MAX-M10S integration manual
    const uint8_t ubx_sleep[] = {
        0xB5, 0x62, // UBX header
        0x02, 0x41, // Class=RXM, ID=PMREQ
        0x08, 0x00, // Payload length = 8
        0x00, 0x00, 0x00, 0x00, // Duration = 0 (indefinite backup)
        0x02, 0x00, 0x00, 0x00, // Flags: bit1=backup
        0x4D, 0x3B  // Checksum (CK_A, CK_B)
    };
    Serial1.write(ubx_sleep, sizeof(ubx_sleep));
    Serial.println("[GPS] Sent UBX backup command — GPS powering down");
    // TODO: [HARDWARE-TEST] Verify via Serial monitor that NMEA sentences stop
}

void wakeGPS() {
    // Wake by toggling UART TX line (any character wakes from backup mode)
    Serial1.write(0xFF);
    delay(100);
    Serial.println("[GPS] Wake signal sent — waiting for GPS restart...");
    delay(GPS_WARMUP_MS); // Wait for almanac-aided startup
}

void feedGPS() {
    while (Serial1.available()) {
        gps.encode(Serial1.read());
    }
}

GPSData_t getLastKnownLocation() {
    return lastFix;
}
