#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"

// A local shot stays with one owned unit until its action settles; no combat state is changed.
// 로컬 시점은 행동이 끝날 때까지 소유 유닛 한 명을 유지하며 전투 상태를 변경하지 않습니다.
struct PROJECTA_API FCombatShoulderFocus
{
    int32 Update(const FCombatRoundView& View, int32 ParticipantSlot, int32 PreferredUnitId);
    void Finish() { bFinished = true; }
    void Reset() { *this = FCombatShoulderFocus(); }

private:
    FGuid CombatId;
    int32 RoundNumber = 0;
    int32 OwnerSlot = 0;
    int32 UnitId = INDEX_NONE;
    bool bFinished = false;
};
