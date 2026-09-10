#include "Game/GameState/GameplayGameState.h"
#include "Combat/CombatManager.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Net/UnrealNetwork.h"

FGameplayViewState FGameplayViewState::FromRun(const URunStateSubsystem* Run, const FText& Message)
{
    FGameplayViewState View;
    View.FlowMessage = Message;
    if (Run)
    {
        View.Phase = Run->GetPhase();
        View.LastResult = Run->GetLastResult();
        View.PartyMembers = Run->GetPartyMembers();
        View.Nodes = Run->GetNodes();
        View.CompletedNodes = Run->GetCompletedNodes();
        for (const FRunNodeDefinition& Node : View.Nodes)
        {
            if (Run->CanStartNode(Node.NodeId))
            {
                View.AvailableNodes.Add(Node.NodeId);
            }
        }
        if (!Run->GetSaveError().IsEmpty())
        {
            View.FlowMessage = Run->GetSaveError();
        }
    }
    return View;
}

void AGameplayGameState::InitializeServerView(AEncounterManager* Encounter, ACombatArena* Arena)
{
    if (!HasAuthority() || !Encounter || !GetGameInstance())
    {
        return;
    }
    if (RunState)
    {
        RunState->OnRunStateChanged.RemoveAll(this);
    }
    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.RemoveAll(this);
    }
    RunState = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    EncounterManager = Encounter;
    CombatManager = Encounter->GetCombatManager();
    CombatArena = Arena;
    RunState->OnRunStateChanged.AddUObject(this, &AGameplayGameState::RefreshServerView);
    EncounterManager->OnFlowChanged.AddUObject(this, &AGameplayGameState::RefreshServerView);
    RefreshServerView();
}

void AGameplayGameState::RefreshServerView()
{
    if (!HasAuthority())
    {
        return;
    }
    ViewState = FGameplayViewState::FromRun(RunState, EncounterManager ? EncounterManager->GetFlowMessage() : FText::GetEmpty());
    ForceNetUpdate();
    OnRep_GameplayView();
}

void AGameplayGameState::OnRep_GameplayView()
{
    OnGameplayViewChanged.Broadcast();
}

void AGameplayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGameplayGameState, ViewState);
    DOREPLIFETIME(AGameplayGameState, CombatManager);
    DOREPLIFETIME(AGameplayGameState, CombatArena);
}

void AGameplayGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (RunState)
    {
        RunState->OnRunStateChanged.RemoveAll(this);
    }
    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.RemoveAll(this);
    }
    OnGameplayViewChanged.Clear();
    Super::EndPlay(EndPlayReason);
}
