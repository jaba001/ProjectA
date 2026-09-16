#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatRoundProjectile.generated.h"

class AUnitBase;
class ACombatRoundProjectile;
class UStaticMeshComponent;
enum class ETeam : uint8;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCombatRoundProjectileImpact, AUnitBase*, AUnitBase*, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatRoundProjectileResolved, ACombatRoundProjectile*);

// The round coordinator advances this projectile independently of the source ability.
// 라운드 조정자가 시전자 어빌리티와 독립적으로 이 투사체를 진행합니다.
UCLASS()
class PROJECTA_API ACombatRoundProjectile : public AActor
{
    GENERATED_BODY()

public:
    ACombatRoundProjectile();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Bind completion delegates before initialization because invalid data resolves immediately.
    // 잘못된 데이터는 즉시 종료되므로 초기화 전에 완료 델리게이트를 연결합니다.
    void InitializeProjectile(AUnitBase* Source, AUnitBase* Target, FVector AimPoint, float Speed, float Damage, float Radius, float Lifetime, bool bHoming, bool bTargetOnly);

    // Set the encounter roster before initialization; an empty list permits no unit hits.
    // 초기화 전에 전투 참가 목록을 설정하며 빈 목록은 모든 유닛 피격을 차단합니다.
    void SetAllowedTargets(const TArray<AUnitBase*>& Targets);

    // Only the server coordinator supplies simulation steps; actor Tick never applies damage.
    // 서버 조정자만 시뮬레이션 간격을 전달하며 액터 Tick은 피해를 적용하지 않습니다.
    void AdvanceProjectile(float DeltaSeconds);
    bool HasResolved() const { return bResolved; }

    FOnCombatRoundProjectileImpact OnImpact;
    FOnCombatRoundProjectileResolved OnResolved;

private:
    bool IsEligibleTarget(AUnitBase* Unit) const;
    void ResolveProjectile(AUnitBase* HitUnit = nullptr, bool bDestroyActor = true);

    UFUNCTION()
    void OnRep_VisualRadius();

    UPROPERTY(VisibleAnywhere, Category = "Round Combat")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_VisualRadius)
    float VisualRadius = 12.0f;

    TWeakObjectPtr<AUnitBase> SourceUnit;
    TWeakObjectPtr<AUnitBase> TargetUnit;
    TSet<TWeakObjectPtr<AUnitBase>> AllowedTargets;
    ETeam SourceTeam;
    FVector TargetPoint = FVector::ZeroVector;
    float FlightSpeed = 0.0f;
    float DamageAmount = 0.0f;
    float CollisionRadius = 12.0f;
    float RemainingLifetime = 0.0f;
    bool bTrackTarget = false;
    bool bOnlyTarget = false;
    bool bRestrictTargets = false;
    bool bInitialized = false;
    bool bResolved = false;
};
