// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

//#include <util/system.h>
#include <amount.h>
#include <primitives/transaction.h>

class CBettingsView;
class CBlockIndex;
class CCoinsViewCache;
class CPayoutInfoDB;
class CBetOut;
class CTransaction;
class CBlock;
class UniValue;
class JSONRPCRequest;
class CBettingTx;

enum WagerrBettingProtocolNr {
    WBP01 = 1,
    WBP02 = 2,
    WBP03 = 3,
    WBP04 = 4,
    WBP05 = 5
};

/** Validating the payout block using the payout vector. **/
bool IsBlockPayoutsValid(CBettingsView &bettingsViewCache, const std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayoutsIn, const CBlock& block, const int nBlockHeight, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward);

/** Check Betting Tx when try accept tx to memory pool **/
bool CheckBettingTx(const CBettingsView& bettingsViewCache, const std::unique_ptr<CBettingTx> bettingTx, CAmount betAmount, bool validOracleTx, const int height);
bool CheckBettingTx(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, const CTransaction& tx, const int height);

/** Parse the transaction for betting data **/
void ProcessBettingTx(const CCoinsViewCache  &view, CBettingsView& bettingsViewCache, std::vector<uint32_t>& vCurResultIDs, const CTransactionRef& tx, const CBlockIndex* pindex, const CBlock &block, const bool wagerrProtocolV3);

CAmount GetBettingPayouts(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, const CBlockIndex* pindexPrev, std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayouts);

bool BettingUndo(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, const CBlockIndex* pindex, const std::vector<CTransactionRef>& vtx);

UniValue getbetbytxid(const JSONRPCRequest& request);
UniValue getbet(const JSONRPCRequest& request);

#endif // WAGERR_BET_H
