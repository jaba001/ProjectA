#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Components/Button.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/MainMenu/RunSurrenderWidget.h"
#include "Components/VerticalBox.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ComboBoxString.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

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
            Test->AddError(TEXT("Packaged Continue timed out."));
            return true;
        }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || !World->IsGameWorld() || !World->GetGameInstance())
            {
                continue;
            }
            if (bContinued)
            {
                URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
                TArray<UUserWidget*> Screens;
                UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, URunEncounterWidget::StaticClass(), false);
                for (UUserWidget* Screen : Screens)
                {
                    URunEncounterWidget* Encounter = Cast<URunEncounterWidget>(Screen);
                    if (!Encounter->IsActivated()) continue;
                    UVerticalBox* Actions = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("EncounterActions")));
                    UButton* Action = Run->GetPhase() == ERunPhase::EncounterChoice ? (Actions ? Cast<UButton>(Actions->GetChildAt(1)) : nullptr) : Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_LeaveShop")));
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
                if (!Continue || !Continue->GetIsEnabled())
                {
                    continue;
                }
                if (!bClicked)
                {
                    bClicked = true;
                    Continue->OnClicked.Broadcast();
                    return false;
                }
                URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
                Test->TestTrue(TEXT("Menu Continue opens saved result"), Run->GetPhase() == ERunPhase::Result);
                if (!Test->TestEqual(TEXT("Restored party count"), Run->GetPartyMembers().Num(), 1))
                {
                    return true;
                }
                Test->TestEqual(TEXT("Menu restores name"), Run->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Restart Mage")));
                Test->TestEqual(TEXT("Menu restores HP"), Run->GetPartyMembers()[0].CurrentHP, 61.0f);
                Continue->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Result Continue opens the required encounter choice."), Run->GetPhase() == ERunPhase::EncounterChoice);
                bContinued = true;
                return false;
            }
        }
        return false;
    }

private:
    FAutomationTestBase* Test;
    double Started;
    bool bClicked = false;
    bool bContinued = false;
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
