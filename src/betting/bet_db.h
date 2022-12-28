// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_DB_H
#define WAGERR_BET_DB_H

#include <chainparams.h>
#include <dstencode.h>
#include <util.h>
#include <script/standard.h>
#include <serialize.h>
#include <betting/bet_common.h>
#include <betting/bet_tx.h>
#include <flushablestorage/flushablestorage.h>
#include <boost/filesystem.hpp>
#include <boost/variant.hpp>
#include <boost/exception/to_string.hpp>

/*
 * Peerless betting database structures
 */

// MappingKey
typedef struct MappingKey {
    MappingType nMType;
    uint32_t nId;

    MappingKey() {}
    MappingKey(MappingType type, uint32_t id) : nMType(type), nId(id) {}

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        uint32_t be_val;
        be_val = htonl((uint32_t)nMType);
        ser_writedata32(s, be_val);
        be_val = htonl(nId);
        ser_writedata32(s, be_val);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        uint32_t be_val;
        be_val = ser_readdata32(s);
        nMType = (MappingType) ntohl(be_val);
        be_val = ser_readdata32(s);
        nId = ntohl(be_val);
    }

} MappingKey;

class CMappingDB
{
public:

    std::string sName;

    explicit CMappingDB() {}
    explicit CMappingDB(std::string& name) : sName(name) {}

    static std::string ToTypeName(MappingType type);
    static MappingType FromTypeName(const std::string& name);

    SERIALIZE_METHODS(CMappingDB, obj)
    {
        READWRITE(obj.sName);
    }
};

// EventKey
typedef struct EventKey {
    uint32_t eventId;

    explicit EventKey(uint32_t id) : eventId(id) { }
    explicit EventKey(const EventKey& key) : eventId(key.eventId) { }
    explicit EventKey(EventKey&& key) : eventId(key.eventId) { }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        uint32_t be_val;
        be_val = htonl(eventId);
        ser_writedata32(s, be_val);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        uint32_t be_val;
        be_val = ser_readdata32(s);
        eventId = ntohl(be_val);
    }

} EventKey;

// class using for saving short event info inside universal bet model
class CPeerlessBaseEventDB
{
public:
    uint32_t nEventId = 0;
    uint64_t nStartTime = 0;
    uint32_t nSport = 0;
    uint32_t nTournament = 0;
    uint32_t nStage = 0;
    uint32_t nHomeTeam = 0;
    uint32_t nAwayTeam = 0;
    uint32_t nHomeOdds = 0;
    uint32_t nAwayOdds = 0;
    uint32_t nDrawOdds = 0;
    int32_t  nSpreadPoints = 0;
    uint32_t nSpreadHomeOdds = 0;
    uint32_t nSpreadAwayOdds = 0;
    uint32_t nTotalPoints = 0;
    uint32_t nTotalOverOdds = 0;
    uint32_t nTotalUnderOdds = 0;

    // Used in version 1 events
    int nEventCreationHeight = 0;
    bool fLegacyInitialHomeFavorite = true;

    // Default Constructor.
    explicit CPeerlessBaseEventDB() {}

    inline void ExtractDataFromTx(const CPeerlessEventTx& eventTx) {
        nEventId = eventTx.nEventId;
        nStartTime = eventTx.nStartTime;
        nSport = eventTx.nSport;
        nTournament = eventTx.nTournament;
        nStage = eventTx.nStage;
        nHomeTeam = eventTx.nHomeTeam;
        nAwayTeam = eventTx.nAwayTeam;
        nHomeOdds = eventTx.nHomeOdds;
        nAwayOdds = eventTx.nAwayOdds;
        nDrawOdds = eventTx.nDrawOdds;
    }

    inline void ExtractDataFromTx(const CPeerlessUpdateOddsTx& updateOddsTx) {
        nHomeOdds = updateOddsTx.nHomeOdds;
        nAwayOdds = updateOddsTx.nAwayOdds;
        nDrawOdds = updateOddsTx.nDrawOdds;
    }

    inline void ExtractDataFromTx(const CPeerlessSpreadsEventTx& spreadsEventTx) {
        nSpreadPoints = spreadsEventTx.nPoints;
        nSpreadHomeOdds = spreadsEventTx.nHomeOdds;
        nSpreadAwayOdds = spreadsEventTx.nAwayOdds;
    }

    inline void ExtractDataFromTx(const CPeerlessTotalsEventTx& totalsEventTx) {
        nTotalPoints = totalsEventTx.nPoints;
        nTotalOverOdds = totalsEventTx.nOverOdds;
        nTotalUnderOdds = totalsEventTx.nUnderOdds;
    }

    inline void ExtractDataFromTx(const CPeerlessEventPatchTx& eventPatchTx) {
        nStartTime = eventPatchTx.nStartTime;
    }

    SERIALIZE_METHODS(CPeerlessBaseEventDB, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nStartTime);
        READWRITE(obj.nSport);
        READWRITE(obj.nTournament);
        READWRITE(obj.nStage);
        READWRITE(obj.nHomeTeam);
        READWRITE(obj.nAwayTeam);
        READWRITE(obj.nHomeOdds);
        READWRITE(obj.nAwayOdds);
        READWRITE(obj.nDrawOdds);
        READWRITE(obj.nSpreadPoints);
        READWRITE(obj.nSpreadHomeOdds);
        READWRITE(obj.nSpreadAwayOdds);
        READWRITE(obj.nTotalPoints);
        READWRITE(obj.nTotalOverOdds);
        READWRITE(obj.nTotalUnderOdds);

        READWRITE(obj.nEventCreationHeight);
        if (obj.nEventCreationHeight < Params().GetConsensus().WagerrProtocolV3StartHeight()) {
            READWRITE(obj.fLegacyInitialHomeFavorite);
        }
    }
};

// class using for saving full event info in DB
class CPeerlessExtendedEventDB : public CPeerlessBaseEventDB
{
public:
    uint32_t nMoneyLineHomePotentialLiability = 0;
    uint32_t nMoneyLineAwayPotentialLiability = 0;
    uint32_t nMoneyLineDrawPotentialLiability = 0;
    uint32_t nSpreadHomePotentialLiability = 0;
    uint32_t nSpreadAwayPotentialLiability = 0;
    uint32_t nSpreadPushPotentialLiability = 0;
    uint32_t nTotalOverPotentialLiability = 0;
    uint32_t nTotalUnderPotentialLiability = 0;
    uint32_t nTotalPushPotentialLiability = 0;
    uint32_t nMoneyLineHomeBets = 0;
    uint32_t nMoneyLineAwayBets = 0;
    uint32_t nMoneyLineDrawBets = 0;
    uint32_t nSpreadHomeBets = 0;
    uint32_t nSpreadAwayBets = 0;
    uint32_t nSpreadPushBets = 0;
    uint32_t nTotalOverBets = 0;
    uint32_t nTotalUnderBets = 0;
    uint32_t nTotalPushBets = 0;

    // Default Constructor.
    explicit CPeerlessExtendedEventDB() : CPeerlessBaseEventDB() {}

    SERIALIZE_METHODS(CPeerlessExtendedEventDB, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nStartTime);
        READWRITE(obj.nSport);
        READWRITE(obj.nTournament);
        READWRITE(obj.nStage);
        READWRITE(obj.nHomeTeam);
        READWRITE(obj.nAwayTeam);
        READWRITE(obj.nHomeOdds);
        READWRITE(obj.nAwayOdds);
        READWRITE(obj.nDrawOdds);
        READWRITE(obj.nSpreadPoints);
        READWRITE(obj.nSpreadHomeOdds);
        READWRITE(obj.nSpreadAwayOdds);
        READWRITE(obj.nTotalPoints);
        READWRITE(obj.nTotalOverOdds);
        READWRITE(obj.nTotalUnderOdds);
        READWRITE(obj.nMoneyLineHomePotentialLiability);
        READWRITE(obj.nMoneyLineAwayPotentialLiability);
        READWRITE(obj.nMoneyLineDrawPotentialLiability);
        READWRITE(obj.nSpreadHomePotentialLiability);
        READWRITE(obj.nSpreadAwayPotentialLiability);
        READWRITE(obj.nSpreadPushPotentialLiability);
        READWRITE(obj.nTotalOverPotentialLiability);
        READWRITE(obj.nTotalUnderPotentialLiability);
        READWRITE(obj.nTotalPushPotentialLiability);
        READWRITE(obj.nMoneyLineHomeBets);
        READWRITE(obj.nMoneyLineAwayBets);
        READWRITE(obj.nMoneyLineDrawBets);
        READWRITE(obj.nSpreadHomeBets);
        READWRITE(obj.nSpreadAwayBets);
        READWRITE(obj.nSpreadPushBets);
        READWRITE(obj.nTotalOverBets);
        READWRITE(obj.nTotalUnderBets);
        READWRITE(obj.nTotalPushBets);

        READWRITE(obj.nEventCreationHeight);
        if (obj.nEventCreationHeight < Params().GetConsensus().WagerrProtocolV3StartHeight()) {
            READWRITE(obj.fLegacyInitialHomeFavorite);
        }
    }
};

// ResultKey
using ResultKey = EventKey;

class CPeerlessResultDB
{
public:
    uint32_t nEventId;
    uint32_t nResultType;
    uint32_t nHomeScore;
    uint32_t nAwayScore;

    // Default Constructor.
    explicit CPeerlessResultDB() {}

    // Parametrized Constructor.
    explicit CPeerlessResultDB(uint32_t eventId, uint32_t resultType, uint32_t homeScore, uint32_t awayScore) :
        nEventId(eventId), nResultType(resultType), nHomeScore(homeScore), nAwayScore(awayScore) {}

    SERIALIZE_METHODS(CPeerlessResultDB, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nResultType);
        READWRITE(obj.nHomeScore);
        READWRITE(obj.nAwayScore);
    }
};

// Field event key (individual sport)
using FieldEventKey = EventKey;

typedef struct ContenderInfo {

    uint32_t nInputOdds = 0;

    uint32_t nOutrightOdds = 0;
    uint32_t nOutrightBets = 0;
    uint32_t nOutrightPotentialLiability = 0;

    uint32_t nPlaceOdds = 0;
    uint32_t nPlaceBets = 0;
    uint32_t nPlacePotentialLiability = 0;

    uint32_t nShowOdds = 0;
    uint32_t nShowBets = 0;
    uint32_t nShowPotentialLiability = 0;

    uint32_t nModifier = 0;

    explicit ContenderInfo() {}
    explicit ContenderInfo(const uint32_t inputOdds, const uint32_t outrightOdds, const uint32_t placeOdds, const uint32_t showOdds, const uint32_t modifier)
        : nInputOdds(inputOdds)
        , nOutrightOdds(outrightOdds)
        , nOutrightBets(0)
        , nOutrightPotentialLiability(0)
        , nPlaceOdds(placeOdds)
        , nPlaceBets(0)
        , nPlacePotentialLiability(0)
        , nShowOdds(showOdds)
        , nShowBets(0)
        , nShowPotentialLiability(0)
        , nModifier(modifier)
    {}

    SERIALIZE_METHODS(ContenderInfo, obj)
    {
        READWRITE(obj.nInputOdds);

        READWRITE(obj.nOutrightOdds);
        READWRITE(obj.nOutrightBets);
        READWRITE(obj.nOutrightPotentialLiability);

        READWRITE(obj.nPlaceOdds);
        READWRITE(obj.nPlaceBets);
        READWRITE(obj.nPlacePotentialLiability);

        READWRITE(obj.nShowOdds);
        READWRITE(obj.nShowBets);
        READWRITE(obj.nShowPotentialLiability);

        READWRITE(obj.nModifier);
    }
} ContenderInfo;

class CFieldEventDB
{
public:
    uint32_t nEventId      = 0;
    uint64_t nStartTime    = 0;
    uint8_t nGroupType    = 0;
    uint8_t nMarketType   = 0;
    uint32_t nSport        = 0;
    uint32_t nTournament = 0;
    uint32_t nStage      = 0;
    uint32_t nMarginPercent = 0;
    // contenderId : ContenderInfo
    std::map<uint32_t, ContenderInfo> contenders;

    // Default Constructor.
    explicit CFieldEventDB() {}

    void ExtractDataFromTx(const CFieldEventTx& tx);
    void ExtractDataFromTx(const CFieldUpdateOddsTx& tx);
    void ExtractDataFromTx(const CFieldUpdateMarginTx& tx);
    void ExtractDataFromTx(const CFieldUpdateModifiersTx& tx);

    void CalcOdds();
    uint32_t NoneZeroOddsContendersCount();
    bool IsMarketOpen(const FieldBetOutcomeType type);

    std::string ToString();
    std::string ContendersToString();

    SERIALIZE_METHODS(CFieldEventDB, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nStartTime);
        READWRITE(obj.nGroupType);
        READWRITE(obj.nMarketType);
        READWRITE(obj.nSport);
        READWRITE(obj.nTournament);
        READWRITE(obj.nStage);
        READWRITE(obj.nMarginPercent);
        READWRITE(obj.contenders);
    }

private:
    double GetLambda(const uint32_t ContendersSize);
    double GetRHO(const uint32_t ContendersSize);

    void Permutations2(const std::map<uint32_t, uint32_t>& mContendersOdds, std::vector<std::vector<uint32_t>>& perms);
    void Permutations3(const std::map<uint32_t, uint32_t>& mContendersOdds, std::vector<std::vector<uint32_t>>& perms);

    void CalculateFairOdds(std::map<uint32_t, uint32_t>& mContendersFairOdds);
    void CalculateOutrightOdds(const std::map<uint32_t, uint32_t>& mContendersFairOdds, std::map<uint32_t, uint32_t>& mContendersOutrightOdds);
    uint32_t CalculateAnimalPlaceOdds(const uint32_t idx, const double lambda, const std::map<uint32_t, uint32_t>& mContendersFairOdds);
    uint32_t CalculateAnimalShowOdds(const uint32_t idx, const double lambda, const double rho, const std::map<uint32_t, uint32_t>& mContendersFairOdds);
    uint32_t CalculateOddsInFirstN(const uint32_t idx, const std::vector<std::vector<uint32_t>>& permutations, const std::map<uint32_t, uint32_t>& mContendersFairOdds);

    double CalculateX(const std::vector<std::pair<uint32_t, uint32_t>>& vContendersOddsMods, const double realMarginIn);
    double CalculateM(const std::vector<std::pair<uint32_t, uint32_t>>& vContendersOddsMods, const double realMarginIn);

    uint32_t CalculateMarketOdds(const double X, const double m, const uint32_t oddsMods, const uint16_t modifier);
};

// Field event Result key
using FieldResultKey = EventKey;

class CFieldResultDB
{
public:
    uint32_t nEventId;
    uint8_t nResultType;
    // contenderId : ContenderResult
    std::map<uint32_t, uint8_t> contendersResults;

    // Default Constructor.
    explicit CFieldResultDB() {}
    explicit CFieldResultDB(const uint32_t eventId, const uint8_t resultType)
        : nEventId(eventId)
        , nResultType(resultType)
    {}
    explicit CFieldResultDB(const uint32_t eventId, const uint8_t resultType, const std::map<uint32_t, uint8_t> mContendersResults)
        : nEventId(eventId)
        , nResultType(resultType)
        , contendersResults(mContendersResults)
    {}

    SERIALIZE_METHODS(CFieldResultDB, obj){
        READWRITE(obj.nEventId);
        READWRITE(obj.nResultType);
        READWRITE(obj.contendersResults);
    }
};

// PeerlessBetKey
typedef struct PeerlessBetKey {
    uint32_t blockHeight;
    COutPoint outPoint;

    explicit PeerlessBetKey() : blockHeight(0), outPoint(COutPoint()) { }
    explicit PeerlessBetKey(uint32_t height, COutPoint out) : blockHeight(height), outPoint(out) { }
    explicit PeerlessBetKey(const PeerlessBetKey& betKey) : blockHeight{betKey.blockHeight}, outPoint{betKey.outPoint} { }

    bool operator==(const PeerlessBetKey& rhs) const
    {
        return blockHeight == rhs.blockHeight && outPoint == rhs.outPoint;
    }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        uint32_t be_val;
        be_val = htonl(blockHeight);
        ser_writedata32(s, be_val);
        
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        uint32_t be_val;
        be_val = ser_readdata32(s);
        blockHeight = ntohl(be_val);
    }

} PeerlessBetKey;

class CPeerlessLegDB
{
public:
    uint32_t nEventId;
    OutcomeType nOutcome;

    // Default constructor.
    explicit CPeerlessLegDB() {}
    explicit CPeerlessLegDB(uint32_t eventId, OutcomeType outcome) : nEventId(eventId), nOutcome(outcome) {}

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        uint8_t outcome;
        outcome = (uint8_t) nOutcome;
        ser_writedata8(s, outcome);
        
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        uint8_t outcome;
        outcome = ser_readdata8(s);
        nOutcome = (OutcomeType) outcome;
    }

};

// Class for serializing bets in DB
class CPeerlessBetDB
{
public:
    CAmount betAmount;
    CTxDestination playerAddress;
    // one elem means single bet, else it is parlay bet, max size = 5
    std::vector<CPeerlessLegDB> legs;
    // vector for member event condition
    std::vector<CPeerlessBaseEventDB> lockedEvents;
    int64_t betTime;
    BetResultType resultType = BetResultType::betResultUnknown;
    CAmount payout = 0;
    uint32_t payoutHeight = 0;

    explicit CPeerlessBetDB() { }
    explicit CPeerlessBetDB(const CAmount amount, const CTxDestination address, const std::vector<CPeerlessLegDB> vLegs, const std::vector<CPeerlessBaseEventDB> vEvents, const int64_t time) :
        betAmount(amount), playerAddress(address), legs(vLegs), lockedEvents(vEvents), betTime(time) { }
    explicit CPeerlessBetDB(const CPeerlessBetDB& bet)
    {
        betAmount = bet.betAmount;
        playerAddress = bet.playerAddress;
        legs = bet.legs;
        lockedEvents = bet.lockedEvents;
        betTime = bet.betTime;
        completed = bet.completed;
        resultType = bet.resultType;
        payout = bet.payout;
        payoutHeight = bet.payoutHeight;
    }

    bool IsCompleted() const { return completed; }
    void SetCompleted() { completed = true; }
    // for undo
    void SetUncompleted() { completed = false; }

    SERIALIZE_METHODS(CPeerlessBetDB, obj)
    {
        std::string address_str;
        uint8_t res_type;

        SER_WRITE(obj, address_str = EncodeDestination(obj.playerAddress));
        SER_WRITE(obj, res_type = (uint8_t) obj.resultType);

        READWRITE(obj.betAmount, address_str, obj.legs, obj.lockedEvents, obj.betTime, obj.completed, res_type, obj.payout, obj.payoutHeight);

        SER_READ(obj, obj.playerAddress = DecodeDestination(address_str));
        SER_READ(obj, obj.resultType = (BetResultType) res_type);
    }

private:
    bool completed = false;
};

// Field Bet Key
using FieldBetKey = PeerlessBetKey;

class CFieldLegDB
{
public:
    uint32_t nEventId;
    FieldBetOutcomeType nOutcome;
    uint32_t nContenderId;

    // Default constructor.
    explicit CFieldLegDB() {}
    explicit CFieldLegDB(const uint32_t eventId, const FieldBetOutcomeType outcome, const uint32_t contenderId)
        : nEventId(eventId)
        , nOutcome(outcome)
        , nContenderId(contenderId)
    {}

    SERIALIZE_METHODS(CFieldLegDB, obj)
    {
        uint8_t market;

        SER_WRITE(obj, market = (uint8_t) obj.nOutcome);

        READWRITE(obj.nEventId, market, obj.nContenderId);

        SER_READ(obj, obj.nOutcome = (FieldBetOutcomeType) market);
    }
};

class CFieldBetDB
{
public:
    CAmount betAmount;
    CTxDestination playerAddress;
    // one elem means single bet, else it is parlay bet, max size = 5
    std::vector<CFieldLegDB> legs;
    // vector for member event condition, max size = 5
    std::vector<CFieldEventDB> lockedEvents;
    int64_t betTime;
    BetResultType resultType = BetResultType::betResultUnknown;
    CAmount payout = 0;
    uint32_t payoutHeight = 0;

    // Default Constructor.
    explicit CFieldBetDB() {}
    explicit CFieldBetDB(const CAmount amount,
                         const CTxDestination address,
                         const std::vector<CFieldLegDB> vLegs,
                         const std::vector<CFieldEventDB> vEvents,
                         const int64_t time
    )   : betAmount(amount)
        , playerAddress(address)
        , legs(vLegs)
        , lockedEvents(vEvents)
        , betTime(time)
    {}

    bool IsCompleted() const { return completed; }
    void SetCompleted() { completed = true; }
    // for undo
    void SetUncompleted() { completed = false; }

    SERIALIZE_METHODS(CFieldBetDB, obj)
    {
        std::string address_str;
        uint8_t res_type;

        SER_WRITE(obj, address_str = EncodeDestination(obj.playerAddress));
        SER_WRITE(obj, res_type = (uint8_t) obj.resultType);

        READWRITE(obj.betAmount, address_str, obj.legs, obj.lockedEvents, obj.betTime, obj.completed, res_type, obj.payout, obj.payoutHeight);

        SER_READ(obj, obj.playerAddress = DecodeDestination(address_str));
        SER_READ(obj, obj.resultType = (BetResultType) res_type);
    }

private:
    bool completed = false;
};

/*
 * Chain Games database structures
 */

class CChainGamesEventDB
{
public:
    uint32_t nEventId;
    uint32_t nEntryFee;

    // Default Constructor.
    explicit CChainGamesEventDB() {}
    explicit CChainGamesEventDB(uint32_t eventId, uint32_t entryFee) : nEventId(eventId), nEntryFee(entryFee) {}

    SERIALIZE_METHODS(CChainGamesEventDB, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nEntryFee);
    }
};

using ChainGamesBetKey = PeerlessBetKey;

class CChainGamesBetDB
{
public:
    uint32_t nEventId;
    CAmount betAmount;
    CTxDestination playerAddress;
    int64_t betTime;
    CAmount payout = 0;
    uint32_t payoutHeight = 0;

    // Default Constructor.
    explicit CChainGamesBetDB() {}

    // Parametrized Constructor.
    explicit CChainGamesBetDB(uint32_t eventId, CAmount amount, CTxDestination address, int64_t time) :
        nEventId(eventId), betAmount(amount), playerAddress(address), betTime(time) { }

    SERIALIZE_METHODS(CChainGamesBetDB, obj)
    {
        std::string address_str;

        SER_WRITE(obj, address_str = EncodeDestination(obj.playerAddress));

        READWRITE(obj.nEventId, obj.completed, obj.betAmount, address_str, obj.betTime, obj.payout, obj.payoutHeight);

        SER_READ(obj, obj.playerAddress = DecodeDestination(address_str));
    }

    bool IsCompleted() const { return completed; }
    void SetCompleted() { completed = true; }
    // for undo
    void SetUncompleted() { completed = false; }

private:
    bool completed = false;
};

class CChainGamesResultDB
{
public:
    uint16_t nEventId;

    // Default Constructor.
    explicit CChainGamesResultDB() {}

    explicit CChainGamesResultDB(uint16_t nEventId) : nEventId(nEventId) {};

    SERIALIZE_METHODS(CChainGamesResultDB, obj)
    {
        READWRITE(obj.nEventId);
    }
};

/*
 * Quick games database structures
 */

using QuickGamesBetKey = PeerlessBetKey;

class CQuickGamesBetDB
{
public:
    QuickGamesType gameType;
    std::vector<unsigned char> vBetInfo;
    CAmount betAmount;
    CTxDestination playerAddress;
    int64_t betTime;
    BetResultType resultType = BetResultType::betResultUnknown;
    CAmount payout = 0;

    explicit CQuickGamesBetDB() { }
    explicit CQuickGamesBetDB(const QuickGamesType gameType, const std::vector<unsigned char>& vBetInfo, const CAmount betAmount, const CTxDestination& playerAddress, const int64_t betTime) :
        gameType(gameType), vBetInfo(vBetInfo), betAmount(betAmount), playerAddress(playerAddress), betTime(betTime) { }
    explicit CQuickGamesBetDB(const CQuickGamesBetDB& cgBet) :
        gameType(cgBet.gameType), vBetInfo(cgBet.vBetInfo), betAmount(cgBet.betAmount), playerAddress(cgBet.playerAddress), betTime(cgBet.betTime), resultType(cgBet.resultType), payout(cgBet.payout), completed(cgBet.completed) { }

    bool IsCompleted() { return completed; }
    void SetCompleted() { completed = true; }
    // for undo
    void SetUncompleted() { completed = false; }

    SERIALIZE_METHODS(CQuickGamesBetDB, obj){
        std::string address_str;
        uint8_t game_type, res_type;

        SER_WRITE(obj, game_type = (uint8_t) obj.gameType);
        SER_WRITE(obj, address_str = EncodeDestination(obj.playerAddress));
        SER_WRITE(obj, res_type = (uint8_t) obj.resultType);

        READWRITE(game_type, obj.vBetInfo, obj.betAmount, address_str, obj.betTime, res_type, obj.payout, obj.completed);

        SER_READ(obj, obj.gameType = (QuickGamesType) game_type);
        SER_READ(obj, obj.playerAddress = DecodeDestination(address_str));
        SER_READ(obj, obj.resultType = (BetResultType) res_type);
    }
private:
    bool completed = false;
};

/*
 * Betting undo database structures
 */

using BettingUndoKey = uint256;

using BettingUndoVariant = boost::variant<CPeerlessExtendedEventDB, CFieldEventDB>;

typedef enum BettingUndoTypes {
    UndoPeerlessEvent = 0,
    UndoFieldEvent    = 1
} BettingUndoTypes;

class CBettingUndoDB
{
public:
    uint32_t height = 0;

    explicit CBettingUndoDB() { }

    explicit CBettingUndoDB(const BettingUndoVariant& undoVar, const uint32_t height) : height{height}, undoVariant{undoVar} { }

    bool Inited() {
        return !undoVariant.empty();
    }

    BettingUndoVariant Get() {
        return undoVariant;
    }

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action) {
        READWRITE(height);
        int undoType;
        if (ser_action.ForRead()) {
            READWRITE(undoType);
            switch ((BettingUndoTypes)undoType)
            {
                case UndoPeerlessEvent:
                {
                    CPeerlessExtendedEventDB event{};
                    READWRITE(event);
                    undoVariant = event;
                    break;
                }
                case UndoFieldEvent:
                {
                    CFieldEventDB event{};
                    READWRITE(event);
                    undoVariant = event;
                    break;
                }
                default:
                    std::runtime_error("Undefined undo type");
            }
        }
        else {
            undoType = undoVariant.which();
            READWRITE(undoType);
            switch ((BettingUndoTypes)undoType)
            {
                case UndoPeerlessEvent:
                {
                    CPeerlessExtendedEventDB event = boost::get<CPeerlessExtendedEventDB>(undoVariant);
                    READWRITE(event);
                    break;
                }
                case UndoFieldEvent:
                {
                    CFieldEventDB event = boost::get<CFieldEventDB>(undoVariant);
                    READWRITE(event);
                }
                default:
                    std::runtime_error("Undefined undo type");
            }
        }
    }
    template <typename Stream>
    void Serialize(Stream& s) const
    {
        const_cast<CBettingUndoDB*>(this)->SerializationOp(s, CSerActionSerialize());
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        SerializationOp(s, CSerActionUnserialize());
    }

private:
    BettingUndoVariant undoVariant;
};

/*
 * Betting payout info database structures
 */

using PayoutInfoKey = PeerlessBetKey;

class CPayoutInfoDB
{
public:
    PeerlessBetKey betKey;
    PayoutType payoutType;

    explicit CPayoutInfoDB() { }
    explicit CPayoutInfoDB(PeerlessBetKey &betKey, PayoutType payoutType) : betKey{betKey}, payoutType{payoutType} { }
    explicit CPayoutInfoDB(const CPayoutInfoDB& payoutInfo) : betKey{payoutInfo.betKey}, payoutType{payoutInfo.payoutType} { }

    SERIALIZE_METHODS(CPayoutInfoDB, obj)
    {
        uint8_t payout_type;

        SER_WRITE(obj, payout_type = (uint8_t) obj.payoutType);

        READWRITE(obj.betKey, payout_type);

        SER_READ(obj, obj.payoutType = (PayoutType) payout_type);
    }

    inline int CompareTo(const CPayoutInfoDB& rhs) const
    {
        if (betKey.blockHeight < rhs.betKey.blockHeight)
            return -1;
        if (betKey.blockHeight > rhs.betKey.blockHeight)
            return 1;
        if (betKey.outPoint < rhs.betKey.outPoint)
            return -1;
        if (betKey.outPoint != rhs.betKey.outPoint) // !(betKey.outPoint < rhs.betKey.outPoint) is demonstrated above
            return 1;
        if ((uint8_t)payoutType < (uint8_t)rhs.payoutType)
            return -1;
        if ((uint8_t)payoutType > (uint8_t)rhs.payoutType)
            return 1;
        return 0;
    }

    inline bool operator==(const CPayoutInfoDB& rhs) const { return CompareTo(rhs) == 0; }
    inline bool operator!=(const CPayoutInfoDB& rhs) const { return CompareTo(rhs) != 0; }
    inline bool operator<=(const CPayoutInfoDB& rhs) const { return CompareTo(rhs) <= 0; }
    inline bool operator>=(const CPayoutInfoDB& rhs) const { return CompareTo(rhs) >= 0; }
    inline bool operator<(const CPayoutInfoDB& rhs) const { return CompareTo(rhs) < 0; }
    inline bool operator>(const CPayoutInfoDB& rhs) const { return CompareTo(rhs) > 0; }
};

/*
 * Betting Database Model
 */

class CBettingDB
{
protected:
    CFlushableStorageKV db;
    CFlushableStorageKV& GetDb();
public:
    // Default Constructor.
    explicit CBettingDB(CStorageKV& db) : db{db} { }
    // Cache copy constructor (we should set global flushable storage ref as flushable storage of cached copy)
    explicit CBettingDB(CBettingDB& bdb) : CBettingDB(bdb.GetDb()) { }

    ~CBettingDB() {}

    bool Flush();

    std::unique_ptr<CStorageKVIterator> NewIterator();

    template<typename KeyType>
    bool Exists(const KeyType& key)
    {
        return db.Exists(DbTypeToBytes(key));
    }

    template<typename KeyType, typename ValueType>
    bool Write(const KeyType& key, const ValueType& value)
    {
        auto vKey = DbTypeToBytes(key);
        auto vValue = DbTypeToBytes(value);
        if (db.Exists(vKey))
            return false;
        return db.Write(vKey, vValue);
    }

    template<typename KeyType, typename ValueType>
    bool Update(const KeyType& key, const ValueType& value)
    {
        auto vKey = DbTypeToBytes(key);
        auto vValue = DbTypeToBytes(value);
        if (!db.Exists(vKey))
            return false;
        return db.Write(vKey, vValue);
    }

    template<typename KeyType>
    bool Erase(const KeyType& key)
    {
        auto vKey = DbTypeToBytes(key);
        if (!db.Exists(vKey))
            return false;
        return db.Erase(vKey);
    }

    template<typename KeyType, typename ValueType>
    bool Read(const KeyType& key, ValueType& value)
    {
        auto vKey = DbTypeToBytes(key);
        std::vector<unsigned char> vValue;
        if (db.Read(vKey, vValue)) {
            BytesToDbType(vValue, value);
            return true;
        }
        return false;
    }

    unsigned int GetCacheSize();

    unsigned int GetCacheSizeBytesToWrite();

    static size_t dbWrapperCacheSize();

    static std::string MakeDbPath(const char* name);

    template<typename T>
    static std::vector<unsigned char> DbTypeToBytes(const T& value)
    {
        CDataStream stream(SER_DISK, CLIENT_VERSION);
        stream << value;
        return std::vector<unsigned char>(stream.begin(), stream.end());
    }

    template<typename T>
    static void BytesToDbType(const std::vector<unsigned char>& bytes, T& value)
    {
        CDataStream stream(bytes, SER_DISK, CLIENT_VERSION);
        stream >> value;
        assert(stream.size() == 0);
    }
};

using FailedTxKey = BettingUndoKey;

/** Container for several db objects */
class CBettingsView
{
    // fields will be init in init.cpp
public:
    std::unique_ptr<CBettingDB> mappings; // "mappings"
    std::unique_ptr<CStorageKV> mappingsStorage;
    std::unique_ptr<CBettingDB> results; // "results"
    std::unique_ptr<CStorageKV> resultsStorage;
    std::unique_ptr<CBettingDB> events; // "events"
    std::unique_ptr<CStorageKV> eventsStorage;
    std::unique_ptr<CBettingDB> bets; // "bets"
    std::unique_ptr<CStorageKV> betsStorage;
    std::unique_ptr<CBettingDB> undos; // "undos"
    std::unique_ptr<CStorageKV> undosStorage;
    std::unique_ptr<CBettingDB> payoutsInfo; // "payoutsinfo"
    std::unique_ptr<CStorageKV> payoutsInfoStorage;
    std::unique_ptr<CBettingDB> quickGamesBets; // "quickgamesbets"
    std::unique_ptr<CStorageKV> quickGamesBetsStorage;
    std::unique_ptr<CBettingDB> chainGamesLottoEvents; // "cglottoevents"
    std::unique_ptr<CStorageKV> chainGamesLottoEventsStorage;
    std::unique_ptr<CBettingDB> chainGamesLottoBets; // "cglottobets"
    std::unique_ptr<CStorageKV> chainGamesLottoBetsStorage;
    std::unique_ptr<CBettingDB> chainGamesLottoResults; // "cglottoresults"
    std::unique_ptr<CStorageKV> chainGamesLottoResultsStorage;
    // save failed tx ids which contain in chain, but not affect on
    // it needed to avoid undo issues, when we try undo not affected tx
    std::unique_ptr<CBettingDB> failedBettingTxs; // "failedtxs"
    std::unique_ptr<CStorageKV> failedBettingTxsStorage;
    // field betting
    std::unique_ptr<CBettingDB> fieldEvents; // "events"
    std::unique_ptr<CStorageKV> fieldEventsStorage;
    std::unique_ptr<CBettingDB> fieldResults; // "results"
    std::unique_ptr<CStorageKV> fieldResultsStorage;
    std::unique_ptr<CBettingDB> fieldBets; // "bets"
    std::unique_ptr<CStorageKV> fieldBetsStorage;

    // default constructor
    explicit CBettingsView() { }

    // copy constructor for creating DB cache
    explicit CBettingsView(CBettingsView* phr);

    bool Flush();

    unsigned int GetCacheSize();

    unsigned int GetCacheSizeBytesToWrite();

    void SetLastHeight(uint32_t height);

    uint32_t GetLastHeight();

    bool SaveBettingUndo(const BettingUndoKey& key, std::vector<CBettingUndoDB> vUndos);

    bool EraseBettingUndo(const BettingUndoKey& key);

    std::vector<CBettingUndoDB> GetBettingUndo(const BettingUndoKey& key);

    bool ExistsBettingUndo(const BettingUndoKey& key);

    void PruneOlderUndos(const uint32_t height);

    bool SaveFailedTx(const FailedTxKey& key);

    bool ExistFailedTx(const FailedTxKey& key);

    bool EraseFailedTx(const FailedTxKey& key);
};

#endif