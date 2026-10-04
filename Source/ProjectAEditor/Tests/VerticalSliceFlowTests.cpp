#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Game/Encounter/CombatArena.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
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
#include "DataAsset/CharacterAppearanceCatalog.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Engine/World.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"
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
#include "Unit/CharacterAppearanceComponent.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace ProjectAVerticalSliceTests
{
// Prepare the real PIE window without changing production display settings or accepting a smaller capture.
// 제품 화면 설정을 변경하거나 작은 캡처를 허용하지 않고 실제 PIE 창을 준비합니다.
class FPIEViewportPreparation
{
public:
    bool Update(FAutomationTestBase* Test, UWorld* World, FIntPoint ExpectedSize)
    {
        if (RequestedSize != ExpectedSize)
        {
            RequestedSize = ExpectedSize;
            bResizeRequested = bReady = bFailed = false;
            WarmFrames = ResizeRetries = 0;
        }
        if (bReady) return true;
        UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        if (!Window.IsValid() || !Viewport || !Viewport->Viewport) return false;
        if (!bResizeRequested)
        {
            const FIntPoint Before = Viewport->Viewport->GetSizeXY();
            Test->AddInfo(FString::Printf(TEXT("Saved UI viewport before resize: requested=%dx%d physical=%dx%d window=%s localGeometry=%s DPI=%.3f."), ExpectedSize.X, ExpectedSize.Y, Before.X, Before.Y, *FVector2D(Window->GetSizeInScreen()).ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), Window->GetDPIScaleFactor()));
            Window->Resize(FVector2D(ExpectedSize.X, ExpectedSize.Y));
            bResizeRequested = true;
            return false;
        }
        if (++WarmFrames < 2) return false;
        const FIntPoint After = Viewport->Viewport->GetSizeXY();
        if (After != ExpectedSize && ResizeRetries < 3)
        {
            const FVector2D ClientSize = Window->GetClientSizeInScreen();
            const FVector2D Correction(ExpectedSize.X - After.X, ExpectedSize.Y - After.Y);
            Test->AddInfo(FString::Printf(TEXT("Saved UI viewport resize retry=%d requested=%dx%d physical=%dx%d client=%s correction=%s."), ResizeRetries + 1, ExpectedSize.X, ExpectedSize.Y, After.X, After.Y, *ClientSize.ToString(), *Correction.ToString()));
            Window->Resize(ClientSize + Correction);
            ++ResizeRetries;
            WarmFrames = 0;
            return false;
        }
        Test->AddInfo(FString::Printf(TEXT("Saved UI viewport after resize: requested=%dx%d physical=%dx%d window=%s localGeometry=%s DPI=%.3f warmFrames=%d."), ExpectedSize.X, ExpectedSize.Y, After.X, After.Y, *FVector2D(Window->GetSizeInScreen()).ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), Window->GetDPIScaleFactor(), WarmFrames));
        bReady = Test->TestTrue(TEXT("The actual PIE viewport has the exact requested physical review dimensions within three border-feedback retries."), After == ExpectedSize);
        bFailed = !bReady;
        return bReady;
    }

    bool HasFailed() const { return bFailed; }

private:
    bool bResizeRequested = false;
    bool bReady = false;
    bool bFailed = false;
    int32 WarmFrames = 0;
    int32 ResizeRetries = 0;
    FIntPoint RequestedSize = FIntPoint::ZeroValue;
};

// Capture the actual gameplay Slate widget rather than the surrounding editor window.
// 주변 에디터 창 대신 실제 게임플레이 Slate 위젯을 캡처합니다.
bool CaptureGameplayUI(FAutomationTestBase* Test, UWorld* World, const FString& Filename, FIntPoint ExpectedSize)
{
    UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
    const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
    const FIntPoint ViewportSize = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    if (!Test->TestTrue(TEXT("The actual gameplay viewport retains the exact requested physical Slate UI capture dimensions."), Widget.IsValid() && ViewportSize == ExpectedSize)) return false;
    TArray<FColor> Pixels;
    FIntVector Dimensions;
    if (!Test->TestTrue(TEXT("The actual gameplay Slate capture has the complete viewport dimensions."), FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Dimensions) && Dimensions.X == ViewportSize.X && Dimensions.Y == ViewportSize.Y && Pixels.Num() == ViewportSize.X * ViewportSize.Y)) return false;
    for (FColor& Pixel : Pixels) Pixel.A = 255;
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Dimensions.X, Dimensions.Y, Pixels, Png);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    if (!Test->TestTrue(TEXT("The complete gameplay UI capture is saved before travel or PIE teardown."), !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *Filename) && IFileManager::Get().FileSize(*Filename) == Png.Num())) return false;
    Test->AddInfo(FString::Printf(TEXT("Gameplay Slate screenshot: %s (%dx%d)."), *Filename, Dimensions.X, Dimensions.Y));
    return true;
}

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

// Verify that creation commits the default character without opening its optional editor.
// 생성 즉시 기본 캐릭터가 확정되고 선택적인 편집창은 열리지 않는지 검사합니다.
bool VerifyCreatedSlot(FAutomationTestBase* Test, UCharacterCreationWidget* Creation, int32 SlotIndex)
{
    UWidget* Panel = Creation ? Creation->GetWidgetFromName(TEXT("ProfessionDetailPanel")) : nullptr;
    if (!Test->TestTrue(TEXT("Creating a character keeps the detail editor closed."), Panel && Panel->GetVisibility() == ESlateVisibility::Collapsed)) return false;
    const TArray<FRunPartyMember> Members = Creation->GetPartyMembers();
    if (!Test->TestTrue(TEXT("Creation immediately commits a named default character without selecting direct control."), Members.IsValidIndex(SlotIndex) && Members[SlotIndex].bCreated && !Members[SlotIndex].CharacterName.ToString().TrimStartAndEnd().IsEmpty() && !Members[SlotIndex].bPlayerControlled)) return false;
    UButton* Edit = Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Edit"), SlotIndex))));
    UButton* Control = Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_PlayerControl"), SlotIndex))));
    return Test->TestTrue(TEXT("The created card immediately offers editing and direct-control selection."), Edit && Edit->IsVisible() && Edit->GetIsEnabled() && Control && Control->IsVisible() && Control->GetIsEnabled());
}

// Preserve saved-menu, preview cleanup and character draft coverage independently of combat execution.
// 전투 실행과 독립적으로 저장된 메뉴, 미리보기 정리 및 캐릭터 초안 검증을 유지합니다.
class FPlayMenuLifecycle : public IAutomationLatentCommand
{
public:
    explicit FPlayMenuLifecycle(FAutomationTestBase* InTest) : Test(InTest), StageStarted(FPlatformTime::Seconds()), bFlowOnly(FParse::Param(FCommandLine::Get(), TEXT("ProjectAFlowOnly")))
    {
        if (bFlowOnly) Test->AddInfo(TEXT("-ProjectAFlowOnly skips preview animation playback, looping/loop-boundary checks and asset screenshots; immediate character creation, ClassInfo layout, selection/edit/save/cancel buttons and preview actor cleanup remain covered."));
    }

    virtual ~FPlayMenuLifecycle() override
    {
        RestorePreviewPointer();
    }

    virtual bool Update() override
    {
        if (!bRequestedPIE)
        {
            PlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            PlaySettings->SetPlayNetMode(PIE_Standalone);
            PlaySettings->SetPlayNumberOfClients(1);
            PlaySettings->SetRunUnderOneProcess(true);
            PlaySettings->bLaunchSeparateServer = false;
            PlaySettings->NewWindowWidth = 1280;
            PlaySettings->NewWindowHeight = 720;
            PlaySettings->SetClientWindowSize(FIntPoint(1280, 720));
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = PlaySettings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/MainMenu");
            GEditor->RequestPlaySession(Params);
            bRequestedPIE = true;
            StageStarted = FPlatformTime::Seconds();
            return false;
        }
        if (FPlatformTime::Seconds() - StageStarted > (Stage == 2 ? 120.0 : 60.0))
        {
            AMainMenuPlayerController* Menu = GEditor->PlayWorld ? Cast<AMainMenuPlayerController>(GEditor->PlayWorld->GetFirstPlayerController()) : nullptr;
            if (Menu) for (int32 Index = 0; Index < 4; ++Index) LogPreview(Menu, Index, TEXT("timeout"));
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
            if (PreviewRatioIndex == 0 && !ViewportPreparation.Update(Test, World, FIntPoint(1280, 720))) return ViewportPreparation.HasFailed();
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
                            if (!VerifyCreatedSlot(Test, Creation, Index)) return true;
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
                        if (!bFlowOnly && !ReviewPreviewRatios(Menu, World)) return bPreviewGeometryFailed || ViewportPreparation.HasFailed();
                        PreviewWorldStart = World->GetTimeSeconds();
                        const float ScreenBottom = Creation->GetCachedGeometry().LocalToAbsolute(Creation->GetCachedGeometry().GetLocalSize()).Y;
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            UWidget* Info = Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_ClassInfo"), Index)));
                            const FGeometry& Geometry = Info->GetCachedGeometry();
                            Test->TestTrue(TEXT("Every ClassInfo button fits inside the creation screen."), Geometry.LocalToAbsolute(Geometry.GetLocalSize()).Y <= ScreenBottom + 1.0f);
                            if (bFlowOnly) continue;
                            AActor* Actor = Menu->GetPreviewStage()->GetPreviewActorForSlot(Index);
                            USkeletalMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
                            UAnimSingleNodeInstance* Animation = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
                            if (!Require(Animation && Animation->IsPlaying() && Animation->IsLooping(), TEXT("Every preview runs its looping idle animation."))) return true;
                            PreviewAnimationLengths[Index] = Animation->GetLength();
                            PreviewAnimationRates[Index] = Animation->GetPlayRate();
                            PreviewAnimationStartTimes[Index] = PreviewAnimationLastTimes[Index] = Animation->GetCurrentTime();
                            if (!Require(PreviewAnimationLengths[Index] > 0.f && PreviewAnimationRates[Index] > 0.f, TEXT("Every preview has a positive authored loop length and play rate."))) return true;
                            LogPreview(Menu, Index, TEXT("start"));
                        }
                        if (!bFlowOnly)
                        {
                            PreviewCaptureStage = 2;
                            ProfessionPanelTime = FPlatformTime::Seconds();
                            return false;
                        }
                        PreviewCaptureStage = 2;
                    }
                    if (!bFlowOnly)
                    {
                        bool bWholeCycles = true;
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            AActor* Actor = Menu->GetPreviewStage()->GetPreviewActorForSlot(Index);
                            USkeletalMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
                            UAnimSingleNodeInstance* Animation = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
                            if (!Require(Animation && Animation->IsPlaying() && Animation->IsLooping() && FMath::IsNearlyEqual(Animation->GetPlayRate(), PreviewAnimationRates[Index]), TEXT("The authored preview retains its playing looping state and original play rate while observed."))) return true;
                            const float Current = Animation->GetCurrentTime();
                            const float Previous = PreviewAnimationLastTimes[Index];
                            if (Current + KINDA_SMALL_NUMBER < Previous)
                            {
                                ++PreviewAnimationWraps[Index];
                                PreviewAnimationAdvance[Index] += PreviewAnimationLengths[Index] - Previous + Current;
                            }
                            else PreviewAnimationAdvance[Index] += FMath::Max(0.f, Current - Previous);
                            PreviewAnimationLastTimes[Index] = Current;
                            bWholeCycles &= PreviewAnimationWraps[Index] > 0 && PreviewAnimationAdvance[Index] + KINDA_SMALL_NUMBER >= PreviewAnimationLengths[Index] && World->GetTimeSeconds() - PreviewWorldStart + KINDA_SMALL_NUMBER >= PreviewAnimationLengths[Index] / PreviewAnimationRates[Index];
                        }
                        if (!bWholeCycles) return false;
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            LogPreview(Menu, Index, TEXT("end"));
                            if (!Require(PreviewAnimationWraps[Index] > 0 && PreviewAnimationAdvance[Index] + KINDA_SMALL_NUMBER >= PreviewAnimationLengths[Index], TEXT("Idle animation advances through at least one whole cycle and its loop boundary in the actual preview world."))) return true;
                        }
                    }
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Delete"), Index))))->OnClicked.Broadcast();
                        Test->TestNull(TEXT("Deleting the slot removes its animated preview actor."), Menu->GetPreviewStage()->GetPreviewActorForSlot(Index));
                    }
                    PreviewCaptureStage = 3;
                }
                if (!bBodyDraftPrepared)
                {
                    CreateButton->OnClicked.Broadcast();
                    if (!VerifyCreatedSlot(Test, Creation, 0)) return true;
                    bBodyDraftPrepared = true;
                }
                if (!ReviewBodyControls(Creation, Menu, World)) return bBodyReviewFailed;
                UButton* Edit = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Edit")));
                UButton* Info = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_ClassInfo")));
                UEditableTextBox* NameInput = Cast<UEditableTextBox>(Creation->GetWidgetFromName(TEXT("ProfessionNameInput")));
                UComboBoxString* ClassSelect = Cast<UComboBoxString>(Creation->GetWidgetFromName(TEXT("ProfessionClassSelect")));
                if (!Require(Edit && Info && NameInput && ClassSelect, TEXT("Slot detail controls exist.")))
                {
                    return true;
                }
                if (!bFlowOnly)
                {
                    UCameraComponent* Camera = Menu->GetPreviewStage()->FindComponentByClass<UCameraComponent>();
                    if (!Require(Camera != nullptr, TEXT("The actual preview stage has its party camera before editing."))) return true;
                    PartyCameraBeforeDetails = Camera->GetComponentTransform();
                    PartyFOVBeforeDetails = Camera->FieldOfView;
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
            if (!bProfessionCaptured && !bFlowOnly)
            {
                if (!Capture(TEXT("00-ProfessionDetails.png"))) return true;
                bProfessionCaptured = true;
                ProfessionPanelTime = FPlatformTime::Seconds();
                return false;
            }
            if (!bProfessionDetailsClosed)
            {
                Creation->CloseSlotDetails();
                bProfessionDetailsClosed = true;
                if (!bFlowOnly)
                {
                    UCameraComponent* Camera = Menu->GetPreviewStage()->FindComponentByClass<UCameraComponent>();
                    if (!Require(Camera && Camera->GetComponentTransform().Equals(PartyCameraBeforeDetails, 0.1f) && FMath::IsNearlyEqual(Camera->FieldOfView, PartyFOVBeforeDetails, 0.01f), TEXT("Closing the actual detail screen restores the saved party camera transform and field of view."))) return true;
                    ResetPreviewCameraObservation();
                    return false;
                }
            }
            if (!bFlowOnly && !WaitForPreviewCamera(Menu, World)) return false;
            if (!bFlowOnly)
            {
                UCameraComponent* Camera = Menu->GetPreviewStage()->FindComponentByClass<UCameraComponent>();
                if (!Require(Camera && Menu->PlayerCameraManager->GetCameraLocation().Equals(Camera->GetComponentLocation(), 0.1f) && Menu->PlayerCameraManager->GetCameraRotation().Equals(Camera->GetComponentRotation(), 0.05f) && FMath::IsNearlyEqual(Menu->PlayerCameraManager->GetFOVAngle(), Camera->FieldOfView, 0.01f) && FMath::IsNearlyEqual(Camera->FieldOfView, PartyFOVBeforeDetails, 0.01f) && FMath::IsNearlyEqual(Camera->FieldOfView, 90.f, 0.01f), TEXT("The actual player camera cache converges to the current native overview camera and its original 90-degree overview field of view after detail closure and bounds refitting."))) return true;
                Test->AddInfo(FString::Printf(TEXT("Detail close camera convergence: original=%s current=%s actual=%s originalFOV=%.3f currentFOV=%.3f actualFOV=%.3f stableFrames=%d stableWorldSeconds=%.3f."), *PartyCameraBeforeDetails.GetLocation().ToString(), *Camera->GetComponentLocation().ToString(), *Menu->PlayerCameraManager->GetCameraLocation().ToString(), PartyFOVBeforeDetails, Camera->FieldOfView, Menu->PlayerCameraManager->GetFOVAngle(), PreviewCameraStableFrames, World->GetTimeSeconds() - PreviewCameraStableWorldStart));
                if (!ReviewPartyBounds(Menu, World, FIntPoint(1280, 720), TEXT("details-closed")) || !Capture(TEXT("00-PartyAfterDetails.png"))) return true;
            }
            for (int32 Index = 1; Index < 4; ++Index)
            {
                Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Delete"), Index))))->OnClicked.Broadcast();
                if (!Require(Menu->GetPreviewStage()->GetPreviewActorForSlot(Index) == nullptr, TEXT("The restored overview fixture removes its extra created slots through actual delete buttons."))) return true;
            }
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
                    if (!VerifyCreatedSlot(Test, Draft, Index)) return false;
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

    // Find the real detail actions by their visible labels inside the actual editor panel.
    // 실제 편집 패널 안의 표시된 라벨로 상세 행동 버튼을 찾습니다.
    UButton* FindDetailButton(UCharacterCreationWidget* Creation, const FString& Label)
    {
        UWidget* Panel = Creation->GetWidgetFromName(TEXT("ProfessionDetailPanel"));
        UButton* Result = nullptr;
        Creation->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            UButton* Button = Cast<UButton>(Widget);
            UTextBlock* Text = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
            if (!Text || Text->GetText().ToString() != Label) return;
            for (UWidget* Parent = Button->GetParent(); Parent; Parent = Parent->GetParent()) if (Parent == Panel) Result = Button;
        });
        return Result;
    }

    bool CheckStoredBody(UCharacterCreationWidget* Creation, FName ExpectedBodyId)
    {
        const FRunPartyMember Actual = Creation->GetPartyMembers()[0];
        FRunPartyMember Expected = BodyOriginalMember;
        Expected.Appearance.BodyId = ExpectedBodyId;
        Expected.CharacterName = Actual.CharacterName;
        const bool bStored = Actual.CharacterName.ToString() == BodyOriginalMember.CharacterName.ToString() && FRunPartyMember::StaticStruct()->CompareScriptStruct(&Expected, &Actual, 0);
        if (!Require(bStored, TEXT("A body edit changes only its saved body identifier and preserves the original name, class, outfit identifiers, control and complete remaining draft data."))) bBodyReviewFailed = true;
        return bStored;
    }

    bool CheckPreviewBody(UCharacterCreationWidget* Creation, AMainMenuPlayerController* Menu, FName BodyId, bool bDetailOpen)
    {
        FProfessionDefinition Profession;
        const bool bResolved = Creation->PartyDefinition && Creation->PartyDefinition->ResolveProfession(BodyOriginalMember.ClassId, Profession);
        const FCharacterAppearanceBodyVariant* Body = bResolved && Profession.AppearanceCatalog ? Profession.AppearanceCatalog->FindBodyVariant(BodyId) : nullptr;
        AActor* Actor = Menu->GetPreviewStage()->GetPreviewActorForSlot(0);
        USkeletalMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
        UCharacterAppearanceComponent* Appearance = Actor ? Actor->FindComponentByClass<UCharacterAppearanceComponent>() : nullptr;
        UWidget* Panel = Creation->GetWidgetFromName(TEXT("ProfessionDetailPanel"));
        UTextBlock* Name = Cast<UTextBlock>(Creation->GetWidgetFromName(TEXT("AppearanceBodyName")));
        const bool bMesh = Body && Mesh && Mesh->IsVisible() && !Actor->IsHidden() && Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetPathName() == Body->Mesh.ToSoftObjectPath().ToString() && Appearance && Profession.AppearanceCatalog->FindBodyVariant(Appearance->Selection.BodyId) == Body;
        const bool bUI = Panel && (bDetailOpen ? Panel->IsVisible() && Name && Name->GetText().ToString() == Body->DisplayName.ToString() : Panel->GetVisibility() == ESlateVisibility::Collapsed);
        if (!Require(bMesh && bUI, TEXT("The actual preview uses the catalog's original selected body mesh and the actual detail label or closed panel agrees with that body.")))
        {
            bBodyReviewFailed = true;
            return false;
        }
        Test->AddInfo(FString::Printf(TEXT("Body UI stage=%d selected=%s originalMesh=%s savedBody=%s detailOpen=%d."), BodyReviewStage, *BodyId.ToString(), *Mesh->GetSkeletalMeshAsset()->GetPathName(), *Creation->GetPartyMembers()[0].Appearance.BodyId.ToString(), bDetailOpen));
        return true;
    }

    // Release the routed review gesture and restore the cursor even when a later assertion fails.
    // 이후 검사가 실패해도 라우팅한 검수 제스처를 해제하고 커서를 복원합니다.
    void RestorePreviewPointer()
    {
        if (!FSlateApplication::IsInitialized()) return;
        FSlateApplication& Slate = FSlateApplication::Get();
        if (bPreviewPointerPressed)
        {
            const FVector2D Cursor = Slate.GetCursorPos();
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, Cursor, Cursor, TSet<FKey>(), EKeys::RightMouseButton, 0, FModifierKeysState()));
            bPreviewPointerPressed = false;
        }
        if (bPreviewCursorMoved) Slate.SetCursorPos(PreviousPreviewCursor);
        bPreviewCursorMoved = false;
    }

    // Route physical-style Slate pointer events through the actual preview backdrop; never call the rotation implementation directly.
    // 회전 구현을 직접 호출하지 않고 실제 프리뷰 배경으로 Slate 포인터 이벤트를 라우팅합니다.
    bool ReviewPreviewDrag(UCharacterCreationWidget* Creation, AMainMenuPlayerController* Menu)
    {
        if (PreviewDragStage >= 4) return true;
        FSlateApplication& Slate = FSlateApplication::Get();
        const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Creation->TakeWidget());
        UWidget* Panel = Creation->GetWidgetFromName(TEXT("ProfessionDetailPanel"));
        AActor* Actor = Menu->GetPreviewStage()->GetPreviewActorForSlot(0);
        const FGeometry& Geometry = Creation->GetCachedGeometry();
        const auto Fail = [this]() { bBodyReviewFailed = true; return false; };
        if (!Require(Window.IsValid() && Window->GetNativeWindow().IsValid() && Panel && Actor && !Geometry.GetLocalSize().IsNearlyZero(), TEXT("The actual body editor supplies a native Slate window, preview actor and hit-test geometry for RMB dragging."))) return Fail();
        const TSet<FKey> Pressed{EKeys::RightMouseButton};
        const TSet<FKey> Released;
        if (PreviewDragStage == 0)
        {
            if (!Require(!Slate.GetPressedMouseButtons().Contains(EKeys::RightMouseButton) && !Creation->HasMouseCaptureByUser(0), TEXT("The isolated preview drag starts with its RMB and widget capture released."))) return Fail();
            PreviousPreviewCursor = Slate.GetCursorPos();
            bPreviewCursorMoved = true;
            PreviewRotationBeforeDrag = Actor->GetActorQuat();
            const FVector2D LocalSize = Geometry.GetLocalSize();
            PreviewDragStart = Geometry.LocalToAbsolute(FVector2D(LocalSize.X * 0.25, LocalSize.Y * 0.5));
            PreviewDragEnd = Geometry.LocalToAbsolute(FVector2D(LocalSize.X * 0.25 + 80.0, LocalSize.Y * 0.5));
            if (!Require(Geometry.IsUnderLocation(PreviewDragStart) && Geometry.IsUnderLocation(PreviewDragEnd) && !Panel->GetCachedGeometry().IsUnderLocation(PreviewDragStart) && !Panel->GetCachedGeometry().IsUnderLocation(PreviewDragEnd), TEXT("The complete real RMB drag lies on the visible preview backdrop outside every detail form control."))) return Fail();
            Slate.SetCursorPos(PreviewDragStart);
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, PreviewDragStart, PreviousPreviewCursor, Released, EKeys::Invalid, 0, FModifierKeysState()));
            const FWidgetPath Path = Slate.LocateWindowUnderMouse(PreviewDragStart, Slate.GetInteractiveTopLevelWindows());
            if (!Require(Path.IsValid() && Path.ContainsWidget(&Creation->TakeWidget().Get()), TEXT("The physical-style pointer hit path routes through the actual character creation widget."))) return Fail();
            bPreviewPointerPressed = true;
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), FPointerEvent(FSlateApplication::CursorPointerIndex, PreviewDragStart, PreviewDragStart, Pressed, EKeys::RightMouseButton, 0, FModifierKeysState()));
            if (!Require(Creation->HasMouseCaptureByUser(0), TEXT("Actual Slate RMB down reaches NativeOnMouseButtonDown and captures the creation widget."))) return Fail();
        }
        else if (PreviewDragStage == 1)
        {
            if (!Require(Creation->HasMouseCaptureByUser(0), TEXT("The real RMB gesture retains capture before moving."))) return Fail();
            Slate.SetCursorPos(PreviewDragEnd);
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, PreviewDragEnd, PreviewDragStart, Pressed, EKeys::Invalid, 0, FModifierKeysState()));
            PreviewRotationAfterDrag = Actor->GetActorQuat();
            const double Delta = FMath::Abs(FRotator::NormalizeAxis(PreviewRotationAfterDrag.Rotator().Yaw - PreviewRotationBeforeDrag.Rotator().Yaw));
            if (!Require(Delta > 10.0 && Delta < 80.0 && Creation->HasMouseCaptureByUser(0) && CheckStoredBody(Creation, TEXT("Female")), TEXT("Actual captured Slate move rotates the pending body while leaving the saved female party draft unchanged."))) return Fail();
            Test->AddInfo(FString::Printf(TEXT("Actual Slate RMB preview drag: start=%s end=%s yawBefore=%.3f yawAfter=%.3f delta=%.3f capture=%d."), *PreviewDragStart.ToString(), *PreviewDragEnd.ToString(), PreviewRotationBeforeDrag.Rotator().Yaw, PreviewRotationAfterDrag.Rotator().Yaw, Delta, Creation->HasMouseCaptureByUser(0)));
        }
        else if (PreviewDragStage == 2)
        {
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, PreviewDragEnd, PreviewDragEnd, Released, EKeys::RightMouseButton, 0, FModifierKeysState()));
            bPreviewPointerPressed = false;
            if (!Require(!Creation->HasMouseCaptureByUser(0) && !Slate.GetPressedMouseButtons().Contains(EKeys::RightMouseButton), TEXT("Actual Slate RMB up reaches NativeOnMouseButtonUp and releases both widget capture and pressed-button state."))) return Fail();
            Slate.SetCursorPos(PreviewDragStart);
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, PreviewDragStart, PreviewDragEnd, Released, EKeys::Invalid, 0, FModifierKeysState()));
            if (!Require(Actor->GetActorQuat().Equals(PreviewRotationAfterDrag, 0.001), TEXT("A real pointer move after RMB release cannot continue rotating the preview."))) return Fail();
        }
        else if (PreviewDragStage == 3)
        {
            bPreviewPointerPressed = true;
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), FPointerEvent(FSlateApplication::CursorPointerIndex, PreviewDragStart, PreviewDragStart, Pressed, EKeys::RightMouseButton, 0, FModifierKeysState()));
            if (!Require(Creation->HasMouseCaptureByUser(0), TEXT("A second actual RMB down captures the editor so cancel must release an active gesture."))) return Fail();
        }
        ++PreviewDragStage;
        return PreviewDragStage >= 4;
    }

    // Drive real body selector, save and cancel delegates while observing pending previews and the stored party draft separately.
    // 실제 몸체 선택·저장·취소 델리게이트를 사용하고 대기 중 프리뷰와 저장된 파티 초안을 각각 관찰합니다.
    bool ReviewBodyControls(UCharacterCreationWidget* Creation, AMainMenuPlayerController* Menu, UWorld* World)
    {
        if (BodyReviewStage >= 7) return true;
        UButton* Edit = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Edit")));
        UButton* Next = Cast<UButton>(Creation->GetWidgetFromName(TEXT("AppearanceBodyNext")));
        UButton* Previous = Cast<UButton>(Creation->GetWidgetFromName(TEXT("AppearanceBodyPrevious")));
        UButton* Save = FindDetailButton(Creation, TEXT("저장"));
        UButton* Cancel = FindDetailButton(Creation, TEXT("닫기 / 취소"));
        const auto Fail = [this]() { bBodyReviewFailed = true; return false; };
        if (!Require(Edit && Next && Previous && Save && Cancel, TEXT("The real body selector and detail save/cancel actions are present in the saved character creation UI."))) return Fail();
        if (BodyReviewStage == 0)
        {
            BodyOriginalMember = Creation->GetPartyMembers()[0];
            FProfessionDefinition Profession;
            if (!Require(Creation->PartyDefinition && Creation->PartyDefinition->ResolveProfession(BodyOriginalMember.ClassId, Profession) && Profession.AppearanceCatalog && Profession.AppearanceCatalog->DefaultBodyId == FName(TEXT("Male")) && Profession.AppearanceCatalog->FindBodyVariant(TEXT("Female")), TEXT("The current profession resolves its actual default male and selectable female catalog entries."))) return Fail();
            Edit->OnClicked.Broadcast();
            if (!Require(Next->GetIsEnabled() && Previous->GetIsEnabled(), TEXT("Both actual body selector arrows are enabled while editing."))) return Fail();
            Next->OnClicked.Broadcast();
            if (!CheckStoredBody(Creation, BodyOriginalMember.Appearance.BodyId) || !CheckPreviewBody(Creation, Menu, TEXT("Female"), true)) return Fail();
        }
        else
        {
            if (!bFlowOnly && !WaitForPreviewCamera(Menu, World)) return false;
            if (BodyReviewStage == 1)
            {
                if (!CheckPreviewBody(Creation, Menu, TEXT("Female"), true) || (!bFlowOnly && !Capture(TEXT("00-BodyFemaleDraft.png")))) return Fail();
                Save->OnClicked.Broadcast();
                if (!CheckStoredBody(Creation, TEXT("Female")) || !CheckPreviewBody(Creation, Menu, TEXT("Female"), false)) return Fail();
                SavedFemalePreviewRotation = Menu->GetPreviewStage()->GetPreviewActorForSlot(0)->GetActorQuat();
                Test->AddInfo(FString::Printf(TEXT("Saved female preview original facing: %s."), *SavedFemalePreviewRotation.Rotator().ToString()));
            }
            else if (BodyReviewStage == 2)
            {
                Edit->OnClicked.Broadcast();
                if (!CheckPreviewBody(Creation, Menu, TEXT("Female"), true)) return Fail();
                if (!Require(Menu->GetPreviewStage()->GetPreviewActorForSlot(0)->GetActorQuat().Equals(SavedFemalePreviewRotation, 0.001), TEXT("Opening the saved female editor retains that body's original facing before switching to a pending male body."))) return Fail();
                Previous->OnClicked.Broadcast();
                if (!CheckStoredBody(Creation, TEXT("Female")) || !CheckPreviewBody(Creation, Menu, TEXT("Male"), true)) return Fail();
            }
            else if (BodyReviewStage == 3)
            {
                if (!bFlowOnly && !ReviewPreviewDrag(Creation, Menu)) return false;
                if (!CheckPreviewBody(Creation, Menu, TEXT("Male"), true) || (!bFlowOnly && !Capture(TEXT("00-BodyMaleCancelledDraft.png")))) return Fail();
                Cancel->OnClicked.Broadcast();
                if (!CheckStoredBody(Creation, TEXT("Female")) || !CheckPreviewBody(Creation, Menu, TEXT("Female"), false)) return Fail();
                if (!bFlowOnly)
                {
                    AActor* Actor = Menu->GetPreviewStage()->GetPreviewActorForSlot(0);
                    Test->AddInfo(FString::Printf(TEXT("Actual Cancel after captured RMB: capture=%d RMBPressed=%d actor=%s actualFacing=%s savedFemaleFacing=%s pendingMaleFacing=%s femaleErrorDegrees=%.3f."), Creation->HasMouseCaptureByUser(0), FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::RightMouseButton), *GetNameSafe(Actor), Actor ? *Actor->GetActorRotation().ToString() : TEXT("missing"), *SavedFemalePreviewRotation.Rotator().ToString(), *PreviewRotationBeforeDrag.Rotator().ToString(), Actor ? FMath::RadiansToDegrees(Actor->GetActorQuat().AngularDistance(SavedFemalePreviewRotation)) : -1.0));
                    if (!Require(!Creation->HasMouseCaptureByUser(0), TEXT("The actual Cancel button releases the active character editor RMB capture."))) return Fail();
                    if (!Require(Actor && Actor->GetActorQuat().Equals(SavedFemalePreviewRotation, 0.001), TEXT("The actual Cancel button restores the saved female body's own original facing after a pending male drag."))) return Fail();
                    RestorePreviewPointer();
                }
            }
            else if (BodyReviewStage == 4)
            {
                if (!CheckPreviewBody(Creation, Menu, TEXT("Female"), false) || (!bFlowOnly && !Capture(TEXT("00-BodyFemaleAfterCancel.png")))) return Fail();
                Edit->OnClicked.Broadcast();
                if (!CheckPreviewBody(Creation, Menu, TEXT("Female"), true)) return Fail();
                Previous->OnClicked.Broadcast();
                if (!CheckPreviewBody(Creation, Menu, TEXT("Male"), true)) return Fail();
                Save->OnClicked.Broadcast();
                if (!CheckStoredBody(Creation, TEXT("Male")) || !CheckPreviewBody(Creation, Menu, TEXT("Male"), false)) return Fail();
            }
            else if (BodyReviewStage == 5)
            {
                for (int32 Index = 1; Index < 4; ++Index)
                {
                    UButton* Create = Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index))));
                    if (!Require(Create && Create->GetIsEnabled(), TEXT("Actual create buttons prepare the other three default party slots for detail-close camera review."))) return Fail();
                    Create->OnClicked.Broadcast();
                    if (!VerifyCreatedSlot(Test, Creation, Index)) return Fail();
                }
            }
            else if (BodyReviewStage == 6)
            {
                if (!CheckStoredBody(Creation, TEXT("Male")) || !CheckPreviewBody(Creation, Menu, TEXT("Male"), false) || (!bFlowOnly && (!ReviewPartyBounds(Menu, World, FIntPoint(1280, 720), TEXT("body-default-restored")) || !Capture(TEXT("00-BodyMaleRestored.png"))))) return Fail();
                Test->AddInfo(bFlowOnly ? TEXT("Actual body arrows, original Female/Male mesh application, save, cancel and complete draft preservation are verified; FlowOnly skips rendered preview RMB input.") : TEXT("Actual body arrows, original Female/Male mesh application, save, cancel, complete draft preservation and routed Slate RMB down/move/up, capture release and cancel-facing restoration are verified."));
            }
        }
        ++BodyReviewStage;
        ResetPreviewCameraObservation();
        return false;
    }

    void ResetPreviewCameraObservation()
    {
        bPreviewCameraObserved = false;
        PreviewCameraStableFrames = 0;
        PreviewCameraStableWorldStart = 0.f;
    }

    // Observe the actual player camera after native window resizing instead of forcing preview state or manually ticking it.
    // 네이티브 창 크기 변경 후 프리뷰 상태나 Tick을 강제하지 않고 실제 플레이어 카메라를 관찰합니다.
    bool WaitForPreviewCamera(AMainMenuPlayerController* Menu, UWorld* World)
    {
        AMainMenuPreviewStage* Preview = Menu ? Menu->GetPreviewStage() : nullptr;
        UCameraComponent* Camera = Preview ? Preview->FindComponentByClass<UCameraComponent>() : nullptr;
        APlayerCameraManager* Manager = Menu ? Menu->PlayerCameraManager : nullptr;
        if (!Camera || !Manager || Menu->GetViewTarget() != Preview) return false;
        const FVector Location = Manager->GetCameraLocation();
        const FRotator Rotation = Manager->GetCameraRotation();
        const float FOV = Manager->GetFOVAngle();
        const bool bMatchesComponent = Location.Equals(Camera->GetComponentLocation(), 0.1f) && Rotation.Equals(Camera->GetComponentRotation(), 0.05f) && FMath::IsNearlyEqual(FOV, Camera->FieldOfView, 0.01f);
        const bool bUnchanged = bPreviewCameraObserved && Location.Equals(PreviousPreviewCameraLocation, 0.1f) && Rotation.Equals(PreviousPreviewCameraRotation, 0.05f) && FMath::IsNearlyEqual(FOV, PreviousPreviewCameraFOV, 0.01f);
        if (!bMatchesComponent || !bUnchanged)
        {
            PreviewCameraStableFrames = 0;
            PreviewCameraStableWorldStart = World->GetTimeSeconds();
        }
        else ++PreviewCameraStableFrames;
        bPreviewCameraObserved = true;
        PreviousPreviewCameraLocation = Location;
        PreviousPreviewCameraRotation = Rotation;
        PreviousPreviewCameraFOV = FOV;
        return bMatchesComponent && PreviewCameraStableFrames >= 5 && World->GetTimeSeconds() - PreviewCameraStableWorldStart >= 0.15f;
    }

    bool ReviewPreviewRatios(AMainMenuPlayerController* Menu, UWorld* World)
    {
        const FIntPoint Sizes[] = {FIntPoint(1280, 720), FIntPoint(1680, 720), FIntPoint(1024, 768), FIntPoint(1280, 720)};
        const TCHAR* Filenames[] = {TEXT("00-FourPreviews.png"), TEXT("00-FourPreviews-21x9.png"), TEXT("00-FourPreviews-4x3.png"), TEXT("00-FourPreviews-Restored16x9.png")};
        if (PreviewRatioIndex >= UE_ARRAY_COUNT(Sizes)) return true;
        const FIntPoint Size = Sizes[PreviewRatioIndex];
        if (!ViewportPreparation.Update(Test, World, Size) || !WaitForPreviewCamera(Menu, World)) return false;
        if (!ReviewPartyBounds(Menu, World, Size, TEXT("ratio")))
        {
            bPreviewGeometryFailed = true;
            return false;
        }
        const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VerticalSliceScreenshots"), Filenames[PreviewRatioIndex]));
        if (!CaptureGameplayUI(Test, World, Filename, Size))
        {
            bPreviewGeometryFailed = true;
            return false;
        }
        ++PreviewRatioIndex;
        ResetPreviewCameraObservation();
        return false;
    }

    bool ReviewPartyBounds(AMainMenuPlayerController* Menu, UWorld* World, FIntPoint Size, const TCHAR* Phase)
    {
        for (int32 Index = 0; Index < 4; ++Index)
        {
            AActor* Actor = Menu->GetPreviewStage()->GetPreviewActorForSlot(Index);
            USkeletalMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
            if (!Require(Mesh && Mesh->IsVisible() && !Actor->IsHidden() && !Mesh->Bounds.BoxExtent.IsNearlyZero(), TEXT("Every original party preview has actual visible skeletal mesh bounds during ratio review.")))
            {
                bPreviewGeometryFailed = true;
                return false;
            }
            double MinX = TNumericLimits<double>::Max();
            double MaxX = -TNumericLimits<double>::Max();
            double MinY = TNumericLimits<double>::Max();
            double MaxY = -TNumericLimits<double>::Max();
            bool bAllInside = true;
            for (int32 Corner = 0; Corner < 8; ++Corner)
            {
                const FVector Point = Mesh->Bounds.Origin + Mesh->Bounds.BoxExtent * FVector((Corner & 1) ? 1.0 : -1.0, (Corner & 2) ? 1.0 : -1.0, (Corner & 4) ? 1.0 : -1.0);
                FVector2D Pixel;
                const bool bProjected = Menu->ProjectWorldLocationToScreen(Point, Pixel, true);
                bAllInside &= bProjected && Pixel.X >= 0.0 && Pixel.X <= Size.X;
                if (bProjected)
                {
                    MinX = FMath::Min(MinX, Pixel.X);
                    MaxX = FMath::Max(MaxX, Pixel.X);
                    MinY = FMath::Min(MinY, Pixel.Y);
                    MaxY = FMath::Max(MaxY, Pixel.Y);
                }
            }
            Test->AddInfo(FString::Printf(TEXT("Party preview phase=%s ratio=%dx%d slot=%d mesh=%s projectedX=%.2f..%.2f projectedY=%.2f..%.2f camera=%s stableFrames=%d stableWorldSeconds=%.3f."), Phase, Size.X, Size.Y, Index, *Mesh->GetName(), MinX, MaxX, MinY, MaxY, *Menu->PlayerCameraManager->GetCameraLocation().ToString(), PreviewCameraStableFrames, World->GetTimeSeconds() - PreviewCameraStableWorldStart));
            if (!Require(bAllInside, TEXT("All eight corners of each actual party mesh remain horizontally inside the requested physical viewport after camera convergence.")))
            {
                bPreviewGeometryFailed = true;
                return false;
            }
        }
        return true;
    }

    bool Capture(const TCHAR* FileName)
    {
        return CaptureGameplayUI(Test, GEditor->PlayWorld, FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VerticalSliceScreenshots"), FileName)), FIntPoint(1280, 720));
    }

    void LogPreview(AMainMenuPlayerController* Menu, int32 Index, const TCHAR* Phase)
    {
        AActor* Actor = Menu && Menu->GetPreviewStage() ? Menu->GetPreviewStage()->GetPreviewActorForSlot(Index) : nullptr;
        USkeletalMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
        UAnimSingleNodeInstance* Animation = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
        UWorld* World = Menu ? Menu->GetWorld() : nullptr;
        Test->AddInfo(FString::Printf(TEXT("Preview %s slot=%d world=%.3f elapsed=%.3f start=%.3f time=%.3f length=%.3f rate=%.3f observedAdvance=%.3f wraps=%d playing=%d looping=%d meshTick=%d registeredTick=%d visible=%d actorHidden=%d pauseAnims=%d visibilityTick=%d animScale=%.3f worldPaused=%d."), Phase, Index, World ? World->GetTimeSeconds() : -1.f, World ? World->GetTimeSeconds() - PreviewWorldStart : -1.f, PreviewAnimationStartTimes[Index], Animation ? Animation->GetCurrentTime() : -1.f, Animation ? Animation->GetLength() : -1.f, Animation ? Animation->GetPlayRate() : -1.f, PreviewAnimationAdvance[Index], PreviewAnimationWraps[Index], Animation && Animation->IsPlaying(), Animation && Animation->IsLooping(), Mesh && Mesh->IsComponentTickEnabled(), Mesh && Mesh->PrimaryComponentTick.IsTickFunctionRegistered(), Mesh && Mesh->IsVisible(), Actor && Actor->IsHidden(), Mesh && Mesh->bPauseAnims, Mesh ? static_cast<int32>(Mesh->VisibilityBasedAnimTickOption) : -1, Mesh ? Mesh->GlobalAnimRateScale : -1.f, World && World->IsPaused()));
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    double StageStarted;
    bool bFlowOnly = false;
    bool bProfessionPanelTested = false;
    int32 PreviewCaptureStage = 0;
    bool bProfessionCaptured = false;
    bool bProfessionDetailsClosed = false;
    bool bBodyDraftPrepared = false;
    bool bBodyReviewFailed = false;
    int32 BodyReviewStage = 0;
    FRunPartyMember BodyOriginalMember;
    int32 PreviewDragStage = 0;
    bool bPreviewPointerPressed = false;
    bool bPreviewCursorMoved = false;
    FVector2D PreviousPreviewCursor = FVector2D::ZeroVector;
    FVector2D PreviewDragStart = FVector2D::ZeroVector;
    FVector2D PreviewDragEnd = FVector2D::ZeroVector;
    FQuat PreviewRotationBeforeDrag = FQuat::Identity;
    FQuat PreviewRotationAfterDrag = FQuat::Identity;
    FQuat SavedFemalePreviewRotation = FQuat::Identity;
    double ProfessionPanelTime = 0.0;
    float PreviewAnimationLengths[4] = {};
    float PreviewAnimationRates[4] = {};
    float PreviewAnimationStartTimes[4] = {};
    float PreviewAnimationLastTimes[4] = {};
    float PreviewAnimationAdvance[4] = {};
    int32 PreviewAnimationWraps[4] = {};
    float PreviewWorldStart = 0.f;
    int32 PreviewRatioIndex = 0;
    bool bPreviewGeometryFailed = false;
    bool bPreviewCameraObserved = false;
    int32 PreviewCameraStableFrames = 0;
    float PreviewCameraStableWorldStart = 0.f;
    FVector PreviousPreviewCameraLocation = FVector::ZeroVector;
    FRotator PreviousPreviewCameraRotation = FRotator::ZeroRotator;
    float PreviousPreviewCameraFOV = 0.f;
    FTransform PartyCameraBeforeDetails;
    float PartyFOVBeforeDetails = 0.f;
    bool bRequestedPIE = false;
    FPIEViewportPreparation ViewportPreparation;
    TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
};

// Drive saved-menu buttons and send one Slate click through viewport hit testing and controller input.
// 저장된 메뉴 버튼을 사용하고 Slate 클릭 한 번을 뷰포트 히트 테스트와 컨트롤러 입력으로 전달합니다.
class FPlaySavedSkillLoadout : public IAutomationLatentCommand
{
public:
    FPlaySavedSkillLoadout(FAutomationTestBase* InTest, const TArray<FCombatRoundSkill>& InSkills, int32 InSkillIndex) : Test(InTest), Skills(InSkills), SkillIndex(InSkillIndex)
    {
    }

    virtual ~FPlaySavedSkillLoadout() override
    {
        if (ObservedTargetASC.IsValid()) ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UAS_Unit::GetHPAttribute()).Remove(HPChangedHandle);
        if (bReviewPointerPressed && FSlateApplication::IsInitialized()) FSlateApplication::Get().ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, ClickPosition, ClickPosition, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
        if (bMovedCursor && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(PreviousCursor);
    }

    virtual bool Update() override
    {
        if (StageStarted == 0.0) StageStarted = FPlatformTime::Seconds();
        if (!bRequestedPIE)
        {
            PlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            PlaySettings->SetPlayNetMode(PIE_Standalone);
            PlaySettings->SetPlayNumberOfClients(1);
            const FIntPoint Sizes[] = { FIntPoint(1280, 720), FIntPoint(1024, 768), FIntPoint(1600, 720), FIntPoint(1280, 800) };
            const FIntPoint ViewportSize = Sizes[SkillIndex % UE_ARRAY_COUNT(Sizes)];
            ExpectedViewportSize = ViewportSize;
            PlaySettings->NewWindowWidth = ViewportSize.X;
            PlaySettings->NewWindowHeight = ViewportSize.Y;
            PlaySettings->SetClientWindowSize(ViewportSize);
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = PlaySettings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/MainMenu");
            GEditor->RequestPlaySession(Params);
            bRequestedPIE = true;
            return false;
        }
        if (FPlatformTime::Seconds() - StageStarted > 60.0)
        {
            UWorld* DiagnosticWorld = GEditor->PlayWorld;
            AGameplayPlayerController* DiagnosticController = DiagnosticWorld ? Cast<AGameplayPlayerController>(DiagnosticWorld->GetFirstPlayerController()) : nullptr;
            URunStateSubsystem* DiagnosticRun = DiagnosticController && DiagnosticController->GetGameInstance() ? DiagnosticController->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
            ACombatRoundCoordinator* DiagnosticRound = DiagnosticController ? DiagnosticController->GetRoundCoordinator() : nullptr;
            Test->AddInfo(FString::Printf(TEXT("Saved skill timeout world=%.3f runPhase=%d roundPhase=%d sourceAlive=%d sourceHP=%.1f damageChanges=%d targetHP=%.1f->%.1f."), DiagnosticWorld ? DiagnosticWorld->GetTimeSeconds() : -1.f, DiagnosticRun ? static_cast<int32>(DiagnosticRun->GetPhase()) : -1, DiagnosticRound ? static_cast<int32>(DiagnosticRound->GetView().Phase) : -1, Source.IsValid() && Source->IsUnitAlive(), Source.IsValid() && Source->GetAttributeSet() ? Source->GetAttributeSet()->GetHP() : -1.f, HPChangeCount, InitialTargetHP, LowestTargetHP));
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
            if (!VerifyCreatedSlot(Test, Creation, 0)) return true;
            UButton* Control = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_PlayerControl")));
            UButton* Start = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_StartGame")));
            if (!Require(Control && Control->GetIsEnabled() && Start, TEXT("The created card exposes direct control and Start."))) return true;
            Control->OnClicked.Broadcast();
            const int32 CreatedCount = Creation->GetPartyMembers().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated; }).Num();
            const int32 ControlledCount = Creation->GetPartyMembers().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled; }).Num();
            if (!Require(CreatedCount == 1 && ControlledCount == 1 && Start->GetIsEnabled(), TEXT("Exactly one controlled character starts without AI companions."))) return true;
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
            if (!ViewportPreparation.Update(Test, World, ExpectedViewportSize)) return ViewportPreparation.HasFailed();
            // Preserve full saved-loadout execution coverage without giving new characters free shop skills.
            // 새 캐릭터에게 상점 스킬을 무료로 주지 않고 저장된 전체 장착의 실행 범위를 검사합니다.
            if (!bInstalledSavedLoadout)
            {
                const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
                TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
                if (!Require(Saved.IsValid() && Saved->Party.Num() >= 1, TEXT("A fresh Run has a durable party before preparing the acquired-loadout fixture."))) return true;
                FRunPartyMember* Member = Saved->Party.FindByPredicate([](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.bPlayerControlled; });
                if (!Require(Member && Member->bHasSkillLoadout && Member->Skills.Num() == 1 && Member->Gold == 10, TEXT("A new controlled character starts with one unarmed skill and ten gold."))) return true;
                Member->Skills.Reset();
                for (const FCombatRoundSkill& Skill : Skills)
                {
                    const FString Name = FPrimaryAssetId::FromString(Skill.SkillId.ToString()).PrimaryAssetName.ToString();
                    Member->Skills.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/%s.%s"), *Name, *Name)));
                }
                Member->Gold = 6;
                FText Error;
                if (!Require(UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0) && Run->LoadCheckpoint(Error), *FString::Printf(TEXT("The acquired-loadout fixture survives actual save and reload: %s"), *Error.ToString()))) return true;
                bInstalledSavedLoadout = true;
                return false;
            }
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
            if (!Require(Run->GetPhase() != ERunPhase::Defeat, TEXT("The disposable survival fixture preserves the player while its single authored attack resolves."))) return true;
            if (Round && Round->GetView().PendingProjectiles > 0) bSawProjectile = true;
            if (Source.IsValid() && Source->GetCurrentActionPoint() == InitialAP - Skills[SkillIndex].ActionPointCost) bSawAPCost = true;
            if (Source.IsValid() && Skills[SkillIndex].Kind == ECombatRoundSkillKind::Projectile && !Source->GetActorLocation().Equals(ActionOrigin, 0.1f)) bRangedStayedAtOrigin = false;
            const bool bSettled = Run->GetPhase() == ERunPhase::Result || (Round && (Round->GetView().Phase == ECombatRoundPhase::Finished || (Round->GetView().Phase == ECombatRoundPhase::Planning && Round->GetView().RoundNumber > InitialRound)));
            if (!bSettled) return false;
            const float ExpectedHP = FMath::Max(0.f, InitialTargetHP - Skills[SkillIndex].Power);
            Require(HPChangeCount == 1 && FMath::IsNearlyEqual(LowestTargetHP, ExpectedHP, 0.01f), TEXT("The actual target health changes once to the clamped expected value."));
            Require(bSawAPCost, TEXT("The selected skill consumes its authored AP cost."));
            Require(!Round || Round->GetView().PendingProjectiles == 0, TEXT("The round settles with no pending projectile."));
            if (Skills[SkillIndex].Kind == ECombatRoundSkillKind::Projectile) Require(bSawProjectile, TEXT("The ranged skill creates an actual in-flight round projectile."));
            if (Skills[SkillIndex].Kind == ECombatRoundSkillKind::Projectile) Require(bRangedStayedAtOrigin, TEXT("The ranged caster stays at its original position through cast, projectile flight and round completion."));
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
            if (!bPreparedSurvival)
            {
                // Isolate only fixture survival through live GAS; preserve target HP, skill power and action costs.
                // 실제 GAS로 테스트 생존만 격리하며 대상 HP, 스킬 위력과 행동 비용은 유지합니다.
                UAbilitySystemComponent* AbilitySystem = Source->GetAbilitySystemComponent();
                if (!Require(AbilitySystem != nullptr, TEXT("The actual controlled unit provides GAS attributes for a disposable survival fixture."))) return true;
                AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), 10000.f);
                AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 10000.f);
                FCombatCheckpointData Checkpoint;
                FText Error;
                if (!Require(Round->CapturePlanningCheckpoint(Checkpoint, Error), *FString::Printf(TEXT("The live survival fixture satisfies actual checkpoint validation: %s"), *Error.ToString()))) return true;
                bPreparedSurvival = true;
                return false;
            }
            const int32 AllyCount = Round->GetView().Units.FilterByPredicate([](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy; }).Num();
            if (!Require(AllyCount == 1 && Controlled->SkillIds.Num() == Skills.Num(), TEXT("The real encounter restores one warrior with the explicitly saved skills."))) return true;
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
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            FVector2D Pixel = FVector2D::ZeroVector;
            const FIntPoint ViewportSize = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
            const APlayerCameraManager* Camera = Controller->PlayerCameraManager;
            const bool bProjected = ViewportWidget.IsValid() && ViewportSize.X > 0 && ViewportSize.Y > 0 && Controller->ProjectWorldLocationToScreen(Target->Unit->GetActorLocation(), Pixel) && Pixel.X >= 4.f && Pixel.Y >= 4.f && Pixel.X < ViewportSize.X - 4.f && Pixel.Y < ViewportSize.Y - 4.f;
            const FVector CameraLocation = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
            const FRotator CameraRotation = Camera ? Camera->GetCameraRotation() : FRotator::ZeroRotator;
            const float CameraFOV = Camera ? Camera->GetFOVAngle() : 0.f;
            const ACombatArena* Arena = Round->GetArena();
            AActor* ViewTarget = Controller->GetViewTarget();
            const bool bArenaCamera = Camera && Arena && ViewTarget && ViewTarget->FindComponentByClass<UCameraComponent>() && (!IsValid(Arena->CameraAnchor) || ViewTarget == Arena->CameraAnchor);
            const bool bCameraStable = bArenaCamera && bProjected && bProjectionObserved && CameraLocation.Equals(PreviousCameraLocation, 0.25f) && CameraRotation.Equals(PreviousCameraRotation, 0.05f) && FMath::IsNearlyEqual(CameraFOV, PreviousCameraFOV, 0.01f) && Pixel.Equals(PreviousTargetPixel, 1.f);
            if (!bCameraStable)
            {
                ProjectionStableWorldTime = World->GetTimeSeconds();
                ProjectionStableFrames = 0;
            }
            else ++ProjectionStableFrames;
            bProjectionObserved = bProjected;
            PreviousCameraLocation = CameraLocation;
            PreviousCameraRotation = CameraRotation;
            PreviousCameraFOV = CameraFOV;
            PreviousTargetPixel = Pixel;
            UIReadiness += FString::Printf(TEXT("; projection=%d pixel=%s viewport=%dx%d camera=%s rotation=%s FOV=%.2f stableFrames=%d stableWorldSeconds=%.3f viewTarget=%s"), bProjected, *Pixel.ToString(), ViewportSize.X, ViewportSize.Y, *CameraLocation.ToString(), *CameraRotation.ToString(), CameraFOV, ProjectionStableFrames, World->GetTimeSeconds() - ProjectionStableWorldTime, *GetNameSafe(Controller->GetViewTarget()));
            if (!bCameraStable || ProjectionStableFrames < 3 || World->GetTimeSeconds() - ProjectionStableWorldTime < 0.15f) return false;
            FSlateApplication& Slate = FSlateApplication::Get();
            const FGeometry& Geometry = ViewportWidget->GetCachedGeometry();
            const FIntPoint Size = Viewport->Viewport->GetSizeXY();
            const FVector2D Cursor = Geometry.LocalToAbsolute(Pixel * Geometry.GetLocalSize() / FVector2D(Size));
            if (!bMovedCursor) PreviousCursor = Slate.GetCursorPos();
            bMovedCursor = true;
            Slate.SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
            Slate.SetCursorPos(Cursor);
            Viewport->Viewport->SetMouse(FMath::RoundToInt(Pixel.X), FMath::RoundToInt(Pixel.Y));
            const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(ViewportWidget.ToSharedRef());
            if (!Require(Window.IsValid(), TEXT("The game viewport has a Slate window for one real input click."))) return true;
            const TSet<FKey> Pressed = { EKeys::LeftMouseButton };
            const TSet<FKey> Released;
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, Cursor, PreviousCursor, Released, EKeys::Invalid, 0, FModifierKeysState()));
            const FWidgetPath CursorPath = Slate.LocateWindowUnderMouse(Cursor, Slate.GetInteractiveTopLevelWindows());
            // CommonUI blocks input during screen transitions; wait for the viewport before the single press.
            // CommonUI 화면 전환 중에는 입력이 차단되므로 클릭 한 번 전에 뷰포트 입력 준비를 기다립니다.
            if (!CursorPath.IsValid() || CursorPath.GetLastWidget() != ViewportWidget.ToSharedRef()) return false;
            ObservedTargetASC = Target->Unit->GetAbilitySystemComponent();
            HPChangedHandle = ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UAS_Unit::GetHPAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
            {
                if (Data.NewValue < Data.OldValue)
                {
                    LowestTargetHP = FMath::Min(LowestTargetHP, FMath::Max(0.f, Data.NewValue));
                    ++HPChangeCount;
                }
            });
            Test->AddInfo(FString::Printf(TEXT("Single click pixel=%s cursor=%s viewport=%s hit=%s"), *Pixel.ToString(), *Cursor.ToString(), *Geometry.GetLocalSize().ToString(), CursorPath.IsValid() ? *CursorPath.GetLastWidget()->GetTypeAsString() : TEXT("none")));
            bReviewPointerPressed = true;
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), FPointerEvent(FSlateApplication::CursorPointerIndex, Cursor, Cursor, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
            ClickPosition = Cursor;
            bMousePressed = true;
            Advance();
            return false;
        }
        if (Stage == 5)
        {
            if (bMousePressed)
            {
                const TSet<FKey> Released;
                FSlateApplication::Get().ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, ClickPosition, ClickPosition, Released, EKeys::LeftMouseButton, 0, FModifierKeysState()));
                bReviewPointerPressed = false;
                bMousePressed = false;
                return false;
            }
            if (FPlatformTime::Seconds() - StageStarted < 0.25) return false;
            TArray<UWidget*> Widgets;
            Planning->WidgetTree->GetAllWidgets(Widgets);
            TArray<UCombatRoundSkillButton*> Buttons;
            for (UWidget* Widget : Widgets)
            {
                if (UCombatRoundSkillButton* Button = Cast<UCombatRoundSkillButton>(Widget)) Buttons.Add(Button);
            }
            if (!Require(Buttons.Num() == Skills.Num(), TEXT("Selecting the enemy exposes each saved skill button."))) return true;
            for (const FCombatRoundSkill& Skill : Skills)
            {
                UCombatRoundSkillButton** Match = Buttons.FindByPredicate([&Skill](const UCombatRoundSkillButton* Button) { return Button->GetSkillId() == Skill.SkillId; });
                const FString State = FString::Printf(TEXT("Saved skill button %s is displayed and enabled for enemy %d: %s"), *Skill.SkillId.ToString(), TargetId, *DescribeWidget(Match ? *Match : nullptr));
                if (!Require(Match && IsDisplayed(*Match) && (*Match)->GetIsEnabled(), *State)) return true;
                const UTextBlock* Label = Cast<UTextBlock>((*Match)->GetContent());
                if (!Require(Label && Label->GetText().ToString().Contains(Skill.Name.ToString()), TEXT("The skill button displays the saved skill name."))) return true;
            }
            if (!CaptureGameplayUI(Test, World, FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SkillLoadoutScreenshots"), FString::Printf(TEXT("%d-SavedSkills.png"), SkillIndex + 1))), ExpectedViewportSize)) return true;
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
                ActionOrigin = Source->GetActorLocation();
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
    bool bInstalledSavedLoadout = false;
    bool bPreparedSurvival = false;
    bool bProjectionObserved = false;
    int32 ProjectionStableFrames = 0;
    float ProjectionStableWorldTime = 0.f;
    FVector PreviousCameraLocation = FVector::ZeroVector;
    FRotator PreviousCameraRotation = FRotator::ZeroRotator;
    float PreviousCameraFOV = 0.f;
    FVector2D PreviousTargetPixel = FVector2D::ZeroVector;
    int32 Stage = 0;
    double StageStarted = 0.0;
    FString UIReadiness;
    int32 SourceId = INDEX_NONE;
    int32 TargetId = INDEX_NONE;
    FIntPoint TargetCoord = FIntPoint::ZeroValue;
    int32 PlanIndex = 0;
    bool bAwaitingPlan = false;
    bool bReviewPointerPressed = false;
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
    bool bRangedStayedAtOrigin = true;
    FVector ActionOrigin;
    bool bMovedCursor = false;
    FVector2D PreviousCursor;
    FVector2D ClickPosition;
    bool bMousePressed = false;
    bool bRequestedPIE = false;
    FPIEViewportPreparation ViewportPreparation;
    FIntPoint ExpectedViewportSize = FIntPoint::ZeroValue;
    TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
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
    const TArray<FString> AssetNames = {TEXT("BPDA_DefaulatAttack"), TEXT("BPDA_swoard_attack")};
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
    AddInfo(TEXT("Runs two saved-menu/encounter PIE sessions; enemy selection uses one Slate mouse press/release through the viewport and controller, skill/menu buttons use their delegates."));
    for (int32 Index = 0; Index < Skills.Num(); ++Index)
    {
        ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/MainMenu")));
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectAVerticalSliceTests::FPlaySavedSkillLoadout>(this, Skills, Index));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    }
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectAVerticalSliceTests::FCleanupSkillLoadoutSave>(this, Slot));
    return true;
}

#endif
