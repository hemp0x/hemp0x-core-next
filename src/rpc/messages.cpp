// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets/assets.h"
#include "assets/assetdb.h"
#include "assets/messages.h"
#include "assets/myassetsdb.h"
#include <map>
#include <vector>
#include <algorithm>
#include "tinyformat.h"

#include "amount.h"
#include "base58.h"
#include "chain.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "httpserver.h"
#include "init.h"
#include "validation.h"
#include "undo.h"
#include "net.h"
#include "policy/feerate.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "policy/rbf.h"
#include "rpc/mining.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "script/sign.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet/coincontrol.h"
#include "wallet/feebumper.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

std::string MessageActivationWarning()
{
    return AreMessagesDeployed() ? "" : "\nTHIS COMMAND IS NOT YET ACTIVE!\nhttps://github.com/Hemp0xProject/rips/blob/master/rip-0005.mediawiki\n";
}

static std::string DeriveAuthorityAsset(const std::string& strName)
{
    size_t tildePos = strName.find('~');
    if (tildePos != std::string::npos) {
        return strName.substr(0, tildePos) + OWNER_TAG;
    }
    return strName;
}

static std::string GetMessageBlockHash(const CMessage& message)
{
    if (message.nBlockHeight <= 0)
        return "";

    if (message.status == MessageStatus::ORPHAN)
        return "";

    LOCK(cs_main);
    if (message.nBlockHeight > chainActive.Height())
        return "";

    CBlockIndex* pindex = chainActive[message.nBlockHeight];
    return pindex ? pindex->GetBlockHash().ToString() : "";
}

static bool NormalizeMessageChannel(std::string& channel)
{
    AssetType type;
    if (!IsAssetNameValid(channel, type))
        return false;

    if (type == AssetType::ROOT || type == AssetType::SUB || type == AssetType::RESTRICTED) {
        channel += OWNER_TAG;
        if (!IsAssetNameValid(channel, type))
            return false;
    }

    return type == AssetType::OWNER || type == AssetType::MSGCHANNEL;
}

// Stable ordering for paginated reads: by block height, then by outpoint
// (txid, vout). Falls back to outpoint ordering when heights are equal.
static bool MessageHeightOrder(const CMessage& a, const CMessage& b)
{
    if (a.nBlockHeight != b.nBlockHeight)
        return a.nBlockHeight < b.nBlockHeight;
    return a.out < b.out;
}

// Extract on-chain asset messages from a block's transaction outputs for the
// rescan path. Mirrors the message-population logic in ConnectBlock/CheckTxAssets
// (owner or message-channel asset transfers carrying a non-empty, non-expired
// message payload).
//
// R3: rescan is exact-by-default. The sender-ownership filter from
// Consensus::CheckTxAssets is ALWAYS reapplied here, reconstructed from the
// block's CBlockUndo (spent Coin records). pblockUndo must be non-null and
// valid; this function validates the undo shape and parser invariants and
// returns false with a descriptive error if anything is inconsistent, so the
// caller fails the rescan clearly instead of producing a partial/inexact index.
//
// On success returns true. On failure returns false and sets strError with a
// message identifying the block height and the cause.
static bool ExtractMessagesFromBlock(const CBlock& block, int nHeight, int64_t nBlockTime,
                                     const CBlockUndo* pblockUndo,
                                     std::vector<CMessage>& vMessages,
                                     std::string& strError)
{
    if (!pblockUndo) {
        // The caller is responsible for reading undo data; reaching here with a
        // null pointer is an internal programming error for non-coinbase blocks.
        strError = strprintf("undo data pointer is null for block %d", nHeight);
        return false;
    }

    // Validate undo shape: one CTxUndo per non-coinbase transaction.
    if (block.vtx.size() < 1) {
        strError = strprintf("block %d has no transactions", nHeight);
        return false;
    }
    if (pblockUndo->vtxundo.size() != block.vtx.size() - 1) {
        strError = strprintf("undo shape mismatch at block %d: vtxundo size %u does not match non-coinbase tx count %u",
                             nHeight, pblockUndo->vtxundo.size(), block.vtx.size() - 1);
        return false;
    }

    for (size_t txi = 0; txi < block.vtx.size(); ++txi) {
        const auto& tx = block.vtx[txi];
        if (!tx) {
            strError = strprintf("null transaction ref at index %u in block %d", txi, nHeight);
            return false;
        }
        const uint256 txhash = tx->GetHash();

        // Build the per-transaction input asset address map from undo data,
        // mirroring Consensus::CheckTxAssets. Coinbase (txi==0) has no undo
        // entry and no spendable inputs.
        std::map<std::string, std::string> mapAddresses;
        if (txi > 0) {
            const CTxUndo& txundo = pblockUndo->vtxundo[txi - 1];
            // The undo vector should align with the transaction's inputs.
            if (txundo.vprevout.size() != tx->vin.size()) {
                strError = strprintf("undo input count mismatch at block %d tx %u: undo %u vs vin %u",
                                     nHeight, txi, txundo.vprevout.size(), tx->vin.size());
                return false;
            }
            for (size_t j = 0; j < txundo.vprevout.size(); ++j) {
                const Coin& coin = txundo.vprevout[j];
                if (coin.IsSpent())
                    continue;
                if (!coin.IsAsset())
                    continue;
                CAssetOutputEntry data;
                if (!GetAssetData(coin.out.scriptPubKey, data)) {
                    // A valid connected block should have parseable asset scripts
                    // in its spent asset coins. Treat this as an extraction error
                    // rather than silently skipping, so rescan never produces an
                    // inexact index.
                    strError = strprintf("GetAssetData failed for spent asset coin at block %d tx %u input %u",
                                         nHeight, txi, j);
                    return false;
                }
                // std::map::insert is first-wins, matching CheckTxAssets.
                mapAddresses.insert(std::make_pair(data.assetName, EncodeDestination(data.destination)));
            }
        }

        int index = 0;
        for (const auto& txout : tx->vout) {
            int nType = -1;
            bool fOwner = false;
            if (!txout.scriptPubKey.IsAssetScript(nType, fOwner)) {
                index++;
                continue;
            }

            if (nType != TX_TRANSFER_ASSET) {
                index++;
                continue;
            }

            CAssetTransfer transfer;
            std::string strAddress;
            if (!TransferAssetFromScript(txout.scriptPubKey, transfer, strAddress)) {
                // The output was identified as a TX_TRANSFER_ASSET asset script
                // but the transfer payload could not be parsed. A valid connected
                // block should not have this condition. Treat it as an extraction
                // error so exact replay never silently hides a parse failure.
                strError = strprintf("TransferAssetFromScript failed for transfer output at block %d tx %u vout %d",
                                     nHeight, txi, index);
                return false;
            }

            if (transfer.message.empty()) {
                index++;
                continue;
            }

            if (!(IsAssetNameAnOwner(transfer.strName) || IsAssetNameAnMsgChannel(transfer.strName))) {
                index++;
                continue;
            }

            // Use the block time as the expiry reference so rescan is deterministic
            // and matches what live indexing would have stored when the block
            // originally connected (block time ~= wall time at connect).
            if (transfer.nExpireTime != 0 && transfer.nExpireTime <= nBlockTime) {
                index++;
                continue;
            }

            // Apply the sender-ownership filter (always, since undo is required):
            // only extract the message if one of this tx's inputs was the same
            // asset sent to the same address (the sender owns the channel asset
            // they are messaging on). This matches CheckTxAssets exactly.
            auto it = mapAddresses.find(transfer.strName);
            if (it == mapAddresses.end() || it->second != strAddress) {
                index++;
                continue;
            }

            COutPoint out(txhash, index);
            CMessage message(out, transfer.strName, transfer.message, transfer.nExpireTime, nBlockTime);
            message.nBlockHeight = nHeight;
            vMessages.push_back(message);
            index++;
        }
    }

    return true;
}

static void LoadMessagesForRPC(std::set<CMessage>& setMessages)
{
    pmessagedb->LoadMessages(setMessages);

    LOCK(cs_messaging);

    for (const auto& pair : mapDirtyMessagesOrphaned) {
        CMessage message = pair.second;
        message.status = MessageStatus::ORPHAN;
        if (setMessages.count(message))
            setMessages.erase(message);
        setMessages.insert(message);
    }

    for (const auto& out : setDirtyMessagesRemove) {
        CMessage message;
        message.out = out;
        setMessages.erase(message);
    }

    for (const auto& pair : mapDirtyMessagesAdd) {
        setMessages.erase(pair.second);
        setMessages.insert(pair.second);
    }
}

static UniValue MessageEntryToJSON(const CMessage& message, const std::string& block_hash = "")
{
    UniValue obj(UniValue::VOBJ);

    obj.push_back(Pair("Asset Name", message.strName));
    obj.push_back(Pair("Message", EncodeAssetData(message.ipfsHash)));
    obj.push_back(Pair("Time", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", message.time)));
    obj.push_back(Pair("Block Height", message.nBlockHeight));
    obj.push_back(Pair("Status", MessageStatusToString(message.status)));
    try {
        std::string date = DateTimeStrFormat("%Y-%m-%d %H:%M:%S", message.nExpiredTime);
        if (message.nExpiredTime)
            obj.push_back(Pair("Expire Time", date));
    } catch (...) {
        obj.push_back(Pair("Expire UTC Time", message.nExpiredTime));
    }

    obj.push_back(Pair("txid", message.out.hash.ToString()));
    obj.push_back(Pair("vout", static_cast<int>(message.out.n)));
    obj.push_back(Pair("channel", message.strName));
    obj.push_back(Pair("authority_asset", DeriveAuthorityAsset(message.strName)));
    obj.push_back(Pair("authority_address", ""));
    obj.push_back(Pair("block_hash", block_hash));
    obj.push_back(Pair("sender_address", ""));

    return obj;
}

UniValue getmessaginginfo(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "getmessaginginfo\n"
                "\nReturns the current messaging subsystem state, including the optional\n"
                "full message index (-messageindex=1) state, sync height, rescan need,\n"
                "and any in-progress rescan progress.\n"
                "\nResult:\n"
                "{\n"
                "  \"enabled\" : true|false,           (boolean) Whether messaging is enabled (-disablemessaging not set)\n"
                "  \"messaging_active\" : true|false,   (boolean) Whether messaging BIP9 deployment is active\n"
                "  \"restricted_active\" : true|false,  (boolean) Whether restricted assets are active\n"
                "  \"activation_block\" : n,            (numeric) Messaging activation block height (0 if not set)\n"
                "  \"databases_available\" : true|false,(boolean) Whether message and channel databases are accessible\n"
                "  \"caches_available\" : true|false,   (boolean) Whether in-memory caches are available\n"
                "  \"message_count\" : n,               (numeric or null) Approximate number of stored messages\n"
                "  \"channel_count\" : n,               (numeric or null) Approximate number of subscribed channels\n"
                "  \"dirty_cache_size_bytes\" : n,      (numeric) Estimated size of dirty cache entries in bytes\n"
                "  \"wallet_available\" : true|false,   (boolean) Whether a wallet is loaded\n"
                "  \"messageindex\" : true|false,       (boolean) Whether full message indexing (-messageindex=1) is enabled\n"
                "  \"message_index_mode\" : \"all|subscription\", (string) \"all\" when -messageindex=1, else \"subscription\"\n"
                "  \"message_index_activation_height\" : n, (numeric) Messaging activation height used as default rescan start\n"
                "  \"message_index_synced_height\" : n, (numeric or null) Last block height indexed for messages under -messageindex=1\n"
                "  \"message_index_best_height\" : n,   (numeric) Current active-chain height\n"
                "  \"message_index_synced\" : true|false,(boolean) Whether the full index is caught up to the chain tip\n"
                "  \"message_index_needs_rescan\" : true|false, (boolean) Whether a backfill via rescanmessages is recommended\n"
                "  \"message_index_last_scan_start_height\" : n, (numeric or null) Last explicit rescan start height\n"
                "  \"message_index_last_scan_stop_height\" : n, (numeric or null) Last explicit rescan stop height\n"
                "  \"message_index_last_scan_completed\" : true|false, (boolean or null) Whether the last rescan finished successfully\n"
                "  \"message_rescan_in_progress\" : true|false, (boolean) Whether a rescanmessages scan is currently running\n"
                "  \"message_rescan_start_height\" : n,  (numeric) In-progress rescan start height\n"
                "  \"message_rescan_stop_height\" : n,   (numeric) In-progress rescan stop height\n"
                "  \"message_rescan_current_height\" : n,(numeric) In-progress rescan current height\n"
                "  \"message_rescan_scanned_blocks\" : n,(numeric) In-progress rescan blocks scanned\n"
                "  \"message_rescan_messages_found\" : n,(numeric) In-progress rescan messages found\n"
                "  \"message_rescan_messages_added\" : n,(numeric) In-progress rescan messages added\n"
                "  \"message_rescan_last_error\" : str,  (string) Last rescan error, if any\n"
                "  \"message_rescan_started_at\" : n,    (numeric) Unix time the in-progress/last rescan started\n"
                "  \"message_rescan_finished_at\" : n,   (numeric) Unix time the last rescan finished, if any\n"
                "  \"pruned\" : true|false,              (boolean) Whether the node is running in prune mode\n"
                "  \"warnings\" : [...]                 (array of strings) Any warnings about the messaging state\n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli("getmessaginginfo", "")
                + HelpExampleRpc("getmessaginginfo", "")
        );

    UniValue obj(UniValue::VOBJ);
    UniValue warnings(UniValue::VARR);

    obj.pushKV("enabled", fMessaging);
    obj.pushKV("messaging_active", AreMessagesDeployed());
    obj.pushKV("restricted_active", AreRestrictedAssetsDeployed());
    obj.pushKV("activation_block", static_cast<int>(GetParams().MessagingActivationBlock()));

    bool dbsAvailable = (pmessagedb != nullptr) && (pmessagechanneldb != nullptr);
    obj.pushKV("databases_available", dbsAvailable);
    if (!dbsAvailable)
        warnings.push_back("One or more messaging databases are not available");

    bool cachesAvailable = (pMessagesCache != nullptr) && (pMessageSubscribedChannelsCache != nullptr);
    obj.pushKV("caches_available", cachesAvailable);
    if (!cachesAvailable)
        warnings.push_back("One or more messaging caches are not available");

    if (dbsAvailable && cachesAvailable) {
        std::set<CMessage> setMessages;
        pmessagedb->LoadMessages(setMessages);

        int msgCount = setMessages.size();
        {
            LOCK(cs_messaging);
            for (auto pair : mapDirtyMessagesOrphaned) {
                CMessage message = pair.second;
                if (setMessages.count(message))
                    setMessages.erase(message);
                setMessages.insert(message);
            }
            for (auto out : setDirtyMessagesRemove) {
                CMessage message;
                message.out = out;
                setMessages.erase(message);
            }
            for (auto pair : mapDirtyMessagesAdd) {
                setMessages.erase(pair.second);
                setMessages.insert(pair.second);
            }
            msgCount = setMessages.size();
        }
        obj.pushKV("message_count", msgCount);

        std::set<std::string> setChannels;
        pmessagechanneldb->LoadMyMessageChannels(setChannels);
        {
            LOCK(cs_messaging);
            for (auto name : setDirtyChannelsRemove)
                setChannels.erase(name);
            for (auto name : setDirtyChannelsAdd)
                setChannels.insert(name);
        }
        obj.pushKV("channel_count", static_cast<int>(setChannels.size()));
    } else {
        obj.pushKV("message_count", NullUniValue);
        obj.pushKV("channel_count", NullUniValue);
    }

    obj.pushKV("dirty_cache_size_bytes", static_cast<int64_t>(GetMessageDirtyCacheSize()));

    bool wallet_available = false;
#ifdef ENABLE_WALLET
    {
        LOCK(cs_wallets);
        wallet_available = vpwallets.size() > 0;
    }
#endif
    obj.pushKV("wallet_available", wallet_available);

    if (!fMessaging)
        warnings.push_back("Messaging is disabled via -disablemessaging");

    if (AreMessagesDeployed() && dbsAvailable && cachesAvailable && fMessaging && !wallet_available)
        warnings.push_back("Messaging is active but no wallet is loaded; channel scanning requires a wallet");

    // --- Optional full message index (-messageindex=1) state ---
    obj.pushKV("messageindex", fMessageIndex);
    obj.pushKV("message_index_mode", fMessageIndex ? std::string("all") : std::string("subscription"));

    const unsigned int nActivationHeight = GetParams().MessagingActivationBlock();
    obj.pushKV("message_index_activation_height", static_cast<int>(nActivationHeight));

    int nBestHeight = 0;
    {
        LOCK(cs_main);
        nBestHeight = chainActive.Height();
    }
    obj.pushKV("message_index_best_height", nBestHeight);

    const int nSyncedHeight = GetMessageIndexSyncedHeight();
    if (fMessageIndex) {
        obj.pushKV("message_index_synced_height", nSyncedHeight);
    } else {
        obj.pushKV("message_index_synced_height", NullUniValue);
    }

    // Synced: only meaningful under full-index mode. Treat as synced when the
    // index has reached the active-chain tip (or messaging is not yet active).
    bool fIndexSynced = false;
    if (fMessageIndex && AreMessagesDeployed() && dbsAvailable && cachesAvailable) {
        fIndexSynced = (nSyncedHeight >= nBestHeight) || (nBestHeight <= static_cast<int>(nActivationHeight));
    }
    obj.pushKV("message_index_synced", fIndexSynced);

    // Needs-rescan: full indexing is on, chain has post-activation history, and
    // the index has not caught up. Because the synced height is advanced on
    // every ConnectBlock (before UpdateTip), it tracks the active tip exactly
    // during normal operation and initial sync, so a real gap only appears after
    // enabling -messageindex=1 over existing history, clearing the message DB,
    // or a stale/missing synced-height marker.
    bool fNeedsRescan = false;
    if (fMessageIndex && AreMessagesDeployed() && dbsAvailable && cachesAvailable &&
        nBestHeight > static_cast<int>(nActivationHeight) &&
        nSyncedHeight < nBestHeight) {
        fNeedsRescan = true;
    }
    obj.pushKV("message_index_needs_rescan", fNeedsRescan);

    // Persisted last explicit rescan metadata.
    if (pmessagedb) {
        int64_t nLastStart = 0, nLastStop = 0;
        bool fLastCompleted = false;
        if (pmessagedb->ReadMetaInt64(MSG_META_LAST_SCAN_START, nLastStart))
            obj.pushKV("message_index_last_scan_start_height", static_cast<int>(nLastStart));
        else
            obj.pushKV("message_index_last_scan_start_height", NullUniValue);

        if (pmessagedb->ReadMetaInt64(MSG_META_LAST_SCAN_STOP, nLastStop))
            obj.pushKV("message_index_last_scan_stop_height", static_cast<int>(nLastStop));
        else
            obj.pushKV("message_index_last_scan_stop_height", NullUniValue);

        if (pmessagedb->ReadFlag(MSG_META_LAST_SCAN_COMPLETED, fLastCompleted))
            obj.pushKV("message_index_last_scan_completed", fLastCompleted);
        else
            obj.pushKV("message_index_last_scan_completed", NullUniValue);
    } else {
        obj.pushKV("message_index_last_scan_start_height", NullUniValue);
        obj.pushKV("message_index_last_scan_stop_height", NullUniValue);
        obj.pushKV("message_index_last_scan_completed", NullUniValue);
    }

    // In-progress (or most-recent) rescan progress snapshot.
    MessageRescanProgress progress;
    GetMessageRescanProgress(progress);
    obj.pushKV("message_rescan_in_progress", progress.fInProgress);
    obj.pushKV("message_rescan_start_height", progress.nStartHeight);
    obj.pushKV("message_rescan_stop_height", progress.nStopHeight);
    obj.pushKV("message_rescan_current_height", progress.nCurrentHeight);
    obj.pushKV("message_rescan_scanned_blocks", progress.nScannedBlocks);
    obj.pushKV("message_rescan_messages_found", progress.nMessagesFound);
    obj.pushKV("message_rescan_messages_added", progress.nMessagesAdded);
    obj.pushKV("message_rescan_last_error", progress.strLastError);
    obj.pushKV("message_rescan_started_at", progress.nStartedAt);
    obj.pushKV("message_rescan_finished_at", progress.nFinishedAt);

    obj.pushKV("pruned", fPruneMode);
    if (fPruneMode && fMessageIndex && AreMessagesDeployed())
        warnings.push_back("Node is pruned; historical all-message rescans may be limited to blocks still on disk. New messages are still indexed as blocks connect.");

    if (fMessageIndex && AreMessagesDeployed() && dbsAvailable && cachesAvailable && fNeedsRescan)
        warnings.push_back("Full message indexing is enabled but the message index is behind the chain tip. Run rescanmessages to backfill history.");

    obj.pushKV("warnings", warnings);

    return obj;
}

UniValue viewallmessages(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() > 5)
        throw std::runtime_error(
                "viewallmessages ( count ) ( offset ) ( \"channel/pattern\" ) ( start_height ) ( stop_height )\n"
                + MessageActivationWarning() +
                "\nView all messages that the node contains. With -messageindex=1 this\n"
                "includes every indexed on-chain asset message; with the default\n"
                "subscription mode it includes only subscribed-channel messages.\n"
                "\nArguments (all optional; omitted arguments preserve historical behavior):\n"
                "1. count              (numeric, optional, default=0) Maximum number of messages to return. 0 = return all.\n"
                "2. offset             (numeric, optional, default=0) Number of leading messages to skip (stable cursor).\n"
                "3. \"channel/pattern\"  (string, optional) Filter by channel. Supports '*' glob (e.g. \"*/H0XC!\").\n"
                "                              A bare asset name (ROOT or ROOT/SUB) is normalized to its owner channel (ROOT!).\n"
                "4. start_height       (numeric, optional, default=0) Only include messages at or above this block height. 0 = no lower bound.\n"
                "5. stop_height        (numeric, optional, default=0) Only include messages at or below this block height. 0 = no upper bound.\n"
                "\nWhen no arguments are supplied, behavior and ordering are identical to\n"
                "previous releases (all stored messages in database order). When any\n"
                "filter/pagination argument is supplied, results are ordered by block\n"
                "height then txid/vout for stable paging.\n"
                "\nResult:\n"
                "\"Asset Name:\"                     (string) The name of the asset the message was sent on\n"
                "\"Message:\"                        (string) The IPFS hash of the message\n"
                "\"Time:\"                           (Date) The time as a date in the format (YY-mm-dd Hour-minute-second)\n"
                "\"Block Height:\"                   (number) The height of the block the message was included in\n"
                "\"Status:\"                         (string) Status of the message (READ, UNREAD, ORPHAN, EXPIRED, SPAM, HIDDEN, ERROR)\n"
                "\"Expire Time:\"                    (Date, optional) If the message had an expiration date assigned, it will be shown here in the format (YY-mm-dd Hour-minute-second)\n"
                "\"Expire UTC Time:\"                (Date, optional) If the message contains an expire date that is too large, the UTC number will be displayed\n"
                "\"txid:\"                           (string) The transaction id of the message\n"
                "\"channel:\"                        (string) The display channel / asset channel the message belongs to\n"
                "\"authority_asset:\"                (string) The message authority asset (e.g. ROOT/H0XC!), if known\n"
                "\"authority_address:\"              (string) Authority address (currently unavailable, returns \"\")\n"
                "\"block_hash:\"                     (string) Block hash (populated when filtering/paging is used; empty otherwise for backward compatibility)\n"
                "\"sender_address:\"                 (string) Sender address (currently unavailable from cached data, returns \"\")\n"
                "\nExamples:\n"
                + HelpExampleCli("viewallmessages", "")
                + HelpExampleCli("viewallmessages", "200 0 \"*/H0XC!\" 270144 null")
                + HelpExampleRpc("viewallmessages", "")
        );

    if (!fMessaging)
        return UniValue(UniValue::VARR);

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        return UniValue(UniValue::VARR);

    const bool fHasArgs = request.params.size() > 0;

    int nCount = 0;
    int nOffset = 0;
    std::string channelPattern;
    bool fUseChannelFilter = false;
    bool fChannelIsGlob = false;
    int nStartHeight = 0;
    int nStopHeight = 0;

    if (request.params.size() > 0 && !request.params[0].isNull())
        nCount = request.params[0].get_int();
    if (request.params.size() > 1 && !request.params[1].isNull())
        nOffset = request.params[1].get_int();
    if (request.params.size() > 2 && !request.params[2].isNull()) {
        channelPattern = request.params[2].get_str();
        fUseChannelFilter = !channelPattern.empty();
        fChannelIsGlob = channelPattern.find('*') != std::string::npos;
        if (fUseChannelFilter && !fChannelIsGlob) {
            if (!NormalizeMessageChannel(channelPattern))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel must be an owner asset or message channel asset.");
        }
    }
    if (request.params.size() > 3 && !request.params[3].isNull())
        nStartHeight = request.params[3].get_int();
    if (request.params.size() > 4 && !request.params[4].isNull())
        nStopHeight = request.params[4].get_int();

    if (nOffset < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "offset must be >= 0");
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "count must be >= 0");

    std::set<CMessage> setMessages;
    LoadMessagesForRPC(setMessages);

    UniValue messages(UniValue::VARR);

    // Backward-compatible path: no arguments -> return all in existing DB order.
    if (!fHasArgs) {
        for (const auto& message : setMessages) {
            messages.push_back(MessageEntryToJSON(message));
        }
        return messages;
    }

    // Filtered / paginated path: stable order by (height, outpoint).
    std::vector<CMessage> vFiltered;
    vFiltered.reserve(setMessages.size());
    for (const auto& message : setMessages) {
        if (fUseChannelFilter) {
            if (fChannelIsGlob) {
                if (!GlobMatchChannel(channelPattern, message.strName))
                    continue;
            } else if (message.strName != channelPattern) {
                continue;
            }
        }
        if (nStartHeight > 0 && message.nBlockHeight < nStartHeight)
            continue;
        if (nStopHeight > 0 && message.nBlockHeight > nStopHeight)
            continue;
        vFiltered.push_back(message);
    }

    std::sort(vFiltered.begin(), vFiltered.end(), MessageHeightOrder);

    if (nOffset >= static_cast<int>(vFiltered.size()))
        return messages;

    int nRemaining = static_cast<int>(vFiltered.size()) - nOffset;
    int nToReturn = (nCount == 0) ? nRemaining : std::min(nCount, nRemaining);

    for (int i = 0; i < nToReturn; ++i) {
        const CMessage& message = vFiltered[nOffset + i];
        messages.push_back(MessageEntryToJSON(message, GetMessageBlockHash(message)));
    }

    return messages;
}

UniValue viewallmessagechannels(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "viewallmessagechannels \n"
                + MessageActivationWarning() +
                "\nView all message channels the wallet is subscribed to\n"
                "\nResult:[\n"
                "\"Asset Name\"                      (string) The asset channel name\n"
                "\n]\n"
                "\nExamples:\n"
                + HelpExampleCli("viewallmessagechannels", "")
                + HelpExampleRpc("viewallmessagechannels", "")
        );

    if (!fMessaging)
        return UniValue(UniValue::VARR);

    if (!pMessageSubscribedChannelsCache || !pmessagechanneldb)
        return UniValue(UniValue::VARR);

    std::set<std::string> setChannels;
    pmessagechanneldb->LoadMyMessageChannels(setChannels);

    {
        LOCK(cs_messaging);

        for (auto name : setDirtyChannelsRemove)
            setChannels.erase(name);

        for (auto name : setDirtyChannelsAdd)
            setChannels.insert(name);
    }

    UniValue channels(UniValue::VARR);

    for (auto name : setChannels)
        channels.push_back(name);

    return channels;
}

UniValue subscribetochannel(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                "subscribetochannel \n"
                + MessageActivationWarning() +
                "\nSubscribe to a certain message channel\n"
                "\nArguments:\n"
                "1. \"channel_name\"            (string, required) The channel name to subscribe to, it must end with '!' or have an '~' in the name\n"
                "\nResult:[\n"
                "\n]\n"
                "\nExamples:\n"
                + HelpExampleCli("subscribetochannel", "\"ASSET_NAME!\"")
                + HelpExampleRpc("subscribetochannel", "\"ASSET_NAME!\"")
        );

    if (!fMessaging)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Messaging is disabled. To enable messaging, run the wallet without -disablemessaging or remove disablemessaging from your hemp0x.conf");

    if (!pMessageSubscribedChannelsCache || !pmessagechanneldb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Message database isn't setup");

    std::string channel_name = request.params[0].get_str();

    AssetType type;
    if (!IsAssetNameValid(channel_name, type))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel Name is not valid.");

    if (type == AssetType::ROOT || type == AssetType::SUB) {
        channel_name += "!";
        if (!IsAssetNameValid(channel_name, type))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel Name is not valid.");
    }

    if (type != AssetType::OWNER && type != AssetType::MSGCHANNEL)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel Name must be a owner asset, or a message channel asset e.g OWNER!, MSG_CHANNEL~123.");

    AddChannel(channel_name);

    return "Subscribed to channel: " + channel_name;
}

UniValue unsubscribefromchannel(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                "unsubscribefromchannel \n"
                + MessageActivationWarning() +
                "\nUnsubscribe from a certain message channel\n"
                "\nArguments:\n"
                "1. \"channel_name\"            (string, required) The channel name to unsubscribe from, must end with '!' or have an '~' in the name\n"
                "\nResult:[\n"
                "\n]\n"
                "\nExamples:\n"
                + HelpExampleCli("unsubscribefromchannel", "\"ASSET_NAME!\"")
                + HelpExampleRpc("unsubscribefromchannel", "\"ASSET_NAME!\"")
        );

    if (!fMessaging)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Messaging is disabled. To enable messaging, run the wallet without -disablemessaging or remove disablemessaging from your hemp0x.conf");

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessageSubscribedChannelsCache || !pmessagechanneldb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Message database isn't setup");

    std::string channel_name = request.params[0].get_str();

    AssetType type;
    if (!IsAssetNameValid(channel_name, type))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel Name is not valid.");

    if (type == AssetType::ROOT || type == AssetType::SUB) {
        channel_name += "!";

        if (!IsAssetNameValid(channel_name, type))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel Name is not valid.");
    }

    if (type != AssetType::OWNER && type != AssetType::MSGCHANNEL)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel Name must be a owner asset, or a message channel asset e.g OWNER!, MSG_CHANNEL~123.");

    RemoveChannel(channel_name);

    return "Unsubscribed from channel: " + channel_name;
}

UniValue clearmessages(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "clearmessages \n"
                + MessageActivationWarning() +
                "\nDelete current database of messages\n"
                "\nResult:[\n"
                "\n]\n"
                "\nExamples:\n"
                + HelpExampleCli("clearmessages", "")
                + HelpExampleRpc("clearmessages", "")
        );

    if (!fMessaging)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Messaging is disabled. To enable messaging, run the wallet without -disablemessaging or remove disablemessaging from your hemp0x.conf");

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Message database isn't setup");

    int count = 0;
    {
        LOCK(cs_messaging);
        count += mapDirtyMessagesAdd.size();
        pMessagesCache->Clear();
        setDirtyMessagesRemove.clear();
        mapDirtyMessagesAdd.clear();
        mapDirtyMessagesOrphaned.clear();
    }
    pmessagedb->EraseAllMessages(count);

    // Clearing the message DB invalidates the full-index synced-height marker so
    // that getmessaginginfo can correctly report message_index_needs_rescan and
    // Commander can offer a recovery action.
    ResetMessageIndexMetadata();
    ClearMessageRescanProgress();

    return "Erased " + std::to_string(count) + " Messages from the database and cache";
}

UniValue viewchannelmessages(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 5)
        throw std::runtime_error(
                "viewchannelmessages \"channel\" ( count ) ( offset ) ( start_height ) ( stop_height )\n"
                + MessageActivationWarning() +
                "\nView messages for a specific channel.\n"
                "\nArguments:\n"
                "1. \"channel\"              (string, required) The channel to view messages for.\n"
                "                              Root/sub/restricted asset names are normalized to their owner asset form.\n"
                "                              Examples: ROOT -> ROOT!, ROOT/SUB -> ROOT/SUB!, $ASSET -> $ASSET!.\n"
                "                              Owner assets and valid message-channel assets are used as-is.\n"
                "                              Examples: ROOT! or ROOT~ANNOUNCEMENTS.\n"
                "2. count                  (numeric, optional, default=0) Maximum number of messages to return. 0 = return all.\n"
                "3. offset                 (numeric, optional, default=0) Number of leading messages to skip (stable cursor).\n"
                "4. start_height           (numeric, optional, default=0) Only include messages at or above this block height. 0 = no lower bound.\n"
                "5. stop_height            (numeric, optional, default=0) Only include messages at or below this block height. 0 = no upper bound.\n"
                "\nWhen only the channel is supplied, behavior and ordering are identical to\n"
                "previous releases. When count/offset or height bounds are supplied, results\n"
                "are ordered by block height then txid/vout for stable paging.\n"
                "\nResult: same per-message fields as viewallmessages.\n"
                "\nExamples:\n"
                + HelpExampleCli("viewchannelmessages", "ROOT/H0XC!")
                + HelpExampleCli("viewchannelmessages", "ROOT/H0XC 100 0")
                + HelpExampleRpc("viewchannelmessages", "\"ROOT/H0XC!\"")
        );

    if (!fMessaging)
        return UniValue(UniValue::VARR);

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        return UniValue(UniValue::VARR);

    std::string channel = request.params[0].get_str();

    if (!NormalizeMessageChannel(channel))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel must be an owner asset or message channel asset.");

    const bool fHasExtraArgs = request.params.size() > 1;

    int nCount = 0;
    int nOffset = 0;
    int nStartHeight = 0;
    int nStopHeight = 0;
    if (request.params.size() > 1 && !request.params[1].isNull())
        nCount = request.params[1].get_int();
    if (request.params.size() > 2 && !request.params[2].isNull())
        nOffset = request.params[2].get_int();
    if (request.params.size() > 3 && !request.params[3].isNull())
        nStartHeight = request.params[3].get_int();
    if (request.params.size() > 4 && !request.params[4].isNull())
        nStopHeight = request.params[4].get_int();

    if (nOffset < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "offset must be >= 0");
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "count must be >= 0");

    std::set<CMessage> setMessages;
    LoadMessagesForRPC(setMessages);

    UniValue messages(UniValue::VARR);

    if (!fHasExtraArgs) {
        for (const auto& message : setMessages) {
            if (message.strName != channel)
                continue;
            messages.push_back(MessageEntryToJSON(message, GetMessageBlockHash(message)));
        }
        return messages;
    }

    std::vector<CMessage> vFiltered;
    for (const auto& message : setMessages) {
        if (message.strName != channel)
            continue;
        if (nStartHeight > 0 && message.nBlockHeight < nStartHeight)
            continue;
        if (nStopHeight > 0 && message.nBlockHeight > nStopHeight)
            continue;
        vFiltered.push_back(message);
    }

    std::sort(vFiltered.begin(), vFiltered.end(), MessageHeightOrder);

    if (nOffset >= static_cast<int>(vFiltered.size()))
        return messages;

    int nRemaining = static_cast<int>(vFiltered.size()) - nOffset;
    int nToReturn = (nCount == 0) ? nRemaining : std::min(nCount, nRemaining);

    for (int i = 0; i < nToReturn; ++i) {
        const CMessage& message = vFiltered[nOffset + i];
        messages.push_back(MessageEntryToJSON(message, GetMessageBlockHash(message)));
    }

    return messages;
}

UniValue getmessagetxid(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
                "getmessagetxid \"channel\" timestamp \"message_hash\"\n"
                + MessageActivationWarning() +
                "\nResolve a message transaction id by channel, timestamp, and message hash.\n"
                "\nArguments:\n"
                "1. \"channel\"              (string, required) The message channel name\n"
                "2. timestamp                (numeric, required) The Unix timestamp of the message\n"
                "3. \"message_hash\"         (string, required) The IPFS hash of the message\n"
                "\nResult:\n"
                "{\n"
                "  \"txid\": \"...\",                (string) The transaction id\n"
                "  \"channel\": \"...\",            (string) The message channel\n"
                "  \"block_height\": n,            (numeric) Block height of the message\n"
                "  \"block_hash\": \"...\",         (string) Block hash\n"
                "  \"message_hash\": \"...\",       (string) The IPFS hash of the message\n"
                "  \"timestamp\": n                (numeric) The message timestamp\n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli("getmessagetxid", "\"ROOT/H0XC!\" 1234567890 \"QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E\"")
                + HelpExampleRpc("getmessagetxid", "\"ROOT/H0XC!\", 1234567890, \"QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E\"")
        );

    if (!fMessaging)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Messaging is disabled.");

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Message database is not available");

    std::string channel = request.params[0].get_str();
    int64_t timestamp = request.params[1].get_int64();
    std::string message_hash = request.params[2].get_str();

    if (!NormalizeMessageChannel(channel))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel must be an owner asset or message channel asset.");

    std::string decoded_hash = DecodeAssetData(message_hash);
    if (decoded_hash.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "message_hash must be a valid IPFS hash or 64-character transaction id/message hash.");

    std::set<CMessage> setMessages;
    LoadMessagesForRPC(setMessages);

    std::vector<CMessage> matches;
    for (const auto& message : setMessages) {
        if (message.strName != channel)
            continue;
        if (message.time != timestamp)
            continue;
        if (message.ipfsHash != decoded_hash)
            continue;
        matches.push_back(message);
    }

    if (matches.empty())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No message found matching the given channel, timestamp, and message hash.");

    if (matches.size() > 1)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Multiple messages match the given criteria. Refine your query.");

    CMessage msg = matches[0];

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("txid", msg.out.hash.ToString()));
    result.push_back(Pair("channel", msg.strName));
    result.push_back(Pair("block_height", msg.nBlockHeight));
    result.push_back(Pair("block_hash", GetMessageBlockHash(msg)));
    result.push_back(Pair("message_hash", EncodeAssetData(msg.ipfsHash)));
    result.push_back(Pair("timestamp", msg.time));

    return result;
}

#ifdef ENABLE_WALLET
UniValue sendmessage(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error(
                "sendmessage \"channel_name\" \"ipfs_hash\" (expire_time)\n"
                + MessageActivationWarning() +
                "\nCreates and broadcasts a message transaction to the network for a channel this wallet owns"
                "\nArguments:\n"
                "1. \"channel_name\"             (string, required) Name of the channel that you want to send a message with (message channel, administrator asset), if a non administrator asset name is given, the administrator '!' will be added to it\n"
                "2. \"ipfs_hash\"                (string, required) The IPFS hash of the message\n"
                "3. \"expire_time\"              (numeric, optional) UTC timestamp of when the message expires\n"
                "\nResult:[\n"
                "txid\n"
                "]\n"
                "\nExamples:\n"
                + HelpExampleCli("sendmessage", "\"ASSET_NAME!\" \"QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E\" 15863654")
                + HelpExampleCli("sendmessage", "\"ASSET_NAME!\" \"QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E\" 15863654")
        );

    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::string asset_name = request.params[0].get_str();
    std::string ipfs_hash = request.params[1].get_str();

    int64_t expire_time = 0;
    if (request.params.size() > 2)
        expire_time = request.params[2].get_int64();

    CheckIPFSTxidMessage(ipfs_hash, expire_time);

    AssetType type;
    std::string strNameError;
    if (!IsAssetNameValid(asset_name, type, strNameError))
        throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid asset_name: ") + strNameError);

    if (type != AssetType::MSGCHANNEL && type != AssetType::OWNER && type != AssetType::ROOT && type != AssetType::SUB && type != AssetType::RESTRICTED)
        throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid asset_name: Only message channels, root, sub, restricted, and owner assets are allowed"));

    if (type == AssetType::ROOT || type == AssetType::SUB || type == AssetType::RESTRICTED)
        asset_name += OWNER_TAG;

    std::pair<int, std::string> error;
    std::vector< std::pair<CAssetTransfer, std::string> >vTransfers;

    std::map<std::string, std::vector<COutput> > mapAssetCoins;
    pwallet->AvailableAssets(mapAssetCoins);

    if (!mapAssetCoins.count(asset_name))
        throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Wallet doesn't own the asset_name: " + asset_name));

    CTxDestination dest;
    ExtractDestination(mapAssetCoins.at(asset_name)[0].tx->tx->vout[mapAssetCoins.at(asset_name)[0].i].scriptPubKey, dest);
    std::string address = EncodeDestination(dest);

    vTransfers.emplace_back(std::make_pair(CAssetTransfer(asset_name, OWNER_ASSET_AMOUNT, DecodeAssetData(ipfs_hash), expire_time), address));
    CReserveKey reservekey(pwallet);
    CWalletTx transaction;
    CAmount nRequiredFee;

    CCoinControl ctrl;

    if (!CreateTransferAssetTransaction(pwallet, ctrl, vTransfers, "", error, transaction, reservekey, nRequiredFee))
        throw JSONRPCError(error.first, error.second);

    std::string txid;
    if (!SendAssetTransaction(pwallet, transaction, reservekey, error, txid))
        throw JSONRPCError(error.first, error.second);

    UniValue result(UniValue::VARR);
    result.push_back(txid);
    return result;
}

UniValue viewmytaggedaddresses(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "viewmytaggedaddresses \n"
                + MessageActivationWarning() +
                "\nView all addresses this wallet owns that have been tagged\n"
                "\nResult:\n"
                "{\n"
                "\"Address:\"                        (string) The address that was tagged\n"
                "\"Tag Name:\"                       (string) The asset name\n"
                "\"[Assigned|Removed]:\"             (Date) The UTC datetime of the assignment or removal of the tag in the format (YY-mm-dd HH:MM:SS)\n"
                "                                         (Only the most recent tagging/untagging event will be returned for each address)\n"
                "}...\n"
                "\nExamples:\n"
                + HelpExampleCli("viewmytaggedaddresses", "")
                + HelpExampleRpc("viewmytaggedaddresses", "")
        );

    if (!AreRestrictedAssetsDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Restricted assets must be deployed first.");

    std::vector<std::tuple<std::string, std::string, bool, uint32_t> > myTaggedAddresses;

    if (!pmyrestricteddb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "My restricted database is not available");

    pmyrestricteddb->LoadMyTaggedAddresses(myTaggedAddresses);
    UniValue myTags(UniValue::VARR);

    for (auto item : myTaggedAddresses) {
        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("Address", std::get<0>(item)));
        obj.push_back(Pair("Tag Name", std::get<1>(item)));
        if (std::get<2>(item))
            obj.push_back(Pair("Assigned", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", std::get<3>(item))));
        else
            obj.push_back(Pair("Removed", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", std::get<3>(item))));

        myTags.push_back(obj);
    }

    return myTags;
}

UniValue viewmyrestrictedaddresses(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "viewmyrestrictedaddresses \n"
                + MessageActivationWarning() +
                "\nView all addresses this wallet owns that have been restricted\n"
                "\nResult:\n"
                "{\n"
                "\"Address:\"                        (string) The address that was restricted\n"
                "\"Asset Name:\"                     (string) The asset that the restriction applies to\n"
                "\"[Restricted|Derestricted]:\"      (Date) The UTC datetime of the restriction or derestriction in the format (YY-mm-dd HH:MM:SS))\n"
                "                                         (Only the most recent restriction/derestriction event will be returned for each address)\n"
                "}...\n"
                "\nExamples:\n"
                + HelpExampleCli("viewmyrestrictedaddresses", "")
                + HelpExampleRpc("viewmyrestrictedaddresses", "")
        );

    if (!AreRestrictedAssetsDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Restricted assets must be deployed first.");

    std::vector<std::tuple<std::string, std::string, bool, uint32_t> > myRestrictedAddresses;

    if (!pmyrestricteddb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "My restricted database is not available");

    pmyrestricteddb->LoadMyRestrictedAddresses(myRestrictedAddresses);
    UniValue myRestricted(UniValue::VARR);

    for (auto item : myRestrictedAddresses) {
        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("Address", std::get<0>(item)));
        obj.push_back(Pair("Asset Name", std::get<1>(item)));
        if (std::get<2>(item))
            obj.push_back(Pair("Restricted", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", std::get<3>(item))));
        else
            obj.push_back(Pair("Derestricted", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", std::get<3>(item))));

        myRestricted.push_back(obj);
    }

    return myRestricted;
}
#endif

UniValue rescanmessages(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
                "rescanmessages ( start_height ) ( stop_height ) ( \"channel\" )\n"
                + MessageActivationWarning() +
                "\nRebuild/backfill the message database by scanning active-chain blocks\n"
                "from disk. Intended for one-time recovery after enabling -messageindex=1,\n"
                "restoring a snapshot, or clearing the message DB. It is not the live\n"
                "notification mechanism; with -messageindex=1 Core indexes new messages\n"
                "as blocks connect.\n"
                "\nDoes not require a wallet, txindex, address index, spent index, or\n"
                "timestamp index. Rescan extraction is exact: it applies the same\n"
                "sender-ownership filter as live indexing using block undo data. If\n"
                "block undo data is unavailable, unreadable, or inconsistent, the\n"
                "rescan fails with a clear error (use an archival node or a later\n"
                "start_height). Does not change consensus, mempool policy, channel\n"
                "subscriptions, or wallet state. Already-indexed messages are not\n"
                "duplicated. Only one rescan may run at a time.\n"
                "\nArguments:\n"
                "1. start_height   (numeric, optional, default=messaging activation height) First block height to scan.\n"
                "2. stop_height    (numeric, optional, default=active chain tip) Last block height to scan. Use null for the tip.\n"
                "3. \"channel\"      (string, optional, default=all) Only store messages for this channel.\n"
                "                              Root/sub/restricted asset names are normalized to their owner channel form.\n"
                "                              Owner assets and message-channel assets are used as-is.\n"
                "\nWith a channel filter, only that channel is backfilled. Without a channel:\n"
                "  - if -messageindex=1, all channels are backfilled;\n"
                "  - if -messageindex=0, only currently subscribed channels are backfilled.\n"
                "\nNote: only a successful, unfiltered, all-message rescan that covers the\n"
                "full history from the messaging activation height marks the full message\n"
                "index as synced. Channel-filtered, partial-height, failed, or interrupted\n"
                "rescans store messages but leave message_index_needs_rescan=true so\n"
                "Commander can offer a full recovery action.\n"
                "\nResult:\n"
                "{\n"
                "  \"start_height\" : n,      (numeric) First scanned height\n"
                "  \"stop_height\" : n,       (numeric) Last scanned height\n"
                "  \"scanned_blocks\" : n,    (numeric) Number of blocks scanned\n"
                "  \"messages_found\" : n,    (numeric) Matching messages found\n"
                "  \"messages_added\" : n,    (numeric) New messages stored\n"
                "  \"messages_skipped\" : n,  (numeric) Already-indexed messages skipped\n"
                "  \"channel\" : \"...\",       (string, or null) Channel filter used\n"
                "  \"messageindex\" : true|false, (boolean) Whether -messageindex=1 is enabled\n"
                "  \"completed\" : true|false,    (boolean) Whether the scan ran to completion\n"
                "  \"pruned\" : true|false,   (boolean) Whether the node is pruned\n"
                "  \"warnings\" : [...]       (array of strings) Any warnings\n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli("rescanmessages", "")
                + HelpExampleCli("rescanmessages", "270144 null")
                + HelpExampleCli("rescanmessages", "270144 null ROOT/H0XC")
                + HelpExampleRpc("rescanmessages", "")
        );

    if (!fMessaging)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Messaging is disabled. To enable messaging, run the wallet without -disablemessaging or remove disablemessaging from your hemp0x.conf");

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        throw JSONRPCError(RPC_DATABASE_ERROR, "Message database isn't setup");

    const unsigned int nActivation = GetParams().MessagingActivationBlock();
    int nStartHeight = static_cast<int>(nActivation);
    if (request.params.size() > 0 && !request.params[0].isNull()) {
        nStartHeight = request.params[0].get_int();
        if (nStartHeight < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "start_height must be >= 0");
    }
    // Never silently scan from genesis if messaging activates later.
    if (nActivation > 0 && nStartHeight < static_cast<int>(nActivation))
        nStartHeight = static_cast<int>(nActivation);

    bool fChannelFilter = false;
    std::string channelFilter;
    if (request.params.size() > 2 && !request.params[2].isNull()) {
        channelFilter = request.params[2].get_str();
        if (!channelFilter.empty()) {
            fChannelFilter = true;
            if (!NormalizeMessageChannel(channelFilter))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Channel must be an owner asset or message channel asset.");
        }
    }

    int nBestHeight;
    {
        LOCK(cs_main);
        nBestHeight = chainActive.Height();
    }

    int nStopHeight = nBestHeight;
    if (request.params.size() > 1 && !request.params[1].isNull()) {
        nStopHeight = request.params[1].get_int();
        if (nStopHeight < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "stop_height must be >= 0");
    }
    if (nStopHeight > nBestHeight)
        nStopHeight = nBestHeight;
    if (nStopHeight < nStartHeight)
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("stop_height (%d) must be >= start_height (%d)", nStopHeight, nStartHeight));

    // Determine whether a given message should be stored based on the mode and
    // optional channel filter. With -messageindex=1 store everything (subject to
    // the channel filter); otherwise only store currently subscribed channels.
    bool fStoreAll = fMessageIndex || fChannelFilter;
    auto ShouldStore = [&](const std::string& strName) -> bool {
        if (fChannelFilter)
            return strName == channelFilter;
        if (fStoreAll)
            return true;
        return IsChannelSubscribed(strName);
    };

    // Atomically check-and-start the rescan under cs_messaging. This prevents
    // the TOCTOU race where two RPC threads both observe fInProgress=false
    // before either sets it to true. All argument validation is done above, so
    // we only set the in-progress flag when we are actually about to scan.
    MessageRescanProgress progress;
    progress.fInProgress = true;
    progress.nStartHeight = nStartHeight;
    progress.nStopHeight = nStopHeight;
    progress.nCurrentHeight = nStartHeight - 1;
    progress.nStartedAt = GetTime();
    if (!TryStartMessageRescan(progress))
        throw JSONRPCError(RPC_MISC_ERROR, "A message rescan is already in progress. Poll getmessaginginfo for message_rescan_* progress and retry after it completes.");

    UniValue warnings(UniValue::VARR);

    int nScanned = 0;
    int nFound = 0;
    int nAdded = 0;
    int nSkipped = 0;
    int nCurrentHeight = nStartHeight - 1;
    int64_t nLastProgressLog = GetTime();
    bool fInterrupted = false;
    // fCompleted means "the requested scan reached stop_height". It is the
    // result reported to the caller and used to decide synced-height advancement.
    bool fCompleted = false;
    // fGuardDismissed is separate from fCompleted. The RAII guard skips its
    // failure finalization ONLY when fGuardDismissed is true, which is set
    // exclusively on the success path AFTER all success work (flush, metadata
    // writes, synced-height persistence, progress finalization) has completed.
    // If any success step fails, fGuardDismissed stays false and the guard
    // finalizes progress as a failure.
    bool fGuardDismissed = false;
    // Captured failure reason for the RAII guard and metadata writes.
    std::string strFailureReason;

    // R6: Capture the synced height before any candidate advance so the guard
    // can restore it if the success path fails after advancing in-memory state
    // but before persistence/guard-dismissal. fSyncedHeightCandidateAdvanced is
    // set true only when the success path calls SetMessageIndexSyncedHeight;
    // the guard restores the previous value if the guard runs (i.e. the success
    // path did not dismiss it).
    const int nPreviousSyncedHeight = GetMessageIndexSyncedHeight();
    bool fSyncedHeightCandidateAdvanced = false;

    // RAII guard: guarantees progress is finalized (fInProgress=false,
    // nFinishedAt, strLastError, last-scan metadata) on every exit path,
    // including non-UniValue exceptions. It never advances the synced height;
    // that is done explicitly only on the success path. The guard records
    // completed=false unless the success path sets fGuardDismissed=true after
    // all success work is done. If a candidate synced-height advance was made
    // but the guard runs (success path failed after the advance), the guard
    // restores the previous synced height so getmessaginginfo cannot report
    // the index as synced after a failed rescan.
    //
    // The destructor is noexcept-safe: if any called function throws during
    // stack unwinding, the exception is caught and logged, never rethrown.
    // Metadata write return values are checked and logged (not thrown).
    struct RescanProgressGuard {
        int nStartHeight;
        int nStopHeight;
        int& nCurrentHeight;
        int& nScanned;
        int& nFound;
        int& nAdded;
        bool& fGuardDismissed;
        std::string& strFailureReason;
        int nPreviousSyncedHeight;
        bool& fSyncedHeightCandidateAdvanced;

        ~RescanProgressGuard() noexcept {
            // If the success path set fGuardDismissed=true, all success work
            // already completed and finalized progress. Otherwise finalize as
            // a failure/interruption.
            if (fGuardDismissed)
                return;

            try {
                // R6: If the success path advanced the in-memory synced height
                // but then failed before dismissing the guard, restore the
                // previous value so getmessaginginfo does not report the index
                // as synced after a failed rescan.
                if (fSyncedHeightCandidateAdvanced) {
                    SetMessageIndexSyncedHeightExact(nPreviousSyncedHeight);
                }

                std::string strError = strFailureReason;
                if (strError.empty())
                    strError = "rescan did not complete";

                MessageRescanProgress p;
                GetMessageRescanProgress(p);
                p.fInProgress = false;
                p.nCurrentHeight = nCurrentHeight;
                p.nScannedBlocks = nScanned;
                p.nMessagesFound = nFound;
                p.nMessagesAdded = nAdded;
                p.nFinishedAt = GetTime();
                p.strLastError = strError;
                SetMessageRescanProgress(p);

                // Record last-scan metadata but do NOT advance the full-index synced
                // height on any non-completion path. Check return values and log
                // failures; never throw from this noexcept destructor.
                if (pmessagedb) {
                    if (!pmessagedb->WriteMetaInt64(MSG_META_LAST_SCAN_START, nStartHeight))
                        LogPrintf("rescanmessages: failed to write MSG_META_LAST_SCAN_START\n");
                    if (!pmessagedb->WriteMetaInt64(MSG_META_LAST_SCAN_STOP, nCurrentHeight))
                        LogPrintf("rescanmessages: failed to write MSG_META_LAST_SCAN_STOP\n");
                    if (!pmessagedb->WriteFlag(MSG_META_LAST_SCAN_COMPLETED, false))
                        LogPrintf("rescanmessages: failed to write MSG_META_LAST_SCAN_COMPLETED\n");
                }
            } catch (const std::exception& e) {
                // Never throw out of a destructor during stack unwinding. Log
                // the failure so it is not silently lost.
                LogPrintf("rescanmessages: RescanProgressGuard finalization failed: %s\n", e.what());
            } catch (...) {
                LogPrintf("rescanmessages: RescanProgressGuard finalization failed: unknown exception\n");
            }
        }
    } progressGuard{nStartHeight, nStopHeight, nCurrentHeight, nScanned, nFound, nAdded,
                    fGuardDismissed, strFailureReason, nPreviousSyncedHeight,
                    fSyncedHeightCandidateAdvanced};

    try {
        for (int nHeight = nStartHeight; nHeight <= nStopHeight; ++nHeight) {
            if (ShutdownRequested()) {
                warnings.push_back("Rescan interrupted by shutdown");
                fInterrupted = true;
                strFailureReason = "Rescan interrupted by shutdown";
                break;
            }

            CBlockIndex* pindex = nullptr;
            {
                LOCK(cs_main);
                pindex = chainActive[nHeight];
                if (!pindex) {
                    strFailureReason = strprintf("Block at height %d is not in the active chain", nHeight);
                    throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
                }
            }

            // Pruned/unavailable block handling: fail clearly rather than silently.
            if (!(pindex->nStatus & BLOCK_HAVE_DATA)) {
                strFailureReason = strprintf("Block data for height %d is not available (pruned). Use a later start_height or an archival node to rescan older history.", nHeight);
                throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
            }

            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, GetParams().GetConsensus())) {
                strFailureReason = strprintf("Failed to read block %d from disk", nHeight);
                throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
            }

            // R3: read block undo data and require it. The sender-ownership filter
            // from Consensus::CheckTxAssets can only be reproduced exactly with
            // valid undo data. If undo data is unavailable, unreadable, or
            // inconsistent, fail the rescan clearly. Do NOT fall back to
            // output-only extraction by default — that could index messages live
            // indexing would reject.
            if (block.vtx.size() > 1) {
                CDiskBlockPos undoPos = pindex->GetUndoPos();
                if (undoPos.IsNull() || !pindex->pprev) {
                    strFailureReason = strprintf("Block undo data is not available for height %d. Exact rescan requires undo data. Use an archival node or a later start_height.", nHeight);
                    throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
                }

                CBlockUndo blockUndo;
                if (!UndoReadFromDisk(blockUndo, undoPos, pindex->pprev->GetBlockHash())) {
                    strFailureReason = strprintf("Failed to read block undo data for height %d. Exact rescan requires undo data. Use an archival node or a later start_height.", nHeight);
                    throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
                }

                nScanned++;
                nCurrentHeight = nHeight;

                std::vector<CMessage> vBlockMessages;
                std::string strExtractError;
                if (!ExtractMessagesFromBlock(block, nHeight, block.nTime, &blockUndo, vBlockMessages, strExtractError)) {
                    strFailureReason = strExtractError;
                    throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
                }

                for (const auto& message : vBlockMessages) {
                    // messages_found counts messages matching the active filter
                    // (channel filter, or all/subscribed depending on mode). A message
                    // is then either added (new) or skipped (already indexed).
                    if (!ShouldStore(message.strName))
                        continue;

                    nFound++;

                    // Avoid duplicating already-indexed messages and preserve any
                    // existing read/status state. GetMessage checks the dirty cache
                    // and the on-disk DB.
                    CMessage existing;
                    if (GetMessage(message.out, existing)) {
                        nSkipped++;
                        continue;
                    }

                    AddMessage(message);
                    nAdded++;
                }
            } else {
                // Coinbase-only block: no undo data needed, nothing to extract.
                nScanned++;
                nCurrentHeight = nHeight;
            }

            // Update in-progress progress periodically (per block is cheap and
            // lets Commander poll smoothly).
            {
                MessageRescanProgress p;
                GetMessageRescanProgress(p);
                p.nCurrentHeight = nCurrentHeight;
                p.nScannedBlocks = nScanned;
                p.nMessagesFound = nFound;
                p.nMessagesAdded = nAdded;
                SetMessageRescanProgress(p);
            }

            if (GetTime() >= nLastProgressLog + 60) {
                nLastProgressLog = GetTime();
                const double dProgress = (nStopHeight > nStartHeight)
                    ? (double)(nHeight - nStartHeight) / (double)(nStopHeight - nStartHeight)
                    : 1.0;
                LogPrintf("rescanmessages: progress=%f height=%d/%d scanned=%d found=%d added=%d skipped=%d\n",
                          dProgress, nHeight, nStopHeight, nScanned, nFound, nAdded, nSkipped);
            }
        }
    } catch (const UniValue& e) {
        // RPC errors: strFailureReason was already set before throwing, so the
        // RAII guard will record it. Re-throw so the RPC layer returns the error
        // to the caller. (No synced-height advance; the guard handles metadata.)
        throw;
    } catch (const std::exception& e) {
        // Non-RPC exception: record the reason and re-throw as a generic RPC error
        // so the caller sees a clear failure. The guard finalizes progress.
        strFailureReason = std::string("rescan failed: ") + e.what();
        throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
    } catch (...) {
        // Unknown exception: finalize as a generic failure.
        strFailureReason = "rescan failed: unknown exception";
        throw JSONRPCError(RPC_MISC_ERROR, strFailureReason);
    }

    // If the scan was interrupted by shutdown, it is not complete. The guard
    // has already recorded the interruption metadata; return a result now.
    if (fInterrupted) {
        if (fPruneMode)
            warnings.push_back("Node is pruned; historical all-message rescans can only cover blocks still on disk. New messages are still indexed as blocks connect.");

        UniValue result(UniValue::VOBJ);
        result.pushKV("start_height", nStartHeight);
        result.pushKV("stop_height", nStopHeight);
        result.pushKV("scanned_blocks", nScanned);
        result.pushKV("messages_found", nFound);
        result.pushKV("messages_added", nAdded);
        result.pushKV("messages_skipped", nSkipped);
        result.pushKV("channel", fChannelFilter ? UniValue(channelFilter) : NullUniValue);
        result.pushKV("messageindex", fMessageIndex);
        result.pushKV("completed", false);
        result.pushKV("pruned", fPruneMode);
        result.pushKV("warnings", warnings);
        return result;
    }

    // Flush pending dirty messages so counts and getmessaginginfo reflect them.
    // A flush failure is a failure: do not mark the rescan completed and do not
    // advance the synced height. The guard finalizes progress as failed.
    {
        LOCK(cs_messaging);
        if (pmessagedb) {
            if (!pmessagedb->Flush()) {
                strFailureReason = "Failed to flush the message database after rescan";
                throw JSONRPCError(RPC_DATABASE_ERROR, strFailureReason);
            }
        }
    }

    // At this point the scan ran to stop_height without interruption and the
    // flush succeeded. Determine completion status.
    fCompleted = (nCurrentHeight >= nStopHeight);

    // Record last-scan metadata. Every write must succeed; a failure here is a
    // database error that must not be hidden. Do NOT advance the synced height
    // or dismiss the guard unless all metadata writes succeed.
    if (pmessagedb) {
        if (!pmessagedb->WriteMetaInt64(MSG_META_LAST_SCAN_START, nStartHeight)) {
            strFailureReason = "Failed to write last-scan start height metadata";
            throw JSONRPCError(RPC_DATABASE_ERROR, strFailureReason);
        }
        if (!pmessagedb->WriteMetaInt64(MSG_META_LAST_SCAN_STOP, nStopHeight)) {
            strFailureReason = "Failed to write last-scan stop height metadata";
            throw JSONRPCError(RPC_DATABASE_ERROR, strFailureReason);
        }
        if (!pmessagedb->WriteFlag(MSG_META_LAST_SCAN_COMPLETED, fCompleted)) {
            strFailureReason = "Failed to write last-scan completed metadata";
            throw JSONRPCError(RPC_DATABASE_ERROR, strFailureReason);
        }
    }

    // Advance the full-index synced height ONLY for a successful, unfiltered,
    // all-message rescan that covers the full history from the messaging
    // activation height (or continues from where the index was already synced).
    // Channel-filtered, partial-height, failed, or interrupted rescans must not
    // mark the all-message index as complete — that would cause getmessaginginfo
    // to report message_index_synced=true / message_index_needs_rescan=false
    // even when the message DB is incomplete.
    //
    // R6: This advances the in-memory synced height before persistence. If
    // persistence or any later success step fails, the guard restores the
    // previous value (captured in nPreviousSyncedHeight) so getmessaginginfo
    // cannot report the index as synced after a failed rescan.
    if (fMessageIndex && !fChannelFilter && fCompleted) {
        int nCurrentSynced = GetMessageIndexSyncedHeight();
        if (nStartHeight <= static_cast<int>(nActivation) || nCurrentSynced >= nStartHeight - 1) {
            SetMessageIndexSyncedHeight(nStopHeight);
            fSyncedHeightCandidateAdvanced = true;
        }
    }
    // Persist synced-height metadata to disk. A failure here is a database
    // error that must not be hidden. Do NOT dismiss the guard on failure. If
    // this fails after a candidate advance, the guard restores the previous
    // synced height.
    if (fMessageIndex) {
        if (!PersistMessageIndexMetadata()) {
            strFailureReason = "Failed to persist message index metadata after rescan";
            throw JSONRPCError(RPC_DATABASE_ERROR, strFailureReason);
        }
    }

    // Finalize success progress. fInProgress=false, clear last error.
    {
        MessageRescanProgress p;
        GetMessageRescanProgress(p);
        p.fInProgress = false;
        p.nCurrentHeight = nCurrentHeight;
        p.nScannedBlocks = nScanned;
        p.nMessagesFound = nFound;
        p.nMessagesAdded = nAdded;
        p.nFinishedAt = GetTime();
        p.strLastError.clear();
        SetMessageRescanProgress(p);
    }

    // All success-path work is complete: flush, metadata writes, synced-height
    // persistence, and progress finalization. Now dismiss the guard so it does
    // not re-finalize progress as a failure.
    fGuardDismissed = true;

    if (fPruneMode)
        warnings.push_back("Node is pruned; historical all-message rescans can only cover blocks still on disk. New messages are still indexed as blocks connect.");

    UniValue result(UniValue::VOBJ);
    result.pushKV("start_height", nStartHeight);
    result.pushKV("stop_height", nStopHeight);
    result.pushKV("scanned_blocks", nScanned);
    result.pushKV("messages_found", nFound);
    result.pushKV("messages_added", nAdded);
    result.pushKV("messages_skipped", nSkipped);
    result.pushKV("channel", fChannelFilter ? UniValue(channelFilter) : NullUniValue);
    result.pushKV("messageindex", fMessageIndex);
    result.pushKV("completed", fCompleted);
    result.pushKV("pruned", fPruneMode);
    result.pushKV("warnings", warnings);

    return result;
}

UniValue viewindexedmessagechannels(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
                "viewindexedmessagechannels ( \"pattern\" ) ( count ) ( offset )\n"
                + MessageActivationWarning() +
                "\nDiscover channels that have indexed on-chain asset messages, without\n"
                "requiring channel subscriptions. Useful for community chat / indexer\n"
                "nodes running -messageindex=1 to find chat-active channels.\n"
                "\nArguments:\n"
                "1. \"pattern\"   (string, optional) Filter channels by '*' glob (e.g. \"*/H0XC!\"). Empty = all channels.\n"
                "2. count        (numeric, optional, default=0) Maximum number of channels to return. 0 = return all.\n"
                "3. offset       (numeric, optional, default=0) Number of leading channels to skip.\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"channel\" : \"...\",     (string) Channel name\n"
                "    \"message_count\" : n,    (numeric) Number of indexed messages in this channel\n"
                "    \"first_height\" : n,     (numeric) First message block height\n"
                "    \"last_height\" : n,      (numeric) Last message block height\n"
                "    \"last_time\" : n         (numeric) Last message timestamp\n"
                "  },...\n"
                "]\n"
                "\nExamples:\n"
                + HelpExampleCli("viewindexedmessagechannels", "")
                + HelpExampleCli("viewindexedmessagechannels", "\"*/H0XC!\" 100 0")
                + HelpExampleRpc("viewindexedmessagechannels", "")
        );

    if (!fMessaging)
        return UniValue(UniValue::VARR);

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        return UniValue(UniValue::VARR);

    std::string pattern;
    bool fUsePattern = false;
    int nCount = 0;
    int nOffset = 0;
    if (request.params.size() > 0 && !request.params[0].isNull()) {
        pattern = request.params[0].get_str();
        fUsePattern = !pattern.empty();
    }
    if (request.params.size() > 1 && !request.params[1].isNull())
        nCount = request.params[1].get_int();
    if (request.params.size() > 2 && !request.params[2].isNull())
        nOffset = request.params[2].get_int();

    if (nOffset < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "offset must be >= 0");
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "count must be >= 0");

    struct ChannelAgg {
        int nCount;
        int nFirstHeight;
        int nLastHeight;
        int64_t nLastTime;
    };

    std::set<CMessage> setMessages;
    LoadMessagesForRPC(setMessages);

    std::map<std::string, ChannelAgg> aggMap;
    for (const auto& message : setMessages) {
        if (message.status == MessageStatus::ORPHAN)
            continue;
        if (fUsePattern && !GlobMatchChannel(pattern, message.strName))
            continue;

        auto it = aggMap.find(message.strName);
        if (it == aggMap.end()) {
            ChannelAgg a;
            a.nCount = 1;
            a.nFirstHeight = message.nBlockHeight;
            a.nLastHeight = message.nBlockHeight;
            a.nLastTime = message.time;
            aggMap.emplace(message.strName, a);
        } else {
            it->second.nCount++;
            if (message.nBlockHeight < it->second.nFirstHeight)
                it->second.nFirstHeight = message.nBlockHeight;
            if (message.nBlockHeight > it->second.nLastHeight)
                it->second.nLastHeight = message.nBlockHeight;
            if (message.time > it->second.nLastTime)
                it->second.nLastTime = message.time;
        }
    }

    // Stable order by channel name.
    std::vector<std::pair<std::string, ChannelAgg>> vChannels(aggMap.begin(), aggMap.end());
    std::sort(vChannels.begin(), vChannels.end(),
              [](const std::pair<std::string, ChannelAgg>& a, const std::pair<std::string, ChannelAgg>& b) {
                  return a.first < b.first;
              });

    UniValue result(UniValue::VARR);
    if (nOffset >= static_cast<int>(vChannels.size()))
        return result;

    int nRemaining = static_cast<int>(vChannels.size()) - nOffset;
    int nToReturn = (nCount == 0) ? nRemaining : std::min(nCount, nRemaining);

    for (int i = 0; i < nToReturn; ++i) {
        const auto& item = vChannels[nOffset + i];
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("channel", item.first);
        obj.pushKV("message_count", item.second.nCount);
        obj.pushKV("first_height", item.second.nFirstHeight);
        obj.pushKV("last_height", item.second.nLastHeight);
        obj.pushKV("last_time", item.second.nLastTime);
        result.push_back(obj);
    }

    return result;
}

static const CRPCCommand commands[] =
    {           //  category    name                          actor (function)             argNames
                //  ----------- ------------------------      -----------------------      ----------
            { "messages",       "getmessaginginfo",           &getmessaginginfo,           {}},
            { "messages",       "viewallmessages",            &viewallmessages,            {"count","offset","channel","start_height","stop_height"}},
            { "messages",       "viewallmessagechannels",     &viewallmessagechannels,     {}},
            { "messages",       "viewindexedmessagechannels", &viewindexedmessagechannels, {"pattern","count","offset"}},
            { "messages",       "viewchannelmessages",        &viewchannelmessages,        {"channel","count","offset","start_height","stop_height"}},
            { "messages",       "getmessagetxid",             &getmessagetxid,             {"channel","timestamp","message_hash"}},
            { "messages",       "subscribetochannel",         &subscribetochannel,         {"channel_name"}},
            { "messages",       "unsubscribefromchannel",     &unsubscribefromchannel,     {"channel_name"}},
            { "messages",       "rescanmessages",             &rescanmessages,             {"start_height","stop_height","channel"}},
#ifdef ENABLE_WALLET
            { "messages",       "sendmessage",                &sendmessage,                {"channel", "ipfs_hash", "expire_time"}},
            {"restricted",        "viewmytaggedaddresses",      &viewmytaggedaddresses,       {}},
            {"restricted",        "viewmyrestrictedaddresses",  &viewmyrestrictedaddresses,   {}},
#endif
            { "messages",       "clearmessages",              &clearmessages,              {}},
    };

void RegisterMessageRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
