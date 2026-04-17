#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attribute/AS_Unit.h"
#include "UnitBase.generated.h"

class ACombatGridTile;
class AUnitAIController;
class UGameplayAbility;
class USkillDefinitionDataAsset;

UENUM(BlueprintType)
enum class ETeam : uint8
{
    Player,
    Enemy
};

UENUM(BlueprintType)
enum class EUnitActionType : uint8
{
    None,
    Skill,
    Move,
    Item
};

UENUM(BlueprintType)
enum class EUnitMovePhase : uint8
{
    None,
    MovingToTile,
    MovingToTarget,
    WaitingForSkill,
    ReturningToOriginalTile
};

UCLASS()
class PROJECTA_API AUnitBase
    : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // Construction and base interface
    AUnitBase();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintCallable, Category = "UnitBase|GAS")
    UAS_Unit* GetAttributeSet() const { return AttributeSet; }

    // Actor lifecycle
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Network replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // Unit identifier
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnitBase")
    int32 UnitIndex = 0;

    // Team affiliation
    UPROPERTY(Replicated)
    ETeam Team = ETeam::Player;

    UFUNCTION(BlueprintCallable, Category = "UnitBase")
    void SetTeam(ETeam NewTeam);

    UFUNCTION(BlueprintCallable, Category = "UnitBase")
    ETeam GetTeam() const { return Team; }

protected:
    // Default battle orientation
    // Player uses Yaw 90
    // Enemy uses Yaw -90
    UPROPERTY()
    FRotator DefaultBattleRotation;

    // Current action type
    UPROPERTY()
    EUnitActionType CurrentActionType = EUnitActionType::None;

public:
    // Whether this unit is currently active in turn
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UnitBase|Turn")
    bool bIsActiveTurn = false;

    // Activate unit and reset AP at turn start
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Turn")
    virtual void OnTurnStart();

    // Deactivate unit at turn end
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Turn")
    virtual void OnTurnEnd();

    // Flag indicating turn must end after current action
    UPROPERTY()
    bool bTurnMustEndAfterCurrentAction = false;

    // Check if turn must end after current action
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Turn")
    bool MustEndTurnAfterCurrentAction() const { return bTurnMustEndAfterCurrentAction; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Turn")
    bool IsActiveTurn() const { return bIsActiveTurn; }

public:
    // Action resources
    // Reset to MaxActionPoint at turn start
    UFUNCTION(BlueprintCallable, Category = "UnitBase|ActionPoint")
    int32 GetCurrentActionPoint() const { return CurrentActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|ActionPoint")
    int32 GetMaxActionPoint() const { return MaxActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|ActionPoint")
    bool HasEnoughActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "UnitBase|ActionPoint")
    bool ConsumeActionPoint(int32 Cost);

    UFUNCTION(BlueprintCallable, Category = "UnitBase|ActionPoint")
    void ResetActionPoint() { CurrentActionPoint = MaxActionPoint; }

public:
    // Sub-action resources
    // Reset to MaxSubActionPoint at turn start
    UFUNCTION(BlueprintCallable, Category = "UnitBase|SubActionPoint")
    int32 GetCurrentSubActionPoint() const { return CurrentSubActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|SubActionPoint")
    int32 GetMaxSubActionPoint() const { return MaxSubActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|SubActionPoint")
    bool HasEnoughSubActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "UnitBase|SubActionPoint")
    bool ConsumeSubActionPoint(int32 Cost);

    UFUNCTION(BlueprintCallable, Category = "UnitBase|SubActionPoint")
    void ResetSubActionPoint() { CurrentSubActionPoint = MaxSubActionPoint; }

public:
    // Check if unit is alive
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Death")
    virtual bool IsUnitAlive() const;

    // Handle unit death
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Death")
    virtual void Die();

    // Ragdoll impulse
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnitBase|Death")
    FVector DeathImpulse;

protected:
    // Death flag
    UPROPERTY()
    bool bIsDead = false;

public:
    // Current occupied combat tile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UnitBase|Grid")
    ACombatGridTile* CurrentTile = nullptr;

    // Pending tile for movement
    UPROPERTY()
    ACombatGridTile* PendingTile = nullptr;

    // Original tile before action (used for return after melee attack)
    UPROPERTY()
    ACombatGridTile* OriginalTileBeforeSkill = nullptr;

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Grid")
    void SetCurrentTile(ACombatGridTile* NewTile);

    ACombatGridTile* GetCurrentTile() const { return CurrentTile; }

public:
    // Movement logic
    // Move to target tile
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    virtual void MoveToTile(ACombatGridTile* TargetTile);

    // Move toward a target unit
    // Currently used for melee attack approach
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    virtual void MoveToTarget(AUnitBase* TargetUnit);

    // Return to original tile after action
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    virtual void ReturnToOriginalTile();

    // Snap to tile center with interpolation
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    virtual void SnapToTile(ACombatGridTile* Tile, const FRotator& TargetRotation);

    // Callback after MoveComponentTo completes
    UFUNCTION(Category = "UnitBase|Movement")
    virtual void OnSnapToTileFinished();

    // Callback after returning to original tile completes
    UFUNCTION(Category = "UnitBase|Movement")
    virtual void OnReturnToOriginalTileFinished();

    // Check if unit is currently moving or acting
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    bool IsBusy() const { return MovePhase != EUnitMovePhase::None; }

    // Entry point for AIController movement completion callback
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    virtual void HandleMoveCompleted();

    // Get or create AIController
    AUnitAIController* GetOrCreateAIController();

protected:
    // Current movement/action phase
    UPROPERTY()
    EUnitMovePhase MovePhase = EUnitMovePhase::None;

public:
    // Current skill target unit
    UPROPERTY()
    AUnitBase* PendingTargetUnit = nullptr;

    // Selected target tile for current skill input
    UPROPERTY()
    ACombatGridTile* PendingSkillTargetTile = nullptr;

    // Skill definition data currently pending execution
    UPROPERTY()
    TObjectPtr<USkillDefinitionDataAsset> PendingSkillData = nullptr;

    // Ability class scheduled for execution
    UPROPERTY()
    TSubclassOf<UGameplayAbility> PendingSkillAbilityClass = nullptr;

    // If true, move to target before executing skill
    // If false, execute skill immediately in place
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    virtual void StartSkill(USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile);

    // Execute skill on stored target using GAS Ability
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    virtual void ExecuteSkillAtTarget();

    // Resolve actual target units affected by the skill
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    virtual TArray<AUnitBase*> ResolveSkillTargetUnits();

    // Handle skill completion
    // Default behavior is returning to original tile
    UFUNCTION(Category = "UnitBase|Skill")
    virtual void OnSkillFinished();

    // Clear skill context
    UFUNCTION(Category = "UnitBase|Skill")
    virtual void ClearSkillContext();

    // Prevent duplicate damage application within a single action
    UPROPERTY()
    bool bSkillDamageApplied = false;

protected:
    // Movement action category
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UnitBase|Move")
    int32 MoveRange = 1;

public:
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Move")
    int32 GetMoveRange() const { return MoveRange; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Move")
    virtual void StartMoveAction(ACombatGridTile* TargetTile);

    UFUNCTION(Category = "UnitBase|Move")
    virtual void OnMoveActionFinished();

    UFUNCTION(Category = "UnitBase|Move")
    virtual void ClearMoveContext();

public:
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Item")
    virtual void StartItemAction(AUnitBase* TargetUnit);

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Item")
    virtual void ExecuteItemAtTarget();

    UFUNCTION(Category = "UnitBase|Item")
    virtual void OnItemFinished();

    UFUNCTION(Category = "UnitBase|Item")
    virtual void ClearItemContext();

protected:
    // GAS core component
    // Handles Ability, Effect, and Attribute processing
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UnitBase|GAS")
    UAbilitySystemComponent* AbilitySystem = nullptr;

    // Attribute set
    UPROPERTY()
    UAS_Unit* AttributeSet = nullptr;

    // Default attack Ability class
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|GAS|Ability")
    TSubclassOf<UGameplayAbility> DefaultAttackAbilityClass;

    // Additional skill slots for AI and combat logic
    // Assumes up to 4 skills equipped in addition to default attack
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|GAS|Ability")
    TArray<TSubclassOf<UGameplayAbility>> EquippedSkillAbilityClasses;

    // Skill definition data currently equipped by this unit
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UnitBase|Skill", meta = (AllowPrivateAccess = "true"))
    TArray<TObjectPtr<USkillDefinitionDataAsset>> EquippedSkillDataAssets;

public:
    // Get skill abilities available for AI evaluation
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    virtual TArray<TSubclassOf<UGameplayAbility>> GetAvailableSkillAbilityClasses() const;

    // Find SkillData matching a given AbilityClass
    USkillDefinitionDataAsset* FindSkillDataByAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const;

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    TSubclassOf<UGameplayAbility> GetDefaultAttackAbilityClass() const { return DefaultAttackAbilityClass; }

protected:
    // Initial attributes
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|GAS|Attribute")
    float InitMaxHP = 100.f;

    // Max Action Points per unit
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UnitBase|ActionPoint")
    int32 MaxActionPoint = 2;

    // Remaining Action Points for current turn
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UnitBase|ActionPoint")
    int32 CurrentActionPoint = 0;

    // Max Sub Action Points per unit
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UnitBase|SubActionPoint")
    int32 MaxSubActionPoint = 1;

    // Remaining Sub Action Points for current turn
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UnitBase|SubActionPoint")
    int32 CurrentSubActionPoint = 0;

};