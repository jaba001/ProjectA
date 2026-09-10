#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/CombatResult.h"
#include "RunStateSubsystem.generated.h"

class UPartyDefinitionDataAsset;
class URunSaveGame;

DECLARE_MULTICAST_DELEGATE(FOnRunStateChanged);

// Owns one in-memory run; encounter actors and UI remain world-scoped.
// 하나의 메모리 내 진행을 소유하며 인카운터 액터와 UI는 월드에 속합니다.
UCLASS()
class PROJECTA_API URunStateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    URunStateSubsystem();
    // Resolve the checkpoint before menu initialization so local Snapshot runs cannot overwrite PvE progress.
    // 로컬 Snapshot 진행이 PvE 기록을 덮어쓰지 않도록 메뉴 초기화 전에 체크포인트를 결정합니다.
    static FString ResolveCheckpointSlot(const TCHAR* CommandLine);
    // Keep the selected catalog across travel so previews and spawning use the same data.
    // 미리보기와 스폰이 같은 데이터를 쓰도록 선택한 목록을 레벨 이동 동안 유지합니다.
    UPROPERTY(Transient)
    TObjectPtr<UPartyDefinitionDataAsset> PartyDefinition;

    UFUNCTION(BlueprintCallable, Category = "Run")
    bool InitializeRun(const TArray<FRunPartyMember>& Members, FText& OutError);

    // Accept validated data from a future authority layer; this API does not authenticate accounts.
    // 향후 권위 계층의 데이터를 검증해 받으며 이 API 자체는 계정을 인증하지 않습니다.
    bool InitializeRunWithIdentity(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError);

    UFUNCTION(BlueprintPure, Category = "Run|Identity")
    const FRunIdentityData& GetRunIdentity() const { return RunIdentity; }

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

    bool SaveCheckpoint(FText& OutError);
    bool LoadCheckpoint(FText& OutError);
    bool CanContinueSavedRun(FText& OutError) const;
    // The menu opens Standalone and cannot bind accounts from a cooperative or authenticated session.
    // 메뉴는 Standalone으로 열리며 협동 또는 인증 세션의 계정을 연결할 수 없습니다.
    bool CanContinueStandaloneSavedRun(FText& OutError) const;
    bool LoadStandaloneCheckpoint(FText& OutError);
    void EnableCheckpointSaving(const FString& Slot = FString());
    const FText& GetSaveError() const { return SaveError; }
    bool CommitCombatCheckpoint(const FCombatCheckpointData& Checkpoint, FText& OutError);
    const FCombatCheckpointData& GetCombatCheckpoint() const { return CombatCheckpoint; }
    bool HasCombatCheckpoint() const { return CombatCheckpoint.Revision > 0; }
    bool IsCheckpointSavingEnabled() const { return bCheckpointSaving; }
    bool ValidateCheckpointHost(const FRunAccountId& AccountId, FText& OutError) const;

private:
    bool ValidateSave(const URunSaveGame* Save, FText& OutError) const;
    bool ValidateContinuableSave(const URunSaveGame* Save, bool bStandaloneOnly, FText& OutError) const;
    bool CanContinueSavedRunInternal(bool bStandaloneOnly, FText& OutError) const;
    bool LoadCheckpointInternal(bool bStandaloneOnly, FText& OutError);
    URunSaveGame* CreateSaveData() const;
    void AutoSaveCheckpoint();
    FString SaveSlot = TEXT("ProjectA_Run");
    bool bCheckpointSaving = false;
    FText SaveError;

    UPROPERTY(Transient)
    FCombatCheckpointData CombatCheckpoint;

    UPROPERTY(Transient)
    FRunIdentityData RunIdentity;

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
