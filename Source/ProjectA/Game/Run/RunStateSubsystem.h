#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/CombatResult.h"
#include "RunStateSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnRunStateChanged);

// Owns one in-memory run; encounter actors and UI remain world-scoped.
// 하나의 메모리 내 진행을 소유하며 인카운터 액터와 UI는 월드에 속합니다.
UCLASS()
class PROJECTA_API URunStateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Run")
    bool InitializeRun(const TArray<FRunPartyMember>& Members, FText& OutError);

    UFUNCTION(BlueprintPure, Category = "Run")
    const TArray<FRunPartyMember>& GetPartyMembers() const { return PartyMembers; }

    UFUNCTION(BlueprintPure, Category = "Run")
    const TArray<FRunNodeDefinition>& GetNodes() const { return Nodes; }

    UFUNCTION(BlueprintPure, Category = "Run")
    const TArray<FName>& GetCompletedNodes() const { return CompletedNodes; }

    UFUNCTION(BlueprintPure, Category = "Run")
    ERunPhase GetPhase() const { return Phase; }

    UFUNCTION(BlueprintPure, Category = "Run")
    ECombatResult GetLastResult() const { return LastResult; }

    UFUNCTION(BlueprintPure, Category = "Run")
    FName GetCurrentNodeId() const { return CurrentNodeId; }

    UFUNCTION(BlueprintPure, Category = "Run")
    FName GetCurrentEncounterId() const { return CurrentEncounterId; }

    UFUNCTION(BlueprintPure, Category = "Run")
    bool CanStartNode(FName NodeId) const;

    bool BeginEncounter(FName NodeId);
    bool MarkCombatStarted();
    bool CompleteEncounter(ECombatResult Result);
    bool AbortEncounter();
    bool ContinueRun();
    void UpdatePartyMemberHP(int32 SlotIndex, float CurrentHP);

    FOnRunStateChanged OnRunStateChanged;

private:
    UPROPERTY(Transient)
    TArray<FRunPartyMember> PartyMembers;

    UPROPERTY(Transient)
    TArray<FRunNodeDefinition> Nodes;

    UPROPERTY(Transient)
    TArray<FName> CompletedNodes;

    UPROPERTY(Transient)
    FName CurrentNodeId;

    UPROPERTY(Transient)
    FName CurrentEncounterId;

    UPROPERTY(Transient)
    ERunPhase Phase = ERunPhase::None;

    UPROPERTY(Transient)
    ECombatResult LastResult = ECombatResult::None;
};
