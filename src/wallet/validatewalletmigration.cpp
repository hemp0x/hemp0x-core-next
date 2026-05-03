// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet.h"

#include "rpcwallet.h"
#include "wallet/migration_crypto.h"
#include "support/cleanse.h"
#include "utilstrencodings.h"

#include <fstream>
#include <stdint.h>

#include <univalue.h>

static const int64_t MIGRATION_VALIDATE_MAX_KDF_ITERATIONS = 5000000;
static const char* const MIGRATION_AUTH_FAILURE =
    "Authentication failed. The passphrase is incorrect or the envelope has been tampered with.";

UniValue validatewalletmigration(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "validatewalletmigration \"filename\" ( \"passphrase\" )\n"
            "\nValidates a Hemp0x Core wallet migration envelope without creating or modifying wallets.\n"
            "\nSupports v1 (public-only) and v2 (encrypted private) envelopes produced by exportwalletmigration.\n"
            "For v1 envelopes: validates public metadata, no passphrase required.\n"
            "For v2 envelopes: requires the export passphrase to decrypt and validate the private payload.\n"
            "\nArguments:\n"
            "1. \"filename\"          (string, required) Path to a migration envelope file (v1 or v2)\n"
            "2. \"passphrase\"        (string, optional) Required for v2 encrypted envelopes. Ignored for v1.\n"
            "\nResult: (metadata only, NO secrets)\n"
            "{\n"
            "  \"valid\" : true|false,             (boolean)\n"
            "  \"envelope_version\" : n,            (numeric)\n"
            "  \"schema_identifier\" : \"...\",      (string)\n"
            "  \"source_client_version\" : \"...\",  (string)\n"
            "  \"exported_at\" : n,                 (numeric)\n"
            "  \"chain\" : {                        (object)\n"
            "    \"network\" : \"...\",              (string)\n"
            "    \"coin_type_bip44\" : n,           (numeric)\n"
            "    \"matches_current_chain\" : bool   (boolean)\n"
            "  },\n"
            "  \"private_keys_included\" : bool,    (boolean)\n"
            "  \"private\" : {                      (object)\n"
            "    \"present\" : bool,                (boolean)\n"
            "    \"encrypted\" : bool,              (boolean)\n"
            "    \"kdf_profile\" : \"...\",           (string)\n"
            "    \"kdf_iterations\" : n,            (numeric)\n"
            "    \"cipher_profile\" : \"...\",        (string)\n"
            "    \"payload_format\" : \"...\",        (string)\n"
            "    \"decryption_successful\" : bool   (boolean)\n"
            "  },\n"
            "  \"wallet_type\" : \"...\",             (string)\n"
            "  \"coin_type\" : n,                   (numeric)\n"
            "  \"account\" : n,                     (numeric)\n"
            "  \"mnemonic_language\" : \"...\",       (string)\n"
            "  \"mnemonic_word_count\" : n,         (numeric)\n"
            "  \"derivation_profile_id\" : \"...\",   (string)\n"
            "  \"external_count_hint\" : n,         (numeric)\n"
            "  \"change_count_hint\" : n,           (numeric)\n"
            "  \"restorable\" : bool,               (boolean)\n"
            "  \"restorable_reason\" : \"...\",       (string)\n"
            "  \"errors\" : [...],                  (array)\n"
            "  \"warnings\" : [...]                 (array)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validatewalletmigration", "\"/tmp/migration.json\"")
            + HelpExampleCli("validatewalletmigration", "\"/tmp/migration.json\" \"my export passphrase\"")
            + HelpExampleRpc("validatewalletmigration", "\"/tmp/migration.json\"")
        );

    ObserveSafeMode();

    std::string strFilename = request.params[0].get_str();
    std::string strPassphrase;
    if (request.params.size() >= 2 && !request.params[1].isNull()) {
        strPassphrase = request.params[1].get_str();
    }

    auto CleansePassphrase = [&strPassphrase]() {
        if (!strPassphrase.empty()) {
            memory_cleanse(&strPassphrase[0], strPassphrase.size());
        }
    };

    auto CleanseBytes = [](std::vector<unsigned char>& bytes) {
        if (!bytes.empty()) {
            memory_cleanse(bytes.data(), bytes.size());
        }
    };

    if (strFilename.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Filename cannot be empty");
    }

    boost::filesystem::path filepath = strFilename;
    filepath = boost::filesystem::absolute(filepath);

    if (!boost::filesystem::exists(filepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "File not found: " + filepath.string());
    }
    if (boost::filesystem::is_directory(filepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Path is a directory, not a file: " + filepath.string());
    }

    std::ifstream file(filepath.string(), std::ios::binary);
    if (!file.is_open()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Cannot open file: " + filepath.string());
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    UniValue envelope;
    if (!envelope.read(content)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Migration envelope file is not valid JSON.");
    }
    if (!envelope.isObject()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Migration envelope must be a JSON object.");
    }

    UniValue reply(UniValue::VOBJ);
    UniValue errors(UniValue::VARR);
    UniValue warnings(UniValue::VARR);
    bool fValid = true;
    bool fRestorable = false;
    std::string restorableReason;

    if (!envelope.exists("envelope_version") || !envelope["envelope_version"].isNum()) {
        fValid = false;
        errors.push_back("Missing or invalid required field: envelope_version");
        reply.pushKV("valid", UniValue(fValid));
        reply.pushKV("envelope_version", envelope.exists("envelope_version")
            ? envelope["envelope_version"].get_int() : -1);
        reply.pushKV("errors", errors);
        reply.pushKV("warnings", warnings);
        reply.pushKV("restorable", UniValue(false));
        reply.pushKV("restorable_reason", "Invalid or missing envelope_version");
        return reply;
    }

    int envVersion = envelope["envelope_version"].get_int();
    if (envVersion != 1 && envVersion != 2) {
        fValid = false;
        errors.push_back(strprintf(
            "Unknown envelope version %d. This daemon supports v1 and v2.", envVersion));
        reply.pushKV("valid", UniValue(fValid));
        reply.pushKV("envelope_version", envVersion);
        reply.pushKV("errors", errors);
        reply.pushKV("warnings", warnings);
        reply.pushKV("restorable", UniValue(false));
        reply.pushKV("restorable_reason", strprintf("Unsupported envelope version %d", envVersion));
        return reply;
    }

    reply.pushKV("envelope_version", envVersion);

    std::string schemaId = envelope.exists("schema_identifier") && envelope["schema_identifier"].isStr()
        ? envelope["schema_identifier"].get_str() : "";
    reply.pushKV("schema_identifier", schemaId);

    std::string expectedSchema = envVersion == 1
        ? "hemp0x-core.migration-envelope.v1"
        : "hemp0x-core.migration-envelope.v2";
    if (schemaId.empty()) {
        errors.push_back("Missing required field: schema_identifier");
        fValid = false;
    } else if (schemaId != expectedSchema) {
        warnings.push_back(strprintf(
            "Schema identifier '%s' does not match expected '%s'. "
            "The envelope may be from a different version or incompatible tool.",
            schemaId, expectedSchema));
    }

    std::string sourceVersion = envelope.exists("source_client_version") && envelope["source_client_version"].isStr()
        ? envelope["source_client_version"].get_str() : "unknown";
    reply.pushKV("source_client_version", sourceVersion);

    int64_t exportedAt = envelope.exists("exported_at") && envelope["exported_at"].isNum()
        ? envelope["exported_at"].get_int64() : 0;
    reply.pushKV("exported_at", exportedAt);
    if (exportedAt <= 0) {
        warnings.push_back("Missing or invalid exported_at timestamp");
    }

    UniValue chainResult(UniValue::VOBJ);
    std::string envelopeNetwork = "";
    int envelopeCoinType = 0;
    if (envelope.exists("chain") && envelope["chain"].isObject()) {
        UniValue chainObj = envelope["chain"];
        if (chainObj.exists("network") && chainObj["network"].isStr()) {
            envelopeNetwork = chainObj["network"].get_str();
        }
        if (chainObj.exists("coin_type_bip44") && chainObj["coin_type_bip44"].isNum()) {
            envelopeCoinType = chainObj["coin_type_bip44"].get_int();
        }
    }
    if (envelopeNetwork.empty()) {
        errors.push_back("Missing or invalid required field: chain.network");
        fValid = false;
    }
    chainResult.pushKV("network", envelopeNetwork);
    chainResult.pushKV("coin_type_bip44", envelopeCoinType);
    std::string currentNetwork = GetParams().NetworkIDString();
    bool chainMatches = (envelopeNetwork == currentNetwork);
    chainResult.pushKV("matches_current_chain", UniValue(chainMatches));
    reply.pushKV("chain", chainResult);

    if (!chainMatches) {
        warnings.push_back(strprintf(
            "Envelope network '%s' does not match current chain '%s'. "
            "A wallet restored from this envelope would produce different addresses.",
            envelopeNetwork, currentNetwork));
    }

    bool fPrivateKeysIncluded = false;
    if (envelope.exists("wallet_summary") && envelope["wallet_summary"].isObject()) {
        UniValue ws = envelope["wallet_summary"];
        if (ws.exists("private_keys_included") && ws["private_keys_included"].isBool()) {
            fPrivateKeysIncluded = ws["private_keys_included"].get_bool();
        }
    }
    reply.pushKV("private_keys_included", UniValue(fPrivateKeysIncluded));

    if (envVersion == 1) {
        bool fHD = false;
        bool fBip44 = false;
        bool fMnemonic = false;
        if (envelope.exists("wallet_summary") && envelope["wallet_summary"].isObject()) {
            UniValue ws = envelope["wallet_summary"];
            if (ws.exists("hd_enabled") && ws["hd_enabled"].isBool()) fHD = ws["hd_enabled"].get_bool();
            if (ws.exists("bip44_enabled") && ws["bip44_enabled"].isBool()) fBip44 = ws["bip44_enabled"].get_bool();
            if (ws.exists("mnemonic_available") && ws["mnemonic_available"].isBool()) fMnemonic = ws["mnemonic_available"].get_bool();
        }
        reply.pushKV("wallet_type", "public_metadata_only");
        reply.pushKV("hd_enabled", UniValue(fHD));
        reply.pushKV("bip44_enabled", UniValue(fBip44));
        reply.pushKV("mnemonic_available", UniValue(fMnemonic));

        UniValue derivationProfiles(UniValue::VARR);
        if (envelope.exists("derivation") && envelope["derivation"].isArray()) {
            for (const UniValue& prof : envelope["derivation"].getValues()) {
                if (!prof.isObject()) continue;
                UniValue profileEntry(UniValue::VOBJ);
                if (prof.exists("profile_id")) profileEntry.pushKV("profile_id", prof["profile_id"]);
                if (prof.exists("purpose")) profileEntry.pushKV("purpose", prof["purpose"]);
                if (prof.exists("coin_type")) profileEntry.pushKV("coin_type", prof["coin_type"]);
                if (prof.exists("address_type")) profileEntry.pushKV("address_type", prof["address_type"]);
                derivationProfiles.push_back(profileEntry);
            }
        }
        reply.pushKV("derivation_profiles", derivationProfiles);

        if (fValid) {
            warnings.push_back("This is a public-only envelope. No private keys are included.");
            warnings.push_back("Cannot restore a wallet from a public-only envelope. "
                "Use include_private=true in exportwalletmigration to produce a restorable envelope.");
        }
        restorableReason = "No private key material in envelope";
    } else {
        bool fPrivatePresent = envelope.exists("private") && envelope["private"].isObject();
        UniValue privateResult(UniValue::VOBJ);
        privateResult.pushKV("present", UniValue(fPrivatePresent));
        privateResult.pushKV("encrypted", UniValue(false));
        privateResult.pushKV("decryption_successful", UniValue(false));

        if (!fPrivatePresent) {
            fValid = false;
            errors.push_back(
                "Envelope version 2 requires a 'private' object but none was found. "
                "Re-export with include_private=true on a BIP39/BIP44 coin420 wallet.");
            privateResult.pushKV("kdf_profile", NullUniValue);
            privateResult.pushKV("kdf_iterations", NullUniValue);
            privateResult.pushKV("cipher_profile", NullUniValue);
            privateResult.pushKV("payload_format", NullUniValue);
            restorableReason = "Missing private object in envelope";
        } else {
            UniValue priv = envelope["private"];

            if (strPassphrase.empty()) {
                fValid = false;
                errors.push_back(
                    "Passphrase is required for v2 encrypted envelopes but was not provided.");
                restorableReason = "Passphrase required for v2 envelope";
            }

            bool fEncrypted = priv.exists("encrypted") && priv["encrypted"].isBool()
                && priv["encrypted"].get_bool();
            privateResult.pushKV("encrypted", UniValue(fEncrypted));
            if (!fEncrypted) {
                errors.push_back("Private payload is not encrypted. Encrypted private payloads are required.");
                fValid = false;
                restorableReason = "Private payload is not encrypted";
            }

            std::string kdfProfile = priv.exists("kdf_profile") && priv["kdf_profile"].isStr()
                ? priv["kdf_profile"].get_str() : "";
            privateResult.pushKV("kdf_profile", kdfProfile);
            if (kdfProfile != "pbkdf2-hmac-sha512-v1") {
                errors.push_back(strprintf(
                    "Unsupported KDF profile '%s'. Only pbkdf2-hmac-sha512-v1 is supported.", kdfProfile));
                fValid = false;
                restorableReason = strprintf("Unsupported KDF profile: %s", kdfProfile);
            }

            int64_t kdfIterations = priv.exists("kdf_iterations") && priv["kdf_iterations"].isNum()
                ? priv["kdf_iterations"].get_int64() : 0;
            privateResult.pushKV("kdf_iterations", kdfIterations);
            if (kdfIterations < 100000) {
                errors.push_back(strprintf(
                    "KDF iteration count %d is below the supported minimum (100,000).", (int)kdfIterations));
                fValid = false;
                restorableReason = "Unsupported KDF iteration count";
            } else if (kdfIterations > MIGRATION_VALIDATE_MAX_KDF_ITERATIONS) {
                errors.push_back(strprintf(
                    "KDF iteration count %d exceeds the supported maximum (%d).",
                    (int)kdfIterations, (int)MIGRATION_VALIDATE_MAX_KDF_ITERATIONS));
                fValid = false;
                restorableReason = "Unsupported KDF iteration count";
            }

            std::string cipherProfile = priv.exists("cipher_profile") && priv["cipher_profile"].isStr()
                ? priv["cipher_profile"].get_str() : "";
            privateResult.pushKV("cipher_profile", cipherProfile);
            if (cipherProfile != "aes-256-gcm-v1") {
                errors.push_back(strprintf(
                    "Unsupported cipher profile '%s'. Only aes-256-gcm-v1 is supported.", cipherProfile));
                fValid = false;
                restorableReason = strprintf("Unsupported cipher profile: %s", cipherProfile);
            }

            std::string payloadFormat = priv.exists("payload_format") && priv["payload_format"].isStr()
                ? priv["payload_format"].get_str() : "";
            privateResult.pushKV("payload_format", payloadFormat);
            if (payloadFormat != "hemp0x-core.private-migration-payload.v1") {
                errors.push_back(strprintf(
                    "Unsupported private payload format '%s'. Only hemp0x-core.private-migration-payload.v1 is supported.",
                    payloadFormat));
                fValid = false;
                restorableReason = strprintf("Unsupported private payload format: %s", payloadFormat);
            }

            std::string aadProfile = priv.exists("aad_profile") && priv["aad_profile"].isStr()
                ? priv["aad_profile"].get_str() : "";
            privateResult.pushKV("aad_profile", aadProfile);
            if (aadProfile != "hemp0x-core-migration-aad-v1") {
                errors.push_back(strprintf(
                    "Unsupported AAD profile '%s'. Only hemp0x-core-migration-aad-v1 is supported.",
                    aadProfile));
                fValid = false;
                restorableReason = strprintf("Unsupported AAD profile: %s", aadProfile);
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
                    errors.push_back("Invalid private field lengths. The envelope may be corrupted.");
                    fValid = false;
                    restorableReason = "Invalid private crypto field lengths";
                } else {
                    std::vector<unsigned char> aad = MigrationBuildAAD(
                        schemaId, envVersion, envelopeNetwork, envelopeCoinType,
                        exportedAt, "private-payload");

                    std::vector<unsigned char> key = MigrationDeriveKey(
                        strPassphrase, salt, (unsigned int)kdfIterations);
                    CleansePassphrase();
                    if (key.empty()) {
                        errors.push_back(MIGRATION_AUTH_FAILURE);
                        fValid = false;
                        restorableReason = "Decryption failed";
                    } else {
                        std::vector<unsigned char> plaintext;
                        bool decrypted = MigrationDecrypt(key, iv, ciphertext, aad, tag, plaintext);
                        CleanseBytes(key);

                        if (!decrypted) {
                            privateResult.pushKV("decryption_successful", UniValue(false));
                            errors.push_back(MIGRATION_AUTH_FAILURE);
                            fValid = false;
                            restorableReason = "Decryption failed";
                        } else {
                            privateResult.pushKV("decryption_successful", UniValue(true));
                            std::string plaintextStr(plaintext.begin(), plaintext.end());
                            CleanseBytes(plaintext);

                            UniValue payload;
                            if (!payload.read(plaintextStr)) {
                                memory_cleanse(&plaintextStr[0], plaintextStr.size());
                                errors.push_back("Decrypted private payload is not valid JSON.");
                                fValid = false;
                                restorableReason = "Payload JSON parse failed";
                            } else {
                                memory_cleanse(&plaintextStr[0], plaintextStr.size());

                                int payloadVersion = payload.exists("payload_version") && payload["payload_version"].isNum()
                                    ? payload["payload_version"].get_int() : -1;
                                if (payloadVersion != 1) {
                                    errors.push_back(strprintf(
                                        "Unsupported payload version %d. Only version 1 is supported.", payloadVersion));
                                    fValid = false;
                                    restorableReason = strprintf("Unsupported payload version %d", payloadVersion);
                                }

                                std::string walletType = payload.exists("wallet_type") && payload["wallet_type"].isStr()
                                    ? payload["wallet_type"].get_str() : "";
                                reply.pushKV("wallet_type", walletType);
                                if (walletType != "bip39_bip44_p2pkh") {
                                    errors.push_back(strprintf(
                                        "Unsupported wallet type '%s'. Only bip39_bip44_p2pkh is supported.",
                                        walletType));
                                    fValid = false;
                                    restorableReason = strprintf("Unsupported wallet type: %s", walletType);
                                }

                                int coinType = payload.exists("coin_type") && payload["coin_type"].isNum()
                                    ? payload["coin_type"].get_int() : -1;
                                reply.pushKV("coin_type", coinType);
                                if (coinType != 420) {
                                    errors.push_back(strprintf(
                                        "Coin type %d is not supported. Only canonical coin type 420 is supported.",
                                        coinType));
                                    fValid = false;
                                    restorableReason = strprintf("Unsupported coin type: %d", coinType);
                                }

                                int account = payload.exists("account") && payload["account"].isNum()
                                    ? payload["account"].get_int() : -1;
                                reply.pushKV("account", account);
                                if (account != 0) {
                                    errors.push_back(strprintf(
                                        "Unsupported account %d. Only account 0 is supported.", account));
                                    fValid = false;
                                    restorableReason = strprintf("Unsupported account: %d", account);
                                }

                                std::string mnemonicLanguage = "";
                                int mnemonicWordCount = 0;
                                if (payload.exists("mnemonic") && payload["mnemonic"].isObject()) {
                                    UniValue mn = payload["mnemonic"];
                                    if (mn.exists("language") && mn["language"].isStr()) {
                                        mnemonicLanguage = mn["language"].get_str();
                                    }
                                    if (mn.exists("words") && mn["words"].isStr()) {
                                        std::string words = mn["words"].get_str();
                                        int spaces = 0;
                                        for (char c : words) { if (c == ' ') spaces++; }
                                        mnemonicWordCount = words.empty() ? 0 : spaces + 1;
                                        if (mnemonicWordCount != 12 && mnemonicWordCount != 18 && mnemonicWordCount != 24) {
                                            errors.push_back(strprintf(
                                                "Mnemonic word count %d is not supported. Expected 12, 18, or 24 words.",
                                                mnemonicWordCount));
                                            fValid = false;
                                            restorableReason = "Unsupported mnemonic word count";
                                        }
                                    } else {
                                        errors.push_back("Missing mnemonic words in private payload.");
                                        fValid = false;
                                        restorableReason = "Missing mnemonic words";
                                    }
                                } else {
                                    errors.push_back("Missing mnemonic object in private payload.");
                                    fValid = false;
                                    restorableReason = "Missing mnemonic data";
                                }
                                reply.pushKV("mnemonic_language", mnemonicLanguage);
                                reply.pushKV("mnemonic_word_count", mnemonicWordCount);

                                std::string derivationProfileId =
                                    payload.exists("derivation_profile") && payload["derivation_profile"].isStr()
                                    ? payload["derivation_profile"].get_str() : "";
                                reply.pushKV("derivation_profile_id", derivationProfileId);
                                if (derivationProfileId != "hemp0x.mainnet.bip44.p2pkh.coin420.v1") {
                                    errors.push_back(strprintf(
                                        "Unsupported derivation profile '%s'. Only canonical Hemp0x coin420 is supported.",
                                        derivationProfileId));
                                    fValid = false;
                                    restorableReason = "Unsupported derivation profile";
                                }

                                int64_t extCount = payload.exists("external_count_hint") && payload["external_count_hint"].isNum()
                                    ? payload["external_count_hint"].get_int64() : 0;
                                int64_t changeCount = payload.exists("change_count_hint") && payload["change_count_hint"].isNum()
                                    ? payload["change_count_hint"].get_int64() : 0;
                                reply.pushKV("external_count_hint", extCount);
                                reply.pushKV("change_count_hint", changeCount);

                                if (fValid && chainMatches) {
                                    fRestorable = true;
                                    restorableReason = "BIP39/BIP44 coin420 payload validated";
                                } else if (fValid && !chainMatches) {
                                    restorableReason = "Chain mismatch";
                                }
                            }
                        }
                    }
                }
            } else if (!strPassphrase.empty() && fValid) {
                errors.push_back("Missing or invalid private encryption fields.");
                fValid = false;
                restorableReason = "Missing private encryption fields";
            } else if (!strPassphrase.empty()) {
                CleansePassphrase();
            }
        }

        reply.pushKV("private", privateResult);
    }

    reply.pushKV("valid", UniValue(fValid));
    reply.pushKV("restorable", UniValue(fRestorable));
    reply.pushKV("restorable_reason", restorableReason);
    reply.pushKV("errors", errors);
    reply.pushKV("warnings", warnings);

    return reply;
}
