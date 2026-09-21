#pragma once

#include "CoreMinimal.h"

class AUnitBase;
class UAnimMontage;
class USkeletalMesh;
class UWorld;
struct FCombatRoundSkill;
struct FCombatRoundUnitView;

namespace CombatWeaponTrace
{
    struct FBladePose
    {
        FVector Base = FVector::ZeroVector;
        FVector Tip = FVector::ZeroVector;
    };

    // MontageSeconds is an asset position; evaluate the authored pose without depending on rendered mesh ticks.
    // MontageSeconds는 에셋 재생 위치이며 화면 메시 틱에 의존하지 않고 작성된 포즈를 평가합니다.
    PROJECTA_API bool SampleBoneTransform(const USkeletalMesh* Mesh, const UAnimMontage* Montage, FName Slot, FName BoneOrSocket, double MontageSeconds, FTransform& OutComponentTransform);
    PROJECTA_API bool SampleBlade(const AUnitBase* Source, const FCombatRoundSkill& Skill, const UAnimMontage* Montage, double MontageSeconds, FBladePose& Out);
    PROJECTA_API AUnitBase* FindFirstHit(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FBladePose& Previous, const FBladePose& Current, float Radius);
}
