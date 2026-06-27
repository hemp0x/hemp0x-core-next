// Copyright (c) 2018-2020 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef HEMP0X_MESSAGES_H
#define HEMP0X_MESSAGES_H

#include <uint256.h>
#include <serialize.h>
#include <sync.h>

class CMessage;
class COutPoint;

// Message Database caches
extern std::set<COutPoint> setDirtyMessagesRemove;
extern std::map<COutPoint, CMessage> mapDirtyMessagesAdd;
extern std::map<COutPoint, CMessage> mapDirtyMessagesOrphaned;

// Message Channel Database caches
extern std::set<std::string> setDirtyChannelsAdd;
extern std::set<std::string> setDirtyChannelsRemove;
extern std::set<std::string> setSubscribedChannelsAskedForFalse;

// Spam prevention address index
extern std::set<std::string> setDirtySeenAddressAdd;
extern std::set<std::string> setAddressAskedForFalse;

// Lock for messaging
extern CCriticalSection cs_messaging;

size_t GetMessageDirtyCacheSize();
bool IsChannelSubscribed(const std::string &name); // Is this channel marked as spamA

bool GetMessage(const COutPoint &out, CMessage &message);

void AddChannel(const std::string &name);

void RemoveChannel(const std::string &name);

void AddMessage(const CMessage &message);

void RemoveMessage(const CMessage &message);
void RemoveMessage(const COutPoint &out);

void OrphanMessage(const CMessage &message);
void OrphanMessage(const COutPoint &out);

// Simple '*' glob matcher used for channel/pattern filters (e.g. "*/H0XC!").
// An empty pattern matches everything.
bool GlobMatchChannel(const std::string& pattern, const std::string& channel);

#ifdef ENABLE_WALLET
bool ScanForMessageChannels(std::string& strError);
#endif
bool IsAddressSeen(const std::string &address); // Has this address already been sent an asset before
void AddAddressSeen(const std::string &address);

// In-progress rescan progress snapshot. Updated by rescanmessages and read by
// getmessaginginfo so Commander can poll a background rescan from a separate
// RPC connection. Guarded by cs_messaging.
struct MessageRescanProgress {
    bool fInProgress;
    int nStartHeight;
    int nStopHeight;
    int nCurrentHeight;
    int nScannedBlocks;
    int nMessagesFound;
    int nMessagesAdded;
    std::string strLastError;
    int64_t nStartedAt;
    int64_t nFinishedAt;

    MessageRescanProgress() { Reset(); }
    void Reset() {
        fInProgress = false;
        nStartHeight = 0;
        nStopHeight = 0;
        nCurrentHeight = 0;
        nScannedBlocks = 0;
        nMessagesFound = 0;
        nMessagesAdded = 0;
        strLastError.clear();
        nStartedAt = 0;
        nFinishedAt = 0;
    }
};

// Persisted message-index metadata keys (stored in CMessageDB under MESSAGE_META)
extern const std::string MSG_META_SYNCED_HEIGHT;
extern const std::string MSG_META_LAST_SCAN_START;
extern const std::string MSG_META_LAST_SCAN_STOP;
extern const std::string MSG_META_LAST_SCAN_COMPLETED;
extern const std::string MSG_META_INDEX_ENABLED;

// Last block height indexed for messages under full-index (-messageindex=1) mode.
int GetMessageIndexSyncedHeight();
// Monotonic setter: only advances the synced height (never decreases). Used by
// normal block connection and rescan success paths.
void SetMessageIndexSyncedHeight(int nHeight);
// Exact setter: sets the synced height to nHeight regardless of the current
// value. Used to restore a previous synced height after a failed rescan
// candidate advance. Not for normal block-connection use.
void SetMessageIndexSyncedHeightExact(int nHeight);
// Contiguous advance: advances the synced height to nHeight only if coverage
// is contiguous — i.e. the index is already synced through nHeight-1, or
// nHeight is at/below the messaging activation boundary. Returns true if the
// marker was advanced, false if a gap prevented advancement. Used by
// ConnectBlock so new blocks arriving after a historical gap do not hide the
// gap from getmessaginginfo.
bool AdvanceMessageIndexSyncedHeightIfContiguous(int nHeight, int nActivationHeight);

// Snapshot/copy of the current (in-progress or last) rescan progress.
bool GetMessageRescanProgress(MessageRescanProgress &progress);
void SetMessageRescanProgress(const MessageRescanProgress &progress);
void ClearMessageRescanProgress();

// Returns true if a rescanmessages scan is currently in progress. Used by
// tests and getmessaginginfo for state reporting. This must NOT be used as the
// start guard by rescanmessages — use TryStartMessageRescan() instead, which
// atomically checks and sets the in-progress flag under cs_messaging to avoid
// a TOCTOU race between two concurrent RPC threads.
bool IsMessageRescanInProgress();

// Atomically check-and-start a rescan under cs_messaging. If no rescan is in
// progress, sets g_message_rescan = initialProgress (with fInProgress forced
// true) and returns true. If a rescan is already in progress, returns false
// without modifying state. This is the single correct entry point for starting
// a rescanmessages scan.
bool TryStartMessageRescan(const MessageRescanProgress& initialProgress);

// Load persisted message-index metadata into in-memory state at startup.
void LoadMessageIndexMetadata();
// Persist in-memory message-index metadata (synced height, enabled flag) to disk.
// Returns true if all writes succeeded, false on any write failure.
bool PersistMessageIndexMetadata();
// Reset message-index metadata to defaults (used by clearmessages).
void ResetMessageIndexMetadata();

enum class MessageStatus {
    READ = 0,
    UNREAD = 1,
    EXPIRED = 2,
    SPAM = 3,
    HIDDEN = 4,
    ORPHAN = 5,
    MSG_ERROR = 6
};

int8_t IntFromMessageStatus(MessageStatus status);
MessageStatus MessageStatusFromInt(int8_t nStatus);

std::string MessageStatusToString(MessageStatus status);

class CMessage {
public:

    COutPoint out;
    std::string strName;
    std::string ipfsHash;
    int64_t time;
    int64_t nExpiredTime;
    MessageStatus status;
    int nBlockHeight;

    CMessage();

    void SetNull() {
        nExpiredTime = 0;
        out = COutPoint();
        strName = "";
        ipfsHash = "";
        time = 0;
        status = MessageStatus::MSG_ERROR;
        nBlockHeight = 0;
    }

    std::string ToString() const {
        return strprintf("CMessage(%s, Name=%s, Message=%s, Expires=%u, Time=%u, BlockHeight=%u)", out.ToString(), strName,
                         EncodeAssetData(ipfsHash), nExpiredTime, time, nBlockHeight);
    }

    CMessage(const COutPoint &out, const std::string &strName, const std::string &ipfsHash, const int64_t &nExpiredTime,
             const int64_t &time);

    bool operator<(const CMessage &rhs) const {
        return out < rhs.out;
    }

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(out);
        READWRITE(strName);
        READWRITE(ipfsHash);
        READWRITE(time);
        READWRITE(nExpiredTime);
        READWRITE(nBlockHeight);

        if (ser_action.ForRead()) {
            int8_t nStatus = 6;
            ::Unserialize(s, nStatus);
            status = MessageStatusFromInt(nStatus);
        } else {
            ::Serialize(s, IntFromMessageStatus(status));
        }
    }
};

class CZMQMessage {
public:
    int blockHeight;
    std::string assetName;
    std::string ipfsHash;
    int64_t nExpireTime;

    CZMQMessage(const CMessage& message) {
        this->blockHeight = message.nBlockHeight;
        this->assetName = message.strName;
        this->ipfsHash = message.ipfsHash;
        this->nExpireTime = message.nExpiredTime;
    }

    std::string createJsonString();
};

#endif //HEMP0X_MESSAGES_H
