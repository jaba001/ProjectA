#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Types/UnitActionTypes.h"
#include "UnitBase.generated.h"

class ACombatGridTile;
class AUnitAIController;
class UGameplayAbility;
class USkillDefinitionDataAsset;
class AUnitBase;
struct FCombatCheckpointUnit;

// Team affiliation used by combat units.
// 전투 유닛의 소속 팀을 나타냅니다.
UENUM(BlueprintType)
enum class ETeam : uint8
{
    Player,
    Enemy
};

// High-level action currently being performed by a unit.
// 유닛이 현재 수행 중인 상위 행동 종류입니다.
UENUM(BlueprintType)
enum class EUnitActionType : uint8
{
    None,
    Skill,
    Move,
    Item
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUnitActionCompleted, AUnitBase*, EUnitActionType, EUnitActionResult);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnitDied, AUnitBase*);

// Movement and action phase used while a unit is busy.
// 유닛이 바쁜 동안 사용하는 이동 및 행동 단계입니다.
UENUM(BlueprintType)
enum class EUnitMovePhase : uint8
{
    None,
    MovingToTile,
    MovingToTarget,
    WaitingForSkill,
    ReturningToOriginalTile
};

// Base character class for all combat units.
// 모든 전투 유닛의 기본 캐릭터 클래스입니다.
UCLASS()
class PROJECTA_API AUnitBase
    : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // Construction and base interface
    // 생성과 기본 인터페이스 처리입니다.
    AUnitBase();

    // Returns the ability system component for GAS integration.
    // GAS 연동에 사용할 어빌리티 시스템 컴포넌트를 반환합니다.
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // Returns the unit attribute set.
    // 유닛 어트리뷰트 세트를 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "UnitBase|GAS")
    UAS_Unit* GetAttributeSet() const { return AttributeSet; }

    // Actor lifecycle
    // 액터 생명주기 처리입니다.
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    // Network replication
    // 네트워크 복제 속성을 등록합니다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // Unit identifier
    // 유닛 식별 번호입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "UnitBase")
    int32 UnitIndex = 0;

    // Display name copied from the run party without owning persistent state.
    // 영구 상태를 소유하지 않고 런 파티에서 복사한 표시 이름입니다.
    UPROPERTY(BlueprintReadOnly, Replicated, Category = "UnitBase|Runtime")
    FText RuntimeCharacterName;

    // Team affiliation
    // 유닛의 팀 소속입니다.
    UPROPERTY(ReplicatedUsing = OnRep_Team)
    ETeam Team = ETeam::Player;

    // Changes this unit's team affiliation.
    // 이 유닛의 팀 소속을 변경합니다.
    UFUNCTION(BlueprintCallable, Category = "UnitBase")
    void SetTeam(ETeam NewTeam);

    // Returns this unit's team affiliation.
    // 이 유닛의 팀 소속을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "UnitBase")
    ETeam GetTeam() const { return Team; }

protected:
    // Replication updates presentation without running server combat callbacks.
    // 복제는 서버 전투 콜백을 실행하지 않고 화면만 갱신합니다.
    UFUNCTION()
    void OnRep_Team();

    UFUNCTION()
    void OnRep_CurrentTile(ACombatGridTile* PreviousTile);

    UFUNCTION()
    void OnRep_Death();

    void ApplyDeathPresentation();
    bool bDeathPresentationApplied = false;

    // Default battle orientation
    // Player uses Yaw 90
    // Enemy uses Yaw -90
    UPROPERTY()
    FRotator DefaultBattleRotation;

    // Current action type
    UPROPERTY(Replicated)
    EUnitActionType CurrentActionType = EUnitActionType::None;

public:
    // Whether this unit is currently active in turn
    // 현재 이 유닛의 턴이 활성화되어 있는지 여부입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|Turn")
    bool bIsActiveTurn = false;

    // Activate unit and reset AP at turn start
    // 턴 시작 시 유닛을 활성화하고 행동력을 초기화합니다.
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Turn")
    virtual void OnTurnStart();

    // Deactivate unit at turn end
    // 턴 종료 시 유닛을 비활성화합니다.
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Turn")
    virtual void OnTurnEnd();

    // Flag indicating turn must end after current action
    UPROPERTY(Replicated)
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
    void ResetActionPoint();

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
    void ResetSubActionPoint();

public:
    // Check if unit is alive
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Death")
    virtual bool IsUnitAlive() const;

    // Handle unit death
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Death")
    virtual void Die();

    // Ragdoll impulse
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "UnitBase|Death")
    FVector DeathImpulse;

protected:
    // Death flag
    UPROPERTY(ReplicatedUsing = OnRep_Death)
    bool bIsDead = false;

public:
    // Current occupied combat tile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentTile, Category = "UnitBase|Grid")
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
    bool IsBusy() const { return CurrentActionType != EUnitActionType::None || MovePhase != EUnitMovePhase::None; }

    // Cancels active abilities and movement before encounter cleanup.
    // 인카운터 정리 전에 활성 어빌리티와 이동을 취소합니다.
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Action")
    void CancelCurrentAction();

    FOnUnitActionCompleted OnActionCompleted;
    FOnUnitDied OnUnitDied;

    // Entry point for AIController movement completion callback
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Movement")
    virtual void HandleMoveCompleted();

    // Entry point for AIController movement failure callback
    virtual void HandleMoveFailed(EUnitActionResult Result = EUnitActionResult::Failed);

    // Get or create AIController
    AUnitAIController* GetOrCreateAIController();

protected:
    // Action lifetime is independent of whether the skill requires movement.
    // 행동 수명은 스킬의 이동 필요 여부와 독립적입니다.
    void BeginCurrentAction(EUnitActionType ActionType);
    void CompleteCurrentAction(EUnitActionResult Result);
    virtual void OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result);
    void RestoreActionOrigin();
    void HandleSkillAbilityEnded(const FAbilityEndedData& EndedData);
    void CompleteSkillExecution(EUnitActionResult Result);

    UFUNCTION()
    void HandleActionSnapFinished(int32 ActionSerial);

    UPROPERTY()
    TObjectPtr<ACombatGridTile> ActionOriginTile = nullptr;

    FTransform ActionOriginTransform;
    FDelegateHandle SkillAbilityEndedHandle;
    FGameplayAbilitySpecHandle ActiveSkillHandle;
    uint32 CurrentActionSerial = 0;
    bool bSkillRequiresReturn = false;

    // Current movement/action phase
    UPROPERTY(Replicated)
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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|Move")
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|Item")
    float HealingItemAmount = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|Item")
    int32 HealingItemCount = 1;

    UFUNCTION(BlueprintPure, Category = "UnitBase|Item")
    bool CanUseHealingItem(AUnitBase* TargetUnit) const;

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    bool AcquireAndEquipSkill(USkillDefinitionDataAsset* Skill);

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    USkillDefinitionDataAsset* AcquireSkillFromPool(class USkillPoolDataAsset* Pool);

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
    UPROPERTY(EditDefaultsOnly, Replicated, Category = "UnitBase|GAS|Ability")
    TSubclassOf<UGameplayAbility> DefaultAttackAbilityClass;

    // Additional skill slots for AI and combat logic
    // Assumes up to 4 skills equipped in addition to default attack
    UPROPERTY(EditDefaultsOnly, Replicated, Category = "UnitBase|GAS|Ability")
    TArray<TSubclassOf<UGameplayAbility>> EquippedSkillAbilityClasses;

    // Skill definition data currently equipped by this unit
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|Skill", meta = (AllowPrivateAccess = "true"))
    TArray<TObjectPtr<USkillDefinitionDataAsset>> EquippedSkillDataAssets;

public:
    // Get skill abilities available for AI evaluation
    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    virtual TArray<TSubclassOf<UGameplayAbility>> GetAvailableSkillAbilityClasses() const;

    // Find SkillData matching a given AbilityClass
    USkillDefinitionDataAsset* FindSkillDataByAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const;

    const TArray<TObjectPtr<USkillDefinitionDataAsset>>& GetEquippedSkillDataAssets() const { return EquippedSkillDataAssets; }

    UFUNCTION(BlueprintCallable, Category = "UnitBase|Skill")
    TSubclassOf<UGameplayAbility> GetDefaultAttackAbilityClass() const { return DefaultAttackAbilityClass; }

    float GetInitialMaxHP() const { return InitMaxHP; }
    // Configure runtime movement data before this unit enters an active turn.
    // 유닛이 활성 턴에 들어가기 전에 런타임 이동 데이터를 설정합니다.
    bool ConfigureMoveRange(int32 InMoveRange);
    // Apply resolved profession data before the spawned unit enters combat.
    // 스폰 유닛이 전투에 들어가기 전에 해석된 직업 데이터를 적용합니다.
    bool ConfigureProfession(float MaxHP, int32 AP, int32 SubAP, const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills);
    bool CaptureCheckpointState(FCombatCheckpointUnit& OutState, FText& OutError) const;
    bool RestoreCheckpointState(const FCombatCheckpointUnit& State, FText& OutError);

private:
    bool bCheckpointStateRestored = false;

protected:
    // Initial attributes
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|GAS|Attribute")
    float InitMaxHP = 100.f;

    // Max Action Points per unit
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "UnitBase|ActionPoint")
    int32 MaxActionPoint = 2;

    // Remaining Action Points for current turn
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|ActionPoint")
    int32 CurrentActionPoint = 0;

    // Max Sub Action Points per unit
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "UnitBase|SubActionPoint")
    int32 MaxSubActionPoint = 1;

    // Remaining Sub Action Points for current turn
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "UnitBase|SubActionPoint")
    int32 CurrentSubActionPoint = 0;

};
