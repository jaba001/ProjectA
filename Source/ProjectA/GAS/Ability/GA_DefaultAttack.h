#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GA_AttackBase.h"
#include "GA_DefaultAttack.generated.h"

class AUnitBase;

// Gameplay ability for single-target default attacks.
// 단일 대상 기본 공격을 처리하는 게임플레이 어빌리티입니다.
UCLASS()
class PROJECTA_API UGA_DefaultAttack : public UGA_AttackBase
{
    GENERATED_BODY()

public:
    // Sets default attack ability values.
    // 기본 공격 어빌리티 기본값을 설정합니다.
    UGA_DefaultAttack();

protected:
    // Cache the current default attack target unit.
    // 현재 기본 공격 대상 유닛을 캐시합니다.
    virtual bool CacheAttackContext() override;

    // Validate whether the cached target unit is valid.
    // 캐시된 대상 유닛이 유효한지 검사합니다.
    virtual bool ValidateAttackContext() const override;

    // Apply actual damage to the single target.
    // 단일 대상에게 실제 피해를 적용합니다.
    virtual void ApplyAttackEffect() override;

    // Clear the single-target cache.
    // 단일 대상 캐시를 정리합니다.
    virtual void ClearCachedAttackContext() override;

    // Apply the damage GE to the target ASC.
    // 대상 ASC에 피해 GE를 적용합니다.
    void ApplyDamageEffectToTarget();

protected:
    // Current attack target unit.
    // 현재 공격 대상 유닛입니다.
    UPROPERTY()
    AUnitBase* CachedTargetUnit = nullptr;
};
