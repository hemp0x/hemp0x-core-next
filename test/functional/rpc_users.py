#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test multiple RPC users and verify auth-failure logging."""

import os
import http.client
import urllib.parse
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import str_to_b64str, assert_equal

class HTTPBasicsTest (Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def setup_chain(self):
        super().setup_chain()
        #Append rpcauth to hemp0x.conf before initialization
        rpcauth = "rpcauth=rt:93648e835a54c573682c2eb19f882535$7681e9c5b74bdd85e78166031d2058e1069b3ed7ed967c93fc63abba06f31144"
        rpcauth2 = "rpcauth=rt2:f8607b1a88861fac29dfccf9b52ff9f$ff36a0c23c8c62b4846112e50fa888416e94c17bfd4c42f88fd8f55ec6a3137e"
        rpcuser = "rpcuser=rpcuser💻"
        rpcpassword = "rpcpassword=rpcpassword🔑"
        with open(os.path.join(self.options.tmpdir+"/node0", "hemp0x.conf"), 'a', encoding='utf8') as f:
            f.write(rpcauth+"\n")
            f.write(rpcauth2+"\n")
        with open(os.path.join(self.options.tmpdir+"/node1", "hemp0x.conf"), 'a', encoding='utf8') as f:
            f.write(rpcuser+"\n")
            f.write(rpcpassword+"\n")

    def run_test(self):

        ##################################################
        # Check correctness of the rpcauth config option #
        ##################################################
        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        auth_pair = url.username + ':' + url.password

        #New authpair generated via share/rpcuser tool
        password = "cA773lm788buwYe4g4WT+05pKyNruVKjQ25x3n0DQcM="

        #Second authpair with different username
        password2 = "8/F3uMDw4KSEbw96U3CA1C4X05dkHDN2BPFjTgZW4KI="
        auth_pair_new = "rt:"+password

        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()
        
        #Use new authpair to confirm both work
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair_new)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()

        #Wrong login name with rt's password
        auth_pair_new = "rtwrong:"+password
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair_new)}

        with self.nodes[0].assert_debug_log(expected_msgs=["incorrect password attempt"], timeout=10):
            conn = http.client.HTTPConnection(url.hostname, url.port)
            conn.connect()
            conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 401)
            conn.close()

        #Wrong password for rt
        auth_pair_new = "rt:"+password+"wrong"
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair_new)}

        with self.nodes[0].assert_debug_log(expected_msgs=["incorrect password attempt"], timeout=10):
            conn = http.client.HTTPConnection(url.hostname, url.port)
            conn.connect()
            conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 401)
            conn.close()

        #Verify credentials do not appear in debug.log
        debug_log = os.path.join(self.nodes[0].datadir, "regtest", "debug.log")
        with open(debug_log, encoding='utf-8') as dl:
            log_content = dl.read()
        assert "rtwrong" not in log_content, "Leaked username in debug log"
        assert password+"wrong" not in log_content, "Leaked password in debug log"

        #Correct for rt2
        auth_pair_new = "rt2:"+password2
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair_new)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()

        #Wrong password for rt2
        auth_pair_new = "rt2:"+password2+"wrong"
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair_new)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 401)
        conn.close()

        ###############################################################
        # Check correctness of the rpcuser/rpcpassword config options #
        ###############################################################
        url = urllib.parse.urlparse(self.nodes[1].url)

        # rpcuser and rpcpassword authpair
        rpc_user_auth_pair = "rpcuser💻:rpcpassword🔑"

        headers = {"Authorization": "Basic " + str_to_b64str(rpc_user_auth_pair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()

        #Wrong login name with rpcuser's password
        rpc_user_auth_pair = "rpcuserwrong:rpcpassword"
        headers = {"Authorization": "Basic " + str_to_b64str(rpc_user_auth_pair)}

        with self.nodes[1].assert_debug_log(expected_msgs=["incorrect password attempt"], timeout=10):
            conn = http.client.HTTPConnection(url.hostname, url.port)
            conn.connect()
            conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 401)
            conn.close()

        #Wrong password for rpcuser
        rpc_user_auth_pair = "rpcuser:rpcpasswordwrong"
        headers = {"Authorization": "Basic " + str_to_b64str(rpc_user_auth_pair)}

        with self.nodes[1].assert_debug_log(expected_msgs=["incorrect password attempt"], timeout=10):
            conn = http.client.HTTPConnection(url.hostname, url.port)
            conn.connect()
            conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 401)
            conn.close()

        #Verify legacy credentials do not appear in debug.log
        debug_log = os.path.join(self.nodes[1].datadir, "regtest", "debug.log")
        with open(debug_log, encoding='utf-8') as dl:
            log_content = dl.read()
        assert "rpcuserwrong" not in log_content, "Leaked username in debug log"
        assert "rpcpasswordwrong" not in log_content, "Leaked password in debug log"

        ################################################################
        # Verify successful auth still works after repeated failures    #
        ################################################################

        # rpcauth path: correct credentials still work on node 0
        url = urllib.parse.urlparse(self.nodes[0].url)
        auth_pair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()

        # rpcuser/rpcpassword path: correct credentials still work on node 1
        url = urllib.parse.urlparse(self.nodes[1].url)
        rpc_user_auth_pair = "rpcuser💻:rpcpassword🔑"
        headers = {"Authorization": "Basic " + str_to_b64str(rpc_user_auth_pair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()


if __name__ == '__main__':
    HTTPBasicsTest ().main ()
