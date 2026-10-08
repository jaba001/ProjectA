#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Controller/GameplayPlayerController.h"
#include "Game/GameState/GameplayGameState.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/MainMenu/RunSurrenderWidget.h"
#include "Components/VerticalBox.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ComboBoxString.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTodoPackagedCatalogTest, "ProjectA.TodoReview.PackagedCsv", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTodoPackagedCatalogTest::RunTest(const FString& Parameters)
{
    // Read the production path through Unreal's platform file so the same check exercises the staged UFS catalog in a cooked game.
    // Unreal 플랫폼 파일로 실제 경로를 읽어 같은 검사에서 쿠킹된 게임의 UFS 카탈로그도 확인합니다.
    const FString CatalogPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/WEAPON_ASSETS.csv"));
    TArray<FRunItemDefinition> Catalog;
    FText Error;
    if (!TestTrue(TEXT("The runtime platform file exposes the authored catalog at its project-relative path."), IFileManager::Get().FileExists(*CatalogPath))) return false;
    if (!TestTrue(TEXT("The production catalog loader reads the staged CSV without a source-tree fallback."), RunItemShopCatalog::Load(Catalog, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    if (!TestEqual(TEXT("The staged authored catalog retains all 49 supported equipment definitions."), Catalog.Num(), 49)) return false;
    TSet<FSoftObjectPath> Assets;
    for (const FRunItemDefinition& Item : Catalog)
    {
        if (!TestTrue(TEXT("Every catalog item retains its unique original asset path, display name, whole price and gameplay tags."), !Item.Asset.IsNull() && !Assets.Contains(Item.Asset) && !Item.DisplayName.IsEmpty() && Item.Price == 1 && Item.Tags.HasTag(RunItemShopCatalog::GetWeaponTag()))) return false;
        if (!TestNotNull(FString::Printf(TEXT("Every staged catalog item resolves an equipment profile: %s"), *Item.Asset.ToString()), URunEquipmentCatalog::Get().ResolveProfile(Item))) return false;
        if (!TestTrue(FString::Printf(TEXT("The original item package exists in the runtime filesystem: %s"), *Item.Asset.GetLongPackageName()), FPackageName::DoesPackageExist(Item.Asset.GetLongPackageName()))) return false;
        Assets.Add(Item.Asset);
    }
    AddInfo(FString::Printf(TEXT("Production CSV path=%s; items=%d; cooked=%d."), *CatalogPath, Catalog.Num(), FPlatformProperties::RequiresCookedData()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTodoPackagedSkillProfilesTest, "ProjectA.TodoReview.PackagedSkillProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTodoPackagedSkillProfilesTest::RunTest(const FString& Parameters)
{
    // Load the production party and construct only an in-memory shop; no checkpoint, authored asset or source default is modified.
    // 실제 파티를 불러와 메모리 상점만 구성하며 체크포인트·작성 에셋·원본 기본값을 변경하지 않습니다.
    TStrongObjectPtr<UPartyDefinitionDataAsset> Party(LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty")));
    URunEncounterPoolDataAsset* Pool = Party.IsValid() ? Party->RunEncounterPool.Get() : nullptr;
    if (!TestTrue(TEXT("The actual party retains its authored DrGame encounter and skill-pool references."), Pool && FSoftObjectPath(Pool) == FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Encounters/DA_DrGameRunEncounterPool.DA_DrGameRunEncounterPool")) && Pool->SkillShopPool.ToSoftObjectPath() == FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/SkillPools/DA_DrGameSkillShopPool.DA_DrGameSkillShopPool")))) return false;
    FRunSkillShopState Shop;
    FText Error;
    if (!TestTrue(TEXT("The official runtime builder resolves the authored catalog entirely in memory."), Pool->BuildSkillShop(Shop, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    if (!TestEqual(TEXT("The runtime skill catalog retains all 61 authored candidates."), Shop.Catalog.Num(), 61) || !TestTrue(TEXT("The official tag, weight, profile and offer validation accepts the memory-only shop."), URunEncounterPoolDataAsset::ValidateSkillShop(Shop, Error))) return false;
    TSet<FSoftObjectPath> Paths;
    TSet<FName> SkillIds;
    for (const FRunSkillShopOffer& Offer : Shop.Catalog)
    {
        if (!TestTrue(TEXT("Every runtime skill candidate retains one unique package, positive original price and weight."), !Offer.Skill.IsNull() && !Paths.Contains(Offer.Skill) && Offer.Price == 1 && Offer.BaseWeight > 0.f && FPackageName::DoesPackageExist(Offer.Skill.GetLongPackageName()))) return false;
        TStrongObjectPtr<USkillDefinitionDataAsset> Asset(Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad()));
        FCombatRoundSkill Skill;
        if (!TestTrue(FString::Printf(TEXT("The runtime filesystem resolves the original authored skill: %s"), *Offer.Skill.ToString()), Asset.IsValid() && Asset->ResolveRoundSkill(Skill, Error) && !SkillIds.Contains(Skill.SkillId) && Offer.Tags == Skill.EffectTags)) return false;
        Paths.Add(Offer.Skill);
        SkillIds.Add(Skill.SkillId);
    }
    struct FExpectedProfile
    {
        const TCHAR* Asset;
        const TCHAR* Niagara;
    };
    const FExpectedProfile Expected[] =
    {
        {TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/__GroundAttackVFX/DA_DrGame_GroundAttackVFX_Line_Lava.DA_DrGame_GroundAttackVFX_Line_Lava"), TEXT("/Game/__GroundAttackVFX/NS/NS_Line_Lava.NS_Line_Lava")},
        {TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/__AoeVFX/DA_DrGame_AoeVFX_AOE_PoisonCarousel.DA_DrGame_AoeVFX_AOE_PoisonCarousel"), TEXT("/Game/__AoeVFX/NS/NS_AOE_PoisonCarousel.NS_AOE_PoisonCarousel")},
        {TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/_LevelUpSpawn/DA_DrGame_LevelUpSpawn_Spawn_Ninja_Root.DA_DrGame_LevelUpSpawn_Spawn_Ninja_Root"), TEXT("/Game/_LevelUpSpawn/NS/NS_Spawn_Ninja_Root.NS_Spawn_Ninja_Root")}
    };
    TArray<FCombatRoundSkill> Profiles;
    for (const FExpectedProfile& Entry : Expected)
    {
        const FSoftObjectPath Path(Entry.Asset);
        TStrongObjectPtr<USkillDefinitionDataAsset> Asset(Cast<USkillDefinitionDataAsset>(Path.TryLoad()));
        FCombatRoundSkill Skill;
        if (!TestTrue(TEXT("Each repaired project DataAsset is an authored candidate with an unchanged resolved visual profile."), Paths.Contains(Path) && Asset.IsValid() && Asset->bUseRoundDefinition && Asset->ResolveRoundSkill(Skill, Error) && FCombatSkillVfx::StaticStruct()->CompareScriptStruct(&Asset->RoundDefinition.Vfx, &Skill.Vfx, 0))) return false;
        if (!TestTrue(TEXT("The loaded repaired profile retains its exact available original Niagara reference."), Skill.Vfx.Niagara.ToSoftObjectPath() == FSoftObjectPath(Entry.Niagara) && Skill.Vfx.Niagara.LoadSynchronous() != nullptr)) return false;
        Profiles.Add(MoveTemp(Skill));
    }
    if (!TestFalse(TEXT("The cooked Lava project profile has no StepDistance override."), Profiles[0].Vfx.FloatParameters.Contains(TEXT("User.StepDistance")))) return false;
    const FCombatSkillVfx& Poison = Profiles[1].Vfx;
    const bool* AudioOn = Poison.BoolParameters.Find(TEXT("User.AudioOn"));
    if (!TestTrue(TEXT("The cooked Poison project profile disables embedded audio and directly loads its identical original cue."), AudioOn && !*AudioOn && Poison.Sound.ToSoftObjectPath() == FSoftObjectPath(TEXT("/Game/__AoeVFX/_GenericSource/SFX/Sfx_Hit_Poison_Cue.Sfx_Hit_Poison_Cue")) && Poison.Sound.LoadSynchronous() != nullptr && Poison.SoundVolume == 1.f && Poison.SoundPitch == 1.f && Poison.SoundMaxDuration == 5.f)) return false;
    const FCombatSkillVfx& Ninja = Profiles[2].Vfx;
    if (!TestTrue(TEXT("The cooked Ninja project profile retains its repaired visual height without overriding original HeightOffset."), Ninja.RelativeTransform.GetTranslation().Z == -60.0 && !Ninja.FloatParameters.Contains(TEXT("User.HeightOffset")))) return false;
    AddInfo(FString::Printf(TEXT("Runtime skill catalog=%d; repaired project profiles=%d; cooked=%d; checkpoint writes=0; source Niagara defaults were not inspected."), Shop.Catalog.Num(), Profiles.Num(), FPlatformProperties::RequiresCookedData()));
    return true;
}

// Exercise real game-window resolution confirmation and timeout while restoring the user's settings.
// 사용자 설정을 복원하면서 실제 게임 창 해상도 확인과 시간 초과 복구를 실행합니다.
class FGameWindowOptions : public IAutomationLatentCommand
{
public:
    explicit FGameWindowOptions(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual ~FGameWindowOptions() override
    {
        if (Options.IsValid()) Options->DeactivateWidget();
        if (bCaptured)
        {
            UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
            Settings->SetScreenResolution(OriginalResolution);
            Settings->SetFullscreenMode(OriginalMode);
            Settings->ScalabilityQuality = OriginalQuality;
            Settings->SetVSyncEnabled(bOriginalVSync);
            Settings->ApplySettings(false);
            Settings->ConfirmVideoMode();
            Settings->SaveSettings();
        }
    }

    virtual bool Update() override
    {
        if (Started == 0) Started = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - Started > 40)
        {
            Test->AddError(FString::Printf(TEXT("Game window options timed out at stage %d."), Stage));
            return true;
        }
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetFirstPlayerController()) World = Context.World();
        }
        if (!World || !World->GetGameViewport() || !World->GetGameViewport()->Viewport) return false;
        UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
        if (Stage == 0)
        {
            OriginalResolution = Settings->GetScreenResolution();
            OriginalMode = Settings->GetFullscreenMode();
            OriginalQuality = Settings->ScalabilityQuality;
            bOriginalVSync = Settings->IsVSyncEnabled();
            bCaptured = true;
            Options.Reset(CreateWidget<UOptionsWidget>(World->GetFirstPlayerController(), UOptionsWidget::StaticClass()));
            Options->AddToViewport(100);
            Settings->SetFullscreenMode(EWindowMode::Windowed);
            Settings->SetScreenResolution(FIntPoint(1280, 720));
            Settings->ApplyResolutionSettings(false);
            Options->ActivateWidget();
            Stage = 1;
            return false;
        }
        if (Stage == 1)
        {
            if (World->GetGameViewport()->Viewport->GetSizeXY() != Settings->GetScreenResolution()) return false;
            UComboBoxString* Resolution = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("ResolutionSelect")));
            if (!Test->TestNotNull(TEXT("The real options screen exposes resolution choices."), Resolution)) return true;
            const int32 Index = Resolution->GetSelectedIndex() == 0 ? 1 : 0;
            if (!Test->TestTrue(TEXT("Two window resolutions are available for preview."), Resolution->GetOptionCount() > Index)) return true;
            PreviousResolution = Settings->GetScreenResolution();
            Resolution->SetSelectedIndex(Index);
            Options->ApplyOptions();
            Stage = 2;
            Started = FPlatformTime::Seconds();
            return false;
        }
        UWidget* Confirmation = Options->GetWidgetFromName(TEXT("VideoConfirmationPanel"));
        if (Stage == 2)
        {
            if (World->GetGameViewport()->Viewport->GetSizeXY() != Settings->GetScreenResolution()) return false;
            if (!Test->TestTrue(TEXT("Changing the real window exposes confirmation."), Confirmation && Confirmation->GetVisibility() == ESlateVisibility::Visible && Settings->GetScreenResolution() != PreviousResolution)) return true;
            Options->RevertOptions();
            Stage = 3;
            return false;
        }
        if (Stage == 3)
        {
            if (World->GetGameViewport()->Viewport->GetSizeXY() != PreviousResolution) return false;
            Test->TestEqual(TEXT("Explicit revert restores the actual game viewport."), Settings->GetScreenResolution(), PreviousResolution);
            UComboBoxString* Resolution = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("ResolutionSelect")));
            Resolution->SetSelectedIndex(Resolution->GetSelectedIndex() == 0 ? 1 : 0);
            Options->ApplyOptions();
            Stage = 4;
            Started = FPlatformTime::Seconds();
            return false;
        }
        if (Stage == 4)
        {
            if (!Confirmation || Confirmation->GetVisibility() != ESlateVisibility::Collapsed) return false;
            if (World->GetGameViewport()->Viewport->GetSizeXY() != PreviousResolution) return false;
            Test->TestEqual(TEXT("Unconfirmed video change times out and restores the actual viewport."), Settings->GetScreenResolution(), PreviousResolution);
            UComboBoxString* Resolution = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("ResolutionSelect")));
            Resolution->SetSelectedIndex(Resolution->GetSelectedIndex() == 0 ? 1 : 0);
            Options->ApplyOptions();
            Options->ConfirmOptions();
            const FIntPoint Confirmed = Settings->GetScreenResolution();
            Settings->LoadSettings(true);
            Test->TestEqual(TEXT("Confirmed resolution persists to disk."), Settings->GetScreenResolution(), Confirmed);
            Test->AddInfo(TEXT("Actual uncooked game window: preview, explicit revert, timeout revert and confirmed disk reload passed; original settings restored at teardown."));
            return true;
        }
        return false;
    }

private:
    FAutomationTestBase* Test;
    TStrongObjectPtr<UOptionsWidget> Options;
    int32 Stage = 0;
    double Started = 0;
    bool bCaptured = false;
    FIntPoint OriginalResolution;
    FIntPoint PreviousResolution;
    EWindowMode::Type OriginalMode = EWindowMode::Windowed;
    Scalability::FQualityLevels OriginalQuality;
    bool bOriginalVSync = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameWindowOptionsTest, "ProjectA.Menu.GameWindowOptions", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FGameWindowOptionsTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FGameWindowOptions(this));
    return true;
}

class FGameMenuSurrender : public IAutomationLatentCommand
{
public:
    explicit FGameMenuSurrender(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()), Slot(URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get())) {}
    virtual ~FGameMenuSurrender() override
    {
        if (bCreated) UGameplayStatics::DeleteGameInSlot(Slot, 0);
    }
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 30)
        {
            Test->AddError(TEXT("Surrender UI timed out."));
            return true;
        }
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game) World = Context.World();
        }
        if (!World || !World->GetGameInstance()) return false;
        TArray<UUserWidget*> Screens;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, UMainMenuScreenWidget::StaticClass(), false);
        if (Screens.IsEmpty()) return false;
        UMainMenuScreenWidget* Menu = Cast<UMainMenuScreenWidget>(Screens[0]);
        UButton* Surrender = Cast<UButton>(Menu->GetWidgetFromName(TEXT("Button_Surrender")));
        UButton* Continue = Cast<UButton>(Menu->GetWidgetFromName(TEXT("Button_Continue")));
        if (!Surrender || !Continue) return false;
        if (Stage == 0)
        {
            if (!Test->TestTrue(TEXT("Surrender UI uses a new isolated slot."), Slot.StartsWith(TEXT("ProjectA_Automation_Surrender_")) && !UGameplayStatics::DoesSaveGameExist(Slot, 0))) return true;
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
            FRunPartyMember Member;
            Member.SlotIndex = 0;
            Member.bCreated = true;
            Member.bPlayerControlled = true;
            Member.ClassId = TEXT("Warrior");
            Member.CharacterName = FText::FromString(TEXT("Surrender fixture"));
            FText Error;
            if (!Test->TestTrue(TEXT("Create the eligible single-player save."), Run->InitializeRun({ Member }, Error))) return true;
            bCreated = true;
            UGameplayStatics::LoadDataFromSlot(Before, Slot, 0);
            Menu->RefreshResumeActions();
            if (!Test->TestTrue(TEXT("Continue and surrender are initially available."), Continue->GetIsEnabled() && Surrender->GetIsEnabled())) return true;
            Surrender->OnClicked.Broadcast();
            Stage = 1;
            return false;
        }
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, URunSurrenderWidget::StaticClass(), false);
        URunSurrenderWidget* Confirmation = nullptr;
        for (UUserWidget* Candidate : Screens)
        {
            if (Cast<URunSurrenderWidget>(Candidate)->IsActivated()) Confirmation = Cast<URunSurrenderWidget>(Candidate);
        }
        if (Stage == 2)
        {
            if (Confirmation) return false;
            TArray<uint8> After;
            UGameplayStatics::LoadDataFromSlot(After, Slot, 0);
            Test->TestTrue(TEXT("Cancelling the real confirmation preserves the save bytes."), Before == After);
            Surrender->OnClicked.Broadcast();
            Stage = 3;
            return false;
        }
        if (!Confirmation) return false;
        UButton* Cancel = Cast<UButton>(Confirmation->GetWidgetFromName(TEXT("Button_SurrenderCancel")));
        UButton* Confirm = Cast<UButton>(Confirmation->GetWidgetFromName(TEXT("Button_SurrenderConfirm")));
        if (!Cancel || !Confirm || !Confirm->GetIsEnabled()) return false;
        if (Stage == 1)
        {
            Cancel->OnClicked.Broadcast();
            Stage = 2;
            return false;
        }
        FRunCheckpointStorage::FailNextDeleteForTesting();
        Confirm->OnClicked.Broadcast();
        TArray<uint8> After;
        UGameplayStatics::LoadDataFromSlot(After, Slot, 0);
        Test->TestTrue(TEXT("A failed UI deletion preserves bytes and allows retry."), Before == After && Confirmation->IsActivated() && Confirm->GetIsEnabled());
        Confirm->OnClicked.Broadcast();
        Test->TestTrue(TEXT("Successful UI surrender closes confirmation and disables Continue."), !Confirmation->IsActivated() && !Continue->GetIsEnabled() && !Surrender->GetIsEnabled() && !UGameplayStatics::DoesSaveGameExist(Slot, 0));
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started;
    FString Slot;
    int32 Stage = 0;
    bool bCreated = false;
    TArray<uint8> Before;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameMenuSurrenderTest, "ProjectA.Menu.GameMenuSurrender", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FGameMenuSurrenderTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FGameMenuSurrender(this));
    return true;
}

class FContinueSavedMenu : public IAutomationLatentCommand
{
public:
    explicit FContinueSavedMenu(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 30.0)
        {
            Test->AddError(FString::Printf(TEXT("Menu Continue timed out: menuClicked=%d rewardClicked=%d resultContinued=%d. %s"), bClicked, bRewardClicked, bContinued, *LastState));
            return true;
        }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || !World->IsGameWorld() || !World->GetGameInstance())
            {
                continue;
            }
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            CaptureState(bContinued ? TEXT("WaitingForShop") : bClicked ? TEXT("WaitingForResult") : TEXT("WaitingForMenu"), World, Run);
            if (!Run) continue;
            if (bContinued)
            {
                TArray<UUserWidget*> Screens;
                UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, URunEncounterWidget::StaticClass(), false);
                for (UUserWidget* Screen : Screens)
                {
                    URunEncounterWidget* Encounter = Cast<URunEncounterWidget>(Screen);
                    if (!Encounter->IsActivated()) continue;
                    UVerticalBox* Actions = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("EncounterActions")));
                    UButton* Action = Run->GetPhase() == ERunPhase::EncounterChoice ? (Actions ? Cast<UButton>(Actions->GetChildAt(1)) : nullptr) : Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_LeaveShop")));
                    CaptureState(TEXT("WaitingForShopAction"), World, Run, Screen, Action);
                    if (Action && Action->GetIsEnabled()) Action->OnClicked.Broadcast();
                    break;
                }
                if (Run->GetPhase() != ERunPhase::Map) return false;
                Test->TestTrue(TEXT("Saved result Continue and shop buttons unlock the second encounter."), Run->CanStartNode(TEXT("Combat_02")));
                UGameplayStatics::DeleteGameInSlot(URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()), 0);
                return true;
            }
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, bClicked ? UEncounterResultWidget::StaticClass() : UMainMenuScreenWidget::StaticClass(), false);
            for (UUserWidget* Widget : Widgets)
            {
                UButton* Continue = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_Continue")));
                if (!bClicked)
                {
                    UMainMenuScreenWidget* Menu = Cast<UMainMenuScreenWidget>(Widget);
                    CaptureState(TEXT("WaitingForMenuContinue"), World, Run, Widget, Continue);
                    if (!Menu || !Menu->IsActivated()) continue;
                    if (!VerifySavedItemDefinitions()) return true;
                    FText EligibilityError;
                    const bool bEligible = Run->CanContinueStandaloneSavedRun(EligibilityError);
                    LastState += FString::Printf(TEXT(" eligible=%d explicitSlot=%s fixtureExists=%d eligibilityError=%s"), bEligible, *URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()), UGameplayStatics::DoesSaveGameExist(URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()), 0), *EligibilityError.ToString());
                    Test->AddInfo(LastState);
                    // Verify the writer fixture and the actual menu before invoking its delegate; do not repair eligibility in the test.
                    // delegate 호출 전에 writer 저장과 실제 메뉴를 검증하며 테스트에서 이어하기 적격성을 보정하지 않습니다.
                    if (!Test->TestTrue(TEXT("The independent writer fixture is eligible for Standalone menu Continue."), bEligible) || !Test->TestNotNull(TEXT("The active main menu exposes its authored Continue button."), Continue) || !Test->TestTrue(TEXT("The eligible active menu enables Continue."), Continue->GetIsEnabled()))
                    {
                        Test->AddError(LastState);
                        return true;
                    }
                    bClicked = true;
                    Continue->OnClicked.Broadcast();
                    CaptureState(TEXT("WaitingForGameplayTravel"), World, Run, Widget, Continue);
                    Test->AddInfo(LastState);
                    return false;
                }
                if (!Continue) continue;
                UEncounterResultWidget* Result = Cast<UEncounterResultWidget>(Widget);
                CaptureState(TEXT("WaitingForActiveResult"), World, Run, Widget, Continue);
                if (!Result || !Result->IsActivated()) continue;
                Test->TestTrue(TEXT("Menu Continue opens saved result"), Run->GetPhase() == ERunPhase::Result);
                if (!Test->TestEqual(TEXT("Restored party count"), Run->GetPartyMembers().Num(), 1))
                {
                    return true;
                }
                Test->TestEqual(TEXT("Menu restores name"), Run->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Restart Mage")));
                Test->TestEqual(TEXT("Menu restores HP"), Run->GetPartyMembers()[0].CurrentHP, 61.0f);
                if (!Test->TestTrue(TEXT("Menu restores the first completed victory without starting the next battle."), Run->GetCompletedNodes() == TArray<FName>{TEXT("Combat_01")} && Run->GetCurrentNodeId() == TEXT("Combat_01") && Run->GetLastResult() == ECombatResult::Victory)) return true;
                AGameplayPlayerController* Controller = Result->GetOwningPlayer<AGameplayPlayerController>();
                AGameplayGameState* State = World->GetGameState<AGameplayGameState>();
                CaptureState(TEXT("WaitingForRewardView"), World, Run, Widget, Continue);
                if (!Controller || !State || State->GetViewState().GoldRewardState.NodeId != Run->GetCurrentNodeId()) return false;
                const FGameplayViewState& View = State->GetViewState();
                UGameplayActionButton* Card = Cast<UGameplayActionButton>(Result->GetWidgetFromName(TEXT("GoldReward1")));
                CaptureState(bRewardClicked ? TEXT("WaitingForRewardClaim") : TEXT("WaitingForRewardCard"), World, Run, Widget, Continue);
                if (!bRewardClicked)
                {
                    if (!Card || !Card->GetIsEnabled() || Controller->IsRewardSelectionPending()) return false;
                    const FRunPartyMember& Member = Run->GetPartyMembers()[0];
                    RewardCharacterId = Controller->GetRewardCharacterId(View);
                    if (!Test->TestTrue(TEXT("The result restores three unclaimed choices for the original personal reward owner."), Run->GetGoldRewardState().SchemaVersion == 1 && Run->GetGoldRewardState().GoldChoices.Num() == 3 && Run->GetGoldRewardState().Claims.IsEmpty() && Run->GetGoldRewardRecipientIds() == TArray<FGuid>{RewardCharacterId} && RewardCharacterId == Member.CharacterId && Member.bPlayerControlled && !Member.OwnerAccountId.IsEmpty())) return true;
                    if (!Test->TestTrue(TEXT("Result Continue remains disabled until the personal reward is collected."), !Continue->GetIsEnabled() && !Run->CanContinueAfterRewards())) return true;
                    RewardChoices = Run->GetGoldRewardState().GoldChoices;
                    for (int32 Amount : RewardChoices)
                    {
                        if (!Test->TestTrue(TEXT("The restored reward choices stay within five to fifteen gold."), Amount >= 5 && Amount <= 15)) return true;
                    }
                    GoldBeforeReward = Member.Gold;
                    RewardOwner = Member.OwnerAccountId;
                    // Invoke the authored reward-card delegate and wait for its request to update both Run and visible state.
                    // 제작된 보상 카드 delegate를 호출하고 요청이 Run과 화면 상태에 모두 반영될 때까지 기다립니다.
                    Card->OnClicked.Broadcast();
                    bRewardClicked = true;
                    return false;
                }
                const FRunGoldRewardClaim* Claim = Run->GetGoldRewardState().Claims.FindByPredicate([this](const FRunGoldRewardClaim& Candidate) { return Candidate.CharacterId == RewardCharacterId; });
                const FRunGoldRewardClaim* VisibleClaim = View.GoldRewardState.Claims.FindByPredicate([this](const FRunGoldRewardClaim& Candidate) { return Candidate.CharacterId == RewardCharacterId; });
                if (!Claim || !VisibleClaim || Controller->IsRewardSelectionPending() || !Continue->GetIsEnabled()) return false;
                const FRunPartyMember& Member = Run->GetPartyMembers()[0];
                const FRunPartyMember* VisibleMember = View.PartyMembers.FindByPredicate([this](const FRunPartyMember& Candidate) { return Candidate.CharacterId == RewardCharacterId; });
                if (!Test->TestTrue(TEXT("The clicked card pays exactly its amount to the original owner and restores Continue."), Claim->ChoiceIndex == 0 && VisibleClaim->ChoiceIndex == 0 && Run->GetGoldRewardState().Claims.Num() == 1 && Member.CharacterId == RewardCharacterId && Member.OwnerAccountId == RewardOwner && Member.Gold == GoldBeforeReward + RewardChoices[0] && VisibleMember && VisibleMember->Gold == Member.Gold && Run->CanContinueAfterRewards() && View.bCanContinueAfterRewards)) return true;
                if (!Test->TestTrue(TEXT("Reward collection preserves all restored amounts and disables its claimed card."), Card && !Card->GetIsEnabled() && Run->GetGoldRewardState().GoldChoices == RewardChoices && View.GoldRewardState.GoldChoices == RewardChoices)) return true;
                Continue->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Result Continue opens the required encounter choice."), Run->GetPhase() == ERunPhase::EncounterChoice);
                bContinued = true;
                return false;
            }
        }
        return false;
    }

private:
    // Diagnose the actual writer save and verify that value comparison still rejects changed definition fields.
    // 실제 writer 저장을 진단하고 값 비교가 변경된 정의 필드를 계속 거절하는지 검증합니다.
    bool VerifySavedItemDefinitions()
    {
        FText Error;
        TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()), Error)));
        if (!Test->TestNotNull(TEXT("The real menu reads the isolated writer's Run SaveGame."), Save.Get()))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        for (const FRunPartyMember& Member : Save->Party)
        {
            for (const FRunItemDefinition& Item : Member.Items)
            {
                const FRunItemDefinition* CatalogItem = Save->ItemShopState.Catalog.FindByPredicate([&Item](const FRunItemDefinition& Candidate) { return Candidate.Asset == Item.Asset; });
                if (!Test->TestNotNull(TEXT("Every preserved starting item has its frozen catalog definition."), CatalogItem)) return false;
                Test->AddInfo(FString::Printf(TEXT("Saved item definition: asset=%s catalogName=%s ownedName=%s catalogTextKey=%s ownedTextKey=%s strictEqual=%d valueEqual=%d GIsEditor=%d"), *Item.Asset.ToString(), *CatalogItem->DisplayName.ToString(), *Item.DisplayName.ToString(), *FTextInspector::GetKey(CatalogItem->DisplayName).Get(FString()), *FTextInspector::GetKey(Item.DisplayName).Get(FString()), FRunItemDefinition::StaticStruct()->CompareScriptStruct(CatalogItem, &Item, 0), RunItemShopCatalog::IsSameDefinition(*CatalogItem, Item), GIsEditor));
                if (!Test->TestTrue(TEXT("Menu save validation preserves identical item values across Editor and game text identities."), RunItemShopCatalog::IsSameDefinition(*CatalogItem, Item))) return false;
                FRunItemDefinition Changed = Item;
                Changed.Price = Item.Price == 1 ? 2 : 1;
                if (!Test->TestFalse(TEXT("Saved item comparison rejects a changed price."), RunItemShopCatalog::IsSameDefinition(*CatalogItem, Changed))) return false;
                Changed = Item;
                Changed.Asset.Reset();
                if (!Test->TestFalse(TEXT("Saved item comparison rejects a changed asset."), RunItemShopCatalog::IsSameDefinition(*CatalogItem, Changed))) return false;
                Changed = Item;
                Changed.Tags.RemoveTag(RunItemShopCatalog::GetWeaponTag());
                if (!Test->TestFalse(TEXT("Saved item comparison rejects changed content tags."), RunItemShopCatalog::IsSameDefinition(*CatalogItem, Changed))) return false;
                Changed = Item;
                Changed.DisplayName = FText::FromString(Item.DisplayName.ToString() + TEXT(" changed"));
                if (!Test->TestFalse(TEXT("Saved item comparison rejects a changed frozen display name."), RunItemShopCatalog::IsSameDefinition(*CatalogItem, Changed))) return false;
            }
        }
        return true;
    }

    // Retain the latest menu, Run and reward diagnostics so every wait has a useful timeout reason.
    // 모든 대기의 시간 초과 원인을 확인하도록 최신 메뉴·Run·보상 진단을 보존합니다.
    void CaptureState(const TCHAR* Stage, UWorld* World, const URunStateSubsystem* Run, UUserWidget* Widget = nullptr, const UButton* Continue = nullptr)
    {
        const UCommonActivatableWidget* Active = Cast<UCommonActivatableWidget>(Widget);
        const UTextBlock* SaveStatus = Widget ? Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("SaveStatus"))) : nullptr;
        AGameplayPlayerController* Controller = Widget ? Widget->GetOwningPlayer<AGameplayPlayerController>() : nullptr;
        const AGameplayGameState* State = World->GetGameState<AGameplayGameState>();
        const UButton* Card = Widget ? Cast<UButton>(Widget->GetWidgetFromName(TEXT("GoldReward1"))) : nullptr;
        LastState = FString::Printf(TEXT("stage=%s world=%s phase=%d node=%s encounter=%s party=%d completed=%d widget=%s active=%d continueFound=%d continueEnabled=%d saveError=%s saveStatus=%s"), Stage, *World->GetName(), Run ? static_cast<int32>(Run->GetPhase()) : INDEX_NONE, Run ? *Run->GetCurrentNodeId().ToString() : TEXT("None"), Run ? *Run->GetCurrentEncounterId().ToString() : TEXT("None"), Run ? Run->GetPartyMembers().Num() : INDEX_NONE, Run ? Run->GetCompletedNodes().Num() : INDEX_NONE, *GetNameSafe(Widget), Active && Active->IsActivated(), Continue != nullptr, Continue && Continue->GetIsEnabled(), Run ? *Run->GetSaveError().ToString() : TEXT("NoRun"), SaveStatus ? *SaveStatus->GetText().ToString() : TEXT("NoSaveStatus"));
        LastState += FString::Printf(TEXT(" controller=%s gameState=%s rewardNode=%s choices=%d claims=%d recipients=%d viewRewardNode=%s viewClaims=%d cardFound=%d cardEnabled=%d rewardPending=%d rewardMessage=%s"), *GetNameSafe(Widget ? Widget->GetOwningPlayer() : nullptr), *GetNameSafe(State), Run ? *Run->GetGoldRewardState().NodeId.ToString() : TEXT("None"), Run ? Run->GetGoldRewardState().GoldChoices.Num() : INDEX_NONE, Run ? Run->GetGoldRewardState().Claims.Num() : INDEX_NONE, Run ? Run->GetGoldRewardRecipientIds().Num() : INDEX_NONE, State ? *State->GetViewState().GoldRewardState.NodeId.ToString() : TEXT("None"), State ? State->GetViewState().GoldRewardState.Claims.Num() : INDEX_NONE, Card != nullptr, Card && Card->GetIsEnabled(), Controller && Controller->IsRewardSelectionPending(), Controller ? *Controller->GetRewardSelectionMessage().ToString() : TEXT("NoGameplayController"));
    }

    FAutomationTestBase* Test;
    double Started;
    bool bClicked = false;
    bool bRewardClicked = false;
    bool bContinued = false;
    FGuid RewardCharacterId;
    FRunAccountId RewardOwner;
    TArray<int32> RewardChoices;
    int32 GoldBeforeReward = 0;
    FString LastState = TEXT("No game world with a GameInstance was found.");
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedContinueTest, "ProjectA.Menu.PackagedContinue", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedContinueTest::RunTest(const FString& Parameters)
{
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    if (!TestTrue(TEXT("Continue automation requires the explicit isolated writer slot."), Slot == TEXT("T11_ProcessRestart") || Slot.StartsWith(TEXT("ProjectA_Automation_Restart_")))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FContinueSavedMenu(this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedMenuQuitTest, "ProjectA.Menu.PackagedQuit", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedMenuQuitTest::RunTest(const FString& Parameters)
{
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (!World || !World->IsGameWorld())
        {
            continue;
        }
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UMainMenuScreenWidget::StaticClass(), false);
        for (UUserWidget* Widget : Widgets)
        {
            if (UButton* Quit = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_Quit"))))
            {
                const TWeakObjectPtr<UButton> WeakQuit = Quit;
                World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakQuit]()
                {
                    if (WeakQuit.IsValid())
                    {
                        UE_LOG(LogTemp, Display, TEXT("[T11] Invoking packaged main-menu Quit button."));
                        WeakQuit->OnClicked.Broadcast();
                    }
                }));
                return true;
            }
        }
    }
    AddError(TEXT("Packaged MainMenu Quit button was not found."));
    return false;
}

#endif
