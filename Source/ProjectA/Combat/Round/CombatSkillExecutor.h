#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"

class ACombatGridManager;
class ACombatRoundProjectile;
class AActor;

// Skill execution owns collision geometry and GAS effects; the coordinator owns clocks and lifecycle transitions.
// 스킬 실행은 충돌 지오메트리와 GAS 효과를 소유하며 조정자는 시간과 생명주기 전이를 소유합니다.
namespace CombatSkillExecution
{
    struct PROJECTA_API FReleaseContext
    {
        AActor* Owner = nullptr;
        AUnitBase* Source = nullptr;
        AUnitBase* Target = nullptr;
        ACombatGridManager* Grid = nullptr;
        FVector AimLocation = FVector::ZeroVector;
        FIntPoint TargetCoord = FIntPoint::ZeroValue;
    };

    struct PROJECTA_API FReleaseResult
    {
        bool bSucceeded = false;
        FText Status;
    };

    struct PROJECTA_API FWeaponTraceState
    {
        double SampleTime = -1.0;
        FVector PreviousBase = FVector::ZeroVector;
        FVector PreviousTip = FVector::ZeroVector;
    };

    enum class ETraceResult : uint8
    {
        Pending,
        Hit,
        Miss,
        Invalid
    };

    PROJECTA_API bool CanUseSkill(const AUnitBase* Source, const FCombatRoundSkill& Skill);
    PROJECTA_API bool CanAffectTarget(const AUnitBase* Target, const FCombatRoundSkill& Skill);
    PROJECTA_API bool MatchesOwnedTags(const AUnitBase* Unit, const FGameplayTagQuery& Query, const FGameplayTagContainer& Required, const FGameplayTagContainer& Blocked);
    PROJECTA_API bool ApplyEffect(AUnitBase* Source, AUnitBase* Target, const FCombatRoundSkill& Skill);
    PROJECTA_API FReleaseResult Release(const FReleaseContext& Context, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill, TFunctionRef<void(AUnitBase*)> OnHit, TFunctionRef<void(ACombatRoundProjectile*)> RegisterProjectile);
    PROJECTA_API ETraceResult AdvanceWeaponTrace(AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill, double Elapsed, FWeaponTraceState& State, AUnitBase*& OutHit, FText& OutStatus);
}
