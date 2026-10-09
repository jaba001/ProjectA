#include "Game/GameState/GameplayGameState.h"
#include "Combat/CombatManager.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Run/RunItemShopCatalog.h"
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
        View.DungeonState = Run->GetDungeonState();
        View.SkillShopState = Run->GetSkillShopState();
        // Replicate displayed stock while keeping selection catalogs and queries on the authority.
        // 진열 상품만 복제하고 후보 카탈로그와 선택 조건은 권위 측에 유지합니다.
        View.SkillShopState.Catalog.Reset();
        View.SkillShopState.Query = FGameplayTagQuery();
        View.ItemShopState = Run->GetItemShopState();
        const FRunWeaponSkillRulesState* FrozenWeaponRules = Run->GetWeaponSkillRules().SchemaVersion == 1 ? &Run->GetWeaponSkillRules() : nullptr;
        View.bCanRerollItemShop = View.Phase == ERunPhase::Shop && View.EncounterProgress.IsItemShop() && RunItemShopCatalog::CanReroll(Run->GetItemShopState(), FrozenWeaponRules);
        View.ItemRarities = Run->GetWeaponSkillRules().Rarities;
        View.RecoveryState = Run->GetRecoveryState();
        View.bTargetRun = Run->IsTargetRun();
        View.TargetCompletedSteps = Run->GetCompletedNodes().Num() + Run->GetTargetRunState().CompletedEncounterChoices.Num();
        // Replicate only the visible stock; the frozen candidate catalog remains on the server.
        // 표시 중인 재고만 복제하고 고정된 후보 카탈로그는 서버에 보관합니다.
        View.ItemShopState.Catalog.Reset();
        View.GoldRewardState = Run->GetGoldRewardState();
        const FRunLevelDesignState& LevelDesign = Run->GetTargetRunState().LevelDesign;
        const int32 CompletedCount = View.CompletedNodes.Num();
        if (View.Phase == ERunPhase::Result && View.LastResult == ECombatResult::Victory && CompletedCount % 2 == 1 && LevelDesign.SchemaVersion == 1 && LevelDesign.Rules.IsValidIndex(CompletedCount / 2)) View.VictoryRestHP = LevelDesign.Rules[CompletedCount / 2].RestHP;
        View.GoldRewardRecipientIds = Run->GetGoldRewardRecipientIds();
        View.bCanContinueAfterRewards = Run->CanContinueAfterRewards();
        const bool bOrdinarySinglePlayer = !Run->IsManagedRun() && Run->GetRunIdentity().Origin == ERunIdentityOrigin::LocalDevelopment && Run->GetRunIdentity().OriginalParticipants.Num() == 1;
        const FRunEncounterOffer* Selected = View.EncounterProgress.FindSelectedOffer();
        const bool bRevival = View.bTargetRun && Selected && Selected->GetResolvedTag().MatchesTag(FRunEncounterOffer::GetRevivalTag());
        for (const FRunPartyMember& Member : View.PartyMembers)
        {
            FText EquipmentError;
            if (Run->CanChangeEquipment(Member.OwnerAccountId, Member.CharacterId, EquipmentError)) View.EquipmentEditableCharacterIds.Add(Member.CharacterId);
            if (!Member.bCreated || (bRevival ? Member.CurrentHP != 0.f : Member.CurrentHP <= 0.f) || !Member.CharacterId.IsValid() || Member.OwnerAccountId.IsEmpty()) continue;
            if (!(bOrdinarySinglePlayer ? Member.bPlayerControlled : !Run->IsManagedRun() || Run->GetParticipation().HumanParticipants.Contains(Member.OwnerAccountId))) continue;
            View.ShopBuyerCharacterIds.Add(Member.CharacterId);
            FProfessionDefinition Profession;
            FText ProfessionError;
            if (Run->ResolveMemberProfession(Member, Profession, ProfessionError) && FMath::IsFinite(Profession.MaxHP) && Profession.MaxHP > 0.f)
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
