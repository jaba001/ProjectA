#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameplayEffect.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GAS/Effect/GE_Damage.h"
#include "GAS/Effect/GE_Heal.h"
#include "GAS/Effect/GE_Shield.h"
#include "Grid/Combat/CombatGridManager.h"
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
#include "TodoReviewWindowPlacement.h"
#include "TodoReviewGameplayPresentation.h"
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
                for (const FRunPartyMember& Member : OriginalParty)
                {
                    FProfessionDefinition Profession;
                    FText Error;
                    if (!Check(Run->ResolveMemberProfession(Member, Profession, Error) && FMath::IsNearlyEqual(Member.CurrentHP, Profession.MaxHP) && Member.Skills.Num() == 1 && Member.Consumables.Num() == 1 && Member.Consumables[0].Quantity == Run->GetRecoveryState().StartingQuantity, TEXT("Every freshly created character retains authored maximum health, its starting skill and normal consumable quantity: ") + Error.ToString())) return End();
                }
                bInitialPartyRecorded = true;
                Event(Run, TEXT("normal_party_created"));
            }
            if (Stage == 7)
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
            ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
            const FString Signature = FString::Printf(TEXT("%d:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d"), static_cast<int32>(Run->GetPhase()), *Run->GetCurrentNodeId().ToString(), *Run->GetEncounterProgress().SelectedEncounterId.ToString(), Run->GetCompletedNodes().Num(), Run->GetTargetRunState().CompletedEncounterChoices.Num(), Round ? static_cast<int32>(Round->GetView().Phase) : -1, Round ? Round->GetView().RoundNumber : 0, Round ? Round->GetView().PlanRevision : 0, Run->GetSkillShopState().Revision, Run->GetItemShopState().Revision, Run->GetRecoveryState().Revision, Run->GetGoldRewardState().Claims.Num());
            if (Signature != LastSignature)
            {
                LastSignature = Signature;
                LastProgress = Now;
            }
            const bool bStable = Run->GetPhase() != ERunPhase::Preparing && (Run->GetPhase() != ERunPhase::Combat || (Round && Round->GetView().Phase == ECombatRoundPhase::Planning && !Controller->IsRoundRequestPending()));
            if (bStable && !VerifySavedBoundary(Run, Signature)) return End();
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
            if (Saved.Phase != Run->GetPhase() || Saved.Result != Run->GetLastResult() || Saved.CurrentNode != Run->GetCurrentNodeId() || Saved.CurrentEncounter != Run->GetCurrentEncounterId() || Saved.CompletedNodes != Run->GetCompletedNodes() || Saved.Party.Num() != Run->GetPartyMembers().Num()) return false;
            if (!FRunIdentityData::StaticStruct()->CompareScriptStruct(&Saved.Identity, &Run->GetRunIdentity(), 0) || !FRunTargetState::StaticStruct()->CompareScriptStruct(&Saved.TargetRun, &Run->GetTargetRunState(), 0) || !FRunEncounterProgress::StaticStruct()->CompareScriptStruct(&Saved.EncounterProgress, &Run->GetEncounterProgress(), 0) || !FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Saved.GoldRewardState, &Run->GetGoldRewardState(), 0) || !FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Saved.SkillShopState, &Run->GetSkillShopState(), 0) || !FRunItemShopState::StaticStruct()->CompareScriptStruct(&Saved.ItemShopState, &Run->GetItemShopState(), 0)) return false;
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

        bool VisitShop(URunStateSubsystem* Run)
        {
            if (Controller->IsShopPurchasePending()) return false;
            const FRunPartyMember* Member = DirectMember(Run);
            const FRunEncounterOffer* Encounter = Run->GetEncounterProgress().FindSelectedOffer();
            if (!Check(Member && Encounter, TEXT("The selected normal encounter retains its original direct character and offer."))) return End();
            FProfessionDefinition Profession;
            FText Error;
            if (!Check(Run->ResolveMemberProfession(*Member, Profession, Error), Error.ToString())) return End();
            const FString VisitKey = FString::Printf(TEXT("%d:%s"), Run->GetTargetRunState().CompletedEncounterChoices.Num(), *Encounter->EncounterId.ToString());
            const FGameplayTag Tag = Encounter->GetResolvedTag();
            const bool bRecovery = Tag.MatchesTag(FRunEncounterOffer::GetRecoveryTag());
            const bool bRevival = Tag.MatchesTag(FRunEncounterOffer::GetRevivalTag());
            const bool bConsumable = Tag.MatchesTag(FRunEncounterOffer::GetConsumableShopTag());
            const FRunRecoveryState& Rules = Run->GetRecoveryState();
            const int32 ServicePrice = bRecovery ? Rules.RecoveryPrice : bRevival ? Rules.RevivalPrice : Rules.ConsumablePrice;
            if ((bRecovery || bRevival || bConsumable) && !AttemptedServices.Contains(VisitKey) && Member->Gold >= ServicePrice && (bRecovery ? Member->CurrentHP > 0.f && Member->CurrentHP < Profession.MaxHP : bRevival ? Member->CurrentHP == 0.f : Member->CurrentHP > 0.f))
            {
                AttemptedServices.Add(VisitKey);
                Event(Run, TEXT("request_service_purchase"), Tag.ToString());
                Controller->RequestPurchaseShopOffer(Member->CharacterId, Tag.GetTagName(), Run->GetRecoveryState().Revision);
                LastProgress = FPlatformTime::Seconds();
                return false;
            }
            if (Tag.MatchesTag(FRunEncounterOffer::GetSkillShopTag()) && Member->CurrentHP > 0.f)
            {
                const FRunSkillShopState& Shop = Run->GetSkillShopState();
                if (Member->CurrentHP < Profession.MaxHP && Member->Gold >= Shop.Recovery.Price && !AttemptedServices.Contains(VisitKey + TEXT(":full_recovery")))
                {
                    AttemptedServices.Add(VisitKey + TEXT(":full_recovery"));
                    Event(Run, TEXT("request_skill_shop_recovery"));
                    Controller->RequestPurchaseShopOffer(Member->CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Shop.Revision);
                    LastProgress = FPlatformTime::Seconds();
                    return false;
                }
                if (Member->Skills.Num() < 5)
                {
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
                if (!Check(!Reward.GoldChoices.IsEmpty(), TEXT("A pending human reward has actual authored choices."))) return End();
                int32 Choice = 0;
                for (int32 Index = 1; Index < Reward.GoldChoices.Num(); ++Index) if (Reward.GoldChoices[Index] > Reward.GoldChoices[Choice]) Choice = Index;
                Event(Run, TEXT("request_offered_gold_reward"), FString::FromInt(Reward.GoldChoices[Choice]));
                Controller->RequestSelectGoldReward(CharacterId, Run->GetCurrentNodeId(), Choice);
                LastProgress = FPlatformTime::Seconds();
                return false;
            }
            if (!Check(Run->CanContinueAfterRewards(), TEXT("The actual victory can continue after its required public reward claims."))) return End();
            Event(Run, TEXT("request_continue_run"));
            Controller->RequestContinueRun();
            return false;
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

        bool PlanRound(URunStateSubsystem* Run, ACombatRoundCoordinator* Round)
        {
            if (Controller->IsRoundRequestPending() || !Controller->IsRoundInputEnabled()) return false;
            const FCombatRoundView View = Round->GetView();
            if (!Check(View.RoundNumber <= 100, TEXT("One normal combat stays within the one-hundred-round observation budget; no forced result is applied."))) return End();
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
            const FCombatRoundUnitView* Direct = View.Units.FindByPredicate([this](const FCombatRoundUnitView& Entry) { return !Entry.bEnemy && Entry.OwnerSlot == Controller->GetRoundParticipantSlot() && Entry.Unit && Entry.Unit->IsUnitAlive(); });
            if (!Direct) return false;
            if (bAwaitingPlan)
            {
                if (!Check(FCombatRoundCommand::StaticStruct()->CompareScriptStruct(&Direct->Command, &PendingCommand, 0), TEXT("The normal player's public plan is accepted without changing any companion AI plan: ") + Controller->GetRoundRequestStatus().ToString())) return End();
                bAwaitingPlan = false;
                Controller->SetRoundReady(true);
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
                    const int32 Priority = bHeal && HP < MaxHP * 0.25f ? 3 : bDamage && HP <= Skill->Power ? 2 : bDamage ? 1 : 0;
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
                        if (Priority > BestPriority || (Priority == BestPriority && (EffectiveUtility > BestUtility || (FMath::IsNearlyEqual(EffectiveUtility, BestUtility) && Distance < BestDistance))))
                        {
                            Best = Candidate;
                            BestPriority = Priority;
                            BestUtility = EffectiveUtility;
                            BestDistance = Distance;
                        }
                    }
                }
            }
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
            Controller->SubmitRoundPlan(Best);
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
            Report->SetNumberField(TEXT("completed_combats"), CompletedCombats);
            Report->SetNumberField(TEXT("completed_encounter_choices"), CompletedChoices);
            Report->SetNumberField(TEXT("normal_rounds_submitted"), RoundsSubmitted);
            Report->SetNumberField(TEXT("saved_boundary_deserializations"), SavedBoundaryReads);
            Report->SetStringField(TEXT("scope"), TEXT("Actual normal single-player menu creates four default characters with one direct and three production companions. Public encounter/shop/reward/node/plan/Ready requests only. No HP, damage, AP, skill catalog, stock, gold, enemy roster, timing, physics or result overrides. Test-only strategy prioritizes legal critical healing, finishing a visible enemy, damage, then support; takes offered recovery and skills and maximum offered gold. Actual first-victory menu restart when reached, plus checkpoint deserialization at stable boundaries. A natural defeat ends observation honestly; one run does not establish overall balance, all strategies, online PvP or manual play quality."));
            Report->SetArrayField(TEXT("events"), Events);
            Report->SetObjectField(TEXT("first_planning_presentation"), PresentationReport);
            FString JSON;
            Check(FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&JSON)) && FFileHelper::SaveStringToFile(JSON, *(Output / TEXT("NormalTargetRun.json"))), TEXT("The complete normal-run outcome and unmodified boundary history are saved."));
            Test->AddInfo(FString::Printf(TEXT("Normal target Run outcome=%s; combat=%d/20 encounter=%d/60; actual Continue=%d; file=%s."), *Outcome, CompletedCombats, CompletedChoices, bRestartVerified, *(Output / TEXT("NormalTargetRun.json"))));
        }

        FAutomationTestBase* Test;
        FString Slot;
        FString Output;
        FString Outcome;
        FString LastSignature;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TStrongObjectPtr<URunSaveGame> RestartSave;
        TWeakObjectPtr<AGameplayPlayerController> Controller;
        TSharedPtr<ISlateViewport> RetainedViewport;
        TArray<FRunPartyMember> OriginalParty;
        FRunIdentityData OriginalIdentity;
        TArray<TSharedPtr<FJsonValue>> Events;
        TSet<FString> VerifiedBoundaries;
        TSet<FString> AttemptedServices;
        TSet<FString> PlannedRounds;
        TSet<FGuid> VerifiedCombatStats;
        FCombatRoundCommand PendingCommand;
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
