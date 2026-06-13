# CargoPulse v2.0 — Intelligent Cold Chain Guardian

> **FAR AWAY 2026 Hackathon** | Logistics & Transit Theme
> Built on top of the open source [iot-risk-data-logger-nfc-samd21](https://github.com/polesskiy-dev/iot-risk-data-logger-nfc-samd21-hardware) by polesskiy-dev (MIT License)

---

## The Problem

India loses **₹92,000 crore worth of pharmaceutical products annually** due to cold chain failures — yet 68% of breaches go undetected until the shipment reaches its destination. Traditional loggers are passive: they record data, but cannot raise alarms, reveal breach locations, or confirm package integrity during transit.

## What CargoPulse Does

CargoPulse v2.0 is an intelligent cold chain sentinel that combines real-time LoRa wireless alerting, GPS breach-location tagging, cryptographically-signed tamper-proof logs, and a driver-facing e-ink status display into a single compact unit. When temperature, humidity, or shock thresholds are crossed, it instantly transmits an alert over LoRa (865 MHz India band) with GPS coordinates and a hardware-signed payload — enabling logistics operators to intervene mid-route, not at destination.

## Why V1 Failed → What V2 Fixes

| Feature | V1 (SAMD21) | V2 (CargoPulse) |
|---|---|---|
| **Wireless Range** | NFC only (≤10 cm) | LoRa SX1262 (≤15 km) |
| **Alert Timing** | At destination only | Real-time mid-route |
| **Breach Location** | Unknown | GPS-tagged (u-blox MAX-M10S) |
| **Driver Feedback** | None | 1.54" e-ink status display |
| **Power** | 2×AAA (non-rechargeable) | LiPo + USB-C charging (TP4056) |
| **Enclosure** | Indoor-only | IP65-rated for Indian outdoor use |

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    CargoPulse v2.0                          │
│                                                             │
│  ┌──────────────┐   I2C    ┌──────────┐  ┌──────────────┐  │
│  │ ESP32-C3-    │◄────────►│  SHT40   │  │  ATECC608A   │  │
│  │ MINI-1 MCU   │          │ Temp/Hum │  │ HW Crypto    │  │
│  │              │◄────────►│ ST25DV   │  │ (I2C)        │  │
│  │              │          │ NFC      │  └──────────────┘  │
│  │              │   SPI    ┌──────────┐                     │
│  │              │◄────────►│ ADXL345  │  LoRa SX1262       │
│  │              │          │ Shock    │  ┌──────────────┐  │
│  │              │◄────────►│ e-ink    │  │ 865 MHz TX   │  │
│  │              │          │ Display  │  │ U.FL Antenna │  │
│  │              │   UART   ┌──────────┐  └──────────────┘  │
│  │              │◄────────►│MAX-M10S  │                     │
│  └──────────────┘          │   GPS    │  ┌──────────────┐  │
│         │                  └──────────┘  │TP4056+LiPo   │  │
│         │                               │MAX17048 Fuel  │  │
│         └───────────────────────────────│Gauge+USB-C    │  │
│                                         └──────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Hardware — What's New in V2

| Component | Purpose | Why Chosen |
|---|---|---|
| **ESP32-C3-MINI-1** | Main MCU | Native USB, WiFi+BLE, LoRa-SPI capable, 160MHz |
| **SX1262 LoRa** | Long-range alerting | 15km range, 865MHz India band, RadioLib support |
| **u-blox MAX-M10S** | GPS location | Ultra-low-power, <1.5m accuracy, switchable domain |
| **1.54" e-ink** | Driver display | Zero-power hold, sunlight readable, 200×200px |
| **TP4056** | LiPo charging | USB-C input, built-in protection, ₹35/unit |
| **MAX17048** | Fuel gauge | 1% SOC accuracy over I2C |
| **ADXL345** | 3-axis shock | ±16g range, SPI, industry standard |
| **SHT40** | Temp/Humidity | ±0.2°C accuracy, I2C, replaces SHT3x |
| **ST25DV04K** | NFC tag | Backward compatible read for phones |
| **ATECC608A** | Hardware crypto | Signs every payload — tamper-proof chain of custody |

## Firmware Architecture

The firmware runs a 5-state machine on the ESP32-C3:

```
IDLE → (timer 5min) → SAMPLING → (threshold OK) → IDLE
                              → (threshold BREACH) → BREACH_ALERT → IDLE
IDLE → (NFC poll) → NFC_SERVE → IDLE
Any state → (battery critical) → SLEEP
```

- **IDLE**: Deep sleep, wake on RTC timer every 5 minutes
- **SAMPLING**: Read SHT40 + ADXL345 + MAX17048 + GPS, write to NOR flash log
- **BREACH_ALERT**: Sign payload with ATECC608A, transmit LoRa JSON, flash RGB RED, buzz, update e-ink
- **NFC_SERVE**: Respond to NFC reader with last N log entries
- **SLEEP**: Extreme power save when battery < 10%

## Simulation Demo

The Wokwi simulation sketch is in `/simulation/wokwi_sketch.ino`.

To run:
1. Go to [wokwi.com](https://wokwi.com)
2. Create new ESP32 project
3. Paste `wokwi_sketch.ino` content
4. Import `wokwi.json` as the diagram
5. Run — watch temperature rise from 4°C to 8°C breach

Expected output at breach:
```
⚠ BREACH DETECTED | Temp: 8.2°C | Humidity: 65% | GPS: 28.6139,77.2090 | LoRa TX: SENT | Sig: 0xA3F2...
RGB: RED
BUZZER: ON
```

## Live Dashboard Demo

Open `/docs/demo_dashboard.html` in any modern browser.

- No server needed — pure HTML/JS/Leaflet.js
- Map centered on Delhi with breach marker
- Real-time timeline sidebar: Normal → Warning → BREACH

## Power Budget

| Mode | Current Draw | Duration/Day | Energy |
|---|---|---|---|
| **Deep Sleep** | 150 µA | 23.5 hrs | 3.525 mAh |
| **Sampling** | 45 mA | 0.4 hrs | 18 mAh |
| **LoRa TX (breach)** | 120 mA | 0.05 hrs | 6 mAh |
| **GPS Fix** | 18 mA | 0.08 hrs | 1.44 mAh |
| **Total/Day** | — | — | **~29 mAh** |
| **1000mAh LiPo** | — | — | **~34 day runtime** |

## BOM & Cost

| Component | Part | Qty | Unit (INR) | Total |
|---|---|---|---|---|
| MCU | ESP32-C3-MINI-1 | 1 | ₹180 | ₹180 |
| LoRa | SX1262 module (EBYTE E22) | 1 | ₹320 | ₹320 |
| GPS | u-blox MAX-M10S module | 1 | ₹450 | ₹450 |
| Display | 1.54" e-ink module | 1 | ₹280 | ₹280 |
| Sensor | SHT40 | 1 | ₹95 | ₹95 |
| Accel | ADXL345 breakout | 1 | ₹65 | ₹65 |
| Crypto | ATECC608A | 1 | ₹55 | ₹55 |
| NFC | ST25DV04K | 1 | ₹75 | ₹75 |
| Charger | TP4056 module | 1 | ₹35 | ₹35 |
| Fuel gauge | MAX17048 | 1 | ₹85 | ₹85 |
| Battery | LiPo 1000mAh | 1 | ₹120 | ₹120 |
| Passives/PCB | Resistors, caps, PCB | 1 | ₹40 | ₹40 |
| **TOTAL** | | | | **₹1,800** |

## Build Instructions

1. **Order PCB** — Upload `/hardware/cargopulse_v2_schematic.pdf` + Gerbers to JLCPCB (5 pcs ~₹350 shipped)
2. **Source Components** — Use BOM `/hardware/bom_v2.csv` with Mouser/Robu.in/DigiKey
3. **Flash Firmware** — Install Arduino IDE 2.x, add ESP32 board package, open `/firmware/main.ino`, select "ESP32C3 Dev Module", flash via USB-C
4. **Calibrate** — Run with Serial Monitor at 115200 baud, verify sensor readings match reference thermometer
5. **Field Test** — Place in cold box, verify LoRa alerts reach gateway at ≥500m distance

## Credits & License

**Original Project:**
polesskiy-dev / iot-risk-data-logger-nfc-samd21 (MIT License)
https://github.com/polesskiy-dev/iot-risk-data-logger-nfc-samd21-hardware

**CargoPulse v2.0 additions:**
hydra-eng (MIT License) — 2026

Full license text: see [LICENSE](LICENSE) file
