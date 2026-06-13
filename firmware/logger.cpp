/**
 * @file logger.cpp
 * @brief Cryo Sentinel — SPI NOR Flash Logger Implementation
 *
 * Uses the SPIFlash library (Mayoogh Girish / Mikal Hart fork compatible with
 * Winbond W25Q32 4MB NOR flash) for low-level read/write/erase.
 *
 * Library: https://github.com/Marzogh/SPIMemory (install via Arduino Library Manager)
 *
 * Flash Layout:
 *   Sector 0 (4KB):   Header — magic, write pointer, entry count
 *   Sectors 1-1023:   Log data — circular buffer of 32-byte LogEntry_t records
 */

#include "logger.h"
#include "config.h"
#include <SPI.h>
#include <mbedtls/sha256.h>
// #include <SPIMemory.h>  // TODO: [HARDWARE-TEST] Uncomment when SPIMemory library installed

// =============================================================================
// FLASH LAYOUT CONSTANTS
// =============================================================================
#define FLASH_HEADER_ADDR   0x000000  // Start of header sector
#define FLASH_LOG_START     0x001000  // Log data starts at 4KB offset
#define FLASH_PAGE_SIZE     256       // SPI NOR flash page size (bytes)
#define FLASH_SECTOR_SIZE   4096      // Erase sector size (bytes)
#define ENTRY_SIZE          sizeof(LogEntry_t) // Should be 32 bytes

// Flash header magic number to detect first boot vs. valid log
#define FLASH_MAGIC         0xCE10C3A1  // Magic: CE=Cryo Sentinel, 10=v1.0, C3=ESP32-C3

// =============================================================================
// HEADER STRUCTURE (stored at FLASH_HEADER_ADDR)
// =============================================================================
typedef struct __attribute__((packed)) {
    uint32_t magic;         ///< Must equal FLASH_MAGIC
    uint32_t write_ptr;     ///< Next write address (absolute flash address)
    uint32_t entry_count;   ///< Total entries written (wraps at LOG_MAX_ENTRIES)
    uint32_t log_start;     ///< Absolute address of log start (FLASH_LOG_START)
    uint8_t  version;       ///< Log format version (1)
    uint8_t  reserved[11];  ///< Padding to 32 bytes
} FlashHeader_t;

// =============================================================================
// MODULE STATE
// =============================================================================
static FlashHeader_t header    = {0};
static bool          flash_ok  = false;
// static SPIFlash flash(PIN_ADXL_CS); // TODO: [HARDWARE-TEST] Use dedicated flash CS pin

// =============================================================================
// SECURE LEDGER HASH-CHAINING (SHA-256 context)
// =============================================================================
#include <esp_attr.h>
RTC_DATA_ATTR static uint8_t running_hash[32] = {0};
RTC_DATA_ATTR static bool rtc_hash_valid = false;

static void calculateSHA256(const uint8_t *data1, size_t len1, const uint8_t *data2, size_t len2, uint8_t *output) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256
    mbedtls_sha256_update_ret(&ctx, data1, len1);
    mbedtls_sha256_update_ret(&ctx, data2, len2);
    mbedtls_sha256_finish_ret(&ctx, output);
    mbedtls_sha256_free(&ctx);
}

// =============================================================================
// CRC8 (reused from sensors.cpp — polynomial 0x31)
// =============================================================================
static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

// =============================================================================
// SIMULATED FLASH (in-memory for testing without hardware)
// =============================================================================
// TODO: [HARDWARE-TEST] Remove simulation block and use SPIMemory library
#define SIM_FLASH_ENTRIES   256  // In-memory simulation stores 256 entries max
static LogEntry_t  sim_flash[SIM_FLASH_ENTRIES];
static uint32_t    sim_write_ptr  = 0;
static uint32_t    sim_entry_count = 0;

// =============================================================================
// PUBLIC IMPLEMENTATIONS
// =============================================================================

bool initLogger() {
    // TODO: [HARDWARE-TEST] Replace with actual SPIMemory init:
    // if (!flash.begin()) {
    //     Serial.println("[LOGGER] SPI flash init failed");
    //     return false;
    // }
    // flash.readAnything(FLASH_HEADER_ADDR, header);
    // if (header.magic != FLASH_MAGIC) {
    //     // First boot — format flash
    //     header.magic       = FLASH_MAGIC;
    //     header.write_ptr   = FLASH_LOG_START;
    //     header.entry_count = 0;
    //     header.log_start   = FLASH_LOG_START;
    //     header.version     = 1;
    //     flash.eraseBlock32K(0);
    //     flash.writeAnything(FLASH_HEADER_ADDR, header);
    // }

    // Simulation init
    memset(sim_flash, 0, sizeof(sim_flash));
    sim_write_ptr   = 0;
    sim_entry_count = 0;
    if (!rtc_hash_valid) {
        memset(running_hash, 0, 32);
        rtc_hash_valid = true;
    }
    flash_ok = true;
    Serial.println("[LOGGER] Flash logger initialized (simulation mode)");
    Serial.printf("[LOGGER] Entry size: %d bytes | Max entries: %d\n",
                  ENTRY_SIZE, SIM_FLASH_ENTRIES);
    return true;
}

bool writeLogEntry(uint32_t timestamp, float temp_c, float humidity,
                   float shock_g, float lat, float lon, uint8_t flags) {
    if (!flash_ok) return false;

    LogEntry_t entry;
    entry.timestamp    = timestamp;
    entry.temp_c       = temp_c;
    entry.humidity_pct = humidity;
    entry.shock_g      = shock_g;
    entry.lat          = lat;
    entry.lon          = lon;
    entry.flags        = flags;
    entry.breach_type  = (flags & LOG_FLAG_BREACH) ? 1 : 0;
    entry.soc_pct      = 80; // TODO: Pass actual SOC from BatteryData_t
    entry.crc8         = crc8((const uint8_t*)&entry, ENTRY_SIZE - 1);

    // TODO: [HARDWARE-TEST] Write to actual SPI flash:
    // uint32_t addr = header.log_start + (header.write_ptr % LOG_MAX_ENTRIES) * ENTRY_SIZE;
    // if (addr % FLASH_SECTOR_SIZE == 0) flash.eraseSector(addr); // Erase before write
    // flash.writeAnything(addr, entry);
    // header.write_ptr = (header.write_ptr + 1) % LOG_MAX_ENTRIES;
    // header.entry_count++;
    // flash.writeAnything(FLASH_HEADER_ADDR, header);

    // Simulation write
    sim_flash[sim_write_ptr % SIM_FLASH_ENTRIES] = entry;
    sim_write_ptr++;
    sim_entry_count++;

    // Compute cryptographic hash chain: new_hash = SHA256(entry + previous_hash)
    uint8_t new_hash[32];
    calculateSHA256((const uint8_t*)&entry, sizeof(LogEntry_t), running_hash, 32, new_hash);
    memcpy(running_hash, new_hash, 32);

    Serial.printf("[LOGGER] Entry #%lu written | T=%.2f°C H=%.1f%% S=%.2fg\n",
                  sim_entry_count, temp_c, humidity, shock_g);
    Serial.print("[SECURE-LEDGER] Hash Chain: 0x");
    for (int i = 0; i < 8; i++) Serial.printf("%02X", running_hash[i]);
    Serial.println("...");
    return true;
}

bool readLogEntry(uint32_t index, LogEntry_t *entry) {
    if (!flash_ok || entry == nullptr) return false;
    if (index >= sim_entry_count) return false;

    // TODO: [HARDWARE-TEST] Read from actual SPI flash:
    // uint32_t oldest = (header.entry_count > LOG_MAX_ENTRIES)
    //                 ? header.write_ptr : 0;
    // uint32_t actual_idx = (oldest + index) % LOG_MAX_ENTRIES;
    // uint32_t addr = header.log_start + actual_idx * ENTRY_SIZE;
    // flash.readAnything(addr, *entry);
    // return (entry->crc8 == crc8((const uint8_t*)entry, ENTRY_SIZE - 1));

    // Simulation read
    *entry = sim_flash[index % SIM_FLASH_ENTRIES];
    return (entry->crc8 == crc8((const uint8_t*)entry, ENTRY_SIZE - 1));
}

uint32_t getLogCount() {
    return sim_entry_count;
    // TODO: [HARDWARE-TEST] return header.entry_count;
}

void clearLog() {
    memset(sim_flash, 0, sizeof(sim_flash));
    sim_write_ptr   = 0;
    sim_entry_count = 0;
    memset(running_hash, 0, 32);
    Serial.println("[LOGGER] Log cleared");
    // TODO: [HARDWARE-TEST] flash.eraseChip();
}

uint8_t getFlashUsagePct() {
    return (uint8_t)((sim_entry_count * 100UL) / SIM_FLASH_ENTRIES);
}

void getRunningHash(uint8_t *hash_buf) {
    if (hash_buf != nullptr) {
        memcpy(hash_buf, running_hash, 32);
    }
}
