#include "Controller/GameplayPlayerController.h"

#include "Combat/CombatManager.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Game/Encounter/EncounterDungeonRoute.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunDungeonPlan.h"
#include "UI/Gameplay/GameplayRootWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Game/Development/DevelopmentCoopLobby.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "Net/UnrealNetwork.h"

void AGameplayPlayerController::ServerSetDevelopmentReady_Implementation(bool bReady)
{
    if (!UDevelopmentCoopSubsystem::IsAvailable()) return;
    AGameplayGameState* State = GetWorld()->GetGameState<AGameplayGameState>();
    if (State && State->GetDevelopmentLobby()) State->GetDevelopmentLobby()->SetReady(this, bReady);
}

void AGameplayPlayerController::RequestStartDevelopmentCoop()
{
    if (!HasAuthority() || !IsLocalController() || !UDevelopmentCoopSubsystem::IsAvailable()) return;
    AGameplayGameState* State = GetWorld()->GetGameState<AGameplayGameState>();
    if (State && State->GetDevelopmentLobby()) State->GetDevelopmentLobby()->Start(this);
}

AGameplayPlayerController::AGameplayPlayerController()
{
    bAutoManageActiveCameraTarget = false;
    GameplayRootWidgetClass = UGameplayRootWidget::StaticClass();
}

void AGameplayPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // Initial replication can precede client BeginPlay; keep the server's received context intact.
    // 초기 복제는 클라이언트 BeginPlay보다 먼저 올 수 있으므로 수신한 서버 문맥을 유지합니다.
    if (HasAuthority())
    {
        SetCombatContext(nullptr, false);
    }

    if (!IsLocalController())
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        // Clear the legacy UIOnly viewport lock carried across travel from MainMenu.
        // MainMenu의 기존 UIOnly 설정에서 레벨 이동 후 남은 뷰포트 입력 잠금을 해제합니다.
        if (UGameViewportClient* ViewportClient = GameInstance->GetGameViewportClient())
        {
            ViewportClient->SetIgnoreInput(false);
        }

        if (HasAuthority())
        {
            RunState = GameInstance->GetSubsystem<URunStateSubsystem>();
        }
    }

    if (RunState)
    {
        RunState->OnRunStateChanged.AddUObject(this, &AGameplayPlayerController::RefreshGameplayFlow);
    }

    if (!GameplayRootWidgetClass)
    {
        GameplayRootWidgetClass = UGameplayRootWidget::StaticClass();
    }

    GameplayRootWidget = CreateWidget<UGameplayRootWidget>(this, GameplayRootWidgetClass);

    if (GameplayRootWidget)
    {
        GameplayRootWidget->AddToViewport();
    }

    TryBindGameplayState();
    if (!GameplayState)
    {
        GetWorldTimerManager().SetTimer(BindStateTimer, this, &AGameplayPlayerController::TryBindGameplayState, 0.1f, true);
    }
    RefreshGameplayFlow();

    // Restore this player's viewport focus after travel so CommonUI can apply the active screen's input config.
    // 레벨 이동 후 이 플레이어의 뷰포트 포커스를 복원하여 CommonUI가 활성 화면의 입력 설정을 적용하도록 합니다.
    if (FSlateApplication::IsInitialized())
    {
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (TSharedPtr<FSlateUser> SlateUser = LocalPlayer->GetSlateUser())
            {
                FSlateApplication::Get().SetUserFocusToGameViewport(SlateUser->GetUserIndex(), EFocusCause::SetDirectly);
            }
        }
    }
}

void AGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bPresentationEnding = true;
    ResetEncounterPresentation();
    if (DungeonRoute.IsValid()) DungeonRoute->Destroy();
    if (PreparedDungeonRoute.IsValid()) PreparedDungeonRoute->Destroy();
    DungeonRoute.Reset();
    PreparedDungeonRoute.Reset();
    GetWorldTimerManager().ClearTimer(BindStateTimer);
    if (GameplayState)
    {
        GameplayState->OnGameplayViewChanged.RemoveAll(this);
    }
    if (RunState)
    {
        RunState->OnRunStateChanged.RemoveAll(this);
    }

    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.RemoveAll(this);
    }

    if (GameplayRootWidget)
    {
        GameplayRootWidget->RemoveFromParent();
    }

    Super::EndPlay(EndPlayReason);
}

void AGameplayPlayerController::InitializeGameplay(AEncounterManager* InEncounterManager)
{
    if (HasAuthority() && GetGameInstance())
    {
        RunState = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    }
    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.RemoveAll(this);
    }

    EncounterManager = InEncounterManager;

    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.AddUObject(this, &AGameplayPlayerController::RefreshGameplayFlow);
    }

    RefreshRunFlowPermissions();
}

void AGameplayPlayerController::RequestStartNode(FName NodeId, FGameplayTag DifficultyTag)
{
    if (CanIssueRunCommands() && EncounterManager)
    {
        EncounterManager->RequestStartNode(NodeId, DifficultyTag);
    }
}

void AGameplayPlayerController::RequestContinueRun()
{
    if (CanIssueRunCommands() && EncounterManager)
    {
        EncounterManager->ContinueRun();
    }
}

bool AGameplayPlayerController::CanIssueRunCommands() const
{
    if (!HasAuthority() || !IsLocalController())
    {
        return false;
    }
    const URunStateSubsystem* CurrentRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (GetNetMode() == NM_Standalone && (!CurrentRun || !CurrentRun->IsManagedRun()))
    {
        return true;
    }
    // Recheck the current Run host against the server's trusted connection assignment for every request.
    // 요청마다 현재 Run Host와 서버가 신뢰 배정한 연결을 다시 대조합니다.
    const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
    return Mode && Mode->CanControlRunFlow(this);
}

void AGameplayPlayerController::RequestSelectRunEncounter(FName EncounterId)
{
    if (CanIssueRunCommands() && EncounterManager) EncounterManager->SelectRunEncounter(EncounterId);
}

void AGameplayPlayerController::RequestLeaveRunEncounter()
{
    if (CanIssueRunCommands() && EncounterManager) EncounterManager->LeaveRunEncounter();
}

void AGameplayPlayerController::RequestToggleInventory()
{
    if (IsLocalController() && GameplayRootWidget) GameplayRootWidget->ToggleInventory();
}

void AGameplayPlayerController::RefreshRunFlowPermissions()
{
    if (HasAuthority())
    {
        const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
        FRunAccountId AccountId;
        if (Mode) Mode->ResolveRunParticipant(this, AccountId);
        if (RunParticipantAccount != AccountId)
        {
            RunParticipantAccount = AccountId;
            ForceNetUpdate();
        }
    }
    RefreshGameplayFlow();
}

void AGameplayPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AGameplayPlayerController, RunParticipantAccount, COND_OwnerOnly);
}

void AGameplayPlayerController::OnRep_RunParticipantAccount()
{
    RefreshGameplayFlow();
}

FGuid AGameplayPlayerController::GetShopBuyerCharacterId(const FGameplayViewState& View) const
{
    if (!IsLocalController() || View.Phase != ERunPhase::Shop || RunParticipantAccount.IsEmpty()) return FGuid();
    const FRunPartyMember* Member = View.PartyMembers.FindByPredicate([this, &View](const FRunPartyMember& Candidate) { return Candidate.OwnerAccountId == RunParticipantAccount && View.ShopBuyerCharacterIds.Contains(Candidate.CharacterId); });
    return Member ? Member->CharacterId : FGuid();
}

FGuid AGameplayPlayerController::GetInventoryCharacterId(const FGameplayViewState& View) const
{
    if (!IsLocalController() || RunParticipantAccount.IsEmpty()) return FGuid();
    // Inventory remains readable after death; shop eligibility is intentionally separate.
    // 사망 후에도 인벤토리는 조회할 수 있으며 상점 구매 가능 여부와 분리합니다.
    const FRunPartyMember* Member = View.PartyMembers.FindByPredicate([this](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.OwnerAccountId == RunParticipantAccount && Candidate.bPlayerControlled; });
    if (!Member) Member = View.PartyMembers.FindByPredicate([this](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.OwnerAccountId == RunParticipantAccount; });
    return Member ? Member->CharacterId : FGuid();
}

FGuid AGameplayPlayerController::GetRewardCharacterId(const FGameplayViewState& View) const
{
    if (!IsLocalController() || View.Phase != ERunPhase::Result || RunParticipantAccount.IsEmpty()) return FGuid();
    FGuid FirstCharacterId;
    for (const FRunPartyMember& Member : View.PartyMembers)
    {
        if (!Member.bCreated || Member.OwnerAccountId != RunParticipantAccount || !View.GoldRewardRecipientIds.Contains(Member.CharacterId)) continue;
        if (!FirstCharacterId.IsValid()) FirstCharacterId = Member.CharacterId;
        // Offer each owned character its pending reward before returning to an already completed claim.
        // 이미 수령한 캐릭터로 돌아가기 전에 본인 소유 캐릭터의 미수령 보상을 차례로 표시합니다.
        if (!View.GoldRewardState.Claims.ContainsByPredicate([&Member](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == Member.CharacterId; })) return Member.CharacterId;
    }
    return FirstCharacterId;
}

bool AGameplayPlayerController::IsRoundInputEnabled() const
{
    return Super::IsRoundInputEnabled() && (!GameplayRootWidget || !GameplayRootWidget->IsUtilityMenuOpen());
}

void AGameplayPlayerController::RequestPurchaseShopOffer(FGuid CharacterId, FName OfferId, int32 ExpectedShopRevision)
{
    if (!IsLocalController() || bShopPurchasePending || !CharacterId.IsValid() || OfferId.IsNone()) return;
    ShopPurchaseMessage = FText::GetEmpty();
    bShopPurchasePending = true;
    bPendingItemShop = HasAuthority() && RunState ? RunState->GetEncounterProgress().IsItemShop() : GameplayState && GameplayState->GetViewState().EncounterProgress.IsItemShop();
    const FRunEncounterOffer* Selected = HasAuthority() && RunState ? RunState->GetEncounterProgress().FindSelectedOffer() : GameplayState ? GameplayState->GetViewState().EncounterProgress.FindSelectedOffer() : nullptr;
    bPendingService = Selected && Selected->IsService();
    PendingShopRevision = INDEX_NONE;
    RefreshGameplayFlow();
    if (HasAuthority()) ExecuteShopPurchase(CharacterId, OfferId, ExpectedShopRevision);
    else ServerPurchaseShopOffer(CharacterId, OfferId, ExpectedShopRevision);
}

void AGameplayPlayerController::ServerPurchaseShopOffer_Implementation(FGuid CharacterId, FName OfferId, int32 ExpectedShopRevision)
{
    ExecuteShopPurchase(CharacterId, OfferId, ExpectedShopRevision);
}

void AGameplayPlayerController::ExecuteShopPurchase(FGuid CharacterId, FName OfferId, int32 ExpectedShopRevision)
{
    if (!HasAuthority()) return;
    const AGameplayGameState* State = GetWorld() ? GetWorld()->GetGameState<AGameplayGameState>() : nullptr;
    const ADevelopmentCoopLobby* Lobby = State ? State->GetDevelopmentLobby() : nullptr;
    if (Lobby && (!Lobby->HasStarted() || Lobby->GetMembers().ContainsByPredicate([](const FDevelopmentCoopMember& Member) { return !Member.bConnected; })))
    {
        ClientReceiveShopPurchaseResult(false, NSLOCTEXT("RunSkillShop", "DisconnectedBuyer", "협동 참가자의 연결 상태를 확인해 주세요."), INDEX_NONE);
        return;
    }
    FText Error = NSLOCTEXT("RunSkillShop", "UnboundBuyer", "현재 연결에 배정된 직접 조작 캐릭터만 구매할 수 있습니다.");
    const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
    const URunStateSubsystem* CurrentRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const bool bItemShop = CurrentRun && CurrentRun->GetEncounterProgress().IsItemShop();
    const FRunEncounterOffer* Selected = CurrentRun ? CurrentRun->GetEncounterProgress().FindSelectedOffer() : nullptr;
    const bool bService = Selected && Selected->IsService();
    const FGameplayTag ServiceTag = bService ? Selected->GetResolvedTag() : FGameplayTag();
    FRunAccountId BuyerAccountId;
    bool bSucceeded = false;
    if (Mode && Mode->ResolveRunParticipant(this, BuyerAccountId))
    {
        AEncounterManager* Manager = Mode->GetEncounterManager();
        if (Manager) bSucceeded = Manager->PurchaseShopOffer(BuyerAccountId, CharacterId, OfferId, Error, ExpectedShopRevision);
    }
    if (bSucceeded)
    {
        if (bService)
        {
            Error = ServiceTag.MatchesTag(FRunEncounterOffer::GetConsumableShopTag()) ? NSLOCTEXT("RunRecovery", "ConsumablePurchased", "회복 소모품 1개를 구매하고 진행을 저장했습니다.") : ServiceTag.MatchesTag(FRunEncounterOffer::GetRevivalTag()) ? NSLOCTEXT("RunRecovery", "RevivalPurchased", "캐릭터를 부활시키고 진행을 저장했습니다.") : NSLOCTEXT("RunRecovery", "Purchased", "회복 서비스를 구매하고 진행을 저장했습니다.");
        }
        else if (bItemShop) Error = OfferId == FRunItemShopState::GetRerollOfferId() ? NSLOCTEXT("RunItemShop", "Rerolled", "아이템 상점의 상품을 다시 추첨했습니다.") : NSLOCTEXT("RunItemShop", "PurchasedInventory", "아이템을 구매해 인벤토리에 보관했습니다. 인벤토리(I)를 열어 장비 슬롯으로 드래그하여 장착하세요.");
        else if (OfferId == FRunSkillShopState::GetRerollOfferId()) Error = FText::Format(NSLOCTEXT("RunSkillShop", "RerolledCount", "스킬 상품 {0}개를 다시 추첨했습니다. 다음 리롤 비용이 1G 증가했습니다."), FText::AsNumber(CurrentRun->GetSkillShopState().Offers.Num()));
        else Error = OfferId == FRunSkillShopState::GetRecoveryOfferId() ? NSLOCTEXT("RunSkillShop", "Recovered", "HP를 회복했습니다.") : NSLOCTEXT("RunSkillShop", "Purchased", "스킬을 구매했습니다. 다음 전투부터 사용할 수 있습니다.");
    }
    const int32 ConfirmedShopRevision = bSucceeded && CurrentRun ? (bService ? CurrentRun->GetRecoveryState().Revision : bItemShop ? CurrentRun->GetItemShopState().Revision : CurrentRun->GetSkillShopState().Revision) : INDEX_NONE;
    ClientReceiveShopPurchaseResult(bSucceeded, Error, ConfirmedShopRevision);
}

void AGameplayPlayerController::ClientReceiveShopPurchaseResult_Implementation(bool bSucceeded, const FText& Message, int32 ConfirmedShopRevision)
{
    if (!bShopPurchasePending) return;
    PendingShopRevision = bSucceeded ? ConfirmedShopRevision : INDEX_NONE;
    bShopPurchasePending = PendingShopRevision != INDEX_NONE;
    ShopPurchaseMessage = Message;
    RefreshGameplayFlow();
}

bool AGameplayPlayerController::CanChangeEquipment(const FGameplayViewState& View, FGuid CharacterId) const
{
    if (!IsLocalController() || View.Phase != ERunPhase::Shop || RunParticipantAccount.IsEmpty() || bEquipmentChangePending || !View.EquipmentEditableCharacterIds.Contains(CharacterId)) return false;
    return View.PartyMembers.ContainsByPredicate([this, CharacterId](const FRunPartyMember& Member) { return Member.bCreated && Member.CharacterId == CharacterId && Member.OwnerAccountId == RunParticipantAccount; });
}

void AGameplayPlayerController::RequestChangeEquipment(const FRunEquipmentCommand& Command)
{
    if (!IsLocalController() || bEquipmentChangePending || !Command.CharacterId.IsValid() || Command.ItemIndex < 0) return;
    EquipmentMessage = FText::GetEmpty();
    PendingEquipmentCharacterId = Command.CharacterId;
    PendingEquipmentRevision = INDEX_NONE;
    bEquipmentChangePending = true;
    RefreshGameplayFlow();
    if (HasAuthority()) ExecuteEquipmentChange(Command);
    else ServerChangeEquipment(Command);
}

void AGameplayPlayerController::ServerChangeEquipment_Implementation(const FRunEquipmentCommand& Command)
{
    ExecuteEquipmentChange(Command);
}

void AGameplayPlayerController::ExecuteEquipmentChange(const FRunEquipmentCommand& Command)
{
    if (!HasAuthority()) return;
    const AGameplayGameState* State = GetWorld() ? GetWorld()->GetGameState<AGameplayGameState>() : nullptr;
    const ADevelopmentCoopLobby* Lobby = State ? State->GetDevelopmentLobby() : nullptr;
    if (Lobby && (!Lobby->HasStarted() || Lobby->GetMembers().ContainsByPredicate([](const FDevelopmentCoopMember& Member) { return !Member.bConnected; })))
    {
        ClientReceiveEquipmentResult(Command.CharacterId, false, NSLOCTEXT("RunEquipment", "Disconnected", "협동 참가자의 연결 상태를 확인해 주세요."), INDEX_NONE);
        return;
    }
    FText Error = NSLOCTEXT("RunEquipment", "UnboundOwner", "현재 연결에 배정된 캐릭터의 장비만 변경할 수 있습니다.");
    const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
    FRunAccountId AccountId;
    bool bSucceeded = false;
    if (Mode && Mode->ResolveRunParticipant(this, AccountId))
    {
        if (AEncounterManager* Manager = Mode->GetEncounterManager()) bSucceeded = Manager->ChangeEquipment(AccountId, Command, Error);
    }
    const URunStateSubsystem* CurrentRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const FRunPartyMember* Member = CurrentRun ? CurrentRun->GetPartyMembers().FindByPredicate([&Command](const FRunPartyMember& Entry) { return Entry.CharacterId == Command.CharacterId; }) : nullptr;
    if (bSucceeded) Error = NSLOCTEXT("RunEquipment", "Changed", "장비 구성을 저장했습니다.");
    ClientReceiveEquipmentResult(Command.CharacterId, bSucceeded, Error, bSucceeded && Member ? Member->Equipment.Revision : INDEX_NONE);
}

void AGameplayPlayerController::ClientReceiveEquipmentResult_Implementation(FGuid CharacterId, bool bSucceeded, const FText& Message, int32 ConfirmedRevision)
{
    if (!bEquipmentChangePending || PendingEquipmentCharacterId != CharacterId) return;
    PendingEquipmentRevision = bSucceeded ? ConfirmedRevision : INDEX_NONE;
    bEquipmentChangePending = PendingEquipmentRevision != INDEX_NONE;
    EquipmentMessage = Message;
    RefreshGameplayFlow();
}

void AGameplayPlayerController::RequestSelectGoldReward(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex)
{
    if (!IsLocalController() || bRewardSelectionPending || !CharacterId.IsValid() || ExpectedNodeId.IsNone() || ChoiceIndex < 0 || ChoiceIndex >= 3) return;
    RewardSelectionMessage = FText::GetEmpty();
    PendingRewardCharacterId = CharacterId;
    PendingRewardNodeId = ExpectedNodeId;
    bRewardSelectionPending = true;
    bAwaitingRewardReplication = false;
    RefreshGameplayFlow();
    if (HasAuthority()) ExecuteGoldRewardSelection(CharacterId, ExpectedNodeId, ChoiceIndex);
    else ServerSelectGoldReward(CharacterId, ExpectedNodeId, ChoiceIndex);
}

void AGameplayPlayerController::ServerSelectGoldReward_Implementation(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex)
{
    ExecuteGoldRewardSelection(CharacterId, ExpectedNodeId, ChoiceIndex);
}

void AGameplayPlayerController::ExecuteGoldRewardSelection(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex)
{
    if (!HasAuthority()) return;
    const AGameplayGameState* State = GetWorld() ? GetWorld()->GetGameState<AGameplayGameState>() : nullptr;
    const ADevelopmentCoopLobby* Lobby = State ? State->GetDevelopmentLobby() : nullptr;
    if (Lobby && (!Lobby->HasStarted() || Lobby->GetMembers().ContainsByPredicate([](const FDevelopmentCoopMember& Member) { return !Member.bConnected; })))
    {
        ClientReceiveGoldRewardResult(CharacterId, ExpectedNodeId, false, NSLOCTEXT("RunGoldReward", "DisconnectedRecipient", "협동 참가자의 연결 상태를 확인해 주세요."));
        return;
    }
    FText Error = NSLOCTEXT("RunGoldReward", "UnboundRecipient", "현재 연결에 배정된 본인 캐릭터의 보상만 선택할 수 있습니다.");
    const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
    FRunAccountId AccountId;
    bool bSucceeded = false;
    if (Mode && Mode->ResolveRunParticipant(this, AccountId))
    {
        AEncounterManager* Manager = Mode->GetEncounterManager();
        if (Manager) bSucceeded = Manager->SelectGoldReward(AccountId, CharacterId, ExpectedNodeId, ChoiceIndex, Error);
    }
    if (bSucceeded) Error = NSLOCTEXT("RunGoldReward", "RewardClaimed", "전투 보상을 받았습니다.");
    ClientReceiveGoldRewardResult(CharacterId, ExpectedNodeId, bSucceeded, Error);
}

void AGameplayPlayerController::ClientReceiveGoldRewardResult_Implementation(FGuid CharacterId, FName ExpectedNodeId, bool bSucceeded, const FText& Message)
{
    if (PendingRewardCharacterId != CharacterId || PendingRewardNodeId != ExpectedNodeId) return;
    RewardSelectionMessage = Message;
    bRewardSelectionPending = bSucceeded;
    bAwaitingRewardReplication = bSucceeded;
    RefreshGameplayFlow();
}

void AGameplayPlayerController::RequestRetryCombatCheckpoint()
{
    // The current local server retries its own storage; this is not a client progression command.
    // 현재 로컬 서버가 자신의 저장을 재시도하며 클라이언트 진행 명령으로 사용하지 않습니다.
    if (CanRetryGameplayRecovery())
    {
        FText Error;
        EncounterManager->RetryCombatCheckpoint(Error);
        RefreshGameplayFlow();
    }
}

bool AGameplayPlayerController::CanRetryGameplayRecovery() const
{
    if (!HasAuthority() || !IsLocalController() || !EncounterManager || !EncounterManager->CanRetryCombatCheckpoint())
    {
        return false;
    }
    const URunStateSubsystem* CurrentRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
    return !CurrentRun || !CurrentRun->IsManagedRun() || (Mode && Mode->CanControlRunFlow(this, true));
}

bool AGameplayPlayerController::CanActivateRoundCamera() const
{
    const ERunPhase Phase = HasAuthority() && RunState ? RunState->GetPhase() : GameplayState ? GameplayState->GetViewState().Phase : ERunPhase::None;
    return !bPresentationEnding && Phase == ERunPhase::Combat;
}

void AGameplayPlayerController::ResetEncounterPresentation()
{
    GetWorldTimerManager().ClearTimer(EncounterPresentationTimer);
    ++PresentationGeneration;
    if (PresentedStage.IsValid()) PresentedStage->StopPresentation();
    if (DungeonRoute.IsValid())
    {
        DungeonRoute->OnTravelFinished.RemoveAll(this);
        DungeonRoute->ResetAtJunction();
    }
    PresentedStage.Reset();
    PresentationViewTarget.Reset();
    bWorldEncounterPresentation = false;
    bEncounterPresentationTransition = false;
}

bool AGameplayPlayerController::PrepareDungeonRoutes(const FGameplayViewState& View)
{
    const FRunDungeonState& Plan = View.DungeonState;
    const int32 Index = RunDungeonPlan::FindVisit(Plan, View.EncounterProgress);
    if (Plan.SchemaVersion != 0 && (Plan.SchemaVersion != 1 || !Plan.Visits.IsValidIndex(Index))) return false;
    if (CachedDungeonSeed != Plan.Seed || CachedDungeonVersion != Plan.SchemaVersion)
    {
        PreparedDungeonVisitIndex = INDEX_NONE;
        CachedDungeonSeed = Plan.Seed;
        CachedDungeonVersion = Plan.SchemaVersion;
    }

    // Keep only the current and next physical sections; the complete route remains serialized values.
    // 전체 경로는 직렬화된 값으로 유지하고 현재 구간과 다음 구간만 실제 공간으로 보관합니다.
    if (Index != INDEX_NONE && PreparedDungeonVisitIndex == Index && PreparedDungeonRoute.IsValid())
    {
        Swap(DungeonRoute, PreparedDungeonRoute);
        PreparedDungeonVisitIndex = INDEX_NONE;
    }
    const auto SpawnRoute = [this](float X)
    {
        FActorSpawnParameters Parameters;
        Parameters.Owner = this;
        Parameters.ObjectFlags |= RF_Transient;
        AEncounterDungeonRoute* Route = GetWorld()->SpawnActor<AEncounterDungeonRoute>(AEncounterDungeonRoute::StaticClass(), FVector(X, 0.f, -6000.f), FRotator::ZeroRotator, Parameters);
        if (Route) Route->SetPresentationVisible(false);
        return Route;
    };
    if (!DungeonRoute.IsValid()) DungeonRoute = SpawnRoute(PreparedDungeonRoute.IsValid() && FMath::IsNearlyZero(PreparedDungeonRoute->GetActorLocation().X) ? 16000.f : 0.f);
    const int32 Variant = Plan.Visits.IsValidIndex(Index) ? Plan.Visits[Index].LayoutVariant : 0;
    if (!DungeonRoute.IsValid() || !DungeonRoute->ConfigureLayout(Variant)) return false;
    DungeonRoute->SetPresentationVisible(true);

    const int32 NextIndex = Index == INDEX_NONE ? INDEX_NONE : Index + 1;
    if (Plan.Visits.IsValidIndex(NextIndex))
    {
        if (!PreparedDungeonRoute.IsValid()) PreparedDungeonRoute = SpawnRoute(FMath::IsNearlyZero(DungeonRoute->GetActorLocation().X) ? 16000.f : 0.f);
        if (PreparedDungeonRoute.IsValid())
        {
            PreparedDungeonRoute->SetPresentationVisible(false);
            if (PreparedDungeonVisitIndex != NextIndex)
            {
                PreparedDungeonRoute->ResetAtJunction();
                PreparedDungeonVisitIndex = PreparedDungeonRoute->ConfigureLayout(Plan.Visits[NextIndex].LayoutVariant) ? NextIndex : INDEX_NONE;
            }
        }
    }
    else
    {
        if (PreparedDungeonRoute.IsValid()) PreparedDungeonRoute->Destroy();
        PreparedDungeonRoute.Reset();
        PreparedDungeonVisitIndex = INDEX_NONE;
    }
    return true;
}

void AGameplayPlayerController::RefreshEncounterPresentation(const FGameplayViewState& View)
{
    if (!IsLocalController() || bPresentationEnding) return;
    const FRunEncounterProgress& Progress = View.EncounterProgress;
    TArray<FName> OfferIds;
    for (const FRunEncounterOffer& Offer : Progress.Offers) OfferIds.Add(Offer.EncounterId);
    const bool bWalkFromJunction = View.Phase == ERunPhase::Shop && PresentationPhase == ERunPhase::EncounterChoice && PresentationViewTarget == DungeonRoute && DungeonRoute.IsValid() && PresentedCompletedCount == Progress.AfterCompletedNodeCount && PresentedVisitIndex == Progress.VisitIndex && PresentedOfferIds == OfferIds && PresentedDungeonSeed == View.DungeonState.Seed && PresentedDungeonVersion == View.DungeonState.SchemaVersion;
    const bool bSameVisit = PresentationPhase == View.Phase && PresentedEncounterId == Progress.SelectedEncounterId && PresentedCompletedCount == Progress.AfterCompletedNodeCount && PresentedVisitIndex == Progress.VisitIndex && PresentedOfferIds == OfferIds && PresentedDungeonSeed == View.DungeonState.Seed && PresentedDungeonVersion == View.DungeonState.SchemaVersion;
    if (!bSameVisit)
    {
        ResetEncounterPresentation();
        PresentationPhase = View.Phase;
        PresentedEncounterId = Progress.SelectedEncounterId;
        PresentedCompletedCount = Progress.AfterCompletedNodeCount;
        PresentedVisitIndex = Progress.VisitIndex;
        PresentedOfferIds = MoveTemp(OfferIds);
        PresentedDungeonSeed = View.DungeonState.Seed;
        PresentedDungeonVersion = View.DungeonState.SchemaVersion;
    }
    if (View.Phase != ERunPhase::Shop && View.Phase != ERunPhase::Map && View.Phase != ERunPhase::EncounterChoice)
    {
        if (DungeonRoute.IsValid()) DungeonRoute->SetPresentationVisible(false);
        if (PreparedDungeonRoute.IsValid()) PreparedDungeonRoute->SetPresentationVisible(false);
        return;
    }
    if (PresentationViewTarget.IsValid()) return;
    if (bWorldEncounterPresentation) ResetEncounterPresentation();
    // The authored marker identifies the unified map; its aerial camera is replaced locally.
    // 제작된 마커로 통합 맵을 식별하고 공중 카메라를 로컬 던전 연출로 대체합니다.
    AActor* OverviewMarker = nullptr;
    for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("GameplayEncounterOverview")) && (!OverviewMarker || It->GetPathName() < OverviewMarker->GetPathName())) OverviewMarker = *It;
    }
    const bool bDungeonReady = OverviewMarker && PrepareDungeonRoutes(View);
    AActor* ViewTarget = nullptr;
    if (View.Phase == ERunPhase::Shop)
    {
        const FRunEncounterOffer* Offer = Progress.FindSelectedOffer();
        AEncounterPrototypeStage* SelectedStage = nullptr;
        if (Offer)
        {
            for (TActorIterator<AEncounterPrototypeStage> It(GetWorld()); It; ++It)
            {
                AEncounterPrototypeStage* Candidate = *It;
                if (Candidate->GetOwner() && Candidate->GetOwner()->IsA<AEncounterDungeonRoute>()) continue;
                if (!Candidate->MatchesOffer(*Offer)) continue;
                if (!SelectedStage || Candidate->Priority > SelectedStage->Priority || (Candidate->Priority == SelectedStage->Priority && Candidate->GetPathName() < SelectedStage->GetPathName())) SelectedStage = Candidate;
            }
        }
        if (SelectedStage)
        {
            const int32 Direction = Progress.Offers.IndexOfByPredicate([&Progress](const FRunEncounterOffer& Entry) { return Entry.EncounterId == Progress.SelectedEncounterId; });
            if (bDungeonReady && Direction >= 0 && Direction < 3)
            {
                if (AEncounterPrototypeStage* DungeonStage = DungeonRoute->ConfigureStage(SelectedStage, Direction))
                {
                    PresentedStage = DungeonStage;
                    PresentationViewTarget = DungeonStage;
                    bWorldEncounterPresentation = true;
                    if (bWalkFromJunction)
                    {
                        // Move only after the server commits the selected offer, never on the button request.
                        // 버튼 요청 시점이 아니라 서버가 후보 선택을 확정한 뒤에만 이동합니다.
                        bEncounterPresentationTransition = true;
                        SetViewTargetWithBlend(DungeonRoute.Get(), 0.f);
                        DungeonRoute->OnTravelFinished.AddUObject(this, &AGameplayPlayerController::FinishEncounterPresentation, PresentationGeneration);
                        if (DungeonRoute->StartTravel(Direction)) return;
                        DungeonRoute->OnTravelFinished.RemoveAll(this);
                    }
                    // Continue and late replication open directly at the saved encounter's arrival point.
                    // 이어하기와 늦게 수신한 복제 상태는 저장된 인카운터 도착점에서 시작합니다.
                    DungeonRoute->FinishAtStage();
                    FinishEncounterPresentation(PresentationGeneration);
                    if (PlayerCameraManager) PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.3f, FLinearColor::Black, false, false);
                    return;
                }
            }
            PresentedStage = SelectedStage;
            ViewTarget = SelectedStage;
        }
    }
    else
    {
        if (bDungeonReady)
        {
            DungeonRoute->ResetAtJunction();
            PresentationViewTarget = DungeonRoute.Get();
            bWorldEncounterPresentation = true;
            SetViewTargetWithBlend(DungeonRoute.Get(), 0.f);
            if (PlayerCameraManager) PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.3f, FLinearColor::Black, false, false);
            return;
        }
        ViewTarget = OverviewMarker;
    }
    // Missing stages keep legacy maps and their existing full-screen encounter layout usable.
    // 무대가 없는 기존 맵에서는 원래 전체 화면 인카운터 구성을 그대로 사용합니다.
    if (!ViewTarget) return;
    PresentationViewTarget = ViewTarget;
    bWorldEncounterPresentation = true;
    bEncounterPresentationTransition = GetViewTarget() != ViewTarget;
    if (!bEncounterPresentationTransition)
    {
        if (PresentedStage.IsValid()) PresentedStage->StartPresentation();
        return;
    }
    SetViewTargetWithBlend(ViewTarget, 0.65f, VTBlend_Cubic);
    GetWorldTimerManager().SetTimer(EncounterPresentationTimer, FTimerDelegate::CreateUObject(this, &AGameplayPlayerController::FinishEncounterPresentation, PresentationGeneration), 0.65f, false);
}

void AGameplayPlayerController::FinishEncounterPresentation(uint32 Generation)
{
    if (!IsLocalController() || bPresentationEnding || Generation != PresentationGeneration) return;
    const FGameplayViewState View = HasAuthority() && RunState ? FGameplayViewState::FromRun(RunState, FText::GetEmpty()) : GameplayState ? GameplayState->GetViewState() : FGameplayViewState();
    const FRunEncounterProgress& Progress = View.EncounterProgress;
    if (View.Phase != PresentationPhase || Progress.SelectedEncounterId != PresentedEncounterId || Progress.AfterCompletedNodeCount != PresentedCompletedCount || Progress.VisitIndex != PresentedVisitIndex || !PresentationViewTarget.IsValid())
    {
        ResetEncounterPresentation();
        RefreshGameplayFlow();
        return;
    }
    // Complete the local blend before exposing buttons, independently of purchase and save revisions.
    // 구매 및 저장 revision과 별개로 로컬 카메라 이동을 끝낸 뒤 버튼을 표시합니다.
    SetViewTargetWithBlend(PresentationViewTarget.Get(), 0.f);
    if (PresentedStage.IsValid()) PresentedStage->StartPresentation();
    bEncounterPresentationTransition = false;
    RefreshGameplayFlow();
}

void AGameplayPlayerController::RefreshGameplayFlow()
{
    if (bPresentationEnding) return;
    const ERunPhase CurrentPhase = HasAuthority() && RunState ? RunState->GetPhase() : GameplayState ? GameplayState->GetViewState().Phase : ERunPhase::None;
    if (CurrentPhase != ERunPhase::Shop)
    {
        EquipmentMessage = FText::GetEmpty();
        bEquipmentChangePending = false;
        PendingEquipmentCharacterId.Invalidate();
        PendingEquipmentRevision = INDEX_NONE;
    }
    else if (PendingEquipmentRevision != INDEX_NONE)
    {
        // Wait for the committed equipment projection as well as the request result.
        // 요청 결과와 함께 저장된 장비의 표시 뷰가 도착할 때까지 기다립니다.
        const TArray<FRunPartyMember>* Members = HasAuthority() && RunState ? &RunState->GetPartyMembers() : GameplayState ? &GameplayState->GetViewState().PartyMembers : nullptr;
        const FRunPartyMember* Member = Members ? Members->FindByPredicate([this](const FRunPartyMember& Entry) { return Entry.CharacterId == PendingEquipmentCharacterId; }) : nullptr;
        if (Member && Member->Equipment.Revision >= PendingEquipmentRevision)
        {
            bEquipmentChangePending = false;
            PendingEquipmentRevision = INDEX_NONE;
        }
    }
    if (CurrentPhase != ERunPhase::Shop)
    {
        ShopPurchaseMessage = FText::GetEmpty();
        bShopPurchasePending = false;
        bPendingItemShop = false;
        bPendingService = false;
        PendingShopRevision = INDEX_NONE;
    }
    else if (PendingShopRevision != INDEX_NONE)
    {
        // Wait for the saved stock and buyer values of the requested shop before allowing another action.
        // 요청한 상점의 저장된 재고와 구매자 정보가 표시 뷰에 도착한 뒤 다음 행동을 허용합니다.
        int32 DisplayedShopRevision = INDEX_NONE;
        const FRunEncounterProgress* Progress = HasAuthority() && RunState ? &RunState->GetEncounterProgress() : GameplayState ? &GameplayState->GetViewState().EncounterProgress : nullptr;
        const FRunEncounterOffer* Selected = Progress ? Progress->FindSelectedOffer() : nullptr;
        if (Progress && Progress->IsItemShop() == bPendingItemShop && (Selected && Selected->IsService()) == bPendingService)
        {
            if (HasAuthority() && RunState) DisplayedShopRevision = bPendingService ? RunState->GetRecoveryState().Revision : bPendingItemShop ? RunState->GetItemShopState().Revision : RunState->GetSkillShopState().Revision;
            else if (GameplayState) DisplayedShopRevision = bPendingService ? GameplayState->GetViewState().RecoveryState.Revision : bPendingItemShop ? GameplayState->GetViewState().ItemShopState.Revision : GameplayState->GetViewState().SkillShopState.Revision;
        }
        if (DisplayedShopRevision >= PendingShopRevision)
        {
            bShopPurchasePending = false;
            PendingShopRevision = INDEX_NONE;
        }
    }
    if (CurrentPhase != ERunPhase::Result)
    {
        RewardSelectionMessage = FText::GetEmpty();
        PendingRewardCharacterId.Invalidate();
        PendingRewardNodeId = NAME_None;
        bRewardSelectionPending = false;
        bAwaitingRewardReplication = false;
    }
    else if (bAwaitingRewardReplication)
    {
        // Keep choices locked until the saved claim reaches the same local or replicated view as the gold balance.
        // 저장된 수령 상태가 골드 잔액과 같은 로컬 또는 복제 뷰에 도착할 때까지 선택을 잠급니다.
        const FRunGoldRewardState* Rewards = HasAuthority() && RunState ? &RunState->GetGoldRewardState() : GameplayState ? &GameplayState->GetViewState().GoldRewardState : nullptr;
        if (Rewards && Rewards->NodeId == PendingRewardNodeId && Rewards->Claims.ContainsByPredicate([this](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == PendingRewardCharacterId && Claim.ChoiceIndex != INDEX_NONE; }))
        {
            bRewardSelectionPending = false;
            bAwaitingRewardReplication = false;
        }
    }
    if (IsLocalController() && GameplayRootWidget && GameplayState)
    {
        GameplayRootWidget->RefreshDevelopmentLobby(GameplayState->GetDevelopmentLobby());
        if (GameplayState->GetDevelopmentLobby()) GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>()->Connected();
    }
    if (!HasAuthority())
    {
        if (GameplayState && GameplayRootWidget)
        {
            const FGameplayViewState& View = GameplayState->GetViewState();
            RefreshEncounterPresentation(View);
            GameplayRootWidget->RefreshFlowView(View, false, false, bWorldEncounterPresentation, bEncounterPresentationTransition);
            if (View.Phase == ERunPhase::Combat && GameplayState->GetArena())
            {
                GameplayState->GetArena()->ActivateArena(this);
            }
        }
        return;
    }
    if (!RunState)
    {
        return;
    }

    const bool bManagedInputAllowed = !RunState->IsManagedRun() || (RunState->HasManagedLease() && !RunState->IsManagedResumePending() && RunState->GetLocalCaller() == RunState->GetRunIdentity().HostAccountId);
    ACombatManager* Manager = nullptr;
    FText FlowMessage;

    if (EncounterManager)
    {
        Manager = EncounterManager->GetCombatManager();
        FlowMessage = EncounterManager->GetFlowMessage();
    }

    const bool bInCombat = RunState->GetPhase() == ERunPhase::Combat && bManagedInputAllowed && Manager && Manager->IsCombatActive();
    SetCombatContext(Manager, bInCombat);

    if (GameplayRootWidget)
    {
        const FGameplayViewState View = FGameplayViewState::FromRun(RunState, FlowMessage);
        RefreshEncounterPresentation(View);
        GameplayRootWidget->RefreshFlowView(View, CanIssueRunCommands(), CanRetryGameplayRecovery(), bWorldEncounterPresentation, bEncounterPresentationTransition);
    }

    // Active CommonUI screens own the input config; the controller keeps combat authorization.
    // 활성 CommonUI 화면이 입력 설정을 소유하고 컨트롤러는 전투 조작 허용 상태를 유지합니다.
    bShowMouseCursor = true;
}

void AGameplayPlayerController::TryBindGameplayState()
{
    AGameplayGameState* State = GetWorld()->GetGameState<AGameplayGameState>();
    if (!State)
    {
        return;
    }
    if (GameplayState != State)
    {
        if (GameplayState)
        {
            GameplayState->OnGameplayViewChanged.RemoveAll(this);
        }
        GameplayState = State;
        GameplayState->OnGameplayViewChanged.AddUObject(this, &AGameplayPlayerController::RefreshGameplayFlow);
    }
    GetWorldTimerManager().ClearTimer(BindStateTimer);
    RefreshGameplayFlow();
}
