// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/migration_envelope.h"

#include "chainparams.h"
#include "support/cleanse.h"
#include "util.h"
#include "utilstrencodings.h"
#include "wallet/migration_crypto.h"

#include <fstream>
#include <stdint.h>

#include <univalue.h>

static const int64_t MIGRATION_VALIDATE_MAX_KDF_ITERATIONS = 5000000;
static const char* const MIGRATION_AUTH_FAILURE =
    "Authentication failed. The passphrase is incorrect or the envelope has been tampered with.";

static void CleansePassphrase(std::string& strPassphrase)
{
    if (!strPassphrase.empty()) {
        memory_cleanse(&strPassphrase[0], strPassphrase.size());
    }
}

static void CleanseBytes(std::vector<unsigned char>& bytes)
{
    if (!bytes.empty()) {
        memory_cleanse(bytes.data(), bytes.size());
    }
}

static bool FindJsonStringValue(std::string& json, const std::string& key, size_t& value_begin, size_t& value_end)
{
    const std::string quoted_key = "\"" + key + "\"";
    size_t pos = json.find(quoted_key);
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find(':', pos + quoted_key.size());
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != '"') {
        return false;
    }
    value_begin = ++pos;
    bool escaped = false;
    while (pos < json.size()) {
        if (escaped) {
            escaped = false;
        } else if (json[pos] == '\\') {
            escaped = true;
        } else if (json[pos] == '"') {
            value_end = pos;
            return true;
        }
        ++pos;
    }
    return false;
}

static int CountMnemonicWordsInJson(std::string& json)
{
    size_t begin = 0;
    size_t end = 0;
    if (!FindJsonStringValue(json, "words", begin, end) || begin == end) {
        return 0;
    }
    int count = 1;
    for (size_t i = begin; i < end; ++i) {
        if (json[i] == ' ') {
            ++count;
        }
    }
    return count;
}

static void SanitizeJsonStringField(std::string& json, const std::string& key)
{
    size_t begin = 0;
    size_t end = 0;
    if (!FindJsonStringValue(json, key, begin, end)) {
        return;
    }
    if (end > begin) {
        memory_cleanse(&json[begin], end - begin);
        json.erase(begin, end - begin);
    }
}

bool ValidateMigrationEnvelopeFile(
    const boost::filesystem::path& path,
    const std::string& passphrase,
    MigrationEnvelopeValidation& out,
    std::string& rpc_error)
{
    out = MigrationEnvelopeValidation();

    if (path.empty()) {
        rpc_error = "Filename cannot be empty";
        return false;
    }

    if (!boost::filesystem::exists(path)) {
        rpc_error = "File not found: " + path.string();
        return false;
    }
    if (boost::filesystem::is_directory(path)) {
        rpc_error = "Path is a directory, not a file: " + path.string();
        return false;
    }

    std::ifstream file(path.string(), std::ios::binary);
    if (!file.is_open()) {
        rpc_error = "Cannot open file: " + path.string();
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    UniValue envelope;
    if (!envelope.read(content)) {
        rpc_error = "Migration envelope file is not valid JSON.";
        return false;
    }
    if (!envelope.isObject()) {
        rpc_error = "Migration envelope must be a JSON object.";
        return false;
    }

    std::string strPassphrase = passphrase;

    bool fValid = true;
    bool fRestorable = false;

    if (!envelope.exists("envelope_version") || !envelope["envelope_version"].isNum()) {
        fValid = false;
        out.errors.push_back("Missing or invalid required field: envelope_version");
        out.restorable_reason = "Invalid or missing envelope_version";
        out.valid = fValid;
        out.restorable = fRestorable;
        out.envelope_version = envelope.exists("envelope_version")
            ? envelope["envelope_version"].get_int() : -1;
        CleansePassphrase(strPassphrase);
        return true;
    }

    int envVersion = envelope["envelope_version"].get_int();
    if (envVersion != 1 && envVersion != 2) {
        fValid = false;
        out.errors.push_back(strprintf(
            "Unknown envelope version %d. This daemon supports v1 and v2.", envVersion));
        out.restorable_reason = strprintf("Unsupported envelope version %d", envVersion);
        out.valid = fValid;
        out.restorable = fRestorable;
        out.envelope_version = envVersion;
        CleansePassphrase(strPassphrase);
        return true;
    }

    out.envelope_version = envVersion;

    std::string schemaId = envelope.exists("schema_identifier") && envelope["schema_identifier"].isStr()
        ? envelope["schema_identifier"].get_str() : "";
    out.schema_identifier = schemaId;

    std::string expectedSchema = envVersion == 1
        ? "hemp0x-core.migration-envelope.v1"
        : "hemp0x-core.migration-envelope.v2";
    if (schemaId.empty()) {
        out.errors.push_back("Missing required field: schema_identifier");
        fValid = false;
    } else if (schemaId != expectedSchema) {
        out.warnings.push_back(strprintf(
            "Schema identifier '%s' does not match expected '%s'. "
            "The envelope may be from a different version or incompatible tool.",
            schemaId, expectedSchema));
    }

    out.source_client_version = envelope.exists("source_client_version") && envelope["source_client_version"].isStr()
        ? envelope["source_client_version"].get_str() : "unknown";

    out.exported_at = envelope.exists("exported_at") && envelope["exported_at"].isNum()
        ? envelope["exported_at"].get_int64() : 0;
    if (out.exported_at <= 0) {
        out.warnings.push_back("Missing or invalid exported_at timestamp");
    }

    if (envelope.exists("chain") && envelope["chain"].isObject()) {
        UniValue chainObj = envelope["chain"];
        if (chainObj.exists("network") && chainObj["network"].isStr()) {
            out.network = chainObj["network"].get_str();
        }
        if (chainObj.exists("coin_type_bip44") && chainObj["coin_type_bip44"].isNum()) {
            out.coin_type_bip44 = chainObj["coin_type_bip44"].get_int();
        }
    }
    if (out.network.empty()) {
        out.errors.push_back("Missing or invalid required field: chain.network");
        fValid = false;
    }
    std::string currentNetwork = GetParams().NetworkIDString();
    out.matches_current_chain = (out.network == currentNetwork);
    if (!out.matches_current_chain) {
        out.warnings.push_back(strprintf(
            "Envelope network '%s' does not match current chain '%s'. "
            "A wallet restored from this envelope would produce different addresses.",
            out.network, currentNetwork));
    }

    if (envelope.exists("wallet_summary") && envelope["wallet_summary"].isObject()) {
        UniValue ws = envelope["wallet_summary"];
        if (ws.exists("private_keys_included") && ws["private_keys_included"].isBool()) {
            out.private_keys_included = ws["private_keys_included"].get_bool();
        }
    }

    if (envVersion == 1) {
        if (envelope.exists("wallet_summary") && envelope["wallet_summary"].isObject()) {
            UniValue ws = envelope["wallet_summary"];
            if (ws.exists("hd_enabled") && ws["hd_enabled"].isBool()) out.hd_enabled = ws["hd_enabled"].get_bool();
            if (ws.exists("bip44_enabled") && ws["bip44_enabled"].isBool()) out.bip44_enabled = ws["bip44_enabled"].get_bool();
            if (ws.exists("mnemonic_available") && ws["mnemonic_available"].isBool()) out.mnemonic_available = ws["mnemonic_available"].get_bool();
        }
        out.wallet_type = "public_metadata_only";

        if (envelope.exists("derivation") && envelope["derivation"].isArray()) {
            for (const UniValue& prof : envelope["derivation"].getValues()) {
                if (!prof.isObject()) continue;
                MigrationDerivationProfileEntry entry;
                if (prof.exists("profile_id") && prof["profile_id"].isStr())
                    entry.profile_id = prof["profile_id"].get_str();
                if (prof.exists("purpose") && prof["purpose"].isNum())
                    entry.purpose = prof["purpose"].get_int();
                if (prof.exists("coin_type") && prof["coin_type"].isNum())
                    entry.coin_type = prof["coin_type"].get_int();
                if (prof.exists("address_type") && prof["address_type"].isStr())
                    entry.address_type = prof["address_type"].get_str();
                out.derivation_profiles.push_back(entry);
            }
        }

        if (fValid) {
            out.warnings.push_back("This is a public-only envelope. No private keys are included.");
            out.warnings.push_back("Cannot restore a wallet from a public-only envelope. "
                "Use include_private=true in exportwalletmigration to produce a restorable envelope.");
        }
        out.restorable_reason = "No private key material in envelope";
    } else {
        out.private_present = envelope.exists("private") && envelope["private"].isObject();

        if (!out.private_present) {
            fValid = false;
            out.errors.push_back(
                "Envelope version 2 requires a 'private' object but none was found. "
                "Re-export with include_private=true on a BIP39/BIP44 coin420 wallet.");
            out.restorable_reason = "Missing private object in envelope";
        } else {
            UniValue priv = envelope["private"];

            if (strPassphrase.empty()) {
                fValid = false;
                out.errors.push_back(
                    "Passphrase is required for v2 encrypted envelopes but was not provided.");
                out.restorable_reason = "Passphrase required for v2 envelope";
            }

            bool fEncrypted = priv.exists("encrypted") && priv["encrypted"].isBool()
                && priv["encrypted"].get_bool();
            out.private_encrypted = fEncrypted;
            if (!fEncrypted) {
                out.errors.push_back("Private payload is not encrypted. Encrypted private payloads are required.");
                fValid = false;
                out.restorable_reason = "Private payload is not encrypted";
            }

            out.kdf_profile = priv.exists("kdf_profile") && priv["kdf_profile"].isStr()
                ? priv["kdf_profile"].get_str() : "";
            if (out.kdf_profile != "pbkdf2-hmac-sha512-v1") {
                out.errors.push_back(strprintf(
                    "Unsupported KDF profile '%s'. Only pbkdf2-hmac-sha512-v1 is supported.", out.kdf_profile));
                fValid = false;
                out.restorable_reason = strprintf("Unsupported KDF profile: %s", out.kdf_profile);
            }

            out.kdf_iterations = priv.exists("kdf_iterations") && priv["kdf_iterations"].isNum()
                ? priv["kdf_iterations"].get_int64() : 0;
            if (out.kdf_iterations < 100000) {
                out.errors.push_back(strprintf(
                    "KDF iteration count %d is below the supported minimum (100,000).", (int)out.kdf_iterations));
                fValid = false;
                out.restorable_reason = "Unsupported KDF iteration count";
            } else if (out.kdf_iterations > MIGRATION_VALIDATE_MAX_KDF_ITERATIONS) {
                out.errors.push_back(strprintf(
                    "KDF iteration count %d exceeds the supported maximum (%d).",
                    (int)out.kdf_iterations, (int)MIGRATION_VALIDATE_MAX_KDF_ITERATIONS));
                fValid = false;
                out.restorable_reason = "Unsupported KDF iteration count";
            }

            out.cipher_profile = priv.exists("cipher_profile") && priv["cipher_profile"].isStr()
                ? priv["cipher_profile"].get_str() : "";
            if (out.cipher_profile != "aes-256-gcm-v1") {
                out.errors.push_back(strprintf(
                    "Unsupported cipher profile '%s'. Only aes-256-gcm-v1 is supported.", out.cipher_profile));
                fValid = false;
                out.restorable_reason = strprintf("Unsupported cipher profile: %s", out.cipher_profile);
            }

            out.payload_format = priv.exists("payload_format") && priv["payload_format"].isStr()
                ? priv["payload_format"].get_str() : "";
            if (out.payload_format != "hemp0x-core.private-migration-payload.v1") {
                out.errors.push_back(strprintf(
                    "Unsupported private payload format '%s'. Only hemp0x-core.private-migration-payload.v1 is supported.",
                    out.payload_format));
                fValid = false;
                out.restorable_reason = strprintf("Unsupported private payload format: %s", out.payload_format);
            }

            out.aad_profile = priv.exists("aad_profile") && priv["aad_profile"].isStr()
                ? priv["aad_profile"].get_str() : "";
            if (out.aad_profile != "hemp0x-core-migration-aad-v1") {
                out.errors.push_back(strprintf(
                    "Unsupported AAD profile '%s'. Only hemp0x-core-migration-aad-v1 is supported.",
                    out.aad_profile));
                fValid = false;
                out.restorable_reason = strprintf("Unsupported AAD profile: %s", out.aad_profile);
            }

            if (!strPassphrase.empty() && fValid &&
                priv.exists("salt") && priv["salt"].isStr() &&
                priv.exists("iv") && priv["iv"].isStr() &&
                priv.exists("tag") && priv["tag"].isStr() &&
                priv.exists("ciphertext") && priv["ciphertext"].isStr()) {

                std::string saltHex = priv["salt"].get_str();
                std::string ivHex = priv["iv"].get_str();
                std::string tagHex = priv["tag"].get_str();
                std::string ciphertextHex = priv["ciphertext"].get_str();

                std::vector<unsigned char> salt = ParseHex(saltHex);
                std::vector<unsigned char> iv = ParseHex(ivHex);
                std::vector<unsigned char> tag = ParseHex(tagHex);
                std::vector<unsigned char> ciphertext = ParseHex(ciphertextHex);

                if (salt.size() != MIGRATION_KDF_SALT_SIZE || iv.size() != MIGRATION_GCM_IV_SIZE ||
                    tag.size() != MIGRATION_GCM_TAG_SIZE || ciphertext.empty()) {
                    out.errors.push_back("Invalid private field lengths. The envelope may be corrupted.");
                    fValid = false;
                    out.restorable_reason = "Invalid private crypto field lengths";
                } else {
                    std::vector<unsigned char> aad = MigrationBuildAAD(
                        schemaId, envVersion, out.network, out.coin_type_bip44,
                        out.exported_at, "private-payload");

                    std::vector<unsigned char> key = MigrationDeriveKey(
                        strPassphrase, salt, (unsigned int)out.kdf_iterations);
                    CleansePassphrase(strPassphrase);
                    if (key.empty()) {
                        out.errors.push_back(MIGRATION_AUTH_FAILURE);
                        fValid = false;
                        out.restorable_reason = "Decryption failed";
                    } else {
                        std::vector<unsigned char> plaintext;
                        bool decrypted = MigrationDecrypt(key, iv, ciphertext, aad, tag, plaintext);
                        CleanseBytes(key);

                        if (!decrypted) {
                            out.decryption_successful = false;
                            out.errors.push_back(MIGRATION_AUTH_FAILURE);
                            fValid = false;
                            out.restorable_reason = "Decryption failed";
                        } else {
                            out.decryption_successful = true;
                            std::string plaintextStr(plaintext.begin(), plaintext.end());
                            CleanseBytes(plaintext);
                            int secretMnemonicWordCount = CountMnemonicWordsInJson(plaintextStr);
                            SanitizeJsonStringField(plaintextStr, "words");
                            SanitizeJsonStringField(plaintextStr, "mnemonic_passphrase");

                            UniValue payload;
                            if (!payload.read(plaintextStr)) {
                                memory_cleanse(&plaintextStr[0], plaintextStr.size());
                                out.errors.push_back("Decrypted private payload is not valid JSON.");
                                fValid = false;
                                out.restorable_reason = "Payload JSON parse failed";
                            } else {
                                memory_cleanse(&plaintextStr[0], plaintextStr.size());

                                int payloadVersion = payload.exists("payload_version") && payload["payload_version"].isNum()
                                    ? payload["payload_version"].get_int() : -1;
                                if (payloadVersion != 1) {
                                    out.errors.push_back(strprintf(
                                        "Unsupported payload version %d. Only version 1 is supported.", payloadVersion));
                                    fValid = false;
                                    out.restorable_reason = strprintf("Unsupported payload version %d", payloadVersion);
                                }

                                out.wallet_type = payload.exists("wallet_type") && payload["wallet_type"].isStr()
                                    ? payload["wallet_type"].get_str() : "";
                                if (out.wallet_type != "bip39_bip44_p2pkh") {
                                    out.errors.push_back(strprintf(
                                        "Unsupported wallet type '%s'. Only bip39_bip44_p2pkh is supported.",
                                        out.wallet_type));
                                    fValid = false;
                                    out.restorable_reason = strprintf("Unsupported wallet type: %s", out.wallet_type);
                                }

                                out.payload_coin_type = payload.exists("coin_type") && payload["coin_type"].isNum()
                                    ? payload["coin_type"].get_int() : -1;
                                if (out.payload_coin_type != 420) {
                                    out.errors.push_back(strprintf(
                                        "Coin type %d is not supported. Only canonical coin type 420 is supported.",
                                        out.payload_coin_type));
                                    fValid = false;
                                    out.restorable_reason = strprintf("Unsupported coin type: %d", out.payload_coin_type);
                                }

                                out.account = payload.exists("account") && payload["account"].isNum()
                                    ? payload["account"].get_int() : -1;
                                if (out.account != 0) {
                                    out.errors.push_back(strprintf(
                                        "Unsupported account %d. Only account 0 is supported.", out.account));
                                    fValid = false;
                                    out.restorable_reason = strprintf("Unsupported account: %d", out.account);
                                }

                                if (payload.exists("mnemonic") && payload["mnemonic"].isObject()) {
                                    UniValue mn = payload["mnemonic"];
                                    if (mn.exists("language") && mn["language"].isStr()) {
                                        out.mnemonic_language = mn["language"].get_str();
                                    }
                                    if (mn.exists("words") && mn["words"].isStr()) {
                                        out.mnemonic_word_count = secretMnemonicWordCount;
                                        if (out.mnemonic_word_count != 12 && out.mnemonic_word_count != 18 && out.mnemonic_word_count != 24) {
                                            out.errors.push_back(strprintf(
                                                "Mnemonic word count %d is not supported. Expected 12, 18, or 24 words.",
                                                out.mnemonic_word_count));
                                            fValid = false;
                                            out.restorable_reason = "Unsupported mnemonic word count";
                                        }
                                    } else {
                                        out.errors.push_back("Missing mnemonic words in private payload.");
                                        fValid = false;
                                        out.restorable_reason = "Missing mnemonic words";
                                    }
                                } else {
                                    out.errors.push_back("Missing mnemonic object in private payload.");
                                    fValid = false;
                                    out.restorable_reason = "Missing mnemonic data";
                                }

                                out.derivation_profile_id =
                                    payload.exists("derivation_profile") && payload["derivation_profile"].isStr()
                                    ? payload["derivation_profile"].get_str() : "";
                                if (out.derivation_profile_id != "hemp0x.mainnet.bip44.p2pkh.coin420.v1") {
                                    out.errors.push_back(strprintf(
                                        "Unsupported derivation profile '%s'. Only canonical Hemp0x coin420 is supported.",
                                        out.derivation_profile_id));
                                    fValid = false;
                                    out.restorable_reason = "Unsupported derivation profile";
                                }

                                out.external_count_hint = payload.exists("external_count_hint") && payload["external_count_hint"].isNum()
                                    ? payload["external_count_hint"].get_int64() : 0;
                                out.change_count_hint = payload.exists("change_count_hint") && payload["change_count_hint"].isNum()
                                    ? payload["change_count_hint"].get_int64() : 0;

                                if (fValid && out.matches_current_chain) {
                                    fRestorable = true;
                                    out.restorable_reason = "BIP39/BIP44 coin420 payload validated";
                                } else if (fValid && !out.matches_current_chain) {
                                    out.restorable_reason = "Chain mismatch";
                                }
                            }
                        }
                    }
                }
            } else if (!strPassphrase.empty() && fValid) {
                out.errors.push_back("Missing or invalid private encryption fields.");
                fValid = false;
                out.restorable_reason = "Missing private encryption fields";
            } else if (!strPassphrase.empty()) {
                CleansePassphrase(strPassphrase);
            }
        }
    }

    CleansePassphrase(strPassphrase);
    out.valid = fValid;
    out.restorable = fRestorable;

    return true;
}

static SecureString ExtractMnemonicWordsFromPayload(std::string& json)
{
    size_t begin = 0;
    size_t end = 0;
    if (!FindJsonStringValue(json, "words", begin, end) || begin == end) {
        return SecureString();
    }
    SecureString result(json.begin() + begin, json.begin() + end);
    return result;
}

static SecureString ExtractMnemonicPassphraseFromPayload(std::string& json)
{
    size_t begin = 0;
    size_t end = 0;
    if (!FindJsonStringValue(json, "mnemonic_passphrase", begin, end)) {
        return SecureString();
    }
    if (begin == end) {
        return SecureString();
    }
    SecureString result(json.begin() + begin, json.begin() + end);
    return result;
}

static int64_t ExtractInt64FromPayload(std::string& json, const std::string& key)
{
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) {
        return 0;
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return 0;
    }
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
    if (pos >= json.size() || !isdigit(json[pos])) {
        return 0;
    }
    int64_t result = 0;
    while (pos < json.size() && isdigit(json[pos])) {
        result = result * 10 + (json[pos] - '0');
        ++pos;
    }
    return result;
}

static int64_t ExtractBestBlockHeightFromEnvelope(const UniValue& envelope)
{
    if (!envelope.exists("derivation") || !envelope["derivation"].isArray()) {
        return 0;
    }

    for (const UniValue& prof : envelope["derivation"].getValues()) {
        if (!prof.isObject()) {
            continue;
        }
        if (prof.exists("best_block_height") && prof["best_block_height"].isNum()) {
            return prof["best_block_height"].get_int64();
        }
    }

    return 0;
}

bool ReadMigrationEnvelopeRestoreData(
    const boost::filesystem::path& path,
    const std::string& passphrase,
    MigrationEnvelopeRestoreData& out,
    std::string& rpc_error)
{
    out = MigrationEnvelopeRestoreData();
    out.best_block_height = 0;
    out.exported_at = 0;

    if (path.empty()) {
        rpc_error = "Filename cannot be empty";
        return false;
    }

    std::string strPassphrase = passphrase;

    MigrationEnvelopeValidation result;
    std::string validate_rpc_error;
    bool ok = ValidateMigrationEnvelopeFile(path, strPassphrase, result, validate_rpc_error);
    CleansePassphrase(strPassphrase);
    if (!ok) {
        rpc_error = validate_rpc_error;
        return false;
    }

    out.validation = result;

    if (!result.valid) {
        rpc_error = "Migration envelope is not valid.";
        return false;
    }

    if (result.envelope_version == 1) {
        rpc_error = "Cannot restore a wallet from a v1 public-only envelope. Use include_private=true in exportwalletmigration to produce a restorable envelope.";
        return false;
    }

    if (!result.restorable) {
        rpc_error = "Migration envelope is not restorable: " + result.restorable_reason;
        return false;
    }

    if (!result.decryption_successful) {
        rpc_error = "Migration envelope failed to decrypt internal payload. The passphrase may be incorrect or the envelope may be tampered with.";
        return false;
    }

    if (result.payload_coin_type != 420) {
        rpc_error = strprintf("Cannot restore coin type %d. Only canonical coin type 420 is supported.", result.payload_coin_type);
        return false;
    }

    if (result.account != 0) {
        rpc_error = strprintf("Cannot restore account %d. Only account 0 is supported.", result.account);
        return false;
    }

    if (path.empty()) {
        rpc_error = "Filename cannot be empty";
        return false;
    }

    if (!boost::filesystem::exists(path)) {
        rpc_error = "File not found: " + path.string();
        return false;
    }

    std::ifstream file(path.string(), std::ios::binary);
    if (!file.is_open()) {
        rpc_error = "Cannot open file: " + path.string();
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    UniValue envelope;
    if (!envelope.read(content)) {
        rpc_error = "Migration envelope file is not valid JSON.";
        return false;
    }

    strPassphrase = passphrase;

    int envVersion = envelope["envelope_version"].get_int();
    std::string schemaId = envelope["schema_identifier"].get_str();

    out.exported_at = envelope.exists("exported_at") && envelope["exported_at"].isNum()
        ? envelope["exported_at"].get_int64() : 0;

    UniValue priv = envelope["private"];

    std::string saltHex = priv["salt"].get_str();
    std::string ivHex = priv["iv"].get_str();
    std::string tagHex = priv["tag"].get_str();
    std::string ciphertextHex = priv["ciphertext"].get_str();

    std::vector<unsigned char> salt = ParseHex(saltHex);
    std::vector<unsigned char> iv = ParseHex(ivHex);
    std::vector<unsigned char> tag = ParseHex(tagHex);
    std::vector<unsigned char> ciphertext = ParseHex(ciphertextHex);

    std::string network = envelope["chain"]["network"].get_str();
    int coinType = envelope["chain"]["coin_type_bip44"].get_int();
    int64_t exportedAt = out.exported_at;

    std::vector<unsigned char> aad = MigrationBuildAAD(
        schemaId, envVersion, network, coinType, exportedAt, "private-payload");

    int64_t kdfIterations = priv["kdf_iterations"].get_int64();

    std::vector<unsigned char> key = MigrationDeriveKey(
        strPassphrase, salt, (unsigned int)kdfIterations);
    CleansePassphrase(strPassphrase);
    if (key.empty()) {
        rpc_error = "Authentication failed. The passphrase is incorrect or the envelope has been tampered with.";
        return false;
    }

    std::vector<unsigned char> plaintext;
    bool decrypted = MigrationDecrypt(key, iv, ciphertext, aad, tag, plaintext);
    CleanseBytes(key);

    if (!decrypted) {
        rpc_error = "Authentication failed. The passphrase is incorrect or the envelope has been tampered with.";
        return false;
    }

    std::string plaintextStr(plaintext.begin(), plaintext.end());
    CleanseBytes(plaintext);

    SecureString ssWords = ExtractMnemonicWordsFromPayload(plaintextStr);
    SecureString ssPassphrase = ExtractMnemonicPassphraseFromPayload(plaintextStr);
    int64_t bestBlockHeight = ExtractBestBlockHeightFromEnvelope(envelope);
    if (bestBlockHeight == 0) {
        bestBlockHeight = ExtractInt64FromPayload(plaintextStr, "best_block_height");
    }

    if (!plaintextStr.empty()) {
        memory_cleanse(&plaintextStr[0], plaintextStr.size());
    }

    if (ssWords.empty()) {
        rpc_error = "Failed to extract mnemonic from envelope payload.";
        return false;
    }

    out.mnemonic_words = ssWords;
    out.mnemonic_passphrase = ssPassphrase;
    out.best_block_height = bestBlockHeight;

    return true;
}
