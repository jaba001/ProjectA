#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunIdentityTypes.h"
#include "Types/CombatResult.h"
#include "CombatViewTypes.generated.h"

class AUnitBase;

// Actor references belong only to this replicated presentation view, never to persistent Run or command data.
// 액터 참조는 이 복제 표시 뷰에만 속하며 영구 Run 또는 명령 데이터에는 저장하지 않습니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatUnitView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AUnitBase> Unit = nullptr;

    UPROPERTY(BlueprintReadOnly)
    FGuid RuntimeUnitId;

    UPROPERTY(BlueprintReadOnly)
    FGuid CharacterId;

    UPROPERTY(BlueprintReadOnly)
    FRunAccountId OwnerAccountId;
};

// Clients observe a server-produced combat view without constructing or advancing a TurnManager.
// 클라이언트는 TurnManager를 생성하거나 진행하지 않고 서버가 만든 전투 뷰를 관찰합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatViewState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid CombatInstanceId;

    UPROPERTY(BlueprintReadOnly)
    FGuid RunId;

    UPROPERTY(BlueprintReadOnly)
    int32 HostEpoch = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TurnSerial = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentTurnIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    bool bCombatActive = false;

    UPROPERTY(BlueprintReadOnly)
    bool bAwaitingTurnCheckpoint = false;

    UPROPERTY(BlueprintReadOnly)
    bool bSuspendedForRecovery = false;

    UPROPERTY(BlueprintReadOnly)
    ECombatResult CombatResult = ECombatResult::None;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AUnitBase> CurrentUnit = nullptr;

    // The server supplies the display name so PIE actor naming cannot change client HUD text.
    // PIE 액터 이름 차이로 클라이언트 HUD 문구가 달라지지 않도록 서버가 표시 이름을 제공합니다.
    UPROPERTY(BlueprintReadOnly)
    FString CurrentUnitName = TEXT("None");

    UPROPERTY(BlueprintReadOnly)
    TArray<FCombatUnitView> Units;

    // A completed action also refreshes observers when turn identity itself has not changed.
    // 턴 식별자가 그대로여도 행동이 완료되면 관찰자를 갱신합니다.
    UPROPERTY()
    uint32 ViewRevision = 0;
};
