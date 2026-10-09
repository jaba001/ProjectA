#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Game/Run/RunTypes.h"
#include "Game/Run/RunEncounterTypes.h"
#include "Game/Run/RunDungeonTypes.h"
#include "Game/Run/RunSkillShopTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"
#include "Game/Run/RunGoldRewardTypes.h"
#include "Game/Run/RunParticipationTypes.h"
#include "Game/Run/TargetRunTypes.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Types/CombatResult.h"
#include "RunSaveGame.generated.h"

UCLASS()
class PROJECTA_API URunSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 Version = 2;
    UPROPERTY()
    FRunIdentityData Identity;
    UPROPERTY()
    FRunParticipationData Participation;
    UPROPERTY()
    TArray<FRunPartyMember> Party;
    UPROPERTY()
    TArray<FRunNodeDefinition> Nodes;
    UPROPERTY()
    TArray<FName> CompletedNodes;
    UPROPERTY()
    FName CurrentNode;
    UPROPERTY()
    FName CurrentEncounter;
    UPROPERTY()
    ERunPhase Phase = ERunPhase::None;
    UPROPERTY()
    ECombatResult Result = ECombatResult::None;
    UPROPERTY()
    FSoftObjectPath Catalog;
    UPROPERTY()
    FCombatCheckpointData CombatCheckpoint;
    UPROPERTY()
    FRunEncounterProgress EncounterProgress;
    // Missing dungeon data preserves the fixed layout of existing saves.
    // 던전 데이터가 없는 기존 저장은 고정 배치를 유지합니다.
    UPROPERTY()
    FRunDungeonState DungeonState;
    UPROPERTY()
    FRunSkillShopState SkillShopState;
    UPROPERTY()
    FRunItemShopState ItemShopState;
    // Default to the original acquisition policy when loading an existing Run.
    // 기존 Run을 불러올 때 기본값은 원래 획득 정책을 유지합니다.
    UPROPERTY()
    int32 WeaponSkillAcquisitionVersion = 0;
    UPROPERTY()
    FRunWeaponSkillRulesState WeaponSkillRules;
    UPROPERTY()
    FRunGoldRewardState GoldRewardState;
    UPROPERTY()
    FRunTargetState TargetRun;
};
