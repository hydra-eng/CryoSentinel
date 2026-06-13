/**
 * @file crypto.cpp
 * @brief CargoPulse v2.0 — ATECC608A Hardware Crypto Implementation
 *
 * Uses SparkFun ATECC608A library for hardware ECDSA signing.
 * Each device has a unique ECC private key stored in ATECC608A slot 0.
 * Signatures allow the cloud gateway to verify payload integrity.
 *
 * Based on: polesskiy-dev/iot-risk-data-logger-nfc-samd21 (MIT)
 * CargoPulse v2.0 modifications: hydra-eng — 2026
 */

#include "crypto.h"
#include "config.h"
#include <Wire.h>

// SparkFun ATECC608A Library
// Install: Arduino Library Manager → "SparkFun ATECC608A"
// #include <SparkFun_ATECCX08a_Arduino_Library.h>  // TODO: [HARDWARE-TEST] Uncomment

// =============================================================================
// MODULE STATE
// =============================================================================
// static ATECCX08A atecc;  // TODO: [HARDWARE-TEST] Uncomment with library
static bool crypto_ok    = false;
static uint8_t device_sn[9] = {0};

// =============================================================================
// SIMULATED SIGNATURE (for testing without hardware)
// In production, replaced by ATECC608A ECDSA P-256 signature
// =============================================================================
static uint8_t sim_sig_template[64] = {
    0xA3, 0xF2, 0x8B, 0x1C, 0xD4, 0x77, 0xE5, 0x29,
    0x3A, 0x8F, 0x01, 0x5D, 0xB2, 0xC9, 0xE8, 0x44,
    0x7F, 0x1A, 0x62, 0x35, 0x98, 0xCB, 0xDE, 0x06,
    0x4E, 0x93, 0xA1, 0xF5, 0x28, 0x7C, 0xB0, 0xD3,
    0x59, 0x1E, 0xAF, 0x82, 0x30, 0x6D, 0xC7, 0xE4,
    0x71, 0xBA, 0x03, 0x5F, 0xC8, 0xD9, 0x4B, 0x16,
    0x87, 0x2A, 0xF6, 0x41, 0x9E, 0xCC, 0x73, 0xDA,
    0x05, 0x60, 0xEB, 0xB8, 0x3D, 0x94, 0x1F, 0x7B
};

// =============================================================================
// INIT
// =============================================================================
bool initCrypto() {
    // TODO: [HARDWARE-TEST] Replace with real ATECC608A init:
    // Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    // if (!atecc.begin(Wire, CRYPTO_KEY_SLOT)) {
    //     Serial.println("[CRYPTO] ATECC608A not found — check I2C wiring at 0x60");
    //     return false;
    // }
    // if (!atecc.readConfigZone(configLockStatus, dataLockStatus, slotLockStatus)) {
    //     Serial.println("[CRYPTO] ATECC608A config read failed");
    //     return false;
    // }
    // atecc.readSerialNumber(device_sn);

    // Simulation init — probe I2C address 0x60
    Wire.beginTransmission(ADDR_ATECC608A);
    uint8_t result = Wire.endTransmission();
    if (result == 0) {
        crypto_ok = true;
        Serial.println("[CRYPTO] ATECC608A detected at 0x60");
    } else {
        Serial.println("[CRYPTO] ATECC608A not found — using simulated signatures");
        crypto_ok = false; // Simulation mode still works
    }

    // Generate simulated serial number based on millis
    device_sn[0] = 0x01; device_sn[1] = 0x23;
    for (int i = 2; i < 9; i++) device_sn[i] = (millis() >> (i*2)) & 0xFF;

    Serial.print("[CRYPTO] Device SN: ");
    for (int i = 0; i < 9; i++) Serial.printf("%02X", device_sn[i]);
    Serial.println();

    return true; // Returns true even in simulation for non-fatal operation
}

// =============================================================================
// SIGN PAYLOAD
// =============================================================================
bool signPayload(const uint8_t *data, size_t len, uint8_t *signature) {
    if (data == nullptr || signature == nullptr || len == 0) return false;

    // TODO: [HARDWARE-TEST] Real ATECC608A signing:
    // uint8_t hash[32];
    // atecc.sha256(data, len, hash);
    // if (!atecc.ecSign(CRYPTO_KEY_SLOT, hash, signature)) {
    //     Serial.println("[CRYPTO] Signing failed — ATECC608A error");
    //     return false;
    // }
    // return true;

    // Simulation: XOR data into template signature for deterministic test output
    memcpy(signature, sim_sig_template, CRYPTO_SIG_LEN);
    for (size_t i = 0; i < len && i < CRYPTO_SIG_LEN; i++) {
        signature[i % CRYPTO_SIG_LEN] ^= data[i];
    }
    // Make first 4 bytes memorable for Serial output
    signature[0] = 0xA3; signature[1] = 0xF2;
    signature[2] = 0xC3; signature[3] = 0xD1;

    return true;
}

// =============================================================================
// VERIFY SIGNATURE
// =============================================================================
bool verifySignature(const uint8_t *data, size_t len, const uint8_t *signature) {
    if (data == nullptr || signature == nullptr) return false;

    // TODO: [HARDWARE-TEST] Real ATECC608A verification:
    // uint8_t hash[32];
    // atecc.sha256(data, len, hash);
    // return atecc.ecVerify(hash, signature, pubkey);

    // Simulation: re-sign and compare
    uint8_t test_sig[64];
    if (!signPayload(data, len, test_sig)) return false;
    return (memcmp(test_sig, signature, CRYPTO_SIG_LEN) == 0);
}

// =============================================================================
// SERIAL NUMBER
// =============================================================================
bool getSerialNumber(uint8_t *sn_buf) {
    if (sn_buf == nullptr) return false;
    memcpy(sn_buf, device_sn, 9);
    return true;
}

// =============================================================================
// NONCE GENERATION
// =============================================================================
bool generateNonce(uint8_t *nonce_buf) {
    if (nonce_buf == nullptr) return false;

    // TODO: [HARDWARE-TEST] Use ATECC608A TRNG:
    // return atecc.genRand(nonce_buf, 32);

    // Simulation: use ESP32 hardware RNG
    for (int i = 0; i < 32; i++) {
        nonce_buf[i] = (uint8_t)(esp_random() & 0xFF);
    }
    return true;
}
