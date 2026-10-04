#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "Camera/CameraComponent.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GAS/Effect/GE_Damage.h"
#include "GAS/Effect/GE_Shield.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Scalability.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "TodoRenderReviewTestTypes.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Debug/CombatDebugWidget.h"
#include "Unit/UnitBase.h"
#include "Unit/PlayerUnit.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace TodoRenderReview
{
const TMap<FString, FString> EnvironmentMaps =
{
    {TEXT("DungeonFantasy"), TEXT("Dungeon/DungeonFantasy")},
    {TEXT("DungeonStone"), TEXT("Dungeon/DungeonStone")},
    {TEXT("MeadowBloom"), TEXT("Grassland/MeadowBloom")},
    {TEXT("PineRidge"), TEXT("Forest/PineRidge")},
    {TEXT("BambooGarden"), TEXT("Forest/BambooGarden")},
    {TEXT("CrimsonForest"), TEXT("Forest/CrimsonForest")},
    {TEXT("DesertCanyon"), TEXT("Desert/DesertCanyon")},
    {TEXT("DesertOasis"), TEXT("Desert/DesertOasis")},
    {TEXT("DarkMarsh"), TEXT("Forest/DarkMarsh")},
    {TEXT("SavannahGrove"), TEXT("Grassland/SavannahGrove")},
    {TEXT("FrozenPass"), TEXT("Ice/FrozenPass")},
    {TEXT("IceCitadel"), TEXT("Ice/IceCitadel")},
    {TEXT("PalmCoast"), TEXT("Summer/PalmCoast")},
    {TEXT("SunsetLagoon"), TEXT("Summer/SunsetLagoon")}
};

bool Prepare(FAutomationTestBase* Test, const FString& MapPath, FString& OutSlot)
{
    if (!GEditor || !GEngine || !FApp::CanEverRender() || !FSlateApplication::IsInitialized() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        Test->AddError(TEXT("TODO render review requires a rendering editor and Slate; do not use -nullrhi."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            Test->AddError(TEXT("Close the existing PIE session before TODO render review."));
            return false;
        }
    }
    OutSlot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_TodoReview_");
    bool bValidSuffix = OutSlot.Len() > Prefix.Len();
    for (TCHAR Character : OutSlot.Mid(Prefix.Len()))
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_')) bValidSuffix = false;
    }
    if (!OutSlot.StartsWith(Prefix) || !bValidSuffix || UGameplayStatics::DoesSaveGameExist(OutSlot, 0))
    {
        Test->AddError(TEXT("Use -ProjectASaveSlot=ProjectA_Automation_TodoReview_<fresh alphanumeric suffix>. Existing save slots are never touched."));
        return false;
    }
    FEditorLoadMap Load(MapPath);
    Load.Update();
    const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != MapPath)
    {
        Test->AddError(TEXT("The existing review map could not be loaded: ") + MapPath);
        return false;
    }
    const AWorldSettings* WorldSettings = EditorWorld ? EditorWorld->GetWorldSettings() : nullptr;
    if (!WorldSettings || !WorldSettings->DefaultGameMode || WorldSettings->DefaultGameMode->GetPathName() != TEXT("/Game/User_JeHoon/Blueprint/Game/BP_CombatDebugGameMode.BP_CombatDebugGameMode_C"))
    {
        Test->AddError(TEXT("The map must retain its saved BP_CombatDebugGameMode; review does not override or save map settings."));
        return false;
    }
    return true;
}

// Run the existing maps in disposable standalone PIE, then capture the gameplay camera with Slate UI.
// 기존 맵을 폐기 가능한 독립 PIE에서 실행한 뒤 Slate UI를 포함한 게임 카메라 화면을 캡처합니다.
class FReview : public IAutomationLatentCommand
{
public:
    FReview(FAutomationTestBase* InTest, FString InMapPath, FString InName, FString InSlot, bool bInHealth) : Test(InTest), MapPath(MoveTemp(InMapPath)), Name(MoveTemp(InName)), Slot(MoveTemp(InSlot)), bHealth(bInHealth)
    {
        bRenderOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));
        Test->AddInfo(FString::Printf(TEXT("Render review mode: render_offscreen=%d; native_window_position_regression=%d."), bRenderOffscreen, bHealth));
        OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TodoReview/Render") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    }

    virtual ~FReview() override
    {
        RestorePointer();
        RestoreReviewWindow();
        ReleaseRetainedViewport();
    }

    virtual bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (Stage == 99)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && FPlatformTime::Seconds() - Started < 30.0) return false;
                if (Context.WorldType == EWorldType::PIE) Test->AddError(TEXT("TODO review PIE did not close within 30 seconds."));
            }
            ReleaseRetainedViewport();
            Check(!UGameplayStatics::DoesSaveGameExist(Slot, 0), TEXT("The isolated debug review created no Run save."));
            return true;
        }
        if (FPlatformTime::Seconds() - Started > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("TODO review %s timed out at stage %d."), *Name, Stage));
            return End();
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
            Params.GlobalMapOverride = MapPath;
            GEditor->RequestPlaySession(Params);
            Advance(1);
            return false;
        }
        if (Stage == 1)
        {
            if (!Connect()) return false;
            if (!VerifyRoster()) return End();
            TArray<UUserWidget*> DebugWidgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller.Get(), DebugWidgets, UCombatDebugWidget::StaticClass(), false);
            bool bCollapsedDebug = false;
            for (UUserWidget* DebugWidget : DebugWidgets)
            {
                if (DebugWidget->GetOwningPlayer() != Controller.Get() || !DebugWidget->WidgetTree) continue;
                TArray<UWidget*> Children;
                DebugWidget->WidgetTree->GetAllWidgets(Children);
                for (UWidget* Child : Children)
                {
                    UButton* Button = Cast<UButton>(Child);
                    UTextBlock* Caption = Button ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr;
                    if (!Caption || Caption->GetText().ToString() != TEXT("디버그 도구 열기 / 접기")) continue;
                    // Collapse the real debug panel through its button so it cannot cover the reviewed units or HP labels.
                    // 검수할 유닛과 HP 표시를 가리지 않도록 실제 버튼 경로로 디버그 패널을 접습니다.
                    Button->OnClicked.Broadcast();
                    bCollapsedDebug = true;
                    break;
                }
            }
            if (!Check(bCollapsedDebug, TEXT("The actual debug toggle collapses its panel before gameplay screenshots."))) return End();
            UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
            if (!Check(Window.IsValid(), TEXT("The disposable PIE viewport owns a real Slate window."))) return End();
            Test->AddInfo(FString::Printf(TEXT("Review viewport before resize: %s; window client: %s; Slate: %s."), *Viewport->Viewport->GetSizeXY().ToString(), *Window->GetClientSizeInScreen().ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString()));
            // Restore the requested client size after Slate's initial work-area fitting, without changing user display settings.
            // 사용자 화면 설정을 바꾸지 않고 Slate의 초기 작업 영역 맞춤 이후 요청한 클라이언트 크기를 복원합니다.
            Window->Resize(FVector2D(1280, 720));
            Probe.Reset(CreateWidget<UTodoHealthPaintProbe>(Controller.Get()));
            if (!Check(Probe.IsValid(), TEXT("An editor-only probe owns the actual local PIE controller."))) return End();
            Advance(2);
            return false;
        }
        if (!Check(Controller.IsValid() && Round.IsValid() && Health.IsValid(), TEXT("The real PIE controller, coordinator and HP viewport widget remain available."))) return End();
        if (Stage == 2 || Stage == 20 || Stage == 21)
        {
            if (!Warm(30, 1.0)) return false;
            UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
            const FIntPoint ActualSize = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
            if (ActualSize != DesiredSize)
            {
                const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
                const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
                if (!Check(Window.IsValid() && ActualSize.X > 0 && ActualSize.Y > 0 && ResizeAttempts < 3, TEXT("The review viewport converges to its requested physical size after bounded Slate border correction."))) return End();
                // Correct the measured viewport difference because Slate window borders consume part of the client area.
                // Slate 창 테두리가 클라이언트 영역 일부를 사용하므로 측정된 뷰포트 크기 차이를 보정합니다.
                Window->Resize(Window->GetClientSizeInScreen() + FVector2D(DesiredSize.X - ActualSize.X, DesiredSize.Y - ActualSize.Y));
                ++ResizeAttempts;
                Advance(Stage);
                return false;
            }
            if (Stage == 21)
            {
                SampleQuality = Scalability::GetQualityLevels();
                static TOptional<Scalability::FQualityLevels> EnvironmentQuality;
                if (!EnvironmentQuality.IsSet()) EnvironmentQuality = SampleQuality;
                if (!Check(SampleQuality == EnvironmentQuality.GetValue(), TEXT("Every current map frame sample uses the same actual scalability values."))) return End();
                SampleLastPlatform = FPlatformTime::Seconds();
                SampleLastEngineFrame = GFrameCounter;
                SampleLastWorldTime = Controller->GetWorld()->GetTimeSeconds();
                Advance(22);
                return false;
            }
            FTodoHealthPaintObservation Alive = Observe();
            if (!Check(Alive.Labels.Num() == 5 && Alive.BoxCount == 15, TEXT("The actual gameplay geometry paints five HP labels, five backgrounds and ten health-bar boxes.")) || !VerifyHealthPlacement(Alive)) return End();
            if (!bHealth)
            {
                const FString Suffix = ScreenIndex == 0 ? TEXT("gameplay_ui_16x9") : ScreenIndex == 1 ? TEXT("gameplay_ui_21x9") : TEXT("gameplay_ui_4x3");
                if (!Capture(Suffix)) return End();
                if (ScreenIndex < 2)
                {
                    ++ScreenIndex;
                    DesiredSize = ScreenIndex == 1 ? FIntPoint(1680, 720) : FIntPoint(1024, 768);
                    const TSharedPtr<SViewport> Widget = Viewport->GetGameViewportWidget();
                    const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
                    if (!Check(Window.IsValid(), TEXT("Each aspect review retains its actual gameplay window."))) return End();
                    Window->Resize(FVector2D(DesiredSize.X, DesiredSize.Y));
                    ResizeAttempts = 0;
                    Advance(20);
                    return false;
                }
                // Warm the common comparison resolution after all image readbacks before observing frame intervals.
                // 모든 이미지 읽기 이후 공통 비교 해상도로 예열한 뒤 프레임 간격을 관찰합니다.
                DesiredSize = FIntPoint(1280, 720);
                const TSharedPtr<SViewport> Widget = Viewport->GetGameViewportWidget();
                const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
                if (!Check(Window.IsValid(), TEXT("The environment comparison restores its actual 1280x720 gameplay window."))) return End();
                Window->Resize(FVector2D(DesiredSize.X, DesiredSize.Y));
                ResizeAttempts = 0;
                Advance(21);
                return false;
            }
            FGameplayTagContainer Tags;
            Tags.AddTag(ProjectACombatTags::Skill_Effect_Shield);
            if (!Check(UCombatEffectLibrary::ApplyTaggedEffectToUnit(Ally.Get(), Ally.Get(), UGE_Shield::StaticClass(), 7.f, Tags), TEXT("A transient GAS shield enables a real shield-label paint observation."))) return End();
            Advance(3);
            return false;
        }
        if (Stage == 3)
        {
            if (!Warm(5, 0.1)) return false;
            const FTodoHealthPaintObservation Shielded = Observe();
            if (!Check(Shielded.Labels.Num() == 5 && Shielded.BoxCount == 15 && Shielded.Labels.ContainsByPredicate([](const FString& Label) { return Label.Contains(TEXT("보호막")); }), TEXT("The real NativePaint emits the shield text alongside the live HP and bar draws.")) || !VerifyHealthPlacement(Shielded) || !Capture(TEXT("alive_shield_ui"))) return End();
            const UAS_Unit* Attributes = Ally->GetAttributeSet();
            if (!Check(UCombatEffectLibrary::ApplyDamageToUnit(Enemy.Get(), Ally.Get(), UGE_Damage::StaticClass(), Attributes->GetHP() + Attributes->GetShield() + 1.f), TEXT("Actual GAS damage kills the shielded ally."))) return End();
            Advance(4);
            return false;
        }
        if (Stage == 4)
        {
            if (!Warm(5, 0.1)) return false;
            const FTodoHealthPaintObservation Dead = Observe();
            if (!Check(!Ally->IsUnitAlive() && Dead.Labels.Num() == 4 && Dead.BoxCount == 12 && !Dead.Labels.ContainsByPredicate([](const FString& Label) { return Label.Contains(TEXT("보호막")); }), TEXT("GAS death removes the ally's actual HP text, shield text, background and both bar draws.")) || !VerifyHealthPlacement(Dead) || !Capture(TEXT("dead_ui"))) return End();
            FText Error;
            if (!Check(Round->ReviveDebugUnit(Controller.Get(), AllyId, Error), TEXT("The public debug revival succeeds: ") + Error.ToString())) return End();
            Advance(5);
            return false;
        }
        if (Stage == 5)
        {
            if (!Warm(5, 0.1)) return false;
            const FTodoHealthPaintObservation Revived = Observe();
            if (!Check(Ally->IsUnitAlive() && Revived.Labels.Num() == 5 && Revived.BoxCount == 15 && !Revived.Labels.ContainsByPredicate([](const FString& Label) { return Label.Contains(TEXT("보호막")); }), TEXT("Debug revival restores the actual HP/background/bar output and clears the spent shield.")) || !VerifyHealthPlacement(Revived)) return End();
            UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
            int32 NativeX = 0;
            int32 NativeY = 0;
            int32 NativeWidth = 0;
            int32 NativeHeight = 0;
            if (!Check(Window.IsValid() && Window->GetNativeWindow().IsValid() && Window->GetNativeWindow()->GetRestoredDimensions(NativeX, NativeY, NativeWidth, NativeHeight), TEXT("The disposable review window reports its actual native position before the offset regression."))) return End();
            OriginalReviewWindowPosition = Window->GetPositionInScreen();
            OriginalNativePosition = FIntPoint(NativeX, NativeY);
            OriginalNativeWindowSize = FIntPoint(NativeWidth, NativeHeight);
            MovedReviewWindow = Window;
            // Change only the disposable PIE window position, then observe naturally repainted labels without modifying display preferences.
            // 화면 설정을 변경하지 않고 폐기 가능한 PIE 창 위치만 바꾼 뒤 자연스럽게 다시 그려진 표시를 관찰합니다.
            Window->MoveWindowTo(OriginalReviewWindowPosition + FVector2D(37.0, 29.0));
            Advance(23);
            return false;
        }
        if (Stage == 23)
        {
            if (!Warm(8, 0.2)) return false;
            const TSharedPtr<SWindow> Window = MovedReviewWindow.Pin();
            int32 NativeX = 0;
            int32 NativeY = 0;
            int32 NativeWidth = 0;
            int32 NativeHeight = 0;
            if (!Check(Window.IsValid() && Window->GetNativeWindow().IsValid() && Window->GetNativeWindow()->GetRestoredDimensions(NativeX, NativeY, NativeWidth, NativeHeight) && (FVector2D(NativeX, NativeY) - FVector2D(OriginalNativePosition)).Size() > 16.0 && FIntPoint(NativeWidth, NativeHeight) == OriginalNativeWindowSize, TEXT("A second actual native window position changes the desktop offset while retaining the original window size."))) return End();
            const FTodoHealthPaintObservation Moved = Observe();
            if (!Check(Moved.Labels.Num() == 5 && Moved.BoxCount == 15, TEXT("The moved review window retains five real live HP labels and their original draw counts.")) || !VerifyHealthPlacement(Moved) || !Capture(TEXT("revived_ui"))) return End();
            return End();
        }
        if (Stage == 10)
        {
            const bool bArrived = Ally->GetCurrentTile() == Destination.Get() && Destination->GetOccupyingUnit() == Ally.Get() && FVector::Dist2D(Ally->GetActorLocation(), Destination->GetActorLocation()) < 2.f;
            if (!bArrived)
            {
                if (!Check(Round->IsSAPMovementInProgress(), TEXT("SAP remains active until the owned unit reaches the reserved destination."))) return End();
                return false;
            }
            if (!Check(FVector::Dist2D(Ally->GetActorLocation(), InitialLocation) > 2.f && Ally->GetCurrentSubActionPoint() == InitialSAP - 1, TEXT("Real world frames move the owned character to the reserved tile and pay SAP once."))) return End();
            Test->AddInfo(TEXT("The public controller reservation and actual Slate LMB tile click both reach the authoritative SAP plan; actual world frames verify movement, one SAP charge and destination occupancy. Hardware mouse input and network replication are separate."));
            return End();
        }
        if (Stage == 11)
        {
            if (!Warm(3, 0.05)) return false;
            const FCombatRoundUnitView* Reserved = FindAllyView();
            if (!Check(Reserved && Reserved->bHasMovePlan && Reserved->MoveDestinationCoord == Destination->GridCoord && Ally->GetActorLocation().Equals(InitialLocation, 0.01f) && Ally->GetCurrentSubActionPoint() == InitialSAP, TEXT("The public reservation remains authoritative while the actual planning widget observes its normal refresh tick."))) return End();
            UButton* Cancel = FindPlanningButton(TEXT("이동 예약 취소"));
            UButton* Move = FindPlanningButton(TEXT("이동 예약 변경"));
            const bool bButtonsReady = Cancel && Cancel->GetIsEnabled() && Move && Move->GetIsEnabled();
            // The real widget refreshes on accumulated world delta every 0.1 seconds; external requests need a bounded observation wait.
            // 실제 위젯은 월드 시간 누적 0.1초마다 갱신하므로 외부 요청 이후 제한된 관찰 대기를 적용합니다.
            if (!bButtonsReady && FPlatformTime::Seconds() - Started < 2.0) return false;
            if (!Check(bButtonsReady, FString::Printf(TEXT("The real planning screen exposes enabled cancellation and reserved-destination buttons within two seconds; map=%s phase=%d pending=%d input=%d widgetActive=%d cancel=%s enabled=%d move=%s enabled=%d existingNewPlanButton=%s worldSinceReservation=%.3f elapsed=%.3f."), *Name, static_cast<int32>(Round->GetView().Phase), Controller->IsRoundRequestPending(), Controller->IsRoundInputEnabled(), Planning.IsValid() && Planning->IsActivated(), *GetNameSafe(Cancel), Cancel && Cancel->GetIsEnabled(), *GetNameSafe(Move), Move && Move->GetIsEnabled(), *GetNameSafe(FindPlanningButton(TEXT("이동 예약 · SAP 1"))), Controller->GetWorld()->GetTimeSeconds() - MovementReservedWorldTime, FPlatformTime::Seconds() - Started))) return End();
            Cancel->OnClicked.Broadcast();
            const FCombatRoundUnitView* Entry = FindAllyView();
            if (!Check(!Controller->IsRoundRequestPending() && Entry && !Entry->bHasMovePlan && Ally->GetActorLocation().Equals(InitialLocation, 0.01f) && Ally->GetCurrentSubActionPoint() == InitialSAP, TEXT("The actual UI cancel removes only the public move reservation without moving or charging SAP."))) return End();
            Move = FindPlanningButton(TEXT("이동 예약 · SAP 1"));
            if (!Check(Move && Move->GetIsEnabled(), TEXT("The actual canceled plan restores the enabled new destination-selection button."))) return End();
            Move->OnClicked.Broadcast();
            Advance(12);
            return false;
        }
        if (Stage == 12)
        {
            if (!Warm(3, 0.05)) return false;
            FVector2D Pixel;
            FVector2D Cursor;
            if (!Check(PositionPointer(Destination.Get(), ClickWorldPosition, Pixel, Cursor), TEXT("The actual destination remains on screen, uncovered by UI and is the real Visibility cursor-trace hit."))) return End();
            if (!Check(Controller->IsRoundInputEnabled() && Planning.IsValid() && Controller->OnRoundWorldTileClicked.IsBoundToObject(Planning.Get()), TEXT("The real planning widget owns the public tile-selection input delegate."))) return End();
            FSlateApplication& Slate = FSlateApplication::Get();
            const TSharedPtr<SViewport> Widget = Controller->GetWorld()->GetGameViewport()->GetGameViewportWidget();
            const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Widget.ToSharedRef());
            if (!Check(Window.IsValid() && Window->GetNativeWindow().IsValid() && !Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton), TEXT("The real destination click has a native window and begins with LMB released."))) return End();
            TileClickObserver = Controller->OnRoundWorldTileClicked.AddLambda([this](FIntPoint Coord) { ObservedClickCoord = Coord; ++ObservedTileClicks; });
            ClickCursor = Cursor;
            Test->AddInfo(FString::Printf(TEXT("Actual environment tile click: map=%s coord=%s projectedPixel=%s SlateCursor=%s tracedActor=%s."), *Name, *Destination->GridCoord.ToString(), *Pixel.ToString(), *Cursor.ToString(), *Destination->GetName()));
            bPointerPressed = true;
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), FPointerEvent(FSlateApplication::CursorPointerIndex, Cursor, Cursor, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
            Advance(13);
            return false;
        }
        if (Stage == 13)
        {
            FSlateApplication::Get().ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, ClickCursor, ClickCursor, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
            bPointerPressed = false;
            Advance(14);
            return false;
        }
        if (Stage == 14)
        {
            if (!Warm(3, 0.05) || Controller->IsRoundRequestPending()) return false;
            const FCombatRoundUnitView* Entry = FindAllyView();
            if (!Check(ObservedTileClicks == 1 && ObservedClickCoord == Destination->GridCoord && Entry && Entry->bHasMovePlan && Entry->MoveDestinationCoord == Destination->GridCoord && Ally->GetActorLocation().Equals(InitialLocation, 0.01f) && Ally->GetCurrentSubActionPoint() == InitialSAP, TEXT("Actual Slate LMB down/up dispatches the original destination once and the normal controller/server path reserves it without moving or charging SAP."))) return End();
            RestorePointer();
            Controller->SetRoundReady(true);
            if (!Check(Round->IsSAPMovementInProgress(), TEXT("The normal Ready request starts the server's SAP movement after the actual mouse reservation."))) return End();
            Advance(10);
            return false;
        }
        if (Stage == 22)
        {
            const uint64 Frame = GFrameCounter;
            if (Frame == SampleLastEngineFrame) return false;
            const double Now = FPlatformTime::Seconds();
            UWorld* World = Controller->GetWorld();
            if (Frame != SampleLastEngineFrame + 1)
            {
                WallIntervals.Reset();
                WorldDeltas.Reset();
                AppDeltas.Reset();
                SampleRows.Reset();
                if (!Check(++SampleRestarts <= 5, TEXT("A bounded environment sample obtains 180 consecutive actual engine frames."))) return End();
            }
            else
            {
                const double Wall = (Now - SampleLastPlatform) * 1000.0;
                const double WorldDelta = World->GetDeltaSeconds() * 1000.0;
                const double AppDelta = FApp::GetDeltaTime() * 1000.0;
                if (!Check(FMath::IsFinite(Wall) && Wall > 0.0 && FMath::IsFinite(WorldDelta) && WorldDelta > 0.0 && World->GetTimeSeconds() > SampleLastWorldTime && FMath::IsFinite(AppDelta) && AppDelta > 0.0, TEXT("Every sampled real frame has positive finite platform and engine deltas with advancing PIE world time."))) return End();
                WallIntervals.Add(Wall);
                WorldDeltas.Add(WorldDelta);
                AppDeltas.Add(AppDelta);
                SampleRows.Add(FString::Printf(TEXT("%llu,%.6f,%.6f,%.6f,%.6f"), Frame, static_cast<double>(World->GetTimeSeconds()), Wall, WorldDelta, AppDelta));
            }
            SampleLastPlatform = Now;
            SampleLastEngineFrame = Frame;
            SampleLastWorldTime = World->GetTimeSeconds();
            if (WallIntervals.Num() < 180) return false;
            if (!WriteFrameTiming() || !ReserveMovement()) return End();
            Advance(11);
            return false;
        }
        return false;
    }

private:
    bool Connect()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || World->GetNetMode() != NM_Standalone) continue;
            Controller = Cast<ACombatDebugPlayerController>(World->GetFirstPlayerController());
            Mode = World->GetAuthGameMode<ACombatDebugGameMode>();
            Round = Controller.IsValid() ? Controller->GetRoundCoordinator() : nullptr;
            if (!Controller.IsValid() || !Mode.IsValid() || !Round.IsValid() || !Controller->IsLocalController() || Round->GetView().Phase != ECombatRoundPhase::Planning) continue;
            RetainViewport();
            if (!RetainedViewport.IsValid()) continue;
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller.Get(), Widgets, UCombatUnitHealthDebugWidget::StaticClass(), false);
            for (UUserWidget* Widget : Widgets)
            {
                const UCombatRoundPlanningWidget* PlanningOwner = Widget->GetTypedOuter<UCombatRoundPlanningWidget>();
                if (Widget->GetOwningPlayer() == Controller.Get() && PlanningOwner && PlanningOwner->IsActivated() && Widget->GetCachedGeometry().GetLocalSize().X > 0.0) Health = Cast<UCombatUnitHealthDebugWidget>(Widget);
            }
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller.Get(), Widgets, UCombatRoundPlanningWidget::StaticClass(), false);
            for (UUserWidget* Widget : Widgets) if (Widget->GetOwningPlayer() == Controller.Get() && Cast<UCombatRoundPlanningWidget>(Widget)->IsActivated()) Planning = Cast<UCombatRoundPlanningWidget>(Widget);
            if (!Planning.IsValid()) continue;
            if (Health.IsValid()) return true;
        }
        return false;
    }

    bool VerifyRoster()
    {
        UWorld* World = Controller->GetWorld();
        const URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        ACombatArena* Arena = Round->GetArena();
        if (!Check(Run && !Run->IsManagedRun() && !Run->HasManagedLease() && ACombatDebugGameMode::IsDebugWorld(World), TEXT("The saved debug game mode provides standalone combat outside persistent Run ownership."))) return false;
        if (!Check(IsValid(Arena) && IsValid(Arena->Grid) && !Arena->Grid->TileMap.IsEmpty() && IsValid(Arena->CameraAnchor) && Controller->GetViewTarget() == Arena->CameraAnchor && Arena->CameraAnchor->FindComponentByClass<UCameraComponent>(), TEXT("The authored grid and gameplay camera are active in the actual PIE session."))) return false;
        int32 EnemyCount = 0;
        for (const FCombatRoundUnitView& Entry : Round->GetView().Units)
        {
            if (!Check(IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive() && Entry.Unit->GetCurrentTile() && Entry.Unit->GetCurrentTile()->GetOccupyingUnit() == Entry.Unit, TEXT("Each authored combat unit has a live actor and matching tile occupancy."))) return false;
            if (Entry.bEnemy)
            {
                Enemy = Entry.Unit;
                ++EnemyCount;
            }
            else
            {
                Ally = Entry.Unit;
                AllyId = Entry.UnitId;
            }
        }
        FProfessionDefinition Warrior;
        FText Error;
        return Check(Round->GetView().Units.Num() == 5 && EnemyCount == 4 && Ally.IsValid() && Mode->PartyDefinition && Mode->PartyDefinition->ResolveProfession(TEXT("Warrior"), Warrior, Error) && Ally->IsA(Warrior.CombatClass), TEXT("The existing map spawns the configured warrior and four enemies without a test asset copy."));
    }

    FTodoHealthPaintObservation Observe()
    {
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        return Probe->ObservePaint(Health->GetPaintSpaceGeometry(), Window);
    }

    // Compare final emitted window-space elements with the independent UMG world-to-viewport projection API.
    // 최종 창 좌표 그리기 요소를 독립적인 UMG 월드→뷰포트 투영 API와 비교합니다.
    bool VerifyHealthPlacement(const FTodoHealthPaintObservation& Observation)
    {
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (!Check(Viewport && Viewport->Viewport && Widget.IsValid(), TEXT("Health placement observations retain the real render viewport."))) return false;
        const FIntPoint PixelSize = Viewport->Viewport->GetSizeXY();
        const FGeometry PaintGeometry = Widget->GetPaintSpaceGeometry();
        const FVector2D PaintSize = PaintGeometry.GetLocalSize();
        const FVector2D UMGSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Controller.Get()).GetLocalSize();
        if (!Check(PixelSize == DesiredSize && PaintSize.X > 0.0 && PaintSize.Y > 0.0 && UMGSize.X > 0.0 && UMGSize.Y > 0.0 && Observation.LabelBounds.Num() == Observation.Labels.Num() && Observation.PanelBounds.Num() == Observation.Labels.Num(), TEXT("Every emitted HP label has a real panel and text bound at the exact requested viewport size."))) return false;
        const auto ToPixel = [&PaintGeometry, PaintSize, PixelSize](double X, double Y)
        {
            return PaintGeometry.AbsoluteToLocal(FVector2D(X, Y)) / PaintSize * FVector2D(PixelSize);
        };
        int32 Index = 0;
        double MaxAnchorError = 0.0;
        for (const FCombatRoundUnitView& Entry : Round->GetView().Units)
        {
            const AUnitBase* Unit = Entry.Unit;
            if (!IsValid(Unit) || !Unit->IsUnitAlive()) continue;
            const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
            if (!Check(IsValid(Capsule) && Observation.PanelBounds.IsValidIndex(Index) && Observation.LabelBounds.IsValidIndex(Index), TEXT("Each live fixture unit contributes one ordered real HP panel and label."))) return false;
            FVector2D Projected;
            const FVector Anchor = Capsule->GetComponentLocation() + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 24.f);
            if (!Check(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller.Get(), Anchor, Projected, false), TEXT("The official UMG API projects each capsule-top HP anchor."))) return false;
            const FVector2D Expected = Projected / UMGSize * FVector2D(PixelSize);
            const FSlateRect& Panel = Observation.PanelBounds[Index];
            const FSlateRect& Label = Observation.LabelBounds[Index];
            const FVector2D PanelMin = ToPixel(Panel.Left, Panel.Top);
            const FVector2D PanelMax = ToPixel(Panel.Right, Panel.Bottom);
            const FVector2D LabelMin = ToPixel(Label.Left, Label.Top);
            const FVector2D LabelMax = ToPixel(Label.Right, Label.Bottom);
            const FVector2D Actual((PanelMin.X + PanelMax.X) * 0.5, PanelMax.Y);
            const double Error = FVector2D::Distance(Actual, Expected);
            MaxAnchorError = FMath::Max(MaxAnchorError, Error);
            const bool bPanelVisible = PanelMin.X >= -1.0 && PanelMin.Y >= -1.0 && PanelMax.X <= PixelSize.X + 1.0 && PanelMax.Y <= PixelSize.Y + 1.0;
            const bool bLabelContained = LabelMin.X >= PanelMin.X - 1.0 && LabelMin.Y >= PanelMin.Y - 1.0 && LabelMax.X <= PanelMax.X + 1.0 && LabelMax.Y <= PanelMax.Y + 1.0;
            const bool bLabelCentered = FMath::Abs((LabelMin.X + LabelMax.X) * 0.5 - Expected.X) <= 2.0;
            if (!Check(Error <= 2.0 && bPanelVisible && bLabelContained && bLabelCentered, FString::Printf(TEXT("Unit %d HP panel bottom-center and text align with the actual projected capsule anchor without clipping; expected=%s; actual=%s; error=%.3fpx; panel=[%s,%s]; label=[%s,%s]; viewport=%s."), Entry.UnitId, *Expected.ToString(), *Actual.ToString(), Error, *PanelMin.ToString(), *PanelMax.ToString(), *LabelMin.ToString(), *LabelMax.ToString(), *PixelSize.ToString()))) return false;
            ++Index;
        }
        if (!Check(Index == Observation.Labels.Num(), TEXT("All live units and emitted HP placements correspond without missing or extra draws."))) return false;
        Test->AddInfo(FString::Printf(TEXT("Actual HP placement: %s; viewport=%s; live units=%d; maximum anchor error=%.3fpx; paint origin=%s; desktop origin=%s."), *Name, *PixelSize.ToString(), Index, MaxAnchorError, *Health->GetPaintSpaceGeometry().LocalToAbsolute(FVector2D::ZeroVector).ToString(), *Health->GetTickSpaceGeometry().LocalToAbsolute(FVector2D::ZeroVector).ToString()));
        return true;
    }

    void RestoreReviewWindow()
    {
        const TSharedPtr<SWindow> Window = MovedReviewWindow.Pin();
        if (Window.IsValid() && Window->GetNativeWindow().IsValid()) Window->MoveWindowTo(OriginalReviewWindowPosition);
        MovedReviewWindow.Reset();
    }

    bool Capture(const FString& Suffix)
    {
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (!Check(Viewport && Viewport->Viewport && Viewport->Viewport->GetSizeXY() == DesiredSize && Widget.IsValid(), FString::Printf(TEXT("The actual gameplay viewport matches the requested aspect before capturing Slate UI; expected %s; observed %s."), *DesiredSize.ToString(), Viewport && Viewport->Viewport ? *Viewport->Viewport->GetSizeXY().ToString() : TEXT("missing")))) return false;
        TArray<FColor> Pixels;
        FIntVector Size;
        if (!Check(FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size) && Size.X == DesiredSize.X && Size.Y == DesiredSize.Y && Pixels.Num() == Size.X * Size.Y, TEXT("A complete Slate screenshot at the requested aspect includes the real gameplay UI."))) return false;
        for (FColor& Pixel : Pixels) Pixel.A = 255;
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        const FString Filename = OutputDirectory / (Name + TEXT("_") + Suffix + TEXT(".png"));
        if (!Check(Png.Num() > 0 && FFileHelper::SaveArrayToFile(Png, *Filename) && IFileManager::Get().FileSize(*Filename) > 0, TEXT("The UI screenshot is written before PIE teardown."))) return false;
        Test->AddInfo(TEXT("TODO review screenshot: ") + Filename);
        return true;
    }

    bool ReserveMovement()
    {
        InitialLocation = Ally->GetActorLocation();
        InitialSAP = Ally->GetCurrentSubActionPoint();
        const FIntPoint Home = Ally->GetCurrentTile()->GridCoord;
        FText Error;
        for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, -1), FIntPoint(0, 1)})
        {
            ACombatGridTile* Candidate = Round->GetArena()->Grid->GetTileAtCoord(Home + Offset);
            if (!Candidate || !Round->CanMoveUnit(AllyId, Candidate->GridCoord, Error)) continue;
            for (const FVector OffsetPoint : {FVector(0, 0, 10), FVector(45, 0, 10), FVector(-45, 0, 10), FVector(0, 45, 10), FVector(0, -45, 10)})
            {
                FVector2D Pixel;
                FVector2D Cursor;
                const FVector Point = Candidate->GetActorLocation() + OffsetPoint;
                if (!PositionPointer(Candidate, Point, Pixel, Cursor)) continue;
                Destination = Candidate;
                ClickWorldPosition = Point;
                break;
            }
            if (Destination.IsValid()) break;
        }
        if (!Check(Destination.IsValid() && InitialSAP > 0, TEXT("The real arena exposes an on-screen legal adjacent free tile whose actual cursor trace and Slate path are unobstructed."))) return false;
        FCombatRoundCommand Idle;
        Idle.UnitId = AllyId;
        Idle.DestinationCoord = Home;
        Controller->SubmitRoundPlan(Idle);
        if (!Check(!Controller->IsRoundRequestPending(), TEXT("The actual standalone controller receives the server's idle-plan response."))) return false;
        Controller->SubmitRoundMove(AllyId, Destination->GridCoord);
        const FCombatRoundUnitView* Entry = FindAllyView();
        if (!Check(Entry && Entry->bHasMovePlan && Entry->MoveDestinationCoord == Destination->GridCoord && Ally->GetActorLocation().Equals(InitialLocation, 0.01f) && Ally->GetCurrentSubActionPoint() == InitialSAP, TEXT("The public controller request reserves movement without moving or spending SAP during planning."))) return false;
        MovementReservedWorldTime = Controller->GetWorld()->GetTimeSeconds();
        return true;
    }

    const FCombatRoundUnitView* FindAllyView() const
    {
        return Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == AllyId; });
    }

    UButton* FindPlanningButton(const FString& Caption) const
    {
        UButton* Result = nullptr;
        if (Planning.IsValid() && Planning->WidgetTree) Planning->WidgetTree->ForEachWidget([&Result, &Caption](UWidget* Widget)
        {
            UButton* Button = Cast<UButton>(Widget);
            const UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
            if (Button && Button->IsVisible() && Label && Label->GetText().ToString() == Caption) Result = Button;
        });
        return Result;
    }

    // Use the actual platform cursor and viewport trace, then require the genuine world hit to be the intended tile.
    // 실제 플랫폼 커서와 뷰포트 추적을 사용하고 실제 월드 충돌 대상이 의도한 타일인지 확인합니다.
    bool PositionPointer(ACombatGridTile* Tile, const FVector& Point, FVector2D& OutPixel, FVector2D& OutCursor)
    {
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (!Tile || !Viewport || !Viewport->Viewport || !Widget.IsValid()) return false;
        const FIntPoint Size = Viewport->Viewport->GetSizeXY();
        if (Size != FIntPoint(1280, 720) || !Controller->ProjectWorldLocationToScreen(Point, OutPixel) || OutPixel.X < 4.f || OutPixel.Y < 4.f || OutPixel.X >= Size.X - 4.f || OutPixel.Y >= Size.Y - 4.f) return false;
        FSlateApplication& Slate = FSlateApplication::Get();
        const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Widget.ToSharedRef());
        if (!Window.IsValid() || !Window->GetNativeWindow().IsValid()) return false;
        if (!bCursorMoved)
        {
            PreviousCursor = Slate.GetCursorPos();
            bCursorMoved = true;
            Window->BringToFront();
        }
        const FGeometry& Geometry = Widget->GetCachedGeometry();
        OutCursor = Geometry.LocalToAbsolute(OutPixel * Geometry.GetLocalSize() / FVector2D(Size));
        const FVector2D Before = Slate.GetCursorPos();
        Slate.SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
        Slate.SetCursorPos(OutCursor);
        Viewport->Viewport->SetMouse(FMath::RoundToInt(OutPixel.X), FMath::RoundToInt(OutPixel.Y));
        Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, OutCursor, Before, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
        const FWidgetPath Path = Slate.LocateWindowUnderMouse(OutCursor, Slate.GetInteractiveTopLevelWindows());
        if (!Path.IsValid() || Path.GetLastWidget() != Widget.ToSharedRef()) return false;
        FHitResult Hit;
        if (!Controller->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit) || Hit.GetActor() != Tile) return false;
        // The production click also tests registered capsules even though their Visibility response is Ignore.
        // 제품 클릭은 캡슐의 Visibility 응답이 Ignore여도 등록된 캡슐을 별도로 검사합니다.
        FVector RayStart;
        FVector RayDirection;
        if (!Controller->DeprojectMousePositionToWorld(RayStart, RayDirection)) return false;
        const FVector RayEnd = RayStart + RayDirection * (Hit.Distance + 1.f);
        for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
        {
            UCapsuleComponent* Capsule = IsValid(Unit.Unit) && Unit.HP > 0.f && Unit.Unit->GetActorEnableCollision() ? Unit.Unit->GetCapsuleComponent() : nullptr;
            FHitResult CapsuleHit;
            if (Capsule && Capsule->IsQueryCollisionEnabled() && Capsule->LineTraceComponent(CapsuleHit, RayStart, RayEnd, FCollisionQueryParams()) && CapsuleHit.Distance <= Hit.Distance) return false;
        }
        return true;
    }

    void RestorePointer()
    {
        if (Controller.IsValid() && TileClickObserver.IsValid()) Controller->OnRoundWorldTileClicked.Remove(TileClickObserver);
        TileClickObserver.Reset();
        if (!FSlateApplication::IsInitialized()) return;
        FSlateApplication& Slate = FSlateApplication::Get();
        if (bPointerPressed)
        {
            const FVector2D Cursor = Slate.GetCursorPos();
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, Cursor, Cursor, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
            bPointerPressed = false;
        }
        if (bCursorMoved) Slate.SetCursorPos(PreviousCursor);
        bCursorMoved = false;
    }

    TSharedRef<FJsonObject> SummarizeIntervals(const TArray<double>& Values) const
    {
        TArray<double> Sorted = Values;
        Sorted.Sort();
        double Total = 0.0;
        for (double Value : Values) Total += Value;
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("mean_ms"), Total / Values.Num());
        Result->SetNumberField(TEXT("median_ms"), (Sorted[(Sorted.Num() - 1) / 2] + Sorted[Sorted.Num() / 2]) * 0.5);
        Result->SetNumberField(TEXT("p95_ms"), Sorted[FMath::CeilToInt(Sorted.Num() * 0.95) - 1]);
        Result->SetNumberField(TEXT("min_ms"), Sorted[0]);
        Result->SetNumberField(TEXT("max_ms"), Sorted.Last());
        return Result;
    }

    // Observe current idle frame intervals without screenshots, readbacks, manual world ticks or quality changes.
    // 캡처·이미지 읽기·수동 월드 Tick·품질 변경 없이 현재 유휴 프레임 간격을 관찰합니다.
    bool WriteFrameTiming()
    {
        if (!Check(WallIntervals.Num() == 180 && SampleQuality == Scalability::GetQualityLevels() && Controller->GetWorld()->GetGameViewport()->Viewport->GetSizeXY() == FIntPoint(1280, 720), TEXT("The full current frame sample retains 180 real frames, 1280x720 and its unchanged actual quality values."))) return false;
        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("map"), MapPath);
        Record->SetStringField(TEXT("run_type"), TEXT("Editor standalone PIE current idle planning scene"));
        Record->SetNumberField(TEXT("sample_frames"), WallIntervals.Num());
        Record->SetNumberField(TEXT("discarded_nonconsecutive_windows"), SampleRestarts);
        Record->SetNumberField(TEXT("viewport_width"), 1280);
        Record->SetNumberField(TEXT("viewport_height"), 720);
        Record->SetBoolField(TEXT("render_offscreen"), bRenderOffscreen);
        Record->SetBoolField(TEXT("capture_readback_during_samples"), false);
        Record->SetBoolField(TEXT("cpu_or_gpu_present_time_measured"), false);
        Record->SetBoolField(TEXT("previous_version_fps_improvement_proven"), false);
        Record->SetStringField(TEXT("limits"), TEXT("Platform intervals and engine deltas include editor scheduling; CPU, GPU and physical display present times are not measured separately. The render_offscreen field records the actual execution mode. No previous-version baseline is measured."));
        Record->SetObjectField(TEXT("wall_frame_interval"), SummarizeIntervals(WallIntervals));
        Record->SetObjectField(TEXT("world_delta"), SummarizeIntervals(WorldDeltas));
        Record->SetObjectField(TEXT("app_delta"), SummarizeIntervals(AppDeltas));
        TSharedRef<FJsonObject> Quality = MakeShared<FJsonObject>();
        Quality->SetNumberField(TEXT("resolution"), SampleQuality.ResolutionQuality);
        Quality->SetNumberField(TEXT("view_distance"), SampleQuality.ViewDistanceQuality);
        Quality->SetNumberField(TEXT("anti_aliasing"), SampleQuality.AntiAliasingQuality);
        Quality->SetNumberField(TEXT("shadow"), SampleQuality.ShadowQuality);
        Quality->SetNumberField(TEXT("global_illumination"), SampleQuality.GlobalIlluminationQuality);
        Quality->SetNumberField(TEXT("reflection"), SampleQuality.ReflectionQuality);
        Quality->SetNumberField(TEXT("post_process"), SampleQuality.PostProcessQuality);
        Quality->SetNumberField(TEXT("texture"), SampleQuality.TextureQuality);
        Quality->SetNumberField(TEXT("effects"), SampleQuality.EffectsQuality);
        Quality->SetNumberField(TEXT("foliage"), SampleQuality.FoliageQuality);
        Quality->SetNumberField(TEXT("shading"), SampleQuality.ShadingQuality);
        Quality->SetNumberField(TEXT("landscape"), SampleQuality.LandscapeQuality);
        Record->SetObjectField(TEXT("scalability"), Quality);
        TSharedRef<FJsonObject> CVars = MakeShared<FJsonObject>();
        for (const TCHAR* CVarName : {TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("r.ReflectionMethod"), TEXT("r.Nanite"), TEXT("r.ScreenPercentage"), TEXT("r.VSync"), TEXT("t.MaxFPS")})
        {
            const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
            CVars->SetStringField(CVarName, CVar ? CVar->GetString() : TEXT("unavailable"));
        }
        Record->SetObjectField(TEXT("render_cvars"), CVars);
        FString Json;
        FJsonSerializer::Serialize(Record, TJsonWriterFactory<>::Create(&Json));
        const FString JsonPath = OutputDirectory / (Name + TEXT("_current_frame_timing.json"));
        const FString CsvPath = OutputDirectory / (Name + TEXT("_current_frame_timing.csv"));
        const FString Csv = TEXT("engine_frame,world_time_s,wall_interval_ms,world_delta_ms,app_delta_ms\n") + FString::Join(SampleRows, TEXT("\n")) + TEXT("\n");
        if (!Check(FFileHelper::SaveStringToFile(Json, *JsonPath) && FFileHelper::SaveStringToFile(Csv, *CsvPath), TEXT("Current raw frame intervals and their explicit limitations are saved only after sampling completes."))) return false;
        Test->AddInfo(FString::Printf(TEXT("Current environment sample: %s; frames=180; 1280x720; wall mean=%.3fms median=%.3fms p95=%.3fms; JSON=%s; CSV=%s. These are current PIE intervals, not CPU/GPU present timing or before/after FPS improvement."), *Name, SummarizeIntervals(WallIntervals)->GetNumberField(TEXT("mean_ms")), SummarizeIntervals(WallIntervals)->GetNumberField(TEXT("median_ms")), SummarizeIntervals(WallIntervals)->GetNumberField(TEXT("p95_ms")), *JsonPath, *CsvPath));
        return true;
    }

    bool Check(bool bCondition, const FString& Message)
    {
        return Test->TestTrue(*Message, bCondition);
    }

    bool Warm(int32 Frames, double Seconds)
    {
        ++WarmFrames;
        return WarmFrames >= Frames && FPlatformTime::Seconds() - Started >= Seconds && FAssetCompilingManager::Get().GetNumRemainingAssets() == 0;
    }

    void Advance(int32 Next)
    {
        Stage = Next;
        Started = FPlatformTime::Seconds();
        WarmFrames = 0;
    }

    bool End()
    {
        RetainViewport();
        RestorePointer();
        RestoreReviewWindow();
        Probe.Reset();
        GEditor->RequestEndPlayMap();
        Test->AddInfo(TEXT("Render review checks authored camera/grid/roster, real HP draw output and actual Slate tile input. Current capture-free frame intervals are recorded for environment comparison; physical display present, before/after FPS baselines, sound and network support are separate."));
        Advance(99);
        return false;
    }

    // Keep a game-thread owner while PIE removes its window so queued Slate draws cannot destroy the viewport on the render thread.
    // PIE가 창을 제거하는 동안 게임 스레드 소유 참조를 유지하여 대기 중인 Slate 렌더 작업이 뷰포트를 렌더 스레드에서 파괴하지 않게 합니다.
    void RetainViewport()
    {
        check(IsInGameThread());
        if (RetainedViewport.IsValid() || !Controller.IsValid() || !Controller->GetWorld()) return;
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
    }

    // Drain queued Slate work after teardown, then release the last fixture reference on the required game thread.
    // 종료 후 대기 중인 Slate 작업을 비운 뒤 검수 참조를 필수 게임 스레드에서 해제합니다.
    void ReleaseRetainedViewport()
    {
        check(IsInGameThread());
        if (!RetainedViewport.IsValid()) return;
        FlushRenderingCommands();
        RetainedViewport.Reset();
    }

    FAutomationTestBase* Test;
    FString MapPath;
    FString Name;
    FString Slot;
    FString OutputDirectory;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TStrongObjectPtr<UTodoHealthPaintProbe> Probe;
    TSharedPtr<ISlateViewport> RetainedViewport;
    TWeakPtr<SWindow> MovedReviewWindow;
    FVector2D OriginalReviewWindowPosition = FVector2D::ZeroVector;
    FIntPoint OriginalNativePosition = FIntPoint::ZeroValue;
    FIntPoint OriginalNativeWindowSize = FIntPoint::ZeroValue;
    TWeakObjectPtr<ACombatDebugPlayerController> Controller;
    TWeakObjectPtr<ACombatDebugGameMode> Mode;
    TWeakObjectPtr<ACombatRoundCoordinator> Round;
    TWeakObjectPtr<UCombatUnitHealthDebugWidget> Health;
    TWeakObjectPtr<UCombatRoundPlanningWidget> Planning;
    TWeakObjectPtr<AUnitBase> Ally;
    TWeakObjectPtr<AUnitBase> Enemy;
    TWeakObjectPtr<ACombatGridTile> Destination;
    FVector InitialLocation = FVector::ZeroVector;
    int32 InitialSAP = 0;
    int32 AllyId = INDEX_NONE;
    float MovementReservedWorldTime = 0.f;
    FVector ClickWorldPosition = FVector::ZeroVector;
    FVector2D ClickCursor = FVector2D::ZeroVector;
    FVector2D PreviousCursor = FVector2D::ZeroVector;
    bool bCursorMoved = false;
    bool bPointerPressed = false;
    FDelegateHandle TileClickObserver;
    FIntPoint ObservedClickCoord = FIntPoint::ZeroValue;
    int32 ObservedTileClicks = 0;
    TArray<double> WallIntervals;
    TArray<double> WorldDeltas;
    TArray<double> AppDeltas;
    TArray<FString> SampleRows;
    Scalability::FQualityLevels SampleQuality;
    double SampleLastPlatform = 0.0;
    uint64 SampleLastEngineFrame = 0;
    float SampleLastWorldTime = 0.f;
    int32 SampleRestarts = 0;
    int32 Stage = 0;
    int32 WarmFrames = 0;
    double Started = 0.0;
    int32 ResizeAttempts = 0;
    int32 ScreenIndex = 0;
    FIntPoint DesiredSize = FIntPoint(1280, 720);
    bool bRenderOffscreen = false;
    bool bHealth = false;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTodoEnvironmentRenderReview, "ProjectA.TodoReview.Environment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTodoEnvironmentRenderReview::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    for (const TPair<FString, FString>& Map : TodoRenderReview::EnvironmentMaps)
    {
        OutBeautifiedNames.Add(Map.Key);
        OutTestCommands.Add(Map.Key);
    }
}

bool FTodoEnvironmentRenderReview::RunTest(const FString& Parameters)
{
    const FString* RelativePath = TodoRenderReview::EnvironmentMaps.Find(Parameters);
    if (!RelativePath) return false;
    const FString MapPath = TEXT("/Game/User_JeHoon/LEVEL/Environment/") + *RelativePath;
    FString Slot;
    if (!TodoRenderReview::Prepare(this, MapPath, Slot)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(TodoRenderReview::FReview(this, MapPath, Parameters, Slot, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTodoDeathHealthPaintReview, "ProjectA.TodoReview.DeathHealthPaint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTodoDeathHealthPaintReview::RunTest(const FString& Parameters)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen")))
    {
        AddError(TEXT("DeathHealthPaint includes two actual native Windows positions; omit -RenderOffscreen to run this regression. No editor preferences are changed."));
        return false;
    }
    const FString MapPath = TEXT("/Game/User_JeHoon/LEVEL/Environment/Grassland/MeadowBloom");
    FString Slot;
    if (!TodoRenderReview::Prepare(this, MapPath, Slot)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(TodoRenderReview::FReview(this, MapPath, TEXT("DeathHealthPaint"), Slot, true));
    return true;
}

#endif
