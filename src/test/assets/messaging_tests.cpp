// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assets/assets.h>
#include <assets/messages.h>

#include <test/test_hemp0x.h>

#include <boost/test/unit_test.hpp>

#include <amount.h>
#include <base58.h>
#include <chainparams.h>
#include "consensus/consensus.h"
#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(messaging_tests, BasicTestingSetup)

    BOOST_AUTO_TEST_CASE(transfer_hashes_test)
    {
        std::string error = "";

        CAssetTransfer transfer1("ASSET", 1 * COIN, DecodeAssetData("QmRAQB6YaCyidP37UdDnjFY5vQuiBrcqdyoW1CuDgwxkD4"));
        BOOST_CHECK_MESSAGE(transfer1.IsValid(error), "Transfer Valid Test 1 - failed -" + error);

        // Asset transfer is Zero failure
        CAssetTransfer transfer3("ASSET", 0);
        BOOST_CHECK_MESSAGE(!transfer3.IsValid(error), "Transfer Valid Test 3 did not fail");

        // empty message with an expiration date failure
        std::string message = "";
        int64_t date = 15555555;
        CAssetTransfer transfer4("ASSET", 1 * COIN, message, date);
        transfer4.nExpireTime = date;
        BOOST_CHECK_MESSAGE(!transfer4.IsValid(error), "Transfer Valid Test 4 did not fail");

        // negative expiration date failure
        int64_t date2 = -1;
        CAssetTransfer transfer5("ASSET", 1 * COIN, message, date2);
        transfer5.nExpireTime = date2;
        BOOST_CHECK_MESSAGE(!transfer5.IsValid(error), "Transfer Valid Test 5 did not fail");
    }

    BOOST_AUTO_TEST_CASE(message_encoding_check)
    {
        std::string hash1 = "0000002a7eea17df5164b3dd8f49bbc3dc268d92c39bf62e17b4e07326a11609";
        std::string hash2 = "6d539a227b256e0fce13c57d75a9135aa133533d10a0bde055e9322c6bac9435";

        std::string ipfs1 = "QmVUXZ1UiwGVuKMPuBagveHexGiRRTQLN8JDrBKauECSFQ";
        std::string ipfs2 = "QmX6972nFtqu1Y15qy1jyQm5mkDQx7JSoF2LEAqtnvGYyv";

        auto decoded1 = DecodeAssetData(hash1);
        auto decoded2 = DecodeAssetData(hash2);

        auto ipfsdecoded1 = DecodeAssetData(ipfs1);
        auto ipfsdecoded2 = DecodeAssetData(ipfs2);

        std::string error = "";
        BOOST_CHECK_MESSAGE(IsHex(hash1) && hash1.length() == 64, "Test 1 Failed");
        BOOST_CHECK_MESSAGE(IsHex(hash2) && hash2.length() == 64, "Test 2 Failed ");
        BOOST_CHECK_MESSAGE(CheckEncoded(ipfsdecoded1, error), "Test 3 Failed - " +  error);
        BOOST_CHECK_MESSAGE(CheckEncoded(ipfsdecoded2, error), "Test 4 Failed - " +  error);
    }

    BOOST_AUTO_TEST_CASE(message_status_conversion)
    {
        // All known values round-trip
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::READ), "READ");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::UNREAD), "UNREAD");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::EXPIRED), "EXPIRED");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::SPAM), "SPAM");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::HIDDEN), "HIDDEN");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::ORPHAN), "ORPHAN");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatus::MSG_ERROR), "ERROR");

        // IntFromMessageStatus -> MessageStatusFromInt round trip
        for (int8_t i = 0; i <= 6; i++) {
            MessageStatus s = (MessageStatus)i;
            BOOST_CHECK_EQUAL((int8_t)MessageStatusFromInt(IntFromMessageStatus(s)), (int8_t)s);
        }

        // Out-of-range values clamp to MSG_ERROR
        BOOST_CHECK_MESSAGE(MessageStatusFromInt(-1) == MessageStatus::MSG_ERROR, "Clamp -1 failed");
        BOOST_CHECK_MESSAGE(MessageStatusFromInt(7) == MessageStatus::MSG_ERROR, "Clamp 7 failed");
        BOOST_CHECK_MESSAGE(MessageStatusFromInt(100) == MessageStatus::MSG_ERROR, "Clamp 100 failed");

        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatusFromInt(-1)), "ERROR");
        BOOST_CHECK_EQUAL(MessageStatusToString(MessageStatusFromInt(7)), "ERROR");
    }

    BOOST_AUTO_TEST_CASE(zmq_json_encoding)
    {
        COutPoint out(uint256(), 0);
        const std::string asset_name = "CHANNEL!\"\\\n";
        CMessage msg(out, asset_name, "ipfshash_raw", 0, 1234567890);
        msg.nBlockHeight = 42;
        msg.nExpiredTime = 9999999999;
        msg.ipfsHash = "QmTestIPFS";

        CZMQMessage zmqmsg(msg);
        std::string json = zmqmsg.createJsonString();

        BOOST_CHECK_MESSAGE(!json.empty(), "ZMQ JSON should not be empty");
        BOOST_CHECK_MESSAGE(json[0] == '{', "ZMQ JSON should start with '{'");
        BOOST_CHECK_MESSAGE(json[json.size() - 1] == '}', "ZMQ JSON should end with '}'");
        BOOST_CHECK_MESSAGE(json.find("\"blockheight\"") != std::string::npos, "Missing blockheight field");
        BOOST_CHECK_MESSAGE(json.find("\"assetname\"") != std::string::npos, "Missing assetname field");
        BOOST_CHECK_MESSAGE(json.find("\"ipfshash\"") != std::string::npos, "Missing ipfshash field");
        BOOST_CHECK_MESSAGE(json.find("\"expiretime\"") != std::string::npos, "Missing expiretime field");

        UniValue parsed;
        BOOST_CHECK_MESSAGE(parsed.read(json), "ZMQ JSON should parse as valid JSON");
        BOOST_CHECK_MESSAGE(parsed.isObject(), "ZMQ JSON should parse as an object");
        BOOST_CHECK_EQUAL(find_value(parsed.get_obj(), "blockheight").get_int(), 42);
        BOOST_CHECK_EQUAL(find_value(parsed.get_obj(), "assetname").get_str(), asset_name);
        BOOST_CHECK_EQUAL(find_value(parsed.get_obj(), "ipfshash").get_str(), EncodeAssetData(msg.ipfsHash));
        BOOST_CHECK_EQUAL(find_value(parsed.get_obj(), "expiretime").get_int64(), 9999999999);
    }

    BOOST_AUTO_TEST_CASE(channel_glob_match)
    {
        // Empty pattern matches everything.
        BOOST_CHECK(GlobMatchChannel("", "ROOT/H0XC!"));
        BOOST_CHECK(GlobMatchChannel("", ""));

        // Exact match (no wildcard).
        BOOST_CHECK(GlobMatchChannel("ROOT/H0XC!", "ROOT/H0XC!"));
        BOOST_CHECK(!GlobMatchChannel("ROOT/H0XC!", "ROOT/H0XC~ANN"));

        // Leading wildcard.
        BOOST_CHECK(GlobMatchChannel("*/H0XC!", "ROOT/H0XC!"));
        BOOST_CHECK(GlobMatchChannel("*/H0XC!", "GRIDSHADE/H0XC!"));
        BOOST_CHECK(!GlobMatchChannel("*/H0XC!", "ROOT/H0XC~ANN"));

        // Trailing wildcard.
        BOOST_CHECK(GlobMatchChannel("ROOT/*", "ROOT/H0XC!"));
        BOOST_CHECK(GlobMatchChannel("ROOT/*", "ROOT/SUB!"));

        // Middle wildcard.
        BOOST_CHECK(GlobMatchChannel("R*!", "ROOT!"));
        BOOST_CHECK(GlobMatchChannel("R*!", "RAID!"));
        BOOST_CHECK(!GlobMatchChannel("R*!", "ROOT"));

        // Multiple wildcards / full match.
        BOOST_CHECK(GlobMatchChannel("*", "ANYTHING!"));
        BOOST_CHECK(GlobMatchChannel("*H0XC*", "ROOT/H0XC!"));
        BOOST_CHECK(GlobMatchChannel("*H0XC*", "ROOT/H0XC~ANN"));
        BOOST_CHECK(!GlobMatchChannel("*H0XC*", "ROOT/OTHER!"));
    }

    BOOST_AUTO_TEST_CASE(message_index_synced_height_monotonic)
    {
        // SetMessageIndexSyncedHeight only advances (never decreases) so a reorg
        // to a lower tip followed by reconnect does not regress the marker.
        SetMessageIndexSyncedHeight(0);
        BOOST_CHECK_EQUAL(GetMessageIndexSyncedHeight(), 0);

        SetMessageIndexSyncedHeight(100);
        BOOST_CHECK_EQUAL(GetMessageIndexSyncedHeight(), 100);

        // Lower values must be ignored.
        SetMessageIndexSyncedHeight(50);
        BOOST_CHECK_EQUAL(GetMessageIndexSyncedHeight(), 100);

        SetMessageIndexSyncedHeight(150);
        BOOST_CHECK_EQUAL(GetMessageIndexSyncedHeight(), 150);

        // Reset (as clearmessages does) brings it back to 0.
        ResetMessageIndexMetadata();
        BOOST_CHECK_EQUAL(GetMessageIndexSyncedHeight(), 0);
    }

    BOOST_AUTO_TEST_CASE(rescan_progress_round_trip)
    {
        ClearMessageRescanProgress();

        MessageRescanProgress p;
        GetMessageRescanProgress(p);
        BOOST_CHECK(!p.fInProgress);
        BOOST_CHECK_EQUAL(p.nScannedBlocks, 0);

        p.fInProgress = true;
        p.nStartHeight = 270144;
        p.nStopHeight = 2787000;
        p.nCurrentHeight = 271000;
        p.nScannedBlocks = 856;
        p.nMessagesFound = 12;
        p.nMessagesAdded = 12;
        p.strLastError = "";
        p.nStartedAt = 1700000000;
        SetMessageRescanProgress(p);

        MessageRescanProgress q;
        GetMessageRescanProgress(q);
        BOOST_CHECK(q.fInProgress);
        BOOST_CHECK_EQUAL(q.nStartHeight, 270144);
        BOOST_CHECK_EQUAL(q.nStopHeight, 2787000);
        BOOST_CHECK_EQUAL(q.nCurrentHeight, 271000);
        BOOST_CHECK_EQUAL(q.nScannedBlocks, 856);
        BOOST_CHECK_EQUAL(q.nMessagesFound, 12);
        BOOST_CHECK_EQUAL(q.nMessagesAdded, 12);
        BOOST_CHECK_EQUAL(q.nStartedAt, 1700000000);

        ClearMessageRescanProgress();
        GetMessageRescanProgress(q);
        BOOST_CHECK(!q.fInProgress);
        BOOST_CHECK_EQUAL(q.nScannedBlocks, 0);
    }

BOOST_AUTO_TEST_SUITE_END()
