#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GA_AttackBase.h"
#include "GA_AreaAttack.generated.h"

class ACombatGridTile;
class ACombatGridManager;
class AUnitBase;
class USkillDefinitionDataAsset;

// 타일 중심 범위공격 Ability
// - 공통 공격 수명주기는 UGA_AttackBase 가 담당한다.
// - 이 클래스는 중심 타일/스킬 데이터 캐싱
// - 범위 타일 계산
// - 범위 유닛 수집
// - 다중 대상 데미지 적용
// 을 담당한다.
UCLASS()
class PROJECTA_API UGA_AreaAttack : public UGA_AttackBase
{
    GENERATED_BODY()

public:
    UGA_AreaAttack();

protected:
    // 현재 범위공격 컨텍스트를 캐싱한다.
    virtual bool CacheAttackContext() override;

    // 캐싱된 컨텍스트가 유효한지 검사한다.
    virtual bool ValidateAttackContext() const override;

    // 범위 대상에게 실제 공격 효과를 적용한다.
    virtual void ApplyAttackEffect() override;

    // 범위공격 캐시를 정리한다.
    virtual void ClearCachedAttackContext() override;

protected:
    // 실제 범위 중심 타일을 해석한다.
    ACombatGridTile* ResolveCenterTile() const;

    // 실제 피격 유닛 배열을 계산한다.
    TArray<AUnitBase*> ResolveAreaTargetUnits() const;

    // 팀 규칙 기준으로 실제 영향 대상인지 검사한다.
    bool IsValidAreaTargetUnit(AUnitBase* TargetUnit) const;

protected:
    // 현재 선택된 타겟 타일
    UPROPERTY()
    ACombatGridTile* CachedTargetTile = nullptr;

    // 현재 실행 중인 스킬 정의 데이터
    UPROPERTY()
    USkillDefinitionDataAsset* CachedSkillData = nullptr;
};