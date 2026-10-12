#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunStateSubsystem.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "RunEncounterPIEHelpers.h"
#include "Tests/AutomationEditorCommon.h"
#include "TodoReviewWindowPlacement.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/EncyclopediaWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace UiLayoutScreenshots
{
    template <typename T>
    T* Screen(UWorld* World)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (Widget->GetOwningPlayer() == World->GetFirstPlayerController() && Cast<T>(Widget)->IsActivated()) return Cast<T>(Widget);
        return nullptr;
    }

    // Walk the real menus and first Run screens in one language and capture each layout once for visual review.
    // 한 언어로 실제 메뉴와 첫 Run 화면을 진행하며 각 레이아웃을 한 번씩 캡처해 시각 검수에 사용합니다.
    class FReview : public IAutomationLatentCommand
    {
    public:
        FReview(FAutomationTestBase* InTest, FString InCulture, FString InPreviousCulture, FString InPreviousSetting, FString InSlot, FString InOutput) : Test(InTest), Culture(MoveTemp(InCulture)), PreviousCulture(MoveTemp(InPreviousCulture)), PreviousSetting(MoveTemp(InPreviousSetting)), Slot(MoveTemp(InSlot)), Output(MoveTemp(InOutput)) {}

        virtual ~FReview() override
        {
            ReleaseViewport();
            RestoreLanguage();
        }

        void RestoreLanguage()
        {
            GConfig->SetString(TEXT("Internationalization"), TEXT("Language"), *PreviousSetting, GGameUserSettingsIni);
            FTextLocalizationManager::Get().DisableGameLocalizationPreview();
            FInternationalization::Get().SetCurrentLanguageAndLocale(PreviousCulture);
        }

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Started == 0.0) Started = StageStarted = Now;
            if (Stage == 99)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.WorldType == EWorldType::PIE && Now - StageStarted < 30.0) return false;
                }
                ReleaseViewport();
                if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) UGameplayStatics::DeleteGameInSlot(Slot, 0);
                RestoreLanguage();
                return true;
            }
            if (Now - Started > 300.0 || Now - StageStarted > 90.0)
            {
                Check(false, FString::Printf(TEXT("UI layout review timed out at stage %d."), Stage));
                return End();
            }
            if (Stage == 0) return Start();
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World || !World->GetFirstPlayerController()) return false;
            RetainViewport(World);
            if (Stage == 1)
            {
                UMainMenuScreenWidget* Menu = Screen<UMainMenuScreenWidget>(World);
                if (!Menu || !PrepareViewport(World) || !Warm(World, 30, 1.f)) return false;
                if (!Capture(World, TEXT("01_MainMenu")) || !Click(Menu, TEXT("Button_Options"))) return End();
                Advance(2);
                return false;
            }
            if (Stage == 2)
            {
                UOptionsWidget* Options = Screen<UOptionsWidget>(World);
                if (!Options || !Warm(World, 10, 0.3f)) return false;
                if (!Capture(World, TEXT("02_Options"))) return End();
                Options->DeactivateWidget();
                Advance(3);
                return false;
            }
            if (Stage == 3)
            {
                UMainMenuScreenWidget* Menu = Screen<UMainMenuScreenWidget>(World);
                if (!Menu || !Warm(World, 5, 0.1f)) return false;
                if (!Click(Menu, TEXT("Button_Encyclopedia"))) return End();
                Advance(4);
                return false;
            }
            if (Stage == 4)
            {
                UEncyclopediaWidget* Encyclopedia = Screen<UEncyclopediaWidget>(World);
                if (!Encyclopedia || !Warm(World, 10, 0.3f)) return false;
                if (!Capture(World, TEXT("03_Encyclopedia"))) return End();
                Encyclopedia->DeactivateWidget();
                Advance(5);
                return false;
            }
            if (Stage == 5)
            {
                UMainMenuScreenWidget* Menu = Screen<UMainMenuScreenWidget>(World);
                if (!Menu || !Warm(World, 5, 0.1f)) return false;
                if (!Click(Menu, TEXT("Button_NewGame"))) return End();
                Advance(6);
                return false;
            }
            if (Stage == 6)
            {
                UGameModeSelectionWidget* Mode = Screen<UGameModeSelectionWidget>(World);
                if (!Mode || !Warm(World, 10, 0.3f)) return false;
                if (!Capture(World, TEXT("04_ModeSelect")) || !Click(Mode, TEXT("Button_SinglePlayer"))) return End();
                Advance(7);
                return false;
            }
            if (Stage == 7)
            {
                UCharacterCreationWidget* Creation = Screen<UCharacterCreationWidget>(World);
                if (!Creation) return false;
                if (!bCreated)
                {
                    for (int32 Index = 0; Index < 4; ++Index) if (!Click(Creation, FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index)))) return End();
                    if (!Click(Creation, TEXT("Button_Slot0_PlayerControl"))) return End();
                    bCreated = true;
                    ResetWarm();
                }
                if (!Warm(World, 30, 1.f)) return false;
                if (!Capture(World, TEXT("05_CharacterCreation")) || !Click(Creation, TEXT("Button_StartGame"))) return End();
                Advance(8);
                return false;
            }
            Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            if (!Controller.IsValid() || !Run || Run->GetPhase() == ERunPhase::None) return false;
            if (Stage == 9)
            {
                if (!Warm(World, 10, 0.3f)) return false;
                if (!Capture(World, TEXT("08_Inventory"))) return End();
                Controller->RequestToggleInventory();
                Advance(8);
                return false;
            }
            return AdvanceRun(World, Run);
        }

    private:
        bool AdvanceRun(UWorld* World, URunStateSubsystem* Run)
        {
            const ERunPhase Phase = Run->GetPhase();
            if (Phase == ERunPhase::EncounterChoice || Phase == ERunPhase::Shop)
            {
                URunEncounterWidget* Widget = RunEncounterPIE::FindScreen(Controller.Get());
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), Widget) || !Warm(World, 20, 0.6f)) return false;
                if (Phase == ERunPhase::EncounterChoice)
                {
                    if (!bCapturedChoice && !Capture(World, TEXT("06_EncounterChoice"))) return End();
                    bCapturedChoice = true;
                    const TArray<FRunEncounterOffer>& Offers = Run->GetEncounterProgress().Offers;
                    if (!Check(Offers.Num() > 0, TEXT("The encounter offers at least one branch."))) return End();
                    Controller->RequestSelectRunEncounter(Offers[0].EncounterId);
                }
                else
                {
                    if (!bCapturedShop)
                    {
                        if (!Capture(World, TEXT("07_Shop"))) return End();
                        bCapturedShop = true;
                        Controller->RequestToggleInventory();
                        Advance(9);
                        return false;
                    }
                    Controller->RequestLeaveRunEncounter();
                }
                ResetWarm();
                StageStarted = FPlatformTime::Seconds();
                return false;
            }
            if (Phase == ERunPhase::Map)
            {
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), RunEncounterPIE::FindActiveScreen<URunMapWidget>(Controller.Get())) || !Warm(World, 20, 0.6f)) return false;
                if (!bCapturedMap && !Capture(World, TEXT("09_DifficultyCards"))) return End();
                bCapturedMap = true;
                const FRunNodeDefinition* Next = Run->GetNodes().FindByPredicate([Run](const FRunNodeDefinition& Node) { return Run->CanStartNode(Node.NodeId); });
                if (!Check(Next != nullptr, TEXT("A PvE node is available."))) return End();
                Controller->RequestStartNode(Next->NodeId, RunPveDifficulty::GetMediumTag());
                ResetWarm();
                StageStarted = FPlatformTime::Seconds();
                return false;
            }
            if (Phase == ERunPhase::Combat)
            {
                const ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
                if (!Round || Round->GetView().Phase != ECombatRoundPhase::Planning || !Controller->IsRoundInputEnabled() || !Warm(World, 60, 2.f)) return false;
                if (!Capture(World, TEXT("10_Combat"))) return End();
                return End();
            }
            return false;
        }

        bool Check(bool bCondition, const FString& Message)
        {
            Test->TestTrue(*Message, bCondition);
            return bCondition;
        }

        bool Click(UUserWidget* Widget, FName ButtonName)
        {
            UButton* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(ButtonName)) : nullptr;
            if (!Check(Button && Button->GetIsEnabled(), TEXT("The menu button is available: ") + ButtonName.ToString())) return false;
            Button->OnClicked.Broadcast();
            return true;
        }

        bool Start()
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
            Advance(1);
            return false;
        }

        bool PrepareViewport(UWorld* World)
        {
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
            if (!Viewport || !Viewport->Viewport || !Window.IsValid()) return false;
            const FIntPoint Size = Viewport->Viewport->GetSizeXY();
            if (Size == FIntPoint(1280, 720)) return TodoReviewWindowPlacement::Ensure(Test, World);
            if (LastResizeFrame > 0 && GFrameCounter - LastResizeFrame < 10) return false;
            if (!Check(++ResizeAttempts <= 6 && Size.X > 0 && Size.Y > 0, TEXT("The review viewport converges to 1280x720."))) return false;
            Window->Resize(Window->GetClientSizeInScreen() + FVector2D(1280 - Size.X, 720 - Size.Y));
            LastResizeFrame = GFrameCounter;
            ResetWarm();
            return false;
        }

        bool Warm(UWorld* World, int32 Frames, float Seconds)
        {
            if (WarmFrame != GFrameCounter)
            {
                WarmFrame = GFrameCounter;
                if (WarmFrames++ == 0) WarmWorldTime = World->GetTimeSeconds();
            }
            return WarmFrames >= Frames && World->GetTimeSeconds() - WarmWorldTime >= Seconds;
        }

        void ResetWarm()
        {
            WarmFrame = MAX_uint64;
            WarmFrames = 0;
            WarmWorldTime = 0.f;
        }

        void Advance(int32 Next)
        {
            Stage = Next;
            StageStarted = FPlatformTime::Seconds();
            ResetWarm();
        }

        bool Capture(UWorld* World, const FString& Name)
        {
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            TArray<FColor> Pixels;
            FIntVector Size = FIntVector::ZeroValue;
            if (!Check(Widget.IsValid() && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size) && Size.X > 0 && Size.Y > 0 && Pixels.Num() == Size.X * Size.Y, TEXT("The gameplay viewport produces a screenshot with UI."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
            const FString Filename = Output / (Culture + TEXT("_") + Name + TEXT(".png"));
            if (!Check(!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *Filename), TEXT("The screenshot is written under the review directory."))) return false;
            Test->AddInfo(TEXT("UI layout screenshot: ") + Filename);
            return true;
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
            if (!RetainedViewport.IsValid()) return;
            FlushRenderingCommands();
            RetainedViewport.Reset();
        }

        bool End()
        {
            GEditor->RequestEndPlayMap();
            Advance(99);
            return false;
        }

        FAutomationTestBase* Test;
        FString Culture;
        FString PreviousCulture;
        FString PreviousSetting;
        FString Slot;
        FString Output;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TWeakObjectPtr<AGameplayPlayerController> Controller;
        TSharedPtr<ISlateViewport> RetainedViewport;
        int32 Stage = 0;
        int32 ResizeAttempts = 0;
        int32 WarmFrames = 0;
        uint64 LastResizeFrame = 0;
        uint64 WarmFrame = MAX_uint64;
        float WarmWorldTime = 0.f;
        double Started = 0.0;
        double StageStarted = 0.0;
        bool bCreated = false;
        bool bCapturedChoice = false;
        bool bCapturedShop = false;
        bool bCapturedMap = false;
    };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FUiLayoutScreenshotReview, "ProjectA.TodoReview.UiLayoutScreens", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FUiLayoutScreenshotReview::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    for (const FString& Culture : {FString(TEXT("ko")), FString(TEXT("en"))})
    {
        OutBeautifiedNames.Add(Culture);
        OutTestCommands.Add(Culture);
    }
}

bool FUiLayoutScreenshotReview::RunTest(const FString& Parameters)
{
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_UiLayout_");
    if (!TestTrue(TEXT("UI layout review requires explicit opt-in, left-monitor rendering and no current PIE."), GEditor && GEngine && !GEditor->PlayWorld && FApp::CanEverRender() && FSlateApplication::IsInitialized() && FParse::Param(FCommandLine::Get(), TEXT("ProjectAUiLayoutReview")) && FParse::Param(FCommandLine::Get(), TEXT("ProjectAReviewLeftMonitor")) && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))) return false;
    if (!TestTrue(TEXT("Only Korean and English are reviewed."), Parameters == TEXT("ko") || Parameters == TEXT("en"))) return false;
    if (!TestTrue(TEXT("A fresh owned save slot protects existing Runs."), Slot.StartsWith(Prefix) && !UGameplayStatics::DoesSaveGameExist(Slot, 0))) return false;
    FString OutputRoot;
    if (!TodoReviewWindowPlacement::OutputRoot(this, OutputRoot)) return false;
    const FString Output = OutputRoot / TEXT("UiLayout");
    IFileManager::Get().MakeDirectory(*Output, true);
    // Switch the language for this session only; the saved player setting is untouched and restored afterwards.
    // 이 세션에서만 언어를 바꾸며 저장된 플레이어 설정은 건드리지 않고 끝나면 복원합니다.
    const FString PreviousCulture = FInternationalization::Get().GetCurrentLanguage()->GetName();
    if (!TestTrue(TEXT("The review culture is available."), FInternationalization::Get().SetCurrentLanguageAndLocale(Parameters))) return false;
    // PIE shows native game text unless the editor game localization preview is enabled for the culture.
    // 에디터 PIE는 게임 현지화 미리보기를 켜야 해당 언어의 게임 문구를 표시합니다.
    FTextLocalizationManager::Get().EnableGameLocalizationPreview(Parameters);
    // The project language subsystem re-applies the user setting at PIE start, so override it in memory only (-nowrite).
    // 프로젝트 언어 서브시스템이 PIE 시작 시 사용자 설정을 다시 적용하므로 메모리에서만 바꿉니다(-nowrite).
    FString PreviousSetting;
    GConfig->GetString(TEXT("Internationalization"), TEXT("Language"), PreviousSetting, GGameUserSettingsIni);
    GConfig->SetString(TEXT("Internationalization"), TEXT("Language"), *Parameters, GGameUserSettingsIni);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<UiLayoutScreenshots::FReview>(this, Parameters, PreviousCulture, PreviousSetting, Slot, Output));
    return true;
}

#endif
