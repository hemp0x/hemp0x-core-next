// Copyright (c) 2019 Veil developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <test/test_hemp0x.h>

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <chainparamsbase.h>
#include <crypto/ethash/lib/ethash/endianness.hpp>
#include <crypto/ethash/include/ethash/progpow.hpp>

#include "crypto/ethash/helpers.hpp"
#include "crypto/ethash/progpow_test_vectors.hpp"

#include <array>
#include <cstring>

namespace {
CBlockHeader MakeKawpowTestHeader(uint32_t height)
{
    CBlockHeader header;
    header.nVersion = 4;
    header.hashPrevBlock = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    header.hashMerkleRoot = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    header.nTime = 1767000000;
    header.nBits = 0x1e0fffff;
    header.nHeight = height;
    header.nNonce64 = 0x123456789abcdef0ULL;
    header.mix_hash = uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    return header;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(kawpow_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(kawpow_l1_cache)
{
    auto& context = get_ethash_epoch_context_0();

    constexpr auto test_size = 20;
    std::array<uint32_t, test_size> cache_slice;
    for (size_t i = 0; i < cache_slice.size(); ++i)
    cache_slice[i] = ethash::le::uint32(context.l1_cache[i]);

    const std::array<uint32_t, test_size> expected{
        {2492749011, 430724829, 2029256771, 3095580433, 3583790154, 3025086503,
         805985885, 4121693337, 2320382801, 3763444918, 1006127899, 1480743010,
         2592936015, 2598973744, 3038068233, 2754267228, 2867798800, 2342573634,
         467767296, 246004123}};
    int i = 0;
    for (auto item : cache_slice) {
        BOOST_CHECK(item == expected[i]);
        i++;
    }
}

BOOST_AUTO_TEST_CASE(kawpow_hash_empty)
{
    auto& context = get_ethash_epoch_context_0();

    int count = 1000;
    ethash_result result;
    while (count > 0) {
        result = progpow::hash(context, count, {}, 0);
        --count;
    }

    const auto mix_hex = "6e97b47b134fda0c7888802988e1a373affeb28bcd813b6e9a0fc669c935d03a";
    const auto final_hex = "e601a7257a70dc48fccc97a7330d704d776047623b92883d77111fb36870f3d1";
    BOOST_CHECK_EQUAL(to_hex(result.mix_hash), mix_hex);
    BOOST_CHECK_EQUAL(to_hex(result.final_hash), final_hex);
}

BOOST_AUTO_TEST_CASE(kawpow_hash_30000)
{
    const int block_number = 30000;
    const auto header =
            to_hash256("ffeeddccbbaa9988776655443322110000112233445566778899aabbccddeeff");
    const uint64_t nonce = 0x123456789abcdef0;

    auto context = ethash::create_epoch_context(ethash::get_epoch_number(block_number));

    const auto result = progpow::hash(*context, block_number, header, nonce);
    const auto mix_hex = "177b565752a375501e11b6d9d3679c2df6197b2cab3a1ba2d6b10b8c71a3d459";
    const auto final_hex = "c824bee0418e3cfb7fae56e0d5b3b8b14ba895777feea81c70c0ba947146da69";
    BOOST_CHECK_EQUAL(to_hex(result.mix_hash), mix_hex);
    BOOST_CHECK_EQUAL(to_hex(result.final_hash), final_hex);

}

BOOST_AUTO_TEST_CASE(kawpow_hash_and_verify)
{
    ethash::epoch_context_ptr context{nullptr, nullptr};

    for (auto& t : progpow_hash_test_cases)
    {
        const auto epoch_number = ethash::get_epoch_number(t.block_number);
        if (!context || context->epoch_number != epoch_number)
            context = ethash::create_epoch_context(epoch_number);

        const auto header_hash = to_hash256(t.header_hash_hex);
        const auto nonce = std::stoull(t.nonce_hex, nullptr, 16);
        const auto result = progpow::hash(*context, t.block_number, header_hash, nonce);
        BOOST_CHECK_EQUAL(to_hex(result.mix_hash), t.mix_hash_hex);
        BOOST_CHECK_EQUAL(to_hex(result.final_hash), t.final_hash_hex);

        auto success = progpow::verify(
                *context, t.block_number, header_hash, result.mix_hash, nonce, result.final_hash);
        BOOST_CHECK(success);

        auto lower_boundary = result.final_hash;
        --lower_boundary.bytes[31];
        auto final_failure = progpow::verify(
                *context, t.block_number, header_hash, result.mix_hash, nonce, lower_boundary);
        BOOST_CHECK(!final_failure);

        auto different_mix = result.mix_hash;
        ++different_mix.bytes[7];
        auto mix_failure = progpow::verify(
                *context, t.block_number, header_hash, different_mix, nonce, result.final_hash);
        BOOST_CHECK(!mix_failure);
    }
}

BOOST_AUTO_TEST_CASE(kawpow_search)
{
    auto ctxp = ethash::create_epoch_context_full(0);
    auto& ctx = *ctxp;
    auto& ctxl = reinterpret_cast<const ethash::epoch_context&>(ctx);

    auto boundary = to_hash256("00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    auto sr = progpow::search(ctx, 0, {}, boundary, 700, 100);
    auto srl = progpow::search_light(ctxl, 0, {}, boundary, 700, 100);

    BOOST_CHECK(sr.mix_hash == ethash::hash256{});
    BOOST_CHECK(sr.final_hash == ethash::hash256{});
    BOOST_CHECK(sr.nonce == 0x0);
    BOOST_CHECK(sr.mix_hash == srl.mix_hash);
    BOOST_CHECK(sr.final_hash == srl.final_hash);
    BOOST_CHECK(sr.nonce == srl.nonce);

    // Switch it to a different starting nonce and find another solution
    sr = progpow::search(ctx, 0, {}, boundary, 300, 100);
    srl = progpow::search_light(ctxl, 0, {}, boundary, 300, 100);

    BOOST_CHECK(sr.mix_hash != ethash::hash256{});
    BOOST_CHECK(sr.final_hash != ethash::hash256{});
    BOOST_CHECK(sr.nonce == 395);
    BOOST_CHECK(sr.mix_hash == srl.mix_hash);
    BOOST_CHECK(sr.final_hash == srl.final_hash);
    BOOST_CHECK(sr.nonce == srl.nonce);

    auto r = progpow::hash(ctx, 0, {}, 395);
    BOOST_CHECK(sr.final_hash == r.final_hash);
    BOOST_CHECK(sr.mix_hash == r.mix_hash);
}

BOOST_AUTO_TEST_CASE(kawpow_header_hash_isolation)
{
    const uint32_t old_kawpow_activation_time = nKAWPOWActivationTime;
    nKAWPOWActivationTime = 1766126932;

    CBlockHeader header = MakeKawpowTestHeader(10000);

    uint256 headerHash = header.GetKAWPOWHeaderHash();

    CBlockHeader header2 = header;
    header2.nNonce64 = 0xdeadbeefcafebabeULL;
    BOOST_CHECK_EQUAL(headerHash.GetHex(), header2.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header3 = header;
    header3.mix_hash = uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    BOOST_CHECK_EQUAL(headerHash.GetHex(), header3.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header4 = header;
    header4.nTime = 1767000001;
    BOOST_CHECK(headerHash.GetHex() != header4.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header5 = header;
    header5.nBits = 0x1e0ffffe;
    BOOST_CHECK(headerHash.GetHex() != header5.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header6 = header;
    header6.hashMerkleRoot = uint256S("3333333333333333333333333333333333333333333333333333333333333333");
    BOOST_CHECK(headerHash.GetHex() != header6.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header7 = header;
    header7.hashPrevBlock = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    BOOST_CHECK(headerHash.GetHex() != header7.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header8 = header;
    header8.nVersion = 5;
    BOOST_CHECK(headerHash.GetHex() != header8.GetKAWPOWHeaderHash().GetHex());

    CBlockHeader header9 = header;
    header9.nHeight = 10001;
    BOOST_CHECK(headerHash.GetHex() != header9.GetKAWPOWHeaderHash().GetHex());

    nKAWPOWActivationTime = old_kawpow_activation_time;
}

BOOST_AUTO_TEST_CASE(kawpow_deterministic_block_header)
{
    const uint32_t old_kawpow_activation_time = nKAWPOWActivationTime;
    nKAWPOWActivationTime = 1766126932;

    CBlockHeader header = MakeKawpowTestHeader(7500);

    uint256 hash = header.GetHash();
    BOOST_CHECK_EQUAL(hash.GetHex(),
            "5f200d6142e6e005163a229d74295ac5cab81c3145f0b51ca7bad885f35bf75e");

    uint256 mix_hash_out;
    uint256 hashFull = header.GetHashFull(mix_hash_out);
    BOOST_CHECK_EQUAL(hashFull.GetHex(),
            "33e6205e48d1246979b89e11475426d20f314b789b5fe13f81b1f942bf4d37c6");

    BOOST_CHECK_EQUAL(mix_hash_out.GetHex(),
            "89fddecaf79e8bc59147eaeb7ea4e83e6a9793dd41c4126ca7b15de97aa6cf34");

    nKAWPOWActivationTime = old_kawpow_activation_time;
}

BOOST_AUTO_TEST_CASE(kawpow_epoch_boundary)
{
    const uint32_t old_kawpow_activation_time = nKAWPOWActivationTime;
    nKAWPOWActivationTime = 1766126932;

    CBlockHeader header7499 = MakeKawpowTestHeader(7499);

    CBlockHeader header7500 = header7499;
    header7500.nHeight = 7500;

    CBlockHeader header7501 = header7499;
    header7501.nHeight = 7501;

    uint256 hash7499 = header7499.GetHash();
    uint256 hash7500 = header7500.GetHash();
    uint256 hash7501 = header7501.GetHash();

    BOOST_CHECK(hash7499.GetHex() != hash7500.GetHex());
    BOOST_CHECK(hash7500.GetHex() != hash7501.GetHex());

    BOOST_CHECK_EQUAL(hash7499.GetHex(),
            "b93d8281004233806115efc52bebc32e02f441c39f0bb97df9197057bf6d2047");
    BOOST_CHECK_EQUAL(hash7500.GetHex(),
            "5f200d6142e6e005163a229d74295ac5cab81c3145f0b51ca7bad885f35bf75e");
    BOOST_CHECK_EQUAL(hash7501.GetHex(),
            "3c6e8f913bf64fa56fbda8f0ff4a37ff5a0a2613235f862d22b906d4971a3c5b");

    nKAWPOWActivationTime = old_kawpow_activation_time;
}

BOOST_AUTO_TEST_CASE(kawpow_genesis_x16r_guardrail)
{
    const auto chain_params = CreateChainParams(CBaseChainParams::MAIN);
    const CBlockHeader& genesis = chain_params->GenesisBlock();

    BOOST_CHECK_EQUAL(genesis.GetX16RHash().GetHex(),
            "0000009907c43e63467860fcb2a76eed0200e4f918de00bfea3fa35aa22dddd0");
    BOOST_CHECK_EQUAL(chain_params->GetConsensus().hashGenesisBlock.GetHex(),
            "0000009907c43e63467860fcb2a76eed0200e4f918de00bfea3fa35aa22dddd0");
}

BOOST_AUTO_TEST_CASE(kawpow_endian_helpers)
{
    // Little-endian identity: on little-endian, le::uint32 is no-op
    uint32_t testVal = 0x12345678;
    BOOST_CHECK_EQUAL(ethash::le::uint32(testVal), testVal);

    const uint8_t buf[4] = {0x78, 0x56, 0x34, 0x12};
    uint32_t raw;
    std::memcpy(&raw, buf, sizeof(raw));
    uint32_t decoded = ethash::le::uint32(raw);
    BOOST_CHECK_EQUAL(decoded, testVal);

    // Known edge cases
    BOOST_CHECK_EQUAL(ethash::le::uint32(0x00000000u), 0x00000000u);
    BOOST_CHECK_EQUAL(ethash::le::uint32(0xffffffffu), 0xffffffffu);
    BOOST_CHECK_EQUAL(ethash::le::uint32(0x00000001u), 0x00000001u);
}

BOOST_AUTO_TEST_CASE(kawpow_to_hash256_roundtrip)
{
    const std::string hex = "ffeeddccbbaa9988776655443322110000112233445566778899aabbccddeeff";
    ethash::hash256 h = to_hash256(hex);
    BOOST_CHECK_EQUAL(to_hex(h), hex);
}

BOOST_AUTO_TEST_SUITE_END()
