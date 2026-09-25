#include "Game/GameState/GameplayGameState.h"
#include "Combat/CombatManager.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Game/Development/DevelopmentCoopLobby.h"

void AGameplayGameState::SetDevelopmentLobby(ADevelopmentCoopLobby* Lobby)
{
    if (!HasAuthority()) return;
    DevelopmentLobby = Lobby;
    ForceNetUpdate();
    OnRep_GameplayView();
}

FGameplayViewState FGameplayViewState::FromRun(const URunStateSubsystem* Run, const FText& Message)
{
    FGameplayViewState View;
    View.FlowMessage = Message;
    if (Run)
    {
        View.Phase = Run->GetPhase();
        View.LastResult = Run->GetLastResult();
        View.ConfirmedCombatRevision = Run->GetCombatCheckpoint().Revision;
        View.PartyMembers = Run->GetPartyMembers();
        View.Nodes = Run->GetNodes();
        View.CompletedNodes = Run->GetCompletedNodes();
        View.EncounterProgress = Run->GetEncounterProgress();
        View.SkillShopState = Run->GetSkillShopState();
        View.ItemShopState = Run->GetItemShopState();
        // Replicate only the visible stock; the frozen candidate catalog remains on the server.
        // 표시 중인 재고만 복제하고 고정된 후보 카탈로그는 서버에 보관합니다.
        View.ItemShopState.Catalog.Reset();
        View.GoldRewardState = Run->GetGoldRewardState();
        View.GoldRewardRecipientIds = Run->GetGoldRewardRecipientIds();
        View.bCanContinueAfterRewards = Run->CanContinueAfterRewards();
        const UPartyDefinitionDataAsset* Catalog = Run->PartyDefinition ? Run->PartyDefinition.Get() : GetDefault<UPartyDefinitionDataAsset>();
        const bool bOrdinarySinglePlayer = !Run->IsManagedRun() && Run->GetRunIdentity().Origin == ERunIdentityOrigin::LocalDevelopment && Run->GetRunIdentity().OriginalParticipants.Num() == 1;
        for (const FRunPartyMember& Member : View.PartyMembers)
        {
            if (!Member.bCreated || Member.CurrentHP <= 0.f || !Member.CharacterId.IsValid() || Member.OwnerAccountId.IsEmpty()) continue;
            if (!(bOrdinarySinglePlayer ? Member.bPlayerControlled : !Run->IsManagedRun() || Run->GetParticipation().HumanParticipants.Contains(Member.OwnerAccountId))) continue;
            View.ShopBuyerCharacterIds.Add(Member.CharacterId);
            FProfessionDefinition Profession;
            if (Catalog->ResolveProfession(Member.ClassId, Profession) && FMath::IsFinite(Profession.MaxHP) && Profession.MaxHP > 0.f)
            {
                FRunShopBuyerView& BuyerView = View.ShopBuyerViews.AddDefaulted_GetRef();
                BuyerView.CharacterId = Member.CharacterId;
                BuyerView.MaxHP = Profession.MaxHP;
            }
        }
        for (const FRunNodeDefinition& Node : View.Nodes)
        {
            if (Run->CanStartNode(Node.NodeId))
            {
                View.AvailableNodes.Add(Node.NodeId);
            }
        }
        if (!Run->GetSaveError().IsEmpty())
        {
            // Preserve the preparation reason in both local UI and the replicated presentation.
            // 로컬 UI와 복제된 표시 데이터 모두에서 준비 실패 원인을 보존합니다.
            if (View.FlowMessage.IsEmpty()) View.FlowMessage = Run->GetSaveError();
            else if (!View.FlowMessage.ToString().Contains(Run->GetSaveError().ToString())) View.FlowMessage = FText::Format(FText::FromString(TEXT("{0}\n{1}")), View.FlowMessage, Run->GetSaveError());
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
    DOREPLIFETIME(AGameplayGameState, DevelopmentLobby);
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
