// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets/assets.h"
#include "assets/assetdb.h"
#include "assets/messages.h"
#include "assets/myassetsdb.h"
#include <map>
#include "tinyformat.h"

#include "amount.h"
#include "base58.h"
#include "chain.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "httpserver.h"
#include "validation.h"
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

UniValue getmessaginginfo(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "getmessaginginfo\n"
                "\nReturns the current messaging subsystem state.\n"
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

    obj.pushKV("warnings", warnings);

    return obj;
}

UniValue viewallmessages(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
                "viewallmessages \n"
                + MessageActivationWarning() +
                "\nView all messages that the wallet contains\n"
                "\nResult:\n"
                "\"Asset Name:\"                     (string) The name of the asset the message was sent on\n"
                "\"Message:\"                        (string) The IPFS hash of the message\n"
                "\"Time:\"                           (Date) The time as a date in the format (YY-mm-dd Hour-minute-second)\n"
                "\"Block Height:\"                   (number) The height of the block the message was included in\n"
                "\"Status:\"                         (string) Status of the message (READ, UNREAD, ORPHAN, EXPIRED, SPAM, HIDDEN, ERROR)\n"
                "\"Expire Time:\"                    (Date, optional) If the message had an expiration date assigned, it will be shown here in the format (YY-mm-dd Hour-minute-second)\n"
                "\"Expire UTC Time:\"                (Date, optional) If the message contains an expire date that is too large, the UTC number will be displayed\n"
                "\nExamples:\n"
                + HelpExampleCli("viewallmessages", "")
                + HelpExampleRpc("viewallmessages", "")
        );

    if (!fMessaging)
        return UniValue(UniValue::VARR);

    if (!AreMessagesDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "This command is not yet active. Messaging must be deployed first.");

    if (!pMessagesCache || !pmessagedb)
        return UniValue(UniValue::VARR);

    std::set<CMessage> setMessages;
    pmessagedb->LoadMessages(setMessages);

    {
        LOCK(cs_messaging);

        for (auto pair : mapDirtyMessagesOrphaned) {
            CMessage message = pair.second;
            message.status = MessageStatus::ORPHAN;
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
    }

    UniValue messages(UniValue::VARR);

    for (auto message : setMessages) {
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

        messages.push_back(obj);
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

    return "Erased " + std::to_string(count) + " Messages from the database and cache";
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

static const CRPCCommand commands[] =
    {           //  category    name                          actor (function)             argNames
                //  ----------- ------------------------      -----------------------      ----------
            { "messages",       "getmessaginginfo",           &getmessaginginfo,           {}},
            { "messages",       "viewallmessages",            &viewallmessages,            {}},
            { "messages",       "viewallmessagechannels",     &viewallmessagechannels,     {}},
            { "messages",       "subscribetochannel",         &subscribetochannel,         {"channel_name"}},
            { "messages",       "unsubscribefromchannel",     &unsubscribefromchannel,     {"channel_name"}},
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
