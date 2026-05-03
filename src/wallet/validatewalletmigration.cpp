// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "util.h"
#include "wallet.h"

#include "rpcwallet.h"
#include "wallet/migration_envelope.h"
#include "support/cleanse.h"

#include <boost/filesystem.hpp>
#include <stdint.h>

#include <univalue.h>

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
            "    \"aad_profile\" : \"...\",           (string)\n"
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

    UniValue reply(UniValue::VOBJ);

    if (strFilename.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Filename cannot be empty");
    }

    boost::filesystem::path filepath = strFilename;
    filepath = boost::filesystem::absolute(filepath);

    MigrationEnvelopeValidation result;
    std::string rpc_error;

    bool ok = ValidateMigrationEnvelopeFile(filepath, strPassphrase, result, rpc_error);
    if (!strPassphrase.empty()) {
        memory_cleanse(&strPassphrase[0], strPassphrase.size());
    }

    if (!ok) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, rpc_error);
    }

    reply.pushKV("valid", UniValue(result.valid));
    reply.pushKV("envelope_version", result.envelope_version);
    reply.pushKV("schema_identifier", result.schema_identifier);
    reply.pushKV("source_client_version", result.source_client_version);
    reply.pushKV("exported_at", result.exported_at);

    UniValue chainObj(UniValue::VOBJ);
    chainObj.pushKV("network", result.network);
    chainObj.pushKV("coin_type_bip44", result.coin_type_bip44);
    chainObj.pushKV("matches_current_chain", UniValue(result.matches_current_chain));
    reply.pushKV("chain", chainObj);

    reply.pushKV("private_keys_included", UniValue(result.private_keys_included));

    if (result.envelope_version == 1) {
        reply.pushKV("wallet_type", result.wallet_type);
        reply.pushKV("hd_enabled", UniValue(result.hd_enabled));
        reply.pushKV("bip44_enabled", UniValue(result.bip44_enabled));
        reply.pushKV("mnemonic_available", UniValue(result.mnemonic_available));

        UniValue derivationProfiles(UniValue::VARR);
        for (const auto& entry : result.derivation_profiles) {
            UniValue profileEntry(UniValue::VOBJ);
            if (!entry.profile_id.empty()) profileEntry.pushKV("profile_id", entry.profile_id);
            if (entry.purpose >= 0) profileEntry.pushKV("purpose", entry.purpose);
            if (entry.coin_type) profileEntry.pushKV("coin_type", entry.coin_type);
            if (!entry.address_type.empty()) profileEntry.pushKV("address_type", entry.address_type);
            derivationProfiles.push_back(profileEntry);
        }
        reply.pushKV("derivation_profiles", derivationProfiles);
    } else {
        UniValue privateObj(UniValue::VOBJ);
        privateObj.pushKV("present", UniValue(result.private_present));
        privateObj.pushKV("encrypted", UniValue(result.private_encrypted));
        privateObj.pushKV("kdf_profile", result.kdf_profile);
        privateObj.pushKV("kdf_iterations", result.kdf_iterations);
        privateObj.pushKV("cipher_profile", result.cipher_profile);
        privateObj.pushKV("aad_profile", result.aad_profile);
        privateObj.pushKV("payload_format", result.payload_format);
        privateObj.pushKV("decryption_successful", UniValue(result.decryption_successful));
        reply.pushKV("private", privateObj);

        reply.pushKV("wallet_type", result.wallet_type);
        reply.pushKV("coin_type", result.payload_coin_type);
        reply.pushKV("account", result.account);
        reply.pushKV("mnemonic_language", result.mnemonic_language);
        reply.pushKV("mnemonic_word_count", result.mnemonic_word_count);
        reply.pushKV("derivation_profile_id", result.derivation_profile_id);
        reply.pushKV("external_count_hint", result.external_count_hint);
        reply.pushKV("change_count_hint", result.change_count_hint);
    }

    reply.pushKV("restorable", UniValue(result.restorable));
    reply.pushKV("restorable_reason", result.restorable_reason);

    UniValue errArr(UniValue::VARR);
    for (const auto& e : result.errors) {
        errArr.push_back(e);
    }
    reply.pushKV("errors", errArr);

    UniValue warnArr(UniValue::VARR);
    for (const auto& w : result.warnings) {
        warnArr.push_back(w);
    }
    reply.pushKV("warnings", warnArr);

    return reply;
}
