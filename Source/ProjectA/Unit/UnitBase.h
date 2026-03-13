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
    // 생성 및 기본 인터페이스
    AUnitBase();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintCallable, Category = "GAS")
    UAS_Unit* GetAttributeSet() const { return AttributeSet; }

    // Actor 수명주기
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 네트워크 복제
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // 유닛 식별자
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
    int32 UnitIndex = 0;

    // 소속 팀
    UPROPERTY(Replicated)
    ETeam Team = ETeam::Player;

    UFUNCTION(BlueprintCallable, Category = "Unit")
    void SetTeam(ETeam NewTeam);

    UFUNCTION(BlueprintCallable, Category = "Unit")
    ETeam GetTeam() const { return Team; }

protected:
    // 전투 기준 기본 방향
    // Player 는 Yaw 90
    // Enemy 는 Yaw -90 을 사용하는 방향을 상정한다.
    UPROPERTY()
    FRotator DefaultBattleRotation;

    // 현재 행동 유형
    UPROPERTY()
    EUnitActionType CurrentActionType = EUnitActionType::None;

public:
    // 현재 턴 활성 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn")
    bool bIsActiveTurn = false;

    // 턴 시작 시 활성화 및 AP 초기화
    UFUNCTION(BlueprintCallable, Category = "Turn")
    virtual void OnTurnStart();

    // 턴 종료 시 비활성화
    UFUNCTION(BlueprintCallable, Category = "Turn")
    virtual void OnTurnEnd();

	// 턴 종료 예약 플래그
    UPROPERTY()
    bool bTurnMustEndAfterCurrentAction = false;

	// 현재 행동이 끝난 후 턴 종료 여부 확인
    UFUNCTION(BlueprintCallable, Category = "Turn")
    bool MustEndTurnAfterCurrentAction() const { return bTurnMustEndAfterCurrentAction; }

public:
    // 행동 자원
    // 기본값은 MaxActionPoint 기준으로 턴 시작 시 리셋된다.
    UFUNCTION(BlueprintCallable, Category = "ActionPoint")
    int32 GetCurrentActionPoint() const { return CurrentActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "ActionPoint")
    int32 GetMaxActionPoint() const { return MaxActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "ActionPoint")
    bool HasEnoughActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "ActionPoint")
    bool ConsumeActionPoint(int32 Cost);

    UFUNCTION(BlueprintCallable, Category = "ActionPoint")
    void ResetActionPoint() { CurrentActionPoint = MaxActionPoint; }
    
public:
	// 서브 행동 자원
	// 기본값은　MaxSubActionPoint 기준으로 턴 시작 시 리셋된다.
    UFUNCTION(BlueprintCallable, Category = "SubActionPoint")
    int32 GetCurrentSubActionPoint() const { return CurrentSubActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "SubActionPoint")
    int32 GetMaxSubActionPoint() const { return MaxSubActionPoint; }

    UFUNCTION(BlueprintCallable, Category = "SubActionPoint")
    bool HasEnoughSubActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "SubActionPoint")
    bool ConsumeSubActionPoint(int32 Cost);

    UFUNCTION(BlueprintCallable, Category = "SubActionPoint")
    void ResetSubActionPoint() { CurrentSubActionPoint = MaxSubActionPoint; }

public:
    // 생존 여부 확인
    UFUNCTION(BlueprintCallable, Category = "Death")
    virtual bool IsUnitAlive() const;

    // 사망 처리
    UFUNCTION(BlueprintCallable, Category = "Death")
    virtual void Die();

    // 래그돌 임펄스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death")
    FVector DeathImpulse;

protected:
    // 사망 플래그
    UPROPERTY()
    bool bIsDead = false;

public:
    // 현재 점유 중인 전투 타일
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    ACombatGridTile* CurrentTile = nullptr;

    // 이동 중 도착 예정 타일
    UPROPERTY()
    ACombatGridTile* PendingTile = nullptr;

    // 행동 시작 전 원래 위치
    // 근접 공격 후 복귀 시 사용한다.
    UPROPERTY()
    ACombatGridTile* OriginalTileBeforeSkill = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetCurrentTile(ACombatGridTile* NewTile);

public:
    // 지정 타일로 이동
    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void MoveToTile(ACombatGridTile* TargetTile);

    // 지정 유닛 앞으로 이동
    // 현재는 근접 공격 접근용 흐름에서 사용한다.
    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void MoveToTarget(AUnitBase* TargetUnit);

    // 행동 전 원래 타일로 복귀
    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void ReturnToOriginalTile();

    // 타일 중심으로 위치 보정
    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void SnapToTile(ACombatGridTile* Tile, const FRotator& TargetRotation);

    // MoveComponentTo 완료 콜백
    UFUNCTION(Category = "Movement")
    virtual void OnSnapToTileFinished();

	// 원래 타일로 복귀 완료 콜백
    UFUNCTION(Category = "Movement")
    virtual void OnReturnToOriginalTileFinished();

    // AIController 이동 완료 콜백 진입점
    UFUNCTION(BlueprintCallable, Category = "Movement")
    virtual void HandleMoveCompleted();

    // AIController 확보
    AUnitAIController* GetOrCreateAIController();

protected:
    // 현재 이동/행동 단계
    UPROPERTY()
    EUnitMovePhase MovePhase = EUnitMovePhase::None;

public:
    // 현재 스킬 행동 대상
    UPROPERTY()
    AUnitBase* PendingTargetUnit = nullptr;

    // 현재 실행 예정인 Skill Ability 클래스
    UPROPERTY()
    TSubclassOf<UGameplayAbility> PendingSkillAbilityClass = nullptr;

    // bMoveToTarget 이 true 이면 이동 후 스킬
    // false 이면 제자리에서 즉시 스킬
    UFUNCTION(BlueprintCallable, Category = "Skill")
    virtual void StartSkill(TSubclassOf<UGameplayAbility> AbilityClass, AUnitBase* TargetUnit, bool bMoveToTarget = true);

    // 현재 저장된 타겟에게 실제 스킬 실행
    // PendingSkillAbilityClass를 사용해 GAS Ability 활성화를 시도한다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    virtual void ExecuteSkillAtTarget();

    // 행동 종료 처리
    // 기본 구현은 원래 타일로 복귀를 시작한다.
    UFUNCTION(Category = "Skill")
    virtual void OnSkillFinished();

    // 행동 컨텍스트 정리
    UFUNCTION(Category = "Skill")
    virtual void ClearSkillContext();

    // 동일 행동 내 중복 데미지 적용 방지용 플래그
    UPROPERTY()
    bool bSkillDamageApplied = false;

public:
    UFUNCTION(BlueprintCallable, Category = "Move")
    virtual void StartMoveAction(ACombatGridTile* TargetTile);

    UFUNCTION(Category = "Move")
    virtual void OnMoveActionFinished();

    UFUNCTION(Category = "Move")
    virtual void ClearMoveContext();

public:
    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual void StartItemAction(AUnitBase* TargetUnit);

    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual void ExecuteItemAtTarget();

    UFUNCTION(Category = "Item")
    virtual void OnItemFinished();

    UFUNCTION(Category = "Item")
    virtual void ClearItemContext(); 

protected:
    // GAS 핵심 컴포넌트
    // Ability, Effect, Attribute 처리를 담당한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem = nullptr;

    // 유닛 능력치 집합
    UPROPERTY()
    UAS_Unit* AttributeSet = nullptr;

    // 기본 공격 Ability 클래스
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayAbility> DefaultAttackAbilityClass;

    // AI 및 전투 로직에서 사용할 추가 Skill 슬롯
    // 기본 공격 외에 최대 4개의 스킬을 장착하는 구조를 가정한다.
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayAbility>> EquippedSkillAbilityClasses;

public:
    // AI가 평가 가능한 Skill Ability 목록 반환
    UFUNCTION(BlueprintCallable, Category = "Skill")
    virtual TArray<TSubclassOf<UGameplayAbility>> GetAvailableSkillAbilityClasses() const;

protected:
    // 초기 능력치
    UPROPERTY(EditDefaultsOnly, Category = "GAS_Attribute")
    float InitMaxHP = 100.f;

    // 유닛별 기본 AP 최대치
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionPoint")
    int32 MaxActionPoint = 2;

    // 현재 턴에서 남아 있는 AP
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ActionPoint")
    int32 CurrentActionPoint = 0;

    // 유닛별 기본 SAP 최대치
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SubActionPoint")
    int32 MaxSubActionPoint = 1;

    // 현재 턴에서 남아 있는 SAP
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubActionPoint")
    int32 CurrentSubActionPoint = 0;

};