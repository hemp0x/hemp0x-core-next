// Copyright (c) 2025 The hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// HISTORICAL VALIDATION BASELINE (NON-CONSENSUS GUARDRAIL)
// Purpose: Prove that mainnet chain parameters and trust anchors have not
// accidentally drifted. This test suite does NOT change any consensus rules,
// chain parameters, genesis data, ports, prefixes, checkpoints, assume-valid,
// minimum-chain-work, or deployment metadata. It is a read-only invariant
// check that reviewers and future coders can run before and after sensitive
// modernization work (secp256k1, hashing, LevelDB, wallet-backend, etc.).
//
// Every constant here is copied verbatim from src/chainparams.cpp at the
// time of this baseline. If you intentionally change chain parameters you
// MUST also update these expected constants after careful review.

#include "chainparams.h"
#include "chainparamsbase.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "test/test_hemp0x.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(chainparams_tests, BasicTestingSetup)

// -----------------------------------------------------------------------
// Mainnet genesis block invariants
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_genesis_invariants)
{
    BOOST_TEST_MESSAGE("Mainnet genesis block invariants");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // Genesis block hash — must match the asserted hash in chainparams.cpp
    BOOST_CHECK_EQUAL(
        consensus.hashGenesisBlock.GetHex(),
        "0000009907c43e63467860fcb2a76eed0200e4f918de00bfea3fa35aa22dddd0");

    // Genesis merkle root — must match the asserted merkle root
    BOOST_CHECK_EQUAL(
        chainParams->GenesisBlock().hashMerkleRoot.GetHex(),
        "da4b86406741b5e092c610a5be485e27ea86d265bd8d772670aeb771bdbf318a");

    // Genesis block version, timestamp, nonce, bits
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nVersion, 4);
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nTime, 1766119732u);
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nNonce, 1865918u);
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nBits, 0x1e0fffffu);
}

// -----------------------------------------------------------------------
// Mainnet message start bytes
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_message_start)
{
    BOOST_TEST_MESSAGE("Mainnet message start bytes");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const auto& magic = chainParams->MessageStart();

    // "HEMP" = 0x48, 0x45, 0x4d, 0x50
    BOOST_CHECK_EQUAL(magic[0], 0x48);
    BOOST_CHECK_EQUAL(magic[1], 0x45);
    BOOST_CHECK_EQUAL(magic[2], 0x4d);
    BOOST_CHECK_EQUAL(magic[3], 0x50);
}

// -----------------------------------------------------------------------
// Mainnet port
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_default_port)
{
    BOOST_TEST_MESSAGE("Mainnet default port");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 42069);
}

// -----------------------------------------------------------------------
// Mainnet address prefixes (base58)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_address_prefixes)
{
    BOOST_TEST_MESSAGE("Mainnet address prefixes");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    // PUBKEY_ADDRESS = 60 (0x3c) — mainnet P2PKH starts with 'R'
    const auto& pubkeyPrefix = chainParams->Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    BOOST_REQUIRE_EQUAL(pubkeyPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(pubkeyPrefix[0], 60);

    // SCRIPT_ADDRESS = 122 (0x7a) — mainnet P2SH starts with 'r'
    const auto& scriptPrefix = chainParams->Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    BOOST_REQUIRE_EQUAL(scriptPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(scriptPrefix[0], 122);

    // SECRET_KEY = 128 (0x80) — mainnet WIF private key
    const auto& secretPrefix = chainParams->Base58Prefix(CChainParams::SECRET_KEY);
    BOOST_REQUIRE_EQUAL(secretPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(secretPrefix[0], 128);

    // EXT_PUBLIC_KEY = 0x0488B21E (xpub)
    const auto& extPub = chainParams->Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    BOOST_REQUIRE_EQUAL(extPub.size(), 4u);
    BOOST_CHECK_EQUAL(extPub[0], 0x04);
    BOOST_CHECK_EQUAL(extPub[1], 0x88);
    BOOST_CHECK_EQUAL(extPub[2], 0xB2);
    BOOST_CHECK_EQUAL(extPub[3], 0x1E);

    // EXT_SECRET_KEY = 0x0488ADE4 (xprv)
    const auto& extSec = chainParams->Base58Prefix(CChainParams::EXT_SECRET_KEY);
    BOOST_REQUIRE_EQUAL(extSec.size(), 4u);
    BOOST_CHECK_EQUAL(extSec[0], 0x04);
    BOOST_CHECK_EQUAL(extSec[1], 0x88);
    BOOST_CHECK_EQUAL(extSec[2], 0xAD);
    BOOST_CHECK_EQUAL(extSec[3], 0xE4);
}

// -----------------------------------------------------------------------
// Mainnet ext coin type (BIP44)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_ext_coin_type)
{
    BOOST_TEST_MESSAGE("Mainnet ext coin type");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->ExtCoinType(), 420);
}

// -----------------------------------------------------------------------
// Mainnet block spacing
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_block_spacing)
{
    BOOST_TEST_MESSAGE("Mainnet block spacing");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // Target spacing: 5 seconds
    BOOST_CHECK_EQUAL(consensus.nPowTargetSpacing, 5);

    // Target timespan: 1 week (7 * 24 * 60 * 60)
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan, 7 * 24 * 60 * 60);

    // Difficulty adjustment interval = timespan / spacing
    BOOST_CHECK_EQUAL(consensus.DifficultyAdjustmentInterval(), 120960);
}

// -----------------------------------------------------------------------
// Mainnet max reorg depth
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_max_reorg_depth)
{
    BOOST_TEST_MESSAGE("Mainnet max reorg depth");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->MaxReorganizationDepth(), 60);
    BOOST_CHECK_EQUAL(chainParams->MinReorganizationPeers(), 4);
    BOOST_CHECK_EQUAL(chainParams->MinReorganizationAge(), 60 * 60 * 12);
}

// -----------------------------------------------------------------------
// Mainnet assume-valid hash (block 2,000,000)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_assume_valid)
{
    BOOST_TEST_MESSAGE("Mainnet assume-valid hash");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(
        consensus.defaultAssumeValid.GetHex(),
        "000000002f781ea4d01f5866a8f26747c245ec90111099af851a40144b37e118");
}

// -----------------------------------------------------------------------
// Mainnet minimum chain work
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_minimum_chain_work)
{
    BOOST_TEST_MESSAGE("Mainnet minimum chain work");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(
        consensus.nMinimumChainWork.GetHex(),
        "000000000000000000000000000000000000000000000000000548e1227c9f11");
}

// -----------------------------------------------------------------------
// Mainnet chainTxData (for sync progress estimation)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_chain_tx_data)
{
    BOOST_TEST_MESSAGE("Mainnet chainTxData");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const ChainTxData& txData = chainParams->TxData();

    BOOST_CHECK_EQUAL(txData.nTime, 1777085061);
    BOOST_CHECK_EQUAL(txData.nTxCount, 2116499);
    BOOST_CHECK(txData.dTxRate > 0.0);
}

// -----------------------------------------------------------------------
// Mainnet consensus flags (BIP34, BIP65, BIP66, SegWit, CSV)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_consensus_flags)
{
    BOOST_TEST_MESSAGE("Mainnet consensus flags");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK(consensus.nBIP34Enabled);
    BOOST_CHECK(consensus.nBIP65Enabled);
    BOOST_CHECK(consensus.nBIP66Enabled);
    BOOST_CHECK(consensus.nSegwitEnabled);
    BOOST_CHECK(consensus.nCSVEnabled);

    BOOST_CHECK(!consensus.fPowAllowMinDifficultyBlocks);
    BOOST_CHECK(!consensus.fPowNoRetargeting);
}

// -----------------------------------------------------------------------
// Mainnet subsidy halving interval
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_subsidy_halving_interval)
{
    BOOST_TEST_MESSAGE("Mainnet subsidy halving interval");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(consensus.nSubsidyHalvingInterval, 25000000);
}

// -----------------------------------------------------------------------
// Mainnet PoW limits
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_pow_limits)
{
    BOOST_TEST_MESSAGE("Mainnet PoW limits");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(
        consensus.powLimit.GetHex(),
        "00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    BOOST_CHECK_EQUAL(
        consensus.kawpowLimit.GetHex(),
        "0000000000ffffffffffffffffffffffffffffffffffffffffffffffffffffff");
}

// -----------------------------------------------------------------------
// Mainnet VersionBits deployment metadata
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_versionbits_deployments)
{
    BOOST_TEST_MESSAGE("Mainnet VersionBits deployment metadata");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // DEPLOYMENT_TESTDUMMY
    {
        const auto& d = consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY];
        BOOST_CHECK_EQUAL(d.bit, 28);
        BOOST_CHECK_EQUAL(d.nStartTime, 1199145601);
        BOOST_CHECK_EQUAL(d.nTimeout, 1230767999);
        BOOST_CHECK_EQUAL(d.nOverrideRuleChangeActivationThreshold, 1814u);
        BOOST_CHECK_EQUAL(d.nOverrideMinerConfirmationWindow, 2016u);
    }

    // DEPLOYMENT_ASSETS (RIP2) — bit 6
    {
        const auto& d = consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS];
        BOOST_CHECK_EQUAL(d.bit, 6);
        BOOST_CHECK_EQUAL(d.nStartTime, 1735689600);
        BOOST_CHECK_EQUAL(d.nTimeout, 1798761600);
        BOOST_CHECK_EQUAL(d.nOverrideRuleChangeActivationThreshold, 1814u);
        BOOST_CHECK_EQUAL(d.nOverrideMinerConfirmationWindow, 2016u);
    }

    // DEPLOYMENT_MSG_REST_ASSETS (RIP5 / Restricted assets) — bit 7
    {
        const auto& d = consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS];
        BOOST_CHECK_EQUAL(d.bit, 7);
        BOOST_CHECK_EQUAL(d.nStartTime, 1735689600);
        BOOST_CHECK_EQUAL(d.nTimeout, 1798761600);
        BOOST_CHECK_EQUAL(d.nOverrideRuleChangeActivationThreshold, 1714u);
        BOOST_CHECK_EQUAL(d.nOverrideMinerConfirmationWindow, 2016u);
    }

    // DEPLOYMENT_TRANSFER_SCRIPT_SIZE — bit 8
    {
        const auto& d = consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE];
        BOOST_CHECK_EQUAL(d.bit, 8);
        BOOST_CHECK_EQUAL(d.nStartTime, 1735689600);
        BOOST_CHECK_EQUAL(d.nTimeout, 1798761600);
        BOOST_CHECK_EQUAL(d.nOverrideRuleChangeActivationThreshold, 1714u);
        BOOST_CHECK_EQUAL(d.nOverrideMinerConfirmationWindow, 2016u);
    }

    // DEPLOYMENT_ENFORCE_VALUE — bit 9
    {
        const auto& d = consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE];
        BOOST_CHECK_EQUAL(d.bit, 9);
        BOOST_CHECK_EQUAL(d.nStartTime, 1735689600);
        BOOST_CHECK_EQUAL(d.nTimeout, 1798761600);
        BOOST_CHECK_EQUAL(d.nOverrideRuleChangeActivationThreshold, 1411u);
        BOOST_CHECK_EQUAL(d.nOverrideMinerConfirmationWindow, 2016u);
    }

    // DEPLOYMENT_COINBASE_ASSETS — bit 10
    {
        const auto& d = consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS];
        BOOST_CHECK_EQUAL(d.bit, 10);
        BOOST_CHECK_EQUAL(d.nStartTime, 1735689600);
        BOOST_CHECK_EQUAL(d.nTimeout, 1798761600);
        BOOST_CHECK_EQUAL(d.nOverrideRuleChangeActivationThreshold, 1411u);
        BOOST_CHECK_EQUAL(d.nOverrideMinerConfirmationWindow, 2016u);
    }
}

// -----------------------------------------------------------------------
// Mainnet checkpoints: strictly ascending by height and non-null
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_checkpoints)
{
    BOOST_TEST_MESSAGE("Mainnet checkpoints");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const CCheckpointData& cpData = chainParams->Checkpoints();
    const MapCheckpoints& cps = cpData.mapCheckpoints;

    BOOST_REQUIRE(!cps.empty());

    int prevHeight = -1;
    for (const auto& entry : cps) {
        int height = entry.first;
        const uint256& hash = entry.second;

        BOOST_CHECK_MESSAGE(height > prevHeight,
            "Checkpoint heights not strictly ascending: " << prevHeight << " -> " << height);
        BOOST_CHECK_MESSAGE(!hash.IsNull(),
            "Checkpoint hash is null at height " << height);

        prevHeight = height;
    }
}

// -----------------------------------------------------------------------
// Mainnet checkpoints: must include the current trust-anchor heights
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_checkpoint_trust_anchors)
{
    BOOST_TEST_MESSAGE("Mainnet checkpoint trust anchors");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const CCheckpointData& cpData = chainParams->Checkpoints();
    const MapCheckpoints& cps = cpData.mapCheckpoints;

    // Genesis (height 0)
    BOOST_CHECK_MESSAGE(cps.count(0) > 0, "Missing checkpoint at height 0");
    if (cps.count(0)) {
        BOOST_CHECK_EQUAL(cps.at(0).GetHex(),
            "0000009907c43e63467860fcb2a76eed0200e4f918de00bfea3fa35aa22dddd0");
    }

    // Block 270,144
    BOOST_CHECK_MESSAGE(cps.count(270144) > 0, "Missing checkpoint at height 270144");
    if (cps.count(270144)) {
        BOOST_CHECK_EQUAL(cps.at(270144).GetHex(),
            "0000000582ebb3cb1f1590c43cae7b12b1119178cbd71cbcb6b42084bbe5b085");
    }

    // Block 274,176
    BOOST_CHECK_MESSAGE(cps.count(274176) > 0, "Missing checkpoint at height 274176");
    if (cps.count(274176)) {
        BOOST_CHECK_EQUAL(cps.at(274176).GetHex(),
            "000000018f0fb60cc898229bf3086df590546354d4a0ebd922c5fa4d2b4c91bf");
    }

    // Block 500,000
    BOOST_CHECK_MESSAGE(cps.count(500000) > 0, "Missing checkpoint at height 500000");
    if (cps.count(500000)) {
        BOOST_CHECK_EQUAL(cps.at(500000).GetHex(),
            "00000000c1ca9606453b80f39abf80f99edbb67a1f4a97a35438fae825b92db2");
    }

    // Block 1,000,000
    BOOST_CHECK_MESSAGE(cps.count(1000000) > 0, "Missing checkpoint at height 1000000");
    if (cps.count(1000000)) {
        BOOST_CHECK_EQUAL(cps.at(1000000).GetHex(),
            "0000000489c1038667e941aeafe4177115300da01843adaf36b889a804810bca");
    }

    // Block 1,500,000
    BOOST_CHECK_MESSAGE(cps.count(1500000) > 0, "Missing checkpoint at height 1500000");
    if (cps.count(1500000)) {
        BOOST_CHECK_EQUAL(cps.at(1500000).GetHex(),
            "00000004cfb28dcb6c737915b008f8c5824c8201670875f7461f4019f8e65ebb");
    }

    // Block 2,000,000 (also the assume-valid anchor)
    BOOST_CHECK_MESSAGE(cps.count(2000000) > 0, "Missing checkpoint at height 2000000");
    if (cps.count(2000000)) {
        BOOST_CHECK_EQUAL(cps.at(2000000).GetHex(),
            "000000002f781ea4d01f5866a8f26747c245ec90111099af851a40144b37e118");
    }

    // Block 3,887,915 (KAWPOW declared-height rollout anchor)
    BOOST_CHECK_MESSAGE(cps.count(3887915) > 0, "Missing checkpoint at height 3887915");
    if (cps.count(3887915)) {
        BOOST_CHECK_EQUAL(cps.at(3887915).GetHex(),
            "0000000b68a76df70e3a0451ec1deb9959c3df733968970ed7b8769ce365c11e");
    }
}

// -----------------------------------------------------------------------
// Testnet: internal sanity (network ID, message start, port, prefixes,
//   genesis, consensus flags — no mainnet values required)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet_sanity)
{
    BOOST_TEST_MESSAGE("Testnet sanity invariants");

    const auto chainParams = CreateChainParams(CBaseChainParams::TESTNET);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(chainParams->NetworkIDString(), "test");

    // Message start: "HTNT" = 0x48, 0x54, 0x4e, 0x54
    const auto& magic = chainParams->MessageStart();
    BOOST_CHECK_EQUAL(magic[0], 0x48);
    BOOST_CHECK_EQUAL(magic[1], 0x54);
    BOOST_CHECK_EQUAL(magic[2], 0x4e);
    BOOST_CHECK_EQUAL(magic[3], 0x54);

    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 42068);

    // Address prefixes
    const auto& pubkeyPrefix = chainParams->Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    BOOST_REQUIRE_EQUAL(pubkeyPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(pubkeyPrefix[0], 111);

    const auto& scriptPrefix = chainParams->Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    BOOST_REQUIRE_EQUAL(scriptPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(scriptPrefix[0], 196);

    const auto& secretPrefix = chainParams->Base58Prefix(CChainParams::SECRET_KEY);
    BOOST_REQUIRE_EQUAL(secretPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(secretPrefix[0], 239);

    // EXT_PUBLIC_KEY = 0x043587CF (tpub)
    const auto& extPub = chainParams->Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    BOOST_REQUIRE_EQUAL(extPub.size(), 4u);
    BOOST_CHECK_EQUAL(extPub[0], 0x04);
    BOOST_CHECK_EQUAL(extPub[1], 0x35);
    BOOST_CHECK_EQUAL(extPub[2], 0x87);
    BOOST_CHECK_EQUAL(extPub[3], 0xCF);

    // EXT_SECRET_KEY = 0x04358394 (tprv)
    const auto& extSec = chainParams->Base58Prefix(CChainParams::EXT_SECRET_KEY);
    BOOST_REQUIRE_EQUAL(extSec.size(), 4u);
    BOOST_CHECK_EQUAL(extSec[0], 0x04);
    BOOST_CHECK_EQUAL(extSec[1], 0x35);
    BOOST_CHECK_EQUAL(extSec[2], 0x83);
    BOOST_CHECK_EQUAL(extSec[3], 0x94);

    // Coin type
    BOOST_CHECK_EQUAL(chainParams->ExtCoinType(), 420);

    // Consensus flags — testnet matches mainnet: all enabled
    BOOST_CHECK(consensus.nBIP34Enabled);
    BOOST_CHECK(consensus.nBIP65Enabled);
    BOOST_CHECK(consensus.nBIP66Enabled);
    BOOST_CHECK(consensus.nSegwitEnabled);
    BOOST_CHECK(consensus.nCSVEnabled);

    // Testnet allows min-difficulty blocks
    BOOST_CHECK(consensus.fPowAllowMinDifficultyBlocks);
    BOOST_CHECK(!consensus.fPowNoRetargeting);

    // Genesis block exists and is non-null hash
    BOOST_CHECK(!consensus.hashGenesisBlock.IsNull());
    BOOST_CHECK(!chainParams->GenesisBlock().hashMerkleRoot.IsNull());

    // Checkpoints: at minimum includes genesis
    const CCheckpointData& cpData = chainParams->Checkpoints();
    BOOST_CHECK(!cpData.mapCheckpoints.empty());
}

// -----------------------------------------------------------------------
// Regtest: internal sanity (network ID, message start, port, prefixes,
//   genesis, mine-on-demand — no mainnet values required)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(regtest_sanity)
{
    BOOST_TEST_MESSAGE("Regtest sanity invariants");

    const auto chainParams = CreateChainParams(CBaseChainParams::REGTEST);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(chainParams->NetworkIDString(), "regtest");

    // Message start: "HREG" = 0x48, 0x52, 0x45, 0x47
    const auto& magic = chainParams->MessageStart();
    BOOST_CHECK_EQUAL(magic[0], 0x48);
    BOOST_CHECK_EQUAL(magic[1], 0x52);
    BOOST_CHECK_EQUAL(magic[2], 0x45);
    BOOST_CHECK_EQUAL(magic[3], 0x47);

    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 42067);

    // Address prefixes (same as testnet)
    const auto& pubkeyPrefix = chainParams->Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    BOOST_REQUIRE_EQUAL(pubkeyPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(pubkeyPrefix[0], 111);

    const auto& scriptPrefix = chainParams->Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    BOOST_REQUIRE_EQUAL(scriptPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(scriptPrefix[0], 196);

    const auto& secretPrefix = chainParams->Base58Prefix(CChainParams::SECRET_KEY);
    BOOST_REQUIRE_EQUAL(secretPrefix.size(), 1u);
    BOOST_CHECK_EQUAL(secretPrefix[0], 239);

    // EXT_PUBLIC_KEY = 0x043587CF (tpub)
    const auto& extPub = chainParams->Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    BOOST_REQUIRE_EQUAL(extPub.size(), 4u);
    BOOST_CHECK_EQUAL(extPub[0], 0x04);
    BOOST_CHECK_EQUAL(extPub[1], 0x35);
    BOOST_CHECK_EQUAL(extPub[2], 0x87);
    BOOST_CHECK_EQUAL(extPub[3], 0xCF);

    // EXT_SECRET_KEY = 0x04358394 (tprv)
    const auto& extSec = chainParams->Base58Prefix(CChainParams::EXT_SECRET_KEY);
    BOOST_REQUIRE_EQUAL(extSec.size(), 4u);
    BOOST_CHECK_EQUAL(extSec[0], 0x04);
    BOOST_CHECK_EQUAL(extSec[1], 0x35);
    BOOST_CHECK_EQUAL(extSec[2], 0x83);
    BOOST_CHECK_EQUAL(extSec[3], 0x94);

    // Coin type
    BOOST_CHECK_EQUAL(chainParams->ExtCoinType(), 420);

    // Consensus flags — regtest matches mainnet
    BOOST_CHECK(consensus.nBIP34Enabled);
    BOOST_CHECK(consensus.nBIP65Enabled);
    BOOST_CHECK(consensus.nBIP66Enabled);
    BOOST_CHECK(consensus.nSegwitEnabled);
    BOOST_CHECK(consensus.nCSVEnabled);

    BOOST_CHECK(consensus.fPowAllowMinDifficultyBlocks);
    BOOST_CHECK(consensus.fPowNoRetargeting);

    // Regtest-specific: mine blocks on demand
    BOOST_CHECK(chainParams->MineBlocksOnDemand());
    BOOST_CHECK(!chainParams->MiningRequiresPeers());

    // Genesis block exists
    BOOST_CHECK(!consensus.hashGenesisBlock.IsNull());
    BOOST_CHECK(!chainParams->GenesisBlock().hashMerkleRoot.IsNull());

    // Checkpoints: at minimum includes genesis
    const CCheckpointData& cpData = chainParams->Checkpoints();
    BOOST_CHECK(!cpData.mapCheckpoints.empty());
}

// -----------------------------------------------------------------------
// Mainnet burn amounts — must match chainparams.cpp constants
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_burn_amounts)
{
    BOOST_TEST_MESSAGE("Mainnet burn amounts");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    BOOST_CHECK_EQUAL(chainParams->IssueAssetBurnAmount(), COIN / 4);
    BOOST_CHECK_EQUAL(chainParams->ReissueAssetBurnAmount(), COIN / 20);
    BOOST_CHECK_EQUAL(chainParams->IssueSubAssetBurnAmount(), COIN / 20);
    BOOST_CHECK_EQUAL(chainParams->IssueUniqueAssetBurnAmount(), COIN / 100);
    BOOST_CHECK_EQUAL(chainParams->IssueMsgChannelAssetBurnAmount(), COIN / 20);
    BOOST_CHECK_EQUAL(chainParams->IssueQualifierAssetBurnAmount(), COIN / 4);
    BOOST_CHECK_EQUAL(chainParams->IssueSubQualifierAssetBurnAmount(), COIN / 20);
    BOOST_CHECK_EQUAL(chainParams->IssueRestrictedAssetBurnAmount(), COIN / 4);
    BOOST_CHECK_EQUAL(chainParams->AddNullQualifierTagBurnAmount(), COIN / 20);
}

// -----------------------------------------------------------------------
// Mainnet burn addresses — must match chainparams.cpp constants
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_burn_addresses)
{
    BOOST_TEST_MESSAGE("Mainnet burn addresses");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    BOOST_CHECK_EQUAL(chainParams->IssueAssetBurnAddress(), "RXissueAssetXXXXXXXXXXXXXXXXXhhZGt");
    BOOST_CHECK_EQUAL(chainParams->ReissueAssetBurnAddress(), "RXReissueAssetXXXXXXXXXXXXXXVEFAWu");
    BOOST_CHECK_EQUAL(chainParams->IssueSubAssetBurnAddress(), "RXissueSubAssetXXXXXXXXXXXXXWcwhwL");
    BOOST_CHECK_EQUAL(chainParams->IssueUniqueAssetBurnAddress(), "RXissueUniqueAssetXXXXXXXXXXWEAe58");
    BOOST_CHECK_EQUAL(chainParams->IssueMsgChannelAssetBurnAddress(), "RXissueMsgChanneLAssetXXXXXXSjHvAY");
    BOOST_CHECK_EQUAL(chainParams->IssueQualifierAssetBurnAddress(), "RXissueQuaLifierXXXXXXXXXXXXUgEDbC");
    BOOST_CHECK_EQUAL(chainParams->IssueSubQualifierAssetBurnAddress(), "RXissueSubQuaLifierXXXXXXXXXVTzvv5");
    BOOST_CHECK_EQUAL(chainParams->IssueRestrictedAssetBurnAddress(), "RXissueRestrictedXXXXXXXXXXXXzJZ1q");
    BOOST_CHECK_EQUAL(chainParams->AddNullQualifierTagBurnAddress(), "RXaddTagBurnXXXXXXXXXXXXXXXXZQm5ya");

    // Global burn address
    BOOST_CHECK_EQUAL(chainParams->GlobalBurnAddress(), "RXBurnXXXXXXXXXXXXXXXXXXXXXXWUo9FV");

    // Sanity: IsBurnAddress must return true for each
    BOOST_CHECK(chainParams->IsBurnAddress(chainParams->GlobalBurnAddress()));
    BOOST_CHECK(chainParams->IsBurnAddress("RXissueAssetXXXXXXXXXXXXXXXXXhhZGt"));
}

// -----------------------------------------------------------------------
// Mainnet activation heights (assets, messaging, restricted)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_activation_heights)
{
    BOOST_TEST_MESSAGE("Mainnet activation heights");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    BOOST_CHECK_EQUAL(chainParams->GetAssetActivationHeight(), 1);
    BOOST_CHECK_EQUAL(chainParams->MessagingActivationBlock(), 1u);
    BOOST_CHECK_EQUAL(chainParams->RestrictedActivationBlock(), 1u);
    BOOST_CHECK_EQUAL(chainParams->DGWActivationBlock(), 1u);
}

// -----------------------------------------------------------------------
// Mainnet KAWPOW activation time
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mainnet_kawpow_activation_time)
{
    BOOST_TEST_MESSAGE("Mainnet KAWPOW activation time — present and sane");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(nKAWPOWActivationTime, 1766126932u);
    BOOST_CHECK(nKAWPOWActivationTime > chainParams->GenesisBlock().nTime);
}

// -----------------------------------------------------------------------
// KAWPOW declared header height check activation
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(kawpow_header_height_check_activation)
{
    BOOST_TEST_MESSAGE("KAWPOW header height check activation");

    const auto mainnetParams = CreateChainParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(mainnetParams->GetConsensus().nHeightHeaderCheckActivation, 3894000);

    const auto testnetParams = CreateChainParams(CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(testnetParams->GetConsensus().nHeightHeaderCheckActivation, 0);

    const auto regtestParams = CreateChainParams(CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(regtestParams->GetConsensus().nHeightHeaderCheckActivation, 0);
}

BOOST_AUTO_TEST_SUITE_END()
