// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_MANAGER_H
#define TOKEN_GROUP_MANAGER_H

#include "consensus/tokengroups.h"
#include "tokens/tokengroupconfiguration.h"

#include <functional>
#include <sync.h>
#include <unordered_map>

class CBlockIndex;
class CTokenGroupManager;
class UniValue;

extern std::shared_ptr<CTokenGroupManager> tokenGroupManager;
extern CCriticalSection cs_main;

// TokenGroup Class
// Keeps track of all of the token groups
class CTokenGroupManager
{
public:
    CCriticalSection cs;

private:
    std::map<CTokenGroupID, CTokenGroupCreation> mapTokenGroups;
    std::map<uint32_t, CTokenGroupID> mapBetTokens;
    std::unique_ptr<CTokenGroupCreation> tgMGTCreation;
    std::unique_ptr<CTokenGroupCreation> tgORATCreation;

    // Processed, added and databased while locked by cs_main
    std::vector<CTokenGroupCreation> newTokenGroups;

public:
    CTokenGroupManager();

    void GetNewTokenGroups(std::vector<CTokenGroupCreation>& ret) { ret = newTokenGroups; };
    bool AddTokenGroups(const std::vector<CTokenGroupCreation>& newTokenGroups);
    bool RemoveTokenGroup(CTransaction tx, CTokenGroupID &toRemoveTokenGroupID);
    void ResetTokenGroups();

    bool GetTokenGroupCreation(const CTokenGroupID& tgID, CTokenGroupCreation& tgCreation);
    std::string GetTokenGroupNameByID(CTokenGroupID tokenGroupId);
    std::string GetTokenGroupTickerByID(CTokenGroupID tokenGroupId);
    bool GetTokenGroupIdByTicker(std::string strTicker, CTokenGroupID &tokenGroupID);
    bool GetTokenGroupIdByName(std::string strName, CTokenGroupID &tokenGroupID);
    bool GetTokenGroupIdByEventID(uint32_t nEventID, CTokenGroupID &tokenGroupID);
    std::map<CTokenGroupID, CTokenGroupCreation> GetMapTokenGroups() { return mapTokenGroups; };
    std::map<uint32_t, CTokenGroupID> GetMapBetTokens() { return mapBetTokens; };

    template <typename CTokenGroupDescription> bool GetTokenGroupDescription(const CTokenGroupID& tgID, std::shared_ptr<CTokenGroupDescription>& tgDesc);

    bool StoreManagementTokenGroups(CTokenGroupCreation tokenGroupCreation);
    void ClearManagementTokenGroups();

    bool MatchesMGT(CTokenGroupID tgID);
    bool MatchesORAT(CTokenGroupID tgID);

    bool ManagementTokensCreated();

    CTokenGroupID GetMGTID() { return tgMGTCreation->tokenGroupInfo.associatedGroup; };
    CTokenGroupID GetORATID() { return tgORATCreation->tokenGroupInfo.associatedGroup; };

    bool MGTTokensCreated() { return tgMGTCreation ? true : false; };
    bool ORATTokensCreated() { return tgORATCreation ? true : false; };

    uint16_t GetTokensInBlock(const CBlock& block, const CTokenGroupID& tgId);
    unsigned int GetTokenTxStats(const CTransactionRef &tx, const CCoinsViewCache& view, const CTokenGroupID &tgId, uint16_t &nTokenCount, CAmount &nTokenMint);

    bool TokenMoneyRange(CAmount nValueOut);
    CAmount AmountFromTokenValue(const UniValue& value, const CTokenGroupID& tgID);
    std::string TokenValueFromAmount(const CAmount& amount, const CTokenGroupID& tgID);

    bool CheckFees(const CTransaction &tx, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, CValidationState& state);

    bool CollectTokensFromBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& _state, const CCoinsViewCache& view, bool fJustCheck);
    bool ApplyTokensFromBlock();
    bool UndoBlock(const CBlock& block, const CBlockIndex* pindex);
};

#endif
