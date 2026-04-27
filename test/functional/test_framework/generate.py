#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Functional-test-only block generation helpers.

Core Next release binaries intentionally do not expose local mining RPCs. The
functional test framework still needs deterministic regtest block production,
so tests construct blocks out of process and submit them through the same pool
RPC surface external software uses.
"""

from .blocktools import create_block_from_template, create_coinbase, add_witness_commitment
from .mininode import COIN
from .messages import bytes_to_hex_str, hex_str_to_bytes


def generate_blocks(rpc, nblocks, address=None):
    if address is None:
        address = rpc.getnewaddress()
    return generate_blocks_to_address(rpc, nblocks, address)


def generate_blocks_to_address(rpc, nblocks, address):
    address_info = rpc.validateaddress(address)
    if not address_info.get("isvalid", False):
        raise_rpc_error(-5, "Invalid address")

    script_pub_key = hex_str_to_bytes(address_info["scriptPubKey"])
    block_hashes = []

    for _ in range(nblocks):
        template = get_block_template_or_fallback(rpc)
        height = template["height"]
        value = template["coinbasevalue"]
        tip_header = rpc.getblockheader(template["previousblockhash"])
        tip_time = tip_header["time"]
        block_time = template.get("curtime", tip_time + 1)
        coinbase = create_coinbase(height, value=value, script_pub_key=script_pub_key)
        block = create_block_from_template(template, coinbase, block_time)

        # If the block contains transactions with witness data, add the
        # required witness commitment to the coinbase so the block passes
        # segwit validation.
        has_witness_tx = any(
            tx.wit.vtxinwit for tx in block.vtx[1:]
        )
        if has_witness_tx:
            add_witness_commitment(block)

        block.solve()

        result = rpc.submitblock(bytes_to_hex_str(block.serialize()))
        if result is not None:
            raise_rpc_error(-1, "submitblock failed: %s" % result)
        block_hashes.append(block.hash)

    return block_hashes


def get_block_template_or_fallback(rpc):
    try:
        return rpc.getblocktemplate()
    except Exception as e:
        if not hasattr(e, "error") or e.error.get("code") not in (-9, -10):
            raise

    tip_hash = rpc.getbestblockhash()
    tip_header = rpc.getblockheader(tip_hash)
    # Use VERSIONBITS_TOP_BITS_ASSETS (0x30000000) with asset signaling bit (bit 6).
    version = 0x30000000 | (1 << 6)
    return {
        "version": version,
        "previousblockhash": tip_hash,
        "height": tip_header["height"] + 1,
        "bits": tip_header["bits"],
        "curtime": tip_header["time"] + 1,
        "coinbasevalue": 10 * COIN,
        "transactions": [{"data": rpc.getrawtransaction(txid)} for txid in rpc.getrawmempool()],
    }


def raise_rpc_error(code, message):
    from .authproxy import JSONRPCException
    raise JSONRPCException({"code": code, "message": message})
