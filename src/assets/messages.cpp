// Copyright (c) 2018-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <validation.h>
#include <chainparams.h>
#include <wallet/wallet.h>
#include <script/ismine.h>
#include <base58.h>
#include "messages.h"
#include "myassetsdb.h"
#include <primitives/block.h>
#include <univalue.h>
#include <vector>


std::set<COutPoint> setDirtyMessagesRemove;
std::map<COutPoint, CMessage> mapDirtyMessagesAdd;
std::map<COutPoint, CMessage> mapDirtyMessagesOrphaned;

std::set<std::string> setDirtyChannelsAdd;
std::set<std::string> setDirtyChannelsRemove;
std::set<std::string> setSubscribedChannelsAskedForFalse;

std::set<std::string> setDirtySeenAddressAdd;
std::set<std::string> setAddressAskedForFalse;

CCriticalSection cs_messaging;


int8_t IntFromMessageStatus(MessageStatus status)
{
    return (int8_t)status;
}

MessageStatus MessageStatusFromInt(int8_t nStatus)
{
    if (nStatus < 0 || nStatus > 6)
        return MessageStatus::MSG_ERROR;
    return (MessageStatus)nStatus;
}

std::string MessageStatusToString(MessageStatus status)
{
    switch (status) {
        case MessageStatus::READ: return "READ";
        case MessageStatus::UNREAD: return "UNREAD";
        case MessageStatus::ORPHAN: return "ORPHAN";
        case MessageStatus::EXPIRED: return "EXPIRED";
        case MessageStatus::SPAM: return "SPAM";
        case MessageStatus::HIDDEN: return "HIDDEN";
        case MessageStatus::MSG_ERROR: return "ERROR";
        default: return "ERROR";
    }
}

CMessage::CMessage() {
    SetNull();
}

CMessage::CMessage(const COutPoint& out, const std::string& strName, const std::string& ipfsHash, const int64_t& nExpiredTime, const int64_t& time)
{
    SetNull();
    this->out = out;
    this->strName = strName;
    this->ipfsHash = ipfsHash;
    this->nExpiredTime = nExpiredTime;
    this->time = time;
    status = MessageStatus::UNREAD;
}

bool IsChannelSubscribed(const std::string &name)
{
    LOCK(cs_messaging);

    if (!pMessageSubscribedChannelsCache || !pmessagechanneldb)
        return false;

    // Check Dirty Cache for newly added channel additions
    if (setDirtyChannelsAdd.count(name))
        return true;

    // Check Dirty Cache for newly removed channels
    if (setDirtyChannelsRemove.count(name))
        return false;

    // Check the Channel Cache and see if it is in the Cache
    if (pMessageSubscribedChannelsCache->Exists(name))
        return true;

    // Check if we have already searched for this before
    if (setSubscribedChannelsAskedForFalse.count(name))
        return false;

    // Check to see if the message database contains the asset
    if (pmessagechanneldb->ReadMyMessageChannel(name)) {
        pMessageSubscribedChannelsCache->Put(name, 1);
        return true;
    }

    // Help prevent spam and unneeded database reads
    setSubscribedChannelsAskedForFalse.insert(name);

    return false;
}

bool GetMessage(const COutPoint& out, CMessage& message)
{
    LOCK(cs_messaging);

    if (!pmessagedb || !pMessagesCache)
        return false;

    // Check the dirty add cache
    if (mapDirtyMessagesAdd.count(out)) {
        message = mapDirtyMessagesAdd.at(out);
        return true;
    }

    // Check the dirty remove cache
    if (setDirtyMessagesRemove.count(out))
        return false;

    // Check database cache
    if (pMessagesCache->Exists(out.ToSerializedString())) {
        message = pMessagesCache->Get(out.ToSerializedString());
        return true;
    }

    // Check the database
    if (pmessagedb->ReadMessage(out, message)) {
        pMessagesCache->Put(out.ToSerializedString(), message);
        return true;
    }

    return false;
}

void AddChannel(const std::string &name)
{
    LOCK(cs_messaging);

    // Add channel to dirty cache to add
    setDirtyChannelsAdd.insert(name);

    // If the channel name is in the dirty remove cache. Remove it so it doesn't get deleted on flush
    setDirtyChannelsRemove.erase(name);
    setSubscribedChannelsAskedForFalse.erase(name);
}

void RemoveChannel(const std::string &name)
{
    LOCK(cs_messaging);

    // Add channel to dirty cache to remove
    setDirtyChannelsRemove.insert(name);

    // If the channel name is in the dirty add cache. Remove it so it doesn't get added on flush
    setDirtyChannelsAdd.erase(name);
}

void AddMessage(const CMessage& message)
{
    LOCK(cs_messaging);

    // Add message to dirty map cache to add
    mapDirtyMessagesAdd.insert(std::make_pair(message.out, message));

    // Remove message Out from dirty set Cache to remove
    setDirtyMessagesRemove.erase(message.out);
    mapDirtyMessagesOrphaned.erase(message.out);
}

void RemoveMessage(const CMessage& message)
{
    RemoveMessage(message.out);
}

void RemoveMessage(const COutPoint &out)
{
    LOCK(cs_messaging);

    // Add message out to dirty set Cache to remove
    setDirtyMessagesRemove.insert(out);

    // Remove message from map Dirty Message to add
    mapDirtyMessagesAdd.erase(out);
    mapDirtyMessagesOrphaned.erase(out);
}

void OrphanMessage(const COutPoint &out)
{
    CMessage message;
    if (GetMessage(out, message))
        OrphanMessage(message);
}

void OrphanMessage(const CMessage& message)
{
    LOCK(cs_messaging);

    mapDirtyMessagesOrphaned[message.out] = message;

    // Remove from other dirty caches
    mapDirtyMessagesAdd.erase(message.out);
}

bool GlobMatchChannel(const std::string& pattern, const std::string& channel)
{
    if (pattern.empty())
        return true;

    const size_t m = pattern.size();
    const size_t n = channel.size();
    std::vector<std::vector<bool>> dp(m + 1, std::vector<bool>(n + 1, false));
    dp[0][0] = true;
    for (size_t i = 1; i <= m; ++i) {
        if (pattern[i - 1] == '*')
            dp[i][0] = dp[i - 1][0];
    }
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (pattern[i - 1] == '*') {
                dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
            } else if (pattern[i - 1] == channel[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            }
        }
    }
    return dp[m][n];
}

#ifdef ENABLE_WALLET
bool ScanForMessageChannels(std::string& strError)
{
    LOCK2(cs_messaging, cs_main);

    LogPrintf("%s : Start Scanning For Message Channels\n", __func__);

    CWallet* pwalletForScan = nullptr;
    {
        LOCK(cs_wallets);
        if (vpwallets.size() == 0) {
            strError = "Wallet isn't active on this client. Can't scan for MsgChannels";
            return false;
        }
        pwalletForScan = vpwallets[0];
    }
    // Keep cs_wallets out of the block scan; this is only a lifetime-safe
    // snapshot because wallets are not unloaded while the daemon is running.

    CBlockIndex* blockIndex = chainActive[GetParams().GetAssetActivationHeight()];

    const int nTipHeight = chainActive.Height();
    int64_t nLastProgressLog = GetTime();
    while (blockIndex) {
        if (GetTime() >= nLastProgressLog + 60) {
            nLastProgressLog = GetTime();
            const double dProgress = nTipHeight > 0 ? (double)blockIndex->nHeight / (double)nTipHeight : 0.0;
            LogPrintf("%s : Scanning message channels progress=%f height=%d total=%d\n", __func__, dProgress, blockIndex->nHeight, nTipHeight);
        }

        CBlock block;
        if (!ReadBlockFromDisk(block, blockIndex, GetParams().GetConsensus())) {
            strError = "Block not found on disk";
            return false;
        }

        for (const auto& tx : block.vtx) {

            auto ptx = tx.get();
            if (!ptx) {
                strError = "Failed to get transaction pointer";
                return false;
            }

            for (auto out : ptx->vout) {
                int nType = -1;
                bool fOwner = false;
                if (pwalletForScan && pwalletForScan->IsMine(out) == ISMINE_SPENDABLE) { // Is the out mine
                    if (out.scriptPubKey.IsAssetScript(nType, fOwner)) {
                        CAssetOutputEntry assetData;
                        // Get the asset data from the script
                        if (GetAssetData(out.scriptPubKey, assetData)) {
                            AssetType type;
                            IsAssetNameValid(assetData.assetName, type);

                            if (assetData.type == TX_TRANSFER_ASSET) {
                                if (type == AssetType::MSGCHANNEL || type == AssetType::OWNER) { // Subscribe to any channels or owner tokens you own
                                    AddChannel(assetData.assetName);
                                    AddAddressSeen(EncodeDestination(assetData.destination));
                                } else if (type == AssetType::ROOT || type == AssetType::SUB) { // Subscribe to any assets you are sent, if they are sent to a new address
                                    if (!IsChannelSubscribed(assetData.assetName + OWNER_TAG)) {
                                        if (!IsAddressSeen(EncodeDestination(assetData.destination))) {
                                            AddChannel(assetData.assetName + OWNER_TAG);
                                            AddAddressSeen(EncodeDestination(assetData.destination));
                                        }
                                    }
                                }
                            } else if (assetData.type == TX_NEW_ASSET || assetData.type == TX_REISSUE_ASSET) {
                                if (fOwner || type == AssetType::MSGCHANNEL) {
                                    AddChannel(assetData.assetName);
                                    AddAddressSeen(EncodeDestination(assetData.destination));
                                } else if (type == AssetType::ROOT || type == AssetType::SUB || type == AssetType::RESTRICTED) {
                                    AddChannel(assetData.assetName + "!");
                                    AddAddressSeen(EncodeDestination(assetData.destination));
                                }
                            }
                        } else {
                            LogPrintf("%s : Failed to get GetAssetData call\n", __func__);
                        }
                    }
                }
            }
        }
        blockIndex = chainActive[blockIndex->nHeight + 1];
    }

    LogPrintf("%s : Finished Scanning For Message Channels. Subscribed Messages Channels Found: %u\n", __func__, setDirtyChannelsAdd.size());
    for (auto item : setDirtyChannelsAdd) {
        LogPrintf("%s, ",item);
    }
    LogPrintf("\n");
    return true;
}
#endif

bool IsAddressSeen(const std::string &address)
{
    LOCK(cs_messaging);

    if (!pmessagechanneldb || !pMessagesSeenAddressCache)
        return false;

    if (setDirtySeenAddressAdd.count(address)) // Check dirty set
        return true;

    if (pMessagesSeenAddressCache->Exists(address)) {
        return true;
    }

    if (setAddressAskedForFalse.count(address)) {
        return false;
    }

    if (pmessagechanneldb->ReadUsedAddress(address)) {
        pMessagesSeenAddressCache->Put(address, 1);
        return true;
    }

    setAddressAskedForFalse.insert(address);

    return false;
}

void AddAddressSeen(const std::string &address)
{
    LOCK(cs_messaging);

    setDirtySeenAddressAdd.insert(address);
    setSubscribedChannelsAskedForFalse.erase(address);
}

// ---------------------------------------------------------------------------
// Message index metadata and rescan progress state
//
// This state is non-consensus. It tracks the optional -messageindex=1 full
// message index: the last block height indexed for messages, the last explicit
// rescan result, and the current in-progress rescan progress (so Commander can
// poll getmessaginginfo from a separate RPC connection while rescanmessages
// runs in a background task).
// ---------------------------------------------------------------------------

const std::string MSG_META_SYNCED_HEIGHT = "synced_height";
const std::string MSG_META_LAST_SCAN_START = "last_scan_start_height";
const std::string MSG_META_LAST_SCAN_STOP = "last_scan_stop_height";
const std::string MSG_META_LAST_SCAN_COMPLETED = "last_scan_completed";
const std::string MSG_META_INDEX_ENABLED = "index_enabled";

static int g_nMessageIndexSyncedHeight = 0;
static MessageRescanProgress g_message_rescan;

int GetMessageIndexSyncedHeight()
{
    LOCK(cs_messaging);
    return g_nMessageIndexSyncedHeight;
}

void SetMessageIndexSyncedHeight(int nHeight)
{
    LOCK(cs_messaging);
    if (nHeight > g_nMessageIndexSyncedHeight)
        g_nMessageIndexSyncedHeight = nHeight;
}

void SetMessageIndexSyncedHeightExact(int nHeight)
{
    LOCK(cs_messaging);
    g_nMessageIndexSyncedHeight = nHeight;
}

bool AdvanceMessageIndexSyncedHeightIfContiguous(int nHeight, int nActivationHeight)
{
    LOCK(cs_messaging);

    // Allow advancing to or below the activation boundary unconditionally —
    // no messages exist before activation, so there is no gap to worry about.
    if (nHeight <= nActivationHeight) {
        if (nHeight > g_nMessageIndexSyncedHeight)
            g_nMessageIndexSyncedHeight = nHeight;
        return true;
    }

    // Above activation: only advance if the index is already synced through
    // nHeight-1 (contiguous coverage). This prevents new blocks from hiding a
    // historical gap when -messageindex=1 was enabled over existing history,
    // after clearmessages, or after a partial/failed rescan.
    if (g_nMessageIndexSyncedHeight >= nHeight - 1) {
        if (nHeight > g_nMessageIndexSyncedHeight)
            g_nMessageIndexSyncedHeight = nHeight;
        return true;
    }

    return false;
}

bool GetMessageRescanProgress(MessageRescanProgress &progress)
{
    LOCK(cs_messaging);
    progress = g_message_rescan;
    return true;
}

void SetMessageRescanProgress(const MessageRescanProgress &progress)
{
    LOCK(cs_messaging);
    g_message_rescan = progress;
}

void ClearMessageRescanProgress()
{
    LOCK(cs_messaging);
    g_message_rescan.Reset();
}

bool IsMessageRescanInProgress()
{
    LOCK(cs_messaging);
    return g_message_rescan.fInProgress;
}

bool TryStartMessageRescan(const MessageRescanProgress& initialProgress)
{
    LOCK(cs_messaging);
    if (g_message_rescan.fInProgress)
        return false;
    g_message_rescan = initialProgress;
    g_message_rescan.fInProgress = true;
    return true;
}

void LoadMessageIndexMetadata()
{
    LOCK(cs_messaging);

    g_nMessageIndexSyncedHeight = 0;

    if (!pmessagedb)
        return;

    int64_t value = 0;
    if (pmessagedb->ReadMetaInt64(MSG_META_SYNCED_HEIGHT, value) && value > 0)
        g_nMessageIndexSyncedHeight = static_cast<int>(value);

    // Detect -messageindex setting transitions across restarts. If the persisted
    // enabled flag differs from the current runtime flag, blocks were connected
    // under a different indexing mode since the last synced-height write, so the
    // synced height is no longer valid. Reset it so getmessaginginfo correctly
    // reports message_index_needs_rescan.
    bool fPersistedEnabled = false;
    bool fHasPersistedFlag = pmessagedb->ReadFlag(MSG_META_INDEX_ENABLED, fPersistedEnabled);
    if (fHasPersistedFlag && fPersistedEnabled != fMessageIndex) {
        LogPrintf("Message index setting changed (was %s, now %s); resetting message index synced height\n",
                  fPersistedEnabled ? "enabled" : "disabled",
                  fMessageIndex ? "enabled" : "disabled");
        g_nMessageIndexSyncedHeight = 0;
    }

    // Persist the current enabled flag immediately so the next startup can
    // detect transitions. Without this, the flag could go stale when users
    // disable -messageindex (PersistMessageIndexMetadata is only called during
    // flush when fMessageIndex is true).
    pmessagedb->WriteFlag(MSG_META_INDEX_ENABLED, fMessageIndex);
}

bool PersistMessageIndexMetadata()
{
    LOCK(cs_messaging);

    if (!pmessagedb)
        return false;

    bool fOk = true;
    fOk &= pmessagedb->WriteMetaInt64(MSG_META_SYNCED_HEIGHT, g_nMessageIndexSyncedHeight);
    fOk &= pmessagedb->WriteFlag(MSG_META_INDEX_ENABLED, fMessageIndex);
    return fOk;
}

void ResetMessageIndexMetadata()
{
    LOCK(cs_messaging);

    g_nMessageIndexSyncedHeight = 0;

    if (pmessagedb) {
        pmessagedb->WriteMetaInt64(MSG_META_SYNCED_HEIGHT, 0);
    }
}

size_t GetMessageDirtyCacheSize()
{
    LOCK(cs_messaging);

    size_t size = 0;
    // Messages Caches
    size += 32 * setDirtyMessagesRemove.size(); // COutPoint;
    size += (32 + 123) * mapDirtyMessagesAdd.size(); // COutPoint -> CMessage
    size += (32 + 123) * mapDirtyMessagesOrphaned.size(); // COutPoint -> CMessage


    // Message Channel Caches
    size += 32 * setDirtyChannelsAdd.size();
    size += 32 * setDirtyChannelsRemove.size();
    size += 32 * setSubscribedChannelsAskedForFalse.size();

    // Address Seen Caches
    size += 32 * setDirtySeenAddressAdd.size();
    size += 32 * setAddressAskedForFalse.size();

    return size;
}


std::string CZMQMessage::createJsonString()
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("blockheight", this->blockHeight);
    obj.pushKV("assetname", this->assetName);
    obj.pushKV("ipfshash", EncodeAssetData(this->ipfsHash));
    obj.pushKV("expiretime", this->nExpireTime);
    return obj.write();
}
