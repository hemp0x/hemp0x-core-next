// Copyright (c) 2026 The Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Unit tests for the KAWPOW declared-header-height consensus check.
//
// The nHeight field carried inside a KAWPOW header feeds the PoW hash, the DAG
// epoch and the ProgPoW period. AcceptBlockHeader must reject any KAWPOW
// header whose declared nHeight differs from the actual chain height once
// consensus.nHeightHeaderCheckActivation is reached. These tests drive the
// check through ProcessNewBlockHeaders, the same path network peers use.

#include "chain.h"
#include "chainparams.h"
#include "chainparamsbase.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "miner.h"
#include "primitives/block.h"
#include "pow.h"
#include "test/test_hemp0x.h"
#include "timedata.h"
#include "validation.h"
#include "versionbits.h"

#include <boost/test/unit_test.hpp>

namespace {

// Returns the valid PoW data (final hash + mix hash) for a KAWPOW header at
// the given declared height, mined against the regtest target.
void SolveKawpow(CBlockHeader& header, const Consensus::Params& consensusParams)
{
    uint256 mix_hash;
    while (!CheckProofOfWork(header.GetHashFull(mix_hash), header.nBits, consensusParams)) {
        ++header.nNonce64;
    }
    header.mix_hash = mix_hash;
}

// Test seam: a regtest params clone whose KAWPOW header height check
// activation height can be moved, so pre-activation behavior is testable.
class TestRegtestParams : public CChainParams
{
public:
    explicit TestRegtestParams(const CChainParams& other) : CChainParams(other) {}
    void SetHeightHeaderCheckActivation(int nActivation) { consensus.nHeightHeaderCheckActivation = nActivation; }
};

// Builds a KAWPOW header for the block after pindexPrev, declaring the given
// height, with valid regtest PoW.
CBlockHeader MakeKawpowHeader(const CBlockIndex* pindexPrev, int nDeclaredHeight)
{
    const CChainParams& chainparams = GetParams();
    const Consensus::Params& consensusParams = chainparams.GetConsensus();

    CBlockHeader header;
    header.nVersion = VERSIONBITS_TOP_BITS_ASSETS;
    header.hashPrevBlock = pindexPrev->GetBlockHash();
    header.hashMerkleRoot = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    header.nTime = std::max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());
    header.nBits = GetNextWorkRequired(pindexPrev, &header, consensusParams);
    header.nHeight = nDeclaredHeight;
    SolveKawpow(header, consensusParams);
    return header;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(kawpow_header_height_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(matching_declared_height_accepted)
{
    BOOST_TEST_MESSAGE("KAWPOW header with matching declared height passes");

    const CChainParams& chainparams = GetParams();
    BOOST_CHECK_EQUAL(chainparams.GetConsensus().nHeightHeaderCheckActivation, 0);
    const int nHeight = chainActive.Tip()->nHeight + 1;

    CBlockHeader header = MakeKawpowHeader(chainActive.Tip(), nHeight);

    CValidationState state;
    const CBlockIndex* pindex = nullptr;
    BOOST_CHECK(ProcessNewBlockHeaders({header}, state, chainparams, &pindex));
    BOOST_CHECK(state.IsValid());
    BOOST_CHECK(pindex != nullptr);
    BOOST_CHECK_EQUAL(pindex->nHeight, nHeight);
}

BOOST_AUTO_TEST_CASE(mismatched_declared_height_rejected)
{
    BOOST_TEST_MESSAGE("KAWPOW header with mismatched declared height fails after activation");

    const CChainParams& chainparams = GetParams();
    const int nHeight = chainActive.Tip()->nHeight + 1;

    // Declare one block higher than the actual chain height. PoW is valid for
    // the declared height, so only the height mismatch is wrong.
    CBlockHeader header = MakeKawpowHeader(chainActive.Tip(), nHeight + 1);

    CValidationState state;
    const CBlockIndex* pindex = nullptr;
    BOOST_CHECK(!ProcessNewBlockHeaders({header}, state, chainparams, &pindex));
    BOOST_CHECK(state.IsInvalid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-height");
}

BOOST_AUTO_TEST_CASE(lower_declared_height_rejected)
{
    BOOST_TEST_MESSAGE("KAWPOW header with lower declared height fails after activation");

    const CChainParams& chainparams = GetParams();
    const int nHeight = chainActive.Tip()->nHeight + 1;

    // Declare a lower height than the actual chain position. This mirrors the
    // checkpoint-shortcut risk: a high-position block must not opt into older
    // KAWPOW height semantics.
    CBlockHeader header = MakeKawpowHeader(chainActive.Tip(), nHeight - 1);

    CValidationState state;
    const CBlockIndex* pindex = nullptr;
    BOOST_CHECK(!ProcessNewBlockHeaders({header}, state, chainparams, &pindex));
    BOOST_CHECK(state.IsInvalid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-height");
}

BOOST_AUTO_TEST_CASE(mismatch_before_activation_accepted)
{
    BOOST_TEST_MESSAGE("Mismatched declared height accepted before activation height is reached");

    const int nHeight = chainActive.Tip()->nHeight + 1;

    // Regtest activates at height 0; clone the params and push activation past
    // the current tip so the mismatch is not yet enforced.
    TestRegtestParams params(*CreateChainParams(CBaseChainParams::REGTEST).get());
    params.SetHeightHeaderCheckActivation(nHeight + 1);

    CBlockHeader header = MakeKawpowHeader(chainActive.Tip(), nHeight + 1);

    CValidationState state;
    const CBlockIndex* pindex = nullptr;
    BOOST_CHECK(ProcessNewBlockHeaders({header}, state, params, &pindex));
    BOOST_CHECK(state.IsValid());
}

BOOST_AUTO_TEST_CASE(mainnet_activation_height_set)
{
    BOOST_TEST_MESSAGE("Mainnet KAWPOW declared-height activation is set");

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GetConsensus().nHeightHeaderCheckActivation, 3894000);
}

BOOST_AUTO_TEST_SUITE_END()
