#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "GameFramework/Actor.h"
#include "CombatSkillEffectActor.generated.h"

class ACombatSkillEffectActor;
class UFXSystemComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCombatSkillEffectImpact, AUnitBase*, AUnitBase*, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatSkillEffectResolved, ACombatSkillEffectActor*);

// An authored effect volume shares its world transform with cosmetic VFX; only the server applies hits.
// 작성된 효과 볼륨은 표현 VFX와 월드 변환을 공유하며 서버만 피격을 적용합니다.
UCLASS()
class PROJECTA_API ACombatSkillEffectActor : public AActor
{
    GENERATED_BODY()

public:
    ACombatSkillEffectActor();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Register delegates before initialization because invalid data may resolve synchronously.
    // 잘못된 데이터는 즉시 종료될 수 있으므로 초기화 전에 델리게이트를 등록합니다.
    void InitializeEffect(AUnitBase* Source, AUnitBase* Target, FVector AimLocation, const FCombatRoundSkill& Skill, const TArray<FCombatRoundUnitView>& Units);
    void AdvanceEffect(float DeltaSeconds);
    bool HasResolved() const { return bResolved; }

    FOnCombatSkillEffectImpact OnImpact;
    FOnCombatSkillEffectResolved OnResolved;

private:
    void ResolveEffect(bool bDestroyActor = true);

    UFUNCTION()
    void OnRep_Visual();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastImpact(const FCombatSkillVfx& ImpactVisual, const FTransform& Transform);

    UPROPERTY(ReplicatedUsing = OnRep_Visual)
    FCombatSkillVfx Visual;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UFXSystemComponent>> VisualComponents;

    UPROPERTY(Transient)
    FCombatRoundSkill Definition;

    TWeakObjectPtr<AUnitBase> SourceUnit;
    TWeakObjectPtr<AUnitBase> TargetUnit;
    TArray<TWeakObjectPtr<AUnitBase>> AllowedTargets;
    TSet<TWeakObjectPtr<AUnitBase>> HitOnceUnits;
    FVector StartLocation = FVector::ZeroVector;
    FVector EndLocation = FVector::ZeroVector;
    FVector InitialOcclusionOrigin = FVector::ZeroVector;
    float ElapsedSeconds = 0.f;
    bool bInitialized = false;
    bool bResolved = false;
    bool bOnlyTarget = false;
};
