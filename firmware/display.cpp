/**
 * @file display.cpp
 * @brief CargoPulse v2.0 — e-ink Display Implementation (GxEPD2)
 *
 * Target: 1.54" 200×200 Good Display GDEW0154M09 or compatible
 * Driver IC: SSD1681 (most common for 1.54" B/W e-ink at this resolution)
 *
 * SPI pins: CLK=GPIO4, MOSI=GPIO6, CS=GPIO19, DC=GPIO18, RST=GPIO17, BUSY=GPIO16
 */

#include "display.h"
#include "config.h"
#include <SPI.h>

// GxEPD2 — install via Arduino Library Manager
// Also requires Adafruit GFX library
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

// =============================================================================
// DISPLAY DRIVER CONFIGURATION
// GxEPD2_154_GDEY0154D67 = 1.54" 200x200 B/W with SSD1681 driver
// GxEPD2_BW<driver, buffer_rows>(CS, DC, RST, BUSY)
// =============================================================================
GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT>
    display(PIN_EINK_CS, PIN_EINK_DC, PIN_EINK_RST, PIN_EINK_BUSY);

static bool display_ok = false;

// =============================================================================
// INIT
// =============================================================================
bool initDisplay() {
    // TODO: [HARDWARE-TEST] Ensure SPI.begin() called in setup() before this
    display.init(115200, true, 2, false); // (serial_diag_baud, initial, reset_duration, pulldown_rst)
    display.setRotation(0);              // Portrait, no rotation
    display.setTextColor(GxEPD_BLACK);

    // Clear to white on first init
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());

    display_ok = true;
    Serial.println("[DISPLAY] e-ink initialized (200x200)");

    // Draw boot screen
    updateDisplay(0.0f, 0.0f, 0, STATUS_OK);
    return true;
}

// =============================================================================
// UPDATE DISPLAY
// =============================================================================
void updateDisplay(float temp_c, float humidity_pct,
                   uint8_t soc_pct, StatusType_t status) {
    if (!display_ok) {
        // Print to serial as fallback when display absent
        Serial.printf("[DISPLAY] T=%.1f°C H=%.0f%% BAT=%d%% STATUS=%d\n",
                      temp_c, humidity_pct, soc_pct, (int)status);
        return;
    }

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        // --- Header ---
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(5, 18);
        display.print("CargoPulse v2.0");

        // Horizontal divider
        display.drawFastHLine(0, 24, 200, GxEPD_BLACK);

        // --- Temperature ---
        display.setFont(&FreeSans12pt7b);
        display.setCursor(5, 55);
        display.print("Temp: ");
        display.print(temp_c, 1);
        display.print((char)247); // Degree symbol
        display.print("C");

        // Visual temp bar (0-100% scaled to 2-8°C range)
        int bar_w = constrain((int)((temp_c - 2.0f) / 6.0f * 160), 0, 160);
        display.drawRect(5, 62, 160, 8, GxEPD_BLACK);
        display.fillRect(5, 62, bar_w, 8, GxEPD_BLACK);

        // --- Humidity ---
        display.setCursor(5, 95);
        display.print("Hum:  ");
        display.print((int)humidity_pct);
        display.print("%");

        // --- Battery ---
        display.setCursor(5, 130);
        display.print("Bat:  ");
        display.print(soc_pct);
        display.print("%");

        // Battery icon (outline + fill)
        display.drawRect(165, 118, 25, 14, GxEPD_BLACK);
        display.fillRect(190, 121, 4, 8, GxEPD_BLACK); // Nub
        int bat_fill = (soc_pct * 22) / 100;
        display.fillRect(166, 119, bat_fill, 12, GxEPD_BLACK);

        // Horizontal divider
        display.drawFastHLine(0, 145, 200, GxEPD_BLACK);

        // --- Status ---
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(5, 165);
        switch (status) {
            case STATUS_OK:
                display.print("STATUS: OK");
                break;
            case STATUS_WARNING:
                display.print("STATUS: WARNING");
                break;
            case STATUS_BREACH:
                display.setFont(&FreeSans12pt7b);
                display.setCursor(5, 165);
                display.print("! BREACH !");
                // Draw border for breach emphasis
                display.drawRect(0, 148, 200, 52, GxEPD_BLACK);
                display.drawRect(2, 150, 196, 48, GxEPD_BLACK);
                break;
            case STATUS_SLEEP:
                display.print("BATTERY LOW");
                display.setCursor(5, 185);
                display.print("Charge device");
                break;
        }

        // --- Timestamp footer ---
        display.setFont(nullptr); // Default small font
        display.setCursor(5, 195);
        display.print("ID: " DEVICE_ID "  FW:" FW_VERSION);

    } while (display.nextPage());
}

// =============================================================================
// BREACH SCREEN
// =============================================================================
void showBreachScreen(BreachType_t breach_type, float temp_c) {
    if (!display_ok) return;

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        // Bold warning frame
        display.drawRect(0, 0, 200, 200, GxEPD_BLACK);
        display.drawRect(3, 3, 194, 194, GxEPD_BLACK);

        display.setFont(&FreeSans12pt7b);
        display.setCursor(30, 50);
        display.print("! BREACH !");

        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(10, 90);
        switch (breach_type) {
            case BREACH_TEMP_HIGH: display.print("TEMP TOO HIGH"); break;
            case BREACH_TEMP_LOW:  display.print("TEMP TOO LOW");  break;
            case BREACH_SHOCK:     display.print("SHOCK DETECTED"); break;
            case BREACH_TAMPER:    display.print("TAMPER DETECTED"); break;
            default:               display.print("THRESHOLD EXCEEDED"); break;
        }

        display.setCursor(10, 120);
        display.print("Temp: ");
        display.print(temp_c, 2);
        display.print((char)247);
        display.print("C");

        display.setCursor(10, 150);
        display.print("Alert sent via LoRa");

        display.setCursor(10, 175);
        display.print("Contact: +91-XXXXXXXXXX");

    } while (display.nextPage());
}

void clearDisplay() {
    if (!display_ok) return;
    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
}

void sleepDisplay() {
    if (!display_ok) return;
    display.hibernate(); // GxEPD2 hibernate — zero power, retains image
    Serial.println("[DISPLAY] e-ink in hibernate (retains image, zero draw)");
}
