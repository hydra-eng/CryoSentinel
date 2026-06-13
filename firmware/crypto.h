/**
 * @file crypto.h
 * @brief CargoPulse v2.0 — ATECC608A Hardware Crypto Interface Header
 *
 * Provides hardware-backed payload signing using ATECC608A secure element.
 * Every LoRa alert payload is signed with the device's private key,
 * allowing the receiving gateway to verify payload authenticity.
 *
 * Library: SparkFun ATECC608A Arduino Library
 * Install: Arduino Library Manager → "SparkFun ATECC608A"
 * GitHub: https://github.com/sparkfun/SparkFun_ATECC608A_Arduino_Library
 */

#pragma once
#include <Arduino.h>

// =============================================================================
// CRYPTO CONSTANTS
// =============================================================================
#define CRYPTO_KEY_SLOT     0    ///< ATECC608A key slot for device private key
#define CRYPTO_SIG_LEN      64   ///< ECDSA P-256 signature length (bytes)
#define CRYPTO_PUBKEY_LEN   64   ///< Uncompressed public key (no 0x04 prefix)

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize ATECC608A secure element
 * @return true if device responds and serial number is readable
 * @note Device must be provisioned with key in slot CRYPTO_KEY_SLOT
 *       Provisioning is a one-time factory operation
 */
bool initCrypto();

/**
 * @brief Sign a data payload with the device private key (ECDSA P-256)
 * @param data      Pointer to data bytes to sign
 * @param len       Length of data in bytes
 * @param signature Output buffer, must be at least CRYPTO_SIG_LEN bytes
 * @return true if signature was generated successfully
 * @note ATECC608A computes SHA-256 hash internally before signing
 */
bool signPayload(const uint8_t *data, size_t len, uint8_t *signature);

/**
 * @brief Verify a signature against data and the device's stored public key
 * @param data      Pointer to original data
 * @param len       Length of data
 * @param signature 64-byte ECDSA signature to verify
 * @return true if signature is valid
 */
bool verifySignature(const uint8_t *data, size_t len, const uint8_t *signature);

/**
 * @brief Get the device serial number from ATECC608A
 * @param sn_buf  Output buffer, must be at least 9 bytes
 * @return true if serial number read successfully
 */
bool getSerialNumber(uint8_t *sn_buf);

/**
 * @brief Generate a random 32-byte nonce from the ATECC608A TRNG
 * @param nonce_buf Output buffer, must be at least 32 bytes
 * @return true if nonce generated successfully
 */
bool generateNonce(uint8_t *nonce_buf);
