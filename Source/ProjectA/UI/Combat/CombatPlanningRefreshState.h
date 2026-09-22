#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "GameplayTagContainer.h"
#include "CombatPlanningRefreshState.generated.h"

class ACombatRoundPlayerController;
class ACombatRoundCoordinator;
class ACombatGridTile;

struct FCombatPlanningUnitObservation
{
    TWeakObjectPtr<AUnitBase> Unit;
    TWeakObjectPtr<UObject> AbilitySystem;
    TWeakObjectPtr<ACombatGridTile> CurrentTile;
    FGameplayTagContainer Tags;
    FGameplayTagContainer BlockedAbilityTags;
    FString Name;
    int32 AP = 0;
    int32 SAP = 0;
    int32 MoveRange = 0;
    bool bAlive = false;

    bool operator==(const FCombatPlanningUnitObservation& Other) const;
};

struct FCombatPlanningTileObservation
{
    FIntPoint Coord = FIntPoint::ZeroValue;
    FIntPoint TileCoord = FIntPoint::ZeroValue;
    TWeakObjectPtr<ACombatGridTile> Tile;
    TWeakObjectPtr<AUnitBase> Occupant;
    uint8 Territory = 0;
    bool bProtected = false;
    bool bValid = false;

    bool operator==(const FCombatPlanningTileObservation& Other) const;
};

// Observe replicated data and live query inputs without running validation or changing widgets.
// 검증이나 위젯 변경 없이 복제 데이터와 실제 조회 입력의 변경을 관찰합니다.
USTRUCT()
struct PROJECTA_API FCombatPlanningRefreshState
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    FCombatRoundView View;

    UPROPERTY(Transient)
    TArray<FCombatRoundSkill> Skills;

    TArray<FCombatPlanningUnitObservation> Units;
    TArray<FCombatPlanningTileObservation> Tiles;
    TWeakObjectPtr<ACombatRoundPlayerController> Controller;
    TWeakObjectPtr<ACombatRoundCoordinator> Coordinator;
    TWeakObjectPtr<UObject> Arena;
    TWeakObjectPtr<UObject> Grid;
    FString RequestStatus;
    FString LocalStatus;
    int32 OwnerSlot = 0;
    int32 SelectedUnit = INDEX_NONE;
    int32 SelectedTarget = INDEX_NONE;
    FIntPoint TargetCoord = FIntPoint::ZeroValue;
    bool bActivated = false;
    bool bInputEnabled = false;
    bool bRequestPending = false;
    bool bSAPMovement = false;
    bool bChoosingMove = false;
    bool bHasTarget = false;

    bool operator==(const FCombatPlanningRefreshState& Other) const;
};
