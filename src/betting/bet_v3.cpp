// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_v2.h>
#include <betting/bet_v3.h>
#include <betting/bet_db.h>
#include <betting/oracles.h>
#include <dstencode.h>
#include <pos/kernel.h>
#include <validation.h>
#include <base58.h>

void GetPLRewardPayoutsV3(const uint32_t nNewBlockHeight, const CAmount fee, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    PeerlessBetKey zeroKey{nNewBlockHeight, COutPoint()};

    // Set the OMNO and Dev reward addresses
    CScript payoutScriptDev;
    CScript payoutScriptOMNO;
    if (!GetFeePayoutScripts(nNewBlockHeight, payoutScriptDev, payoutScriptOMNO)) {
        LogPrintf("Unable to find oracle, skipping payouts\n");
        return;
    }

    // Calculate the OMNO reward and the Dev reward.
    // 40% of total fee
    CAmount nOMNOReward = (fee * 4000) / BET_ODDSDIVISOR;
    // 10% of total fee
    CAmount nDevReward  = (fee * 1000) / BET_ODDSDIVISOR;

    if (nDevReward > 0) {
        // Add both reward payouts to the payout vector.
        CBetOut betOutDev(nDevReward, payoutScriptDev, 0);
        CPayoutInfoDB payoutInfoDev(zeroKey, PayoutType::bettingReward);
        vExpectedPayouts.emplace_back(betOutDev);
        vPayoutsInfo.emplace_back(payoutInfoDev);
    }
    if (nOMNOReward > 0) {
        CBetOut betOutOMNO(nOMNOReward, payoutScriptOMNO, 0);
        CPayoutInfoDB payoutInfoOMNO(zeroKey, PayoutType::bettingReward);
        vExpectedPayouts.emplace_back(betOutOMNO);
        vPayoutsInfo.emplace_back(payoutInfoOMNO);
    }
}

uint32_t GetBetSearchStartHeight(int nHeight) {
    if (nHeight >= Params().GetConsensus().WagerrProtocolV3StartHeight()) {
        return nHeight >= Params().GetConsensus().BetBlocksIndexTimespanV3() ? nHeight - Params().GetConsensus().BetBlocksIndexTimespanV3() : Params().GetConsensus().WagerrProtocolV3StartHeight();
    } else {
        return nHeight >= Params().GetConsensus().BetBlocksIndexTimespanV2() ? nHeight - Params().GetConsensus().BetBlocksIndexTimespanV2() : 0;
    }
}

/**
 * Creates the bet payout vector for all winning CPeerlessBetDB bets.
 *
 * @return payout vector, payouts info vector.
 */
void GetPLBetPayoutsV3(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    uint64_t refundOdds{BET_ODDSDIVISOR};

    // Get all the results posted in the prev block.
    std::vector<CPeerlessResultDB> results = GetPLResults(nLastBlockHeight);

    bool fWagerrProtocolV3 = nLastBlockHeight >= Params().GetConsensus().WagerrProtocolV3StartHeight();

    CAmount effectivePayoutsSum, grossPayoutsSum = effectivePayoutsSum = 0;

    LogPrint(BCLog::BETTING, "Start generating peerless bets payouts...\n");

    for (auto result : results) {

        if (result.nResultType == ResultType::eventClosed)
            continue;

        LogPrint(BCLog::BETTING, "Looking for bets of eventId: %lu\n", result.nEventId);

        // look bets during the bet interval
        uint32_t startHeight = GetBetSearchStartHeight(nLastBlockHeight);
        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<PeerlessBetKey, CPeerlessBetDB>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(PeerlessBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {
            bool legHalfLose = false;
            bool legHalfWin = false;
            bool legRefund = false;

            PeerlessBetKey uniBetKey;
            CPeerlessBetDB uniBet;
            CBettingDB::BytesToDbType(it->Key(), uniBetKey);
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            // skip if bet is already handled
            if (fWagerrProtocolV3 && uniBet.IsCompleted()) continue;

            bool completedBet = false;
            // {onchainOdds, effectiveOdds}
            std::pair<uint32_t, uint32_t> finalOdds{0, 0};

            // parlay bet
            if (uniBet.legs.size() > 1) {
                bool resultFound = false;
                for (auto leg : uniBet.legs) {
                    // if we found one result for parlay - check win condition for this and each other legs
                    if (leg.nEventId == result.nEventId) {
                        resultFound = true;
                        break;
                    }
                }
                if (resultFound) {
                    // make assumption that parlay is completed and this result is last
                    completedBet = true;
                    // find all results for all legs
                    bool firstOddMultiply = true;
                    for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                        CPeerlessLegDB &leg = uniBet.legs[idx];
                        CPeerlessBaseEventDB &lockedEvent = uniBet.lockedEvents[idx];
                        // skip this bet if incompleted (can't find one result)
                        CPeerlessResultDB res;
                        if (bettingsViewCache.results->Read(ResultKey{leg.nEventId}, res)) {
                            // {onchainOdds, effectiveOdds}
                            std::pair<uint32_t, uint32_t> betOdds;
                            // if bet placed before 2 mins of event started - refund this bet
                            if (lockedEvent.nStartTime > 0 && uniBet.betTime > ((int64_t)lockedEvent.nStartTime - Params().GetConsensus().BetPlaceTimeoutBlocks())) {
                                betOdds = fWagerrProtocolV3 ? std::pair<uint32_t, uint32_t>{refundOdds, refundOdds} : std::pair<uint32_t, uint32_t>{0, 0};
                            }
                            else {
                                betOdds = GetBetOdds(leg, lockedEvent, res, fWagerrProtocolV3);
                            }

                            if (betOdds.first == 0) { }
                            else if (betOdds.first == refundOdds) {
                                legRefund = true;
                            }
                            else if (betOdds.first == refundOdds / 2) {
                                legHalfLose = true;
                            }
                            else if (betOdds.first < GetBetPotentialOdds(leg, lockedEvent)) {
                                legHalfWin = true;
                            }
                            // multiply odds
                            if (firstOddMultiply) {
                                finalOdds.first = betOdds.first;
                                finalOdds.second = betOdds.second ;
                                firstOddMultiply = false;
                            }
                            else {
                                finalOdds.first = static_cast<uint32_t>(((uint64_t) finalOdds.first * betOdds.first) / BET_ODDSDIVISOR);
                                finalOdds.second = static_cast<uint32_t>(((uint64_t) finalOdds.second * betOdds.second) / BET_ODDSDIVISOR);
                            }
                        }
                        else {
                            completedBet = false;
                            break;
                        }
                    }
                }
            }
            // single bet
            else if (uniBet.legs.size() == 1) {
                CPeerlessLegDB &singleBet = uniBet.legs[0];
                CPeerlessBaseEventDB &lockedEvent = uniBet.lockedEvents[0];

                if (singleBet.nEventId == result.nEventId) {
                    completedBet = true;

                    // if bet placed before 2 mins of event started - refund this bet
                    if (lockedEvent.nStartTime > 0 && uniBet.betTime > ((int64_t)lockedEvent.nStartTime - Params().GetConsensus().BetPlaceTimeoutBlocks())) {
                        if (fWagerrProtocolV3) {
                            finalOdds = std::pair<uint32_t, uint32_t>{refundOdds, refundOdds};
                        } else {
                            finalOdds = std::pair<uint32_t, uint32_t>{0, 0};
                        }
                    } else if ((!fWagerrProtocolV3) && nLastBlockHeight - lockedEvent.nEventCreationHeight > Params().GetConsensus().BetBlocksIndexTimespanV2()) {
                        finalOdds = std::pair<uint32_t, uint32_t>{0, 0};
                    }
                    else {
                        finalOdds = GetBetOdds(singleBet, lockedEvent, result, fWagerrProtocolV3);
                    }

                    if (finalOdds.first == 0) { }
                    else if (finalOdds.first == refundOdds) {
                        legRefund = true;
                    }
                    else if (finalOdds.first == refundOdds / 2) {
                        legHalfLose = true;
                    }
                    else if (finalOdds.first < GetBetPotentialOdds(singleBet, lockedEvent)) {
                        legHalfWin = true;
                    }
                }
            }

            if (completedBet) {
                if (uniBet.betAmount < (Params().GetConsensus().MinBetPayoutRange() * COIN) || uniBet.betAmount > (Params().GetConsensus().MaxBetPayoutRange() * COIN)) {
                    finalOdds = fWagerrProtocolV3 ? std::pair<uint32_t, uint32_t>{refundOdds, refundOdds} : std::pair<uint32_t, uint32_t>{0, 0};
                }

                CAmount effectivePayout, grossPayout, burn;

                if (!fWagerrProtocolV3) {
                    CalculatePayoutBurnAmounts(uniBet.betAmount, finalOdds.first, effectivePayout, burn);
                }
                else {
                    effectivePayout = uniBet.betAmount * finalOdds.second / BET_ODDSDIVISOR;
                    grossPayout = uniBet.betAmount * finalOdds.first / BET_ODDSDIVISOR;
                    effectivePayoutsSum += effectivePayout;
                    grossPayoutsSum += grossPayout;
                }

                if (effectivePayout > 0) {
                    // Add winning payout to the payouts vector.
                    CPayoutInfoDB payoutInfo(uniBetKey, finalOdds.second <= refundOdds ? PayoutType::bettingRefund : PayoutType::bettingPayout);
                    vExpectedPayouts.emplace_back(effectivePayout, GetScriptForDestination(uniBet.playerAddress), uniBet.betAmount);
                    vPayoutsInfo.emplace_back(payoutInfo);

                    if (effectivePayout < uniBet.betAmount) {
                        uniBet.resultType = BetResultType::betResultPartialLose;
                    }
                    else if (finalOdds.first == refundOdds) {
                        uniBet.resultType = BetResultType::betResultRefund;
                    }
                    else if ((uniBet.legs.size() == 1 && legHalfWin) ||
                            (uniBet.legs.size() > 1 && (legHalfWin || legHalfLose || legRefund))) {
                        uniBet.resultType = BetResultType::betResultPartialWin;
                    }
                    else {
                        uniBet.resultType = BetResultType::betResultWin;
                    }
                    // write payout height: result height + 1
                    uniBet.payoutHeight = (uint32_t) nNewBlockHeight;
                }
                else {
                    uniBet.resultType = BetResultType::betResultLose;
                }
                uniBet.payout = effectivePayout;
                LogPrint(BCLog::BETTING, "\nBet %s is handled!\nPlayer address: %s\nFinal onchain odds: %lu, effective odds: %lu\nPayout: %lu\n",
                    uniBetKey.outPoint.ToStringShort(), EncodeDestination(uniBet.playerAddress), finalOdds.first, finalOdds.second, effectivePayout);
                LogPrint(BCLog::BETTING, "Legs:");
                for (auto &leg : uniBet.legs) {
                    LogPrint(BCLog::BETTING, " (eventId: %lu, outcome: %lu) ", leg.nEventId, leg.nOutcome);
                }
                // if handling bet is completed - mark it
                uniBet.SetCompleted();
                vEntriesToUpdate.emplace_back(std::pair<PeerlessBetKey, CPeerlessBetDB>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.bets->Update(pair.first, pair.second);
        }
    }

    if (!fWagerrProtocolV3) {
        GetPLRewardPayoutsV2(nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
    }
    else {
        GetPLRewardPayoutsV3(nNewBlockHeight, grossPayoutsSum - effectivePayoutsSum, vExpectedPayouts, vPayoutsInfo);
    }

    LogPrint(BCLog::BETTING, "Finished generating payouts...\n");

}

/**
 * Creates the bet payout vector for all winning Quick Games bets.
 *
 * @return payout vector.
 */
void GetQuickGamesBetPayouts(CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    if (nLastBlockHeight >= Params().GetConsensus().QuickGamesEndHeight()){
        return;
    }

    LogPrint(BCLog::BETTING, "Start generating quick games bets payouts...\n");

    CBlockIndex *blockIndex = ::ChainActive()[nLastBlockHeight];
    std::map<std::string, CAmount> mExpectedRewards;
    uint32_t blockHeight = static_cast<uint32_t>(nLastBlockHeight);
    auto it = bettingsViewCache.quickGamesBets->NewIterator();
    std::vector<std::pair<QuickGamesBetKey, CQuickGamesBetDB>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(QuickGamesBetKey{blockHeight, COutPoint()})); it->Valid(); it->Next()) {
        QuickGamesBetKey qgKey;
        CQuickGamesBetDB qgBet;

        CBettingDB::BytesToDbType(it->Key(), qgKey);

        if (qgKey.blockHeight != blockHeight)
            break;

        CBettingDB::BytesToDbType(it->Value(), qgBet);
        // skip if already handled
        if (qgBet.IsCompleted())
            continue;

        // invalid game index
        if (qgBet.gameType >= Params().QuickGamesArr().size())
            continue;

        // handle bet by specific game handler from quick games framework
        const CQuickGamesView& gameView = Params().QuickGamesArr()[qgBet.gameType];
        // if odds == 0 - bet lose, if odds > OddsDivisor - bet win, if odds == BET_ODDSDIVISOR - bet refunded
        arith_uint256 hashProofOfStake = UintToArith256(mapProofOfStake[blockIndex->GetBlockHash()]);
        if (hashProofOfStake == 0) {
            CBlock block;
            ReadBlockFromDisk(block, blockIndex, Params().GetConsensus());
            if (block.IsProofOfStake()) {
                uint256 calculatedHashProofOfStake;

                if (CheckProofOfStake(block, calculatedHashProofOfStake, blockIndex)) {
                    hashProofOfStake = UintToArith256(calculatedHashProofOfStake);
                } else {
                    hashProofOfStake = UintToArith256(blockIndex->GetBlockHash());
                }
            }
        }
        uint32_t odds = gameView.handler(qgBet.vBetInfo, UintToArith256(mapProofOfStake[blockIndex->GetBlockHash()]));
        CAmount winningsPermille = qgBet.betAmount * odds;
        CAmount feePermille = winningsPermille > 0 ? (qgBet.betAmount * (odds - BET_ODDSDIVISOR) / 1000 * gameView.nFeePermille) : 0;
        CAmount payout = winningsPermille > 0 ? (winningsPermille - feePermille) / BET_ODDSDIVISOR : 0;

        if (payout > 0) {
            std::string DevPayoutAddr;
            std::string OMNOPayoutAddr;
            if (!GetFeePayoutAddresses(nNewBlockHeight, DevPayoutAddr, OMNOPayoutAddr)) {
                LogPrintf("Unable to find oracle, skipping payouts\n");
                continue;
            }
            qgBet.resultType = odds == BET_ODDSDIVISOR ? BetResultType::betResultRefund : BetResultType::betResultWin;
            // Add winning payout to the payouts vector.
            CPayoutInfoDB payoutInfo(qgKey, odds == BET_ODDSDIVISOR ? PayoutType::quickGamesRefund : PayoutType::quickGamesPayout);
            CBetOut betOut(payout, GetScriptForDestination(qgBet.playerAddress), qgBet.betAmount);
            vExpectedPayouts.emplace_back(betOut);
            vPayoutsInfo.emplace_back(payoutInfo);

            // Dev reward
            CAmount devReward = (CAmount)(feePermille / 1000 * gameView.nDevRewardPermille / BET_ODDSDIVISOR);
            if (mExpectedRewards.find(gameView.specialAddress) == mExpectedRewards.end())
                mExpectedRewards[gameView.specialAddress] = devReward;
            else
                mExpectedRewards[gameView.specialAddress] += devReward;

            // OMNO reward
            CAmount nOMNOReward = (CAmount)(feePermille / 1000 * gameView.nOMNORewardPermille / BET_ODDSDIVISOR);
            if (mExpectedRewards.find(OMNOPayoutAddr) == mExpectedRewards.end())
                mExpectedRewards[OMNOPayoutAddr] = nOMNOReward;
            else
                mExpectedRewards[OMNOPayoutAddr] += nOMNOReward;
        }
        else {
            qgBet.resultType = BetResultType::betResultLose;
        }
        LogPrint(BCLog::BETTING, "\nQuick game: %s, bet %s is handled!\nPlayer address: %s\nPayout: %ll\n\n", gameView.name, qgKey.outPoint.ToStringShort(), EncodeDestination(qgBet.playerAddress), payout);
        // if handling bet is completed - mark it
        qgBet.SetCompleted();
        qgBet.payout = payout;
        vEntriesToUpdate.emplace_back(std::pair<QuickGamesBetKey, CQuickGamesBetDB>{qgKey, qgBet});
    }
    // fill reward outputs
    PayoutInfoKey zeroKey{(uint32_t) nNewBlockHeight, COutPoint()};
    CPayoutInfoDB rewardInfo(zeroKey, PayoutType::quickGamesReward);
    LogPrint(BCLog::BETTING, "Quick game rewards:\n");
    for (auto& reward : mExpectedRewards) {
        LogPrint(BCLog::BETTING, "address: %s, reward: %ll\n", reward.first, reward.second);
        CBetOut rewardOut(reward.second, GetScriptForDestination(DecodeDestination(reward.first)), 0);
        vExpectedPayouts.emplace_back(rewardOut);
        vPayoutsInfo.emplace_back(rewardInfo);
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.quickGamesBets->Update(pair.first, pair.second);
    }

    LogPrint(BCLog::BETTING, "Finished generating payouts...\n");
}

void GetCGLottoBetPayoutsV3(CBettingsView &bettingsViewCache, const int nNewBlockHeight, std::vector<CBetOut>& vExpectedPayouts, std::vector<CPayoutInfoDB>& vPayoutsInfo)
{
    const int nLastBlockHeight = nNewBlockHeight - 1;

    // Get all the results posted in the prev block.
    std::vector<CChainGamesResultDB> results;

    GetCGLottoEventResults(nLastBlockHeight, results);

    std::vector<std::pair<ChainGamesBetKey, CChainGamesBetDB>> vEntriesToUpdate;

    PeerlessBetKey zeroKey{(uint32_t) nNewBlockHeight, COutPoint()};

    LogPrint(BCLog::BETTING, "Start generating chain games bets payouts...\n");

    for (auto result : results) {

        LogPrint(BCLog::BETTING, "Looking for bets of eventId: %lu\n", result.nEventId);

        CChainGamesEventDB cgEvent;
        if (!bettingsViewCache.chainGamesLottoEvents->Read(EventKey{result.nEventId}, cgEvent)) {
            LogPrintf("\n!!! Failed to find event %lu for result !!!\n", result.nEventId);
            continue;
        }

        CAmount entranceFee = static_cast<CAmount>(cgEvent.nEntryFee) * COIN;

        //reset candidate array for this event
        std::vector<std::pair<ChainGamesBetKey, CChainGamesBetDB>> candidates;

        // look bets at last 14 days
        uint32_t startHeight = GetBetSearchStartHeight(nLastBlockHeight);

        auto it = bettingsViewCache.chainGamesLottoBets->NewIterator();
        for (it->Seek(CBettingDB::DbTypeToBytes(ChainGamesBetKey{static_cast<uint32_t>(startHeight), COutPoint()})); it->Valid(); it->Next()) {
            ChainGamesBetKey cgBetKey;
            CChainGamesBetDB cgBet;
            CBettingDB::BytesToDbType(it->Key(), cgBetKey);
            CBettingDB::BytesToDbType(it->Value(), cgBet);

            if (cgBet.IsCompleted() ||
                    cgBet.nEventId != result.nEventId ||
                    cgBet.betAmount != entranceFee) {
                continue;
            }

            cgBet.SetCompleted();
            // Add a bet of each candidate to array
            LogPrint(BCLog::BETTING, "Candidate found, address: %s\n", EncodeDestination(cgBet.playerAddress).c_str());
            candidates.push_back(std::pair<ChainGamesBetKey, CChainGamesBetDB>{cgBetKey, cgBet});
        }

        // Choose winner from candidates who entered the lotto and payout their winnings.
        if (candidates.size() == 1) {
            // Refund the single entrant.
            CAmount noOfBets = candidates.size();
            CTxDestination winnerAddress = candidates[0].second.playerAddress;
            CAmount winnerPayout = entranceFee;

            LogPrint(BCLog::BETTING, "Total number of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrint(BCLog::BETTING, "Winner Address: %s \n", EncodeDestination(winnerAddress).c_str());
            LogPrint(BCLog::BETTING, " This Lotto was refunded as only one person bought a ticket.\n" );

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                vPayoutsInfo.emplace_back(candidates[0].first, PayoutType::chainGamesRefund);
                vExpectedPayouts.emplace_back(winnerPayout, GetScriptForDestination(winnerAddress), entranceFee, result.nEventId);
            }
        }
        else if (candidates.size() >= 2) {
            // Use random number to choose winner.
            auto noOfBets    = candidates.size();

            CBlockIndex *winBlockIndex = ::ChainActive()[nLastBlockHeight];
            arith_uint256 hashProofOfStake = UintToArith256(mapProofOfStake[winBlockIndex->GetBlockHash()]);
            if (hashProofOfStake == 0) hashProofOfStake = UintToArith256(winBlockIndex->GetBlockHash());
            arith_uint256 tempVal = hashProofOfStake / noOfBets;  // quotient
            tempVal = tempVal * noOfBets;
            tempVal = hashProofOfStake - tempVal;           // remainder
            uint64_t winnerNr = tempVal.GetLow64();

            // Split the pot and calculate winnings.
            CTxDestination winnerAddress = candidates[winnerNr].second.playerAddress;
            CAmount totalPot = hashProofOfStake == 0 ? 0 : (noOfBets * entranceFee);
            CAmount winnerPayout = totalPot / 10 * 8;
            candidates[winnerNr].second.payout = winnerPayout;
            CAmount fee = totalPot / 50;

            LogPrint(BCLog::BETTING, "Total number of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrint(BCLog::BETTING, "Winner Address: %s (index no %u) \n", EncodeDestination(winnerAddress).c_str(), winnerNr);
            LogPrint(BCLog::BETTING, "Total Pot: %u, Winnings: %u, Fee: %u \n", totalPot, winnerPayout, fee);

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                CScript payoutScriptDev;
                CScript payoutScriptOMNO;
                if (!GetFeePayoutScripts(nNewBlockHeight, payoutScriptDev, payoutScriptOMNO)) {
                    LogPrintf("Unable to find oracle, skipping payouts\n");
                    continue;
                }
                vPayoutsInfo.emplace_back(candidates[winnerNr].first, PayoutType::chainGamesPayout);
                vExpectedPayouts.emplace_back(winnerPayout, GetScriptForDestination(winnerAddress), entranceFee, result.nEventId);
                LogPrint(BCLog::BETTING, "Reward address: %s, reward: %ll\n", EncodeDestination(CScriptID(payoutScriptOMNO)), fee);
                vPayoutsInfo.emplace_back(zeroKey, PayoutType::chainGamesReward);
                vExpectedPayouts.emplace_back(fee, payoutScriptOMNO, 0);
            }
            vEntriesToUpdate.insert(vEntriesToUpdate.end(), candidates.begin(), candidates.end());
        }
    }

    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.chainGamesLottoBets->Update(pair.first, pair.second);
    }

    LogPrint(BCLog::BETTING, "Finished generating payouts...\n");
}

/**
 * Undo only bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoPLBetPayouts(CBettingsView &bettingsViewCache, int height)
{
    int nCurrentHeight = ::ChainActive().Height();
    // Get all the results posted in the previous block.
    std::vector<CPeerlessResultDB> results = GetPLResults(height - 1);

    LogPrintf("Start undo payouts...\n");

    for (auto result : results) {

        if (result.nResultType == ResultType::eventClosed)
            continue;

        // look bets at last 14 days
        uint32_t startHeight = GetBetSearchStartHeight(nCurrentHeight);

        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<PeerlessBetKey, CPeerlessBetDB>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(PeerlessBetKey{startHeight, COutPoint()})); it->Valid(); it->Next()) {
            PeerlessBetKey uniBetKey;
            CPeerlessBetDB uniBet;
            CBettingDB::BytesToDbType(it->Key(), uniBetKey);
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            // skip if bet is uncompleted
            if (!uniBet.IsCompleted()) continue;

            bool needUndo = false;

            // parlay bet
            if (uniBet.legs.size() > 1) {
                bool resultFound = false;
                for (auto leg : uniBet.legs) {
                    // if we found one result for parlay - check each other legs
                    if (leg.nEventId == result.nEventId) {
                        resultFound = true;
                    }
                }
                if (resultFound) {
                    // make assumption that parlay is handled
                    needUndo = true;
                    // find all results for all legs
                    for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                        CPeerlessLegDB &leg = uniBet.legs[idx];
                        // skip this bet if incompleted (can't find one result)
                        CPeerlessResultDB res;
                        if (!bettingsViewCache.results->Read(ResultKey{leg.nEventId}, res)) {
                            needUndo = false;
                        }
                    }
                }
            }
            // single bet
            else if (uniBet.legs.size() == 1) {
                CPeerlessLegDB &singleBet = uniBet.legs[0];
                if (singleBet.nEventId == result.nEventId) {
                    needUndo = true;
                }
            }

            if (needUndo) {
                uniBet.SetUncompleted();
                uniBet.resultType = BetResultType::betResultUnknown;
                uniBet.payout = 0;
                vEntriesToUpdate.emplace_back(std::pair<PeerlessBetKey, CPeerlessBetDB>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.bets->Update(pair.first, pair.second);
        }
    }
    return true;
}

/**
 * Undo only quick games bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoQGBetPayouts(CBettingsView &bettingsViewCache, int height)
{
    uint32_t blockHeight = static_cast<uint32_t>(height);

    LogPrintf("Start undo quick games payouts...\n");

    auto it = bettingsViewCache.quickGamesBets->NewIterator();
    std::vector<std::pair<QuickGamesBetKey, CQuickGamesBetDB>> vEntriesToUpdate;
    for (it->Seek(CBettingDB::DbTypeToBytes(QuickGamesBetKey{blockHeight, COutPoint()})); it->Valid(); it->Next()) {
        QuickGamesBetKey qgBetKey;
        CQuickGamesBetDB qgBet;
        CBettingDB::BytesToDbType(it->Key(), qgBetKey);
        CBettingDB::BytesToDbType(it->Value(), qgBet);
        // skip if bet is uncompleted
        if (!qgBet.IsCompleted()) continue;

        qgBet.SetUncompleted();
        qgBet.resultType = BetResultType::betResultUnknown;
        qgBet.payout = 0;
        vEntriesToUpdate.emplace_back(std::pair<QuickGamesBetKey, CQuickGamesBetDB>{qgBetKey, qgBet});
    }
    for (auto pair : vEntriesToUpdate) {
        bettingsViewCache.quickGamesBets->Update(pair.first, pair.second);
    }
    return true;
}