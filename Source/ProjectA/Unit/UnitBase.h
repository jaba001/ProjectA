#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attribute/AS_Unit.h"
//#include "AIController.h"

#include "UnitBase.generated.h"

class ACombatGridTile;
class AAIController;
class AUnitAIController;

UENUM(BlueprintType)
enum class ETeam : uint8
{
    Player,
    Enemy
};

UENUM()
enum class EUnitMovePhase : uint8
{
    None,
    MovingToTile,
    MovingToTarget,
    ReturningToOriginalTile
};

UCLASS()
class PROJECTA_API AUnitBase
    : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:

    // Constructor
    AUnitBase();

public:
    // GAS Interface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

public:
    // Actor lifecycle
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // Unit identity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
    int32 UnitIndex = 0;

    // Team information
    UPROPERTY(Replicated)
    ETeam Team = ETeam::Player;

    void SetTeam(ETeam NewTeam);
    ETeam GetTeam() const { return Team; }

protected:
	// player는 90 enemy는 -90이 기본값
    UPROPERTY()
    FRotator DefaultBattleRotation;

public:
    // Turn state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn")
    bool bIsActiveTurn = false;
    
    // Turn events
    UFUNCTION(BlueprintCallable, Category = "Turn")
    virtual void OnTurnStart();

    UFUNCTION(BlueprintCallable, Category = "Turn")
    virtual void OnTurnEnd();

public:

    // Grid system
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    ACombatGridTile* CurrentTile = nullptr;

    UPROPERTY()
	ACombatGridTile* PendingTile = nullptr;

    UPROPERTY()
    AUnitBase* PendingTargetUnit = nullptr;

    UPROPERTY()
    ACombatGridTile* OriginalTileBeforeAction = nullptr;

    UPROPERTY()
    EUnitMovePhase MovePhase = EUnitMovePhase::None;
    
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetCurrentTile(ACombatGridTile* NewTile);

public:
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void MoveToTile(ACombatGridTile* TargetTile);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void MoveToTarget(AUnitBase* TargetUnit);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void ReturnToOriginalTile();

    AUnitAIController* GetOrCreateAIController();

    void HandleMoveCompleted();

public:
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void DealDamage(AUnitBase* TargetUnit, float Damage);

public:
    // Life state
    bool IsUnitAlive() const;
    virtual void Die();

    // Ragdoll impulse strength
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death")
    FVector DeathImpulse;

    // Dead state
    UPROPERTY()
    bool bIsDead = false;

protected:
    // GAS core components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem = nullptr;

    UPROPERTY()
    UAS_Unit* AttributeSet = nullptr;

protected:
    // Initial attributes
    UPROPERTY(EditDefaultsOnly, Category = "Attribute")
    float InitMaxHP = 100.f;

};