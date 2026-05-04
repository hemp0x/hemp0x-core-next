// Copyright (c) 2013-2015 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "key.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_hemp0x.h"

#include <string>
#include <vector>

struct TestDerivation
{
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector
{
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn)
    {}

    TestVector &operator()(std::string pub, std::string prv, unsigned int nChild)
    {
        vDerive.push_back(TestDerivation());
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

TestVector test1 =
        TestVector("000102030405060708090a0b0c0d0e0f")
                ("xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8",
                 "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi",
                 0x80000000)
                ("xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw",
                 "xprv9uHRZZhk6KAJC1avXpDAp4MDc3sQKNxDiPvvkX8Br5ngLNv1TxvUxt4cV1rGL5hj6KCesnDYUhd7oWgT11eZG7XnxHrnYeSvkzY7d2bhkJ7",
                 1)
                ("xpub6ASuArnXKPbfEwhqN6e3mwBcDTgzisQN1wXN9BJcM47sSikHjJf3UFHKkNAWbWMiGj7Wf5uMash7SyYq527Hqck2AxYysAA7xmALppuCkwQ",
                 "xprv9wTYmMFdV23N2TdNG573QoEsfRrWKQgWeibmLntzniatZvR9BmLnvSxqu53Kw1UmYPxLgboyZQaXwTCg8MSY3H2EU4pWcQDnRnrVA1xe8fs",
                 0x80000002)
                ("xpub6D4BDPcP2GT577Vvch3R8wDkScZWzQzMMUm3PWbmWvVJrZwQY4VUNgqFJPMM3No2dFDFGTsxxpG5uJh7n7epu4trkrX7x7DogT5Uv6fcLW5",
                 "xprv9z4pot5VBttmtdRTWfWQmoH1taj2axGVzFqSb8C9xaxKymcFzXBDptWmT7FwuEzG3ryjH4ktypQSAewRiNMjANTtpgP4mLTj34bhnZX7UiM",
                 2)
                ("xpub6FHa3pjLCk84BayeJxFW2SP4XRrFd1JYnxeLeU8EqN3vDfZmbqBqaGJAyiLjTAwm6ZLRQUMv1ZACTj37sR62cfN7fe5JnJ7dh8zL4fiyLHV",
                 "xprvA2JDeKCSNNZky6uBCviVfJSKyQ1mDYahRjijr5idH2WwLsEd4Hsb2Tyh8RfQMuPh7f7RtyzTtdrbdqqsunu5Mm3wDvUAKRHSC34sJ7in334",
                 1000000000)
                ("xpub6H1LXWLaKsWFhvm6RVpEL9P4KfRZSW7abD2ttkWP3SSQvnyA8FSVqNTEcYFgJS2UaFcxupHiYkro49S8yGasTvXEYBVPamhGW6cFJodrTHy",
                 "xprvA41z7zogVVwxVSgdKUHDy1SKmdb533PjDz7J6N6mV6uS3ze1ai8FHa8kmHScGpWmj4WggLyQjgPie1rFSruoUihUZREPSL39UNdE3BBDu76",
                 0);

TestVector test2 =
        TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
                ("xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB",
                 "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U",
                 0)
                ("xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3ZuxV27CprR9LgpeyGmXUbC6wb7ERfvrnKZjXoUmmDznezpbZb7ap6r1D3tgFxHmwMkQTPH",
                 "xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZXo1ex8G81dwSM1fwqWpWkeS3v86pgKt",
                 0xFFFFFFFF)
                ("xpub6ASAVgeehLbnwdqV6UKMHVzgqAG8Gr6riv3Fxxpj8ksbH9ebxaEyBLZ85ySDhKiLDBrQSARLq1uNRts8RuJiHjaDMBU4Zn9h8LZNnBC5y4a",
                 "xprv9wSp6B7kry3Vj9m1zSnLvN3xH8RdsPP1Mh7fAaR7aRLcQMKTR2vidYEeEg2mUCTAwCd6vnxVrcjfy2kRgVsFawNzmjuHc2YmYRmagcEPdU9",
                 1)
                ("xpub6DF8uhdarytz3FWdA8TvFSvvAh8dP3283MY7p2V4SeE2wyWmG5mg5EwVvmdMVCQcoNJxGoWaU9DCWh89LojfZ537wTfunKau47EL2dhHKon",
                 "xprv9zFnWC6h2cLgpmSA46vutJzBcfJ8yaJGg8cX1e5StJh45BBciYTRXSd25UEPVuesF9yog62tGAQtHjXajPPdbRCHuWS6T8XA2ECKADdw4Ef",
                 0xFFFFFFFE)
                ("xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL",
                 "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc",
                 2)
                ("xpub6FnCn6nSzZAw5Tw7cgR9bi15UV96gLZhjDstkXXxvCLsUXBGXPdSnLFbdpq8p9HmGsApME5hQTZ3emM2rnY5agb9rXpVGyy3bdW6EEgAtqt",
                 "xprvA2nrNbFZABcdryreWet9Ea4LvTJcGsqrMzxHx98MMrotbir7yrKCEXw7nadnHM8Dq38EGfSh6dqA9QWTyefMLEcBYJUuekgW4BYPJcr9E7j",
                 0);

TestVector test3 =
        TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
                ("xpub661MyMwAqRbcEZVB4dScxMAdx6d4nFc9nvyvH3v4gJL378CSRZiYmhRoP7mBy6gSPSCYk6SzXPTf3ND1cZAceL7SfJ1Z3GC8vBgp2epUt13",
                 "xprv9s21ZrQH143K25QhxbucbDDuQ4naNntJRi4KUfWT7xo4EKsHt2QJDu7KXp1A3u7Bi1j8ph3EGsZ9Xvz9dGuVrtHHs7pXeTzjuxBrCmmhgC6",
                 0x80000000)
                ("xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHBaohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y",
                 "xprv9uPDJpEQgRQfDcW7BkF7eTya6RPxXeJCqCJGHuCJ4GiRVLzkTXBAJMu2qaMWPrS7AANYqdq6vcBcBUdJCVVFceUvJFjaPdGZ2y9WACViL4L",
                 0);

void RunTest(const TestVector &test)
{
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetSeed(seed.data(), seed.size());
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive)
    {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        CHemp0xExtKey b58key;
        b58key.SetKey(key);
        BOOST_CHECK(b58key.ToString() == derive.prv);

        CHemp0xExtKey b58keyDecodeCheck(derive.prv);
        CExtKey checkKey = b58keyDecodeCheck.GetKey();
        assert(checkKey == key); //ensure a base58 decoded key also matches

        // Test public key
        CHemp0xExtPubKey b58pubkey;
        b58pubkey.SetKey(pubkey);
        BOOST_CHECK(b58pubkey.ToString() == derive.pub);

        CHemp0xExtPubKey b58PubkeyDecodeCheck(derive.pub);
        CExtPubKey checkPubKey = b58PubkeyDecodeCheck.GetKey();
        assert(checkPubKey == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000))
        {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;

        CDataStream ssPub(SER_DISK, CLIENT_VERSION);
        ssPub << pubkeyNew;
        BOOST_CHECK(ssPub.size() == 75);

        CDataStream ssPriv(SER_DISK, CLIENT_VERSION);
        ssPriv << keyNew;
        BOOST_CHECK(ssPriv.size() == 75);

        CExtPubKey pubCheck;
        CExtKey privCheck;
        ssPub >> pubCheck;
        ssPriv >> privCheck;

        BOOST_CHECK(pubCheck == pubkeyNew);
        BOOST_CHECK(privCheck == keyNew);
    }
}

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

    BOOST_AUTO_TEST_CASE(bip32_test_1)
    {
        BOOST_TEST_MESSAGE("Running BIP32 Test 1");
        RunTest(test1);
    }

    BOOST_AUTO_TEST_CASE(bip32_test_2)
    {
        BOOST_TEST_MESSAGE("Running BIP32 Test 2");
        RunTest(test2);
    }

    BOOST_AUTO_TEST_CASE(bip32_test_3)
    {
        BOOST_TEST_MESSAGE("Running BIP32 Test 3");
        RunTest(test3);
    }

    BOOST_AUTO_TEST_CASE(secp256k1_bip32_derivation_guardrail)
    {
        /*
         * BIP32 derivation guardrails that pin the current behavior of
         * CExtKey::Derive, CExtPubKey::Derive, CKey::Derive, and
         * CPubKey::Derive against future libsecp256k1 subtree upgrades.
         *
         * Uses fixed seed material and a known derivation path:
         *   m/44'/420'/0'/0/10
         *
         * Verifies:
         *  (a) All hardened + non-hardened private derivations succeed.
         *  (b) Non-hardened public derivation succeeds.
         *  (c) Private-derived public key matches public-derived child
         *      key for non-hardened derivation (key derivation
         *      consistency).
         *  (d) Serialization round-trip integrity for both CExtKey
         *      and CExtPubKey.
         *  (e) Hemp0x canonical BIP44 coin type 420 assumption
         *      (no coin175 support).
         *
         * Note: CPubKey::Derive asserts (nChild >> 31) == 0, so hardened
         * public derivation is an abort, not a false return. That
         * behavior itself is a guardrail against future API changes.
         *
         * No random/flaky tests. All vectors are deterministic.
         */
        BOOST_TEST_MESSAGE("Running secp256k1 BIP32 derivation guardrail test");

        std::vector<unsigned char> seed = ParseHex(
            "c0ffee0102030405060708090a0b0c0d0e0f"
            "101112131415161718191a1b1c1d1e1f20");
        CExtKey masterKey;
        masterKey.SetSeed(seed.data(), seed.size());
        BOOST_CHECK(masterKey.key.IsValid());

        CExtPubKey masterPubKey = masterKey.Neuter();
        BOOST_CHECK(masterPubKey.pubkey.IsValid());
        BOOST_CHECK(masterPubKey.pubkey.IsCompressed());
        BOOST_CHECK(masterPubKey.pubkey == masterKey.key.GetPubKey());

        unsigned int path[] = {
            0x80000000 | 44,     // purpose (hardened)
            0x80000000 | 420,    // coin type = Hemp0x (hardened)
            0x80000000 | 0,      // account (hardened)
            0,                   // external chain (non-hardened)
            10                   // address index (non-hardened)
        };
        const unsigned int pathLen = sizeof(path) / sizeof(path[0]);

        CExtKey key = masterKey;
        CExtPubKey pubkey = masterPubKey;

        for (unsigned int i = 0; i < pathLen; i++) {
            CExtKey keyNew;
            BOOST_CHECK(key.Derive(keyNew, path[i]));
            CExtPubKey pubkeyNew = keyNew.Neuter();

            if (!(path[i] & 0x80000000)) {
                CExtPubKey pubkeyDirect;
                BOOST_CHECK(pubkey.Derive(pubkeyDirect, path[i]));
                BOOST_CHECK(pubkeyNew == pubkeyDirect);
            }

            BOOST_CHECK(keyNew.key.IsValid());
            BOOST_CHECK(pubkeyNew.pubkey.IsValid());

            key = keyNew;
            pubkey = pubkeyNew;
        }

        std::vector<unsigned char> derivedPubKey(pubkey.pubkey.begin(),
                                                   pubkey.pubkey.end());
        BOOST_CHECK(derivedPubKey.size() == 33);
        BOOST_CHECK(derivedPubKey == ParseHex("02f10fc2398483f46502c9eb1fc5ad4cca044432fafd407e28b4c9b753dd7c55af"));
        BOOST_CHECK(std::vector<unsigned char>(pubkey.chaincode.begin(), pubkey.chaincode.end()) ==
                    ParseHex("3488250925e17f9b15e1450f85988fe4a34018a239b620a976cdeb131a0ee27a"));

        CDataStream ssPriv(SER_DISK, CLIENT_VERSION);
        ssPriv << key;
        BOOST_CHECK(ssPriv.size() == 75);
        CExtKey keyRoundTrip;
        ssPriv >> keyRoundTrip;
        BOOST_CHECK(keyRoundTrip == key);

        CDataStream ssPub(SER_DISK, CLIENT_VERSION);
        ssPub << pubkey;
        BOOST_CHECK(ssPub.size() == 75);
        CExtPubKey pubkeyRoundTrip;
        ssPub >> pubkeyRoundTrip;
        BOOST_CHECK(pubkeyRoundTrip == pubkey);
    }

BOOST_AUTO_TEST_SUITE_END()
