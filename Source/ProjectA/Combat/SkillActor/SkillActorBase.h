#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillActorBase.generated.h"

class ACombatGridTile;
class AUnitBase;
class USceneComponent;
class USkillDefinitionDataAsset;

USTRUCT(BlueprintType)
struct FSkillActorInitData
{
    GENERATED_BODY()

public:
    // Source unit that created this skill actor.
    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<AUnitBase> SourceUnit = nullptr;

    // Skill data used to create this skill actor.
    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<USkillDefinitionDataAsset> SkillData = nullptr;

    // Selected target tile at release time.
    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<ACombatGridTile> TargetTile = nullptr;

    // Cached target world location.
    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    FVector TargetWorldLocation = FVector::ZeroVector;
};

// Base actor for spawned skill objects.
// 스킬 액터 기본 클래스
UCLASS()
class PROJECTA_API ASkillActorBase : public AActor
{
    GENERATED_BODY()

public:
    ASkillActorBase();

public:
    // Initialize this skill actor with runtime skill context.
    // 스킬 액터를 런타임 스킬 컨텍스트로 초기화합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    virtual void InitializeSkillActor(const FSkillActorInitData& InitData);

    // Request impact execution from Blueprint or child logic.
    // 임팩트 실행을 블루프린트나 자식 로직에서 요청합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    void RequestImpact();

    // Finish this skill actor from Blueprint or child logic.
    // 스킬 액터를 블루프린트나 자식 로직에서 종료합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    void RequestFinish();

    // Returns the source unit.
    // 소스 유닛을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    AUnitBase* GetSourceUnit() const;

    // Returns the skill data.
    // 스킬 데이터를 반환합니다    .
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    USkillDefinitionDataAsset* GetSkillData() const;

    // Returns the target tile.
    // 대상 타일을 반환합니다.    
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    ACombatGridTile* GetTargetTile() const;

    // Returns the cached target world location.
    // 대상 월드 위치를 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    FVector GetTargetWorldLocation() const;

    // Returns whether impact was already handled.
    // 임팩트가 이미 처리되었는지 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    bool HasImpactHandled() const;

protected:
    // Called after InitializeSkillActor.
    // InitializeSkillActor 호출 후 실행됩니다.
    virtual void BeginSkillActor();

    // Execute actual impact logic.
    // 실제 임팩트 로직을 실행합니다.
    virtual void HandleImpact();

    // Finishes this skill actor and schedules destruction.
    // 스킬 액터를 종료하고 파괴를 예약합니다.
    virtual void FinishSkillActor();

protected:
    // Root scene component.
    // 루트 씬 컴포넌트.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    // Source unit that created this skill actor.
    // 스킬 액터를 생성한 소스 유닛.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<AUnitBase> SourceUnit = nullptr;

    // Skill data used to create this skill actor.
    // 스킬 데이터를 사용하여 이 스킬 액터를 생성합니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<USkillDefinitionDataAsset> SkillData = nullptr;

    // Selected target tile.
    // 선택된 대상 타일.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor")
    TObjectPtr<ACombatGridTile> TargetTile = nullptr;

    // Cached target world location.
    // 대상 월드 위치를 캐시합니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor")
    FVector TargetWorldLocation = FVector::ZeroVector;

    // Prevent duplicate impact execution.
    // 임팩트 중복 실행 방지.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor")
    bool bImpactHandled = false;

    // Delay before destroying this actor after finish.
    // 종료 후 이 액터를 파괴하기 전의 지연 시간.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillActor")
    float DestroyDelayAfterFinish = 0.3f;
};