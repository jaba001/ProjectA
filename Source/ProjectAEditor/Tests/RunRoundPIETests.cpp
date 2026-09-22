#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RunEncounterPIEHelpers.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Encounter/CombatArena.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Run/RunSaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

namespace ProjectARunRoundTests
{
template <typename T>
T* Screen(AGameplayPlayerController* Controller)
{
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Widgets, T::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        T* Typed = Cast<T>(Widget);
        if (Typed && Typed->GetOwningPlayer() == Controller && Typed->IsActivated()) return Typed;
    }
    return nullptr;
}

// Exercise authored encounters through independent PIE NetDrivers and original participant accounts.
// 별도 PIE NetDriver와 원래 참가자 계정으로 작성된 인카운터를 실행합니다.
class FRunRoundPIE : public IAutomationLatentCommand
{
public:
    FRunRoundPIE(FAutomationTestBase* InTest, int32 InCount) : Test(InTest), Count(InCount), Slot(TEXT("ProjectA_Automation_RunRound_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)) {}

    virtual bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (Stage == 9)
        {
            if (FPlatformTime::Seconds() - Started > 30.0)
            {
                Test->AddError(TEXT("PIE worlds did not close after the Run test."));
                return true;
            }
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE) return false;
            }
            if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) Test->TestTrue(TEXT("Remove only this test's isolated save."), UGameplayStatics::DeleteGameInSlot(Slot, 0));
            return true;
        }
        if (FPlatformTime::Seconds() - Started > 90.0)
        {
            Test->AddError(FString::Printf(TEXT("Run PIE count=%d stage=%d encounter=%d participant=%d timed out. %s"), Count, Stage, EncounterIndex, Active, *Status));
            return End();
        }
        if (Stage == 0)
        {
            Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            Settings->SetPlayNetMode(Count == 1 ? PIE_Standalone : PIE_ListenServer);
            Settings->SetPlayNumberOfClients(Count);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;
            Settings->NewWindowWidth = 1280;
            Settings->NewWindowHeight = 720;
            Settings->SetClientWindowSize(FIntPoint(1280, 720));
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = Settings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Gameplay");
            Params.StartLocation = FVector(0, 0, 300);
            GEditor->RequestPlaySession(Params);
            Advance(1);
            return false;
        }
        if (Stage == 1)
        {
            if (!Connect()) return false;
            if (!Initialize()) return End();
            Advance(2);
            return false;
        }
        URunStateSubsystem* Run = Host->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        if (Stage == 2)
        {
            URunMapWidget* Map = Screen<URunMapWidget>(Host);
            if (!Map || Run->GetPhase() != ERunPhase::Map || !ClientsAt(ERunPhase::Map)) return false;
            UVerticalBox* Nodes = Cast<UVerticalBox>(Map->GetWidgetFromName(TEXT("NodeList")));
            if (!Nodes) return false;
            for (UWidget* Child : Nodes->GetAllChildren())
            {
                UGameplayActionButton* Node = Cast<UGameplayActionButton>(Child);
                if (!Node || !Node->GetIsEnabled()) continue;
                Node->OnClicked.Broadcast();
                Active = 0;
                bMoveReserved = false;
                bSawServerWalking = false;
                bSawRemoteWalking = false;
                bSawMoveCommitted = false;
                bSawMontage = false;
                RemoteCosts.Reset();
                RemoteMontages.Reset();
                RewardPartyBefore.Reset();
                RewardChoices.Reset();
                RewardParticipant = 0;
                bRewardRequestSent = false;
                Advance(3);
                return false;
            }
            return false;
        }
        ACombatRoundCoordinator* Round = Host->GetRoundCoordinator();
        if (Stage == 3)
        {
            if (!Round || Round->GetView().Phase != ECombatRoundPhase::Planning || !Synchronized()) return false;
            const auto& Units = Round->GetView().Units;
            if (!Check(Units.FilterByPredicate([](const auto& Unit) { return !Unit.bEnemy; }).Num() == Count, TEXT("Authored combat spawns exactly one character per participant."))) return End();
            if (!bMoveReserved)
            {
                AGameplayPlayerController* Mover = Clients.IsEmpty() ? Host : Clients.Last();
                const auto* Unit = Units.FindByPredicate([Mover](const auto& Candidate) { return !Candidate.bEnemy && Candidate.OwnerSlot == Mover->GetRoundParticipantSlot(); });
                if (!Unit || !Mover->IsRoundInputEnabled()) return false;
                FText Error;
                for (const auto& Tile : Round->GetArena()->Grid->TileMap)
                {
                    if (!Round->CanMoveUnit(Unit->UnitId, Tile.Key, Error)) continue;
                    MoveUnitId = Unit->UnitId;
                    MoveDestination = Tile.Key;
                    Mover->SubmitRoundMove(MoveUnitId, MoveDestination);
                    bMoveReserved = true;
                    return false;
                }
                Check(false, TEXT("The authored arena has an eligible empty allied SAP destination."));
                return End();
            }
            if (Active >= Count)
            {
                Active = 0;
                Advance(4);
                return false;
            }
            AGameplayPlayerController* Controller = Active == 0 ? Host : Clients[Active - 1];
            const FCombatRoundUnitView* Unit = Units.FindByPredicate([Controller](const auto& Candidate) { return !Candidate.bEnemy && Candidate.OwnerSlot == Controller->GetRoundParticipantSlot(); });
            const FCombatRoundUnitView* Enemy = Units.FindByPredicate([](const auto& Candidate) { return Candidate.bEnemy && Candidate.HP > 0; });
            if (!Unit || !Enemy || !IsValid(Unit->Unit) || Controller->IsRoundRequestPending()) return false;
            AGameplayGameModeBase* Mode = Host->GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
            const FGuid CharacterId = Mode->GetEncounterManager()->GetCombatManager()->GetCharacterId(Unit->Unit);
            const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.CharacterId == CharacterId; });
            TArray<TObjectPtr<USkillDefinitionDataAsset>> MemberSkills;
            FText Error;
            if (!Check(Member && Mode->PartyDefinition->ResolveMemberSkills(*Member, MemberSkills, Error), *FString::Printf(TEXT("The original character resolves its saved acquired loadout: %s"), *Error.ToString()))) return End();
            TArray<FName> ExpectedSkillIds;
            for (const USkillDefinitionDataAsset* Skill : MemberSkills)
            {
                FCombatRoundSkill Definition;
                if (!Check(Skill && Skill->ResolveRoundSkill(Definition, Error), *FString::Printf(TEXT("An authored profession skill resolves for the expected loadout: %s"), *Error.ToString()))) return End();
                ExpectedSkillIds.Add(Definition.SkillId);
            }
            if (!Check(Unit->SkillIds == ExpectedSkillIds && Unit->HP > 0, TEXT("Each original owner retains its ordered profession skills and living character."))) return End();
            FCombatRoundCommand Command;
            Command.UnitId = Unit->UnitId;
            Command.TargetUnitId = Enemy->UnitId;
            Command.TargetCoord = Enemy->HomeCoord;
            Command.DestinationCoord = Unit->HomeCoord;
            Command.SkillId = FName(TEXT("SkillDefinitionDataAsset:BPDA_AreaAttack"));
            Controller->SubmitRoundPlan(Command);
            ++Active;
            return false;
        }
        if (Stage == 4)
        {
            if (!Round || !Synchronized()) return false;
            if (Active >= Count)
            {
                Advance(5);
                return false;
            }
            AGameplayPlayerController* Controller = Active == 0 ? Host : Clients[Active - 1];
            if (Controller->IsRoundRequestPending()) return false;
            const auto* Unit = Round->GetView().Units.FindByPredicate([Controller](const auto& Candidate) { return !Candidate.bEnemy && Candidate.OwnerSlot == Controller->GetRoundParticipantSlot(); });
            if (!Check(Unit && Unit->Command.SkillId == FName(TEXT("SkillDefinitionDataAsset:BPDA_AreaAttack")), TEXT("Each local or remote planning request reaches the authoritative round."))) return End();
            Controller->SetRoundReady(true);
            ++Active;
            return false;
        }
        if (Stage == 5)
        {
            if (Round)
            {
                for (const auto& Unit : Round->GetView().Units)
                {
                    if (Unit.Unit && !Unit.bEnemy && Unit.Unit->HasRoundCastMontageInstance()) bSawMontage = true;
                    if (Unit.Unit && Unit.UnitId == MoveUnitId)
                    {
                        const float Speed = Unit.Unit->GetVelocity().Size2D();
                        if (Speed > 1 && Round->IsSAPMovementInProgress())
                        {
                            bSawServerWalking = Unit.Unit->GetMesh()->GetAnimInstance() && FMath::IsNearlyEqual(Speed, 350.f, 1.f);
                        }
                        if (Unit.HomeCoord == MoveDestination && !Round->IsSAPMovementInProgress()) bSawMoveCommitted = true;
                    }
                }
                for (AGameplayPlayerController* Client : Clients)
                {
                    if (ACombatRoundCoordinator* Remote = Client->GetRoundCoordinator())
                    {
                        const auto* Mover = Remote->GetView().Units.FindByPredicate([this](const auto& Unit) { return Unit.UnitId == MoveUnitId; });
                        if (Mover && Mover->Unit && Mover->Unit->GetVelocity().Size2D() > 1 && Mover->Unit->GetMesh()->GetAnimInstance()) bSawRemoteWalking = true;
                        for (const auto& Unit : Remote->GetView().Units)
                        {
                            if (!Unit.Unit || Unit.bEnemy) continue;
                            if (Unit.Unit->GetCurrentActionPoint() == 1) RemoteCosts.Add(Client);
                            if (Unit.Unit->HasRoundCastMontageInstance()) RemoteMontages.Add(Client);
                        }
                    }
                }
            }
            if (Run->GetPhase() != ERunPhase::Result || !ClientsAt(ERunPhase::Result)) return false;
            if (!Check(Run->GetLastResult() == ECombatResult::Victory, TEXT("The actual authored attack completes the encounter with victory."))) return End();
            UEncounterResultWidget* Result = Screen<UEncounterResultWidget>(Host);
            UButton* Continue = Result ? Cast<UButton>(Result->GetWidgetFromName(TEXT("Button_Continue"))) : nullptr;
            if (!Continue) return false;
            if (RewardPartyBefore.IsEmpty())
            {
                if (!Check(Run->GetGoldRewardState().GoldChoices.Num() == 3 && Run->GetGoldRewardRecipientIds().Num() == Count && Run->GetGoldRewardState().Claims.IsEmpty(), TEXT("Each victory offers three unclaimed gold choices to every human participant."))) return End();
                RewardPartyBefore = Run->GetPartyMembers();
                RewardChoices = Run->GetGoldRewardState().GoldChoices;
                for (int32 Gold : RewardChoices)
                {
                    if (!Check(Gold >= 5 && Gold <= 15, TEXT("Authored victory rewards stay within the inclusive five-to-fifteen gold range."))) return End();
                }
            }
            // Each local owner clicks once, then waits for both the authoritative and replicated personal award.
            // 각 로컬 소유자는 한 번 클릭한 뒤 서버와 복제 화면 양쪽의 개인 보상 반영을 기다립니다.
            if (RewardParticipant < Count)
            {
                AGameplayPlayerController* Controller = RewardParticipant == 0 ? Host : Clients[RewardParticipant - 1];
                AGameplayGameState* State = Controller->GetWorld()->GetGameState<AGameplayGameState>();
                UEncounterResultWidget* RewardScreen = Screen<UEncounterResultWidget>(Controller);
                if (!State || !RewardScreen || State->GetViewState().GoldRewardState.NodeId != Run->GetCurrentNodeId()) return false;
                const FGameplayViewState& View = State->GetViewState();
                const FGuid CharacterId = Controller->GetRewardCharacterId(View);
                const FRunPartyMember* Before = RewardPartyBefore.FindByPredicate([CharacterId](const FRunPartyMember& Member) { return Member.CharacterId == CharacterId; });
                if (!Check(Before && View.GoldRewardRecipientIds.Contains(CharacterId), TEXT("Each result screen resolves its own original reward recipient."))) return End();
                const int32 ChoiceIndex = RewardParticipant % 3;
                UGameplayActionButton* Card = Cast<UGameplayActionButton>(RewardScreen->GetWidgetFromName(FName(*FString::Printf(TEXT("GoldReward%d"), ChoiceIndex + 1))));
                if (!bRewardRequestSent)
                {
                    if (!Card || !Card->GetIsEnabled() || Controller->IsRewardSelectionPending()) return false;
                    if (!Check(!Continue->GetIsEnabled() && !Run->CanContinueAfterRewards(), TEXT("Host Continue waits until every human has collected a reward."))) return End();
                    Card->OnClicked.Broadcast();
                    bRewardRequestSent = true;
                    return false;
                }
                Status = Controller->GetRewardSelectionMessage().ToString();
                const FRunGoldRewardClaim* ServerClaim = Run->GetGoldRewardState().Claims.FindByPredicate([CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; });
                const FRunGoldRewardClaim* VisibleClaim = View.GoldRewardState.Claims.FindByPredicate([CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; });
                if (!ServerClaim || !VisibleClaim || Controller->IsRewardSelectionPending()) return false;
                const FRunPartyMember* ServerMember = Run->GetPartyMembers().FindByPredicate([CharacterId](const FRunPartyMember& Member) { return Member.CharacterId == CharacterId; });
                const FRunPartyMember* VisibleMember = View.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Member) { return Member.CharacterId == CharacterId; });
                const int32 ExpectedGold = Before->Gold + RewardChoices[ChoiceIndex];
                if (!Check(ServerMember && VisibleMember && ServerClaim->ChoiceIndex == ChoiceIndex && VisibleClaim->ChoiceIndex == ChoiceIndex && ServerMember->Gold == ExpectedGold && VisibleMember->Gold == ExpectedGold, TEXT("The selected card pays exactly once to its owner and replicates the same personal balance."))) return End();
                if (!Check(Card && !Card->GetIsEnabled() && Run->GetGoldRewardState().GoldChoices == RewardChoices && View.GoldRewardState.GoldChoices == RewardChoices, TEXT("A claimed card is disabled and reward presentation never rerolls its amounts."))) return End();
                ++RewardParticipant;
                bRewardRequestSent = false;
                return false;
            }
            if (!Continue->GetIsEnabled()) return false;
            for (AGameplayPlayerController* Client : Clients)
            {
                UEncounterResultWidget* RemoteResult = Screen<UEncounterResultWidget>(Client);
                UButton* RemoteContinue = RemoteResult ? Cast<UButton>(RemoteResult->GetWidgetFromName(TEXT("Button_Continue"))) : nullptr;
                const AGameplayGameState* RemoteState = Client->GetWorld()->GetGameState<AGameplayGameState>();
                if (!RemoteContinue || !RemoteState || !RemoteState->GetViewState().bCanContinueAfterRewards || RemoteState->GetViewState().GoldRewardState.Claims.Num() != Count) return false;
                if (!Check(!RemoteContinue->GetIsEnabled() && !Client->CanIssueRunCommands(), TEXT("Only the original host can continue the replicated result."))) return End();
            }
            if (!Check(bSawMontage, TEXT("The authored montage has an active animation instance during real PIE combat."))) return End();
            if (!Check(bSawServerWalking && bSawMoveCommitted, TEXT("SAP moves at 350 cm/s with animation input and commits its new home before AP attacks."))) return End();
            if (Count > 1 && !Check(bSawRemoteWalking, TEXT("A real remote NetDriver delivers SAP walking velocity to an animated mesh."))) return End();
            if (!Check(RemoteCosts.Num() == Clients.Num() && RemoteMontages.Num() == Clients.Num(), TEXT("Every remote client receives AP consumption and actual casting montage playback."))) return End();
            FText Error;
            TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Host->GetGameInstance()));
            Restored->EnableCheckpointSaving(Slot);
            if (!Check(Restored->LoadCheckpoint(Error), *FString::Printf(TEXT("A fresh Run subsystem reloads the durable result: %s"), *Error.ToString()))) return End();
            if (!Check(Restored->GetPhase() == ERunPhase::Result && Restored->GetPartyMembers().Num() == Count && Restored->GetRunIdentity().RunId == Run->GetRunIdentity().RunId, TEXT("Reload preserves phase, party and Run identity."))) return End();
            if (!Check(Restored->CanContinueAfterRewards() && FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Run->GetGoldRewardState(), &Restored->GetGoldRewardState(), 0), TEXT("Durable result reload preserves every selected reward and allows Continue."))) return End();
            for (const FRunPartyMember& Member : Restored->GetPartyMembers())
            {
                const FRunPartyMember* Live = Run->GetPartyMembers().FindByPredicate([&Member](const FRunPartyMember& Candidate) { return Candidate.CharacterId == Member.CharacterId; });
                if (!Check(Live && Live->Gold == Member.Gold, TEXT("The selected personal gold balance survives result reload."))) return End();
            }
            Continue->OnClicked.Broadcast();
            Advance(6);
            return false;
        }
        if (Stage == 6)
        {
            if (EncounterIndex == 0)
            {
                bool bFailed = false;
                if (!RunEncounterPIE::TickToMap(Test, Host, Clients, bFailed)) return bFailed ? End() : false;
                ++EncounterIndex;
                Advance(2);
                return false;
            }
            if (Run->GetPhase() != ERunPhase::Complete || !ClientsAt(ERunPhase::Complete)) return false;
            Check(Run->GetCompletedNodes().Num() == 2, TEXT("Both authored encounters and the intermediate shop complete one Run."));
            Test->AddInfo(FString::Printf(TEXT("%d-player PIE completed two real combats, personal reward-card selection, shop selection/exit, durable result reload and replicated host-only progression."), Count));
            if (Count == 1)
            {
                UClass* UnitClass = LoadClass<AUnitBase>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/Unit/BP_PlayerUnit.BP_PlayerUnit_C"));
                UAnimMontage* Original = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/Unit/Animation/Montage/MM_Attack_01_Montage.MM_Attack_01_Montage"));
                if (!Check(UnitClass && Original, TEXT("Saved player and montage are available for termination checks."))) return End();
                MontageUnit = Host->GetWorld()->SpawnActor<AUnitBase>(UnitClass, FVector(0, 0, 500), FRotator::ZeroRotator);
                LoopMontage.Reset(DuplicateObject<UAnimMontage>(Original, GetTransientPackage()));
                LoopMontage->RateScale = LoopMontage->GetPlayLength() / 0.2f;
                LoopMontage->bEnableAutoBlendOut = false;
                LoopMontage->CompositeSections[0].NextSectionName = LoopMontage->CompositeSections[0].SectionName;
                MontageUnit->SetRoundCastMontage(LoopMontage.Get());
                if (!Check(MontageUnit->HasRoundCastMontageInstance(), TEXT("An actual animated unit starts the transient looping montage."))) return End();
                Advance(7);
                return false;
            }
            return End();
        }
        if (Stage == 7)
        {
            if (FPlatformTime::Seconds() - Started < 0.6) return false;
            Check(MontageUnit->HasRoundCastMontageInstance(), TEXT("The looping montage remains active beyond its single-pass duration."));
            MontageUnit->CancelCurrentAction();
            Check(!MontageUnit->HasRoundCastMontageInstance(), TEXT("Cancellation clears the actual looping montage instance."));
            LoopMontage->RateScale = 0.f;
            MontageUnit->SetRoundCastMontage(LoopMontage.Get());
            Check(!MontageUnit->HasRoundCastMontageInstance(), TEXT("An unplayable rate cannot leave an active montage instance."));
            LoopMontage->RateScale = LoopMontage->GetPlayLength() / 0.2f;
            MontageUnit->SetRoundCastMontage(LoopMontage.Get());
            Check(MontageUnit->HasRoundCastMontageInstance(), TEXT("A valid montage can restart after cancellation and invalid data."));
            MontageUnit->Die();
            Check(!MontageUnit->HasRoundCastMontageInstance(), TEXT("Death clears the actual montage before ragdoll presentation."));
            MontageUnit->Destroy();
            return End();
        }
        return false;
    }

private:
    bool Connect()
    {
        Host = nullptr;
        Clients.Reset();
        ServerControllers.Reset();
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World) continue;
            if (World->GetNetMode() == NM_Client)
            {
                AGameplayPlayerController* Client = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
                if (Client && World->GetNetDriver() && World->GetNetDriver()->ServerConnection && World->GetNetDriver()->ServerConnection->GetConnectionState() == USOCK_Open) Clients.Add(Client);
            }
            else
            {
                for (auto It = World->GetPlayerControllerIterator(); It; ++It)
                {
                    AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(It->Get());
                    if (!Controller) continue;
                    if (Controller->IsLocalController()) Host = Controller;
                    else ServerControllers.Add(Controller);
                }
            }
        }
        if (!Host || Clients.Num() != Count - 1 || ServerControllers.Num() != Count - 1) return false;
        AGameplayGameModeBase* Mode = Host->GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
        return Mode && Mode->PartyDefinition && Mode->GetEncounterManager() && Mode->GetEncounterManager()->GetCombatManager();
    }

    bool Initialize()
    {
        AGameplayGameModeBase* Mode = Host->GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
        URunStateSubsystem* Run = Host->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        Run->PartyDefinition = Mode->PartyDefinition;
        Run->EnableCheckpointSaving(Slot);
        FRunIdentityData Identity;
        Identity.SchemaVersion = 2;
        Identity.Origin = ERunIdentityOrigin::AccountProvider;
        Identity.RunId = FGuid::NewGuid();
        Identity.HostEpoch = 1;
        TArray<FRunPartyMember> Party;
        const FName Classes[] = { TEXT("Warrior"), TEXT("Mage"), TEXT("Archer"), TEXT("Rogue") };
        for (int32 Index = 0; Index < Count; ++Index)
        {
            auto& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("TodoPIEFixture");
            Participant.AccountId.Subject = FString::Printf(TEXT("Owner%d"), Index);
            Participant.JoinOrdinal = Index + 1;
            auto& Member = Party.AddDefaulted_GetRef();
            Member.SlotIndex = Index;
            Member.bCreated = true;
            Member.bPlayerControlled = Index == 0;
            Member.ClassId = Classes[Index];
            Member.CharacterName = FText::FromString(Participant.AccountId.Subject);
            Member.CharacterId = FGuid::NewGuid();
            Member.OwnerAccountId = Participant.AccountId;
        }
        Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
        FText Error;
        const bool bInitialized = Count == 1 ? Run->InitializeRun(Party, Error) : Run->InitializeRunWithIdentity(Party, Identity, Error);
        if (!Check(bInitialized, *FString::Printf(TEXT("Initialize authored Run: %s"), *Error.ToString()))) return false;
        // This network combat fixture starts from an explicitly saved purchase; shop transactions have separate coverage.
        // 이 네트워크 전투 픽스처는 명시적으로 저장한 구매부터 시작하며 상점 거래는 별도로 검사합니다.
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
        if (!Check(Saved.IsValid(), TEXT("New Run has a durable save for the acquired-skill fixture."))) return false;
        for (FRunPartyMember& Member : Saved->Party)
        {
            if (!Check(Member.bHasSkillLoadout && Member.Skills.Num() == 1 && Member.Gold == 10, TEXT("Every newly initialized character starts unarmed with ten gold."))) return false;
            Member.Skills.Add(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_AreaAttack.BPDA_AreaAttack")));
            Member.Gold = 9;
        }
        if (!Check(UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0) && Run->LoadCheckpoint(Error), *FString::Printf(TEXT("Reload explicitly saved area attacks for the network combat fixture: %s"), *Error.ToString()))) return false;
        if (Count > 1)
        {
            if (!Check(Mode->AssignRunParticipant(Host, Identity.HostAccountId), TEXT("Explicitly bind the original host."))) return false;
            for (int32 Index = 0; Index < ServerControllers.Num(); ++Index)
            {
                if (!Check(Mode->AssignRunParticipant(ServerControllers[Index], Identity.OriginalParticipants[Index + 1].AccountId), TEXT("Explicitly bind each original remote owner."))) return false;
            }
        }
        return true;
    }

    bool ClientsAt(ERunPhase Phase)
    {
        for (AGameplayPlayerController* Client : Clients)
        {
            AGameplayGameState* State = Client->GetWorld()->GetGameState<AGameplayGameState>();
            if (!State || State->GetViewState().Phase != Phase) return false;
        }
        return true;
    }

    bool Synchronized()
    {
        ACombatRoundCoordinator* Server = Host->GetRoundCoordinator();
        if (!Server) return false;
        for (AGameplayPlayerController* Client : Clients)
        {
            ACombatRoundCoordinator* Remote = Client->GetRoundCoordinator();
            Status = Client->GetRoundRequestStatus().ToString();
            if (!Remote || Client->GetRoundParticipantSlot() <= 0 || Client->IsRoundRequestPending() || Remote->GetView().CombatId != Server->GetView().CombatId || Remote->GetView().PlanRevision != Server->GetView().PlanRevision || Remote->GetView().Phase != Server->GetView().Phase) return false;
            if (Remote->GetView().Units.Num() != Server->GetView().Units.Num()) return false;
            for (const auto& Unit : Server->GetView().Units)
            {
                const auto* Received = Remote->GetView().Units.FindByPredicate([&Unit](const auto& Candidate) { return Candidate.UnitId == Unit.UnitId; });
                if (!Received || Received->OwnerSlot != Unit.OwnerSlot || Received->HP != Unit.HP || Received->Command.SkillId != Unit.Command.SkillId || Received->Command.TargetUnitId != Unit.Command.TargetUnitId) return false;
            }
        }
        return !Host->IsRoundRequestPending();
    }

    bool Check(bool bValue, const TCHAR* Message) { return Test->TestTrue(Message, bValue); }
    void Advance(int32 Next) { Stage = Next; Started = FPlatformTime::Seconds(); }
    bool End() { GEditor->RequestEndPlayMap(); Advance(9); return false; }
    FAutomationTestBase* Test;
    int32 Count;
    FString Slot;
    FString Status;
    int32 Stage = 0;
    int32 Active = 0;
    int32 EncounterIndex = 0;
    int32 RewardParticipant = 0;
    double Started = 0;
    bool bSawMontage = false;
    bool bMoveReserved = false;
    bool bSawServerWalking = false;
    bool bSawRemoteWalking = false;
    bool bSawMoveCommitted = false;
    bool bRewardRequestSent = false;
    int32 MoveUnitId = INDEX_NONE;
    FIntPoint MoveDestination;
    AGameplayPlayerController* Host = nullptr;
    TArray<AGameplayPlayerController*> Clients;
    TArray<AGameplayPlayerController*> ServerControllers;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TStrongObjectPtr<UAnimMontage> LoopMontage;
    AUnitBase* MontageUnit = nullptr;
    TSet<AGameplayPlayerController*> RemoteCosts;
    TSet<AGameplayPlayerController*> RemoteMontages;
    TArray<FRunPartyMember> RewardPartyBefore;
    TArray<int32> RewardChoices;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FRunRoundPIETest, "ProjectA.RunRoundPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FRunRoundPIETest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (int32 Count : { 1, 2, 4 })
    {
        Names.Add(FString::Printf(TEXT("%dPlayers"), Count));
        Commands.Add(FString::FromInt(Count));
    }
}

bool FRunRoundPIETest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectARunRoundTests::FRunRoundPIE>(this, FCString::Atoi(*Parameters)));
    return true;
}

#endif
