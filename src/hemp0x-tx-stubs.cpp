// Copyright (c) 2026 The Hemp0x Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Offline-safe adapters for asset, messaging, and wallet symbols pulled in by
// coins.cpp and core_write.cpp when compiling hemp0x-tx.
//
// hemp0x-tx is an offline transaction utility; it does not connect to the
// network, does not maintain a UTXO set in persistent storage, and does not
// validate asset protocol rules. These definitions return safe-default values
// (false, empty, or no-op) so that the linker does not drag in the
// entire libhemp0x_server.a and libhemp0x_wallet.a.

#if defined(HAVE_CONFIG_H)
#include "config/hemp0x-config.h"
#endif

#include "coins.h"
#include "assets/assets.h"
#include "wallet/wallet.h"
#include "consensus/consensus.h"
#include "sync.h"
#include "primitives/block.h"
#include "script/ismine.h"
#include "tinyformat.h"

CCriticalSection cs_messaging;
CCriticalSection cs_wallets;
std::vector<CWalletRef> vpwallets;
bool fMessaging = false;
bool fAssetsIsActive = false;
CLRUCache<std::string, int> *pMessageSubscribedChannelsCache = nullptr;

bool AreAssetsDeployed()
{
    return false;
}

unsigned int GetMaxBlockWeight()
{
    return MAX_BLOCK_WEIGHT;
}

bool CTransaction::IsNewAsset() const
{
    return false;
}
bool CTransaction::IsNewUniqueAsset() const
{
    return false;
}
bool CTransaction::IsReissueAsset() const
{
    return false;
}
bool CTransaction::IsNewMsgChannelAsset() const
{
    return false;
}
bool CTransaction::IsNewQualifierAsset() const
{
    return false;
}
bool CTransaction::IsNewRestrictedAsset() const
{
    return false;
}

bool IsAssetNameValid(const std::string& name)
{
    return false;
}
bool IsAssetNameValid(const std::string& name, AssetType& assetType)
{
    return false;
}
bool IsAssetNameValid(const std::string& name, AssetType& assetType, std::string& error)
{
    return false;
}
bool IsAssetNameAnOwner(const std::string& name)
{
    return false;
}
std::string GetParentName(const std::string& name)
{
    return "";
}

bool AssetFromTransaction(const CTransaction& tx, CNewAsset& asset, std::string& strAddress)
{
    return false;
}
bool OwnerFromTransaction(const CTransaction& tx, std::string& ownerName, std::string& strAddress)
{
    return false;
}
bool ReissueAssetFromTransaction(const CTransaction& tx, CReissueAsset& reissue, std::string& strAddress)
{
    return false;
}
bool MsgChannelAssetFromTransaction(const CTransaction& tx, CNewAsset& asset, std::string& strAddress)
{
    return false;
}
bool QualifierAssetFromTransaction(const CTransaction& tx, CNewAsset& asset, std::string& strAddress)
{
    return false;
}
bool RestrictedAssetFromTransaction(const CTransaction& tx, CNewAsset& asset, std::string& strAddress)
{
    return false;
}
bool AssetFromScript(const CScript& scriptPubKey, CNewAsset& asset, std::string& strAddress)
{
    return false;
}
bool ReissueAssetFromScript(const CScript& scriptPubKey, CReissueAsset& reissue, std::string& strAddress)
{
    return false;
}
bool AssetNullDataFromScript(const CScript& scriptPubKey, CNullAssetTxData& assetData, std::string& strAddress)
{
    return false;
}
bool GlobalAssetNullDataFromScript(const CScript& scriptPubKey, CNullAssetTxData& assetData)
{
    return false;
}
bool AssetNullVerifierDataFromScript(const CScript& scriptPubKey, CNullAssetTxVerifierString& verifierData)
{
    return false;
}
bool IsScriptNewUniqueAsset(const CScript& scriptPubKey)
{
    return false;
}

bool GetAssetData(const CScript& script, CAssetOutputEntry& data)
{
    return false;
}
std::string EncodeAssetData(std::string decoded)
{
    return "";
}

bool IsChannelSubscribed(const std::string& name)
{
    return false;
}
void AddChannel(const std::string& name)
{
}
bool IsAddressSeen(const std::string& address)
{
    return false;
}
void AddAddressSeen(const std::string& address)
{
}

bool CAssetsCache::AddNewAsset(const CNewAsset& asset, const std::string address, const int& nHeight, const uint256& blockHash)
{
    return true;
}
bool CAssetsCache::AddTransferAsset(const CAssetTransfer& transferAsset, const std::string& address, const COutPoint& out, const CTxOut& txOut)
{
    return true;
}
bool CAssetsCache::AddReissueAsset(const CReissueAsset& reissue, const std::string address, const COutPoint& out)
{
    return true;
}
bool CAssetsCache::AddOwnerAsset(const std::string& assetsName, const std::string address)
{
    return true;
}
bool CAssetsCache::AddRestrictedVerifier(const std::string& name, const std::string& verifier)
{
    return true;
}
bool CAssetsCache::AddGlobalRestricted(const std::string& name, RestrictedType type)
{
    return true;
}
bool CAssetsCache::AddRestrictedAddress(const std::string& name, const std::string& address, RestrictedType type)
{
    return true;
}
bool CAssetsCache::AddQualifierAddress(const std::string& name, const std::string& address, QualifierType type)
{
    return true;
}
bool CAssetsCache::GetAssetMetaDataIfExists(const std::string& name, CNewAsset& asset)
{
    return false;
}
bool CAssetsCache::GetAssetVerifierStringIfExists(const std::string& name, CNullAssetTxVerifierString& verifier, bool fSkipTempCache)
{
    return false;
}
bool CAssetsCache::TrySpendCoin(const COutPoint& out, const CTxOut& coin)
{
    return true;
}

isminetype CWallet::IsMine(const CTxOut& out) const
{
    return ISMINE_NO;
}

CNullAssetTxVerifierString::CNullAssetTxVerifierString(const std::string& verifier_string) :
    verifier_string(verifier_string)
{
}

CAssetTransfer::CAssetTransfer(const std::string& strAssetName, const CAmount& nAmount, const std::string& message, const int64_t& nExpireTime)
{
}
