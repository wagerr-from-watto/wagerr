// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <amount.h>
#include <uint256.h>
#include <map>
#include <string>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
    /** The number of past blocks (including the block under consideration) to be taken into account for locking in a fork. */
    int64_t nWindowSize{0};
    /** A starting number of blocks, in the range of 1..nWindowSize, which must signal for a fork in order to lock it in. */
    int64_t nThresholdStart{0};
    /** A minimum number of blocks, in the range of 1..nWindowSize, which must signal for a fork in order to lock it in. */
    int64_t nThresholdMin{0};
    /** A coefficient which adjusts the speed a required number of signaling blocks is decreasing from nThresholdStart to nThresholdMin at with each period. */
    int64_t nFalloffCoeff{0};
};

enum LLMQType : uint8_t
{
    LLMQ_NONE = 0xff,

    LLMQ_20_60 = 1, // 50 members, 30 (60%) threshold, one per hour
    LLMQ_40_60 = 2, // 400 members, 240 (60%) threshold, one every 12 hours
    LLMQ_40_85 = 3, // 400 members, 340 (85%) threshold, one every 24 hours
    LLMQ_20_70 = 4, // 100 members, 67 (67%) threshold, one per hour

    // for testing only
    LLMQ_TEST = 100, // 3 members, 2 (66%) threshold, one per hour. Params might differ when -llmqtestparams is used

    // for devnets only
    LLMQ_DEVNET = 101, // 10 members, 6 (60%) threshold, one per hour. Params might differ when -llmqdevnetparams is used

    // for testing activation of new quorums only
    LLMQ_TEST_V17 = 102, // 3 members, 2 (66%) threshold, one per hour. Params might differ when -llmqtestparams is used
};

// Configures a LLMQ and its DKG
// See https://github.com/wagerr/dips/blob/master/dip-0006.md for more details
struct LLMQParams {
    LLMQType type;

    // not consensus critical, only used in logging, RPC and UI
    std::string name;

    // the size of the quorum, e.g. 50 or 400
    int size;

    // The minimum number of valid members after the DKK. If less members are determined valid, no commitment can be
    // created. Should be higher then the threshold to allow some room for failing nodes, otherwise quorum might end up
    // not being able to ever created a recovered signature if more nodes fail after the DKG
    int minSize;

    // The threshold required to recover a final signature. Should be at least 50%+1 of the quorum size. This value
    // also controls the size of the public key verification vector and has a large influence on the performance of
    // recovery. It also influences the amount of minimum messages that need to be exchanged for a single signing session.
    // This value has the most influence on the security of the quorum. The number of total malicious masternodes
    // required to negatively influence signing sessions highly correlates to the threshold percentage.
    int threshold;

    // The interval in number blocks for DKGs and the creation of LLMQs. If set to 24 for example, a DKG will start
    // every 24 blocks, which is approximately once every hour.
    int dkgInterval;

    // The number of blocks per phase in a DKG session. There are 6 phases plus the mining phase that need to be processed
    // per DKG. Set this value to a number of blocks so that each phase has enough time to propagate all required
    // messages to all members before the next phase starts. If blocks are produced too fast, whole DKG sessions will
    // fail.
    int dkgPhaseBlocks;

    // The starting block inside the DKG interval for when mining of commitments starts. The value is inclusive.
    // Starting from this block, the inclusion of (possibly null) commitments is enforced until the first non-null
    // commitment is mined. The chosen value should be at least 5 * dkgPhaseBlocks so that it starts right after the
    // finalization phase.
    int dkgMiningWindowStart;

    // The ending block inside the DKG interval for when mining of commitments ends. The value is inclusive.
    // Choose a value so that miners have enough time to receive the commitment and mine it. Also take into consideration
    // that miners might omit real commitments and revert to always including null commitments. The mining window should
    // be large enough so that other miners have a chance to produce a block containing a non-null commitment. The window
    // should at the same time not be too large so that not too much space is wasted with null commitments in case a DKG
    // session failed.
    int dkgMiningWindowEnd;

    // In the complaint phase, members will vote on other members being bad (missing valid contribution). If at least
    // dkgBadVotesThreshold have voted for another member to be bad, it will considered to be bad by all other members
    // as well. This serves as a protection against late-comers who send their contribution on the bring of
    // phase-transition, which would otherwise result in inconsistent views of the valid members set
    int dkgBadVotesThreshold;

    // Number of quorums to consider "active" for signing sessions
    int signingActiveQuorumCount;

    // Used for intra-quorum communication. This is the number of quorums for which we should keep old connections. This
    // should be at least one more then the active quorums set.
    int keepOldConnections;

    // How many members should we try to send all sigShares to before we give up.
    int recoveryMembers;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    uint256 hashDevnetGenesisBlock;
    int nSubsidyHalvingInterval;
    int nMasternodePaymentsStartBlock;
    int nMasternodePaymentsIncreaseBlock;
    int nMasternodePaymentsIncreasePeriod; // in blocks
    int nInstantSendConfirmationsRequired; // in blocks
    int nInstantSendKeepLock; // in blocks
    int nBudgetPaymentsStartBlock;
    int nBudgetPaymentsCycleBlocks;
    int nBudgetPaymentsWindowBlocks;
    int nSuperblockStartBlock;
    uint256 nSuperblockStartHash;
    int nSuperblockCycle; // in blocks
    int nGovernanceMinQuorum; // Min absolute vote count to trigger an action
    int nGovernanceFilterElements;
    int nMasternodeMinimumConfirmations;
    /** Deployment of v17 Hard Fork */
    int V17DeploymentHeight;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which BIP68, BIP112, and BIP113 become active */
    int CSVHeight;
    /** Block height at which BIP147 becomes active */
    int BIP147Height;
    /** Block height at which DIP0001 becomes active */
    int DIP0001Height;
    /** Block height at which DIP0003 becomes active */
    int DIP0003Height;
    /** Block height at which DIP0003 becomes enforced */
//    int DIP0003EnforcementHeight;
    uint256 DIP0003EnforcementHash;
    /** Block height at which DIP0008 becomes active */
    int DIP0008Height;

    /**
     * Minimum blocks including miner confirmation of the total of nMinerConfirmationWindow blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Default BIP9Deployment::nThresholdStart value for deployments where it's not specified and for unknown deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    // Default BIP9Deployment::nWindowSize value for deployments where it's not specified and for unknown deployments.
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    /** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
    uint16_t nMaturityV1;
    uint16_t nMaturityV2;
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;

    /** Wagerr specific deployment heights */
    int nWagerrProtocolV1StartHeight;
    int nWagerrProtocolV2StartHeight;
    int nWagerrProtocolV3StartHeight;
    int nWagerrProtocolV4StartHeight;
    int nQuickGamesEndHeight;
    int nMaturityV2StartHeight;
    int nKeysRotateHeight;
    int nPosStartHeight;
    int nBlockStakeModifierV1A;
    int nBlockStakeModifierV2;
    int nBlockTimeProtocolV2;
    int ATPStartHeight;

    /** Proof of stake parameters */
    uint256 posLimit;
    uint256 posLimit_V2;
    int64_t nPosTargetSpacing;
    int64_t nPosTargetTimespan;
    int64_t nPosTargetTimespan_V2;
    int32_t nStakeMinDepth;
    int32_t nStakeMinAge;

    /** Time Protocol V2 **/
    int nTimeSlotLength;

    /** ATP parameters */
    std::string WagerrAddrPrefix;
    std::string strTokenManagementKey;
    int nOpGroupNewRequiredConfirmations;

    /** Zerocoin - retired functionality */
    int64_t nZerocoinStartHeight;
    int64_t nZerocoinStartTime;
    int64_t nBlockZerocoinV2;
    int64_t nPublicZCSpends;
    std::string zerocoinModulus;
    int64_t nFakeSerialBlockheightEnd;
    CAmount nSupplyBeforeFakeSerial;
    int32_t nZerocoinRequiredStakeDepth;
    int nMintRequiredConfirmations;
    int nRequiredAccumulation;

    /** Betting */
    int nBetBlocksIndexTimespanV2;
    int nBetBlocksIndexTimespanV3;
    uint64_t nOMNORewardPermille;
    uint64_t nDevRewardPermille;
    uint64_t nBetBlockPayoutAmount;
    int64_t nMinBetPayoutRange;
    int64_t nMaxBetPayoutRange;
    int64_t nMaxParlayBetPayoutRange;
    int nBetPlaceTimeoutBlocks;
    uint32_t nMaxParlayLegs;

    /** these parameters are only used on devnet and can be configured from the outside */
    int nMinimumDifficultyBlocks{0};
    int nHighSubsidyBlocks{0};
    int nHighSubsidyFactor{1};

    std::map<LLMQType, LLMQParams> llmqs;
    LLMQType llmqTypeChainLocks;
    LLMQType llmqTypeInstantSend{LLMQ_NONE};
    LLMQType llmqTypePlatform{LLMQ_NONE};

    bool IsStakeModifierV2(const int64_t nHeight) const { return nHeight >= nBlockStakeModifierV2; }
    bool IsTimeProtocolV2(const int64_t nHeight) const { return nHeight >= nBlockTimeProtocolV2; }
    int COINBASE_MATURITY(const int contextHeight) const {
        return contextHeight < nMaturityV2StartHeight ? nMaturityV1 : nMaturityV2;
    }
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }

    int BetBlocksIndexTimespanV2() const { return nBetBlocksIndexTimespanV2; }
    int BetBlocksIndexTimespanV3() const { return nBetBlocksIndexTimespanV3; }
    uint64_t OMNORewardPermille() const { return nOMNORewardPermille; }
    uint64_t DevRewardPermille() const { return nDevRewardPermille; }
    int BetBlockPayoutAmount() const { return nBetBlockPayoutAmount; } // Currently not used
    int64_t MaxBetPayoutRange() const { return nMaxBetPayoutRange; }
    int64_t MinBetPayoutRange() const { return nMinBetPayoutRange; }
    int64_t MaxParlayBetPayoutRange() const { return nMaxBetPayoutRange; }
    int BetPlaceTimeoutBlocks() const { return nBetPlaceTimeoutBlocks; }
    uint32_t MaxParlayLegs() const { return nMaxParlayLegs; }
    int WagerrProtocolV1StartHeight() const { return nWagerrProtocolV1StartHeight; }
    int WagerrProtocolV2StartHeight() const { return nWagerrProtocolV2StartHeight; }
    int WagerrProtocolV3StartHeight() const { return nWagerrProtocolV3StartHeight; }
    int WagerrProtocolV4StartHeight() const { return nWagerrProtocolV4StartHeight; }
    int QuickGamesEndHeight() const { return nQuickGamesEndHeight; }
};
} // namespace Consensus

// This must be outside of all namespaces. We must also duplicate the forward declaration of is_serializable_enum to
// avoid inclusion of serialize.h here.
template<typename T> struct is_serializable_enum;
template<> struct is_serializable_enum<Consensus::LLMQType> : std::true_type {};

#endif // BITCOIN_CONSENSUS_PARAMS_H
