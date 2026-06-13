/**
 * @file wokwi_sketch.ino
 * @brief Cryo Sentinel — Wokwi ESP32 Simulation Sketch
 *
 * Simulates Cryo Sentinel cold chain monitoring on Wokwi:
 * - Temperature starts at 4.0°C and rises 0.5°C every loop iteration
 * - At 8°C breach threshold: prints alert, simulates RGB RED and buzzer
 * - Continues logging after breach with BREACH flag set
 *
 * How to run:
 * 1. Go to https://wokwi.com
 * 2. Create new ESP32 project
 * 3. Replace sketch with this file
 * 4. Import wokwi.json as diagram
 * 5. Click Run
 *
 *         this sketch uses standard ESP32 as functional equivalent)
 */

// =============================================================================
// WOKWI PIN MAPPING (standard ESP32 DevKit)
// Mapped to simulate Cryo Sentinel hardware behavior
// =============================================================================
#define SIM_PIN_LED_R       25   // Red LED (simulates RGB Red channel)
#define SIM_PIN_LED_G       26   // Green LED (simulates RGB Green channel)  
#define SIM_PIN_LED_B       27   // Blue LED (simulates RGB Blue channel)
#define SIM_PIN_BUZZER      32   // Buzzer (active buzzer via NPN sim)
#define SIM_DHT_PIN         4    // DHT22 stand-in for SHT40

// =============================================================================
// SIMULATION PARAMETERS
// =============================================================================
#define TEMP_START_C        4.0f    // Starting temperature (°C)
#define TEMP_RISE_PER_LOOP  0.5f    // Temperature rise per sampling cycle (°C)
#define TEMP_BREACH_C       8.0f    // Breach threshold (°C)
#define SIM_HUMIDITY        65.0f   // Fixed simulated humidity (%)
#define SIM_GPS_LAT         28.6139 // New Delhi, India (Connaught Place)
#define SIM_GPS_LON         77.2090
#define SIM_DEVICE_ID       "CS-001"
#define SIM_LOOP_DELAY_MS   2000    // 2s between samples (real: 300s)

// =============================================================================
// SIMULATION STATE
// =============================================================================
static float   sim_temp       = TEMP_START_C;
static uint32_t log_index     = 0;
static bool    breach_active  = false;
static uint32_t boot_time_ms  = 0;

// =============================================================================
// HELPERS
// =============================================================================

/** Simulate setting RGB LED state */
void sim_setRGB(bool r, bool g, bool b) {
    digitalWrite(SIM_PIN_LED_R, r ? HIGH : LOW);
    digitalWrite(SIM_PIN_LED_G, g ? HIGH : LOW);
    digitalWrite(SIM_PIN_LED_B, b ? HIGH : LOW);
    
    String color = "OFF";
    if (r && !g && !b) color = "RED";
    else if (!r && g && !b) color = "GREEN";
    else if (!r && !g && b) color = "BLUE";
    else if (r && g && !b) color = "YELLOW";
    else if (r && g && b) color = "WHITE";
    
    Serial.print("RGB: ");
    Serial.println(color);
}

/** Generate simulated hex signature */
String sim_signature() {
    return "0xA3F2C3D1E8B2...";
}

/** Print log entry to Serial Monitor */
void printLogEntry(float temp, float hum, float lat, float lon,
                   bool is_breach, uint32_t entry_num) {
    Serial.print("LOG #");
    Serial.print(entry_num);
    Serial.print(" | Time: ");
    Serial.print((millis() - boot_time_ms) / 1000);
    Serial.print("s");
    Serial.print(" | Temp: ");
    Serial.print(temp, 1);
    Serial.print("°C");
    Serial.print(" | Hum: ");
    Serial.print(hum, 0);
    Serial.print("%");
    Serial.print(" | GPS: ");
    Serial.print(lat, 4);
    Serial.print(",");
    Serial.print(lon, 4);
    if (is_breach) {
        Serial.print(" | [BREACH]");
    }
    Serial.println();
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(SIM_PIN_LED_R, OUTPUT);
    pinMode(SIM_PIN_LED_G, OUTPUT);
    pinMode(SIM_PIN_LED_B, OUTPUT);
    pinMode(SIM_PIN_BUZZER, OUTPUT);

    boot_time_ms = millis();

    Serial.println("=========================================");
    Serial.println("  Cryo Sentinel — Wokwi Simulation");
    Serial.println("  Device: " SIM_DEVICE_ID);
    Serial.println("  Cold Chain: 2°C – 8°C");
    Serial.println("  LoRa: 865.0 MHz (India)");
    Serial.println("=========================================");
    Serial.println();

    // Boot: Green LED = ready
    sim_setRGB(false, true, false);
    delay(1000);
    sim_setRGB(false, false, false);

    Serial.println("[INIT] SHT40 (simulated via DHT22 stand-in) — OK");
    Serial.println("[INIT] ADXL345 (simulated) — OK");
    Serial.println("[INIT] MAX17048 (simulated) — OK");
    Serial.println("[INIT] SX1262 LoRa 865 MHz — OK");
    Serial.println("[INIT] GPS u-blox MAX-M10S — Fix acquired");
    Serial.println("[INIT] ATECC608A crypto — OK");
    Serial.println("[INIT] e-ink display — OK");
    Serial.println();
    Serial.println("--- Starting monitoring loop ---");
    Serial.println();
}

// =============================================================================
// LOOP — Main simulation
// =============================================================================
void loop() {
    log_index++;

    // --- Simulate rising temperature ---
    sim_temp += TEMP_RISE_PER_LOOP;

    // Clamp to realistic range
    if (sim_temp > 15.0f) sim_temp = TEMP_START_C; // Reset for demo

    float current_hum  = SIM_HUMIDITY + (float)(random(-50, 50)) / 10.0f;
    float sim_shock_g  = (float)(random(0, 20)) / 10.0f; // 0.0–2.0g (below threshold)
    uint8_t sim_bat    = 85 - (log_index / 2);            // Slowly drain battery
    sim_bat = max((uint8_t)20, sim_bat);

    // --- Check breach threshold ---
    if (sim_temp >= TEMP_BREACH_C && !breach_active) {
        breach_active = true;

        Serial.println();
        Serial.println("╔══════════════════════════════════════════════════╗");
        Serial.println("║          ⚠  BREACH DETECTED  ⚠                  ║");
        Serial.println("╚══════════════════════════════════════════════════╝");
        Serial.print("⚠ BREACH DETECTED | Temp: ");
        Serial.print(sim_temp, 1);
        Serial.print("°C | Humidity: ");
        Serial.print(current_hum, 0);
        Serial.print("% | GPS: ");
        Serial.print(SIM_GPS_LAT, 4);
        Serial.print(",");
        Serial.print(SIM_GPS_LON, 4);
        Serial.print(" | LoRa TX: SENT | Sig: ");
        Serial.println(sim_signature());
        Serial.println();

        // Visual alert
        sim_setRGB(true, false, false); // RED
        Serial.println("BUZZER: ON");
        digitalWrite(SIM_PIN_BUZZER, HIGH);

        // Buzzer pattern: 3 beeps
        for (int i = 0; i < 3; i++) {
            digitalWrite(SIM_PIN_BUZZER, HIGH);
            delay(250);
            digitalWrite(SIM_PIN_BUZZER, LOW);
            delay(100);
        }

        Serial.println("[LORA] Payload transmitted:");
        Serial.print("  {\"id\":\"" SIM_DEVICE_ID "\",\"bt\":\"TEMP_HIGH\",\"t\":");
        Serial.print(sim_temp, 2);
        Serial.print(",\"lat\":");
        Serial.print(SIM_GPS_LAT, 5);
        Serial.print(",\"lon\":");
        Serial.print(SIM_GPS_LON, 5);
        Serial.println(",\"ts\":1749814474}");

        Serial.println("[DISPLAY] Showing BREACH screen on e-ink");
        Serial.println();

    } else if (sim_temp < TEMP_BREACH_C && breach_active) {
        // Temperature dropped back below threshold
        breach_active = false;
        sim_setRGB(false, true, false); // Green = recovered
        Serial.println("BUZZER: OFF");
        digitalWrite(SIM_PIN_BUZZER, LOW);
        Serial.println("[STATUS] Temperature back in range");
        delay(500);
        sim_setRGB(false, false, false);
    }

    // --- Normal sampling log ---
    if (breach_active) {
        sim_setRGB(true, false, false); // Solid red during breach
    } else {
        // Brief green flash to indicate sampling
        sim_setRGB(false, true, false);
        delay(100);
        sim_setRGB(false, false, false);
    }

    // Log entry with breach flag
    printLogEntry(sim_temp, current_hum, SIM_GPS_LAT, SIM_GPS_LON,
                  breach_active, log_index);

    // Battery status
    if (log_index % 5 == 0) {
        Serial.print("[BATTERY] ");
        Serial.print(sim_bat);
        Serial.println("% SOC");
    }

    delay(SIM_LOOP_DELAY_MS);
}
