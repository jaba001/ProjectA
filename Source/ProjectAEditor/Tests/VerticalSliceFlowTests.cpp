#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "UnrealClient.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace ProjectAVerticalSliceTests
{
template <typename T>
T* FindActiveWidget(UWorld* World)
{
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        T* TypedWidget = Cast<T>(Widget);
        if (TypedWidget && TypedWidget->IsActivated())
        {
            return TypedWidget;
        }
    }
    return nullptr;
}

// Preserve saved-menu, preview cleanup and character draft coverage independently of combat execution.
// 전투 실행과 독립적으로 저장된 메뉴, 미리보기 정리 및 캐릭터 초안 검증을 유지합니다.
class FPlayMenuLifecycle : public IAutomationLatentCommand
{
public:
    explicit FPlayMenuLifecycle(FAutomationTestBase* InTest) : Test(InTest), StageStarted(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StageStarted > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("Saved menu PIE timed out at stage %d."), Stage));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->GetFirstPlayerController())
        {
            return false;
        }
        AMainMenuPlayerController* Menu = Cast<AMainMenuPlayerController>(World->GetFirstPlayerController());
        if (!Menu || !Menu->GetMainMenuRootWidget()) return false;
        UCommonActivatableWidgetStack* MainStack = Cast<UCommonActivatableWidgetStack>(Menu->GetMainMenuRootWidget()->GetWidgetFromName(TEXT("MainStack")));
        UCommonActivatableWidgetStack* MenuStack = Cast<UCommonActivatableWidgetStack>(Menu->GetMainMenuRootWidget()->GetWidgetFromName(TEXT("MenuStack")));
        if (!Require(MainStack && MenuStack, TEXT("The saved menu exposes its actual main and flow stacks."))) return true;

        if (Stage == 0)
        {
            UMainMenuScreenWidget* FirstMenu = Cast<UMainMenuScreenWidget>(MainStack->GetActiveWidget());
            if (!FirstMenu || !FirstMenu->IsActivated()) return false;
            if (!CheckMenuLifecycle(Menu))
            {
                return true;
            }
            if (!CheckOptions(Menu))
            {
                return true;
            }
            FirstMenu->RequestNewGame();
            Advance();
            return false;
        }
        if (Stage == 1)
        {
            UGameModeSelectionWidget* Selection = Cast<UGameModeSelectionWidget>(MenuStack->GetActiveWidget());
            if (!Selection || !Selection->IsActivated()) return false;
            UButton* SinglePlayer = Cast<UButton>(Selection->GetWidgetFromName(TEXT("Button_SinglePlayer")));
            UButton* Multiplayer = Cast<UButton>(Selection->GetWidgetFromName(TEXT("Button_Multiplayer")));
            if (!Require(SinglePlayer && Multiplayer && SinglePlayer->GetIsEnabled(), TEXT("Starting a game offers separate single-player and multiplayer choices."))) return true;
            Test->TestTrue(TEXT("Mode selection hides the first menu."), MainStack->GetVisibility() == ESlateVisibility::Hidden);
            Test->TestTrue(TEXT("Mode selection initially focuses single-player."), Selection->GetDesiredFocusTarget() == SinglePlayer);
            SinglePlayer->OnClicked.Broadcast();
            Advance();
            return false;
        }
        if (Stage == 2)
        {
            UCharacterCreationWidget* Creation = Cast<UCharacterCreationWidget>(MenuStack->GetActiveWidget());
            if (!Creation || !Creation->IsActivated())
            {
                return false;
            }
            UButton* CreateButton = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Create")));
            if (!Require(CreateButton != nullptr, TEXT("CharacterCreation contains the slot zero create button.")))
            {
                return true;
            }
            if (!bProfessionPanelTested)
            {
                if (PreviewCaptureStage < 3)
                {
                    if (PreviewCaptureStage == 0)
                    {
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index))))->OnClicked.Broadcast();
                        }
                        PreviewCaptureStage = 1;
                        ProfessionPanelTime = FPlatformTime::Seconds();
                        return false;
                    }
                    if (FPlatformTime::Seconds() - ProfessionPanelTime < 0.5)
                    {
                        return false;
                    }
                    if (PreviewCaptureStage == 1)
                    {
                        const float ScreenBottom = Creation->GetCachedGeometry().LocalToAbsolute(Creation->GetCachedGeometry().GetLocalSize()).Y;
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            UWidget* Info = Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_ClassInfo"), Index)));
                            const FGeometry& Geometry = Info->GetCachedGeometry();
                            Test->TestTrue(TEXT("Every ClassInfo button fits inside the creation screen."), Geometry.LocalToAbsolute(Geometry.GetLocalSize()).Y <= ScreenBottom + 1.0f);
                        }
                        Capture(TEXT("00-FourPreviews.png"));
                        PreviewCaptureStage = 2;
                        ProfessionPanelTime = FPlatformTime::Seconds();
                        return false;
                    }
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Delete"), Index))))->OnClicked.Broadcast();
                    }
                    PreviewCaptureStage = 3;
                }
                CreateButton->OnClicked.Broadcast();
                UButton* Edit = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Edit")));
                UButton* Info = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_ClassInfo")));
                UEditableTextBox* NameInput = Cast<UEditableTextBox>(Creation->GetWidgetFromName(TEXT("ProfessionNameInput")));
                UComboBoxString* ClassSelect = Cast<UComboBoxString>(Creation->GetWidgetFromName(TEXT("ProfessionClassSelect")));
                if (!Require(Edit && Info && NameInput && ClassSelect, TEXT("Slot detail controls exist.")))
                {
                    return true;
                }
                Edit->OnClicked.Broadcast();
                NameInput->SetText(FText::FromString(TEXT("   ")));
                Creation->SaveSlotDetails();
                Test->TestFalse(TEXT("Blank name cannot be saved."), Creation->GetPartyMembers()[0].CharacterName.ToString().TrimStartAndEnd().IsEmpty());
                NameInput->SetText(FText::FromString(TEXT("Vertical Slice Hero")));
                ClassSelect->SetSelectedIndex(2);
                Creation->SaveSlotDetails();
                Test->TestEqual(TEXT("Edited class is stored in slot zero."), Creation->GetPartyMembers()[0].ClassId, FName(TEXT("Archer")));
                Edit->OnClicked.Broadcast();
                NameInput->SetText(FText::FromString(TEXT("Discard this name")));
                ClassSelect->SetSelectedIndex(1);
                Creation->CloseSlotDetails();
                Test->TestEqual(TEXT("Cancel preserves the saved class."), Creation->GetPartyMembers()[0].ClassId, FName(TEXT("Archer")));
                Test->TestEqual(TEXT("Cancel preserves the saved name."), Creation->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Vertical Slice Hero")));
                Info->OnClicked.Broadcast();
                UTextBlock* Details = Cast<UTextBlock>(Creation->GetWidgetFromName(TEXT("ProfessionDetailText")));
                Test->TestEqual(TEXT("ClassInfo uses the shared catalog."), Details->GetText().ToString(), Creation->PartyDefinition->GetProfessionDetails(TEXT("Archer")).ToString());

                bProfessionPanelTested = true;
                ProfessionPanelTime = FPlatformTime::Seconds();
                return false;
            }
            if (FPlatformTime::Seconds() - ProfessionPanelTime < 0.5)
            {
                return false;
            }
            if (!bProfessionCaptured)
            {
                Capture(TEXT("00-ProfessionDetails.png"));
                bProfessionCaptured = true;
                ProfessionPanelTime = FPlatformTime::Seconds();
                return false;
            }
            Creation->CloseSlotDetails();
            const TArray<FRunPartyMember> Members = Creation->GetPartyMembers();
            if (!Require(Members.Num() == 4 && Members[0].bCreated && !Members[1].bCreated && !Members[2].bCreated && !Members[3].bCreated, TEXT("Character creation exports one created member and three empty slots.")))
            {
                return true;
            }
            Creation->RequestBack();
            Test->TestFalse(TEXT("Completed character draft closes without starting a Run."), Creation->IsActivated());
            Advance();
            return false;
        }
        if (Stage == 3)
        {
            UGameModeSelectionWidget* Selection = Cast<UGameModeSelectionWidget>(MenuStack->GetActiveWidget());
            if (!Selection || !Selection->IsActivated()) return false;
            Test->TestTrue(TEXT("Closing character creation restores mode selection while the first menu stays hidden."), MainStack->GetVisibility() == ESlateVisibility::Hidden);
            UButton* Back = Cast<UButton>(Selection->GetWidgetFromName(TEXT("Button_GameModeBack")));
            if (!Require(Back != nullptr, TEXT("Mode selection provides a way back to the first menu."))) return true;
            Back->OnClicked.Broadcast();
            Advance();
            return false;
        }
        if (Stage == 4)
        {
            if (MenuStack->GetActiveWidget()) return false;
            UMainMenuScreenWidget* FirstMenu = Cast<UMainMenuScreenWidget>(MainStack->GetActiveWidget());
            Test->TestTrue(TEXT("Leaving mode selection restores the active visible first menu."), FirstMenu && FirstMenu->IsActivated() && MainStack->GetVisibility() == ESlateVisibility::Visible);
            return true;
        }

        return true;
    }

private:
    bool Require(bool bCondition, const TCHAR* Message)
    {
        return Test->TestTrue(Message, bCondition);
    }

    void Advance()
    {
        ++Stage;
        StageStarted = FPlatformTime::Seconds();
        Test->AddInfo(FString::Printf(TEXT("Saved menu PIE stage %d."), Stage));
    }

    bool CheckMenuLifecycle(AMainMenuPlayerController* Menu)
    {
        AMainMenuPreviewStage* Preview = Menu->GetPreviewStage();
        Test->TestNull(TEXT("MainMenu does not spawn an extra combat pawn."), Menu->GetPawn());
        if (!Require(Preview && Menu->GetViewTarget() == Preview, TEXT("Saved MainMenu has an active preview camera.")))
        {
            return false;
        }
        UMainMenuRootWidget* NativeRoot = CreateWidget<UMainMenuRootWidget>(Menu, UMainMenuRootWidget::StaticClass());
        NativeRoot->AddToViewport();
        for (const TCHAR* StackName : { TEXT("MainStack"), TEXT("MenuStack"), TEXT("ModalStack") })
        {
            Test->TestNotNull(TEXT("Native root supplies each stack."), NativeRoot->GetWidgetFromName(StackName));
        }
        Test->TestNotNull(TEXT("Native menu pushes to stack."), NativeRoot->PushMainScreen(UMainMenuScreenWidget::StaticClass()));
        UClass* Designer = LoadClass<UCharacterCreationWidget>(nullptr, TEXT("/Game/User_JeHoon/UI/MainMenu/WBP_CharacterCreationWidget.WBP_CharacterCreationWidget_C"));
        for (UClass* WidgetClass : { UCharacterCreationWidget::StaticClass(), Designer })
        {
            UCharacterCreationWidget* Draft = CreateWidget<UCharacterCreationWidget>(Menu, WidgetClass);
            if (!Require(Draft != nullptr, TEXT("Native and Designer creation widgets instantiate.")))
            {
                return false;
            }
            Draft->AddToViewport();
            for (int32 Cycle = 0; Cycle < 2; ++Cycle)
            {
                Draft->ActivateWidget();
                for (const FRunPartyMember& Member : Draft->GetPartyMembers())
                {
                    Test->TestFalse(TEXT("Every new visit starts with an empty draft."), Member.bCreated);
                    Test->TestFalse(TEXT("Every new visit clears the direct-control selection."), Member.bPlayerControlled);
                }
                UButton* Start = Cast<UButton>(Draft->GetWidgetFromName(TEXT("Button_StartGame")));
                if (!Require(Start && !Start->GetIsEnabled(), TEXT("A fresh draft requires a direct-control selection before starting."))) return false;
                Test->TestFalse(TEXT("An empty slot cannot receive direct control."), Draft->SelectPlayerControlledSlot(2));
                TArray<TWeakObjectPtr<AActor>> PreviousPreviews;
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    UButton* Create = Cast<UButton>(Draft->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index))));
                    if (!Require(Create != nullptr, TEXT("All four creation controls are bound.")))
                    {
                        return false;
                    }
                    Create->OnClicked.Broadcast();
                    AActor* Actor = Preview->GetPreviewActorForSlot(Index);
                    Test->TestNotNull(TEXT("Each profession has a visible preview class."), Actor);
                    Test->TestNull(TEXT("Preview cannot execute pawn AI or combat."), Cast<APawn>(Actor));
                    PreviousPreviews.Add(Actor);
                }
                UButton* ThirdControl = Cast<UButton>(Draft->GetWidgetFromName(TEXT("Button_Slot2_PlayerControl")));
                UButton* FourthControl = Cast<UButton>(Draft->GetWidgetFromName(TEXT("Button_Slot3_PlayerControl")));
                UButton* DeleteThird = Cast<UButton>(Draft->GetWidgetFromName(TEXT("Button_Slot2_Delete")));
                if (!Require(ThirdControl && FourthControl && DeleteThird, TEXT("Native and Designer cards expose direct-control selection."))) return false;
                Test->TestFalse(TEXT("Creating companions does not silently select a player character."), Start->GetIsEnabled());
                ThirdControl->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Selecting the third card enables starting with one player character."), Start->GetIsEnabled() && Draft->GetPartyMembers()[2].bPlayerControlled);
                FourthControl->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Selecting a different card replaces the single selection."), !Draft->GetPartyMembers()[2].bPlayerControlled && Draft->GetPartyMembers()[3].bPlayerControlled);
                Draft->ShowSlotDetails(3, true);
                Test->TestFalse(TEXT("The detail modal prevents changing direct control behind it."), Draft->SelectPlayerControlledSlot(0));
                Draft->CloseSlotDetails();
                Test->TestTrue(TEXT("Closing details preserves the selected character."), Draft->GetPartyMembers()[3].bPlayerControlled);
                ThirdControl->OnClicked.Broadcast();
                DeleteThird->OnClicked.Broadcast();
                Test->TestFalse(TEXT("Deleting the selected character requires an explicit replacement selection."), Start->GetIsEnabled());
                Test->TestFalse(TEXT("Deleting a character clears its saved selection flag."), Draft->GetPartyMembers()[2].bPlayerControlled);
                FourthControl->OnClicked.Broadcast();
                Test->TestTrue(TEXT("A surviving companion can be selected before starting."), Start->GetIsEnabled() && Draft->GetPartyMembers()[3].bPlayerControlled);
                if (Cycle == 0)
                {
                    UButton* Close = Cast<UButton>(Draft->GetWidgetFromName(TEXT("Button_Close")));
                    if (!Require(Close != nullptr, TEXT("Close button exists.")))
                    {
                        return false;
                    }
                    Close->OnClicked.Broadcast();
                }
                else
                {
                    Draft->RequestBack();
                }
                Test->TestFalse(TEXT("Back and X deactivate the creation screen."), Draft->IsActivated());
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    Test->TestNull(TEXT("Closing releases every preview slot."), Preview->GetPreviewActorForSlot(Index));
                    Test->TestFalse(TEXT("Closing destroys the previous preview actor."), PreviousPreviews[Index].IsValid());
                }
                Test->TestTrue(TEXT("Back keeps the menu preview camera."), Menu->GetViewTarget() == Preview);
            }
            Draft->RemoveFromParent();
        }
        Preview->SetPreviewActorForSlot(0, TEXT("Archer"));
        Preview->SetPreviewActorForSlot(0, TEXT("MissingProfession"));
        Test->TestNull(TEXT("Missing preview class clears the old actor."), Preview->GetPreviewActorForSlot(0));
        NativeRoot->RemoveFromParent();
        return true;
    }

    bool CheckOptions(AMainMenuPlayerController* Menu)
    {
        UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
        UOptionsWidget* Options = CreateWidget<UOptionsWidget>(Menu, UOptionsWidget::StaticClass());
        if (!Require(Settings && Options, TEXT("The options screen and engine settings are available.")))
        {
            return false;
        }
        const Scalability::FQualityLevels PreviousQuality = Settings->ScalabilityQuality;
        const bool bPreviousVSync = Settings->IsVSyncEnabled();
        const FIntPoint PreviousResolution = Settings->GetScreenResolution();
        const EWindowMode::Type PreviousWindowMode = Settings->GetFullscreenMode();
        ON_SCOPE_EXIT
        {
            Options->DeactivateWidget();
            Settings->SetFullscreenMode(PreviousWindowMode);
            Settings->SetScreenResolution(PreviousResolution);
            // Restore the complete quality snapshot after video setters adjust resolution scale.
            // 화면 설정 함수가 렌더링 비율을 조정한 뒤 전체 품질 스냅샷을 복구합니다.
            Settings->ScalabilityQuality = PreviousQuality;
            Settings->SetVSyncEnabled(bPreviousVSync);
            Settings->ApplyNonResolutionSettings();
            Settings->SaveSettings();
        };
        Options->ActivateWidget();
        UComboBoxString* Resolution = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("ResolutionSelect")));
        UComboBoxString* WindowMode = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("WindowModeSelect")));
        UComboBoxString* Quality = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("QualitySelect")));
        UCheckBox* VSync = Cast<UCheckBox>(Options->GetWidgetFromName(TEXT("VSyncCheck")));
        if (!Require(Resolution && WindowMode && Quality && VSync, TEXT("Options exposes resolution, screen mode, quality and VSync controls.")))
        {
            return false;
        }
        Test->TestEqual(TEXT("All three screen modes are available."), WindowMode->GetOptionCount(), 3);
        Test->TestTrue(TEXT("The current screen has a selectable resolution."), Resolution->GetSelectedIndex() != INDEX_NONE);
        const FString SavedResolutionOption = Resolution->GetSelectedOption();
        const int32 SavedQualityIndex = Quality->GetSelectedIndex();
        WindowMode->SetSelectedIndex(static_cast<int32>(EWindowMode::WindowedFullscreen));
        Test->TestFalse(TEXT("Borderless mode disables independent resolution selection."), Resolution->GetIsEnabled());
        WindowMode->SetSelectedIndex(static_cast<int32>(EWindowMode::Windowed));
        Test->TestTrue(TEXT("Windowed mode enables available resolution choices."), Resolution->GetIsEnabled());
        WindowMode->SetSelectedIndex(static_cast<int32>(PreviousWindowMode == EWindowMode::Windowed ? EWindowMode::Fullscreen : EWindowMode::Windowed));
        if (Resolution->GetOptionCount() > 1) Resolution->SetSelectedIndex((Resolution->GetSelectedIndex() + 1) % Resolution->GetOptionCount());
        Quality->SetSelectedIndex(1);
        VSync->SetIsChecked(!bPreviousVSync);
        Test->TestTrue(TEXT("Draft quality edits do not change engine settings."), Settings->ScalabilityQuality == PreviousQuality);
        Test->TestEqual(TEXT("Options remain unchanged before Apply."), Settings->IsVSyncEnabled(), bPreviousVSync);
        Test->TestEqual(TEXT("Draft screen mode edits do not change engine settings."), Settings->GetFullscreenMode(), PreviousWindowMode);
        Test->TestEqual(TEXT("Draft resolution edits do not change engine settings."), Settings->GetScreenResolution(), PreviousResolution);
        Options->DeactivateWidget();
        Options->ActivateWidget();
        Test->TestEqual(TEXT("Reopening discards the unsaved screen mode."), WindowMode->GetSelectedIndex(), static_cast<int32>(PreviousWindowMode));
        Test->TestEqual(TEXT("Reopening discards the unsaved resolution."), Resolution->GetSelectedOption(), SavedResolutionOption);
        Test->TestEqual(TEXT("Reopening discards the unsaved quality preset."), Quality->GetSelectedIndex(), SavedQualityIndex);
        Test->TestEqual(TEXT("Reopening discards the unsaved VSync value."), VSync->IsChecked(), bPreviousVSync);

        // Leave screen changes unapplied in PIE; real display confirmation is a separate user check.
        // PIE에서는 화면 변경을 적용하지 않으며 실제 화면 확인은 별도 사용자 검증으로 수행합니다.
        Quality->SetSelectedIndex(1);
        VSync->SetIsChecked(!bPreviousVSync);
        Options->ApplyOptions();
        Settings->LoadSettings(true);
        Test->TestEqual(TEXT("Quality persists after reload."), Settings->GetOverallScalabilityLevel(), 1);
        Test->TestEqual(TEXT("VSync persists after reload."), Settings->IsVSyncEnabled(), !bPreviousVSync);
        for (int32 CustomCase = 0; CustomCase < 2; ++CustomCase)
        {
            Settings->SetOverallScalabilityLevel(2);
            if (CustomCase == 0)
            {
                Settings->ScalabilityQuality.LandscapeQuality = 0;
            }
            else
            {
                Settings->ScalabilityQuality.ResolutionQuality = 73.0f;
            }
            const Scalability::FQualityLevels CustomQuality = Settings->ScalabilityQuality;
            Options->DeactivateWidget();
            Options->ActivateWidget();
            Test->TestEqual(TEXT("A custom landscape or render scale is displayed as custom quality."), Quality->GetSelectedIndex(), 5);
            const bool bSavedVSync = !Settings->IsVSyncEnabled();
            VSync->SetIsChecked(bSavedVSync);
            Options->ApplyOptions();
            Settings->LoadSettings(true);
            Test->TestTrue(TEXT("Saving VSync preserves all custom quality fields after reload."), Settings->ScalabilityQuality == CustomQuality);
            Test->TestEqual(TEXT("VSync saves immediately while custom quality is retained."), Settings->IsVSyncEnabled(), bSavedVSync);
            Scalability::FQualityLevels ExpectedPreset;
            ExpectedPreset.SetFromSingleQualityLevel(2);
            Quality->SetSelectedIndex(2);
            Options->ApplyOptions();
            Settings->LoadSettings(true);
            Test->TestTrue(TEXT("Selecting a preset replaces custom landscape and render scale values."), Settings->ScalabilityQuality == ExpectedPreset);
        }
        return true;
    }

    void Capture(const TCHAR* FileName)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VerticalSliceScreenshots"), FileName), true, false);
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    double StageStarted;
    bool bProfessionPanelTested = false;
    int32 PreviewCaptureStage = 0;
    bool bProfessionCaptured = false;
    double ProfessionPanelTime = 0.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceMenuLifecycleTest, "ProjectA.VerticalSlice.SavedMenuLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVerticalSliceMenuLifecycleTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/MainMenu")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectAVerticalSliceTests::FPlayMenuLifecycle>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
