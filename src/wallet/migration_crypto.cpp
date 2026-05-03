// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/migration_crypto.h"

#include <openssl/evp.h>

#include <string>
#include <vector>

std::vector<unsigned char> MigrationDeriveKey(
    const std::string& passphrase,
    const std::vector<unsigned char>& salt,
    unsigned int iterations)
{
    if (salt.size() != MIGRATION_KDF_SALT_SIZE || iterations == 0) {
        return {};
    }
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE);
    if (PKCS5_PBKDF2_HMAC(
            passphrase.c_str(), static_cast<int>(passphrase.size()),
            salt.data(), static_cast<int>(salt.size()),
            static_cast<int>(iterations),
            EVP_sha512(),
            MIGRATION_KDF_KEY_SIZE, key.data()) != 1) {
        return {};
    }
    return key;
}

bool MigrationEncrypt(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& aad,
    std::vector<unsigned char>& ciphertext,
    std::vector<unsigned char>& tag)
{
    ciphertext.clear();
    tag.clear();

    if (key.size() != MIGRATION_KDF_KEY_SIZE || iv.size() != MIGRATION_GCM_IV_SIZE) {
        return false;
    }

    ciphertext.resize(plaintext.size());
    tag.resize(MIGRATION_GCM_TAG_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        ciphertext.clear();
        tag.clear();
        return false;
    }

    int success = 0;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, MIGRATION_GCM_IV_SIZE, nullptr) != 1)
            break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1)
            break;

        int outlen = 0;
        if (!aad.empty()) {
            if (EVP_EncryptUpdate(ctx, nullptr, &outlen, aad.data(), static_cast<int>(aad.size())) != 1)
                break;
        }

        int total = 0;
        if (!plaintext.empty()) {
            if (EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen,
                    plaintext.data(), static_cast<int>(plaintext.size())) != 1)
                break;
            total += outlen;
        }

        int tmplen = 0;
        unsigned char final_dummy = 0;
        unsigned char* final_out = ciphertext.empty() ? &final_dummy : ciphertext.data() + total;
        if (EVP_EncryptFinal_ex(ctx, final_out, &tmplen) != 1)
            break;
        total += tmplen;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, MIGRATION_GCM_TAG_SIZE, tag.data()) != 1)
            break;

        ciphertext.resize(total);
        success = 1;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        ciphertext.clear();
        tag.clear();
        return false;
    }
    return true;
}

bool MigrationDecrypt(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& aad,
    const std::vector<unsigned char>& tag,
    std::vector<unsigned char>& plaintext)
{
    plaintext.clear();

    if (key.size() != MIGRATION_KDF_KEY_SIZE ||
        iv.size() != MIGRATION_GCM_IV_SIZE ||
        tag.size() != MIGRATION_GCM_TAG_SIZE) {
        return false;
    }

    plaintext.resize(ciphertext.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        plaintext.clear();
        return false;
    }

    int success = 0;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, MIGRATION_GCM_IV_SIZE, nullptr) != 1)
            break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1)
            break;

        int outlen = 0;
        if (!aad.empty()) {
            if (EVP_DecryptUpdate(ctx, nullptr, &outlen, aad.data(), static_cast<int>(aad.size())) != 1)
                break;
        }

        int total = 0;
        if (!ciphertext.empty()) {
            if (EVP_DecryptUpdate(ctx, plaintext.data(), &outlen,
                    ciphertext.data(), static_cast<int>(ciphertext.size())) != 1)
                break;
            total += outlen;
        }

        int tmplen = 0;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, MIGRATION_GCM_TAG_SIZE,
                const_cast<void*>(static_cast<const void*>(tag.data()))) != 1)
            break;

        unsigned char final_dummy = 0;
        unsigned char* final_out = plaintext.empty() ? &final_dummy : plaintext.data() + total;
        if (EVP_DecryptFinal_ex(ctx, final_out, &tmplen) != 1)
            break;
        total += tmplen;

        plaintext.resize(total);
        success = 1;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        plaintext.clear();
        return false;
    }
    return true;
}

std::vector<unsigned char> MigrationBuildAAD(
    const std::string& schema_identifier,
    int envelope_version,
    const std::string& network,
    int coin_type,
    int64_t exported_at,
    const std::string& purpose_label)
{
    std::string aad = schema_identifier + ":" +
        std::to_string(envelope_version) + ":" +
        network + ":" +
        std::to_string(coin_type) + ":" +
        std::to_string(exported_at) + ":" +
        purpose_label;
    return std::vector<unsigned char>(aad.begin(), aad.end());
}
