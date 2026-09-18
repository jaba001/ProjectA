#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "UnrealClient.h"
#include "Unit/UnitBase.h"
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

// Drive saved-menu buttons and the public target delegate; operating-system mouse hit testing is separate.
// 저장된 메뉴 버튼과 공개 대상 선택 델리게이트를 사용하며 운영체제 마우스 히트 테스트는 별개입니다.
class FPlaySavedSkillLoadout : public IAutomationLatentCommand
{
public:
    FPlaySavedSkillLoadout(FAutomationTestBase* InTest, const TArray<FCombatRoundSkill>& InSkills, int32 InSkillIndex) : Test(InTest), Skills(InSkills), SkillIndex(InSkillIndex)
    {
    }

    virtual ~FPlaySavedSkillLoadout() override
    {
        if (ObservedTargetASC.IsValid()) ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UAS_Unit::GetHPAttribute()).Remove(HPChangedHandle);
    }

    virtual bool Update() override
    {
        if (StageStarted == 0.0) StageStarted = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - StageStarted > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("Saved skill PIE timed out at stage %d for %s. %s"), Stage, *Skills[SkillIndex].SkillId.ToString(), *UIReadiness));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->GetFirstPlayerController()) return false;
        if (Stage == 0)
        {
            AMainMenuPlayerController* Menu = Cast<AMainMenuPlayerController>(World->GetFirstPlayerController());
            UMainMenuScreenWidget* Screen = FindActiveWidget<UMainMenuScreenWidget>(World);
            if (!Menu || !Screen) return false;
            UButton* NewGame = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_NewGame")));
            if (!Require(NewGame && NewGame->GetIsEnabled(), TEXT("The saved main menu exposes its actual New Game button."))) return true;
            NewGame->OnClicked.Broadcast();
            Advance();
            return false;
        }
        if (Stage == 1)
        {
            UGameModeSelectionWidget* Selection = FindActiveWidget<UGameModeSelectionWidget>(World);
            if (!Selection) return false;
            UButton* Single = Cast<UButton>(Selection->GetWidgetFromName(TEXT("Button_SinglePlayer")));
            if (!Require(Single && Single->GetIsEnabled(), TEXT("The saved menu exposes single-player selection."))) return true;
            Single->OnClicked.Broadcast();
            Advance();
            return false;
        }
        if (Stage == 2)
        {
            UCharacterCreationWidget* Creation = FindActiveWidget<UCharacterCreationWidget>(World);
            if (!Creation) return false;
            UButton* Create = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Create")));
            if (!Require(Create && Create->GetIsEnabled(), TEXT("The actual creation screen can create slot zero."))) return true;
            Create->OnClicked.Broadcast();
            UButton* Control = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_PlayerControl")));
            UButton* Start = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_StartGame")));
            if (!Require(Control && Control->GetIsEnabled() && Start, TEXT("The created card exposes direct control and Start."))) return true;
            Control->OnClicked.Broadcast();
            const int32 CreatedCount = Creation->GetPartyMembers().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated; }).Num();
            if (!Require(CreatedCount == 1 && Start->GetIsEnabled(), TEXT("Exactly one controlled character starts without AI companions."))) return true;
            Start->OnClicked.Broadcast();
            Advance();
            return false;
        }
        AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
        if (!Controller) return false;
        URunStateSubsystem* Run = Controller->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        if (Stage == 3)
        {
            URunMapWidget* Map = FindActiveWidget<URunMapWidget>(World);
            if (!Map || !Run || Run->GetPhase() != ERunPhase::Map) return false;
            UVerticalBox* Nodes = Cast<UVerticalBox>(Map->GetWidgetFromName(TEXT("NodeList")));
            if (!Require(Nodes != nullptr, TEXT("The saved Run map exposes its node buttons."))) return true;
            for (UWidget* Child : Nodes->GetAllChildren())
            {
                UGameplayActionButton* Node = Cast<UGameplayActionButton>(Child);
                if (!Node || !Node->GetIsEnabled()) continue;
                Node->OnClicked.Broadcast();
                Advance();
                return false;
            }
            return !Require(false, TEXT("The new Run has an enabled first encounter button."));
        }
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        if (Stage == 7)
        {
            if (!Run) return false;
            if (Round && Round->GetView().PendingProjectiles > 0) bSawProjectile = true;
            if (Source.IsValid() && Source->GetCurrentActionPoint() == InitialAP - Skills[SkillIndex].ActionPointCost) bSawAPCost = true;
            const bool bSettled = Run->GetPhase() == ERunPhase::Result || (Round && (Round->GetView().Phase == ECombatRoundPhase::Finished || (Round->GetView().Phase == ECombatRoundPhase::Planning && Round->GetView().RoundNumber > InitialRound)));
            if (!bSettled) return false;
            const float ExpectedHP = FMath::Max(0.f, InitialTargetHP - Skills[SkillIndex].Power);
            Require(HPChangeCount == 1 && FMath::IsNearlyEqual(LowestTargetHP, ExpectedHP, 0.01f), TEXT("The actual target health changes once to the clamped expected value."));
            Require(bSawAPCost, TEXT("The selected skill consumes its authored AP cost."));
            Require(!Round || Round->GetView().PendingProjectiles == 0, TEXT("The round settles with no pending projectile."));
            if (Skills[SkillIndex].Kind == ECombatRoundSkillKind::Projectile) Require(bSawProjectile, TEXT("The ranged skill creates an actual in-flight round projectile."));
            Test->AddInfo(FString::Printf(TEXT("Saved skill used: %s (%s), enemy HP %.1f -> %.1f, HP changes %d, projectile observed %s."), *Skills[SkillIndex].Name.ToString(), *Skills[SkillIndex].SkillId.ToString(), InitialTargetHP, LowestTargetHP, HPChangeCount, bSawProjectile ? TEXT("yes") : TEXT("no")));
            return true;
        }
        UCombatRoundPlanningWidget* Planning = FindActiveWidget<UCombatRoundPlanningWidget>(World);
        if (!Round || !Planning || Round->GetView().Phase != ECombatRoundPhase::Planning || Controller->IsRoundRequestPending()) return false;
        if (Stage == 4)
        {
            const FCombatRoundUnitView* Controlled = Round->GetView().Units.FindByPredicate([Controller](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && Unit.OwnerSlot == Controller->GetRoundParticipantSlot() && Unit.HP > 0.f; });
            if (!Controlled || !IsValid(Controlled->Unit)) return false;
            Source = Controlled->Unit;
            SourceId = Controlled->UnitId;
            const int32 AllyCount = Round->GetView().Units.FilterByPredicate([](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy; }).Num();
            if (!Require(AllyCount == 1 && Controlled->SkillIds.Num() == Skills.Num(), TEXT("The real encounter spawns one ally with exactly four authored skills."))) return true;
            for (const FCombatRoundSkill& Skill : Skills)
            {
                if (!Require(Controlled->SkillIds.Contains(Skill.SkillId) && Round->FindSkill(Skill.SkillId), TEXT("Each saved skill belongs to the player's runtime loadout and catalogue."))) return true;
            }
            // Wait for the first real UI refresh before delivering one target-selection event.
            // 대상 선택 이벤트를 한 번 전달하기 전에 실제 UI의 첫 갱신을 기다립니다.
            const bool bInputReady = Controller->IsRoundInputEnabled() && Controller->OnRoundWorldTileClicked.IsBoundToObject(Planning);
            bool bButtonsCreated = true;
            UIReadiness = FString::Printf(TEXT("Input ready=%s; planning %s"), bInputReady ? TEXT("true") : TEXT("false"), *DescribeWidget(Planning));
            for (const FCombatRoundSkill& Skill : Skills)
            {
                UCombatRoundSkillButton* Button = FindSkillButton(Planning, Skill.SkillId);
                bButtonsCreated = bButtonsCreated && Button != nullptr;
                UIReadiness += FString::Printf(TEXT("; %s: %s"), *Skill.SkillId.ToString(), *DescribeWidget(Button));
            }
            if (!bInputReady || !bButtonsCreated || Planning->GetCachedGeometry().GetLocalSize().IsNearlyZero()) return false;
            const FCombatRoundUnitView* Target = nullptr;
            for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
            {
                if (!Unit.bEnemy || Unit.HP <= 0.f || !IsValid(Unit.Unit)) continue;
                if (!Target || FVector::DistSquared(Source->GetActorLocation(), Unit.Unit->GetActorLocation()) < FVector::DistSquared(Source->GetActorLocation(), Target->Unit->GetActorLocation())) Target = &Unit;
            }
            if (!Require(Target && Target->Unit->GetAttributeSet() && Target->Unit->GetAbilitySystemComponent(), TEXT("An actual enemy has observable GAS health."))) return true;
            TargetId = Target->UnitId;
            TargetCoord = Target->HomeCoord;
            InitialTargetHP = LowestTargetHP = Target->Unit->GetAttributeSet()->GetHP();
            ObservedTargetASC = Target->Unit->GetAbilitySystemComponent();
            HPChangedHandle = ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UAS_Unit::GetHPAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
            {
                if (Data.NewValue < Data.OldValue)
                {
                    LowestTargetHP = FMath::Min(LowestTargetHP, FMath::Max(0.f, Data.NewValue));
                    ++HPChangeCount;
                }
            });
            Controller->OnRoundWorldTileClicked.Broadcast(TargetCoord);
            Advance();
            return false;
        }
        if (Stage == 5)
        {
            if (FPlatformTime::Seconds() - StageStarted < 0.25) return false;
            TArray<UWidget*> Widgets;
            Planning->WidgetTree->GetAllWidgets(Widgets);
            TArray<UCombatRoundSkillButton*> Buttons;
            for (UWidget* Widget : Widgets)
            {
                if (UCombatRoundSkillButton* Button = Cast<UCombatRoundSkillButton>(Widget)) Buttons.Add(Button);
            }
            if (!Require(Buttons.Num() == Skills.Num(), TEXT("Selecting the enemy exposes exactly four real skill buttons."))) return true;
            for (const FCombatRoundSkill& Skill : Skills)
            {
                UCombatRoundSkillButton** Match = Buttons.FindByPredicate([&Skill](const UCombatRoundSkillButton* Button) { return Button->GetSkillId() == Skill.SkillId; });
                const FString State = FString::Printf(TEXT("Saved skill button %s is displayed and enabled for enemy %d: %s"), *Skill.SkillId.ToString(), TargetId, *DescribeWidget(Match ? *Match : nullptr));
                if (!Require(Match && IsDisplayed(*Match) && (*Match)->GetIsEnabled(), *State)) return true;
                const UTextBlock* Label = Cast<UTextBlock>((*Match)->GetContent());
                if (!Require(Label && Label->GetText().ToString().Contains(Skill.Name.ToString()), TEXT("The skill button displays the saved skill name."))) return true;
            }
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SkillLoadoutScreenshots"), FString::Printf(TEXT("%d-FourSkills.png"), SkillIndex + 1)), true, false);
            Advance();
            return false;
        }
        if (Stage == 6)
        {
            const FCombatRoundUnitView* Controlled = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SourceId; });
            if (!Require(Controlled != nullptr, TEXT("The selected player remains in the actual encounter."))) return true;
            if (bAwaitingPlan)
            {
                const int32 ExpectedIndex = PlanIndex < Skills.Num() ? PlanIndex : SkillIndex;
                if (!Require(Controlled->Command.SkillId == Skills[ExpectedIndex].SkillId && Controlled->Command.TargetUnitId == TargetId && Controlled->Command.TargetCoord == TargetCoord, TEXT("Clicking a real skill button submits its skill and selected enemy to the server plan."))) return true;
                bAwaitingPlan = false;
                ++PlanIndex;
            }
            if (PlanIndex <= Skills.Num())
            {
                const int32 NextIndex = PlanIndex < Skills.Num() ? PlanIndex : SkillIndex;
                UCombatRoundSkillButton* Button = FindSkillButton(Planning, Skills[NextIndex].SkillId);
                if (!Require(Button && Button->GetIsEnabled() && IsDisplayed(Button), TEXT("The next authored skill can be selected through its visible button."))) return true;
                Button->OnClicked.Broadcast();
                bAwaitingPlan = true;
                return false;
            }
            TArray<UWidget*> Widgets;
            Planning->WidgetTree->GetAllWidgets(Widgets);
            for (UWidget* Widget : Widgets)
            {
                UButton* Button = Cast<UButton>(Widget);
                const UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
                if (!Label || Label->GetText().ToString() != TEXT("준비 완료")) continue;
                if (!Require(Button->GetIsEnabled() && IsDisplayed(Button), TEXT("The actual Ready button accepts the final skill plan."))) return true;
                InitialAP = Source->GetCurrentActionPoint();
                InitialRound = Round->GetView().RoundNumber;
                Button->OnClicked.Broadcast();
                bSawAPCost = Source.IsValid() && Source->GetCurrentActionPoint() == InitialAP - Skills[SkillIndex].ActionPointCost;
                Advance();
                return false;
            }
            return !Require(false, TEXT("The active planning screen contains the real Ready button."));
        }
        return false;
    }

private:
    bool Require(bool bCondition, const TCHAR* Message)
    {
        return Test->TestTrue(FString::Printf(TEXT("[%s] %s"), *Skills[SkillIndex].SkillId.ToString(), Message), bCondition);
    }

    void Advance()
    {
        ++Stage;
        StageStarted = FPlatformTime::Seconds();
    }

    static bool IsDisplayed(UWidget* Widget)
    {
        if (!Widget || Widget->GetCachedGeometry().GetLocalSize().IsNearlyZero()) return false;
        for (UWidget* Current = Widget; Current; Current = Current->GetParent())
        {
            if (Current->GetVisibility() == ESlateVisibility::Collapsed || Current->GetVisibility() == ESlateVisibility::Hidden) return false;
        }
        return true;
    }

    static FString DescribeWidget(UWidget* Widget)
    {
        if (!Widget) return TEXT("missing widget");
        const FVector2D Size = Widget->GetCachedGeometry().GetLocalSize();
        FString HiddenAncestor = TEXT("none");
        for (UWidget* Current = Widget; Current; Current = Current->GetParent())
        {
            if (Current->GetVisibility() == ESlateVisibility::Collapsed || Current->GetVisibility() == ESlateVisibility::Hidden)
            {
                HiddenAncestor = FString::Printf(TEXT("%s(visibility=%d)"), *Current->GetName(), static_cast<int32>(Current->GetVisibility()));
                break;
            }
        }
        return FString::Printf(TEXT("%s visibility=%d enabled=%s size=%.1fx%.1f hidden ancestor=%s tooltip=%s"), *Widget->GetName(), static_cast<int32>(Widget->GetVisibility()), Widget->GetIsEnabled() ? TEXT("true") : TEXT("false"), Size.X, Size.Y, *HiddenAncestor, *Widget->GetToolTipText().ToString());
    }

    static UCombatRoundSkillButton* FindSkillButton(UCombatRoundPlanningWidget* Planning, FName SkillId)
    {
        TArray<UWidget*> Widgets;
        Planning->WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            UCombatRoundSkillButton* Button = Cast<UCombatRoundSkillButton>(Widget);
            if (Button && Button->GetSkillId() == SkillId) return Button;
        }
        return nullptr;
    }

    FAutomationTestBase* Test;
    TArray<FCombatRoundSkill> Skills;
    int32 SkillIndex;
    int32 Stage = 0;
    double StageStarted = 0.0;
    FString UIReadiness;
    int32 SourceId = INDEX_NONE;
    int32 TargetId = INDEX_NONE;
    FIntPoint TargetCoord = FIntPoint::ZeroValue;
    int32 PlanIndex = 0;
    bool bAwaitingPlan = false;
    TWeakObjectPtr<AUnitBase> Source;
    TWeakObjectPtr<UAbilitySystemComponent> ObservedTargetASC;
    FDelegateHandle HPChangedHandle;
    float InitialTargetHP = 0.f;
    float LowestTargetHP = 0.f;
    int32 HPChangeCount = 0;
    int32 InitialAP = 0;
    int32 InitialRound = 0;
    bool bSawAPCost = false;
    bool bSawProjectile = false;
};

// Delete only the unique slot whose absence was checked before any PIE command was queued.
// PIE 명령을 등록하기 전에 없음을 확인한 고유 슬롯만 삭제합니다.
class FCleanupSkillLoadoutSave : public IAutomationLatentCommand
{
public:
    FCleanupSkillLoadoutSave(FAutomationTestBase* InTest, const FString& InSlot) : Test(InTest), Slot(InSlot)
    {
    }

    virtual bool Update() override
    {
        if (GEditor->PlayWorld) return false;
        if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) Test->TestTrue(TEXT("The isolated skill-loadout save is removed after PIE."), UGameplayStatics::DeleteGameInSlot(Slot, 0));
        return true;
    }

private:
    FAutomationTestBase* Test;
    FString Slot;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceSavedSkillLoadoutTest, "ProjectA.VerticalSlice.SavedSkillLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVerticalSliceSavedSkillLoadoutTest::RunTest(const FString& Parameters)
{
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_SkillLoadout_");
    bool bValidSuffix = Slot.Len() > Prefix.Len();
    for (TCHAR Character : Slot.Mid(Prefix.Len()))
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_')) bValidSuffix = false;
    }
    if (!Slot.StartsWith(Prefix) || !bValidSuffix)
    {
        AddError(TEXT("Launch with -ProjectASaveSlot=ProjectA_Automation_SkillLoadout_<unique alphanumeric suffix> to protect existing Run saves."));
        return false;
    }
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        AddError(TEXT("The requested skill-loadout test slot already exists; choose a fresh suffix. No PIE game was started."));
        return false;
    }
    const TArray<FString> AssetNames = { TEXT("BPDA_DefaulatAttack"), TEXT("BPDA_RangedAttack"), TEXT("BPDA_AreaAttack"), TEXT("DA_SweepingStrike") };
    TArray<FCombatRoundSkill> Skills;
    for (const FString& Name : AssetNames)
    {
        const FString Path = FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/%s.%s"), *Name, *Name);
        const USkillDefinitionDataAsset* Asset = LoadObject<USkillDefinitionDataAsset>(nullptr, *Path);
        FCombatRoundSkill Skill;
        FText Error;
        if (!TestNotNull(TEXT("The authored skill asset exists."), Asset) || !Asset->ResolveRoundSkill(Skill, Error))
        {
            AddError(FString::Printf(TEXT("Cannot test saved skill %s: %s"), *Path, *Error.ToString()));
            return false;
        }
        Skills.Add(Skill);
    }
    AddInfo(TEXT("Runs four real saved-menu/encounter PIE sessions through button delegates and the public target delegate; does not synthesize operating-system mouse input."));
    for (int32 Index = 0; Index < Skills.Num(); ++Index)
    {
        ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/MainMenu")));
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectAVerticalSliceTests::FPlaySavedSkillLoadout>(this, Skills, Index));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    }
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectAVerticalSliceTests::FCleanupSkillLoadoutSave>(this, Slot));
    return true;
}

#endif
