// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/validation.h"
#include "chainparams.h"
#include "key.h"
#include "validation.h"
#include "miner.h"
#include "pubkey.h"
#include "txmempool.h"
#include "random.h"
#include "script/standard.h"
#include "script/sign.h"
#include "test/test_hemp0x.h"
#include "utiltime.h"
#include "core_io.h"
#include "keystore.h"
#include "policy/policy.h"

#include <boost/test/unit_test.hpp>

#include "util.h"

bool CheckInputs(const CTransaction &tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheSigStore, bool cacheFullScriptStore, PrecomputedTransactionData &txdata, std::vector<CScriptCheck> *pvChecks);

BOOST_AUTO_TEST_SUITE(tx_validationcache_tests)

    static bool
    ToMemPool(CMutableTransaction &tx)
    {
        LOCK(cs_main);

        CValidationState state;
        return AcceptToMemoryPool(mempool, state, MakeTransactionRef(tx), nullptr /* pfMissingInputs */,
                                  nullptr /* plTxnReplaced */, true /* bypass_limits */, 0 /* nAbsurdFee */);
    }

    BOOST_FIXTURE_TEST_CASE(tx_mempool_block_doublespend_test, TestChain100Setup)
    {

        BOOST_TEST_MESSAGE("Running TX MemPool Block DoubleSpend Test");

        // Make sure skipping validation of transctions that were
        // validated going into the memory pool does not allow
        // double-spends in blocks to pass validation when they should not.

        CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

        // Create a double-spend of mature coinbase txn:
        std::vector<CMutableTransaction> spends;
        spends.resize(2);
        for (int i = 0; i < 2; i++)
        {
            spends[i].nVersion = 1;
            spends[i].vin.resize(1);
            spends[i].vin[0].prevout.hash = coinbaseTxns[0].GetHash();
            spends[i].vin[0].prevout.n = 0;
            spends[i].vout.resize(1);
            spends[i].vout[0].nValue = 11 * CENT;
            spends[i].vout[0].scriptPubKey = scriptPubKey;

            // Sign:
            std::vector<unsigned char> vchSig;
            uint256 hash = SignatureHash(scriptPubKey, spends[i], 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
            BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
            vchSig.push_back((unsigned char) SIGHASH_ALL);
            spends[i].vin[0].scriptSig << vchSig;
        }

        CBlock block;

        // Test 1: block with both of those transactions should be rejected.
        block = CreateAndProcessBlock(spends, scriptPubKey);
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());

        // Test 2: ... and should be rejected if spend1 is in the memory pool
        BOOST_CHECK(ToMemPool(spends[0]));
        block = CreateAndProcessBlock(spends, scriptPubKey);
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
        mempool.clear();

        // Test 3: ... and should be rejected if spend2 is in the memory pool
        BOOST_CHECK(ToMemPool(spends[1]));
        block = CreateAndProcessBlock(spends, scriptPubKey);
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
        mempool.clear();

        // Final sanity test: first spend in mempool, second in block, that's OK:
        std::vector<CMutableTransaction> oneSpend;
        oneSpend.push_back(spends[0]);
        BOOST_CHECK(ToMemPool(spends[1]));
        block = CreateAndProcessBlock(oneSpend, scriptPubKey);
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
        // spends[1] should have been removed from the mempool when the
        // block with spends[0] is accepted:
        BOOST_CHECK_EQUAL(mempool.size(), (uint64_t)0);
    }

    // Run CheckInputs (using pcoinsTip) on the given transaction, for all script
    // flags.  Test that CheckInputs passes for all flags that don't overlap with
    // the failing_flags argument, but otherwise fails.
    // CHECKLOCKTIMEVERIFY and CHECKSEQUENCEVERIFY (and future NOP codes that may
    // get reassigned) have an interaction with DISCOURAGE_UPGRADABLE_NOPS: if
    // the script flags used contain DISCOURAGE_UPGRADABLE_NOPS but don't contain
    // CHECKLOCKTIMEVERIFY (or CHECKSEQUENCEVERIFY), but the script does contain
    // OP_CHECKLOCKTIMEVERIFY (or OP_CHECKSEQUENCEVERIFY), then script execution
    // should fail.
    // Capture this interaction with the upgraded_nop argument: set it when evaluating
    // any script flag that is implemented as an upgraded NOP code.
    void ValidateCheckInputsForAllFlags(CMutableTransaction &tx, uint32_t failing_flags, bool add_to_cache, bool upgraded_nop)
    {
        PrecomputedTransactionData txdata(tx);
        // If we add many more flags, this loop can get too expensive, but we can
        // rewrite in the future to randomly pick a set of flags to evaluate.
        for (uint32_t test_flags = 0; test_flags < (1U << 16); test_flags += 1)
        {
            CValidationState state;
            // Filter out incompatible flag choices
            if ((test_flags & SCRIPT_VERIFY_CLEANSTACK))
            {
                // CLEANSTACK requires P2SH and WITNESS, see VerifyScript() in
                // script/interpreter.cpp
                test_flags |= SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
            }
            if ((test_flags & SCRIPT_VERIFY_WITNESS))
            {
                // WITNESS requires P2SH
                test_flags |= SCRIPT_VERIFY_P2SH;
            }
            bool ret = CheckInputs(tx, state, pcoinsTip, true, test_flags, true, add_to_cache, txdata, nullptr);
            // CheckInputs should succeed iff test_flags doesn't intersect with
            // failing_flags
            bool expected_return_value = !(test_flags & failing_flags);
            if (expected_return_value && upgraded_nop)
            {
                // If the script flag being tested corresponds to an upgraded NOP,
                // then script execution should fail if DISCOURAGE_UPGRADABLE_NOPS
                // is set.
                expected_return_value = !(test_flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS);
            }
            BOOST_CHECK_EQUAL(ret, expected_return_value);

            // Test the caching
            if (ret && add_to_cache)
            {
                // Check that we get a cache hit if the tx was valid
                std::vector<CScriptCheck> scriptchecks;
                BOOST_CHECK(CheckInputs(tx, state, pcoinsTip, true, test_flags, true, add_to_cache, txdata, &scriptchecks));
                BOOST_CHECK(scriptchecks.empty());
            } else
            {
                // Check that we get script executions to check, if the transaction
                // was invalid, or we didn't add to cache.
                std::vector<CScriptCheck> scriptchecks;
                BOOST_CHECK(CheckInputs(tx, state, pcoinsTip, true, test_flags, true, add_to_cache, txdata, &scriptchecks));
                BOOST_CHECK_EQUAL(scriptchecks.size(), tx.vin.size());
            }
        }
    }

    BOOST_FIXTURE_TEST_CASE(witness_stripped_mempool_reject_test, TestChain100Setup)
    {
        BOOST_TEST_MESSAGE("Running Witness Stripped Mempool Reject Test");

        CScript p2pk_scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        CScript p2pkh_scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
        CScript p2wpkh_scriptPubKey = GetScriptForWitness(p2pkh_scriptPubKey);

        CBasicKeyStore keystore;
        keystore.AddKey(coinbaseKey);

        CMutableTransaction fund_tx;
        fund_tx.nVersion = 1;
        fund_tx.vout.resize(1);
        fund_tx.vout[0].nValue = 11 * CENT;
        fund_tx.vout[0].scriptPubKey = p2wpkh_scriptPubKey;
        const COutPoint witness_prevout(fund_tx.GetHash(), 0);
        {
            LOCK(cs_main);
            pcoinsTip->AddCoin(witness_prevout, Coin(fund_tx.vout[0], 1, false), false);
        }

        CMutableTransaction stripped_tx;
        stripped_tx.nVersion = 1;
        stripped_tx.vin.resize(1);
        stripped_tx.vin[0].prevout = witness_prevout;
        stripped_tx.vout.resize(1);
        stripped_tx.vout[0].nValue = 10 * CENT;
        stripped_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;
        {
            SignatureData sigdata;
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &stripped_tx, 0, 11 * CENT, SIGHASH_ALL), p2wpkh_scriptPubKey, sigdata);
            UpdateTransaction(stripped_tx, 0, sigdata);
            stripped_tx.vin[0].scriptWitness.SetNull();
        }

        CValidationState state;
        {
            LOCK(cs_main);
            BOOST_CHECK(IsWitnessStrippedTx(stripped_tx, *pcoinsTip));
            BOOST_CHECK(!AcceptToMemoryPool(mempool, state, MakeTransactionRef(stripped_tx),
                                            nullptr, nullptr, true, 0));
        }

        int nDoS = 0;
        BOOST_CHECK(state.IsInvalid(nDoS));
        BOOST_CHECK_EQUAL(nDoS, 0);
        BOOST_CHECK(state.CorruptionPossible());
        BOOST_CHECK_EQUAL(state.GetRejectCode(), REJECT_NONSTANDARD);
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-witness-stripped");
    }

    BOOST_FIXTURE_TEST_CASE(checkinputs_test, TestChain100Setup)
    {

        BOOST_TEST_MESSAGE("Running CheckInputs Test");

        TurnOffSegwit();
        TurnOffCSV();
        TurnOffBIP34();
        TurnOffBIP65();
        TurnOffBIP66();

        // Test that passing CheckInputs with one set of script flags doesn't imply
        // that we would pass again with a different set of flags.
        InitScriptExecutionCache();

        CScript p2pk_scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        CScript p2sh_scriptPubKey = GetScriptForDestination(CScriptID(p2pk_scriptPubKey));
        CScript p2pkh_scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
        CScript p2wpkh_scriptPubKey = GetScriptForWitness(p2pkh_scriptPubKey);

        CBasicKeyStore keystore;
        keystore.AddKey(coinbaseKey);
        keystore.AddCScript(p2pk_scriptPubKey);

        // flags to test: SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, SCRIPT_VERIFY_CHECKSEQUENCE_VERIFY, SCRIPT_VERIFY_NULLDUMMY, uncompressed pubkey thing

        // Create 2 outputs that match the three scripts above, spending the first
        // coinbase tx.
        CMutableTransaction spend_tx;

        spend_tx.nVersion = 1;
        spend_tx.vin.resize(1);
        spend_tx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
        spend_tx.vin[0].prevout.n = 0;
        spend_tx.vout.resize(4);
        spend_tx.vout[0].nValue = 11 * CENT;
        spend_tx.vout[0].scriptPubKey = p2sh_scriptPubKey;
        spend_tx.vout[1].nValue = 11 * CENT;
        spend_tx.vout[1].scriptPubKey = p2wpkh_scriptPubKey;
        spend_tx.vout[2].nValue = 11 * CENT;
        spend_tx.vout[2].scriptPubKey =
                CScript() << OP_CHECKLOCKTIMEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        spend_tx.vout[3].nValue = 11 * CENT;
        spend_tx.vout[3].scriptPubKey =
                CScript() << OP_CHECKSEQUENCEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

        // Sign, with a non-DER signature
        {
            std::vector<unsigned char> vchSig;
            uint256 hash = SignatureHash(p2pk_scriptPubKey, spend_tx, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
            BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
            vchSig.push_back((unsigned char) 0); // padding byte makes this non-DER
            vchSig.push_back((unsigned char) SIGHASH_ALL);
            spend_tx.vin[0].scriptSig << vchSig;
        }

        LOCK(cs_main);

        // Test that invalidity under a set of flags doesn't preclude validity
        // under other (eg consensus) flags.
        // spend_tx is invalid according to DERSIG
        {
            CValidationState state;
            PrecomputedTransactionData ptd_spend_tx(spend_tx);

            BOOST_CHECK(!CheckInputs(spend_tx, state, pcoinsTip, true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true, true, ptd_spend_tx, nullptr));

            // If we call again asking for scriptchecks (as happens in
            // ConnectBlock), we should add a script check object for this -- we're
            // not caching invalidity (if that changes, delete this test case).
            std::vector<CScriptCheck> scriptchecks;
            BOOST_CHECK(CheckInputs(spend_tx, state, pcoinsTip, true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true, true, ptd_spend_tx, &scriptchecks));
            BOOST_CHECK_EQUAL(scriptchecks.size(), (uint64_t)1);

            // Test that CheckInputs returns true iff DERSIG-enforcing flags are
            // not present.  Don't add these checks to the cache, so that we can
            // test later that block validation works fine in the absence of cached
            // successes.
            ValidateCheckInputsForAllFlags(spend_tx, SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_STRICTENC, false, false);

            // And if we produce a block with this tx, it should be valid (DERSIG not
            // enabled yet), even though there's no cache entry.
            CBlock block;

            block = CreateAndProcessBlock({spend_tx}, p2pk_scriptPubKey);
            BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
            BOOST_CHECK(pcoinsTip->GetBestBlock() == block.GetHash());
        }

        // Test P2SH: construct a transaction that is valid without P2SH, and
        // then test validity with P2SH.
        {
            CMutableTransaction invalid_under_p2sh_tx;
            invalid_under_p2sh_tx.nVersion = 1;
            invalid_under_p2sh_tx.vin.resize(1);
            invalid_under_p2sh_tx.vin[0].prevout.hash = spend_tx.GetHash();
            invalid_under_p2sh_tx.vin[0].prevout.n = 0;
            invalid_under_p2sh_tx.vout.resize(1);
            invalid_under_p2sh_tx.vout[0].nValue = 11 * CENT;
            invalid_under_p2sh_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;
            std::vector<unsigned char> vchSig2(p2pk_scriptPubKey.begin(), p2pk_scriptPubKey.end());
            invalid_under_p2sh_tx.vin[0].scriptSig << vchSig2;

            ValidateCheckInputsForAllFlags(invalid_under_p2sh_tx, SCRIPT_VERIFY_P2SH, true, false);
        }

        // Test CHECKLOCKTIMEVERIFY
        {
            CMutableTransaction invalid_with_cltv_tx;
            invalid_with_cltv_tx.nVersion = 1;
            invalid_with_cltv_tx.nLockTime = 100;
            invalid_with_cltv_tx.vin.resize(1);
            invalid_with_cltv_tx.vin[0].prevout.hash = spend_tx.GetHash();
            invalid_with_cltv_tx.vin[0].prevout.n = 2;
            invalid_with_cltv_tx.vin[0].nSequence = 0;
            invalid_with_cltv_tx.vout.resize(1);
            invalid_with_cltv_tx.vout[0].nValue = 11 * CENT;
            invalid_with_cltv_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

            // Sign
            std::vector<unsigned char> vchSig;
            uint256 hash = SignatureHash(spend_tx.vout[2].scriptPubKey, invalid_with_cltv_tx, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
            BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
            vchSig.push_back((unsigned char) SIGHASH_ALL);
            invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

            ValidateCheckInputsForAllFlags(invalid_with_cltv_tx, SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true, true);

            // Make it valid, and check again
            invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
            CValidationState state;
            PrecomputedTransactionData txdata(invalid_with_cltv_tx);
            BOOST_CHECK(CheckInputs(invalid_with_cltv_tx, state, pcoinsTip, true, SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true, true, txdata, nullptr));
        }

        // TEST CHECKSEQUENCEVERIFY
        {
            CMutableTransaction invalid_with_csv_tx;
            invalid_with_csv_tx.nVersion = 2;
            invalid_with_csv_tx.vin.resize(1);
            invalid_with_csv_tx.vin[0].prevout.hash = spend_tx.GetHash();
            invalid_with_csv_tx.vin[0].prevout.n = 3;
            invalid_with_csv_tx.vin[0].nSequence = 100;
            invalid_with_csv_tx.vout.resize(1);
            invalid_with_csv_tx.vout[0].nValue = 11 * CENT;
            invalid_with_csv_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

            // Sign
            std::vector<unsigned char> vchSig;
            uint256 hash = SignatureHash(spend_tx.vout[3].scriptPubKey, invalid_with_csv_tx, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
            BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
            vchSig.push_back((unsigned char) SIGHASH_ALL);
            invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

            ValidateCheckInputsForAllFlags(invalid_with_csv_tx, SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true, true);

            // Make it valid, and check again
            invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
            CValidationState state;
            PrecomputedTransactionData txdata(invalid_with_csv_tx);
            BOOST_CHECK(CheckInputs(invalid_with_csv_tx, state, pcoinsTip, true, SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true, true, txdata, nullptr));
        }

        // TODO: add tests for remaining script flags

        // Test that passing CheckInputs with a valid witness doesn't imply success
        // for the same tx with a different witness.
        {
            CMutableTransaction valid_with_witness_tx;
            valid_with_witness_tx.nVersion = 1;
            valid_with_witness_tx.vin.resize(1);
            valid_with_witness_tx.vin[0].prevout.hash = spend_tx.GetHash();
            valid_with_witness_tx.vin[0].prevout.n = 1;
            valid_with_witness_tx.vout.resize(1);
            valid_with_witness_tx.vout[0].nValue = 11 * CENT;
            valid_with_witness_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

            // Sign
            SignatureData sigdata;
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &valid_with_witness_tx, 0, 11 * CENT, SIGHASH_ALL), spend_tx.vout[1].scriptPubKey, sigdata);
            UpdateTransaction(valid_with_witness_tx, 0, sigdata);

            // This should be valid under all script flags.
            ValidateCheckInputsForAllFlags(valid_with_witness_tx, 0, true, false);

            // Remove the witness, and check that it is now invalid.
            valid_with_witness_tx.vin[0].scriptWitness.SetNull();
            ValidateCheckInputsForAllFlags(valid_with_witness_tx, SCRIPT_VERIFY_WITNESS, true, false);
        }

        {
            // Test a transaction with multiple inputs.
            CMutableTransaction tx;

            tx.nVersion = 1;
            tx.vin.resize(2);
            tx.vin[0].prevout.hash = spend_tx.GetHash();
            tx.vin[0].prevout.n = 0;
            tx.vin[1].prevout.hash = spend_tx.GetHash();
            tx.vin[1].prevout.n = 1;
            tx.vout.resize(1);
            tx.vout[0].nValue = 22 * CENT;
            tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

            // Sign
            for (int i = 0; i < 2; ++i)
            {
                SignatureData sigdata;
                ProduceSignature(MutableTransactionSignatureCreator(&keystore, &tx, i, 11 * CENT, SIGHASH_ALL), spend_tx.vout[i].scriptPubKey, sigdata);
                UpdateTransaction(tx, i, sigdata);
            }

            // This should be valid under all script flags
            ValidateCheckInputsForAllFlags(tx, 0, true, false);

            // Check that if the second input is invalid, but the first input is
            // valid, the transaction is not cached.
            // Invalidate vin[1]
            tx.vin[1].scriptWitness.SetNull();

            CValidationState state;
            PrecomputedTransactionData txdata(tx);
            // This transaction is now invalid under segwit, because of the second input.
            BOOST_CHECK(!CheckInputs(tx, state, pcoinsTip, true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, nullptr));

            std::vector<CScriptCheck> scriptchecks;
            // Make sure this transaction was not cached (ie because the first
            // input was valid)
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip, true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, &scriptchecks));
            // Should get 2 script checks back -- caching is on a whole-transaction basis.
            BOOST_CHECK_EQUAL(scriptchecks.size(), (uint64_t)2);
        }
        // TEST: native P2WPKH without witness is detected by policy helper
        {
            CMutableTransaction no_witness_tx;
            no_witness_tx.nVersion = 1;
            no_witness_tx.vin.resize(1);
            no_witness_tx.vin[0].prevout.hash = spend_tx.GetHash();
            no_witness_tx.vin[0].prevout.n = 1;  // p2wpkh_scriptPubKey output
            no_witness_tx.vout.resize(1);
            no_witness_tx.vout[0].nValue = 11 * CENT;
            no_witness_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

            SignatureData sigdata;
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &no_witness_tx, 0, 11 * CENT, SIGHASH_ALL), spend_tx.vout[1].scriptPubKey, sigdata);
            UpdateTransaction(no_witness_tx, 0, sigdata);
            // Now strip the witness to simulate witness-stripped attack
            no_witness_tx.vin[0].scriptWitness.SetNull();

            // Verify the helper detects it
            BOOST_CHECK(IsWitnessStrippedTx(no_witness_tx, *pcoinsTip));
        }

        // TEST: P2SH-P2WPKH without witness is also detected
        {
            CScript redeemScript = GetScriptForWitness(p2pkh_scriptPubKey);

            CMutableTransaction create_tx;
            create_tx.nVersion = 1;
            create_tx.vin.resize(1);
            create_tx.vin[0].prevout.hash = coinbaseTxns[1].GetHash();
            create_tx.vin[0].prevout.n = 0;
            create_tx.vout.resize(1);
            create_tx.vout[0].nValue = 5 * COIN;
            create_tx.vout[0].scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));
            {
                std::vector<unsigned char> vchSig;
                uint256 hash = SignatureHash(coinbaseTxns[1].vout[0].scriptPubKey, create_tx, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
                BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
                vchSig.push_back((unsigned char) SIGHASH_ALL);
                create_tx.vin[0].scriptSig << vchSig;
            }
            {
                CBlock block = CreateAndProcessBlock({create_tx}, p2pk_scriptPubKey);
                BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
            }

            CMutableTransaction spend_sh_wpkh;
            spend_sh_wpkh.nVersion = 1;
            spend_sh_wpkh.vin.resize(1);
            spend_sh_wpkh.vin[0].prevout.hash = create_tx.GetHash();
            spend_sh_wpkh.vin[0].prevout.n = 0;
            spend_sh_wpkh.vout.resize(1);
            spend_sh_wpkh.vout[0].nValue = 4 * COIN;
            spend_sh_wpkh.vout[0].scriptPubKey = p2pk_scriptPubKey;
            {
                std::vector<unsigned char> vchSig;
                uint256 hash = SignatureHash(redeemScript, spend_sh_wpkh, 0, SIGHASH_ALL, 0, SIGVERSION_WITNESS_V0);
                BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
                vchSig.push_back((unsigned char) SIGHASH_ALL);
                spend_sh_wpkh.vin[0].scriptSig = CScript() << ToByteVector(redeemScript);
            }
            // No witness data set

            BOOST_CHECK(IsWitnessStrippedTx(spend_sh_wpkh, *pcoinsTip));
        }

        // TEST: P2SH-P2WPKH with a mismatched redeem script is NOT flagged
        {
            CScript actualRedeemScript = GetScriptForWitness(p2pkh_scriptPubKey);
            CKey otherKey;
            otherKey.MakeNewKey(true);
            CScript otherRedeemScript = GetScriptForWitness(GetScriptForDestination(otherKey.GetPubKey().GetID()));

            CMutableTransaction create_tx;
            create_tx.nVersion = 1;
            create_tx.vin.resize(1);
            create_tx.vin[0].prevout.hash = coinbaseTxns[2].GetHash();
            create_tx.vin[0].prevout.n = 0;
            create_tx.vout.resize(1);
            create_tx.vout[0].nValue = 5 * COIN;
            create_tx.vout[0].scriptPubKey = GetScriptForDestination(CScriptID(actualRedeemScript));
            {
                std::vector<unsigned char> vchSig;
                uint256 hash = SignatureHash(coinbaseTxns[2].vout[0].scriptPubKey, create_tx, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
                BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
                vchSig.push_back((unsigned char) SIGHASH_ALL);
                create_tx.vin[0].scriptSig << vchSig;
            }
            {
                CBlock block = CreateAndProcessBlock({create_tx}, p2pk_scriptPubKey);
                BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
            }

            CMutableTransaction spend_mismatch;
            spend_mismatch.nVersion = 1;
            spend_mismatch.vin.resize(1);
            spend_mismatch.vin[0].prevout.hash = create_tx.GetHash();
            spend_mismatch.vin[0].prevout.n = 0;
            spend_mismatch.vout.resize(1);
            spend_mismatch.vout[0].nValue = 4 * COIN;
            spend_mismatch.vout[0].scriptPubKey = p2pk_scriptPubKey;
            spend_mismatch.vin[0].scriptSig = CScript() << ToByteVector(otherRedeemScript);

            BOOST_CHECK(!IsWitnessStrippedTx(spend_mismatch, *pcoinsTip));
        }

        // TEST: ordinary P2PKH without witness is NOT flagged
        {
            CMutableTransaction p2pkh_no_wit;
            p2pkh_no_wit.nVersion = 1;
            p2pkh_no_wit.vin.resize(1);
            p2pkh_no_wit.vin[0].prevout.hash = spend_tx.GetHash();
            p2pkh_no_wit.vin[0].prevout.n = 0;  // p2sh output
            p2pkh_no_wit.vout.resize(1);
            p2pkh_no_wit.vout[0].nValue = 10 * CENT;
            p2pkh_no_wit.vout[0].scriptPubKey = p2pk_scriptPubKey;
            // Malformed scriptSig (not a valid P2SH redeem) - should still be false
            p2pkh_no_wit.vin[0].scriptSig = CScript() << OP_TRUE;

            BOOST_CHECK(!IsWitnessStrippedTx(p2pkh_no_wit, *pcoinsTip));
        }

        // TEST: transaction with witness is never flagged
        {
            CMutableTransaction with_witness_tx;
            with_witness_tx.nVersion = 1;
            with_witness_tx.vin.resize(1);
            with_witness_tx.vin[0].prevout.hash = spend_tx.GetHash();
            with_witness_tx.vin[0].prevout.n = 1;
            with_witness_tx.vout.resize(1);
            with_witness_tx.vout[0].nValue = 10 * CENT;
            with_witness_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;
            SignatureData sigdata;
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &with_witness_tx, 0, 11 * CENT, SIGHASH_ALL), spend_tx.vout[1].scriptPubKey, sigdata);
            UpdateTransaction(with_witness_tx, 0, sigdata);

            BOOST_CHECK(!IsWitnessStrippedTx(with_witness_tx, *pcoinsTip));
        }

        // TEST: non-push scriptSig in P2SH-wrapped witness path does not false-positive
        {
            CScript redeemScript = GetScriptForWitness(p2pkh_scriptPubKey);

            CMutableTransaction create_tx2;
            create_tx2.nVersion = 1;
            create_tx2.vin.resize(1);
            create_tx2.vin[0].prevout.hash = coinbaseTxns[3].GetHash();
            create_tx2.vin[0].prevout.n = 0;
            create_tx2.vout.resize(1);
            create_tx2.vout[0].nValue = 5 * COIN;
            create_tx2.vout[0].scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));
            {
                std::vector<unsigned char> vchSig;
                uint256 hash = SignatureHash(coinbaseTxns[3].vout[0].scriptPubKey, create_tx2, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
                BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
                vchSig.push_back((unsigned char) SIGHASH_ALL);
                create_tx2.vin[0].scriptSig << vchSig;
            }
            {
                CBlock block = CreateAndProcessBlock({create_tx2}, p2pk_scriptPubKey);
                BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
            }

            CMutableTransaction spend_malformed;
            spend_malformed.nVersion = 1;
            spend_malformed.vin.resize(1);
            spend_malformed.vin[0].prevout.hash = create_tx2.GetHash();
            spend_malformed.vin[0].prevout.n = 0;
            spend_malformed.vout.resize(1);
            spend_malformed.vout[0].nValue = 4 * COIN;
            spend_malformed.vout[0].scriptPubKey = p2pk_scriptPubKey;
            // Non-push-only scriptSig (contains OP_ADD) should not trigger detection
            spend_malformed.vin[0].scriptSig = CScript() << ToByteVector(redeemScript) << OP_ADD;

            BOOST_CHECK(!IsWitnessStrippedTx(spend_malformed, *pcoinsTip));
        }
    }

BOOST_AUTO_TEST_SUITE_END()
