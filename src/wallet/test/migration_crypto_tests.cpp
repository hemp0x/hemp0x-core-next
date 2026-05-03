// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemp0x.h"
#include "utilstrencodings.h"
#include "wallet/migration_crypto.h"

#include <cstring>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(migration_crypto_tests, BasicTestingSetup)

static const unsigned int TEST_KDF_ITERATIONS = 1000;

BOOST_AUTO_TEST_CASE(kdf_derive_key_properties)
{
    // Verify that deriving a key with known inputs produces a 32-byte non-zero key.
    std::vector<unsigned char> salt(MIGRATION_KDF_SALT_SIZE, 0x00);
    std::string passphrase = "test passphrase";

    std::vector<unsigned char> key = MigrationDeriveKey(passphrase, salt, TEST_KDF_ITERATIONS);

    BOOST_CHECK_EQUAL(key.size(), static_cast<size_t>(MIGRATION_KDF_KEY_SIZE));
    BOOST_CHECK_EQUAL(
        HexStr(key),
        "1452d6ab1ab9e18b0ed6ef449eb6a960ee792557dcb0986b969e1e43ab168301");

    bool allZero = true;
    for (const auto& b : key) {
        if (b != 0) { allZero = false; break; }
    }
    BOOST_CHECK_MESSAGE(!allZero, "Derived key must not be all zeros");

    // Determinism: deriving again with same inputs must produce the same key.
    std::vector<unsigned char> key2 = MigrationDeriveKey(passphrase, salt, TEST_KDF_ITERATIONS);
    BOOST_CHECK_EQUAL(key.size(), key2.size());
    BOOST_CHECK(std::memcmp(key.data(), key2.data(), key.size()) == 0);
}

BOOST_AUTO_TEST_CASE(kdf_zero_iterations)
{
    std::vector<unsigned char> salt(MIGRATION_KDF_SALT_SIZE, 0x00);
    std::vector<unsigned char> key = MigrationDeriveKey("test", salt, 0);
    BOOST_CHECK(key.empty());
}

BOOST_AUTO_TEST_CASE(kdf_wrong_salt_size)
{
    std::vector<unsigned char> badSalt(16, 0x00); // wrong size
    std::vector<unsigned char> key = MigrationDeriveKey("test", badSalt, TEST_KDF_ITERATIONS);
    BOOST_CHECK(key.empty());
}

BOOST_AUTO_TEST_CASE(gcm_encrypt_decrypt_roundtrip)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0x01);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x02);
    std::vector<unsigned char> plaintext = ParseHex("deadbeefcafebabe0123456789abcdef");
    std::vector<unsigned char> aad = ParseHex("aabbccdd");

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));
    BOOST_CHECK_EQUAL(ciphertext.size(), plaintext.size());
    BOOST_CHECK_EQUAL(tag.size(), static_cast<size_t>(MIGRATION_GCM_TAG_SIZE));

    // Ciphertext must not equal plaintext (encryption happened)
    BOOST_CHECK(std::memcmp(plaintext.data(), ciphertext.data(), plaintext.size()) != 0);

    std::vector<unsigned char> decrypted;
    BOOST_CHECK(MigrationDecrypt(key, iv, ciphertext, aad, tag, decrypted));
    BOOST_CHECK_EQUAL(decrypted.size(), plaintext.size());
    BOOST_CHECK(std::memcmp(plaintext.data(), decrypted.data(), plaintext.size()) == 0);
}

BOOST_AUTO_TEST_CASE(gcm_empty_plaintext_roundtrip)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0x03);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x04);
    std::vector<unsigned char> plaintext;
    std::vector<unsigned char> aad;

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));
    BOOST_CHECK(ciphertext.empty());
    BOOST_CHECK_EQUAL(tag.size(), static_cast<size_t>(MIGRATION_GCM_TAG_SIZE));

    std::vector<unsigned char> decrypted;
    BOOST_CHECK(MigrationDecrypt(key, iv, ciphertext, aad, tag, decrypted));
    BOOST_CHECK(decrypted.empty());
}

BOOST_AUTO_TEST_CASE(gcm_wrong_key_fails)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0x10);
    std::vector<unsigned char> wrongKey(MIGRATION_KDF_KEY_SIZE, 0x20);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x30);
    std::vector<unsigned char> plaintext = ParseHex("feedfacec0c0a");
    std::vector<unsigned char> aad;

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));

    std::vector<unsigned char> decrypted;
    // Wrong key must produce a uniform failure.
    bool result = MigrationDecrypt(wrongKey, iv, ciphertext, aad, tag, decrypted);
    BOOST_CHECK_MESSAGE(!result, "Decryption with wrong key must fail");
    BOOST_CHECK(decrypted.empty());
}

BOOST_AUTO_TEST_CASE(gcm_wrong_aad_fails)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0x40);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x50);
    std::vector<unsigned char> plaintext = ParseHex("abcdef0123456789");
    std::vector<unsigned char> aad = ParseHex("aabb");
    std::vector<unsigned char> wrongAad = ParseHex("ccdd");

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));

    std::vector<unsigned char> decrypted;
    bool result = MigrationDecrypt(key, iv, ciphertext, wrongAad, tag, decrypted);
    BOOST_CHECK_MESSAGE(!result, "Decryption with wrong AAD must fail");
    BOOST_CHECK(decrypted.empty());
}

BOOST_AUTO_TEST_CASE(gcm_tampered_ciphertext_fails)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0x60);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x70);
    std::vector<unsigned char> plaintext = ParseHex("0123456789abcdef0123456789abcdef");
    std::vector<unsigned char> aad;

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));
    BOOST_CHECK(!ciphertext.empty());

    // Flip a bit in the ciphertext.
    std::vector<unsigned char> badCiphertext = ciphertext;
    badCiphertext[0] ^= 0x01;

    std::vector<unsigned char> decrypted;
    bool result = MigrationDecrypt(key, iv, badCiphertext, aad, tag, decrypted);
    BOOST_CHECK_MESSAGE(!result, "Decryption of tampered ciphertext must fail");
    BOOST_CHECK(decrypted.empty());
}

BOOST_AUTO_TEST_CASE(gcm_tampered_tag_fails)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0x80);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x90);
    std::vector<unsigned char> plaintext = ParseHex("fedcba9876543210");
    std::vector<unsigned char> aad;

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));

    // Flip a bit in the tag.
    std::vector<unsigned char> badTag = tag;
    badTag[0] ^= 0x01;

    std::vector<unsigned char> decrypted;
    bool result = MigrationDecrypt(key, iv, ciphertext, aad, badTag, decrypted);
    BOOST_CHECK_MESSAGE(!result, "Decryption with tampered tag must fail");
    BOOST_CHECK(decrypted.empty());
}

BOOST_AUTO_TEST_CASE(gcm_wrong_iv_size_fails)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0xa0);
    std::vector<unsigned char> badIv(8, 0xb0); // 8 bytes instead of 12
    std::vector<unsigned char> plaintext = ParseHex("aaaa");

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(!MigrationEncrypt(key, badIv, plaintext, {}, ciphertext, tag));
    BOOST_CHECK(ciphertext.empty());
    BOOST_CHECK(tag.empty());
}

BOOST_AUTO_TEST_CASE(gcm_wrong_key_size_fails)
{
    std::vector<unsigned char> badKey(16, 0xc0); // 16 bytes instead of 32
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0xd0);
    std::vector<unsigned char> plaintext = ParseHex("bbbb");

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(!MigrationEncrypt(badKey, iv, plaintext, {}, ciphertext, tag));
    BOOST_CHECK(ciphertext.empty());
    BOOST_CHECK(tag.empty());
}

BOOST_AUTO_TEST_CASE(gcm_wrong_tag_size_fails)
{
    std::vector<unsigned char> key(MIGRATION_KDF_KEY_SIZE, 0xe0);
    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0xf0);
    std::vector<unsigned char> ciphertext = ParseHex("0123");
    std::vector<unsigned char> badTag(8, 0x11); // 8 bytes instead of 16

    std::vector<unsigned char> plaintext;
    BOOST_CHECK(!MigrationDecrypt(key, iv, ciphertext, {}, badTag, plaintext));
    BOOST_CHECK(plaintext.empty());
}

BOOST_AUTO_TEST_CASE(aad_deterministic_output)
{
    std::string expected = "hemp0x-core.migration-envelope.v2:2:hemp0x_mainnet:420:1714608000:private-payload";
    std::vector<unsigned char> expectedBytes(expected.begin(), expected.end());

    std::vector<unsigned char> aad = MigrationBuildAAD(
        "hemp0x-core.migration-envelope.v2",
        2,
        "hemp0x_mainnet",
        420,
        1714608000,
        "private-payload");

    BOOST_CHECK_EQUAL(aad.size(), expectedBytes.size());
    BOOST_CHECK(std::memcmp(aad.data(), expectedBytes.data(), aad.size()) == 0);
}

BOOST_AUTO_TEST_CASE(aad_different_purpose_label)
{
    std::vector<unsigned char> aadPayload = MigrationBuildAAD(
        "hemp0x-core.migration-envelope.v2", 2, "hemp0x_mainnet", 420, 1, "private-payload");
    std::vector<unsigned char> aadDek = MigrationBuildAAD(
        "hemp0x-core.migration-envelope.v2", 2, "hemp0x_mainnet", 420, 1, "dek-wrapping");

    BOOST_CHECK(aadPayload != aadDek);
}

BOOST_AUTO_TEST_CASE(gcm_full_workflow_with_aad)
{
    // Full workflow: derive key from passphrase → encrypt with AAD → decrypt with AAD.
    std::vector<unsigned char> salt(MIGRATION_KDF_SALT_SIZE, 0x42);
    std::string passphrase = "workflow test passphrase";
    std::vector<unsigned char> key = MigrationDeriveKey(passphrase, salt, TEST_KDF_ITERATIONS);
    BOOST_REQUIRE(!key.empty());

    std::vector<unsigned char> iv(MIGRATION_GCM_IV_SIZE, 0x55);
    std::vector<unsigned char> plaintext = ParseHex("f00dbabe12344321deadbeef");
    std::vector<unsigned char> aad = MigrationBuildAAD(
        "hemp0x-core.migration-envelope.v2", 2, "hemp0x_mainnet", 420, 1, "private-payload");

    std::vector<unsigned char> ciphertext, tag;
    BOOST_CHECK(MigrationEncrypt(key, iv, plaintext, aad, ciphertext, tag));

    std::vector<unsigned char> decrypted;
    BOOST_CHECK(MigrationDecrypt(key, iv, ciphertext, aad, tag, decrypted));
    BOOST_CHECK_EQUAL(decrypted.size(), plaintext.size());
    BOOST_CHECK(std::memcmp(plaintext.data(), decrypted.data(), plaintext.size()) == 0);
}

BOOST_AUTO_TEST_CASE(constants_match_expectations)
{
    BOOST_CHECK_EQUAL(MIGRATION_KDF_SALT_SIZE, 32u);
    BOOST_CHECK_EQUAL(MIGRATION_KDF_KEY_SIZE, 32u);
    BOOST_CHECK_EQUAL(MIGRATION_GCM_IV_SIZE, 12u);
    BOOST_CHECK_EQUAL(MIGRATION_GCM_TAG_SIZE, 16u);
    BOOST_CHECK_EQUAL(MIGRATION_KDF_ITERATIONS, 600000u);
    BOOST_CHECK_EQUAL(std::string(MIGRATION_KDF_PROFILE), "pbkdf2-hmac-sha512-v1");
    BOOST_CHECK_EQUAL(std::string(MIGRATION_CIPHER_PROFILE), "aes-256-gcm-v1");
}

BOOST_AUTO_TEST_SUITE_END()
