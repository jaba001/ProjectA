#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"
#include "Game/Run/RunEncounterTypes.h"
#include "Game/Run/RunDungeonTypes.h"
#include "Game/Run/RunSkillShopTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"
#include "Game/Run/RunGoldRewardTypes.h"
#include "Types/CombatResult.h"
#include "GameplayViewTypes.generated.h"

class URunStateSubsystem;

USTRUCT()
struct PROJECTA_API FRunShopBuyerView
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid CharacterId;

    UPROPERTY()
    float MaxHP = 0.f;
};

// Read-only presentation values; clients never restore this projection into an authoritative Run.
// 읽기 전용 표시 값이며 클라이언트는 이 뷰를 권위 Run으로 복원하지 않습니다.
USTRUCT()
struct PROJECTA_API FGameplayViewState
{
    GENERATED_BODY()

    UPROPERTY()
    ERunPhase Phase = ERunPhase::None;

    UPROPERTY()
    ECombatResult LastResult = ECombatResult::None;

    UPROPERTY()
    FText FlowMessage;

    UPROPERTY()
    int64 ConfirmedCombatRevision = 0;

    UPROPERTY()
    TArray<FRunPartyMember> PartyMembers;

    UPROPERTY()
    TArray<FRunNodeDefinition> Nodes;

    UPROPERTY()
    TArray<FName> CompletedNodes;

    UPROPERTY()
    TArray<FName> AvailableNodes;

    UPROPERTY()
    FRunEncounterProgress EncounterProgress;

    // Replicate the frozen presentation plan without granting clients progression authority.
    // 진행 권한을 클라이언트에 부여하지 않고 고정된 표시 계획을 복제합니다.
    UPROPERTY()
    FRunDungeonState DungeonState;

    UPROPERTY()
    FRunSkillShopState SkillShopState;

    UPROPERTY()
    FRunItemShopState ItemShopState;

    UPROPERTY()
    bool bCanRerollItemShop = false;

    UPROPERTY()
    TArray<FRunWeaponRarityRule> ItemRarities;

    UPROPERTY()
    FRunRecoveryState RecoveryState;

    UPROPERTY()
    int32 TargetCompletedSteps = 0;

    UPROPERTY()
    bool bTargetRun = false;

    UPROPERTY()
    FRunGoldRewardState GoldRewardState;

    // Show the completed party rest independently of the unclaimed item choice.
    // 아직 고르지 않은 아이템 보상과 별도로 완료된 파티 휴식을 표시합니다.
    UPROPERTY()
    float VictoryRestHP = 0.0f;

    UPROPERTY()
    TArray<FGuid> GoldRewardRecipientIds;

    UPROPERTY()
    bool bCanContinueAfterRewards = false;

    UPROPERTY()
    TArray<FGuid> ShopBuyerCharacterIds;

    UPROPERTY()
    TArray<FRunShopBuyerView> ShopBuyerViews;

    UPROPERTY()
    TArray<FGuid> EquipmentEditableCharacterIds;

    static FGameplayViewState FromRun(const URunStateSubsystem* Run, const FText& Message);
};
