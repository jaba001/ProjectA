#pragma once

#include "CoreMinimal.h"
#include "CombatDebugSkillTiming.generated.h"

// Temporary tuning changes numeric timing only, preserving the authored GAS and presentation contract.
// 임시 조정은 시간 수치만 변경하며 원본 GAS 및 표현 계약을 보존합니다.
USTRUCT()
struct PROJECTA_API FCombatDebugSkillTiming
{
    GENERATED_BODY()

    UPROPERTY()
    float WindupSeconds = 0.3f;

    UPROPERTY()
    float EffectHitDelaySeconds = 0.f;

    UPROPERTY()
    float EffectDuration = 0.5f;

    UPROPERTY()
    float ProjectileSpeed = 700.f;

    UPROPERTY()
    float WeaponTraceDuration = 0.2f;
};
