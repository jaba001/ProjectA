#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GAS/Effect/GE_Damage.h"
#include "GAS/Effect/GE_Heal.h"
#include "GAS/Effect/GE_Shield.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/SecureHash.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationEditorCommon.h"
#include "RunEncounterPIEHelpers.h"
#include "TodoReviewWindowPlacement.h"
#include "TodoReviewGameplayPresentation.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace NormalTargetRunReview
{
    template <typename T>
    T* Screen(UWorld* World)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (Widget->GetOwningPlayer() == World->GetFirstPlayerController() && Cast<T>(Widget)->IsActivated()) return Cast<T>(Widget);
        return nullptr;
    }

    // Use visible state and public requests only; a natural defeat is an observed outcome, never repaired into a victory.
    // 표시되는 상태와 공개 요청만 사용하며 자연 패배를 관측 결과로 기록하고 승리로 보정하지 않습니다.
    class FReview : public IAutomationLatentCommand
    {
    public:
        FReview(FAutomationTestBase* InTest, FString InSlot, FString InOutput) : Test(InTest), Slot(MoveTemp(InSlot)), Output(MoveTemp(InOutput)) {}

        virtual ~FReview() override
        {
            UnbindConsumable();
            ReleaseViewport();
        }

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (RunStarted == 0.0) RunStarted = PhaseStarted = LastProgress = Now;
            if (Stage == 99)
            {
                if (AnyPIE())
                {
                    if (Now - PhaseStarted < 30.0) return false;
                    Check(false, TEXT("Normal Run PIE closes within thirty seconds."));
                    return true;
                }
                ReleaseViewport();
                if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Only the previously absent, owned normal-Run slot is removed."));
                FinishReport();
                return true;
            }
            if (Now - RunStarted > 3600.0 || Now - LastProgress > 180.0)
            {
                Outcome = TEXT("ObservationTimeout");
                Check(false, FString::Printf(TEXT("The normal Run reached its observation budget at stage %d; no victory, time dilation or combat repair was applied."), Stage));
                return End();
            }
            if (Stage == 0) return StartPIE(false);
            if (Stage == 5)
            {
                if (AnyPIE()) return false;
                ReleaseViewport();
                Controller.Reset();
                LastProgress = Now;
                return StartPIE(true);
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World || !World->GetFirstPlayerController()) return false;
            if (Stage == 1 || Stage == 6)
            {
                UMainMenuScreenWidget* Menu = Screen<UMainMenuScreenWidget>(World);
                if (!Cast<AMainMenuPlayerController>(World->GetFirstPlayerController()) || !Menu) return false;
                const bool bContinue = Stage == 6;
                UButton* Button = Cast<UButton>(Menu->GetWidgetFromName(bContinue ? TEXT("Button_Continue") : TEXT("Button_NewGame")));
                if (!Check(Button && Button->GetIsEnabled(), bContinue ? TEXT("The saved target Run enables the actual Continue button after PIE restart.") : TEXT("The actual relocated menu enables New Game for the fresh owned slot."))) return End();
                if (!TodoReviewWindowPlacement::Ensure(Test, World)) return End();
                Button->OnClicked.Broadcast();
                Stage = bContinue ? 7 : 2;
                LastProgress = Now;
                return false;
            }
            if (Stage == 2)
            {
                UGameModeSelectionWidget* Menu = Screen<UGameModeSelectionWidget>(World);
                if (!Menu) return false;
                UButton* Button = Cast<UButton>(Menu->GetWidgetFromName(TEXT("Button_SinglePlayer")));
                if (!Check(Button && Button->GetIsEnabled(), TEXT("The actual menu exposes normal single-player creation."))) return End();
                Button->OnClicked.Broadcast();
                Stage = 3;
                LastProgress = Now;
                return false;
            }
            if (Stage == 3)
            {
                UCharacterCreationWidget* Creation = Screen<UCharacterCreationWidget>(World);
                if (!Creation) return false;
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    UButton* Create = Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index))));
                    if (!Check(Create && Create->GetIsEnabled(), TEXT("Each actual Create button supplies its normal default character."))) return End();
                    Create->OnClicked.Broadcast();
                }
                UButton* Control = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_PlayerControl")));
                UButton* Start = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_StartGame")));
                if (!Check(Control && Start && Control->GetIsEnabled(), TEXT("The normal four-character party can select its first direct-control character."))) return End();
                Control->OnClicked.Broadcast();
                const TArray<FRunPartyMember> Members = Creation->GetPartyMembers();
                if (!Check(Members.FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated; }).Num() == 4 && Members.FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled; }).Num() == 1 && Start->GetIsEnabled(), TEXT("The real creation UI retains four default characters, one direct and three companions."))) return End();
                Start->OnClicked.Broadcast();
                Stage = 4;
                LastProgress = Now;
                return false;
            }
            Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            if (!Controller.IsValid()) return false;
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            if (!Run || Run->GetPhase() == ERunPhase::None) return false;
            RetainViewport(World);
            if (!Check(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TEXT("/Game/User_JeHoon/LEVEL/Core/Gameplay") && Run->IsTargetRun() && !Run->IsManagedRun() && Run->IsCheckpointSavingEnabled(), TEXT("Normal menu creation enters the real saved single-player target Run on Core/Gameplay."))) return End();
            if (!bInitialPartyRecorded)
            {
                if (!Check(Run->GetPartyMembers().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated; }).Num() == 4 && Run->GetNodes().Num() == 20, TEXT("The target Run retains four original characters and its twenty authored combat nodes."))) return End();
                OriginalIdentity = Run->GetRunIdentity();
                OriginalParty = Run->GetPartyMembers();
                if (!Check(Run->UsesWeaponSkills() && Run->GetWeaponSkillRules().SchemaVersion == 1 && Run->GetSkillShopState().SchemaVersion == 0, TEXT("Normal menu creation freezes weapon acquisition without the retired skill shop."))) return End();
                for (const FRunPartyMember& Member : OriginalParty)
                {
                    FProfessionDefinition Profession;
                    FText Error;
                    TArray<FSoftObjectPath> EquippedSkills;
                    if (!Check(Run->ResolveMemberProfession(Member, Profession, Error) && FMath::IsNearlyEqual(Member.CurrentHP, Profession.MaxHP) && Member.InnateSkills.Num() == 1 && Member.Skills.Num() == 2 && Member.Skills.Contains(Member.InnateSkills[0]) && RunEquipmentRules::BuildEquippedSkills(Member, EquippedSkills, Error) && Member.Skills == EquippedSkills && Member.Consumables.Num() == 1 && Member.Consumables[0].Quantity == Run->GetRecoveryState().StartingQuantity, TEXT("Every freshly created character retains authored maximum health, unarmed plus its equipped weapon skill and normal consumable quantity: ") + Error.ToString())) return End();
                }
                bInitialPartyRecorded = true;
                Event(Run, TEXT("normal_party_created"));
            }
            ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
            if (Stage == 7 && bResumingConsumableBoundary)
            {
                if (Run->GetPhase() != ERunPhase::Combat || !Round || Round->GetView().Phase != ECombatRoundPhase::Planning || Controller->IsRoundRequestPending() || !Controller->IsRoundInputEnabled()) return false;
                if (!Check(ConsumableRestartSave.IsValid() && SameSavedBoundary(*ConsumableRestartSave.Get(), Run) && FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&ConsumableRestartSave->CombatCheckpoint, &Run->GetCombatCheckpoint(), 0), TEXT("Actual post-consumable menu Continue preserves the saved party HP, stock, progress and complete planning checkpoint."))) return End();
                if (!Check(Round->GetView().CombatId.IsValid() && Round->GetView().CombatId != ConsumableRestartCombatId, TEXT("Post-consumable Continue creates a new live combat session without reusing its old command identity.")) || !VerifyLivePlanningBoundary(ConsumableRestartSave->CombatCheckpoint, Round)) return End();
                if (!Capture(World, TEXT("AfterConsumableActualContinue"))) return End();
                bConsumableRestartVerified = true;
                bResumingConsumableBoundary = false;
                Event(Run, TEXT("actual_post_consumable_menu_continue_verified"), FString::Printf(TEXT("attempt=%s revision=%lld round=%d"), *Run->GetCombatCheckpoint().AttemptId.ToString(), Run->GetCombatCheckpoint().Revision, Run->GetCombatCheckpoint().RoundNumber));
                ConsumableRestartSave.Reset();
                Stage = 4;
                LastProgress = Now;
            }
            else if (Stage == 7)
            {
                if (Run->GetPhase() != ERunPhase::Result) return false;
                if (!Check(RestartSave.IsValid() && SameSavedBoundary(*RestartSave.Get(), Run), TEXT("Actual post-restart Continue restores the saved first-victory party, progress, rewards and target choices."))) return End();
                bRestartVerified = true;
                RestartSave.Reset();
                Event(Run, TEXT("actual_menu_continue_verified"));
                Stage = 4;
                LastProgress = Now;
            }
            if (!VerifyIdentity(Run)) return End();
            const FString Signature = FString::Printf(TEXT("%d:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d"), static_cast<int32>(Run->GetPhase()), *Run->GetCurrentNodeId().ToString(), *Run->GetEncounterProgress().SelectedEncounterId.ToString(), Run->GetCompletedNodes().Num(), Run->GetTargetRunState().CompletedEncounterChoices.Num(), Round ? static_cast<int32>(Round->GetView().Phase) : -1, Round ? Round->GetView().RoundNumber : 0, Round ? Round->GetView().PlanRevision : 0, Run->GetSkillShopState().Revision, Run->GetItemShopState().Revision, Run->GetRecoveryState().Revision, Run->GetGoldRewardState().Claims.Num());
            if (Signature != LastSignature)
            {
                LastSignature = Signature;
                LastProgress = Now;
            }
            const bool bStable = Run->GetPhase() != ERunPhase::Preparing && (Run->GetPhase() != ERunPhase::Combat || (Round && Round->GetView().Phase == ECombatRoundPhase::Planning && !Controller->IsRoundRequestPending()));
            if (bStable && !VerifySavedBoundary(Run, Signature)) return End();
            if (bObserveConsumableResolving && !ObserveConsumableBoundary(Run, Round)) return bPassed ? false : End();
            if (ConsumableUIUses > 0 && !bConsumableRestartRequested && bStable && Run->GetPhase() == ERunPhase::Combat && Round && Controller->IsRoundInputEnabled() && FindLivingDirectUnit(Run, Round->GetView())) return RestartAfterConsumable(Run, Round);
            if (Run->GetPhase() == ERunPhase::Defeat || Run->GetPhase() == ERunPhase::Complete)
            {
                if (TerminalStarted == 0.0)
                {
                    TerminalStarted = Now;
                    return false;
                }
                if (Now - TerminalStarted < 0.75) return false;
                const bool bComplete = Run->GetPhase() == ERunPhase::Complete;
                if (!Check(bComplete ? Run->GetCompletedNodes().Num() == 20 && Run->GetTargetRunState().CompletedEncounterChoices.Num() == 60 : Run->GetLastResult() == ECombatResult::Defeat && !Run->CanContinueAfterRewards(), TEXT("The actual terminal Run preserves either full target completion or a final natural defeat."))) return End();
                Outcome = bComplete ? TEXT("Completed80Stages") : TEXT("ObservedNaturalDefeat");
                Event(Run, Outcome);
                if (!Capture(World, Outcome)) return End();
                bTerminalObserved = true;
                if (!bComplete) Test->AddWarning(TEXT("The unchanged normal party lost through actual combat. This successful outcome observation is not an eighty-stage completion or a balance-quality approval."));
                return End();
            }
            if (Run->GetPhase() == ERunPhase::EncounterChoice) return SelectEncounter(Run);
            if (Run->GetPhase() == ERunPhase::Shop) return VisitShop(Run);
            if (Run->GetPhase() == ERunPhase::Map)
            {
                const FRunNodeDefinition* Next = Run->GetNodes().FindByPredicate([Run](const FRunNodeDefinition& Node) { return Run->CanStartNode(Node.NodeId); });
                if (!Check(Next != nullptr, TEXT("An unfinished normal route exposes an eligible authored next combat."))) return End();
                Event(Run, TEXT("request_start_node"), Next->NodeId.ToString());
                Controller->RequestStartNode(Next->NodeId);
                return false;
            }
            if (Run->GetPhase() == ERunPhase::Result) return ClaimAndContinue(Run);
            if (Run->GetPhase() == ERunPhase::Combat && Round && Round->GetView().Phase == ECombatRoundPhase::Planning) return PlanRound(Run, Round);
            return false;
        }

    private:
        bool Check(bool bCondition, const FString& Message)
        {
            bPassed = Test->TestTrue(*Message, bCondition) && bPassed;
            return bCondition;
        }

        bool AnyPIE() const
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) return true;
            return false;
        }

        bool StartPIE(bool bContinue)
        {
            Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            Settings->SetPlayNetMode(PIE_Standalone);
            Settings->SetPlayNumberOfClients(1);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;
            Settings->NewWindowWidth = 1280;
            Settings->NewWindowHeight = 720;
            Settings->SetClientWindowSize(FIntPoint(1280, 720));
            if (!TodoReviewWindowPlacement::Configure(Test, Settings.Get())) return End();
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = Settings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu");
            GEditor->RequestPlaySession(Params);
            Stage = bContinue ? 6 : 1;
            LastProgress = FPlatformTime::Seconds();
            return false;
        }

        bool VerifyIdentity(URunStateSubsystem* Run)
        {
            if (!Check(FRunIdentityData::StaticStruct()->CompareScriptStruct(&OriginalIdentity, &Run->GetRunIdentity(), 0), TEXT("Normal progression preserves the original Run and participant identity."))) return false;
            for (const FRunPartyMember& Original : OriginalParty)
            {
                if (!Original.bCreated) continue;
                const FRunPartyMember* Current = Run->GetPartyMembers().FindByPredicate([&Original](const FRunPartyMember& Member) { return Member.CharacterId == Original.CharacterId; });
                if (!Check(Current && Current->OwnerAccountId == Original.OwnerAccountId && Current->SlotIndex == Original.SlotIndex && Current->ClassId == Original.ClassId && Current->bPlayerControlled == Original.bPlayerControlled, TEXT("Every original character keeps its class, owner, slot and direct/companion assignment."))) return false;
            }
            return Check(Run->GetSaveError().IsEmpty(), TEXT("Normal public requests have no unresolved checkpoint write error: ") + Run->GetSaveError().ToString());
        }

        bool SameSavedBoundary(const URunSaveGame& Saved, URunStateSubsystem* Run)
        {
            if (!FRunDungeonState::StaticStruct()->CompareScriptStruct(&Saved.DungeonState, &Run->GetDungeonState(), 0)) return false;
            if (Saved.Phase != Run->GetPhase() || Saved.Result != Run->GetLastResult() || Saved.CurrentNode != Run->GetCurrentNodeId() || Saved.CurrentEncounter != Run->GetCurrentEncounterId() || Saved.CompletedNodes != Run->GetCompletedNodes() || Saved.Party.Num() != Run->GetPartyMembers().Num() || Saved.WeaponSkillAcquisitionVersion != (Run->UsesWeaponSkills() ? 1 : 0)) return false;
            if (!FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Saved.WeaponSkillRules, &Run->GetWeaponSkillRules(), 0) || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Saved.Identity, &Run->GetRunIdentity(), 0) || !FRunTargetState::StaticStruct()->CompareScriptStruct(&Saved.TargetRun, &Run->GetTargetRunState(), 0) || !FRunEncounterProgress::StaticStruct()->CompareScriptStruct(&Saved.EncounterProgress, &Run->GetEncounterProgress(), 0) || !FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Saved.GoldRewardState, &Run->GetGoldRewardState(), 0) || !FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Saved.SkillShopState, &Run->GetSkillShopState(), 0) || !FRunItemShopState::StaticStruct()->CompareScriptStruct(&Saved.ItemShopState, &Run->GetItemShopState(), 0)) return false;
            for (int32 Index = 0; Index < Saved.Party.Num(); ++Index) if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Saved.Party[Index], &Run->GetPartyMembers()[Index], 0)) return false;
            return true;
        }

        bool VerifySavedBoundary(URunStateSubsystem* Run, const FString& Signature)
        {
            if (VerifiedBoundaries.Contains(Signature)) return true;
            FText Error;
            TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Check(Saved.IsValid() && SameSavedBoundary(*Saved.Get(), Run), TEXT("The isolated on-disk checkpoint deserializes to the exact published stable normal-Run boundary: ") + Error.ToString())) return false;
            if (Run->GetPhase() == ERunPhase::Combat && !Check(FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Saved->CombatCheckpoint, &Run->GetCombatCheckpoint(), 0), TEXT("The stable normal planning save preserves its complete authoritative combat checkpoint."))) return false;
            VerifiedBoundaries.Add(Signature);
            ++SavedBoundaryReads;
            Event(Run, TEXT("saved_boundary_deserialized"));
            return true;
        }

        const FRunPartyMember* DirectMember(URunStateSubsystem* Run) const
        {
            return Run->GetPartyMembers().FindByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled; });
        }

        // Spectator slot zero never grants control over a companion with the same zero owner slot.
        // 관전자 슬롯 0은 소유 슬롯이 같은 0인 동료의 조작 권한을 부여하지 않습니다.
        const FCombatRoundUnitView* FindLivingDirectUnit(URunStateSubsystem* Run, const FCombatRoundView& View) const
        {
            const FRunPartyMember* Member = DirectMember(Run);
            ACombatManager* Manager = Controller.IsValid() ? Controller->GetCombatManager() : nullptr;
            const int32 ParticipantSlot = Controller.IsValid() ? Controller->GetRoundParticipantSlot() : 0;
            if (!Member || !Manager || ParticipantSlot <= 0) return nullptr;
            return View.Units.FindByPredicate([Member, Manager, ParticipantSlot](const FCombatRoundUnitView& Entry)
            {
                return !Entry.bEnemy && Entry.OwnerSlot == ParticipantSlot && Entry.Unit && Entry.Unit->IsUnitAlive() && Manager->GetCharacterId(Entry.Unit) == Member->CharacterId;
            });
        }

        bool SelectEncounter(URunStateSubsystem* Run)
        {
            const FRunPartyMember* Member = DirectMember(Run);
            if (!Check(Member && Run->GetEncounterProgress().Offers.Num() == 3, TEXT("The normal target encounter offers three actual choices for the original party."))) return End();
            FProfessionDefinition Profession;
            FText Error;
            if (!Check(Run->ResolveMemberProfession(*Member, Profession, Error), TEXT("Normal encounter strategy resolves current authored growth values: ") + Error.ToString())) return End();
            const FRunEncounterOffer* Best = nullptr;
            int32 BestPriority = MIN_int32;
            for (const FRunEncounterOffer& Offer : Run->GetEncounterProgress().Offers)
            {
                const FGameplayTag Tag = Offer.GetResolvedTag();
                const int32 Priority = Member->CurrentHP <= 0.f && Tag.MatchesTag(FRunEncounterOffer::GetRevivalTag()) ? 5 : Member->CurrentHP > 0.f && Member->CurrentHP < Profession.MaxHP && Tag.MatchesTag(FRunEncounterOffer::GetRecoveryTag()) ? 4 : Tag.MatchesTag(FRunEncounterOffer::GetSkillShopTag()) ? 3 : Tag.MatchesTag(FRunEncounterOffer::GetConsumableShopTag()) ? 2 : 1;
                if (!Best || Priority > BestPriority)
                {
                    Best = &Offer;
                    BestPriority = Priority;
                }
            }
            if (!Check(Best != nullptr, TEXT("Normal encounter strategy selects only an actual offered tag-classified option."))) return End();
            Event(Run, TEXT("request_encounter_choice"), Best->EncounterId.ToString());
            Controller->RequestSelectRunEncounter(Best->EncounterId);
            return false;
        }

        static bool Visible(UWidget* Widget)
        {
            for (UWidget* Current = Widget; Current; Current = Current->GetParent()) if (!Current->IsVisible()) return false;
            return Widget != nullptr;
        }

        bool RequestServiceUI(URunStateSubsystem* Run, const FRunPartyMember& Member, FName OfferId, FGameplayTag Tag, int32 Price, float ExpectedHP, bool bConsumable, bool bFullRecovery)
        {
            URunEncounterWidget* Shop = Screen<URunEncounterWidget>(Controller->GetWorld());
            UGameplayActionButton* Button = Shop ? Cast<UGameplayActionButton>(Shop->GetWidgetFromName(TEXT("Button_ShopRecovery"))) : nullptr;
            const bool bButtonReady = Button && Visible(Button) && Button->GetIsEnabled();
            // Observe the actual CommonUI transition instead of failing before its newly selected screen activates.
            // 새로 선택한 화면이 활성화되기 전에 실패하지 않고 실제 CommonUI 전환을 관측합니다.
            if (!bButtonReady && FPlatformTime::Seconds() - ServiceUIReadyAt < 5.0) return false;
            const AGameplayGameState* GameState = Controller->GetWorld()->GetGameState<AGameplayGameState>();
            const FGameplayViewState* View = GameState ? &GameState->GetViewState() : nullptr;
            const FGuid BuyerId = View ? Controller->GetShopBuyerCharacterId(*View) : FGuid();
            const FRunShopBuyerView* BuyerView = View ? View->ShopBuyerViews.FindByPredicate([BuyerId](const FRunShopBuyerView& Entry) { return Entry.CharacterId == BuyerId; }) : nullptr;
            Event(Run, TEXT("service_ui_readiness"), FString::Printf(TEXT("shop=%s button=%s visible=%d enabled=%d wait_seconds=%.3f pending=%d buyer_matches=%d hp=%.3f displayed_max_hp=%.3f gold=%d price=%d"), *GetNameSafe(Shop), *GetNameSafe(Button), Button && Visible(Button), Button && Button->GetIsEnabled(), FPlatformTime::Seconds() - ServiceUIReadyAt, Controller->IsShopPurchasePending(), BuyerId == Member.CharacterId, Member.CurrentHP, BuyerView ? BuyerView->MaxHP : -1.f, Member.Gold, Price));
            if (!Check(bButtonReady, TEXT("The eligible authored recovery service exposes its enabled original UI button within five natural seconds."))) return false;
            if (!Check(BuyerId == Member.CharacterId && BuyerView && View->Phase == ERunPhase::Shop && View->EncounterProgress.SelectedEncounterId == Run->GetEncounterProgress().SelectedEncounterId, TEXT("The actual shop presentation identifies the original eligible buyer and current encounter."))) return false;
            ExpectedServiceParty = Run->GetPartyMembers();
            FRunPartyMember* Expected = ExpectedServiceParty.FindByPredicate([&Member](const FRunPartyMember& Entry) { return Entry.CharacterId == Member.CharacterId; });
            if (!Check(Expected != nullptr, TEXT("The service observes only the original direct-control buyer."))) return false;
            Expected->Gold -= Price;
            Expected->CurrentHP = ExpectedHP;
            if (bConsumable) ++Expected->Consumables[0].Quantity;
            ExpectedServiceRevision = (bFullRecovery ? Run->GetSkillShopState().Revision : Run->GetRecoveryState().Revision) + 1;
            PendingServiceTag = Tag;
            bPendingFullRecovery = bFullRecovery;
            ServiceObservedAt = 0.0;
            int32 Dispatches = 0;
            FName Dispatched;
            const FDelegateHandle Handle = Button->OnActionRequested.AddLambda([&](FName Id) { ++Dispatches; Dispatched = Id; });
            Event(Run, bFullRecovery ? TEXT("request_skill_shop_recovery") : TEXT("request_service_purchase"), Tag.ToString());
            Button->OnClicked.Broadcast();
            Button->OnActionRequested.Remove(Handle);
            return Check(Dispatches == 1 && Dispatched == OfferId, TEXT("The original service UI delegate dispatches its actual tagged offer exactly once."));
        }

        bool ObserveServiceUI(URunStateSubsystem* Run)
        {
            const int32 Revision = bPendingFullRecovery ? Run->GetSkillShopState().Revision : Run->GetRecoveryState().Revision;
            if (!Check(Revision == ExpectedServiceRevision && ExpectedServiceParty.Num() == Run->GetPartyMembers().Num(), TEXT("The actual UI service commits exactly one expected revision."))) return false;
            for (int32 Index = 0; Index < ExpectedServiceParty.Num(); ++Index) if (!Check(FRunPartyMember::StaticStruct()->CompareScriptStruct(&ExpectedServiceParty[Index], &Run->GetPartyMembers()[Index], 0), TEXT("The UI service changes exactly the buyer's authored HP/stock and price; companions remain unchanged."))) return false;
            if (ServiceObservedAt == 0.0) ServiceObservedAt = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds() - ServiceObservedAt < 0.25) return false;
            if (!VerifySavedBoundary(Run, LastSignature) || !Capture(Controller->GetWorld(), FString::Printf(TEXT("ServiceUI_%02d"), ServiceUIPurchases + 1))) return false;
            ++ServiceUIPurchases;
            if (PendingServiceTag.MatchesTag(FRunEncounterOffer::GetRevivalTag())) ++RevivalUIPurchases;
            else if (PendingServiceTag.MatchesTag(FRunEncounterOffer::GetConsumableShopTag())) ++ConsumableUIPurchases;
            else ++RecoveryUIPurchases;
            Event(Run, TEXT("actual_service_ui_purchase_verified"), PendingServiceTag.ToString());
            ExpectedServiceParty.Reset();
            return true;
        }

        bool VisitShop(URunStateSubsystem* Run)
        {
            if (Controller->IsShopPurchasePending() || Controller->IsEquipmentChangePending()) return false;
            if (!ExpectedServiceParty.IsEmpty() && !ObserveServiceUI(Run)) return bPassed ? false : End();
            const FRunPartyMember* Member = DirectMember(Run);
            const FRunEncounterOffer* Encounter = Run->GetEncounterProgress().FindSelectedOffer();
            if (!Check(Member && Encounter, TEXT("The selected normal encounter retains its original direct character and offer."))) return End();
            FProfessionDefinition Profession;
            FText Error;
            if (!Check(Run->ResolveMemberProfession(*Member, Profession, Error), Error.ToString())) return End();
            const FString VisitKey = FString::Printf(TEXT("%d:%s"), Run->GetTargetRunState().CompletedEncounterChoices.Num(), *Encounter->EncounterId.ToString());
            if (ServiceUIVisit != VisitKey)
            {
                ServiceUIVisit = VisitKey;
                ServiceUIReadyAt = FPlatformTime::Seconds();
                return false;
            }
            if (!RunEncounterPIE::PresentationReady(Controller.Get(), Screen<URunEncounterWidget>(Controller->GetWorld()))) return false;
            AEncounterPrototypeStage* NPC = Cast<AEncounterPrototypeStage>(Controller->GetViewTarget());
            if (!Check(NPC && NPC->MatchesOffer(*Encounter), TEXT("The committed shop reaches its matching authored NPC camera before interaction."))) return End();
            if (!ObservedStages.Contains(NPC->StageId))
            {
                if (!Capture(Controller->GetWorld(), TEXT("NPC_") + NPC->StageId.ToString())) return End();
                ObservedStages.Add(NPC->StageId);
            }
            if (Member->CurrentHP > 0.f && ImproveEquipment(Run, *Member, VisitKey)) return false;
            const FGameplayTag Tag = Encounter->GetResolvedTag();
            const bool bRecovery = Tag.MatchesTag(FRunEncounterOffer::GetRecoveryTag());
            const bool bRevival = Tag.MatchesTag(FRunEncounterOffer::GetRevivalTag());
            const bool bConsumable = Tag.MatchesTag(FRunEncounterOffer::GetConsumableShopTag());
            const FRunRecoveryState& Rules = Run->GetRecoveryState();
            const int32 ServicePrice = bRecovery ? Rules.RecoveryPrice : bRevival ? Rules.RevivalPrice : Rules.ConsumablePrice;
            if ((bRecovery || bRevival || bConsumable) && !AttemptedServices.Contains(VisitKey) && Member->Gold >= ServicePrice && (bRecovery ? Member->CurrentHP > 0.f && Member->CurrentHP < Profession.MaxHP : bRevival ? Member->CurrentHP == 0.f : Member->CurrentHP > 0.f))
            {
                const float ExpectedHP = bConsumable ? Member->CurrentHP : bRevival ? Profession.MaxHP * Rules.RevivalFraction : FMath::Min(Profession.MaxHP, Member->CurrentHP + Rules.RecoveryHP);
                if (!RequestServiceUI(Run, *Member, Tag.GetTagName(), Tag, ServicePrice, ExpectedHP, bConsumable, false)) return bPassed ? false : End();
                AttemptedServices.Add(VisitKey);
                LastProgress = FPlatformTime::Seconds();
                return false;
            }
            if (Tag.MatchesTag(FRunEncounterOffer::GetSkillShopTag()) && Member->CurrentHP > 0.f)
            {
                const FRunSkillShopState& Shop = Run->GetSkillShopState();
                if (Member->CurrentHP < Profession.MaxHP && Member->Gold >= Shop.Recovery.Price && !AttemptedServices.Contains(VisitKey + TEXT(":full_recovery")))
                {
                    if (!RequestServiceUI(Run, *Member, FRunSkillShopState::GetRecoveryOfferId(), FRunEncounterOffer::GetRecoveryTag(), Shop.Recovery.Price, Profession.MaxHP, false, true)) return bPassed ? false : End();
                    AttemptedServices.Add(VisitKey + TEXT(":full_recovery"));
                    LastProgress = FPlatformTime::Seconds();
                    return false;
                }
                const FRunSkillShopOffer* Best = nullptr;
                float BestPower = 0.f;
                for (const FRunSkillShopOffer& Offer : Shop.Offers)
                {
                    const FString Key = VisitKey + TEXT(":") + Offer.OfferId.ToString();
                    if (Offer.Price > Member->Gold || Member->Skills.Contains(Offer.Skill) || AttemptedServices.Contains(Key)) continue;
                    const USkillDefinitionDataAsset* Asset = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
                    FCombatRoundSkill Skill;
                    if (!Asset || !Asset->ResolveRoundSkill(Skill, Error)) continue;
                    const FGameplayTagContainer Tags = EffectiveTags(Skill);
                    float Utility = Tags.HasTag(ProjectACombatTags::Skill_Effect_Damage) ? Skill.Power : Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal) ? Skill.Power * 2.f : 0.f;
                    if (CombatRoundRules::UsesChain(Skill)) for (int32 Jump = 1; Jump < Skill.Chain.MaxTargets; ++Jump) Utility += Skill.Power * FMath::Pow(Skill.Chain.DamageMultiplierPerJump, static_cast<float>(Jump));
                    if (!Best || Utility > BestPower)
                    {
                        Best = &Offer;
                        BestPower = Utility;
                    }
                }
                if (Best && BestPower > 0.f)
                {
                    AttemptedServices.Add(VisitKey + TEXT(":") + Best->OfferId.ToString());
                    Event(Run, TEXT("request_offered_skill_purchase"), Best->OfferId.ToString());
                    Controller->RequestPurchaseShopOffer(Member->CharacterId, Best->OfferId, Shop.Revision);
                    LastProgress = FPlatformTime::Seconds();
                    return false;
                }
            }
            Event(Run, TEXT("request_leave_encounter"), Controller->GetShopPurchaseMessage().ToString());
            Controller->RequestLeaveRunEncounter();
            return false;
        }

        bool ClaimAndContinue(URunStateSubsystem* Run)
        {
            if (!Check(Run->GetLastResult() == ECombatResult::Victory, TEXT("Only an actual victory offers normal Run continuation."))) return End();
            if (!bRestartRequested)
            {
                FText Error;
                RestartSave.Reset(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
                if (!Check(RestartSave.IsValid() && SameSavedBoundary(*RestartSave.Get(), Run), TEXT("The first real victory is durable before actual menu Continue: ") + Error.ToString())) return End();
                if (!Capture(Controller->GetWorld(), TEXT("BeforeActualContinue"))) return End();
                Event(Run, TEXT("request_real_menu_restart"));
                bRestartRequested = true;
                UnbindConsumable();
                RetainViewport(Controller->GetWorld());
                GEditor->RequestEndPlayMap();
                Stage = 5;
                LastProgress = FPlatformTime::Seconds();
                return false;
            }
            if (Controller->IsRewardSelectionPending()) return false;
            const FRunGoldRewardState& Reward = Run->GetGoldRewardState();
            for (const FGuid& CharacterId : Run->GetGoldRewardRecipientIds())
            {
                if (Reward.Claims.ContainsByPredicate([CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; })) continue;
                int32 Choice = 0;
                if (Reward.SchemaVersion == 2)
                {
                    if (!Check(Reward.ItemChoices.Num() == 3 && Reward.GoldChoices.IsEmpty() && Reward.BonusGold > 0, TEXT("A normal weapon Run offers three actual items and one shared gold amount."))) return End();
                    for (int32 Index = 1; Index < Reward.ItemChoices.Num(); ++Index) if (SkillUtility(Reward.ItemChoices[Index].GrantedSkills) > SkillUtility(Reward.ItemChoices[Choice].GrantedSkills)) Choice = Index;
                    Event(Run, TEXT("request_offered_item_reward"), Reward.ItemChoices[Choice].DisplayName.ToString());
                }
                else
                {
                    if (!Check(!Reward.GoldChoices.IsEmpty(), TEXT("A legacy pending human reward has actual authored choices."))) return End();
                    for (int32 Index = 1; Index < Reward.GoldChoices.Num(); ++Index) if (Reward.GoldChoices[Index] > Reward.GoldChoices[Choice]) Choice = Index;
                    Event(Run, TEXT("request_offered_gold_reward"), FString::FromInt(Reward.GoldChoices[Choice]));
                }
                Controller->RequestSelectGoldReward(CharacterId, Run->GetCurrentNodeId(), Choice);
                LastProgress = FPlatformTime::Seconds();
                return false;
            }
            if (!Check(Run->CanContinueAfterRewards(), TEXT("The actual victory can continue after its required public reward claims."))) return End();
            Event(Run, TEXT("request_continue_run"));
            Controller->RequestContinueRun();
            return false;
        }

        // Evaluate visible loadouts on copies; the production requests still validate and save every change.
        // 표시된 장비를 사본에서 평가하며 실제 변경은 기존 요청의 검증과 저장을 거칩니다.
        static float SkillUtility(const TArray<FSoftObjectPath>& Paths)
        {
            float Damage = 0.f;
            float Healing = 0.f;
            float Shield = 0.f;
            for (const FSoftObjectPath& Path : Paths)
            {
                const USkillDefinitionDataAsset* Asset = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
                FCombatRoundSkill Skill;
                FText Error;
                if (!Asset || !Asset->ResolveRoundSkill(Skill, Error)) continue;
                const FGameplayTagContainer Tags = EffectiveTags(Skill);
                float Power = Skill.Power / FMath::Max(1, Skill.ActionPointCost);
                if (CombatRoundRules::UsesChain(Skill)) for (int32 Jump = 1; Jump < Skill.Chain.MaxTargets; ++Jump) Power += Skill.Power * FMath::Pow(Skill.Chain.DamageMultiplierPerJump, static_cast<float>(Jump)) / FMath::Max(1, Skill.ActionPointCost);
                if (Tags.HasTag(ProjectACombatTags::Skill_Effect_Damage)) Damage = FMath::Max(Damage, Power);
                if (Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal)) Healing = FMath::Max(Healing, Power);
                if (Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield)) Shield = FMath::Max(Shield, Power);
            }
            return Damage + Healing * 0.8f + Shield * 0.3f;
        }

        bool ImproveEquipment(URunStateSubsystem* Run, const FRunPartyMember& Member, const FString& VisitKey)
        {
            const float CurrentUtility = SkillUtility(Member.Skills);
            float BestUtility = CurrentUtility;
            FRunEquipmentCommand Best;
            Best.CharacterId = Member.CharacterId;
            Best.ExpectedRevision = Member.Equipment.Revision;
            for (int32 Index = 0; Index < Member.Items.Num(); ++Index)
            {
                for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
                {
                    FRunEquipmentCommand Command = Best;
                    Command.ItemIndex = Index;
                    Command.TargetSlot = URunEquipmentCatalog::GetWeaponSlot(SlotIndex);
                    FRunPartyMember Candidate = Member;
                    FText Error;
                    if (!RunEquipmentRules::Apply(Candidate, Command, Error)) continue;
                    const float Utility = SkillUtility(Candidate.Skills);
                    if (Utility <= BestUtility + KINDA_SMALL_NUMBER) continue;
                    BestUtility = Utility;
                    Best = Command;
                }
            }
            if (Best.ItemIndex != INDEX_NONE)
            {
                Event(Run, TEXT("request_owned_equipment_upgrade"), FString::FromInt(Best.ItemIndex));
                Controller->RequestChangeEquipment(Best);
                ++EquipmentRequests;
                return true;
            }
            if (!Run->GetEncounterProgress().IsItemShop()) return false;
            const FRunItemShopOffer* Purchase = nullptr;
            for (const FRunItemShopOffer& Offer : Run->GetItemShopState().Offers)
            {
                if (Offer.bSold || Offer.Item.Price > Member.Gold - 2 || AttemptedServices.Contains(VisitKey + TEXT(":") + Offer.OfferId.ToString())) continue;
                for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
                {
                    FRunPartyMember Candidate = Member;
                    FRunEquipmentCommand Command;
                    Command.CharacterId = Member.CharacterId;
                    Command.ExpectedRevision = Member.Equipment.Revision;
                    Command.ItemIndex = Candidate.Items.Add(Offer.Item);
                    Command.TargetSlot = URunEquipmentCatalog::GetWeaponSlot(SlotIndex);
                    FText Error;
                    if (!RunEquipmentRules::Apply(Candidate, Command, Error)) continue;
                    const float Utility = SkillUtility(Candidate.Skills);
                    if (Utility <= BestUtility + KINDA_SMALL_NUMBER) continue;
                    BestUtility = Utility;
                    Purchase = &Offer;
                }
            }
            if (!Purchase) return false;
            AttemptedServices.Add(VisitKey + TEXT(":") + Purchase->OfferId.ToString());
            Event(Run, TEXT("request_offered_item_purchase"), Purchase->OfferId.ToString());
            Controller->RequestPurchaseShopOffer(Member.CharacterId, Purchase->OfferId, Run->GetItemShopState().Revision);
            ++ItemPurchaseRequests;
            return true;
        }

        static FGameplayTagContainer EffectiveTags(const FCombatRoundSkill& Skill)
        {
            FGameplayTagContainer Tags = Skill.EffectTags;
            // Resolve the executor's native fallback before reading tags, without changing the skill.
            // 스킬을 바꾸지 않고 실행기가 사용하는 native 대체 효과를 해석한 뒤 태그를 읽습니다.
            const UGameplayEffect* Effect = Skill.EffectClass.GetDefaultObject();
            if (!Effect) Effect = Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal) ? GetDefault<UGE_Heal>() : Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield) ? static_cast<const UGameplayEffect*>(GetDefault<UGE_Shield>()) : GetDefault<UGE_Damage>();
            if (Effect) Tags.AppendTags(Effect->GetAssetTags());
            return Tags;
        }

        void UnbindConsumable()
        {
            if (ConsumableSource.IsValid() && ConsumableSource->GetAbilitySystemComponent())
            {
                UAbilitySystemComponent* ASC = ConsumableSource->GetAbilitySystemComponent();
                ASC->OnGameplayEffectAppliedDelegateToTarget.Remove(ConsumableGasHandle);
                ASC->GetGameplayAttributeValueChangeDelegate(UAS_Unit::GetHPAttribute()).Remove(ConsumableHealthHandle);
            }
            ConsumableGasHandle.Reset();
            ConsumableHealthHandle.Reset();
            ConsumableSource.Reset();
            ConsumableEffectContext = FGameplayEffectContextHandle();
        }

        // Compare restored actors as well as serialized data before any new plan can spend AP or stock.
        // 새 계획이 AP나 재고를 사용하기 전에 직렬화 데이터와 복구된 액터를 함께 대조합니다.
        bool VerifyLivePlanningBoundary(const FCombatCheckpointData& Checkpoint, ACombatRoundCoordinator* Round)
        {
            const FCombatRoundView& View = Round->GetView();
            ACombatManager* Manager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(Controller->GetWorld(), ACombatManager::StaticClass()));
            if (!Check(Manager && View.Phase == ECombatRoundPhase::Planning && View.RoundNumber == Checkpoint.RoundNumber && View.PlanRevision == Checkpoint.PlanRevision && View.Units.Num() == Checkpoint.Units.Num(), TEXT("The live planning session retains the saved round, plan revision and complete unit roster."))) return false;
            for (const FCombatCheckpointUnit& Saved : Checkpoint.Units)
            {
                const FCombatRoundUnitView* Entry = View.Units.FindByPredicate([&Saved](const FCombatRoundUnitView& Candidate) { return Candidate.UnitId == Saved.RoundUnitId; });
                const FCombatCheckpointRoundPlan* Plan = Checkpoint.RoundPlans.FindByPredicate([&Saved](const FCombatCheckpointRoundPlan& Candidate) { return Candidate.UnitId == Saved.RoundUnitId; });
                AUnitBase* Unit = Entry ? Entry->Unit.Get() : nullptr;
                const UAS_Unit* Attributes = Unit ? Unit->GetAttributeSet() : nullptr;
                if (!Check(Unit && Attributes && Plan && FSoftObjectPath(Unit->GetClass()) == Saved.UnitClass && Unit->GetTeam() == Saved.Team && Unit->IsUnitAlive() == !Saved.bDead && Manager->GetCharacterId(Unit) == Saved.CharacterId && Manager->GetOwnerAccountId(Unit) == Saved.OwnerAccountId, TEXT("Each live checkpoint unit keeps its class, team, identity, owner and natural death state."))) return false;
                if (!Check(FMath::IsNearlyEqual(Attributes->GetHP(), Saved.HP) && FMath::IsNearlyEqual(Attributes->GetMaxHP(), Saved.MaxHP) && FMath::IsNearlyEqual(Attributes->GetSpeed(), Saved.Speed) && Attributes->GetShield() == 0.f && Unit->GetCurrentActionPoint() == Saved.AP && Unit->GetMaxActionPoint() == Saved.MaxAP && Unit->GetCurrentSubActionPoint() == Saved.SubAP && Unit->GetMaxSubActionPoint() == Saved.MaxSubAP && Unit->GetMoveRange() == Saved.MoveRange, TEXT("Each live unit preserves saved HP, maximum stats, AP and SAP without a reset or replayed consumable cost."))) return false;
                if (!Check(Unit->Consumables.Num() == Saved.Consumables.Num() && Unit->HealingItemCount == Saved.HealingItemCount && FMath::IsNearlyEqual(Unit->HealingItemAmount, Saved.HealingItemAmount), TEXT("Each live unit retains the complete saved consumable inventory."))) return false;
                for (int32 Index = 0; Index < Saved.Consumables.Num(); ++Index) if (!Check(FRunConsumableStack::StaticStruct()->CompareScriptStruct(&Unit->Consumables[Index], &Saved.Consumables[Index], 0), TEXT("Every restored consumable stack preserves its exact tag, skill and decremented quantity."))) return false;
                if (!Check(Entry->HomeCoord == Saved.GridCoord && (Saved.bHasTile ? Unit->GetCurrentTile() && Unit->GetCurrentTile()->GridCoord == Saved.GridCoord && Unit->GetCurrentTile()->GetOccupyingUnit() == Unit : Unit->GetCurrentTile() == nullptr), TEXT("The saved live grid placement and dead-unit vacancy are preserved."))) return false;
                // CapturePlanningCheckpoint clears unused dead-unit coordinates; normalize only this comparison copy.
                // 저장기가 사망자의 미사용 명령 좌표를 지우므로 비교용 사본만 동일하게 정규화합니다.
                FCombatRoundCommand ComparableCommand = Entry->Command;
                if (Saved.bDead)
                {
                    ComparableCommand.TargetCoord = FIntPoint::ZeroValue;
                    ComparableCommand.DestinationCoord = FIntPoint::ZeroValue;
                }
                if (!Check(FCombatRoundCommand::StaticStruct()->CompareScriptStruct(&ComparableCommand, &Plan->Command, 0) && Entry->bHasMovePlan == Plan->bHasMovePlan && Entry->MoveDestinationCoord == Plan->MoveDestinationCoord && Entry->bReady == (Saved.bDead || Entry->OwnerSlot == 0 || Plan->bReady), TEXT("The actual restored plans and readiness equal the saved boundary before another normal request."))) return false;
            }
            return true;
        }

        bool RestartAfterConsumable(URunStateSubsystem* Run, ACombatRoundCoordinator* Round)
        {
            FText Error;
            ConsumableRestartSave.Reset(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Check(ConsumableRestartSave.IsValid() && SameSavedBoundary(*ConsumableRestartSave.Get(), Run) && FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&ConsumableRestartSave->CombatCheckpoint, &Run->GetCombatCheckpoint(), 0), TEXT("The next stable planning boundary after an observed consumable release is durable before real menu restart: ") + Error.ToString()) || !VerifyLivePlanningBoundary(ConsumableRestartSave->CombatCheckpoint, Round)) return End();
            if (!Capture(Controller->GetWorld(), TEXT("BeforeConsumableActualContinue"))) return End();
            Event(Run, TEXT("request_post_consumable_real_menu_restart"), FString::Printf(TEXT("consumable_combat=%s consumable_round=%d attempt=%s revision=%lld round=%d"), *ConsumableCombatId.ToString(), ConsumableRound, *Run->GetCombatCheckpoint().AttemptId.ToString(), Run->GetCombatCheckpoint().Revision, Run->GetCombatCheckpoint().RoundNumber));
            ConsumableRestartCombatId = Round->GetView().CombatId;
            bConsumableRestartRequested = bResumingConsumableBoundary = true;
            // Discard only observer state from the old world; production checkpoint plans remain untouched.
            // 이전 월드의 관측기 상태만 비우고 실제 체크포인트 계획은 변경하지 않습니다.
            UnbindConsumable();
            bAwaitingPlan = false;
            PendingCommand = FCombatRoundCommand();
            PlannedRounds.Reset();
            VerifiedBoundaries.Reset();
            LastSignature.Reset();
            ConsumablePlanningWaitKey.Reset();
            RetainViewport(Controller->GetWorld());
            GEditor->RequestEndPlayMap();
            Stage = 5;
            LastProgress = FPlatformTime::Seconds();
            return false;
        }

        bool AwaitConsumablePlanningUI(URunStateSubsystem* Run, ACombatRoundCoordinator* Round, const FCombatRoundSkill& Skill)
        {
            const FCombatRoundView& View = Round->GetView();
            const FString Key = View.CombatId.ToString() + TEXT(":") + FString::FromInt(View.RoundNumber);
            const FString ExpectedHeader = FString::Printf(TEXT("라운드 %d · 행동 선택"), View.RoundNumber);
            UCombatRoundPlanningWidget* Planning = Screen<UCombatRoundPlanningWidget>(Controller->GetWorld());
            FString DisplayedHeader;
            bool bButtonExists = false;
            if (Planning && Planning->WidgetTree) Planning->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                const UTextBlock* Label = Cast<UTextBlock>(Widget);
                if (Label && Label->GetText().ToString().StartsWith(TEXT("라운드 "))) DisplayedHeader = Label->GetText().ToString();
                const UCombatRoundSkillButton* Button = Cast<UCombatRoundSkillButton>(Widget);
                if (Button && Button->GetSkillId() == Skill.SkillId) bButtonExists = true;
            });
            if (ConsumablePlanningWaitKey != Key)
            {
                ConsumablePlanningWaitKey = Key;
                ConsumablePlanningFirstHeader = DisplayedHeader;
                ConsumablePlanningWaitAt = FPlatformTime::Seconds();
            }
            // Let the displayed round refresh before the single target click; a stale round refresh clears that selection.
            // 이전 라운드 갱신이 선택을 지우지 않도록 화면의 현재 라운드 갱신 후 대상을 한 번 클릭합니다.
            const bool bReady = Planning && DisplayedHeader == ExpectedHeader && bButtonExists;
            if (!bReady && FPlatformTime::Seconds() - ConsumablePlanningWaitAt < 5.0) return false;
            Event(Run, TEXT("consumable_ui_planning_readiness"), FString::Printf(TEXT("screen=%s first_header=%s displayed_header=%s expected_header=%s button_exists=%d wait_seconds=%.3f"), *GetNameSafe(Planning), *ConsumablePlanningFirstHeader, *DisplayedHeader, *ExpectedHeader, bButtonExists, FPlatformTime::Seconds() - ConsumablePlanningWaitAt));
            if (!bReady) Capture(Controller->GetWorld(), TEXT("ConsumablePlanningNotReady"));
            return Check(bReady, TEXT("The actual planning screen displays the current action-selection round and original consumable button before one target selection."));
        }

        bool RequestConsumableUI(URunStateSubsystem* Run, ACombatRoundCoordinator* Round, const FCombatRoundUnitView& Direct, const FCombatRoundSkill& Skill)
        {
            UCombatRoundPlanningWidget* Planning = Screen<UCombatRoundPlanningWidget>(Controller->GetWorld());
            if (!Check(Planning && Planning->WidgetTree && Direct.Unit && Direct.Unit->GetAbilitySystemComponent(), TEXT("A legal consumable is displayed by the actual active planning screen."))) return false;
            // Exercise the existing world-selection and button delegates; no private widget state or command is injected.
            // 위젯 내부 상태나 명령을 주입하지 않고 기존 전장 선택·버튼 delegate를 실행합니다.
            Controller->OnRoundWorldUnitClicked.Broadcast(Direct.UnitId);
            UCombatRoundSkillButton* Button = nullptr;
            Planning->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                UCombatRoundSkillButton* Candidate = Cast<UCombatRoundSkillButton>(Widget);
                if (Candidate && Candidate->GetSkillId() == Skill.SkillId) Button = Candidate;
            });
            Event(Run, TEXT("consumable_ui_button_selected"), FString::Printf(TEXT("skill=%s button=%s visible=%d enabled=%d tooltip=%s"), *Skill.SkillId.ToString(), *GetNameSafe(Button), Button && Visible(Button), Button && Button->GetIsEnabled(), Button ? *Button->GetToolTipText().ToString() : TEXT("missing")));
            if (!Check(Button && Visible(Button) && Button->GetIsEnabled(), TEXT("The actual tagged consumable button is visible and enabled after selecting the owned unit."))) return false;
            const FRunConsumableStack* Stack = Direct.Unit->Consumables.FindByPredicate([&Skill](const FRunConsumableStack& Entry) { return Skill.EffectTags.HasTagExact(Entry.ItemTag); });
            const FRunPartyMember* Member = DirectMember(Run);
            if (!Check(Stack && Stack->Quantity > 0 && Member, TEXT("The UI request uses available authored stock without a quantity override."))) return false;
            UnbindConsumable();
            ConsumableSource = Direct.Unit;
            ConsumableCharacterId = Member->CharacterId;
            ObservedConsumableSkill = Skill;
            ConsumableCombatId = Round->GetView().CombatId;
            ConsumableRound = Round->GetView().RoundNumber;
            ConsumableQuantityBefore = Stack->Quantity;
            ConsumableAPBefore = Direct.Unit->GetCurrentActionPoint();
            ConsumableGasCount = 0;
            ConsumableHealthChangeCount = 0;
            ConsumableEffectContext = FGameplayEffectContextHandle();
            ConsumableHPBefore = ConsumableHPAfter = 0.f;
            bConsumableContext = true;
            ConsumableBoundaryAt = 0.0;
            UAbilitySystemComponent* ASC = Direct.Unit->GetAbilitySystemComponent();
            ConsumableHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAS_Unit::GetHPAttribute()).AddLambda([this](const FOnAttributeChangeData& Change)
            {
                if (Change.NewValue <= Change.OldValue || !Change.GEModData || !ConsumableSource.IsValid()) return;
                const FGameplayEffectModCallbackData& Mod = *Change.GEModData;
                FGameplayTagContainer Tags;
                Mod.EffectSpec.GetAllAssetTags(Tags);
                if (!Tags.HasAll(ObservedConsumableSkill.EffectTags) || &Mod.Target != ConsumableSource->GetAbilitySystemComponent() || Mod.EffectSpec.GetContext().GetOriginalInstigator() != ConsumableSource.Get()) return;
                // Freeze the first matching HP change and correlate it with the same applied effect context.
                // 첫 일치 HP 변화만 고정하고 동일한 적용 효과 context와 대조합니다.
                if (++ConsumableHealthChangeCount != 1) return;
                ConsumableEffectContext = Mod.EffectSpec.GetContext();
                ConsumableHPBefore = Change.OldValue;
                ConsumableHPAfter = Change.NewValue;
            });
            ConsumableGasHandle = ASC->OnGameplayEffectAppliedDelegateToTarget.AddLambda([this](UAbilitySystemComponent* Recipient, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
            {
                FGameplayTagContainer Tags;
                Spec.GetAllAssetTags(Tags);
                if (!Tags.HasAll(ObservedConsumableSkill.EffectTags)) return;
                ++ConsumableGasCount;
                bConsumableContext &= ConsumableSource.IsValid() && Recipient == ConsumableSource->GetAbilitySystemComponent() && Spec.GetContext().GetOriginalInstigator() == ConsumableSource.Get() && ConsumableHealthChangeCount == 1 && ConsumableEffectContext.IsValid() && Spec.GetContext() == ConsumableEffectContext;
            });
            ++ConsumableUIRequests;
            Button->OnClicked.Broadcast();
            return true;
        }

        bool ReadyConsumableUI()
        {
            UCombatRoundPlanningWidget* Planning = Screen<UCombatRoundPlanningWidget>(Controller->GetWorld());
            UButton* Ready = nullptr;
            if (Planning && Planning->WidgetTree) Planning->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                UButton* Button = Cast<UButton>(Widget);
                UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
                if (Label && Label->GetText().ToString() == TEXT("준비 완료")) Ready = Button;
            });
            if (!Check(Ready && Visible(Ready) && Ready->GetIsEnabled() && ConsumableSource.IsValid(), TEXT("The accepted consumable plan enables the actual Ready button."))) return false;
            if (!Capture(Controller->GetWorld(), FString::Printf(TEXT("ConsumableUI_%02d_Planned"), ConsumableUIRequests))) return false;
            Ready->OnClicked.Broadcast();
            bObserveConsumableResolving = true;
            return Check(ConsumableSource->GetCurrentActionPoint() == ConsumableAPBefore - ObservedConsumableSkill.ActionPointCost && ConsumableSource->Consumables[0].Quantity == ConsumableQuantityBefore, TEXT("The original Ready button spends authored AP once and retains stock until actual release."));
        }

        bool ObserveConsumableBoundary(URunStateSubsystem* Run, ACombatRoundCoordinator* Round)
        {
            const bool bBoundary = Run->GetPhase() == ERunPhase::Result || Run->GetPhase() == ERunPhase::Defeat || Run->GetPhase() == ERunPhase::Complete || (Run->GetPhase() == ERunPhase::Combat && Round && Round->GetView().CombatId == ConsumableCombatId && Round->GetView().RoundNumber > ConsumableRound && Round->GetView().Phase == ECombatRoundPhase::Planning);
            if (!bBoundary) return false;
            if (ConsumableBoundaryAt == 0.0) ConsumableBoundaryAt = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds() - ConsumableBoundaryAt < 0.25) return false;
            const FRunPartyMember* Member = DirectMember(Run);
            const FRunConsumableStack* Stack = Member ? Member->Consumables.FindByPredicate([this](const FRunConsumableStack& Entry) { return ObservedConsumableSkill.EffectTags.HasTagExact(Entry.ItemTag); }) : nullptr;
            const bool bApplied = ConsumableGasCount == 1;
            if (!Check(Member && Member->CharacterId == ConsumableCharacterId && Stack && ConsumableGasCount <= 1 && ConsumableHealthChangeCount == ConsumableGasCount && bConsumableContext && Stack->Quantity == ConsumableQuantityBefore - (bApplied ? 1 : 0), TEXT("The next published boundary retains one correlated GAS release, HP change, and stock decrement, or preserves stock after interruption."))) return false;
            if (bApplied && !Check(ConsumableHPAfter > ConsumableHPBefore && ConsumableHPAfter - ConsumableHPBefore <= ObservedConsumableSkill.Power + KINDA_SMALL_NUMBER, TEXT("The actual self-targeted GAS callback observes positive authored healing without a health override."))) return false;
            if (!VerifySavedBoundary(Run, LastSignature) || !Capture(Controller->GetWorld(), FString::Printf(TEXT("ConsumableUI_%02d_Boundary"), ConsumableUIRequests))) return false;
            TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
            Record->SetStringField(TEXT("skill"), ObservedConsumableSkill.SkillId.ToString());
            Record->SetStringField(TEXT("combat_id"), ConsumableCombatId.ToString());
            Record->SetNumberField(TEXT("round"), ConsumableRound);
            Record->SetNumberField(TEXT("hp_before_gas"), ConsumableHPBefore);
            Record->SetNumberField(TEXT("hp_after_gas"), ConsumableHPAfter);
            Record->SetNumberField(TEXT("ap_paid_at_ready"), ObservedConsumableSkill.ActionPointCost);
            Record->SetNumberField(TEXT("gas_applications"), ConsumableGasCount);
            Record->SetNumberField(TEXT("correlated_hp_changes"), ConsumableHealthChangeCount);
            Record->SetBoolField(TEXT("same_effect_context_verified"), bApplied && bConsumableContext);
            Record->SetNumberField(TEXT("quantity_before"), ConsumableQuantityBefore);
            Record->SetNumberField(TEXT("quantity_at_saved_boundary"), Stack->Quantity);
            Record->SetBoolField(TEXT("applied"), bApplied);
            ConsumableObservations.Add(MakeShared<FJsonValueObject>(Record));
            if (bApplied) ++ConsumableUIUses;
            Event(Run, bApplied ? TEXT("actual_consumable_ui_release_verified") : TEXT("actual_consumable_ui_interrupted_stock_preserved"));
            UnbindConsumable();
            bObserveConsumableResolving = false;
            return true;
        }

        bool PlanRound(URunStateSubsystem* Run, ACombatRoundCoordinator* Round)
        {
            if (Controller->IsRoundRequestPending()) return false;
            const FCombatRoundView View = Round->GetView();
            if (!Check(View.RoundNumber <= 100, TEXT("One normal combat stays within the one-hundred-round observation budget; no forced result is applied."))) return End();
            const FCombatRoundUnitView* Direct = FindLivingDirectUnit(Run, View);
            if (!Direct)
            {
                const FRunPartyMember* Member = DirectMember(Run);
                if (Member && Member->CurrentHP == 0.f)
                {
                    bAwaitingPlan = false;
                    PendingCommand = FCombatRoundCommand();
                    if (!ObservedSpectatorCombats.Contains(View.CombatId))
                    {
                        ObservedSpectatorCombats.Add(View.CombatId);
                        Event(Run, TEXT("observe_companions_after_direct_death"), FString::Printf(TEXT("participant_slot=%d; original_character=%s; no companion plan or Ready request is issued"), Controller->GetRoundParticipantSlot(), *Member->CharacterId.ToString()));
                    }
                }
                return false;
            }
            if (!Controller->IsRoundInputEnabled()) return false;
            if (!VerifiedCombatStats.Contains(View.CombatId))
            {
                ACombatManager* Manager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(Controller->GetWorld(), ACombatManager::StaticClass()));
                if (!Check(Manager != nullptr, TEXT("Normal planning resolves the actual combat manager."))) return End();
                for (const FCombatRoundUnitView& Entry : View.Units)
                {
                    if (Entry.bEnemy || !Entry.Unit) continue;
                    const FGuid CharacterId = Manager->GetCharacterId(Entry.Unit);
                    const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId; });
                    FProfessionDefinition Profession;
                    FText Error;
                    const UAS_Unit* Attributes = Entry.Unit->GetAttributeSet();
                    if (!Check(Member && Attributes && Run->ResolveMemberProfession(*Member, Profession, Error) && FMath::IsNearlyEqual(Attributes->GetMaxHP(), Profession.MaxHP) && Entry.Unit->GetMaxActionPoint() == Profession.ActionPoints && Entry.Unit->GetMaxSubActionPoint() == Profession.SubActionPoints && FMath::IsNearlyEqual(Attributes->GetHP(), Member->CurrentHP), TEXT("Each original character starts combat with the authored growth HP/AP/SAP and its saved, unadjusted current health: ") + Error.ToString())) return End();
                }
                VerifiedCombatStats.Add(View.CombatId);
                Event(Run, TEXT("authored_combat_stats_verified"));
            }
            if (!bCapturedFirstPlanning)
            {
                if (FirstPlanningStarted == 0.0) FirstPlanningStarted = FPlatformTime::Seconds();
                if (!FirstPlanningPresentation.Poll(Controller->GetWorld(), Round, PresentationReport))
                {
                    if (FPlatformTime::Seconds() - FirstPlanningStarted < 30.0) return false;
                    Check(false, TEXT("Normal first planning did not produce its authored camera POV, visible unit meshes and grid in natural frames."));
                    Capture(Controller->GetWorld(), TEXT("NormalFirstPlanningPresentationNotReady"));
                    return End();
                }
                if (!Capture(Controller->GetWorld(), TEXT("NormalFirstPlanning"))) return End();
                bCapturedFirstPlanning = true;
            }
            if (bAwaitingPlan)
            {
                if (!Check(FCombatRoundCommand::StaticStruct()->CompareScriptStruct(&Direct->Command, &PendingCommand, 0), TEXT("The normal player's public plan is accepted without changing any companion AI plan: ") + Controller->GetRoundRequestStatus().ToString())) return End();
                bAwaitingPlan = false;
                const FCombatRoundSkill* AcceptedSkill = Round->FindSkill(PendingCommand.SkillId);
                if (!Check(AcceptedSkill != nullptr, TEXT("The accepted normal plan retains its actual skill profile."))) return End();
                if (RunRecoveryRules::IsConsumable(*AcceptedSkill))
                {
                    if (!ReadyConsumableUI()) return End();
                }
                else Controller->SetRoundReady(true);
                return false;
            }
            const FString RoundKey = View.CombatId.ToString() + TEXT(":") + FString::FromInt(View.RoundNumber);
            if (PlannedRounds.Contains(RoundKey))
            {
                if (!Check(Direct->bReady, TEXT("The real Ready request retains readiness until normal resolution: ") + Controller->GetRoundRequestStatus().ToString())) return End();
                return false;
            }
            for (const FCombatRoundUnitView& Entry : View.Units)
            {
                if (Entry.bEnemy || !Entry.Unit || !Entry.Unit->IsUnitAlive() || Entry.UnitId == Direct->UnitId) continue;
                if (!Check(Entry.OwnerSlot == 0 && Entry.bReady, TEXT("Living companions retain their production AI control and fixed ready plans."))) return End();
            }
            float IncomingPlannedPower = 0.f;
            for (const FCombatRoundUnitView& Enemy : View.Units)
            {
                if (!Enemy.bEnemy || !Enemy.Unit || !Enemy.Unit->IsUnitAlive() || Enemy.Command.TargetUnitId != Direct->UnitId) continue;
                const FCombatRoundSkill* Attack = Round->FindSkill(Enemy.Command.SkillId);
                if (Attack && EffectiveTags(*Attack).HasTag(ProjectACombatTags::Skill_Effect_Damage)) IncomingPlannedPower += Attack->Power;
            }
            FCombatRoundCommand Best;
            int32 BestPriority = MIN_int32;
            double BestUtility = -1.0;
            double BestDistance = TNumericLimits<double>::Max();
            for (FName SkillId : Direct->SkillIds)
            {
                const FCombatRoundSkill* Skill = Round->FindSkill(SkillId);
                if (!Skill || Skill->Kind == ECombatRoundSkillKind::Wait) continue;
                const FGameplayTagContainer Tags = EffectiveTags(*Skill);
                const bool bDamage = Tags.HasTag(ProjectACombatTags::Skill_Effect_Damage);
                const bool bHeal = Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal);
                const bool bShield = Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield);
                if (!bDamage && !bHeal && !bShield) continue;
                for (const FCombatRoundUnitView& Target : View.Units)
                {
                    if (!Target.Unit || !Target.Unit->IsUnitAlive() || !Target.Unit->GetAttributeSet() || (bDamage ? !Target.bEnemy : Target.bEnemy)) continue;
                    const float HP = Target.Unit->GetAttributeSet()->GetHP();
                    const float MaxHP = Target.Unit->GetAttributeSet()->GetMaxHP();
                    const double Utility = bHeal ? FMath::Min(Skill->Power, MaxHP - HP) : bShield ? FMath::Max(0.f, Skill->Power - Target.Unit->GetAttributeSet()->GetShield()) : FMath::Min(Skill->Power, HP);
                    if (Utility <= 0.0) continue;
                    // Heal before a visible queued attack crosses the lethal boundary, instead of waiting below 25% HP.
                    // HP 25% 미만까지 기다리지 않고 표시된 예약 공격이 치명 구간에 도달하기 전에 회복합니다.
                    const bool bDangerousSelfHeal = bHeal && Target.UnitId == Direct->UnitId && IncomingPlannedPower > 0.f && MaxHP - HP >= Skill->Power && HP <= IncomingPlannedPower + Skill->Power;
                    const int32 Priority = bHeal && (HP < MaxHP * 0.25f || bDangerousSelfHeal) ? 3 : bDamage && HP <= Skill->Power ? 2 : bDamage ? 1 : 0;
                    double EffectiveUtility = Utility;
                    if (bDamage && CombatRoundRules::UsesChain(*Skill)) for (int32 Jump = 1; Jump < Skill->Chain.MaxTargets; ++Jump) EffectiveUtility += Skill->Power * FMath::Pow(Skill->Chain.DamageMultiplierPerJump, static_cast<float>(Jump));
                    const double Distance = FVector::DistSquared2D(Direct->Unit->GetActorLocation(), Target.Unit->GetActorLocation());
                    FCombatRoundCommand Candidate;
                    Candidate.UnitId = Direct->UnitId;
                    Candidate.SkillId = SkillId;
                    Candidate.TargetUnitId = CombatRoundRules::UsesUnitTarget(*Skill) ? Target.UnitId : INDEX_NONE;
                    Candidate.TargetCoord = Target.HomeCoord;
                    Candidate.DestinationCoord = Direct->HomeCoord;
                    TArray<FIntPoint> Destinations{Direct->HomeCoord};
                    if (Skill->Approach == ECombatRoundApproach::Tile && Round->GetArena() && Round->GetArena()->Grid)
                    {
                        Round->GetArena()->Grid->TileMap.GetKeys(Destinations);
                        Destinations.Sort([](FIntPoint A, FIntPoint B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
                    }
                    for (FIntPoint Destination : Destinations)
                    {
                        Candidate.DestinationCoord = Destination;
                        FText Error;
                        if (!Round->CanPlanCommand(Candidate, Error)) continue;
                        const FCombatRoundSkill* Previous = Round->FindSkill(Best.SkillId);
                        // For equally useful legal healing, observe an owned consumable once through its actual UI.
                        // 합법 회복의 효용이 같을 때 보유 소모품을 한 번 실제 UI로 관측합니다.
                        const bool bObserveEquivalentConsumable = ConsumableUIUses == 0 && bHeal && RunRecoveryRules::IsConsumable(*Skill) && Previous && !RunRecoveryRules::IsConsumable(*Previous) && FMath::IsNearlyEqual(EffectiveUtility, BestUtility) && FMath::IsNearlyEqual(Distance, BestDistance);
                        if (Priority > BestPriority || (Priority == BestPriority && (EffectiveUtility > BestUtility || (FMath::IsNearlyEqual(EffectiveUtility, BestUtility) && (Distance < BestDistance || bObserveEquivalentConsumable)))))
                        {
                            Best = Candidate;
                            BestPriority = Priority;
                            BestUtility = EffectiveUtility;
                            BestDistance = Distance;
                        }
                    }
                }
            }
            const FCombatRoundSkill* SelectedSkill = Round->FindSkill(Best.SkillId);
            if (SelectedSkill && RunRecoveryRules::IsConsumable(*SelectedSkill) && !AwaitConsumablePlanningUI(Run, Round, *SelectedSkill)) return bPassed ? false : End();
            PlannedRounds.Add(RoundKey);
            ++RoundsSubmitted;
            if (BestPriority == MIN_int32)
            {
                Event(Run, TEXT("request_normal_wait"));
                Controller->SetRoundReady(true);
                return false;
            }
            PendingCommand = Best;
            bAwaitingPlan = true;
            Event(Run, TEXT("request_normal_plan"), FString::Printf(TEXT("skill=%s target=%d destination=%s"), *Best.SkillId.ToString(), Best.TargetUnitId, *Best.DestinationCoord.ToString()));
            if (SelectedSkill && RunRecoveryRules::IsConsumable(*SelectedSkill))
            {
                if (!RequestConsumableUI(Run, Round, *Direct, *SelectedSkill)) return End();
            }
            else Controller->SubmitRoundPlan(Best);
            LastProgress = FPlatformTime::Seconds();
            return false;
        }

        void Event(URunStateSubsystem* Run, const FString& Kind, const FString& Detail = FString())
        {
            TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
            Record->SetStringField(TEXT("event"), Kind);
            Record->SetStringField(TEXT("detail"), Detail);
            Record->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - RunStarted);
            Record->SetStringField(TEXT("phase"), UEnum::GetValueAsString(Run->GetPhase()));
            Record->SetStringField(TEXT("node"), Run->GetCurrentNodeId().ToString());
            Record->SetStringField(TEXT("encounter"), Run->GetCurrentEncounterId().ToString());
            CompletedCombats = Run->GetCompletedNodes().Num();
            CompletedChoices = Run->GetTargetRunState().CompletedEncounterChoices.Num();
            Record->SetNumberField(TEXT("completed_combats"), CompletedCombats);
            Record->SetNumberField(TEXT("completed_encounter_choices"), CompletedChoices);
            TArray<TSharedPtr<FJsonValue>> Party;
            for (const FRunPartyMember& Member : Run->GetPartyMembers())
            {
                if (!Member.bCreated) continue;
                TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("character_id"), Member.CharacterId.ToString());
                Entry->SetStringField(TEXT("class"), Member.ClassId.ToString());
                Entry->SetBoolField(TEXT("direct_control"), Member.bPlayerControlled);
                Entry->SetNumberField(TEXT("saved_hp"), Member.CurrentHP);
                Entry->SetNumberField(TEXT("gold"), Member.Gold);
                Entry->SetNumberField(TEXT("skills"), Member.Skills.Num());
                int32 Quantity = 0;
                for (const FRunConsumableStack& Stack : Member.Consumables) Quantity += Stack.Quantity;
                Entry->SetNumberField(TEXT("consumable_quantity"), Quantity);
                Party.Add(MakeShared<FJsonValueObject>(Entry));
            }
            Record->SetArrayField(TEXT("party"), Party);
            if (ACombatRoundCoordinator* Round = Controller.IsValid() ? Controller->GetRoundCoordinator() : nullptr)
            {
                Record->SetNumberField(TEXT("round"), Round->GetView().RoundNumber);
                TArray<TSharedPtr<FJsonValue>> Units;
                for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
                {
                    TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                    Entry->SetNumberField(TEXT("id"), Unit.UnitId);
                    Entry->SetBoolField(TEXT("enemy"), Unit.bEnemy);
                    Entry->SetNumberField(TEXT("owner_slot"), Unit.OwnerSlot);
                    Entry->SetNumberField(TEXT("hp"), Unit.Unit && Unit.Unit->GetAttributeSet() ? Unit.Unit->GetAttributeSet()->GetHP() : Unit.HP);
                    Entry->SetNumberField(TEXT("max_hp"), Unit.Unit && Unit.Unit->GetAttributeSet() ? Unit.Unit->GetAttributeSet()->GetMaxHP() : 0.f);
                    Entry->SetNumberField(TEXT("ap"), Unit.Unit ? Unit.Unit->GetCurrentActionPoint() : 0);
                    Entry->SetNumberField(TEXT("max_ap"), Unit.Unit ? Unit.Unit->GetMaxActionPoint() : 0);
                    Entry->SetNumberField(TEXT("sap"), Unit.Unit ? Unit.Unit->GetCurrentSubActionPoint() : 0);
                    Entry->SetNumberField(TEXT("max_sap"), Unit.Unit ? Unit.Unit->GetMaxSubActionPoint() : 0);
                    int32 Quantity = 0;
                    if (Unit.Unit) for (const FRunConsumableStack& Stack : Unit.Unit->Consumables) Quantity += Stack.Quantity;
                    Entry->SetNumberField(TEXT("consumable_quantity"), Quantity);
                    Entry->SetStringField(TEXT("status"), Unit.Status.ToString());
                    Entry->SetStringField(TEXT("skill"), Unit.Command.SkillId.ToString());
                    Entry->SetNumberField(TEXT("target"), Unit.Command.TargetUnitId);
                    if (const FCombatRoundSkill* Skill = Round->FindSkill(Unit.Command.SkillId))
                    {
                        Entry->SetNumberField(TEXT("skill_power"), Skill->Power);
                        Entry->SetNumberField(TEXT("skill_ap_cost"), Skill->ActionPointCost);
                        Entry->SetNumberField(TEXT("skill_sap_cost"), Skill->SubActionPointCost);
                        Entry->SetNumberField(TEXT("chain_max_targets"), Skill->Chain.MaxTargets);
                    }
                    Units.Add(MakeShared<FJsonValueObject>(Entry));
                }
                Record->SetArrayField(TEXT("units"), Units);
            }
            Events.Add(MakeShared<FJsonValueObject>(Record));
        }

        bool Capture(UWorld* World, const FString& Name)
        {
            if (!TodoReviewWindowPlacement::Ensure(Test, World)) return false;
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            TArray<FColor> Pixels;
            FIntVector Size = FIntVector::ZeroValue;
            if (!Check(Widget.IsValid() && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size) && Size.X > 0 && Size.Y > 0 && Pixels.Num() == Size.X * Size.Y, TEXT("The normal-run evidence captures the actual complete gameplay Slate viewport."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
            return Check(!PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *(Output / (Name + TEXT(".png")))), TEXT("The normal-run frame is saved only to its owned report directory."));
        }

        void RetainViewport(UWorld* World)
        {
            if (RetainedViewport.IsValid()) return;
            UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
        }

        void ReleaseViewport()
        {
            check(IsInGameThread());
            if (!RetainedViewport.IsValid()) return;
            FlushRenderingCommands();
            RetainedViewport.Reset();
        }

        bool End()
        {
            UnbindConsumable();
            if (Outcome.IsEmpty()) Outcome = TEXT("ValidationFailure");
            if (GEditor && GEditor->PlayWorld) RetainViewport(GEditor->PlayWorld);
            GEditor->RequestEndPlayMap();
            Stage = 99;
            PhaseStarted = FPlatformTime::Seconds();
            return false;
        }

        void FinishReport()
        {
            TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
            Report->SetStringField(TEXT("outcome"), Outcome);
            Report->SetBoolField(TEXT("passed_observation_contract"), bPassed && bTerminalObserved);
            Report->SetBoolField(TEXT("completed_all_eighty_stages"), Outcome == TEXT("Completed80Stages") && bPassed);
            Report->SetBoolField(TEXT("actual_menu_continue_verified"), bRestartVerified);
            Report->SetBoolField(TEXT("post_consumable_menu_restart_requested"), bConsumableRestartRequested);
            Report->SetBoolField(TEXT("actual_post_consumable_menu_continue_verified"), bConsumableRestartVerified);
            Report->SetStringField(TEXT("post_consumable_menu_continue_status"), bConsumableRestartVerified ? TEXT("Actual menu Continue after PIE restart preserved the next stable planning save and live unit HP/AP/SAP, stock, death, position and plans after a successful consumable release.") : bConsumableRestartRequested ? TEXT("Unverified: post-consumable menu restart was requested but restoration validation did not finish.") : TEXT("Unobserved: no eligible stable controllable planning boundary after a successful natural consumable release was reached; no HP, stock or result was forced."));
            Report->SetNumberField(TEXT("completed_combats"), CompletedCombats);
            Report->SetNumberField(TEXT("completed_encounter_choices"), CompletedChoices);
            Report->SetNumberField(TEXT("normal_rounds_submitted"), RoundsSubmitted);
            Report->SetNumberField(TEXT("spectator_combats_after_direct_death"), ObservedSpectatorCombats.Num());
            Report->SetNumberField(TEXT("saved_boundary_deserializations"), SavedBoundaryReads);
            Report->SetNumberField(TEXT("item_purchase_requests"), ItemPurchaseRequests);
            Report->SetNumberField(TEXT("equipment_change_requests"), EquipmentRequests);
            Report->SetNumberField(TEXT("npc_stages_observed"), ObservedStages.Num());
            Report->SetNumberField(TEXT("consumable_ui_requests"), ConsumableUIRequests);
            Report->SetNumberField(TEXT("consumable_ui_uses_observed"), ConsumableUIUses);
            Report->SetArrayField(TEXT("consumable_ui_observations"), ConsumableObservations);
            Report->SetNumberField(TEXT("recovery_ui_purchases"), RecoveryUIPurchases);
            Report->SetNumberField(TEXT("consumable_shop_ui_purchases"), ConsumableUIPurchases);
            Report->SetNumberField(TEXT("revival_ui_purchases"), RevivalUIPurchases);
            Report->SetStringField(TEXT("revival_ui_status"), RevivalUIPurchases > 0 ? TEXT("Observed through the offered original UI after natural death.") : TEXT("Unobserved: no eligible natural-death revival UI purchase was reached; no death or result was forced."));
            Report->SetStringField(TEXT("consumable_ui_status"), ConsumableUIUses > 0 ? TEXT("Actual UI plan/Ready, GAS healing, AP cost, quantity and saved boundary observed.") : TEXT("Unobserved: no successful natural consumable UI release was reached; no health or stock override was applied."));
            Report->SetStringField(TEXT("scope"), TEXT("Actual normal single-player menu creates four default characters with one direct and three production companions. Public encounter/shop/reward/node/plan/Ready requests only. Planning requires a positive participant slot and the original direct character identity; after its natural death, surviving companions remain production AI while the test only observes under the unchanged timeout. No HP, damage, AP, skill catalog, stock, gold, enemy roster, timing, physics or result overrides. Test-only strategy prioritizes legal critical healing or full-value self-healing before visible queued attack power crosses the danger boundary, then finishing a visible enemy, damage and support. Consumable plans use original world-selection/skill/Ready delegates and services use the actual recovery button; these are UI delegates, not physical mouse clicks. For equal legal healing utility, observe an owned consumable once. Observe actual self-heal GAS HP/AP/quantity and durable boundaries; natural death/revival may remain unobserved. Takes offered recovery and usable weapon upgrades through public purchase/equipment requests, reserves two gold for services, and selects offered item rewards by visible skill utility; legacy gold choices still choose the maximum. Actual first-victory menu restart when reached, separately followed or preceded by one actual menu restart at the next controllable planning boundary after successful consumable use. The latter compares the saved HP/AP/stock/progress/checkpoint and restored live units before new plans; it remains unobserved if no eligible boundary occurs. Stable boundaries also undergo checkpoint deserialization. A natural defeat ends observation honestly; one run does not establish overall balance, all strategies, online PvP or manual play quality."));
            Report->SetArrayField(TEXT("events"), Events);
            Report->SetObjectField(TEXT("first_planning_presentation"), PresentationReport);
            FString JSON;
            Check(FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&JSON)) && FFileHelper::SaveStringToFile(JSON, *(Output / TEXT("NormalTargetRun.json"))), TEXT("The complete normal-run outcome and unmodified boundary history are saved."));
            Test->AddInfo(FString::Printf(TEXT("Normal target Run outcome=%s; combat=%d/20 encounter=%d/60; result Continue=%d; post-consumable planning Continue=%d; file=%s."), *Outcome, CompletedCombats, CompletedChoices, bRestartVerified, bConsumableRestartVerified, *(Output / TEXT("NormalTargetRun.json"))));
        }

        FAutomationTestBase* Test;
        FString Slot;
        FString Output;
        FString Outcome;
        FString LastSignature;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TStrongObjectPtr<URunSaveGame> RestartSave;
        TStrongObjectPtr<URunSaveGame> ConsumableRestartSave;
        TWeakObjectPtr<AGameplayPlayerController> Controller;
        TSharedPtr<ISlateViewport> RetainedViewport;
        TArray<FRunPartyMember> OriginalParty;
        FRunIdentityData OriginalIdentity;
        TArray<TSharedPtr<FJsonValue>> Events;
        TSet<FString> VerifiedBoundaries;
        TSet<FString> AttemptedServices;
        TSet<FName> ObservedStages;
        int32 ItemPurchaseRequests = 0;
        int32 EquipmentRequests = 0;
        TSet<FString> PlannedRounds;
        TSet<FGuid> VerifiedCombatStats;
        TSet<FGuid> ObservedSpectatorCombats;
        FCombatRoundCommand PendingCommand;
        FString ServiceUIVisit;
        FString ConsumablePlanningWaitKey;
        FString ConsumablePlanningFirstHeader;
        TArray<FRunPartyMember> ExpectedServiceParty;
        FGameplayTag PendingServiceTag;
        TArray<TSharedPtr<FJsonValue>> ConsumableObservations;
        TWeakObjectPtr<AUnitBase> ConsumableSource;
        FCombatRoundSkill ObservedConsumableSkill;
        FGameplayEffectContextHandle ConsumableEffectContext;
        FGuid ConsumableCombatId;
        FGuid ConsumableRestartCombatId;
        FGuid ConsumableCharacterId;
        FDelegateHandle ConsumableGasHandle;
        FDelegateHandle ConsumableHealthHandle;
        int32 ExpectedServiceRevision = 0;
        int32 ServiceUIPurchases = 0;
        int32 RecoveryUIPurchases = 0;
        int32 RevivalUIPurchases = 0;
        int32 ConsumableUIPurchases = 0;
        int32 ConsumableUIRequests = 0;
        int32 ConsumableUIUses = 0;
        int32 ConsumableRound = 0;
        int32 ConsumableQuantityBefore = 0;
        int32 ConsumableAPBefore = 0;
        int32 ConsumableGasCount = 0;
        int32 ConsumableHealthChangeCount = 0;
        float ConsumableHPBefore = 0.f;
        float ConsumableHPAfter = 0.f;
        double ServiceUIReadyAt = 0.0;
        double ServiceObservedAt = 0.0;
        double ConsumableBoundaryAt = 0.0;
        double ConsumablePlanningWaitAt = 0.0;
        bool bPendingFullRecovery = false;
        bool bObserveConsumableResolving = false;
        bool bConsumableContext = true;
        TodoReviewGameplayPresentation::FReadiness FirstPlanningPresentation;
        TSharedRef<FJsonObject> PresentationReport = MakeShared<FJsonObject>();
        int32 Stage = 0;
        int32 CompletedCombats = 0;
        int32 CompletedChoices = 0;
        int32 RoundsSubmitted = 0;
        int32 SavedBoundaryReads = 0;
        double RunStarted = 0.0;
        double PhaseStarted = 0.0;
        double LastProgress = 0.0;
        double TerminalStarted = 0.0;
        double FirstPlanningStarted = 0.0;
        bool bPassed = true;
        bool bInitialPartyRecorded = false;
        bool bRestartRequested = false;
        bool bRestartVerified = false;
        bool bConsumableRestartRequested = false;
        bool bConsumableRestartVerified = false;
        bool bResumingConsumableBoundary = false;
        bool bTerminalObserved = false;
        bool bAwaitingPlan = false;
        bool bCapturedFirstPlanning = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNormalTargetRunPIEReview, "ProjectA.TodoReview.NormalTargetRun", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalTargetRunPIEReview::RunTest(const FString& Parameters)
{
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_NormalRun_");
    FGuid Guid;
    if (!TestTrue(TEXT("Normal target Run requires a rendering editor, no current PIE and no legacy prototype opt-in."), GEditor && GEngine && !GEditor->PlayWorld && FApp::CanEverRender() && FSlateApplication::IsInitialized() && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && !FParse::Param(FCommandLine::Get(), TEXT("ProjectAPrototypeRun")))) return false;
    if (!TestTrue(TEXT("A previously absent owned UUID slot protects all existing user saves."), Slot.StartsWith(Prefix) && Slot.Len() == Prefix.Len() + 32 && FGuid::ParseExact(Slot.Right(32), EGuidFormats::Digits, Guid) && Guid.IsValid() && !UGameplayStatics::DoesSaveGameExist(Slot, 0))) return false;
    FString OutputRoot;
    if (!TodoReviewWindowPlacement::OutputRoot(this, OutputRoot)) return false;
    const FString Output = OutputRoot / TEXT("NormalRun") / Slot;
    if (!TestTrue(TEXT("The normal-run report uses a new unique directory in Saved only."), !IFileManager::Get().DirectoryExists(*Output) && IFileManager::Get().MakeDirectory(*Output, true))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NormalTargetRunReview::FReview>(this, Slot, Output));
    return true;
}

#endif
