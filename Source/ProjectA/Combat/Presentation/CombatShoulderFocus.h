#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"

// One owned unit stays in focus until melee return or the first ranged/magic impact settles.
// 소유 유닛 한 명을 근접 복귀 직전 또는 최초 원거리·마법 피격 종료까지 따라갑니다.
struct PROJECTA_API FCombatShoulderFocus
{
    int32 Update(const FCombatRoundView& View, int32 ParticipantSlot, int32 PreferredUnitId);
    bool FinishAtReturnPoint(const FCombatRoundUnitView& Unit, const FCombatRoundSkill* Skill);
    void Finish() { bFinished = true; }
    void Reset() { *this = FCombatShoulderFocus(); }

private:
    FGuid CombatId;
    int32 RoundNumber = 0;
    int32 OwnerSlot = 0;
    int32 UnitId = INDEX_NONE;
    bool bFinished = false;
};
