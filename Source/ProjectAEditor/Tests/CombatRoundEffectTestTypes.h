#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "CombatRoundEffectTestTypes.generated.h"

// Editor-only effect defaults let validation tests avoid mutating shared engine or game CDOs.
// 에디터 전용 효과 기본값으로 검증 테스트가 엔진 또는 게임 공용 CDO를 수정하지 않도록 합니다.
UCLASS(Transient, NotBlueprintable)
class UCombatRoundDurationTestEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCombatRoundDurationTestEffect();
};

UCLASS(Transient, NotBlueprintable)
class UCombatRoundInfiniteTestEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCombatRoundInfiniteTestEffect();
};
