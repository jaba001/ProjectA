#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "GAS/CombatGameplayTags.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridTile.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Gameplay/CharacterInventoryPanel.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/InventoryWidget.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"

namespace SkillShopUiReview
{
    template <typename T>
    T* Active(UWorld* World)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (T* Result = Cast<T>(Widget); Result && Result->IsActivated()) return Result;
        return nullptr;
    }

    template <typename T>
    T* Child(UUserWidget* Parent)
    {
        T* Result = nullptr;
        if (Parent && Parent->WidgetTree) Parent->WidgetTree->ForEachWidget([&Result](UWidget* Widget) { if (!Result) Result = Cast<T>(Widget); });
        return Result;
    }

    FString Labels(UWidget* Widget)
    {
        if (!Widget) return FString();
        if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) return Text->GetText().ToString();
        if (UUserWidget* User = Cast<UUserWidget>(Widget)) return User->WidgetTree ? Labels(User->WidgetTree->RootWidget) : FString();
        FString Result;
        if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget)) for (UWidget* Item : Panel->GetAllChildren()) Result += Labels(Item) + TEXT("\n");
        return Result;
    }

    bool IsOwnedSlot(const FString& Slot)
    {
        const FString Prefix = TEXT("ProjectA_Automation_SkillShop_");
        if (!Slot.StartsWith(Prefix) || Slot.Len() != Prefix.Len() + 32) return false;
        for (TCHAR Character : Slot.Mid(Prefix.Len())) if (!FChar::IsHexDigit(Character)) return false;
        return true;
    }

    // Keep the complete authored catalog; only a valid disposable offer presentation is fixed for deterministic UI coverage.
    // 작성된 전체 카탈로그를 보존하고 유효한 일회성 진열만 고정하여 결정적인 UI 검수 범위를 만듭니다.
    class FReview : public IAutomationLatentCommand
    {
    public:
        FReview(FAutomationTestBase* InTest, FString InSlot) : Test(InTest), Slot(MoveTemp(InSlot)), Output(FPaths::ProjectSavedDir() / TEXT("Automation/TodoReview") / Slot)
        {
            Paths = {TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/___LinkChainVFX/DA_DrGame_LinkChainVFX_Link_Electric.DA_DrGame_LinkChainVFX_Link_Electric"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/_LevelUpSpawn/DA_DrGame_LevelUpSpawn_LevelUp_Ascend_Root.DA_DrGame_LevelUpSpawn_LevelUp_Ascend_Root"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/_LevelUpSpawn/DA_DrGame_LevelUpSpawn_Spawn_Ground_Root.DA_DrGame_LevelUpSpawn_Spawn_Ground_Root")};
            Report->SetStringField(TEXT("slot"), Slot);
            Report->SetStringField(TEXT("scope"), TEXT("Fresh four-character standalone Run; public isolated first-result/reward transitions with authored profession HP; original 61-candidate catalog/tags/prices preserved, valid five-offer disposable snapshot fixed for four authored card clicks. Original menu Continue, Leave and the authored Combat_02 map card lead to the next combat; four independent restores execute purchased melee/Chain/heal/shield via public coordinator requests. Only healing current HP is lowered transiently during observed combat. No production/content/balance change, random-selection coverage, audio listening or multiplayer claim. Card delegates and actual Slate I input are distinct from physical mouse purchase."));
        }

        virtual ~FReview() override
        {
            UnbindGas();
            ReleaseViewport();
        }

        virtual bool Update() override
        {
            if (Started == 0.0) Started = FPlatformTime::Seconds();
            if (Stage == 99)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.WorldType != EWorldType::PIE) continue;
                    if (FPlatformTime::Seconds() - Started < 30.0) return false;
                    Check(false, TEXT("Skill-shop review PIE closes within its bounded teardown."));
                    return Finish(false);
                }
                ReleaseViewport();
                if (bContinueAfterTeardown && bPassed)
                {
                    bContinueAfterTeardown = false;
                    if (!Check(FFileHelper::SaveArrayToFile(PurchasedBytes, *SavePath()), TEXT("Only the owned disposable slot is reset to the exact purchased shop bytes for its next independent Continue."))) return Finish();
                    Controller.Reset();
                    Source.Reset();
                    Target.Reset();
                    Advance(0);
                    return false;
                }
                return Finish();
            }
            if (FPlatformTime::Seconds() - Started > 120.0)
            {
                Check(false, FString::Printf(TEXT("Skill-shop review timed out at stage %d, purchase %d, cast %d."), Stage, PurchaseIndex, CastIndex));
                return End(false);
            }
            if (Stage == 0)
            {
                Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
                Settings->SetPlayNetMode(PIE_Standalone);
                Settings->SetPlayNumberOfClients(1);
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
                Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu");
                GEditor->RequestPlaySession(Params);
                Advance(bPurchased ? 20 : 1);
                return false;
            }
            UWorld* World = GEditor->PlayWorld;
            URunStateSubsystem* Run = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
            if (!World || !Run) return false;
            if (Stage == 1)
            {
                UMainMenuScreenWidget* Menu = Active<UMainMenuScreenWidget>(World);
                if (!Menu) return false;
                if (!Click(Menu, TEXT("Button_NewGame"))) return End(false);
                Advance(2);
                return false;
            }
            if (Stage == 2)
            {
                UGameModeSelectionWidget* Selection = Active<UGameModeSelectionWidget>(World);
                if (!Selection) return false;
                if (!Click(Selection, TEXT("Button_SinglePlayer"))) return End(false);
                Advance(3);
                return false;
            }
            if (Stage == 3)
            {
                UCharacterCreationWidget* Creation = Active<UCharacterCreationWidget>(World);
                if (!Creation) return false;
                for (int32 Index = 0; Index < 4; ++Index) if (!Click(Creation, FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index)))) return End(false);
                if (!Click(Creation, TEXT("Button_Slot0_PlayerControl")) || !Click(Creation, TEXT("Button_StartGame"))) return End(false);
                Advance(4);
                return false;
            }
            if (Stage == 20)
            {
                UMainMenuScreenWidget* Menu = Active<UMainMenuScreenWidget>(World);
                if (!Menu) return false;
                FText Error;
                if (!Check(Run->CanContinueStandaloneSavedRun(Error), TEXT("The purchased disposable shop enables production standalone Continue: ") + Error.ToString()) || !Click(Menu, TEXT("Button_Continue"))) return End(false);
                Advance(21);
                return false;
            }
            Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            if (!Controller.IsValid()) return false;
            if (Stage == 4)
            {
                if (Run->GetPhase() != ERunPhase::Map || !Active<URunMapWidget>(World)) return false;
                const FRunPartyMember* Buyer = Run->GetPartyMembers().FindByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled; });
                if (!Check(Buyer && Buyer->Skills.Num() == 1 && Buyer->Gold == 10 && Run->GetPartyMembers().Num() == 4 && Run->GetSkillShopState().Catalog.Num() == 61, TEXT("The authored fresh Run has four characters, one free unarmed skill, 10G and all 61 frozen candidates."))) return End(false);
                CharacterId = Buyer->CharacterId;
                OriginalShop = Run->GetSkillShopState();
                Identity = Run->GetRunIdentity();
                FText Error;
                if (!Check(Run->BeginEncounter(TEXT("Combat_01")) && Run->MarkCombatStarted(), TEXT("Public state transitions prepare only the isolated first combat; no first-combat gameplay success is claimed."))) return End(false);
                const TArray<FRunPartyMember> FirstCombatParty = Run->GetPartyMembers();
                for (const FRunPartyMember& Member : FirstCombatParty)
                {
                    FProfessionDefinition Profession;
                    if (!Check(Run->PartyDefinition && Run->PartyDefinition->ResolveProfession(Member.ClassId, Profession), TEXT("The result fixture uses each actual authored profession's original maximum HP."))) return End(false);
                    Run->UpdatePartyMemberHP(Member.SlotIndex, Profession.MaxHP);
                }
                if (!Check(Run->CompleteEncounter(ECombatResult::Victory), TEXT("The public isolated victory publishes its original reward/shop state without exercising first-combat gameplay."))) return End(false);
                for (const FGuid& Recipient : Run->GetGoldRewardRecipientIds())
                {
                    const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([Recipient](const FRunPartyMember& Value) { return Value.CharacterId == Recipient; });
                    if (!Check(Member && Run->SelectGoldReward(Member->OwnerAccountId, Recipient, Run->GetCurrentNodeId(), 0, Error), TEXT("The disposable first result collects its authored gold reward."))) return End(false);
                }
                Controller->RequestContinueRun();
                if (!Check(Run->GetPhase() == ERunPhase::EncounterChoice, TEXT("The original Continue request exposes the authored shop choices."))) return End(false);
                Advance(5);
                return false;
            }
            if (Stage == 5)
            {
                URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
                if (!Screen) return false;
                UVerticalBox* Choices = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("EncounterActions")));
                UButton* Shop = Choices && Choices->GetChildrenCount() > 0 ? Cast<UButton>(Choices->GetChildAt(0)) : nullptr;
                if (!Check(Shop && Shop->GetIsEnabled(), TEXT("The actual encounter screen exposes its original first skill-shop choice."))) return End(false);
                Shop->OnClicked.Broadcast();
                if (!Check(Run->GetPhase() == ERunPhase::Shop && Run->GetEncounterProgress().SelectedEncounterId == FName(TEXT("Shop_01")), TEXT("The original shop card opens Shop_01."))) return End(false);
                if (!FixDisposableOffers(Run)) return End(false);
                Advance(6);
                return false;
            }
            if (Stage == 6)
            {
                URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
                if (!Screen || !Warm()) return false;
                if (!Purchase(Screen, Run, PurchaseIndex)) return End(false);
                if (++PurchaseIndex < Paths.Num())
                {
                    Advance(6);
                    return false;
                }
                if (!VerifyLearned(Run) || !Check(FFileHelper::LoadFileToArray(PurchasedBytes, *SavePath()), TEXT("The complete purchased shop snapshot is read from the owned slot."))) return End(false);
                bPurchased = true;
                Advance(9);
                return false;
            }
            if (Stage == 9)
            {
                if (!Active<URunEncounterWidget>(World) || !Warm()) return false;
                if (!Capture(World, TEXT("ShopPurchased"))) return End(false);
                SendI(World);
                Advance(7);
                return false;
            }
            if (Stage == 7)
            {
                UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
                if (!Inventory || !Warm()) return false;
                if (!InventorySkills(Inventory, Run)) return End(false);
                Advance(8);
                return false;
            }
            if (Stage == 8)
            {
                if (!Active<UInventoryWidget>(World) || !Warm()) return false;
                if (!Capture(World, TEXT("LearnedSkillTab"))) return End(false);
                SendI(World);
                return End(true);
            }
            if (Stage == 21)
            {
                URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
                if (!Screen || Run->GetPhase() != ERunPhase::Shop || !Warm()) return false;
                if (!VerifyLearned(Run) || !Check(FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Run->GetSkillShopState(), &PurchasedShop, 0), TEXT("Actual Continue restores the exact purchased skill-shop stock and revision.")) || !Click(Screen, TEXT("Button_LeaveShop"))) return End(false);
                Advance(24);
                return false;
            }
            if (Stage == 24)
            {
                URunMapWidget* Map = Active<URunMapWidget>(World);
                if (!Map || Run->GetPhase() != ERunPhase::Map || !Warm()) return false;
                const FName NodeId(TEXT("Combat_02"));
                const TArray<FRunNodeDefinition>& Nodes = Run->GetNodes();
                const int32 NodeIndex = Nodes.IndexOfByPredicate([NodeId](const FRunNodeDefinition& Node) { return Node.NodeId == NodeId; });
                UVerticalBox* Cards = Cast<UVerticalBox>(Map->GetWidgetFromName(TEXT("NodeList")));
                UGameplayActionButton* Button = Cards && NodeIndex != INDEX_NONE && Cards->GetChildrenCount() == Nodes.Num() ? Cast<UGameplayActionButton>(Cards->GetChildAt(NodeIndex)) : nullptr;
                if (!Check(Button && Button->GetIsEnabled() && Run->CanStartNode(NodeId) && Labels(Button).Contains(Nodes[NodeIndex].DisplayName.ToString()), TEXT("Leaving the authored shop exposes the eligible original Combat_02 map card with its correct label."))) return End(false);
                int32 Dispatches = 0;
                FName Observed;
                const FDelegateHandle Handle = Button->OnActionRequested.AddLambda([&](FName Id) { ++Dispatches; Observed = Id; });
                Button->OnClicked.Broadcast();
                Button->OnActionRequested.Remove(Handle);
                if (!Check(Dispatches == 1 && Observed == NodeId && Run->GetCurrentNodeId() == NodeId, TEXT("The actual Combat_02 map card dispatches its original node identifier exactly once."))) return End(false);
                ++StartedNodeCards;
                Advance(22);
                return false;
            }
            if (Stage == 22)
            {
                ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
                if (!Round || Round->GetView().Phase != ECombatRoundPhase::Planning || FAssetCompilingManager::Get().GetNumRemainingAssets() != 0 || !Warm()) return false;
                if (!Check(Run->GetPhase() == ERunPhase::Combat && Run->GetCurrentNodeId() == FName(TEXT("Combat_02")), TEXT("The authored Leave and map-card flow reaches the next actual Combat_02.")) || !PrepareCast(Run, Round)) return End(false);
                Advance(23);
                return false;
            }
            if (Stage == 23)
            {
                if (!ObserveCast(World)) return End(false);
                ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
                if (!Round || (Round->GetView().RoundNumber == InitialRound && Round->GetView().Phase != ECombatRoundPhase::Finished)) return false;
                if (!Check(bSawAP && Applied == 1 && bOriginalCaster && bObservedAttribute && bObservedVisual, TEXT("The purchased skill retains its real GAS effect, original caster, one AP payment and authored visual in the next combat.")) || !Capture(World, FString::Printf(TEXT("Cast%dComplete"), CastIndex))) return End(false);
                TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
                Record->SetStringField(TEXT("asset"), Paths[CastIndex]);
                Record->SetStringField(TEXT("skill_name"), Definition.Name.ToString());
                Record->SetStringField(TEXT("source_class"), Source->GetClass()->GetPathName());
                Record->SetStringField(TEXT("selected_target_class"), Target->GetClass()->GetPathName());
                Record->SetNumberField(TEXT("source_capsule_radius_cm"), Source->GetCapsuleComponent()->GetScaledCapsuleRadius());
                Record->SetNumberField(TEXT("target_capsule_radius_cm"), Target->GetCapsuleComponent()->GetScaledCapsuleRadius());
                Record->SetNumberField(TEXT("source_original_spawn_hp"), OriginalSpawnHP);
                Record->SetNumberField(TEXT("source_ap_before_cast"), InitialAP);
                Record->SetNumberField(TEXT("target_hp_before_cast"), InitialHP);
                Record->SetNumberField(TEXT("target_shield_before_cast"), InitialShield);
                Record->SetBoolField(TEXT("transient_healing_current_hp_lowered"), Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal));
                Record->SetNumberField(TEXT("gas_effects"), Applied);
                Record->SetNumberField(TEXT("paid_ap"), Definition.ActionPointCost);
                Record->SetBoolField(TEXT("original_caster"), bOriginalCaster);
                Record->SetBoolField(TEXT("attribute_observed"), bObservedAttribute);
                Record->SetBoolField(TEXT("authored_visual_observed"), bObservedVisual);
                Record->SetBoolField(TEXT("niagara_observation_required"), !Definition.Vfx.Niagara.IsNull());
                Record->SetBoolField(TEXT("actual_cast_montage_observed"), bObservedMontage);
                Records.Add(MakeShared<FJsonValueObject>(Record));
                UnbindGas();
                ++CastIndex;
                return End(CastIndex < Paths.Num());
            }
            return false;
        }

    private:
        bool Check(bool Condition, const FString& Message)
        {
            const bool Result = Test->TestTrue(Message, Condition);
            bPassed &= Result;
            return Result;
        }

        void Advance(int32 Next)
        {
            Stage = Next;
            Started = FPlatformTime::Seconds();
            WarmFrames = 0;
        }

        bool Warm()
        {
            return ++WarmFrames >= 12 && FPlatformTime::Seconds() - Started >= 0.25;
        }

        FString SavePath() const
        {
            return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"));
        }

        const FRunPartyMember* Buyer(const URunStateSubsystem* Run) const
        {
            return Run->GetPartyMembers().FindByPredicate([this](const FRunPartyMember& Member) { return Member.CharacterId == CharacterId; });
        }

        bool Click(UUserWidget* Screen, FName Name)
        {
            UButton* Button = Screen ? Cast<UButton>(Screen->GetWidgetFromName(Name)) : nullptr;
            if (!Check(Button && Button->GetIsEnabled(), TEXT("The actual authored button is enabled: ") + Name.ToString())) return false;
            Button->OnClicked.Broadcast();
            return true;
        }

        void SendI(UWorld* World)
        {
            const TSharedPtr<SViewport> Widget = World->GetGameViewport()->GetGameViewportWidget();
            FSlateApplication& Slate = FSlateApplication::Get();
            Slate.SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
            Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::I, FModifierKeysState(), 0, false, 0, 0));
            Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::I, FModifierKeysState(), 0, false, 0, 0));
        }

        bool FixDisposableOffers(URunStateSubsystem* Run)
        {
            FText Error;
            TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
            if (!Check(Saved.IsValid() && Saved->Phase == ERunPhase::Shop && Saved->SkillShopState.Catalog.Num() == 61, TEXT("The guarded actual shop snapshot retains all authored candidates before presentation setup."))) return false;
            const TArray<FRunSkillShopOffer> Catalog = Saved->SkillShopState.Catalog;
            Saved->SkillShopState.Offers.Reset();
            for (const FString& Path : Paths)
            {
                const FRunSkillShopOffer* Offer = Catalog.FindByPredicate([&Path](const FRunSkillShopOffer& Value) { return Value.Skill == FSoftObjectPath(Path); });
                USkillDefinitionDataAsset* Asset = Offer ? Cast<USkillDefinitionDataAsset>(Offer->Skill.TryLoad()) : nullptr;
                FCombatRoundSkill Skill;
                if (!Check(Offer && Asset && Offer->Price == 1 && Asset->ResolveRoundSkill(Skill, Error) && Skill.Power > 0.f && Skill.ActionPointCost == 1, TEXT("Each fixed offer is an unchanged original authored 1G candidate with its real runtime profile: ") + Path)) return false;
                Definitions.Add(TStrongObjectPtr<USkillDefinitionDataAsset>(Asset));
                Profiles.Add(Skill);
                Saved->SkillShopState.Offers.Add(*Offer);
            }
            // Legacy melee keeps its ability classification while its authored GameplayEffect supplies the damage tag.
            // 기존 근접 공격은 어빌리티 분류를 유지하며 작성된 GameplayEffect가 피해 태그를 제공합니다.
            const UGameplayEffect* MeleeEffect = Profiles[0].EffectClass ? Profiles[0].EffectClass.GetDefaultObject() : nullptr;
            if (!Check(Profiles[0].Kind == ECombatRoundSkillKind::Melee && Profiles[0].TargetRule == ESkillTargetRule::EnemyUnit && MeleeEffect && MeleeEffect->GetAssetTags().HasTag(ProjectACombatTags::Skill_Effect_Damage), TEXT("The original legacy melee resolves its real attack kind and authored damage-effect tag without modifying ability tags."))) return false;
            if (!Check(CombatRoundRules::UsesChain(Profiles[1]) && Profiles[1].Chain.MaxTargets == 4 && FMath::IsNearlyEqual(Profiles[1].Chain.JumpDistance, 600.f) && FMath::IsNearlyEqual(Profiles[1].Chain.JumpIntervalSeconds, 0.15f) && FMath::IsNearlyEqual(Profiles[1].Chain.DamageMultiplierPerJump, 0.8f) && Profiles[2].EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal) && Profiles[3].EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield), TEXT("The authored DrGame offers retain the selected four-target Chain settings, healing and shield tags."))) return false;
            for (int32 Index = 1; Index < Profiles.Num(); ++Index) if (!Check(FMath::IsNearlyEqual(Profiles[Index].Power, 25.f), TEXT("Purchased DrGame profiles retain their authored test power of 25 without fixture overrides."))) return false;
            for (const FRunSkillShopOffer& Offer : Catalog)
            {
                if (!Saved->SkillShopState.Offers.ContainsByPredicate([&Offer](const FRunSkillShopOffer& Value) { return Value.Skill == Offer.Skill; }))
                {
                    Saved->SkillShopState.Offers.Add(Offer);
                    break;
                }
            }
            if (!Check(Saved->SkillShopState.Offers.Num() == 5 && URunEncounterPoolDataAsset::ValidateSkillShop(Saved->SkillShopState, Error), TEXT("The disposable five-offer presentation obeys the original full-catalog/tag selection validation: ") + Error.ToString())) return false;
            FRunSkillShopState CatalogRestored = Saved->SkillShopState;
            CatalogRestored.Offers = OriginalShop.Offers;
            CatalogRestored.Revision = OriginalShop.Revision;
            if (!Check(FRunSkillShopState::StaticStruct()->CompareScriptStruct(&CatalogRestored, &OriginalShop, 0), TEXT("Only disposable offer presentation/revision differ; frozen catalog, tags/query, weights, recovery and prices remain exact."))) return false;
            if (!Check(UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0) && Run->LoadCheckpoint(Error), TEXT("Only the guarded fresh slot installs the valid fixed offer presentation: ") + Error.ToString())) return false;
            PurchaseParty = Run->GetPartyMembers();
            return Check(Buyer(Run) && Buyer(Run)->Skills.Num() == 1 && Buyer(Run)->Gold >= 4, TEXT("The original unarmed buyer can afford the four authored skills without fixture gold grants."));
        }

        bool Purchase(URunEncounterWidget* Screen, URunStateSubsystem* Run, int32 Index)
        {
            const FRunSkillShopState BeforeShop = Run->GetSkillShopState();
            const TArray<FRunPartyMember> BeforeParty = Run->GetPartyMembers();
            UVerticalBox* Actions = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("ShopActions")));
            if (!Check(Actions && Actions->GetChildrenCount() == 5 && BeforeShop.Offers.IsValidIndex(Index), TEXT("The authored skill merchant has exactly five cards in frozen offer order."))) return false;
            const FRunSkillShopOffer Offer = BeforeShop.Offers[Index];
            UWidget* Card = Actions->GetChildAt(Index);
            UGameplayActionButton* Button = nullptr;
            if (UPanelWidget* Panel = Cast<UPanelWidget>(Card))
            {
                TArray<UWidget*> Pending{Panel};
                while (!Pending.IsEmpty())
                {
                    UWidget* Next = Pending.Pop(EAllowShrinking::No);
                    if (UGameplayActionButton* Typed = Cast<UGameplayActionButton>(Next))
                    {
                        Button = Typed;
                        break;
                    }
                    if (UPanelWidget* Children = Cast<UPanelWidget>(Next)) Pending.Append(Children->GetAllChildren());
                }
            }
            if (!Check(Button && Button->GetIsEnabled() && Labels(Card).Contains(Definitions[Index]->SkillName.ToString()) && Labels(Card).Contains(TEXT("1G")), TEXT("The original skill card displays its current authored name, 1G price and enabled purchase."))) return false;
            int32 Dispatches = 0;
            FName Observed;
            const FDelegateHandle Handle = Button->OnActionRequested.AddLambda([&](FName Id) { ++Dispatches; Observed = Id; });
            Button->OnClicked.Broadcast();
            Button->OnActionRequested.Remove(Handle);
            const FRunPartyMember* Member = Buyer(Run);
            const FRunPartyMember* Before = BeforeParty.FindByPredicate([this](const FRunPartyMember& Value) { return Value.CharacterId == CharacterId; });
            if (!Check(Dispatches == 1 && Observed == Offer.OfferId && Member && Before && Member->Skills.Num() == Before->Skills.Num() + 1 && Member->Skills.Last() == Offer.Skill && Member->Gold == Before->Gold - Offer.Price && Run->GetSkillShopState().Revision == BeforeShop.Revision + 1, TEXT("The actual skill card dispatches its exact OfferId once and durably learns one skill for its original price/revision."))) return false;
            for (int32 PartyIndex = 0; PartyIndex < BeforeParty.Num(); ++PartyIndex) if (BeforeParty[PartyIndex].CharacterId != CharacterId && !Check(FRunPartyMember::StaticStruct()->CompareScriptStruct(&BeforeParty[PartyIndex], &Run->GetPartyMembers()[PartyIndex], 0), TEXT("Purchasing changes no AI companion or original owner fields."))) return false;
            TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
            if (!Check(Durable.IsValid() && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Durable->SkillShopState, &Run->GetSkillShopState(), 0) && Durable->Party.Num() == Run->GetPartyMembers().Num(), TEXT("The authored card purchase is present in actual durable shop storage."))) return false;
            for (int32 PartyIndex = 0; PartyIndex < Durable->Party.Num(); ++PartyIndex) if (!Check(FRunPartyMember::StaticStruct()->CompareScriptStruct(&Durable->Party[PartyIndex], &Run->GetPartyMembers()[PartyIndex], 0), TEXT("Durable purchase retains the entire current party snapshot."))) return false;
            PurchasedShop = Run->GetSkillShopState();
            return true;
        }

        bool VerifyLearned(URunStateSubsystem* Run)
        {
            const FRunPartyMember* Member = Buyer(Run);
            const FRunPartyMember* Before = PurchaseParty.FindByPredicate([this](const FRunPartyMember& Value) { return Value.CharacterId == CharacterId; });
            if (!Check(Member && Before && Member->Skills.Num() == 5 && Member->Skills[0] == Before->Skills[0] && Member->Gold == Before->Gold - 4 && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Run->GetRunIdentity(), 0), TEXT("All purchases retain unarmed plus the four authored skills, charge exactly 4G and preserve original identity."))) return false;
            for (int32 Index = 0; Index < Paths.Num(); ++Index) if (!Check(Member->Skills[Index + 1] == FSoftObjectPath(Paths[Index]), TEXT("The learned order remains the actual four purchased DataAssets."))) return false;
            return true;
        }

        bool InventorySkills(UInventoryWidget* Inventory, URunStateSubsystem* Run)
        {
            UCharacterInventoryPanel* Panel = Child<UCharacterInventoryPanel>(Inventory);
            UHorizontalBox* Tabs = Panel ? Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("InventoryCategoryTabs"))) : nullptr;
            UVerticalBox* Skills = Panel ? Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("InventorySkills"))) : nullptr;
            UButton* Tab = Tabs && Tabs->GetChildrenCount() == 6 ? Cast<UButton>(Tabs->GetChildAt(5)) : nullptr;
            if (!Check(Tab && Skills && Tab->GetIsEnabled(), TEXT("The actual I inventory exposes its existing skill tab."))) return false;
            Tab->OnClicked.Broadcast();
            if (!Check(Skills->GetChildrenCount() == Buyer(Run)->Skills.Num(), TEXT("The actual learned skill tab has all five durable entries."))) return false;
            for (const TStrongObjectPtr<USkillDefinitionDataAsset>& Asset : Definitions) if (!Check(Labels(Skills).Contains(Asset->SkillName.ToString()), TEXT("The real skill tab displays the current purchased label: ") + Asset->SkillName.ToString())) return false;
            return true;
        }

        bool PrepareCast(URunStateSubsystem* Run, ACombatRoundCoordinator* Round)
        {
            if (!VerifyLearned(Run)) return false;
            const FCombatRoundUnitView* Caster = nullptr;
            const FCombatRoundUnitView* Victim = nullptr;
            ACombatManager* Manager = nullptr;
            for (TActorIterator<ACombatManager> It(Controller->GetWorld()); It; ++It)
            {
                if (It->GetRoundCoordinator() != Round) continue;
                Manager = *It;
                break;
            }
            if (!Check(Manager != nullptr, TEXT("The next real combat has its original manager."))) return false;
            Definition = Profiles[CastIndex];
            for (const FCombatRoundUnitView& Entry : Round->GetView().Units)
            {
                if (IsValid(Entry.Unit.Get()) && Manager->GetCharacterId(Entry.Unit.Get()) == CharacterId) Caster = &Entry;
                if (Entry.bEnemy && IsValid(Entry.Unit.Get()) && Entry.Unit->IsUnitAlive() && Entry.Unit->GetCapsuleComponent()->GetScaledCapsuleRadius() >= 35.f && !Victim) Victim = &Entry;
            }
            if (!Check(Caster && Caster->SkillIds.Num() == 5 && Caster->SkillIds.Contains(Definition.SkillId) && Victim && Definition.ActionPointCost == 1, TEXT("Actual next-combat spawn resolves the purchased loadout and a normal original target capsule."))) return false;
            Source = Caster->Unit;
            OriginalSpawnHP = Source->GetAttributeSet()->GetHP();
            const bool bSupport = Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal) || Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield);
            Target = bSupport ? Source : Victim->Unit;
            if (Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal)) Source->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Source->GetAttributeSet()->GetMaxHP() * 0.5f);
            InitialHP = Target->GetAttributeSet()->GetHP();
            InitialShield = Target->GetAttributeSet()->GetShield();
            InitialAP = Source->GetCurrentActionPoint();
            InitialRound = Round->GetView().RoundNumber;
            Applied = 0;
            bOriginalCaster = true;
            bObservedAttribute = bSawAP = bVisualCaptured = bObservedVisual = bObservedMontage = false;
            if (!Definition.Vfx.Niagara.IsNull()) Definition.Vfx.Niagara.LoadSynchronous();
            GasHandle = Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.AddLambda([this](UAbilitySystemComponent* Recipient, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
            {
                FGameplayTagContainer Tags;
                Spec.GetAllAssetTags(Tags);
                if (!Tags.HasAll(Definition.EffectTags) || !Target.IsValid() || Recipient != Target->GetAbilitySystemComponent()) return;
                ++Applied;
                bOriginalCaster &= Spec.GetContext().GetOriginalInstigator() == Source.Get();
                const UAS_Unit* Values = Target->GetAttributeSet();
                bObservedAttribute |= Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal) ? Values->GetHP() > InitialHP : Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield) ? Values->GetShield() > InitialShield : Values->GetHP() < InitialHP;
            });
            FCombatRoundCommand Command;
            Command.UnitId = Caster->UnitId;
            Command.SkillId = Definition.SkillId;
            Command.TargetUnitId = CombatRoundRules::UsesUnitTarget(Definition) ? (bSupport ? Caster->UnitId : Victim->UnitId) : INDEX_NONE;
            Command.TargetCoord = bSupport ? Caster->HomeCoord : Victim->HomeCoord;
            Command.DestinationCoord = Caster->HomeCoord;
            const FCombatRoundView View = Round->GetView();
            FText Error;
            bool bEquippedProfileMatches = false;
            for (const USkillDefinitionDataAsset* Equipped : Source->GetEquippedSkillDataAssets())
            {
                FCombatRoundSkill LiveProfile;
                if (!IsValid(Equipped) || !Equipped->ResolveRoundSkill(LiveProfile, Error) || LiveProfile.SkillId != Definition.SkillId) continue;
                bEquippedProfileMatches = FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&LiveProfile, &Profiles[CastIndex], 0);
                break;
            }
            if (!Check(bEquippedProfileMatches, TEXT("The actual spawned unit's purchased DataAsset resolves every original field, including its chain settings."))) return false;
            const bool bSubmitted = Round->SubmitPlan(Controller.Get(), View.CombatId, View.RoundNumber, View.PlanRevision, Command, Error);
            if (!Check(bSubmitted, TEXT("The public owned request submits the actually purchased profile: ") + Error.ToString())) return false;
            const FCombatRoundView PlannedView = Round->GetView();
            const bool bReady = Round->SetParticipantReady(Controller.Get(), PlannedView.CombatId, PlannedView.RoundNumber, PlannedView.PlanRevision, true, Error);
            if (!Check(bReady, TEXT("The public Ready request uses the accepted plan's current revision: ") + Error.ToString())) return false;
            bSawAP = Source->GetCurrentActionPoint() == InitialAP - Definition.ActionPointCost;
            return Check(bSawAP, TEXT("The purchased skill pays exactly one original AP cost when the round locks."));
        }

        bool ObserveCast(UWorld* World)
        {
            if (!Check(Source.IsValid() && Target.IsValid(), TEXT("The original caster and intended target remain observable during the real cast."))) return false;
            bSawAP |= Source->GetCurrentActionPoint() == InitialAP - Definition.ActionPointCost;
            bObservedMontage |= Source->HasRoundCastMontageInstance();
            if (Definition.Vfx.Niagara.IsNull() && bObservedMontage)
            {
                bObservedVisual = true;
                if (!bVisualCaptured && !Capture(World, FString::Printf(TEXT("Cast%dActive"), CastIndex))) return false;
                bVisualCaptured = true;
            }
            for (TObjectIterator<UNiagaraComponent> It; It; ++It)
            {
                if (It->GetWorld() != World || It->GetAsset() != Definition.Vfx.Niagara.Get() || !It->IsActive()) continue;
                const FNiagaraSystemInstanceControllerPtr Instance = It->GetSystemInstanceController();
                if (!Instance || !Instance->IsValid() || Instance->GetAge() < 0.15f) continue;
                bObservedVisual = true;
                if (!bVisualCaptured && !Capture(World, FString::Printf(TEXT("Cast%dActive"), CastIndex))) return false;
                bVisualCaptured = true;
            }
            return true;
        }

        bool Capture(UWorld* World, const FString& Name)
        {
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const FIntPoint Size = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
            TArray<FColor> Pixels;
            FIntVector Dimensions;
            if (!Check(Widget.IsValid() && Size.X > 0 && Size.Y > 0 && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Dimensions) && Dimensions.X == Size.X && Dimensions.Y == Size.Y && Pixels.Num() == Size.X * Size.Y, TEXT("A fresh full Slate frame contains the actual skill-shop or purchased combat."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
            const FString Path = Output / (Name + TEXT(".png"));
            if (!Check(!PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *Path), TEXT("The fresh review PNG is written under Saved only."))) return false;
            Captures.Add(MakeShared<FJsonValueString>(FPaths::ConvertRelativePathToFull(Path)));
            return true;
        }

        void UnbindGas()
        {
            if (Source.IsValid() && Source->GetAbilitySystemComponent() && GasHandle.IsValid()) Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.Remove(GasHandle);
            GasHandle.Reset();
        }

        bool End(bool Continue)
        {
            UnbindGas();
            UGameViewportClient* Viewport = GEditor->PlayWorld ? GEditor->PlayWorld->GetGameViewport() : nullptr;
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
            bContinueAfterTeardown = Continue;
            GEditor->RequestEndPlayMap();
            Advance(99);
            return false;
        }

        void ReleaseViewport()
        {
            check(IsInGameThread());
            if (!RetainedViewport.IsValid()) return;
            FlushRenderingCommands();
            RetainedViewport.Reset();
        }

        bool Finish(bool SafeToDelete = true)
        {
            for (int32 Index = 0; Index < Definitions.Num(); ++Index)
            {
                FCombatRoundSkill Current;
                FText Error;
                Check(Definitions[Index]->ResolveRoundSkill(Current, Error) && FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Current, &Profiles[Index], 0), TEXT("Every original purchased DataAsset remains unchanged after UI and combat review."));
            }
            if (SafeToDelete && IsOwnedSlot(Slot) && UGameplayStatics::DoesSaveGameExist(Slot, 0)) Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Only the explicitly owned skill-shop review slot is removed after PIE teardown."));
            Report->SetBoolField(TEXT("passed"), bPassed && Records.Num() == 4 && StartedNodeCards == 4);
            Report->SetBoolField(TEXT("owned_slot_cleaned"), !UGameplayStatics::DoesSaveGameExist(Slot, 0));
            Report->SetArrayField(TEXT("casts"), Records);
            Report->SetArrayField(TEXT("captures"), Captures);
            Report->SetNumberField(TEXT("authored_candidate_count"), OriginalShop.Catalog.Num());
            Report->SetNumberField(TEXT("authored_offer_count"), PurchasedShop.Offers.Num());
            Report->SetNumberField(TEXT("purchases"), PurchaseIndex);
            Report->SetNumberField(TEXT("next_combat_node_card_dispatches"), StartedNodeCards);
            FString Json;
            Check(FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Output / TEXT("summary.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM), TEXT("The skill-shop review result is stored under Saved only."));
            return true;
        }

        FAutomationTestBase* Test;
        FString Slot;
        FString Output;
        TArray<FString> Paths;
        TArray<TStrongObjectPtr<USkillDefinitionDataAsset>> Definitions;
        TArray<FCombatRoundSkill> Profiles;
        TArray<uint8> PurchasedBytes;
        TArray<FRunPartyMember> PurchaseParty;
        FRunIdentityData Identity;
        FRunSkillShopState OriginalShop;
        FRunSkillShopState PurchasedShop;
        FGuid CharacterId;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TWeakObjectPtr<AGameplayPlayerController> Controller;
        TWeakObjectPtr<AUnitBase> Source;
        TWeakObjectPtr<AUnitBase> Target;
        TSharedPtr<ISlateViewport> RetainedViewport;
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Records;
        TArray<TSharedPtr<FJsonValue>> Captures;
        FCombatRoundSkill Definition;
        FDelegateHandle GasHandle;
        double Started = 0.0;
        int32 Stage = 0;
        int32 WarmFrames = 0;
        int32 PurchaseIndex = 0;
        int32 CastIndex = 0;
        int32 StartedNodeCards = 0;
        int32 InitialRound = 0;
        int32 InitialAP = 0;
        int32 Applied = 0;
        float InitialHP = 0.f;
        float OriginalSpawnHP = 0.f;
        float InitialShield = 0.f;
        bool bPassed = true;
        bool bPurchased = false;
        bool bContinueAfterTeardown = false;
        bool bOriginalCaster = true;
        bool bObservedAttribute = false;
        bool bSawAP = false;
        bool bObservedVisual = false;
        bool bObservedMontage = false;
        bool bVisualCaptured = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillShopUiReviewTest, "ProjectA.TodoReview.SkillShopUI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillShopUiReviewTest::RunTest(const FString& Parameters)
{
    FString Slot;
    FString RuntimeSlot;
    if (!GEditor || !GEngine || !FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("SkillShopUI requires a rendering Editor and an isolated standalone review."));
        return false;
    }
    if (!FParse::Value(FCommandLine::Get(), TEXT("ProjectASkillShopReviewSlot="), Slot) || !FParse::Value(FCommandLine::Get(), TEXT("ProjectASaveSlot="), RuntimeSlot) || !SkillShopUiReview::IsOwnedSlot(Slot) || Slot != RuntimeSlot || UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        AddError(TEXT("Supply one fresh identical ProjectA_Automation_SkillShop_<32hex> value for -ProjectASkillShopReviewSlot and -ProjectASaveSlot; existing slots are refused."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType != EWorldType::PIE) continue;
        AddError(TEXT("Close the existing PIE before isolated skill-shop review."));
        return false;
    }
    const FString Output = FPaths::ProjectSavedDir() / TEXT("Automation/TodoReview") / Slot;
    if (!IFileManager::Get().MakeDirectory(*Output, true))
    {
        AddError(TEXT("The review output directory cannot be created under Saved."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<SkillShopUiReview::FReview>(this, Slot));
    return true;
}

#endif
