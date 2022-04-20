#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Wagerr Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import WagerrTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, isolate_node, reconnect_isolated_node

'''
p2p_instantsend.py

Tests InstantSend functionality (prevent doublespend for unconfirmed transactions)
'''

class InstantSendTest(WagerrTestFramework):
    def set_test_params(self):
        self.set_wagerr_test_params(7, 3, fast_dip3_enforcement=True)
        # set sender,  receiver,  isolated nodes
        self.isolated_idx = 1
        self.receiver_idx = 2
        self.sender_idx = 3

    def run_test(self):
        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", 0)
        self.wait_for_sporks_same()
        self.mine_quorum()

        self.test_mempool_doublespend()
        self.test_block_doublespend()

    def test_block_doublespend(self):
        sender = self.nodes[self.sender_idx]
        receiver = self.nodes[self.receiver_idx]
        isolated = self.nodes[self.isolated_idx]

        # feed the sender with some balance
        sender_addr = sender.getnewaddress()
        self.nodes[0].sendtoaddress(sender_addr, 1)
        self.bump_mocktime(1)
        self.nodes[0].generate(2)
        self.sync_all()

        # create doublespending transaction, but don't relay it
        dblspnd_tx = self.create_raw_tx(sender, isolated, 0.5, 1, 100)
        # isolate one node from network
        isolate_node(isolated)
        # instantsend to receiver
        receiver_addr = receiver.getnewaddress()
        is_id = sender.sendtoaddress(receiver_addr, 0.9)
        # wait for the transaction to propagate
        connected_nodes = self.nodes.copy()
        del connected_nodes[self.isolated_idx]
        self.sync_mempools(connected_nodes)
        for node in connected_nodes:
            self.wait_for_instantlock(is_id, node)
        # send doublespend transaction to isolated node
        isolated.sendrawtransaction(dblspnd_tx['hex'])
        # generate block on isolated node with doublespend transaction
        self.bump_mocktime(1)
        isolated.generate(1)
        wrong_block = isolated.getbestblockhash()
        # connect isolated block to network
        reconnect_isolated_node(isolated, 0)
        # check doublespend block is rejected by other nodes
        timeout = 10
        for idx, node in enumerate(self.nodes):
            if idx == self.isolated_idx:
                continue
            res = node.waitforblock(wrong_block, timeout)
            assert res['hash'] != wrong_block
            # wait for long time only for first node
            timeout = 1
        # send coins back to the controller node without waiting for confirmations
        receiver.sendtoaddress(self.nodes[0].getnewaddress(), 0.9, "", "", True)
        assert_equal(receiver.getwalletinfo()["balance"], 0)
        # mine more blocks
        # TODO: mine these blocks on an isolated node
        self.bump_mocktime(1)
        # make sure the above TX is on node0
        self.sync_mempools([n for n in self.nodes if n is not isolated])
        self.nodes[0].generate(2)
        self.sync_all()

    def test_mempool_doublespend(self):
        sender = self.nodes[self.sender_idx]
        receiver = self.nodes[self.receiver_idx]
        isolated = self.nodes[self.isolated_idx]
        connected_nodes = self.nodes.copy()
        del connected_nodes[self.isolated_idx]

        # feed the sender with some balance
        sender_addr = sender.getnewaddress()
        self.nodes[0].sendtoaddress(sender_addr, 1)
        self.bump_mocktime(1)
        self.nodes[0].generate(2)
        self.sync_all()

        # create doublespending transaction, but don't relay it
        dblspnd_tx = self.create_raw_tx(sender, isolated, 0.5, 1, 100)
        # isolate one node from network
        isolate_node(isolated)
        # send doublespend transaction to isolated node
        dblspnd_txid = isolated.sendrawtransaction(dblspnd_tx['hex'])
        assert dblspnd_txid in set(isolated.getrawmempool())
        # let isolated node rejoin the network
        # The previously isolated node should NOT relay the doublespending TX
        reconnect_isolated_node(isolated, 0)
        for node in connected_nodes:
            assert_raises_rpc_error(-5, "No such mempool or blockchain transaction", node.getrawtransaction, dblspnd_txid)
        # Instantsend to receiver. The previously isolated node won't accept the tx but it should
        # request the correct TX from other nodes once the corresponding lock is received.
        # And this time the doublespend TX should be pruned once the correct tx is received.
        receiver_addr = receiver.getnewaddress()
        is_id = sender.sendtoaddress(receiver_addr, 0.9)
        # wait for the transaction to propagate
        self.sync_mempools()
        for node in self.nodes:
            self.wait_for_instantlock(is_id, node)
        assert dblspnd_txid not in set(isolated.getrawmempool())
        # send coins back to the controller node without waiting for confirmations
        sentback_id = receiver.sendtoaddress(self.nodes[0].getnewaddress(), 0.9, "", "", True)
        self.sync_mempools()
        for node in self.nodes:
            self.wait_for_instantlock(sentback_id, node)
        assert_equal(receiver.getwalletinfo()["balance"], 0)
        # mine more blocks
        self.bump_mocktime(1)
        self.nodes[0].generate(2)
        self.sync_all()

if __name__ == '__main__':
    InstantSendTest().main()
