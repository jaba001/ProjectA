#pragma once

#include "CoreMinimal.h"

class AActor;
class UFXSystemComponent;
class UWorld;
struct FCombatSkillVfx;
struct FCombatRoundSkill;

// Cosmetic systems reference original assets and never provide authoritative hit results.
// 표현 시스템은 원본 에셋을 참조하며 권한 있는 피격 결과를 제공하지 않습니다.
namespace CombatSkillPresentation
{
    struct FProjectileParameters
    {
        FVector WorldVelocity = FVector::ZeroVector;
        float Lifetime = 0.f;
    };

    // Prepare original effect assets before action clocks start and retain them through the owning combat session.
    // 행동 시간이 시작되기 전에 원본 효과 에셋을 준비하고 소유 전투 세션에서 참조를 유지합니다.
    bool Prepare(UWorld* World, const TArray<FCombatRoundSkill>& Skills, TArray<TObjectPtr<UObject>>& PreparedAssets, FText& OutError);
    void Attach(AActor* Owner, const FCombatSkillVfx& Visual, TArray<TObjectPtr<UFXSystemComponent>>& Components, bool bAutoDestroy = false, const FProjectileParameters* Projectile = nullptr);
    void Destroy(TArray<TObjectPtr<UFXSystemComponent>>& Components);
    void Impact(UWorld* World, const FCombatSkillVfx& Visual, const FTransform& Transform);
}
