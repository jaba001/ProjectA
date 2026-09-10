#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"
#include "Types/CombatResult.h"
#include "GameplayViewTypes.generated.h"

class URunStateSubsystem;

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
    TArray<FRunPartyMember> PartyMembers;

    UPROPERTY()
    TArray<FRunNodeDefinition> Nodes;

    UPROPERTY()
    TArray<FName> CompletedNodes;

    UPROPERTY()
    TArray<FName> AvailableNodes;

    static FGameplayViewState FromRun(const URunStateSubsystem* Run, const FText& Message);
};
