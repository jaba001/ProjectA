#pragma once

#include "Game/Run/RunStateSubsystem.h"

namespace RunRewardTests
{
    // Progression fixtures explicitly collect rewards before testing their unrelated next phase.
    // 진행 테스트는 이후 단계 검증 전에 보상을 명시적으로 수령합니다.
    inline bool CollectPendingGoldRewards(URunStateSubsystem* Run)
    {
        if (!Run) return false;
        const TArray<FGuid> Recipients = Run->GetGoldRewardRecipientIds();
        for (const FGuid& CharacterId : Recipients)
        {
            if (Run->GetGoldRewardState().Claims.ContainsByPredicate([&CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; })) continue;
            const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([&CharacterId](const FRunPartyMember& Entry) { return Entry.CharacterId == CharacterId; });
            FText Error;
            if (!Member || !Run->SelectGoldReward(Member->OwnerAccountId, CharacterId, Run->GetCurrentNodeId(), 0, Error)) return false;
        }
        return Run->CanContinueAfterRewards();
    }
}
