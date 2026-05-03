// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HEMP0X_WALLET_MIGRATION_CRYPTO_H
#define HEMP0X_WALLET_MIGRATION_CRYPTO_H

#include <stdint.h>

#include <string>
#include <vector>

static const unsigned int MIGRATION_KDF_SALT_SIZE = 32;
static const unsigned int MIGRATION_KDF_KEY_SIZE = 32;
static const unsigned int MIGRATION_GCM_IV_SIZE = 12;
static const unsigned int MIGRATION_GCM_TAG_SIZE = 16;
static const unsigned int MIGRATION_KDF_ITERATIONS = 600000;

static const char* const MIGRATION_KDF_PROFILE = "pbkdf2-hmac-sha512-v1";
static const char* const MIGRATION_CIPHER_PROFILE = "aes-256-gcm-v1";

/**
 * Derive a 32-byte key from a passphrase using PBKDF2-HMAC-SHA512.
 *
 * Returns the derived key on success, empty vector on failure.
 */
std::vector<unsigned char> MigrationDeriveKey(
    const std::string& passphrase,
    const std::vector<unsigned char>& salt,
    unsigned int iterations);

/**
 * Encrypt plaintext with AES-256-GCM.
 *
 * @param key   32-byte encryption key
 * @param iv    12-byte IV/nonce (must be unique per key,iv pair)
 * @param plaintext  data to encrypt
 * @param aad   additional authenticated data (may be empty)
 * @param[out] ciphertext  encrypted output (same length as plaintext)
 * @param[out] tag  16-byte GCM authentication tag
 * @return true on success, false on failure (outputs cleared)
 */
bool MigrationEncrypt(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& aad,
    std::vector<unsigned char>& ciphertext,
    std::vector<unsigned char>& tag);

/**
 * Decrypt ciphertext with AES-256-GCM, verifying the authentication tag.
 *
 * @return true if decryption and tag verification succeed.
 *         false if tag or ciphertext is invalid (plaintext cleared).
 */
bool MigrationDecrypt(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& aad,
    const std::vector<unsigned char>& tag,
    std::vector<unsigned char>& plaintext);

/**
 * Build an AAD (additional authenticated data) byte vector for Core
 * migration envelope encryption contexts.
 *
 * Fields are concatenated deterministically with ':' as delimiter.
 * The purpose_label distinguishes payload encryption from DEK wrapping.
 */
std::vector<unsigned char> MigrationBuildAAD(
    const std::string& schema_identifier,
    int envelope_version,
    const std::string& network,
    int coin_type,
    int64_t exported_at,
    const std::string& purpose_label);

#endif // HEMP0X_WALLET_MIGRATION_CRYPTO_H
