#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Protect the current RPC command surface from accidental removals."""

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal, p2p_port, rpc_port


EXPECTED_RPC_COMMANDS = {
    "abandontransaction",
    "abortrescan",
    "addmultisigaddress",
    "addnode",
    "addtagtoaddress",
    "addwitnessaddress",
    "backupwallet",
    "bumpfee",
    "cancelsnapshotrequest",
    "checkaddressrestriction",
    "checkaddresstag",
    "checkglobalrestriction",
    "clearbanned",
    "clearmempool",
    "clearmessages",
    "combinerawtransaction",
    "createmultisig",
    "createrawtransaction",
    "decodeblock",
    "decoderawtransaction",
    "decodescript",
    "disconnectnode",
    "distributereward",
    "dumpprivkey",
    "dumpwallet",
    "encryptwallet",
    "estimatefee",
    "estimatesmartfee",
    "freezeaddress",
    "freezerestrictedasset",
    "fundrawtransaction",
    "getaccount",
    "getaccountaddress",
    "getaddednodeinfo",
    "getaddressbalance",
    "getaddressdeltas",
    "getaddressesbyaccount",
    "getaddressmempool",
    "getaddresstxids",
    "getaddressutxos",
    "getassetdata",
    "getbalance",
    "getbestblockhash",
    "getblock",
    "getblockchaininfo",
    "getblockcount",
    "getblockhash",
    "getblockhashes",
    "getblockheader",
    "getblocktemplate",
    "getcacheinfo",
    "getchaintips",
    "getchaintxstats",
    "getconnectioncount",
    "getdifficulty",
    "getdistributestatus",
    "getinfo",
    "getkawpowhash",
    "getmasterkeyinfo",
    "getmemoryinfo",
    "getmempoolancestors",
    "getmempooldescendants",
    "getmempoolentry",
    "getmempoolinfo",
    "getmininginfo",
    "getmywords",
    "getnettotals",
    "getnetworkhashps",
    "getnetworkinfo",
    "getnewaddress",
    "getpeerinfo",
    "getrawchangeaddress",
    "getrawmempool",
    "getrawtransaction",
    "getreceivedbyaccount",
    "getreceivedbyaddress",
    "getrpcinfo",
    "getsnapshot",
    "getsnapshotrequest",
    "getspentinfo",
    "gettransaction",
    "gettxout",
    "gettxoutproof",
    "gettxoutsetinfo",
    "getunconfirmedbalance",
    "getverifierstring",
    "getwalletinfo",
    "help",
    "importaddress",
    "importmulti",
    "importprivkey",
    "importprunedfunds",
    "importpubkey",
    "importwallet",
    "issue",
    "issuequalifierasset",
    "issuerestrictedasset",
    "issueunique",
    "isvalidverifierstring",
    "keypoolrefill",
    "listaccounts",
    "listaddressesfortag",
    "listaddressgroupings",
    "listaddressrestrictions",
    "listassets",
    "listbanned",
    "listglobalrestrictions",
    "listlockunspent",
    "listmyassets",
    "listreceivedbyaccount",
    "listreceivedbyaddress",
    "listsinceblock",
    "listsnapshotrequests",
    "listtagsforaddress",
    "listtransactions",
    "listunspent",
    "listwallets",
    "lockunspent",
    "move",
    "ping",
    "pprpcsb",
    "preciousblock",
    "prioritisetransaction",
    "pruneblockchain",
    "purgesnapshot",
    "reissue",
    "reissuerestrictedasset",
    "removeprunedfunds",
    "removetagfromaddress",
    "requestsnapshot",
    "rescanblockchain",
    "savemempool",
    "sendfrom",
    "sendfromaddress",
    "sendmany",
    "sendmessage",
    "sendrawtransaction",
    "sendtoaddress",
    "setaccount",
    "setban",
    "setnetworkactive",
    "settxfee",
    "signmessage",
    "signmessagewithprivkey",
    "signrawtransaction",
    "stop",
    "submitblock",
    "subscribetochannel",
    "testmempoolaccept",
    "transfer",
    "transferfromaddress",
    "transferfromaddresses",
    "transferqualifier",
    "unfreezeaddress",
    "unfreezerestrictedasset",
    "unsubscribefromchannel",
    "uptime",
    "validateaddress",
    "verifychain",
    "verifymessage",
    "verifytxoutproof",
    "viewallmessagechannels",
    "viewallmessages",
    "viewmyrestrictedaddresses",
    "viewmytaggedaddresses",
}


def commands_from_help(help_text):
    commands = set()
    for line in help_text.splitlines():
        line = line.strip()
        if not line or line.startswith("=="):
            continue
        command = line.split()[0]
        if command and command[0].islower():
            commands.add(command)
    return commands


class RpcHelpCompatibilityTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-regtest",
            "-port={}".format(p2p_port(0)),
            "-rpcport={}".format(rpc_port(0)),
        ]]

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Checking current RPC command inventory")
        commands = commands_from_help(node.help())
        missing = sorted(EXPECTED_RPC_COMMANDS - commands)
        assert_equal(missing, [])

        self.log.info("Checking representative operational RPCs")
        node.getblockchaininfo()
        node.getnetworkinfo()
        node.getwalletinfo()
        node.getmininginfo()
        node.getnetworkhashps()
        node.getrpcinfo()

        address = node.getnewaddress()
        assert_equal(node.validateaddress(address)["isvalid"], True)

        self.log.info("Checking pool and asset RPC help remains available")
        for command in ("getblocktemplate", "submitblock", "pprpcsb", "getkawpowhash", "listassets"):
            assert_equal(command in node.help(command), True)


if __name__ == '__main__':
    RpcHelpCompatibilityTest().main()
