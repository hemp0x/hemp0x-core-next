// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "fs.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "util.h"
#include "wallet.h"
#include "validation.h"

#include "rpcwallet.h"
#include "wallet/migration_envelope.h"
#include "support/cleanse.h"

#include <boost/filesystem.hpp>
#include <stdint.h>

#include <univalue.h>

static bool IsValidWalletName(const std::string& name)
{
    if (name.empty()) {
        return false;
    }
    if (name.find("..") != std::string::npos) {
        return false;
    }
    if (name.find('/') != std::string::npos) {
        return false;
    }
    if (name.find('\\') != std::string::npos) {
        return false;
    }
    if (name.find(':') != std::string::npos) {
        return false;
    }
    if (name == ".") {
        return false;
    }
    return true;
}

UniValue restorewalletmigration(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
        throw std::runtime_error(
            "restorewalletmigration \"filename\" \"wallet_name\" \"passphrase\" ( birth_height )\n"
            "\nRestores a new wallet from a v2 encrypted Hemp0x Core migration envelope.\n"
            "\nCreates a new BIP39/BIP44 coin420 wallet from the mnemonic in the envelope.\n"
            "Supports v2 encrypted envelopes only. v1 public-only envelopes are rejected.\n"
            "Rejects existing destination wallets. Does not overwrite.\n"
            "\nArguments:\n"
            "1. \"filename\"          (string, required) Path to a v2 encrypted migration envelope file\n"
            "2. \"wallet_name\"       (string, required) Name for the new wallet directory under the datadir\n"
            "3. \"passphrase\"        (string, required) Export passphrase used to decrypt the envelope\n"
            "4. \"birth_height\"      (numeric, optional) Block height to start rescan from. "
                "If absent, uses envelope metadata if available; otherwise 0 (genesis).\n"
            "\nResult:\n"
            "{\n"
            "  \"wallet_name\" : \"...\",           (string)\n"
            "  \"wallet_file\" : \"...\",           (string)\n"
            "  \"wallet_arg\" : \"...\",            (string) Value to pass to -wallet when restarting\n"
            "  \"restored_at\" : n,                (numeric)\n"
            "  \"source_file\" : \"...\",            (string)\n"
            "  \"mnemonic_language\" : \"...\",      (string)\n"
            "  \"mnemonic_word_count\" : n,        (numeric)\n"
            "  \"network\" : \"...\",                (string)\n"
            "  \"coin_type\" : n,                  (numeric)\n"
            "  \"derivation_profile\" : \"...\",     (string)\n"
            "  \"account\" : n,                    (numeric)\n"
            "  \"external_count_hint\" : n,        (numeric)\n"
            "  \"change_count_hint\" : n,          (numeric)\n"
            "  \"keypool_size_after_restore\" : n, (numeric)\n"
            "  \"rescan_start_height\" : n,        (numeric)\n"
            "  \"rescan_end_height\" : n,          (numeric)\n"
            "  \"warnings\" : [...]                (array)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("restorewalletmigration", "\"/tmp/migration.json\" \"restored_wallet\" \"my export passphrase\"")
            + HelpExampleCli("restorewalletmigration", "\"/tmp/migration.json\" \"restored_wallet\" \"my export passphrase\" 2500000")
            + HelpExampleRpc("restorewalletmigration", "\"/tmp/migration.json\", \"restored_wallet\", \"my export passphrase\"")
        );

    ObserveSafeMode();

    std::string strFilename = request.params[0].get_str();
    std::string strWalletName = request.params[1].get_str();
    std::string strPassphrase = request.params[2].get_str();

    if (strFilename.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Filename cannot be empty");
    }

    boost::filesystem::path filepath = strFilename;
    filepath = boost::filesystem::absolute(filepath);

    if (!IsValidWalletName(strWalletName)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Invalid wallet name. The name must not be empty and must not contain "
            "path separators, '..', ':', or '\\'.");
    }

    fs::path walletDir = GetDataDir() / strWalletName;
    fs::path walletFile = walletDir / "wallet.dat";
    std::string walletFileArg = (fs::path(strWalletName) / "wallet.dat").string();

    if (boost::filesystem::exists(walletDir)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Destination wallet \"" + strWalletName + "\" already exists. "
            "Cannot restore into an existing wallet. Choose a different wallet name.");
    }

    for (const auto& loaded : vpwallets) {
        if (loaded && (loaded->GetName() == strWalletName ||
                       loaded->GetName() == walletFileArg ||
                       loaded->GetName() == walletFile.string())) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                "A wallet with the name \"" + strWalletName + "\" is already loaded. "
                "Choose a different wallet name.");
        }
    }

    MigrationEnvelopeRestoreData restoreData;
    std::string rpc_error;

    bool ok = ReadMigrationEnvelopeRestoreData(filepath, strPassphrase, restoreData, rpc_error);
    if (!strPassphrase.empty()) {
        memory_cleanse(&strPassphrase[0], strPassphrase.size());
    }

    if (!ok) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, rpc_error);
    }

    if (restoreData.mnemonic_words.empty()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR,
            "Failed to extract mnemonic from the envelope payload.");
    }

    int64_t birthHeight = 0;
    if (request.params.size() >= 4 && !request.params[3].isNull()) {
        birthHeight = request.params[3].get_int64();
    } else if (restoreData.best_block_height > 0) {
        birthHeight = restoreData.best_block_height;
    }

    CWallet* restoredWallet = nullptr;
    bool shouldCleanup = true;
    bool walletLoaded = false;

    auto CleanseRestoreSecrets = [&]() {
        if (!strPassphrase.empty()) {
            memory_cleanse(&strPassphrase[0], strPassphrase.size());
        }
        if (!restoreData.mnemonic_words.empty()) {
            memory_cleanse(&restoreData.mnemonic_words[0], restoreData.mnemonic_words.size());
        }
        if (!restoreData.mnemonic_passphrase.empty()) {
            memory_cleanse(&restoreData.mnemonic_passphrase[0], restoreData.mnemonic_passphrase.size());
        }
    };

    try {
        restoredWallet = CWallet::RestoreFromMnemonic(
            strWalletName, restoreData.mnemonic_words, restoreData.mnemonic_passphrase);
        if (!restoredWallet) {
            throw std::runtime_error("RestoreFromMnemonic returned null.");
        }
        shouldCleanup = false;
        vpwallets.push_back(restoredWallet);
        walletLoaded = true;
    } catch (const std::runtime_error& e) {
        CleanseRestoreSecrets();
        if (shouldCleanup && boost::filesystem::exists(walletDir)) {
            try { boost::filesystem::remove_all(walletDir); } catch (...) {}
        }
        throw JSONRPCError(RPC_WALLET_ERROR, std::string("Wallet restore failed: ") + e.what());
    } catch (...) {
        CleanseRestoreSecrets();
        if (shouldCleanup && boost::filesystem::exists(walletDir)) {
            try { boost::filesystem::remove_all(walletDir); } catch (...) {}
        }
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet restore failed due to an unexpected error.");
    }

    CBlockIndex* pindexRescan = chainActive.Genesis();
    if (birthHeight > 0) {
        CBlockIndex* pindexWalk = chainActive[birthHeight];
        if (pindexWalk) {
            pindexRescan = pindexWalk;
        }
    }

    int startHeight = pindexRescan->nHeight;
    int endHeight = startHeight;

    try {
        CBlockIndex* scanResult = restoredWallet->ScanForWalletTransactions(
            pindexRescan, nullptr, true);
        if (scanResult) {
            restoredWallet->SetBestChain(chainActive.GetLocator());
            endHeight = scanResult->nHeight;
        } else {
            if (restoredWallet->IsAbortingRescan()) {
                throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted.");
            }
            endHeight = chainActive.Height();
        }

        restoredWallet->SetBestChain(chainActive.GetLocator());
    } catch (const UniValue&) {
        CleanseRestoreSecrets();
        throw;
    } catch (...) {
        UniValue response(UniValue::VOBJ);
        response.pushKV("wallet_name", strWalletName);
        response.pushKV("wallet_file", walletFile.string());
        response.pushKV("wallet_arg", walletFileArg);
        response.pushKV("restored_at", GetTime());
        response.pushKV("source_file", boost::filesystem::absolute(filepath).string());
        response.pushKV("mnemonic_language", restoreData.validation.mnemonic_language);
        response.pushKV("mnemonic_word_count", restoreData.validation.mnemonic_word_count);
        response.pushKV("network", restoreData.validation.network);
        response.pushKV("coin_type", restoreData.validation.payload_coin_type);
        response.pushKV("derivation_profile", restoreData.validation.derivation_profile_id);
        response.pushKV("account", restoreData.validation.account);
        response.pushKV("external_count_hint", restoreData.validation.external_count_hint);
        response.pushKV("change_count_hint", restoreData.validation.change_count_hint);
        response.pushKV("keypool_size_after_restore",
            restoreData.validation.external_count_hint > 0
                ? (int64_t)restoreData.validation.external_count_hint : (int64_t)1000);

        UniValue warnings(UniValue::VARR);
        warnings.push_back("Restored wallet is NOT encrypted. Use encryptwallet to protect it.");
        warnings.push_back("Restored wallet created but rescan failed. "
            "Run rescanblockchain or restart with -rescan to complete the scan.");
        if (!walletLoaded) {
            warnings.push_back("Restored wallet was created on disk but is not loaded. Restart with -wallet="
                + walletFileArg + " before using it.");
        }
        response.pushKV("warnings", warnings);

        CleanseRestoreSecrets();
        return response;
    }
    CleanseRestoreSecrets();

    UniValue response(UniValue::VOBJ);
    response.pushKV("wallet_name", strWalletName);
    response.pushKV("wallet_file", walletFile.string());
    response.pushKV("wallet_arg", walletFileArg);
    response.pushKV("restored_at", GetTime());
    response.pushKV("source_file", boost::filesystem::absolute(filepath).string());
    response.pushKV("mnemonic_language", restoreData.validation.mnemonic_language);
    response.pushKV("mnemonic_word_count", restoreData.validation.mnemonic_word_count);
    response.pushKV("network", restoreData.validation.network);
    response.pushKV("coin_type", restoreData.validation.payload_coin_type);
    response.pushKV("derivation_profile", restoreData.validation.derivation_profile_id);
    response.pushKV("account", restoreData.validation.account);
    response.pushKV("external_count_hint", restoreData.validation.external_count_hint);
    response.pushKV("change_count_hint", restoreData.validation.change_count_hint);
    response.pushKV("keypool_size_after_restore",
        restoreData.validation.external_count_hint > 0
            ? (int64_t)restoreData.validation.external_count_hint : (int64_t)1000);
    response.pushKV("rescan_start_height", startHeight);
    response.pushKV("rescan_end_height", endHeight);

    UniValue warnings(UniValue::VARR);
    warnings.push_back("Restored wallet is NOT encrypted. Use encryptwallet to protect it.");
    warnings.push_back("Ensure the source envelope file and its passphrase are stored securely or deleted.");
    warnings.push_back("Restart the daemon with -wallet=" + walletFileArg + " to use this wallet as the default.");
    if (!restoreData.validation.matches_current_chain) {
        warnings.push_back("Envelope network does not match current chain. Addresses will differ.");
    }
    response.pushKV("warnings", warnings);

    return response;
}
