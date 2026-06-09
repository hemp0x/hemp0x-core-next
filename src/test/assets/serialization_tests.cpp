// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assets/assets.h>

#include <test/test_hemp0x.h>

#include <boost/test/unit_test.hpp>

#include <amount.h>
#include <base58.h>
#include <chainparams.h>
#include <wallet/wallet.h>

BOOST_FIXTURE_TEST_SUITE(serialization_tests, BasicTestingSetup)

    BOOST_AUTO_TEST_CASE(issue_asset_serialization_test)
    {
        BOOST_TEST_MESSAGE("Running Issue Asset Serialization Test");

        SelectParams("test");

        // Create asset
        CNewAsset asset("SERIALIZATION", 100000000, 0, 0, 1, DecodeAssetData("QmacSRmrkVmvJfbCpmU6pK72furJ8E8fbKHindrLxmYMQo"));

        // Create destination
        CTxDestination dest = DecodeDestination("mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp"); // Testnet Address

        BOOST_CHECK(IsValidDestination(dest));

        CScript scriptPubKey = GetScriptForDestination(dest);

        asset.ConstructTransaction(scriptPubKey);

        CNewAsset serializedAsset;
        std::string address;
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, serializedAsset, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.strName == "SERIALIZATION", "Asset names weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.nAmount == 100000000, "Amount weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.units == 0, "Units weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.nReissuable == 0, "Reissuable wasn't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.nHasIPFS == 1, "HasIPFS wasn't equal");
        BOOST_CHECK_MESSAGE(EncodeAssetData(serializedAsset.strIPFSHash) == "QmacSRmrkVmvJfbCpmU6pK72furJ8E8fbKHindrLxmYMQo", "IPFSHash wasn't equal");

        // Bare asset
        CNewAsset asset2("SERIALIZATION", 100000000);
        scriptPubKey = GetScriptForDestination(dest);
        asset2.ConstructTransaction(scriptPubKey);
        CNewAsset serializedAsset2;
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, serializedAsset2, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.strName == "SERIALIZATION", "Asset names weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.nAmount == 100000000, "Amount weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.units == 0, "Units weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.nReissuable == 1, "Reissuable wasn't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.nHasIPFS == 0, "HasIPFS wasn't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.strIPFSHash == "", "IPFSHash wasn't equal");


        // Asset with txid hash instead of ipfs hash
        CNewAsset asset3("SERIALIZATION", 100000000, 0, 1, 1, DecodeAssetData("9c2c8e121a0139ba39bffd3ca97267bca9d4c0c1e84ac0c34a883c28e7a912ca"));
        scriptPubKey = GetScriptForDestination(dest);
        asset3.ConstructTransaction(scriptPubKey);
        CNewAsset serializedAsset3;
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, serializedAsset3, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.strName == "SERIALIZATION", "Asset names weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.nAmount == 100000000, "Amount weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.units == 0, "Units weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.nReissuable == 1, "Reissuable wasn't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.nHasIPFS == 1, "HasIPFS wasn't equal");
        BOOST_CHECK_MESSAGE(EncodeAssetData(serializedAsset3.strIPFSHash) == "9c2c8e121a0139ba39bffd3ca97267bca9d4c0c1e84ac0c34a883c28e7a912ca", "Txid Hash wasn't equal");
    }

    BOOST_AUTO_TEST_CASE(reissue_asset_serialization_test)
    {
        BOOST_TEST_MESSAGE("Running Reissue Asset Serialization Test");

        SelectParams("test");

        // Create asset
        std::string name = "SERIALIZATION";
        CReissueAsset reissue(name, 100000000, 0, 0, DecodeAssetData("QmacSRmrkVmvJfbCpmU6pK72furJ8E8fbKHindrLxmYMQo"));

        // Create destination
        CTxDestination dest = DecodeDestination("mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp"); // Testnet Address

        BOOST_CHECK(IsValidDestination(dest));

        CScript scriptPubKey = GetScriptForDestination(dest);

        reissue.ConstructTransaction(scriptPubKey);

        CReissueAsset serializedAsset;
        std::string address;
        BOOST_CHECK_MESSAGE(ReissueAssetFromScript(scriptPubKey, serializedAsset, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.strName == "SERIALIZATION", "Asset names weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset.nAmount == 100000000, "Amount weren't equal");
        BOOST_CHECK_MESSAGE(EncodeAssetData(serializedAsset.strIPFSHash) == "QmacSRmrkVmvJfbCpmU6pK72furJ8E8fbKHindrLxmYMQo", "IPFSHash wasn't equal");

        // Empty IPFS
        CReissueAsset reissue2(name, 100000000, 0, 0, "");
        scriptPubKey = GetScriptForDestination(dest);
        reissue2.ConstructTransaction(scriptPubKey);
        CReissueAsset serializedAsset2;
        BOOST_CHECK_MESSAGE(ReissueAssetFromScript(scriptPubKey, serializedAsset2, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.strName == "SERIALIZATION", "Asset names weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.nAmount == 100000000, "Amount weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset2.strIPFSHash == "", "IPFSHash wasn't equal");

        // Txid Hash instead of IPFS
        CReissueAsset reissue3(name, 100000000, 0, 0, DecodeAssetData("9c2c8e121a0139ba39bffd3ca97267bca9d4c0c1e84ac0c34a883c28e7a912ca"));
        scriptPubKey = GetScriptForDestination(dest);
        reissue3.ConstructTransaction(scriptPubKey);
        CReissueAsset serializedAsset3;
        BOOST_CHECK_MESSAGE(ReissueAssetFromScript(scriptPubKey, serializedAsset3, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.strName == "SERIALIZATION", "Asset names weren't equal");
        BOOST_CHECK_MESSAGE(serializedAsset3.nAmount == 100000000, "Amount weren't equal");
        BOOST_CHECK_MESSAGE(EncodeAssetData(serializedAsset3.strIPFSHash) == "9c2c8e121a0139ba39bffd3ca97267bca9d4c0c1e84ac0c34a883c28e7a912ca", "IPFSHash wasn't equal");
    }

    BOOST_AUTO_TEST_CASE(owner_asset_serialization_test)
    {
        BOOST_TEST_MESSAGE("Running Owner ASset Serialization Test");

        SelectParams("test");

        // Create asset
        std::string name = "SERIALIZATION";
        CNewAsset asset(name, 100000000);

        // Create destination
        CTxDestination dest = DecodeDestination("mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp"); // Testnet Address

        BOOST_CHECK(IsValidDestination(dest));

        CScript scriptPubKey = GetScriptForDestination(dest);

        asset.ConstructOwnerTransaction(scriptPubKey);

        std::string strOwnerName;
        std::string address;
        std::stringstream ownerName;
        ownerName << name << OWNER_TAG;
        BOOST_CHECK_MESSAGE(OwnerAssetFromScript(scriptPubKey, strOwnerName, address), "Failed to get asset from script");
        BOOST_CHECK_MESSAGE(address == "mfe7MqgYZgBuXzrT2QTFqZwBXwRDqagHTp", "Addresses weren't equal");
        BOOST_CHECK_MESSAGE(strOwnerName == ownerName.str(), "Asset names weren't equal");
    }

    BOOST_AUTO_TEST_CASE(restricted_assets_deserialization)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset restricted_asset("$RESTRICTED", 1000, 8, 0, 1, "QmRAQB6YaCyidP37UdDnjFY5vQuiBrcqdyoW1CuDgwxkD4");

        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        restricted_asset.ConstructTransaction(scriptPubKey);

        BOOST_CHECK_MESSAGE(IsScriptNewRestrictedAsset(scriptPubKey), "Script wasn't a restricted asset");
    }

    BOOST_AUTO_TEST_CASE(message_channel_deserialization)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset message_channel("RESTRICTED~CHANNEL", 1000, 0, 0, 1, "QmRAQB6YaCyidP37UdDnjFY5vQuiBrcqdyoW1CuDgwxkD4");

        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        message_channel.ConstructTransaction(scriptPubKey);

        BOOST_CHECK_MESSAGE(IsScriptNewMsgChannelAsset(scriptPubKey), "Script wasn't a message channel");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_new_asset)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset asset("TESTASSET", 1000, 8, 0, 0, "");
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        asset.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for new asset");
        BOOST_CHECK_MESSAGE(data.type == TX_NEW_ASSET, "type should be TX_NEW_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "TESTASSET", "asset name mismatch");
        BOOST_CHECK_MESSAGE(data.nAmount == 1000, "amount mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_qualifier)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset qualifier("#MYQUAL", 5 * COIN, 0, 0, 0, "");
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        qualifier.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for qualifier");
        BOOST_CHECK_MESSAGE(data.type == TX_NEW_ASSET, "type should be TX_NEW_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "#MYQUAL", "asset name mismatch");
        BOOST_CHECK_MESSAGE(data.nAmount == 5 * COIN, "amount mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_sub_qualifier)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset subqual("#MYQUAL/#SUB1", 5 * COIN, 0, 0, 0, "");
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        subqual.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for sub-qualifier");
        BOOST_CHECK_MESSAGE(data.assetName == "#MYQUAL/#SUB1", "asset name mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_restricted)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset restricted("$MYRESTRICTED", 1000, 0, 0, 0, "");
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        restricted.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for restricted");
        BOOST_CHECK_MESSAGE(data.type == TX_NEW_ASSET, "type should be TX_NEW_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "$MYRESTRICTED", "asset name mismatch");
        BOOST_CHECK_MESSAGE(data.nAmount == 1000, "amount mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_msgchannel)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset msgchan("MYASSET~CHANNEL", 1000, 0, 0, 0, "");
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        msgchan.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for msgchannel");
        BOOST_CHECK_MESSAGE(data.type == TX_NEW_ASSET, "type should be TX_NEW_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "MYASSET~CHANNEL", "asset name mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_owner)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset asset("TESTASSET", 1000);
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        asset.ConstructOwnerTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for owner");
        BOOST_CHECK_MESSAGE(data.type == TX_NEW_ASSET, "type should be TX_NEW_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "TESTASSET!", "owner asset name mismatch");
        BOOST_CHECK_MESSAGE(data.nAmount == OWNER_ASSET_AMOUNT, "owner amount mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_transfer)
    {
        SelectParams(CBaseChainParams::MAIN);

        CAssetTransfer transfer("TESTASSET", 500);
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        transfer.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for transfer");
        BOOST_CHECK_MESSAGE(data.type == TX_TRANSFER_ASSET, "type should be TX_TRANSFER_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "TESTASSET", "asset name mismatch");
        BOOST_CHECK_MESSAGE(data.nAmount == 500, "amount mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_reissue)
    {
        SelectParams(CBaseChainParams::MAIN);

        CReissueAsset reissue("TESTASSET", 200, 8, 0, "");
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        reissue.ConstructTransaction(scriptPubKey);

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(GetAssetData(scriptPubKey, data), "GetAssetData should return true for reissue");
        BOOST_CHECK_MESSAGE(data.type == TX_REISSUE_ASSET, "type should be TX_REISSUE_ASSET");
        BOOST_CHECK_MESSAGE(data.assetName == "TESTASSET", "asset name mismatch");
        BOOST_CHECK_MESSAGE(data.nAmount == 200, "amount mismatch");
    }

    BOOST_AUTO_TEST_CASE(get_asset_data_non_asset)
    {
        SelectParams(CBaseChainParams::MAIN);

        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));

        CAssetOutputEntry data;
        BOOST_CHECK_MESSAGE(!GetAssetData(scriptPubKey, data), "GetAssetData should return false for non-asset");
    }

    BOOST_AUTO_TEST_CASE(asset_from_script_all_new_asset_subtypes)
    {
        SelectParams(CBaseChainParams::MAIN);

        CNewAsset parsed;
        std::string address;
        CScript scriptPubKey;

        // Root asset
        CNewAsset root("ROOTASSET", 1000, 8, 0, 0, "");
        scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        root.ConstructTransaction(scriptPubKey);
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, parsed, address), "AssetFromScript should parse root asset");
        BOOST_CHECK_MESSAGE(parsed.strName == "ROOTASSET", "root name mismatch");

        // Qualifier
        CNewAsset qual("#QUAL", 5 * COIN, 0, 0, 0, "");
        scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        qual.ConstructTransaction(scriptPubKey);
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, parsed, address), "AssetFromScript should parse qualifier");
        BOOST_CHECK_MESSAGE(parsed.strName == "#QUAL", "qualifier name mismatch");

        // Sub-qualifier
        CNewAsset subqual("#QUAL/#SUB", 5 * COIN, 0, 0, 0, "");
        scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        subqual.ConstructTransaction(scriptPubKey);
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, parsed, address), "AssetFromScript should parse sub-qualifier");
        BOOST_CHECK_MESSAGE(parsed.strName == "#QUAL/#SUB", "sub-qualifier name mismatch");

        // Restricted
        CNewAsset restr("$RESTRICTED", 1000, 0, 0, 0, "");
        scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        restr.ConstructTransaction(scriptPubKey);
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, parsed, address), "AssetFromScript should parse restricted");
        BOOST_CHECK_MESSAGE(parsed.strName == "$RESTRICTED", "restricted name mismatch");

        // Message channel
        CNewAsset msgchan("ASSET~CHAN", 1000, 0, 0, 0, "");
        scriptPubKey = GetScriptForDestination(DecodeDestination(GetParams().GlobalBurnAddress()));
        msgchan.ConstructTransaction(scriptPubKey);
        BOOST_CHECK_MESSAGE(AssetFromScript(scriptPubKey, parsed, address), "AssetFromScript should parse msgchannel");
        BOOST_CHECK_MESSAGE(parsed.strName == "ASSET~CHAN", "msgchannel name mismatch");
    }

BOOST_AUTO_TEST_SUITE_END()