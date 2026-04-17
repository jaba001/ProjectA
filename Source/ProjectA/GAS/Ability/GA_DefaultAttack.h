#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GA_AttackBase.h"
#include "GA_DefaultAttack.generated.h"

class AUnitBase;

// 단일 대상 기본공격 Ability
// - 공격 공통 흐름은 UGA_AttackBase 가 담당한다.
// - 이 클래스는 단일 타겟 캐싱/검증/데미지 적용만 담당한다.
UCLASS()
class PROJECTA_API UGA_DefaultAttack : public UGA_AttackBase
{
    GENERATED_BODY()

public:
    UGA_DefaultAttack();

protected:
    // 현재 기본공격 대상 유닛을 캐싱한다.
    virtual bool CacheAttackContext() override;

    // 캐싱된 대상 유닛이 유효한지 검사한다.
    virtual bool ValidateAttackContext() const override;

    // 단일 대상에게 실제 데미지를 적용한다.
    virtual void ApplyAttackEffect() override;

    // 단일 대상 캐시를 정리한다.
    virtual void ClearCachedAttackContext() override;

    // 실제 타겟 ASC 에 데미지 GE 를 적용한다.
    void ApplyDamageEffectToTarget();

protected:
    // 현재 공격 대상 유닛
    UPROPERTY()
    AUnitBase* CachedTargetUnit = nullptr;
};