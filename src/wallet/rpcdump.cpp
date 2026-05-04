// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "chain.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "init.h"
#include "validation.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet.h"
#include "merkleblock.h"
#include "core_io.h"

#include "rpcwallet.h"
#include "wallet/migration_crypto.h"
#include "random.h"
#include "support/cleanse.h"

#include <fstream>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <univalue.h>


std::string static EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

int64_t static DecodeDumpTime(const std::string &str) {
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) |
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

UniValue importprivkey(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            "importprivkey \"privkey\" ( \"label\" ) ( rescan )\n"
            "\nAdds a private key (as returned by dumpprivkey) to your wallet.\n"
            "\nWARNING: Anyone with this private key can spend the funds it controls. Use this RPC only over a trusted local RPC connection and avoid command shells, logs, or scripts that may retain secrets.\n"
            "\nArguments:\n"
            "1. \"privkey\"          (string, required) The private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nDump a private key\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nImport using default blank label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importprivkey", "\"mykey\", \"testing\", false")
        );


    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::string strSecret = request.params[0].get_str();
    std::string strLabel = "";
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");

    CHemp0xSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

    CKey key = vchSecret.GetKey();
    if (!key.IsValid()) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");

    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();
    {
        pwallet->MarkDirty();
        pwallet->SetAddressBook(vchAddress, strLabel, "receive");

        // Don't throw error in case a key is already there
        if (pwallet->HaveKey(vchAddress)) {
            return NullUniValue;
        }

        pwallet->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwallet->AddKeyPubKey(key, pubkey)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");
        }

        // whenever a key is imported, we need to scan the whole chain
        pwallet->UpdateTimeFirstKey(1);

        if (fRescan) {
            pwallet->RescanFromTime(TIMESTAMP_MIN, true /* update */);
        }
    }

    return NullUniValue;
}

UniValue abortrescan(const JSONRPCRequest& request)
{
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "abortrescan\n"
            "\nStops current wallet rescan triggered e.g. by an importprivkey call.\n"
            "\nExamples:\n"
            "\nImport a private key\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nAbort the running wallet rescan\n"
            + HelpExampleCli("abortrescan", "") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("abortrescan", "")
        );

    ObserveSafeMode();
    if (!pwallet->IsScanning() || pwallet->IsAbortingRescan()) return false;
    pwallet->AbortRescan();
    return true;
}

void ImportAddress(CWallet*, const CTxDestination& dest, const std::string& strLabel);
void ImportScript(CWallet* const pwallet, const CScript& script, const std::string& strLabel, bool isRedeemScript)
{
    if (!isRedeemScript && ::IsMine(*pwallet, script) == ISMINE_SPENDABLE) {
        throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
    }

    pwallet->MarkDirty();

    if (!pwallet->HaveWatchOnly(script) && !pwallet->AddWatchOnly(script, 0 /* nCreateTime */)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
    }

    if (isRedeemScript) {
        if (!pwallet->HaveCScript(script) && !pwallet->AddCScript(script)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
        }
        ImportAddress(pwallet, CScriptID(script), strLabel);
    } else {
        CTxDestination destination;
        if (ExtractDestination(script, destination)) {
            pwallet->SetAddressBook(destination, strLabel, "receive");
        }
    }
}

void ImportAddress(CWallet* const pwallet, const CTxDestination& dest, const std::string& strLabel)
{
    CScript script = GetScriptForDestination(dest);
    ImportScript(pwallet, script, strLabel, false);
    // add to address book or update label
    if (IsValidDestination(dest))
        pwallet->SetAddressBook(dest, strLabel, "receive");
}

UniValue importaddress(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            "importaddress \"address\" ( \"label\" rescan p2sh )\n"
            "\nAdds a script (in hex) or address that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"script\"           (string, required) The hex-encoded script (or address)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "4. p2sh                 (boolean, optional, default=false) Add the P2SH version of the script as well\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "If you have the full public key, you should call importpubkey instead of this.\n"
            "\nNote: If you import a non-standard raw script in hex form, outputs sending to it will be treated\n"
            "as change, and not show up in many RPCs.\n"
            "\nExamples:\n"
            "\nImport a script with rescan\n"
            + HelpExampleCli("importaddress", "\"myscript\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importaddress", "\"myscript\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importaddress", "\"myscript\", \"testing\", false")
        );


    std::string strLabel = "";
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");

    // Whether to import a p2sh version, too
    bool fP2SH = false;
    if (!request.params[3].isNull())
        fP2SH = request.params[3].get_bool();

    LOCK2(cs_main, pwallet->cs_wallet);

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (IsValidDestination(dest)) {
        if (fP2SH) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot use the p2sh flag with an address - use a script instead");
        }
        ImportAddress(pwallet, dest, strLabel);
    } else if (IsHex(request.params[0].get_str())) {
        std::vector<unsigned char> data(ParseHex(request.params[0].get_str()));
        ImportScript(pwallet, CScript(data.begin(), data.end()), strLabel, fP2SH);
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hemp0x address or script");
    }

    if (fRescan)
    {
        pwallet->RescanFromTime(TIMESTAMP_MIN, true /* update */);
        pwallet->ReacceptWalletTransactions();
    }

    return NullUniValue;
}

UniValue importprunedfunds(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "importprunedfunds\n"
            "\nImports funds without rescan. Corresponding address or script must previously be included in wallet. Aimed towards pruned wallets. The end-user is responsible to import additional transactions that subsequently spend the imported outputs or rescan after the point in the blockchain the transaction is included.\n"
            "\nArguments:\n"
            "1. \"rawtransaction\" (string, required) A raw transaction in hex funding an already-existing address in wallet\n"
            "2. \"txoutproof\"     (string, required) The hex output from gettxoutproof that contains the transaction\n"
        );

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    uint256 hashTx = tx.GetHash();
    CWalletTx wtx(pwallet, MakeTransactionRef(std::move(tx)));

    CDataStream ssMB(ParseHexV(request.params[1], "proof"), SER_NETWORK, PROTOCOL_VERSION);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    //Search partial merkle tree in proof for our transaction and index in valid block
    std::vector<uint256> vMatch;
    std::vector<unsigned int> vIndex;
    unsigned int txnIndex = 0;
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) == merkleBlock.header.hashMerkleRoot) {

        LOCK(cs_main);

        if (!mapBlockIndex.count(merkleBlock.header.GetHash()) || !chainActive.Contains(mapBlockIndex[merkleBlock.header.GetHash()]))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found in chain");

        std::vector<uint256>::const_iterator it;
        if ((it = std::find(vMatch.begin(), vMatch.end(), hashTx))==vMatch.end()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction given doesn't exist in proof");
        }

        txnIndex = vIndex[it - vMatch.begin()];
    }
    else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Something wrong with merkleblock");
    }

    wtx.nIndex = txnIndex;
    wtx.hashBlock = merkleBlock.header.GetHash();

    LOCK2(cs_main, pwallet->cs_wallet);

    if (pwallet->IsMine(wtx)) {
        pwallet->AddToWallet(wtx, false);
        return NullUniValue;
    }

    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No addresses in wallet correspond to included transaction");
}

UniValue removeprunedfunds(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "removeprunedfunds \"txid\"\n"
            "\nDeletes the specified transaction from the wallet. Meant for use with pruned wallets and as a companion to importprunedfunds. This will affect wallet balances.\n"
            "\nArguments:\n"
            "1. \"txid\"           (string, required) The hex-encoded id of the transaction you are deleting\n"
            "\nExamples:\n"
            + HelpExampleCli("removeprunedfunds", "\"a8d0c0184dde994a09ec054286f1ce581bebf46446a512166eae7628734ea0a5\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("removeprunedfunds", "\"a8d0c0184dde994a09ec054286f1ce581bebf46446a512166eae7628734ea0a5\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());
    std::vector<uint256> vHash;
    vHash.push_back(hash);
    std::vector<uint256> vHashOut;

    if (pwallet->ZapSelectTx(vHash, vHashOut) != DB_LOAD_OK) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Could not properly delete the transaction.");
    }

    if(vHashOut.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Transaction does not exist in wallet.");
    }

    return NullUniValue;
}

UniValue importpubkey(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            "importpubkey \"pubkey\" ( \"label\" rescan )\n"
            "\nAdds a public key (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"pubkey\"           (string, required) The hex-encoded public key\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nImport a public key with rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importpubkey", "\"mypubkey\", \"testing\", false")
        );


    std::string strLabel = "";
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");

    if (!IsHex(request.params[0].get_str()))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");
    std::vector<unsigned char> data(ParseHex(request.params[0].get_str()));
    CPubKey pubKey(data.begin(), data.end());
    if (!pubKey.IsFullyValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");

    LOCK2(cs_main, pwallet->cs_wallet);

    ImportAddress(pwallet, pubKey.GetID(), strLabel);
    ImportScript(pwallet, GetScriptForRawPubKey(pubKey), strLabel, false);

    if (fRescan)
    {
        pwallet->RescanFromTime(TIMESTAMP_MIN, true /* update */);
        pwallet->ReacceptWalletTransactions();
    }

    return NullUniValue;
}


UniValue importwallet(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "importwallet \"filename\"\n"
            "\nImports keys from a wallet dump file (see dumpwallet).\n"
            "\nWARNING: Wallet dump files contain private keys and may contain HD seed material. Protect the file before importing it and securely remove it when it is no longer needed.\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The wallet file\n"
            "\nExamples:\n"
            "\nDump the wallet\n"
            + HelpExampleCli("dumpwallet", "\"test\"") +
            "\nImport the wallet\n"
            + HelpExampleCli("importwallet", "\"test\"") +
            "\nImport using the json rpc call\n"
            + HelpExampleRpc("importwallet", "\"test\"")
        );

    if (fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Importing wallets is disabled in pruned mode");

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::ifstream file;
    file.open(request.params[0].get_str().c_str(), std::ios::in | std::ios::ate);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    int64_t nTimeBegin = chainActive.Tip()->GetBlockTime();

    bool fGood = true;

    int64_t nFilesize = std::max((int64_t)1, (int64_t)file.tellg());
    file.seekg(0, file.beg);

    pwallet->ShowProgress(_("Importing..."), 0); // show progress dialog in GUI
    while (file.good()) {
        pwallet->ShowProgress("", std::max(1, std::min(99, (int)(((double)file.tellg() / (double)nFilesize) * 100))));
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;
        CHemp0xSecret vchSecret;
        if (!vchSecret.SetString(vstr[0]))
            continue;
        CKey key = vchSecret.GetKey();
        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID keyid = pubkey.GetID();
        if (pwallet->HaveKey(keyid)) {
            LogPrintf("Skipping import of %s (key already present)\n", EncodeDestination(keyid));
            continue;
        }
        int64_t nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        bool fLabel = true;
        for (unsigned int nStr = 2; nStr < vstr.size(); nStr++) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel = true;
            }
        }
        LogPrintf("Importing %s...\n", EncodeDestination(keyid));
        if (!pwallet->AddKeyPubKey(key, pubkey)) {
            fGood = false;
            continue;
        }
        pwallet->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwallet->SetAddressBook(keyid, strLabel, "receive");
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();
    pwallet->ShowProgress("", 100); // hide progress dialog in GUI
    pwallet->UpdateTimeFirstKey(nTimeBegin);
    pwallet->RescanFromTime(nTimeBegin, false /* update */);
    pwallet->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    return NullUniValue;
}

UniValue dumpprivkey(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "dumpprivkey \"address\"\n"
            "\nReveals the private key corresponding to 'address'.\n"
            "Then the importprivkey can be used with this output\n"
            "\nWARNING: The returned private key gives full spend authority for this address. Do not expose it in shared terminals, command history, scripts, logs, or support tickets.\n"
            "\nArguments:\n"
            "1. \"address\"   (string, required) The hemp0x address for the private key\n"
            "\nResult:\n"
            "\"key\"                (string) The private key\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"")
            + HelpExampleCli("importprivkey", "\"mykey\"")
            + HelpExampleRpc("dumpprivkey", "\"myaddress\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::string strAddress = request.params[0].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hemp0x address");
    }
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }
    CKey vchSecret;
    if (!pwallet->GetKey(*keyID, vchSecret)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    }
    return CHemp0xSecret(vchSecret).ToString();
}


UniValue dumpwallet(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "dumpwallet \"filename\"\n"
            "\nDumps all wallet keys in a human-readable format to a server-side file. This does not allow overwriting existing files.\n"
            "\nWARNING: The dump file contains private keys and may contain HD seed or mnemonic material. Store it only on trusted storage with restrictive permissions, and securely remove it when it is no longer needed.\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The filename with path (either absolute or relative to hemp0xd)\n"
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"filename\" : {        (string) The filename with full absolute path\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpwallet", "\"test\"")
            + HelpExampleRpc("dumpwallet", "\"test\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    boost::filesystem::path filepath = request.params[0].get_str();
    filepath = boost::filesystem::absolute(filepath);

    /* Prevent arbitrary files from being overwritten. There have been reports
     * that users have overwritten wallet files this way:
     * https://github.com/bitcoin/bitcoin/issues/9934
     * It may also avoid other security issues.
     */
    if (boost::filesystem::exists(filepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, filepath.string() + " already exists. If you are sure this is what you want, move it out of the way first");
    }

    std::ofstream file;
    file.open(filepath.string().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CTxDestination, int64_t> mapKeyBirth;
    const std::map<CKeyID, int64_t>& mapKeyPool = pwallet->GetAllReserveKeys();
    pwallet->GetKeyBirthTimes(mapKeyBirth);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (const auto& entry : mapKeyBirth) {
        if (const CKeyID* keyID = boost::get<CKeyID>(&entry.first)) { // set and test
            vKeyBirth.push_back(std::make_pair(entry.second, *keyID));
        }
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by Hemp0x %s\n", CLIENT_BUILD);
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(chainActive.Tip()->GetBlockTime()));
    file << "\n";

    // add the base58check encoded extended master if the wallet uses HD
    CKeyID seed_id = pwallet->GetHDChain().seed_id;
    if (!seed_id.IsNull())
    {

        if (!pwallet->GetHDChain().IsBip44()) {
            CKey seed;
            if (pwallet->GetKey(seed_id, seed)) {
                CExtKey masterKey;
                masterKey.SetSeed(seed.begin(), seed.size());

                CHemp0xExtKey b58extkey;
                b58extkey.SetKey(masterKey);

                CExtPubKey pubkey;
                pubkey = masterKey.Neuter();

                CHemp0xExtPubKey b58extpubkey;
                b58extpubkey.SetKey(pubkey);

                file << "# extended private masterkey: " << b58extkey.ToString() << "\n\n";
                file << "# extended public masterkey: " << b58extpubkey.ToString() << "\n\n";
            }
        }

		if(pwallet->GetHDChain().IsBip44())
		{
            CWalletDB walletdb(pwallet->GetDBHandle());

            std::vector<unsigned char> vchWords;
            std::vector<unsigned char> vchPassphrase;
            std::vector<unsigned char> vchSeed;
            uint256 hash;

            pwallet->GetBip39Data(hash, vchWords, vchPassphrase, vchSeed);

            CExtKey masterKey;
            masterKey.SetSeed(vchSeed.data(), vchSeed.size());

            CHemp0xExtKey b58extkey;
            b58extkey.SetKey(masterKey);

            CExtPubKey pubkey;
            pubkey = masterKey.Neuter();

            CHemp0xExtPubKey b58extpubkey;
            b58extpubkey.SetKey(pubkey);

            file << "# extended private masterkey: " << b58extkey.ToString() << "\n\n";
            file << "# extended public masterkey: " << b58extpubkey.ToString() << "\n\n";

			file << "# HD seed: " << HexStr(vchSeed) << "\n";
			file << "# mnemonic: " << std::string(vchWords.begin(), vchWords.end()).c_str() << "\n";
			file << "# mnemonic passphrase: " << std::string(vchPassphrase.begin(), vchPassphrase.end()).c_str() << "\n";
			file << "# hash of words: " << hash.GetHex() << "\n\n";
		}
    }
    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strTime = EncodeDumpTime(it->first);
        std::string strAddr = EncodeDestination(keyid);
        CKey key;
        if (pwallet->GetKey(keyid, key)) {
            file << strprintf("%s %s ", CHemp0xSecret(key).ToString(), strTime);
            if (pwallet->mapAddressBook.count(keyid)) {
                file << strprintf("label=%s", EncodeDumpString(pwallet->mapAddressBook[keyid].name));
            } else if (keyid == seed_id) {
                file << "hdseed=1";
            } else if (mapKeyPool.count(keyid)) {
                file << "reserve=1";
            } else if (pwallet->mapKeyMetadata[keyid].hdKeypath == "s") {
                file << "inactivehdseed=1";
            } else {
                file << "change=1";
            }
            file << strprintf(" # addr=%s%s\n", strAddr, (pwallet->mapKeyMetadata[keyid].hdKeypath.size() > 0 ? " hdkeypath="+pwallet->mapKeyMetadata[keyid].hdKeypath : ""));
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();

    UniValue reply(UniValue::VOBJ);
    reply.push_back(Pair("filename", filepath.string()));

    return reply;
}

UniValue getmasterkeyinfo(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "getmasterkeyinfo\n"
                "\nFetches and displays the master private key and the master public key.\n"
                "\nWARNING: This RPC reveals root and account private key material. Use it only on trusted local systems and never paste the output into logs, tickets, chat, or untrusted tools.\n"
                "\nResult:\n"
                "{                           (json object)\n"
                "  \"bip32_root_private\" : (string) extended master private key,\n"
                "  \"bip32_root_public\" :  (string) extended master public key,\n"
                "  \"account_derivation_path\" : (string) The derivation path to the account public/private keys\n"
                "  \"account_extended_private_key\" : (string) extended account private key,\n"
                "  \"account_extended_public_key\" :  (string) extended account public key,\n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli("getmasterkeyinfo", "")
                + HelpExampleRpc("getmasterkeyinfo", "")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    UniValue ret(UniValue::VOBJ);

    // add the base58check encoded extended master if the wallet uses HD
    CKeyID seed_id = pwallet->GetHDChain().seed_id;
    if (!seed_id.IsNull())
    {

        if (!pwallet->GetHDChain().IsBip44()) {
            CKey seed;
            if (pwallet->GetKey(seed_id, seed)) {

                // Create the master key
                CExtKey masterKey;
                masterKey.SetSeed(seed.begin(), seed.size());;

                // Get the Hemp0x Ext Key from the master key
                CHemp0xExtKey b58extkey;
                b58extkey.SetKey(masterKey);

                // Get the public key from the master key
                CExtPubKey pubkey;
                pubkey = masterKey.Neuter();

                // Get the Hemp0x Ext Key from the public key
                CHemp0xExtPubKey b58extpubkey;
                b58extpubkey.SetKey(pubkey);

                // Add the private and public key to the output
                ret.push_back(std::make_pair("bip32_root_private",  b58extkey.ToString()));
                ret.push_back(std::make_pair("bip32_root_public",  b58extpubkey.ToString()));
                ret.push_back(std::make_pair("account_derivation_path",  "m/0'"));

                CExtKey accountKey;
                // derive m/account'
                masterKey.Derive(accountKey, 0 | 0x80000000);

                // Create the account public key from the account private key
                CExtPubKey account_extended_public_key;
                account_extended_public_key = accountKey.Neuter();

                // Create the Hemp0x Account Ext Private Key
                CHemp0xExtKey b58accountextprivatekey;
                b58accountextprivatekey.SetKey(accountKey);

                // Create the Hemp0x Account Ext Public Key
                CHemp0xExtPubKey b58actextpubkey;
                b58actextpubkey.SetKey(account_extended_public_key);

                // Add the account extended public and private keys to the return
                ret.push_back(std::make_pair("account_extended_private_key",  b58accountextprivatekey.ToString()));
                ret.push_back(std::make_pair("account_extended_public_key",  b58actextpubkey.ToString()));

            }
        }

        if(pwallet->GetHDChain().IsBip44())
        {
            CWalletDB walletdb(pwallet->GetDBHandle());

            std::vector<unsigned char> vchWords;
            std::vector<unsigned char> vchPassphrase;
            std::vector<unsigned char> vchSeed;
            uint256 hash;

            pwallet->GetBip39Data(hash, vchWords, vchPassphrase, vchSeed);

            // Create the master key
            CExtKey masterKey;
            masterKey.SetSeed(vchSeed.data(), vchSeed.size());

            // Get the Hemp0x Ext Key from the master key
            CHemp0xExtKey b58extkey;
            b58extkey.SetKey(masterKey);

            // Get the public key from the master key
            CExtPubKey pubkey;
            pubkey = masterKey.Neuter();

            // Get the Hemp0x Ext Key from the public key
            CHemp0xExtPubKey b58extpubkey;
            b58extpubkey.SetKey(pubkey);

            // Add the private and public key to the output
            ret.push_back(std::make_pair("bip32_root_private",  b58extkey.ToString()));
            ret.push_back(std::make_pair("bip32_root_public",  b58extpubkey.ToString()));
            std::string path = strprintf("m/44'/%d'/%d'", GetParams().ExtCoinType(), 0);
            ret.push_back(std::make_pair("account_derivation_path",  path));

            // Lets generate the account private and public keys
            CExtKey purposeKey;
            CExtKey coinTypeKey;
            CExtKey accountKey;
            // derive m/purpose'
            masterKey.Derive(purposeKey, 44 | 0x80000000);
            // derive m/purpose'/coin_type'
            purposeKey.Derive(coinTypeKey, GetParams().ExtCoinType() | 0x80000000);
            // derive m/purpose'/coin_type'/account'
            coinTypeKey.Derive(accountKey, 0 | 0x80000000);

            // Create the account public key from the account private key
            CExtPubKey account_extended_public_key;
            account_extended_public_key = accountKey.Neuter();

            // Create the Hemp0x Account Ext Private Key
            CHemp0xExtKey b58accountextprivatekey;
            b58accountextprivatekey.SetKey(accountKey);

            // Create the Hemp0x Account Ext Public Key
            CHemp0xExtPubKey b58actextpubkey;
            b58actextpubkey.SetKey(account_extended_public_key);

            // Add the account extended public and private keys to the return
            ret.push_back(std::make_pair("account_extended_private_key",  b58accountextprivatekey.ToString()));
            ret.push_back(std::make_pair("account_extended_public_key",  b58actextpubkey.ToString()));
        }
    }

    return ret;
}


UniValue ProcessImport(CWallet * const pwallet, const UniValue& data, const int64_t timestamp)
{
    try {
        bool success = false;

        // Required fields.
        const UniValue& scriptPubKey = data["scriptPubKey"];

        // Should have script or JSON with "address".
        if (!(scriptPubKey.getType() == UniValue::VOBJ && scriptPubKey.exists("address")) && !(scriptPubKey.getType() == UniValue::VSTR)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid scriptPubKey");
        }

        // Optional fields.
        const std::string& strRedeemScript = data.exists("redeemscript") ? data["redeemscript"].get_str() : "";
        const UniValue& pubKeys = data.exists("pubkeys") ? data["pubkeys"].get_array() : UniValue();
        const UniValue& keys = data.exists("keys") ? data["keys"].get_array() : UniValue();
        const bool& internal = data.exists("internal") ? data["internal"].get_bool() : false;
        const bool& watchOnly = data.exists("watchonly") ? data["watchonly"].get_bool() : false;
        const std::string& label = data.exists("label") && !internal ? data["label"].get_str() : "";

        bool isScript = scriptPubKey.getType() == UniValue::VSTR;
        bool isP2SH = strRedeemScript.length() > 0;
        const std::string& output = isScript ? scriptPubKey.get_str() : scriptPubKey["address"].get_str();

        // Parse the output.
        CScript script;
        CTxDestination dest;

        if (!isScript) {
            dest = DecodeDestination(output);
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            }
            script = GetScriptForDestination(dest);
        } else {
            if (!IsHex(output)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid scriptPubKey");
            }

            std::vector<unsigned char> vData(ParseHex(output));
            script = CScript(vData.begin(), vData.end());
        }

        // Watchonly and private keys
        if (watchOnly && keys.size()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Incompatibility found between watchonly and keys");
        }

        // Internal + Label
        if (internal && data.exists("label")) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Incompatibility found between internal and label");
        }

        // Not having Internal + Script
        if (!internal && isScript) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Internal must be set for hex scriptPubKey");
        }

        // Keys / PubKeys size check.
        if (!isP2SH && (keys.size() > 1 || pubKeys.size() > 1)) { // Address / scriptPubKey
            throw JSONRPCError(RPC_INVALID_PARAMETER, "More than private key given for one address");
        }

        // Invalid P2SH redeemScript
        if (isP2SH && !IsHex(strRedeemScript)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid redeem script");
        }

        // Process. //

        // P2SH
        if (isP2SH) {
            // Import redeem script.
            std::vector<unsigned char> vData(ParseHex(strRedeemScript));
            CScript redeemScript = CScript(vData.begin(), vData.end());

            // Invalid P2SH address
            if (!script.IsPayToScriptHash()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid P2SH address / script");
            }

            pwallet->MarkDirty();

            if (!pwallet->AddWatchOnly(redeemScript, timestamp)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
            }

            if (!pwallet->HaveCScript(redeemScript) && !pwallet->AddCScript(redeemScript)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
            }

            CTxDestination redeem_dest = CScriptID(redeemScript);
            CScript redeemDestination = GetScriptForDestination(redeem_dest);

            if (::IsMine(*pwallet, redeemDestination) == ISMINE_SPENDABLE) {
                throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
            }

            pwallet->MarkDirty();

            if (!pwallet->AddWatchOnly(redeemDestination, timestamp)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
            }

            // add to address book or update label
            if (IsValidDestination(dest)) {
                pwallet->SetAddressBook(dest, label, "receive");
            }

            // Import private keys.
            if (keys.size()) {
                for (size_t i = 0; i < keys.size(); i++) {
                    const std::string& privkey = keys[i].get_str();

                    CHemp0xSecret vchSecret;
                    bool fGood = vchSecret.SetString(privkey);

                    if (!fGood) {
                        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
                    }

                    CKey key = vchSecret.GetKey();

                    if (!key.IsValid()) {
                        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");
                    }

                    CPubKey pubkey = key.GetPubKey();
                    assert(key.VerifyPubKey(pubkey));

                    CKeyID vchAddress = pubkey.GetID();
                    pwallet->MarkDirty();
                    pwallet->SetAddressBook(vchAddress, label, "receive");

                    if (pwallet->HaveKey(vchAddress)) {
                        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Already have this key");
                    }

                    pwallet->mapKeyMetadata[vchAddress].nCreateTime = timestamp;

                    if (!pwallet->AddKeyPubKey(key, pubkey)) {
                        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");
                    }

                    pwallet->UpdateTimeFirstKey(timestamp);
                }
            }

            success = true;
        } else {
            // Import public keys.
            if (pubKeys.size() && keys.size() == 0) {
                const std::string& strPubKey = pubKeys[0].get_str();

                if (!IsHex(strPubKey)) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");
                }

                std::vector<unsigned char> vData(ParseHex(strPubKey));
                CPubKey pubKey(vData.begin(), vData.end());

                if (!pubKey.IsFullyValid()) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");
                }

                CTxDestination pubkey_dest = pubKey.GetID();

                // Consistency check.
                if (!isScript && !(pubkey_dest == dest)) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                }

                // Consistency check.
                if (isScript) {
                    CTxDestination destination;

                    if (ExtractDestination(script, destination)) {
                        if (!(destination == pubkey_dest)) {
                            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                        }
                    }
                }

                CScript pubKeyScript = GetScriptForDestination(pubkey_dest);

                if (::IsMine(*pwallet, pubKeyScript) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(pubKeyScript, timestamp)) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
                }

                // add to address book or update label
                if (IsValidDestination(pubkey_dest)) {
                    pwallet->SetAddressBook(pubkey_dest, label, "receive");
                }

                // TODO Is this necessary?
                CScript scriptRawPubKey = GetScriptForRawPubKey(pubKey);

                if (::IsMine(*pwallet, scriptRawPubKey) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(scriptRawPubKey, timestamp)) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
                }

                success = true;
            }

            // Import private keys.
            if (keys.size()) {
                const std::string& strPrivkey = keys[0].get_str();

                // Checks.
                CHemp0xSecret vchSecret;
                bool fGood = vchSecret.SetString(strPrivkey);

                if (!fGood) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
                }

                CKey key = vchSecret.GetKey();
                if (!key.IsValid()) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");
                }

                CPubKey pubKey = key.GetPubKey();
                assert(key.VerifyPubKey(pubKey));

                CTxDestination pubkey_dest = pubKey.GetID();

                // Consistency check.
                if (!isScript && !(pubkey_dest == dest)) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                }

                // Consistency check.
                if (isScript) {
                    CTxDestination destination;

                    if (ExtractDestination(script, destination)) {
                        if (!(destination == pubkey_dest)) {
                            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                        }
                    }
                }

                CKeyID vchAddress = pubKey.GetID();
                pwallet->MarkDirty();
                pwallet->SetAddressBook(vchAddress, label, "receive");

                if (pwallet->HaveKey(vchAddress)) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->mapKeyMetadata[vchAddress].nCreateTime = timestamp;

                if (!pwallet->AddKeyPubKey(key, pubKey)) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");
                }

                pwallet->UpdateTimeFirstKey(timestamp);

                success = true;
            }

            // Import scriptPubKey only.
            if (pubKeys.size() == 0 && keys.size() == 0) {
                if (::IsMine(*pwallet, script) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(script, timestamp)) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
                }

                if (scriptPubKey.getType() == UniValue::VOBJ) {
                    // add to address book or update label
                    if (IsValidDestination(dest)) {
                        pwallet->SetAddressBook(dest, label, "receive");
                    }
                }

                success = true;
            }
        }

        UniValue result = UniValue(UniValue::VOBJ);
        result.pushKV("success", UniValue(success));
        return result;
    } catch (const UniValue& e) {
        UniValue result = UniValue(UniValue::VOBJ);
        result.pushKV("success", UniValue(false));
        result.pushKV("error", e);
        return result;
    } catch (...) {
        UniValue result = UniValue(UniValue::VOBJ);
        result.pushKV("success", UniValue(false));
        result.pushKV("error", JSONRPCError(RPC_MISC_ERROR, "Missing required fields"));
        return result;
    }
}

int64_t GetImportTimestamp(const UniValue& data, int64_t now)
{
    if (data.exists("timestamp")) {
        const UniValue& timestamp = data["timestamp"];
        if (timestamp.isNum()) {
            return timestamp.get_int64();
        } else if (timestamp.isStr() && timestamp.get_str() == "now") {
            return now;
        }
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Expected number or \"now\" timestamp value for key. got type %s", uvTypeName(timestamp.type())));
    }
    throw JSONRPCError(RPC_TYPE_ERROR, "Missing required timestamp field for key");
}

UniValue importmulti(const JSONRPCRequest& mainRequest)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(mainRequest);
    if (!EnsureWalletIsAvailable(pwallet, mainRequest.fHelp)) {
        return NullUniValue;
    }

    // clang-format off
    if (mainRequest.fHelp || mainRequest.params.size() < 1 || mainRequest.params.size() > 2)
        throw std::runtime_error(
            "importmulti \"requests\" ( \"options\" )\n\n"
            "Import addresses/scripts (with private or public keys, redeem script (P2SH)), rescanning all addresses in one-shot-only (rescan can be disabled via options).\n\n"
            "WARNING: The requests array may contain private keys. Use this RPC only over a trusted local RPC connection and avoid command shells, logs, or scripts that may retain secrets.\n\n"
            "Arguments:\n"
            "1. requests     (array, required) Data to be imported\n"
            "  [     (array of json objects)\n"
            "    {\n"
            "      \"scriptPubKey\": \"<script>\" | { \"address\":\"<address>\" }, (string / json, required) Type of scriptPubKey (string for script, json for address)\n"
            "      \"timestamp\": timestamp | \"now\"                        , (integer / string, required) Creation time of the key in seconds since epoch (Jan 1 1970 GMT),\n"
            "                                                              or the string \"now\" to substitute the current synced blockchain time. The timestamp of the oldest\n"
            "                                                              key will determine how far back blockchain rescans need to begin for missing wallet transactions.\n"
            "                                                              \"now\" can be specified to bypass scanning, for keys which are known to never have been used, and\n"
            "                                                              0 can be specified to scan the entire blockchain. Blocks up to 2 hours before the earliest key\n"
            "                                                              creation time of all keys being imported by the importmulti call will be scanned.\n"
            "      \"redeemscript\": \"<script>\"                            , (string, optional) Allowed only if the scriptPubKey is a P2SH address or a P2SH scriptPubKey\n"
            "      \"pubkeys\": [\"<pubKey>\", ... ]                         , (array, optional) Array of strings giving pubkeys that must occur in the output or redeemscript\n"
            "      \"keys\": [\"<key>\", ... ]                               , (array, optional) Array of strings giving private keys whose corresponding public keys must occur in the output or redeemscript\n"
            "      \"internal\": <true>                                    , (boolean, optional, default: false) Stating whether matching outputs should be treated as not incoming payments\n"
            "      \"watchonly\": <true>                                   , (boolean, optional, default: false) Stating whether matching outputs should be considered watched even when they're not spendable, only allowed if keys are empty\n"
            "      \"label\": <label>                                      , (string, optional, default: '') Label to assign to the address (aka account name, for now), only allowed with internal=false\n"
            "    }\n"
            "  ,...\n"
            "  ]\n"
            "2. options                 (json, optional)\n"
            "  {\n"
            "     \"rescan\": <false>,         (boolean, optional, default: true) Stating if should rescan the blockchain after all imports\n"
            "  }\n"
            "\nExamples:\n" +
            HelpExampleCli("importmulti", "'[{ \"scriptPubKey\": { \"address\": \"<my address>\" }, \"timestamp\":1455191478 }, "
                                          "{ \"scriptPubKey\": { \"address\": \"<my 2nd address>\" }, \"label\": \"example 2\", \"timestamp\": 1455191480 }]'") +
            HelpExampleCli("importmulti", "'[{ \"scriptPubKey\": { \"address\": \"<my address>\" }, \"timestamp\":1455191478 }]' '{ \"rescan\": false}'") +

            "\nResponse is an array with the same size as the input that has the execution result :\n"
            "  [{ \"success\": true } , { \"success\": false, \"error\": { \"code\": -1, \"message\": \"Internal Server Error\"} }, ... ]\n");

    // clang-format on

    RPCTypeCheck(mainRequest.params, {UniValue::VARR, UniValue::VOBJ});

    const UniValue& requests = mainRequest.params[0];

    //Default options
    bool fRescan = true;

    if (!mainRequest.params[1].isNull()) {
        const UniValue& options = mainRequest.params[1];

        if (options.exists("rescan")) {
            fRescan = options["rescan"].get_bool();
        }
    }

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    // Verify all timestamps are present before importing any keys.
    const int64_t now = chainActive.Tip() ? chainActive.Tip()->GetMedianTimePast() : 0;
    for (const UniValue& data : requests.getValues()) {
        GetImportTimestamp(data, now);
    }

    bool fRunScan = false;
    const int64_t minimumTimestamp = 1;
    int64_t nLowestTimestamp = 0;

    if (fRescan && chainActive.Tip()) {
        nLowestTimestamp = chainActive.Tip()->GetBlockTime();
    } else {
        fRescan = false;
    }

    UniValue response(UniValue::VARR);

    for (const UniValue& data : requests.getValues()) {
        const int64_t timestamp = std::max(GetImportTimestamp(data, now), minimumTimestamp);
        const UniValue result = ProcessImport(pwallet, data, timestamp);
        response.push_back(result);

        if (!fRescan) {
            continue;
        }

        // If at least one request was successful then allow rescan.
        if (result["success"].get_bool()) {
            fRunScan = true;
        }

        // Get the lowest timestamp.
        if (timestamp < nLowestTimestamp) {
            nLowestTimestamp = timestamp;
        }
    }

    if (fRescan && fRunScan && requests.size()) {
        int64_t scannedTime = pwallet->RescanFromTime(nLowestTimestamp, true /* update */);
        pwallet->ReacceptWalletTransactions();

        if (scannedTime > nLowestTimestamp) {
            std::vector<UniValue> results = response.getValues();
            response.clear();
            response.setArray();
            size_t i = 0;
            for (const UniValue& request : requests.getValues()) {
                // If key creation date is within the successfully scanned
                // range, or if the import result already has an error set, let
                // the result stand unmodified. Otherwise replace the result
                // with an error message.
                if (scannedTime <= GetImportTimestamp(request, now) || results.at(i).exists("error")) {
                    response.push_back(results.at(i));
                } else {
                    UniValue result = UniValue(UniValue::VOBJ);
                    result.pushKV("success", UniValue(false));
                    result.pushKV(
                        "error",
                        JSONRPCError(
                            RPC_MISC_ERROR,
                            strprintf("Rescan failed for key with creation timestamp %d. There was an error reading a "
                                      "block from time %d, which is after or within %d seconds of key creation, and "
                                      "could contain transactions pertaining to the key. As a result, transactions "
                                      "and coins using this key may not appear in the wallet. This error could be "
                                      "caused by pruning or data corruption (see hemp0xd log for details) and could "
                                      "be dealt with by downloading and rescanning the relevant blocks (see -reindex "
                                      "and -rescan options).",
                                GetImportTimestamp(request, now), scannedTime - TIMESTAMP_WINDOW - 1, TIMESTAMP_WINDOW)));
                    response.push_back(std::move(result));
                }
                ++i;
            }
        }
    }

    return response;
}

UniValue exportwalletmigration(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            "exportwalletmigration \"filename\" ( include_private allow_overwrite export_passphrase )\n"
            "\nExports wallet migration data to a JSON envelope file.\n"
            "\nWhen include_private=false (default): produces a public-only envelope.\n"
            "When include_private=true: produces an encrypted private migration envelope\n"
            "for canonical Hemp0x BIP39/BIP44 coin420 wallets only. Non-BIP44, non-BIP39,\n"
            "and non-420 wallets are rejected.\n"
            "\nArguments:\n"
            "1. \"filename\"          (string, required) The output file path (absolute or relative)\n"
            "2. include_private      (boolean, optional, default=false)\n"
            "                         When false: public-only envelope.\n"
            "                         When true: encrypted private envelope (requires export_passphrase).\n"
            "3. allow_overwrite      (boolean, optional, default=false)\n"
            "                         If true, overwrites an existing file.\n"
            "4. export_passphrase    (string, optional, default=empty) Required when include_private=true.\n"
            "                         The passphrase that encrypts the private payload.\n"
            "                         Must be at least 8 characters, at most 1024.\n"
            "\nResult: (public metadata only, NO secrets)\n"
            "{\n"
            "  \"filename\" : \"...\",        (string) Full absolute path of the output file\n"
            "  \"exported_at\" : n,          (numeric) Unix timestamp of export\n"
            "  \"envelope_version\" : n,     (numeric) JSON envelope schema version (1 or 2)\n"
            "  \"chain\" : \"...\",            (string) Network name\n"
            "  \"encrypted\" : true|false,   (boolean) Whether source wallet is encrypted\n"
            "  \"locked\" : true|false,      (boolean) Whether source wallet is locked\n"
            "  \"hd_enabled\" : true|false,  (boolean) Whether HD derivation is active\n"
            "  \"bip44_enabled\" : true|false,(boolean) Whether BIP44 derivation is active\n"
            "  \"private_keys_included\" : true|false, (boolean) Whether private keys were exported\n"
            "  \"keypool_external\" : n,     (numeric) External key pool size\n"
            "  \"warnings\" : [...]          (array of strings) Operational warnings\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("exportwalletmigration", "\"/tmp/migration.json\"")
            + HelpExampleCli("exportwalletmigration", "\"/tmp/migration.json\" false true")
            + HelpExampleCli("exportwalletmigration", "\"/tmp/migration.json\" true false \"my strong passphrase\"")
            + HelpExampleRpc("exportwalletmigration", "\"/tmp/migration.json\"")
        );

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    std::string strFilename = request.params[0].get_str();
    bool fIncludePrivate = request.params.size() >= 2 && !request.params[1].isNull()
        ? request.params[1].get_bool() : false;
    bool fAllowOverwrite = request.params.size() >= 3 && !request.params[2].isNull()
        ? request.params[2].get_bool() : false;
    std::string strExportPassphrase;
    if (request.params.size() >= 4 && !request.params[3].isNull()) {
        strExportPassphrase = request.params[3].get_str();
    }
    auto CleanseExportPassphrase = [&strExportPassphrase]() {
        if (!strExportPassphrase.empty()) {
            memory_cleanse(&strExportPassphrase[0], strExportPassphrase.size());
        }
    };
    auto CleanseBytes = [](std::vector<unsigned char>& bytes) {
        if (!bytes.empty()) {
            memory_cleanse(bytes.data(), bytes.size());
        }
    };

    if (fIncludePrivate && strExportPassphrase.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Export passphrase must not be empty when include_private is true.");
    }
    if (fIncludePrivate && strExportPassphrase.size() < 8) {
        CleanseExportPassphrase();
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Export passphrase must be at least 8 characters.");
    }
    if (fIncludePrivate && strExportPassphrase.size() > 1024) {
        CleanseExportPassphrase();
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "Export passphrase must not exceed 1024 characters.");
    }

    if (fIncludePrivate) {
        if (!pwallet->IsHDEnabled() || !pwallet->GetHDChain().IsBip44()) {
            CleanseExportPassphrase();
            throw JSONRPCError(RPC_WALLET_ERROR,
                "Private migration export requires a canonical Hemp0x BIP39/BIP44 wallet. "
                "This wallet is not BIP44 or does not have mnemonic data.");
        }
        if (!pwallet->HasMnemonicData()) {
            CleanseExportPassphrase();
            throw JSONRPCError(RPC_WALLET_ERROR,
                "Private migration export requires a canonical Hemp0x BIP39/BIP44 wallet. "
                "This wallet does not have BIP39 mnemonic data.");
        }
        if (GetParams().ExtCoinType() != 420) {
            CleanseExportPassphrase();
            throw JSONRPCError(RPC_WALLET_ERROR,
                "Private migration export is only supported for canonical Hemp0x coin type 420. "
                "This chain uses a different coin type.");
        }
        if (pwallet->IsCrypted() && pwallet->IsLocked()) {
            CleanseExportPassphrase();
        }
        EnsureWalletIsUnlocked(pwallet);
    }

    if (strFilename.empty()) {
        if (fIncludePrivate) {
            CleanseExportPassphrase();
        }
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Filename cannot be empty");
    }

    boost::filesystem::path filepath = strFilename;
    filepath = boost::filesystem::absolute(filepath);

    if (boost::filesystem::is_directory(filepath)) {
        if (fIncludePrivate) {
            CleanseExportPassphrase();
        }
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot export to a directory path: " + filepath.string());
    }

    if (filepath.filename().string() == "wallet.dat") {
        if (fIncludePrivate) {
            CleanseExportPassphrase();
        }
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Refusing to write to 'wallet.dat'. Choose a different filename for the migration export.");
    }

    if (boost::filesystem::exists(filepath) && !fAllowOverwrite) {
        if (fIncludePrivate) {
            CleanseExportPassphrase();
        }
        throw JSONRPCError(RPC_INVALID_PARAMETER, filepath.string() + " already exists. Use allow_overwrite=true to overwrite.");
    }

    bool fEncrypted = pwallet->IsCrypted();
    bool fLocked = pwallet->IsLocked();
    bool fHD = pwallet->IsHDEnabled();
    bool fBip44 = pwallet->IsBip44Enabled();
    bool hasMnemonic = pwallet->HasMnemonicData();
    bool hasWatchOnly = pwallet->HaveWatchOnly();
    bool hasPrivateKeys = pwallet->GetKeys().size() > 0;
    size_t kpExternal = pwallet->KeypoolCountExternalKeys();
    CKeyID seed_id = pwallet->GetHDChain().seed_id;
    int64_t nExportTime = GetTime();

    int env_version = fIncludePrivate ? 2 : 1;
    std::string schema_id = fIncludePrivate ?
        "hemp0x-core.migration-envelope.v2" : "hemp0x-core.migration-envelope.v1";

    UniValue envelope(UniValue::VOBJ);
    UniValue warnings(UniValue::VARR);

    envelope.pushKV("envelope_version", env_version);
    envelope.pushKV("schema_identifier", schema_id);
    envelope.pushKV("exported_at", nExportTime);
    envelope.pushKV("source_client", "hemp0x-core");
    envelope.pushKV("source_client_version", FormatFullVersion());

    UniValue chainObj(UniValue::VOBJ);
    chainObj.pushKV("network", GetParams().NetworkIDString());
    chainObj.pushKV("coin_type_bip44", GetParams().ExtCoinType());
    envelope.pushKV("chain", chainObj);

    UniValue walletSummary(UniValue::VOBJ);
    walletSummary.pushKV("wallet_name", pwallet->GetName());
    walletSummary.pushKV("encrypted", UniValue(fEncrypted));
    walletSummary.pushKV("locked", UniValue(fLocked));
    walletSummary.pushKV("hd_enabled", UniValue(fHD));
    walletSummary.pushKV("bip44_enabled", UniValue(fBip44));
    walletSummary.pushKV("private_keys_included", UniValue(fIncludePrivate));
    walletSummary.pushKV("mnemonic_available", UniValue(hasMnemonic));
    walletSummary.pushKV("watch_only_present", UniValue(hasWatchOnly));
    walletSummary.pushKV("private_keys_present", UniValue(hasPrivateKeys));
    walletSummary.pushKV("keypool_external", static_cast<int64_t>(kpExternal));

    if (!seed_id.IsNull() && pwallet->CanSupportFeature(FEATURE_HD_SPLIT)) {
        walletSummary.pushKV("keypool_internal", static_cast<int64_t>(pwallet->GetKeyPoolSize() - kpExternal));
    } else {
        walletSummary.pushKV("keypool_internal", NullUniValue);
    }

    envelope.pushKV("wallet_summary", walletSummary);

    UniValue derivationProfiles(UniValue::VARR);
    if (fHD) {
        UniValue profile(UniValue::VOBJ);
        profile.pushKV("profile_id", fBip44 ? "hemp0x.mainnet.bip44.p2pkh.coin420.v1" : "hemp0x.legacy.bip32.p2pkh.v1");
        profile.pushKV("purpose", fBip44 ? 44 : 0);
        profile.pushKV("coin_type", fBip44 ? GetParams().ExtCoinType() : 0);
        profile.pushKV("address_type", "p2pkh");
        if (fBip44) {
            profile.pushKV("account_derivation_path", strprintf("m/44'/%d'/0'", GetParams().ExtCoinType()));
        }

        if (chainActive.Tip()) {
            profile.pushKV("best_block_height", chainActive.Height());
        }

        derivationProfiles.push_back(profile);
    }
    envelope.pushKV("derivation", derivationProfiles);

    if (fIncludePrivate) {
        std::vector<unsigned char> vchWords;
        std::vector<unsigned char> vchPassphrase;
        std::vector<unsigned char> vchSeed;
        uint256 hash;
        pwallet->GetBip39Data(hash, vchWords, vchPassphrase, vchSeed);
        if (vchWords.empty() || vchSeed.empty()) {
            CleanseExportPassphrase();
            CleanseBytes(vchSeed);
            CleanseBytes(vchWords);
            CleanseBytes(vchPassphrase);
            throw JSONRPCError(RPC_WALLET_ERROR, "Unable to read BIP39 wallet material for migration export.");
        }

        UniValue privatePayload(UniValue::VOBJ);
        privatePayload.pushKV("payload_version", 1);
        privatePayload.pushKV("wallet_type", "bip39_bip44_p2pkh");
        privatePayload.pushKV("network", GetParams().NetworkIDString());
        privatePayload.pushKV("coin_type", GetParams().ExtCoinType());
        privatePayload.pushKV("account", 0);
        privatePayload.pushKV("derivation_profile", "hemp0x.mainnet.bip44.p2pkh.coin420.v1");

        UniValue mnemonicObj(UniValue::VOBJ);
        mnemonicObj.pushKV("language", "english");
        std::string wordsStr(vchWords.begin(), vchWords.end());
        mnemonicObj.pushKV("words", wordsStr);
        privatePayload.pushKV("mnemonic", mnemonicObj);

        std::string passphraseStr(vchPassphrase.begin(), vchPassphrase.end());
        privatePayload.pushKV("mnemonic_passphrase", passphraseStr);

        uint32_t extCounter = pwallet->GetHDChain().nExternalChainCounter;
        uint32_t intCounter = pwallet->GetHDChain().nInternalChainCounter;
        privatePayload.pushKV("external_count_hint", static_cast<int64_t>(extCounter));
        privatePayload.pushKV("change_count_hint", static_cast<int64_t>(intCounter));
        privatePayload.pushKV("exported_at", nExportTime);

        std::string payloadJson = privatePayload.write();
        privatePayload.setNull();
        mnemonicObj.setNull();
        if (!wordsStr.empty()) {
            memory_cleanse(&wordsStr[0], wordsStr.size());
        }
        if (!passphraseStr.empty()) {
            memory_cleanse(&passphraseStr[0], passphraseStr.size());
        }

        std::vector<unsigned char> kdfSalt(MIGRATION_KDF_SALT_SIZE);
        GetStrongRandBytes(kdfSalt.data(), MIGRATION_KDF_SALT_SIZE);

        std::vector<unsigned char> payloadIv(MIGRATION_GCM_IV_SIZE);
        GetStrongRandBytes(payloadIv.data(), MIGRATION_GCM_IV_SIZE);

        std::vector<unsigned char> aad = MigrationBuildAAD(
            schema_id, env_version,
            GetParams().NetworkIDString(),
            GetParams().ExtCoinType(),
            nExportTime, "private-payload");

        std::vector<unsigned char> key = MigrationDeriveKey(
            strExportPassphrase, kdfSalt, MIGRATION_KDF_ITERATIONS);
        if (key.empty()) {
            CleanseExportPassphrase();
            throw JSONRPCError(RPC_WALLET_ERROR, "Key derivation failed.");
        }

        std::vector<unsigned char> payloadPlaintext(payloadJson.begin(), payloadJson.end());
        memory_cleanse(&payloadJson[0], payloadJson.size());

        std::vector<unsigned char> ciphertext;
        std::vector<unsigned char> tag;
        if (!MigrationEncrypt(key, payloadIv, payloadPlaintext, aad, ciphertext, tag)) {
            CleanseBytes(key);
            CleanseBytes(payloadPlaintext);
            CleanseExportPassphrase();
            throw JSONRPCError(RPC_WALLET_ERROR, "Encryption failed.");
        }
        CleanseBytes(payloadPlaintext);

        CleanseExportPassphrase();
        CleanseBytes(key);
        CleanseBytes(vchSeed);
        CleanseBytes(vchWords);
        CleanseBytes(vchPassphrase);

        UniValue encObj(UniValue::VOBJ);
        encObj.pushKV("encrypted", UniValue(true));
        encObj.pushKV("payload_format", "hemp0x-core.private-migration-payload.v1");
        encObj.pushKV("kdf_profile", MIGRATION_KDF_PROFILE);
        encObj.pushKV("kdf_iterations", static_cast<int64_t>(MIGRATION_KDF_ITERATIONS));
        encObj.pushKV("cipher_profile", MIGRATION_CIPHER_PROFILE);
        encObj.pushKV("salt", HexStr(kdfSalt));
        encObj.pushKV("iv", HexStr(payloadIv));
        encObj.pushKV("tag", HexStr(tag));
        encObj.pushKV("aad_profile", "hemp0x-core-migration-aad-v1");
        encObj.pushKV("ciphertext", HexStr(ciphertext));
        envelope.pushKV("private", encObj);

        UniValue keysArray(UniValue::VARR);
        envelope.pushKV("keys", keysArray);

        UniValue watchOnlyArray(UniValue::VARR);
        envelope.pushKV("watch_only_entries", watchOnlyArray);

        UniValue unsupportedArray(UniValue::VARR);
        envelope.pushKV("unsupported_records", unsupportedArray);

        warnings.push_back("Encrypted private migration export completed.");
        warnings.push_back("Anyone with the export passphrase can decrypt and spend all funds.");
        warnings.push_back("Store the passphrase separately from this file.");
        warnings.push_back("Always verify the source wallet.dat remains intact before deleting this export.");
    } else {
        UniValue keysArray(UniValue::VARR);
        envelope.pushKV("keys", keysArray);

        UniValue watchOnlyArray(UniValue::VARR);
        envelope.pushKV("watch_only_entries", watchOnlyArray);

        UniValue unsupportedArray(UniValue::VARR);
        envelope.pushKV("unsupported_records", unsupportedArray);

        warnings.push_back("This file contains PUBLIC wallet metadata only. No private keys, mnemonics, seeds, xprv values, or passphrases are included.");
        warnings.push_back("The 'keys' array is intentionally empty in this public-only export. Full key iteration requires a future build.");
        warnings.push_back("For private key export, use include_private=true with an export passphrase.");
        if (fEncrypted && fLocked) {
            warnings.push_back("Wallet is encrypted and locked. Private key export would require walletpassphrase.");
        }
    }

    UniValue metadata(UniValue::VOBJ);
    if (chainActive.Tip()) {
        metadata.pushKV("best_block_height", chainActive.Height());
        metadata.pushKV("best_block_hash", chainActive.Tip()->GetBlockHash().GetHex());
    }
    metadata.pushKV("transaction_count", static_cast<int64_t>(pwallet->mapWallet.size()));
    metadata.pushKV("balance", ValueFromAmount(pwallet->GetBalance()));
    envelope.pushKV("metadata", metadata);

    envelope.pushKV("warnings", warnings);

    std::string jsonContent = envelope.write(2);

    boost::filesystem::path tempPath = filepath.parent_path() /
        (filepath.filename().string() + "." + boost::filesystem::unique_path("%%%%%%").string() + ".tmp");

    std::ofstream file;
    file.open(tempPath.string().c_str(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open temporary output file: " + tempPath.string());
    }

    file << jsonContent;
    if (file.fail()) {
        file.close();
        boost::filesystem::remove(tempPath);
        throw JSONRPCError(RPC_WALLET_ERROR, "Error writing migration data to temporary file");
    }
    file.close();

    if (fAllowOverwrite && boost::filesystem::exists(filepath)) {
        boost::system::error_code ec;
        boost::filesystem::remove(filepath, ec);
        if (ec) {
            boost::filesystem::remove(tempPath);
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to remove existing destination before overwrite: " + filepath.string());
        }
    }

    if (std::rename(tempPath.string().c_str(), filepath.string().c_str()) != 0) {
        boost::filesystem::remove(tempPath);
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to rename temporary file to final destination: " + filepath.string());
    }

    UniValue reply(UniValue::VOBJ);
    reply.pushKV("filename", filepath.string());
    reply.pushKV("exported_at", nExportTime);
    reply.pushKV("envelope_version", env_version);
    reply.pushKV("chain", GetParams().NetworkIDString());
    reply.pushKV("encrypted", UniValue(fEncrypted));
    reply.pushKV("locked", UniValue(fLocked));
    reply.pushKV("hd_enabled", UniValue(fHD));
    reply.pushKV("bip44_enabled", UniValue(fBip44));
    reply.pushKV("private_keys_included", UniValue(fIncludePrivate));
    reply.pushKV("mnemonic_available", UniValue(hasMnemonic));
    reply.pushKV("watch_only_present", UniValue(hasWatchOnly));
    reply.pushKV("private_keys_present", UniValue(hasPrivateKeys));
    reply.pushKV("total_keys_exported", 0);
    reply.pushKV("keypool_external", static_cast<int64_t>(kpExternal));
    reply.pushKV("warnings", warnings);

    return reply;
}
