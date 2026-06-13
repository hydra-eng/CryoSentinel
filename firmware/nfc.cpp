/**
 * @file nfc.cpp
 * @brief Cryo Sentinel — ST25DV Dynamic NFC Tag Driver Implementation
 *
 * Implements NDEF Text Record formatting and I2C write cycles for
 * ST25DV dynamic tag (I2C address 0x53).
 */

#include "nfc.h"
#include "config.h"
#include "logger.h"
#include <Wire.h>

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

/**
 * @brief Write bytes to ST25DV user EEPROM memory
 * Writes in small blocks (up to 4 bytes) with EEPROM write cycle delay (5ms)
 * to ensure reliability across all dynamic tag versions.
 */
static bool writeST25DV_EEPROM(uint16_t mem_addr, const uint8_t *data, size_t len) {
    size_t written = 0;
    while (written < len) {
        size_t chunk = (len - written > 4) ? 4 : (len - written);
        
        Wire.beginTransmission(ADDR_ST25DV);
        Wire.write((uint8_t)((mem_addr + written) >> 8));    // Addr MSB
        Wire.write((uint8_t)((mem_addr + written) & 0xFF));   // Addr LSB
        
        for (size_t i = 0; i < chunk; i++) {
            Wire.write(data[written + i]);
        }
        
        if (Wire.endTransmission() != 0) {
            Serial.printf("[NFC] I2C write failed at addr 0x%04X\n", mem_addr + written);
            return false;
        }
        
        written += chunk;
        delay(5); // Wait for EEPROM internal write cycle (t_WR)
    }
    return true;
}

// =============================================================================
// PUBLIC IMPLEMENTATIONS
// =============================================================================

bool initNFC() {
    Wire.beginTransmission(ADDR_ST25DV);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("[NFC] ST25DV dynamic tag found at 0x53");
        return true;
    }
    Serial.println("[NFC] ST25DV dynamic tag NOT found — check I2C");
    return false;
}

bool updateNFCTag(float temp, float hum, float lat, float lon, const char* status) {
    uint8_t hash_buf[32];
    getRunningHash(hash_buf);

    char text[160];
    snprintf(text, sizeof(text),
             "ID:%s|T:%.1fC|H:%.0f%%|Loc:%.4f,%.4f|Stat:%s|Sig:%02X%02X%02X%02X",
             DEVICE_ID, temp, hum, lat, lon, status,
             hash_buf[0], hash_buf[1], hash_buf[2], hash_buf[3]);
             
    size_t text_len = strlen(text);
    if (text_len > 150) return false;

    // --- NDEF CC File (Capability Container) at offset 0x0000 ---
    // E1: NFC Forum magic
    // 40: Version 1.0 mapping
    // 40: 512 bytes memory size (ST25DV04K)
    // 01: Read/Write open access
    uint8_t cc_file[] = { 0xE1, 0x40, 0x40, 0x01 };
    if (!writeST25DV_EEPROM(0x0000, cc_file, 4)) {
        return false;
    }

    // --- NDEF Message Formatting starting at offset 0x0004 ---
    uint8_t payload_len = 3 + text_len;       // 1 status byte + 2 lang bytes + text_len
    uint8_t record_len = 4 + payload_len;     // 1 header + 1 type_len + 1 pay_len + 1 type + payload

    uint8_t ndef_buf[140];
    size_t idx = 0;
    
    ndef_buf[idx++] = 0x03;                   // NDEF Message TLV Tag
    ndef_buf[idx++] = record_len;             // Record Length
    ndef_buf[idx++] = 0xD1;                   // Record Header (MB=1, ME=1, SR=1, TNF=1 [Well-Known])
    ndef_buf[idx++] = 0x01;                   // Type Length (1 byte for 'T')
    ndef_buf[idx++] = payload_len;            // Payload Length
    ndef_buf[idx++] = 0x54;                   // Record Type: 'T' (Text Record)
    
    // Text Payload Header
    ndef_buf[idx++] = 0x02;                   // UTF-8 encoding, 2-byte lang code length
    ndef_buf[idx++] = 'e';                    // Lang: "en"
    ndef_buf[idx++] = 'n';
    
    // Copy Text
    memcpy(ndef_buf + idx, text, text_len);
    idx += text_len;
    
    // Terminator TLV
    ndef_buf[idx++] = 0xFE;

    bool success = writeST25DV_EEPROM(0x0004, ndef_buf, idx);
    if (success) {
        Serial.printf("[NFC] Dynamic tag updated: \"%s\"\n", text);
    }
    return success;
}
