#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RunEncounterPIEHelpers.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Library/CombatWeaponTraceLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/SkeletalMesh.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Encounter/CombatArena.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Run/RunSaveGame.h"
#include "GAS/Attribute/AS_Unit.h"
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
    FRunRoundPIE(FAutomationTestBase* InTest, int32 InCount, bool bInGeometryOnly = false) : Test(InTest), Count(InCount), Slot(TEXT("ProjectA_Automation_RunRound_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)), bGeometryOnly(bInGeometryOnly) {}

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
        if (!HostHandle.IsValid() || !IsValid(HostHandle->GetWorld()) || !IsValid(HostHandle->GetGameInstance()) || ClientHandles.ContainsByPredicate([](const TWeakObjectPtr<AGameplayPlayerController>& Client) { return !Client.IsValid() || !IsValid(Client->GetWorld()) || !IsValid(Client->GetGameInstance()); }))
        {
            Test->AddError(FString::Printf(TEXT("Run PIE count=%d stage=%d encounter=%d lost a participant controller or its PIE world."), Count, Stage, EncounterIndex + 1));
            return End();
        }
        URunStateSubsystem* Run = HostHandle->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        if (!Check(IsValid(Run), TEXT("The active PIE host retains its Run subsystem."))) return End();
        if (Run->GetPhase() == ERunPhase::Defeat)
        {
            const int32 Living = Run->GetPartyMembers().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.CurrentHP > 0.0f; }).Num();
            Test->AddError(FString::Printf(TEXT("Run PIE count=%d stage=%d encounter=%d node=%s ended in defeat with %d living party members instead of the fixture victory."), Count, Stage, EncounterIndex + 1, *Run->GetCurrentNodeId().ToString(), Living));
            return End();
        }
        if (Stage == 2)
        {
            if (bGeometryOnly)
            {
                // Use the same authoritative request as the map button without requiring a rendered widget for geometry inspection.
                // 형태 검사에는 렌더링된 위젯을 요구하지 않고 지도 버튼과 같은 권한 요청을 사용합니다.
                if (Run->GetPhase() != ERunPhase::Map) return false;
                const FRunNodeDefinition* Node = Run->GetNodes().FindByPredicate([Run](const FRunNodeDefinition& Candidate) { return Run->CanStartNode(Candidate.NodeId); });
                if (!Check(Node != nullptr, TEXT("The geometry-only fixture has an eligible authored Run node."))) return End();
                Host->RequestStartNode(Node->NodeId);
                Advance(3);
                return false;
            }
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
                bSawBladeCast = false;
                bSawUnarmedCast = false;
                bPreparedCombatHP = false;
                ExpectedSubmittedSkills.Reset();
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
            if (!Check(Units.FilterByPredicate([](const auto& Unit) { return Unit.bEnemy; }).Num() == 4, TEXT("The authored encounter retains all four enemies in every fixture combat."))) return End();
            if (!bPreparedCombatHP)
            {
                // Isolate survival and one-hit enemy health through live GAS attributes without modifying authored balance.
                // 작성된 밸런스를 바꾸지 않고 실제 GAS 속성으로 생존과 적 1회 타격 체력을 고정합니다.
                for (const auto& Unit : Units)
                {
                    if (!IsValid(Unit.Unit) || Unit.HP <= 0.0f) continue;
                    UAbilitySystemComponent* AbilitySystem = Unit.Unit->GetAbilitySystemComponent();
                    if (!Check(IsValid(AbilitySystem), TEXT("Every living fixture unit retains its GAS attributes."))) return End();
                    if (!Unit.bEnemy) AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), FixtureMaxHP);
                    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Unit.bEnemy ? FMath::Min(Unit.HP, FixtureEnemyHP) : FixtureMaxHP);
                }
                FCombatCheckpointData FixtureCheckpoint;
                FText Error;
                if (!Check(Round->CapturePlanningCheckpoint(FixtureCheckpoint, Error), *FString::Printf(TEXT("The survival fixture satisfies the authored checkpoint validation: %s"), *Error.ToString()))) return End();
                bPreparedCombatHP = true;
                return false;
            }
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
            TArray<const FCombatRoundUnitView*> LivingEnemies;
            for (const auto& Candidate : Units)
            {
                if (Candidate.bEnemy && Candidate.HP > 0 && IsValid(Candidate.Unit)) LivingEnemies.Add(&Candidate);
            }
            const FCombatRoundUnitView* Enemy = LivingEnemies.IsEmpty() ? nullptr : LivingEnemies[bGeometryOnly ? 0 : Active % LivingEnemies.Num()];
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
            if (!Check(Unit->SkillIds == ExpectedSkillIds && ExpectedSkillIds.Contains(SwordSkillId) && ExpectedSkillIds.Contains(UnarmedSkillId) && Unit->HP > 0, TEXT("Each original owner retains both saved original attacks, their order, and a living character."))) return End();
            FCombatRoundCommand Command;
            Command.UnitId = Unit->UnitId;
            Command.TargetUnitId = Enemy->UnitId;
            Command.TargetCoord = Enemy->HomeCoord;
            Command.DestinationCoord = Unit->HomeCoord;
            // Keep the original blade profile for upright bodies; the preserved unarmed profile covers the shorter fixture body.
            // 직립 몸체에는 원래 칼날 프로필을 유지하고 작은 픽스처 몸체에는 보존된 비무장 프로필을 사용합니다.
            const bool bUseBlade = bGeometryOnly || Enemy->Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() >= Unit->Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            Command.SkillId = bUseBlade ? SwordSkillId : UnarmedSkillId;
            const FCombatRoundSkill* SelectedSkill = Round->FindSkill(Command.SkillId);
            if (!Check(SelectedSkill && SelectedSkill->bUseWeaponTrace == bUseBlade, TEXT("The fixture selects the real authored blade or unarmed collision profile without changing it."))) return End();
            ExpectedSubmittedSkills.Add(Unit->UnitId, Command.SkillId);
            Controller->SubmitRoundPlan(Command);
            ++Active;
            return false;
        }
        if (Stage == 4)
        {
            if (!Round || !Synchronized()) return false;
            if (Active >= Count)
            {
                PlannedRoundNumber = Round->GetView().RoundNumber;
                Advance(5);
                return false;
            }
            AGameplayPlayerController* Controller = Active == 0 ? Host : Clients[Active - 1];
            if (Controller->IsRoundRequestPending()) return false;
            const auto* Unit = Round->GetView().Units.FindByPredicate([Controller](const auto& Candidate) { return !Candidate.bEnemy && Candidate.OwnerSlot == Controller->GetRoundParticipantSlot(); });
            const FName* ExpectedSkill = Unit ? ExpectedSubmittedSkills.Find(Unit->UnitId) : nullptr;
            if (!Check(Unit && ExpectedSkill && Unit->Command.SkillId == *ExpectedSkill, TEXT("Each local or remote original-attack request reaches the authoritative round."))) return End();
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
                    LogSwordContactDiagnostics(Round, Unit);
                    if (IsValid(Unit.Unit) && !Unit.bEnemy && Unit.ActionPhase == ECombatRoundActionPhase::Casting)
                    {
                        const FCombatRoundSkill* Skill = Round->FindSkill(Unit.Command.SkillId);
                        if (Skill && Skill->SkillId == SwordSkillId && Skill->bUseWeaponTrace) bSawBladeCast = true;
                        if (Skill && Skill->SkillId == UnarmedSkillId && !Skill->bUseWeaponTrace) bSawUnarmedCast = true;
                    }
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
                if (bGeometryOnly && bFullSwordGeometryRecorded)
                {
                    Test->AddInfo(TEXT("Geometry-only diagnostic ended after the complete authored swing scan; encounter victory and multiplayer behavior were not validated."));
                    return End();
                }
            }
            if (Run->GetPhase() == ERunPhase::Combat && Round && Round->GetView().Phase == ECombatRoundPhase::Planning && Round->GetView().RoundNumber > PlannedRoundNumber)
            {
                // Submit another real attack round when fewer participants than enemies cannot finish in one round.
                // 참가자가 적보다 적어 한 라운드에 끝나지 않으면 실제 공격 라운드를 다시 제출합니다.
                if (!Check(Round->GetView().RoundNumber <= 16, TEXT("The original blade and unarmed collisions defeat all four authored enemies within the bounded fixture rounds."))) return End();
                Active = 0;
                ExpectedSubmittedSkills.Reset();
                Advance(3);
                return false;
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
            if (!Check(bSawBladeCast && bSawUnarmedCast, TEXT("Every fixture encounter actually casts both retained attacks before its real victory."))) return End();
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
                if (!Check(Live && Live->Gold == Member.Gold && Live->CurrentHP == Member.CurrentHP && FMath::IsFinite(Member.CurrentHP) && Member.CurrentHP > 0.0f && Member.CurrentHP <= FixtureMaxHP, TEXT("Personal gold and the surviving combat HP persist exactly through result reload."))) return End();
            }
            Continue->OnClicked.Broadcast();
            Advance(6);
            return false;
        }
        if (Stage == 6)
        {
            if (!Check(Run->GetCompletedNodes().Num() == EncounterIndex + 1, TEXT("Each authored victory completes exactly the next Run node."))) return End();
            if (EncounterIndex + 1 < Run->GetNodes().Num())
            {
                bool bFailed = false;
                if (!RunEncounterPIE::TickToMap(Test, Host, Clients, bFailed)) return bFailed ? End() : false;
                ++EncounterIndex;
                Advance(2);
                return false;
            }
            if (Run->GetPhase() != ERunPhase::Complete || !ClientsAt(ERunPhase::Complete)) return false;
            if (!Check(Run->GetCompletedNodes().Num() == Run->GetNodes().Num(), TEXT("All ten authored encounters and the nine intermediate shops complete one Run."))) return End();
            Test->AddInfo(FString::Printf(TEXT("%d-player PIE completed %d real combats and %d intermediate shops, personal reward-card selection, durable result reload and replicated host-only progression."), Count, Run->GetNodes().Num(), Run->GetNodes().Num() - 1));
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
    // Observe original collision geometry without moving bodies or changing the authored trace or victory requirements.
    // 몸체를 이동하거나 작성된 궤적·승리 조건을 바꾸지 않고 원래 충돌 형태를 관찰합니다.
    void LogSwordContactDiagnostics(ACombatRoundCoordinator* Round, const FCombatRoundUnitView& Source)
    {
        if (Source.bEnemy || !IsValid(Source.Unit) || !Source.Unit->IsUnitAlive()) return;
        const FString Key = FString::Printf(TEXT("%d/%d/%d"), EncounterIndex, Round->GetView().RoundNumber, Source.UnitId);
        if ((Source.ActionPhase == ECombatRoundActionPhase::Recovery || Source.ActionPhase == ECombatRoundActionPhase::Returning || CombatRoundRules::IsTerminal(Source.ActionPhase)) && !SwordOutcomeDiagnostics.Contains(Key))
        {
            SwordOutcomeDiagnostics.Add(Key);
            Test->AddInfo(FString::Printf(TEXT("Original attack fixture outcome %s skill=%s phase=%d status=%s"), *Key, *Source.Command.SkillId.ToString(), static_cast<int32>(Source.ActionPhase), *Source.Status.ToString()));
        }
        if (Source.ActionPhase != ECombatRoundActionPhase::Casting || SwordPoseDiagnostics.Contains(Key)) return;
        SwordPoseDiagnostics.Add(Key);
        const FCombatRoundSkill* Skill = Round->FindSkill(Source.Command.SkillId);
        UAnimMontage* Montage = Skill ? Source.Unit->ResolveRoundCastMontage(Skill->CastMontage) : nullptr;
        if (!Skill || !Skill->bUseWeaponTrace || !IsValid(Montage)) return;
        LogFullSwordGeometry(Round, Source, *Skill, Montage);
        TArray<CombatWeaponTrace::FBladePose> Poses;
        TSet<int32> FrozenHits;
        double MinZ = TNumericLimits<double>::Max();
        double MaxZ = TNumericLimits<double>::Lowest();
        const double EndTime = Skill->WindupSeconds + Skill->WeaponTraceDuration;
        for (double Time = Skill->WindupSeconds; Time <= EndTime + UE_DOUBLE_SMALL_NUMBER; Time = FMath::Min(Time + 0.005, EndTime))
        {
            CombatWeaponTrace::FBladePose Pose;
            if (!CombatWeaponTrace::SampleBlade(Source.Unit, *Skill, Montage, Time * Montage->RateScale, Pose)) break;
            const CombatWeaponTrace::FBladePose Previous = Poses.IsEmpty() ? Pose : Poses.Last();
            if (const AUnitBase* Hit = CombatWeaponTrace::FindFirstHit(Source.Unit->GetWorld(), Source.Unit, Round->GetView().Units, Previous, Pose, Skill->WeaponTraceRadius))
            {
                const auto* HitView = Round->GetView().Units.FindByPredicate([Hit](const auto& Candidate) { return Candidate.Unit == Hit; });
                if (HitView) FrozenHits.Add(HitView->UnitId);
            }
            Poses.Add(Pose);
            MinZ = FMath::Min(MinZ, FMath::Min(Pose.Base.Z, Pose.Tip.Z));
            MaxZ = FMath::Max(MaxZ, FMath::Max(Pose.Base.Z, Pose.Tip.Z));
            if (Time + UE_DOUBLE_SMALL_NUMBER >= EndTime) break;
        }
        FString Targets;
        for (const auto& Target : Round->GetView().Units)
        {
            if (!Target.bEnemy || Target.HP <= 0.f || !IsValid(Target.Unit)) continue;
            const UCapsuleComponent* Capsule = Target.Unit->GetCapsuleComponent();
            double MinGap = TNumericLimits<double>::Max();
            for (const auto& Pose : Poses)
            {
                const int32 Intervals = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Pose.Base, Pose.Tip) / Skill->WeaponTraceRadius));
                for (int32 Point = 0; Point <= Intervals; ++Point)
                {
                    FVector Closest;
                    const float Gap = Capsule->GetClosestPointOnCollision(FMath::Lerp(Pose.Base, Pose.Tip, static_cast<double>(Point) / Intervals), Closest);
                    if (Gap >= 0.f) MinGap = FMath::Min(MinGap, static_cast<double>(Gap));
                }
            }
            Targets += FString::Printf(TEXT(" [%d %s center=%s half=%.1f distance=%.1f gap=%.1f frozenHit=%d]"), Target.UnitId, *Target.Unit->GetClass()->GetName(), *Capsule->GetComponentLocation().ToCompactString(), Capsule->GetScaledCapsuleHalfHeight(), FVector::Dist2D(Source.Unit->GetActorLocation(), Target.Unit->GetActorLocation()), MinGap, FrozenHits.Contains(Target.UnitId));
        }
        Test->AddInfo(FString::Printf(TEXT("Sword fixture frozen-pose diagnostics %s source=%s range=%.1f window=%.3f..%.3f radius=%.1f bladeZ=%.1f..%.1f samples=%d%s"), *Key, *Source.Unit->GetActorLocation().ToCompactString(), Skill->HitRange, Skill->WindupSeconds, EndTime, Skill->WeaponTraceRadius, MinZ, MaxZ, Poses.Num(), *Targets));
    }

    // Scan the complete original montage once at a real close-range cast against the shorter authored enemy.
    // 작은 원본 적을 향한 실제 근거리 시전에서 원래 몽타주 전체를 한 번 검사합니다.
    void LogFullSwordGeometry(ACombatRoundCoordinator* Round, const FCombatRoundUnitView& Source, const FCombatRoundSkill& Skill, UAnimMontage* Montage)
    {
        if (bFullSwordGeometryRecorded) return;
        const FCombatRoundUnitView* Target = Round->GetView().Units.FindByPredicate([&Source](const auto& Candidate) { return Candidate.UnitId == Source.Command.TargetUnitId; });
        if (!Target || !IsValid(Target->Unit) || !Target->Unit->IsUnitAlive() || Target->Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() >= Source.Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) return;
        bFullSwordGeometryRecorded = true;
        const double Length = Montage->GetPlayLength();
        if (!Check(FMath::IsFinite(Length) && Length > 0.0 && Length <= 60.0 && FMath::IsFinite(Montage->RateScale) && Montage->RateScale > 0.f, TEXT("The full authored swing scan has a bounded length and valid rate."))) return;
        const UCapsuleComponent* Capsule = Target->Unit->GetCapsuleComponent();
        Test->AddInfo(FString::Printf(TEXT("Sword full geometry identity source=%s mesh=%s requestedMontage=%s resolvedMontage=%s target=%s targetCenter=%s targetHalf=%.1f targetRadius=%.1f montageLength=%.6f rate=%.6f distance=%.1f"), *Source.Unit->GetClass()->GetName(), *Source.Unit->GetMesh()->GetSkeletalMeshAsset()->GetPathName(), *GetPathNameSafe(Skill.CastMontage), *Montage->GetPathName(), *Target->Unit->GetClass()->GetName(), *Capsule->GetComponentLocation().ToCompactString(), Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleRadius(), Length, Montage->RateScale, FVector::Dist2D(Source.Unit->GetActorLocation(), Target->Unit->GetActorLocation())));
        for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
        {
            for (int32 Index = 0; Index < Track.AnimTrack.AnimSegments.Num(); ++Index)
            {
                const FAnimSegment& Segment = Track.AnimTrack.AnimSegments[Index];
                Test->AddInfo(FString::Printf(TEXT("Sword full geometry segment index=%d slot=%s animation=%s montageAsset=%.6f..%.6f animationAsset=%.6f..%.6f rate=%.6f loops=%d"), Index, *Track.SlotName.ToString(), *GetPathNameSafe(Segment.GetAnimReference()), Segment.StartPos, Segment.GetEndPos(), Segment.AnimStartTime, Segment.AnimEndTime, Segment.GetValidPlayRate(), Segment.LoopingCount));
            }
        }
        TArray<FCombatRoundUnitView> TargetOnly = {*Target};
        CombatWeaponTrace::FBladePose Previous;
        bool bHasPrevious = false;
        bool bPreviousContact = false;
        double MinGap = TNumericLimits<double>::Max();
        double MinZ = TNumericLimits<double>::Max();
        double MaxZ = TNumericLimits<double>::Lowest();
        double FirstContact = -1.0;
        double LastContact = -1.0;
        double IntervalStart = -1.0;
        double PreviousTime = 0.0;
        FString ContactIntervals;
        int32 Samples = 0;
        int32 InvalidSamples = 0;
        int32 ContactSamples = 0;
        for (double Time = 0.0; Time <= Length + UE_DOUBLE_SMALL_NUMBER; Time = FMath::Min(Time + 0.005, Length))
        {
            CombatWeaponTrace::FBladePose Pose;
            const bool bSampled = CombatWeaponTrace::SampleBlade(Source.Unit, Skill, Montage, Time, Pose);
            bool bContact = false;
            if (bSampled)
            {
                ++Samples;
                MinZ = FMath::Min(MinZ, FMath::Min(Pose.Base.Z, Pose.Tip.Z));
                MaxZ = FMath::Max(MaxZ, FMath::Max(Pose.Base.Z, Pose.Tip.Z));
                const int32 Intervals = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Pose.Base, Pose.Tip) / Skill.WeaponTraceRadius));
                for (int32 Point = 0; Point <= Intervals; ++Point)
                {
                    FVector Closest;
                    const float Gap = Capsule->GetClosestPointOnCollision(FMath::Lerp(Pose.Base, Pose.Tip, static_cast<double>(Point) / Intervals), Closest);
                    if (Gap >= 0.f) MinGap = FMath::Min(MinGap, static_cast<double>(Gap));
                }
                bContact = CombatWeaponTrace::FindFirstHit(Source.Unit->GetWorld(), Source.Unit, TargetOnly, bHasPrevious ? Previous : Pose, Pose, Skill.WeaponTraceRadius) == Target->Unit;
                Previous = Pose;
                bHasPrevious = true;
            }
            else
            {
                ++InvalidSamples;
                bHasPrevious = false;
            }
            if (bContact)
            {
                ++ContactSamples;
                if (FirstContact < 0.0) FirstContact = Time;
                LastContact = Time;
                if (!bPreviousContact) IntervalStart = Time;
            }
            else if (bPreviousContact) ContactIntervals += FString::Printf(TEXT(" [%.6f..%.6f]"), IntervalStart / Montage->RateScale, PreviousTime / Montage->RateScale);
            bPreviousContact = bContact;
            PreviousTime = Time;
            if (Time + UE_DOUBLE_SMALL_NUMBER >= Length) break;
        }
        if (bPreviousContact) ContactIntervals += FString::Printf(TEXT(" [%.6f..%.6f]"), IntervalStart / Montage->RateScale, PreviousTime / Montage->RateScale);
        Check(Samples > 0, TEXT("The complete authored montage exposes real blade poses for the bounded geometry diagnostic."));
        Test->AddInfo(FString::Printf(TEXT("Sword full geometry result minGap=%.6f bladeZ=%.6f..%.6f firstContactAsset=%.6f lastContactAsset=%.6f firstContactSeconds=%.6f lastContactSeconds=%.6f validSamples=%d invalidSamples=%d contactSamples=%d contactIntervalsSeconds=%s"), MinGap, MinZ, MaxZ, FirstContact, LastContact, FirstContact < 0.0 ? -1.0 : FirstContact / Montage->RateScale, LastContact < 0.0 ? -1.0 : LastContact / Montage->RateScale, Samples, InvalidSamples, ContactSamples, *ContactIntervals));
    }

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
        if (!Mode || !Mode->PartyDefinition || !Mode->GetEncounterManager() || !Mode->GetEncounterManager()->GetCombatManager()) return false;
        HostHandle = Host;
        ClientHandles.Reset();
        for (AGameplayPlayerController* Client : Clients) ClientHandles.Add(Client);
        return true;
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
            if (Count > 1)
            {
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
            }
        }
        Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
        FText Error;
        const bool bInitialized = Count == 1 ? Run->InitializeRun(Party, Error) : Run->InitializeRunWithIdentity(Party, Identity, Error);
        if (!Check(bInitialized, *FString::Printf(TEXT("Initialize authored Run: %s"), *Error.ToString()))) return false;
        if (!Check(Run->GetNodes().Num() == 10, TEXT("The newly initialized Run exposes ten sequential combat nodes."))) return false;
        // This network combat fixture starts from an explicitly saved purchase; shop transactions have separate coverage.
        // 이 네트워크 전투 픽스처는 명시적으로 저장한 구매부터 시작하며 상점 거래는 별도로 검사합니다.
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
        if (!Check(Saved.IsValid(), TEXT("New Run has a durable save for the acquired-skill fixture."))) return false;
        USkillDefinitionDataAsset* Sword = LoadObject<USkillDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack"));
        FCombatRoundSkill SwordDefinition;
        if (!Check(Sword && Sword->ResolveRoundSkill(SwordDefinition, Error) && SwordDefinition.Power > 0.0f && SwordDefinition.bUseWeaponTrace, TEXT("The retained sword skill has a valid original blade collision profile."))) return false;
        SwordSkillId = SwordDefinition.SkillId;
        for (FRunPartyMember& Member : Saved->Party)
        {
            if (!Check(Member.bHasSkillLoadout && Member.Skills.Num() == 1 && Member.Gold == 10, TEXT("Every newly initialized character starts unarmed with ten gold."))) return false;
            USkillDefinitionDataAsset* Unarmed = Cast<USkillDefinitionDataAsset>(Member.Skills[0].TryLoad());
            FCombatRoundSkill UnarmedDefinition;
            if (!Check(Unarmed && Unarmed->ResolveRoundSkill(UnarmedDefinition, Error) && UnarmedDefinition.Power > 0.0f && UnarmedDefinition.Kind == ECombatRoundSkillKind::Melee && UnarmedDefinition.TargetRule == ESkillTargetRule::EnemyUnit && UnarmedDefinition.Approach == ECombatRoundApproach::Unit && !UnarmedDefinition.bUseWeaponTrace && !UnarmedDefinition.bUseEffectCollision, TEXT("The saved starting skill retains its original non-blade melee collision profile."))) return false;
            if (UnarmedSkillId.IsNone()) UnarmedSkillId = UnarmedDefinition.SkillId;
            if (!Check(UnarmedDefinition.SkillId == UnarmedSkillId && UnarmedSkillId != SwordSkillId, TEXT("All original owners retain the same distinct starting unarmed attack."))) return false;
            FixtureEnemyHP = FMath::Min(SwordDefinition.Power, UnarmedDefinition.Power);
            Member.Skills.Add(FSoftObjectPath(Sword));
            Member.Gold = 9;
        }
        if (!Check(UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0) && Run->LoadCheckpoint(Error), *FString::Printf(TEXT("Reload both explicitly saved original attacks for the network combat fixture: %s"), *Error.ToString()))) return false;
        for (const FRunPartyMember& Member : Run->GetPartyMembers())
        {
            const FRunPartyMember* Expected = Saved->Party.FindByPredicate([&Member](const FRunPartyMember& Candidate) { return Candidate.CharacterId == Member.CharacterId; });
            TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
            if (!Check(Expected && Member.bHasSkillLoadout && Member.Skills.Num() == 2 && Member.Skills == Expected->Skills && Mode->PartyDefinition->ResolveMemberSkills(Member, Skills, Error) && Skills.Num() == 2 && FName(*Skills[0]->GetPrimaryAssetId().ToString()) == UnarmedSkillId && FName(*Skills[1]->GetPrimaryAssetId().ToString()) == SwordSkillId, TEXT("Durable reload preserves the original owner's ordered unarmed and blade DataAsset loadout."))) return false;
        }
        if (Count == 1)
        {
            FRunAccountId AccountId;
            if (!Check(Run->GetRunIdentity().Origin == ERunIdentityOrigin::LocalDevelopment && Run->GetRunIdentity().OriginalParticipants.Num() == 1 && Mode->ResolveRunParticipant(Host, AccountId) && AccountId == Run->GetRunIdentity().HostAccountId, TEXT("The standalone fixture resolves the normalized local Run account."))) return false;
            // Real menu travel creates Gameplay after the Run; this in-place fixture must refresh that same account context.
            // 실제 메뉴 이동은 Run 생성 뒤 Gameplay를 만들므로 현재 레벨 픽스처에서도 같은 계정 문맥을 갱신합니다.
            Host->RefreshRunFlowPermissions();
            AGameplayGameState* State = Host->GetWorld()->GetGameState<AGameplayGameState>();
            const FGuid CharacterId = State ? Host->GetInventoryCharacterId(State->GetViewState()) : FGuid();
            if (!Check(CharacterId.IsValid() && Run->GetPartyMembers().ContainsByPredicate([AccountId, CharacterId](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled && Member.OwnerAccountId == AccountId && Member.CharacterId == CharacterId; }), TEXT("The standalone controller resolves its original character before combat, shops and rewards."))) return false;
        }
        else
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
    int32 PlannedRoundNumber = 0;
    int32 RewardParticipant = 0;
    double Started = 0;
    bool bSawMontage = false;
    bool bSawBladeCast = false;
    bool bSawUnarmedCast = false;
    bool bPreparedCombatHP = false;
    static constexpr float FixtureMaxHP = 10000.0f;
    float FixtureEnemyHP = 0.0f;
    FName SwordSkillId;
    FName UnarmedSkillId;
    TMap<int32, FName> ExpectedSubmittedSkills;
    bool bMoveReserved = false;
    bool bSawServerWalking = false;
    bool bSawRemoteWalking = false;
    bool bSawMoveCommitted = false;
    bool bRewardRequestSent = false;
    int32 MoveUnitId = INDEX_NONE;
    FIntPoint MoveDestination;
    AGameplayPlayerController* Host = nullptr;
    TWeakObjectPtr<AGameplayPlayerController> HostHandle;
    TArray<TWeakObjectPtr<AGameplayPlayerController>> ClientHandles;
    TArray<AGameplayPlayerController*> Clients;
    TArray<AGameplayPlayerController*> ServerControllers;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TStrongObjectPtr<UAnimMontage> LoopMontage;
    AUnitBase* MontageUnit = nullptr;
    TSet<AGameplayPlayerController*> RemoteCosts;
    TSet<AGameplayPlayerController*> RemoteMontages;
    TArray<FRunPartyMember> RewardPartyBefore;
    TArray<int32> RewardChoices;
    TSet<FString> SwordPoseDiagnostics;
    TSet<FString> SwordOutcomeDiagnostics;
    bool bGeometryOnly = false;
    bool bFullSwordGeometryRecorded = false;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRoundSwordGeometryTest, "ProjectA.RunRoundSwordGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRoundSwordGeometryTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectARunRoundTests::FRunRoundPIE>(this, 1, true));
    return true;
}

#endif
