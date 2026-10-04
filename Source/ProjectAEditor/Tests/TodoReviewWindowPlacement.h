#pragma once

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace TodoReviewWindowPlacement
{
    // Keep each explicitly named review's artifacts below the project's ignored automation directory.
    // 명시적으로 지정한 검수 결과를 프로젝트의 무시되는 automation 경로 아래에 보관합니다.
    inline bool OutputRoot(FAutomationTestBase* Test, FString& OutRoot)
    {
        OutRoot = FPaths::ProjectSavedDir() / TEXT("Automation/TodoReview");
        FParse::Value(FCommandLine::Get(), TEXT("ProjectAReviewOutputRoot="), OutRoot);
        OutRoot = FPaths::ConvertRelativePathToFull(OutRoot);
        FPaths::NormalizeDirectoryName(OutRoot);
        FPaths::CollapseRelativeDirectories(OutRoot);
        FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation"));
        FPaths::NormalizeDirectoryName(Allowed);
        return Test->TestTrue(TEXT("Explicit review output stays beneath this project's Saved/Automation directory."), OutRoot.StartsWith(Allowed + TEXT("/"), ESearchCase::IgnoreCase));
    }

    // Opt in to the current leftmost physical work area without changing persistent editor preferences.
    // 영구 에디터 설정을 바꾸지 않고 명시적으로 요청한 현재 가장 왼쪽 물리 작업 영역을 사용합니다.
    inline bool LeftWorkArea(FAutomationTestBase* Test, FPlatformRect& OutWorkArea)
    {
        FDisplayMetrics Metrics;
        FDisplayMetrics::RebuildDisplayMetrics(Metrics);
        const FMonitorInfo* Leftmost = nullptr;
        for (const FMonitorInfo& Monitor : Metrics.MonitorInfo)
        {
            if (Monitor.WorkArea.Right <= Monitor.WorkArea.Left || Monitor.WorkArea.Bottom <= Monitor.WorkArea.Top) continue;
            if (!Leftmost || Monitor.WorkArea.Left < Leftmost->WorkArea.Left || (Monitor.WorkArea.Left == Leftmost->WorkArea.Left && Monitor.WorkArea.Top < Leftmost->WorkArea.Top)) Leftmost = &Monitor;
        }
        if (!Test->TestNotNull(TEXT("Left-monitor review requires an actual enumerated monitor; coordinates are never guessed."), Leftmost)) return false;
        OutWorkArea = Leftmost->WorkArea;
        return true;
    }

    inline bool Configure(FAutomationTestBase* Test, ULevelEditorPlaySettings* Settings)
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectAReviewLeftMonitor"))) return true;
        if (!Test->TestTrue(TEXT("Review positioning changes a transient play-settings clone only."), Settings && Settings->GetOutermost() == GetTransientPackage() && !Settings->HasAnyFlags(RF_ClassDefaultObject))) return false;
        FPlatformRect WorkArea;
        if (!LeftWorkArea(Test, WorkArea)) return false;
        Settings->CenterNewWindow = false;
        Settings->NewWindowPosition = FIntPoint(WorkArea.Left + 16, WorkArea.Top + 16);
        Settings->MultipleInstancePositions.Reset();
        Settings->LastSize = FIntPoint::ZeroValue;
        Settings->LastExecutedPlayModeType = PlayMode_InEditorFloating;
        Test->AddInfo(FString::Printf(TEXT("Left-monitor review work area: %d,%d..%d,%d; new PIE origin %s."), WorkArea.Left, WorkArea.Top, WorkArea.Right, WorkArea.Bottom, *Settings->NewWindowPosition.ToString()));
        return true;
    }

    // Recheck the actual window after Slate's DPI fitting and later aspect-ratio changes.
    // Slate의 DPI 맞춤 및 이후 화면 비율 변경 뒤 실제 창을 다시 확인합니다.
    inline bool Ensure(FAutomationTestBase* Test, UWorld* World)
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectAReviewLeftMonitor"))) return true;
        UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() && FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        if (!Test->TestTrue(TEXT("Left-monitor review owns a real PIE window."), Window.IsValid() && Window->GetNativeWindow().IsValid())) return false;
        FPlatformRect WorkArea;
        if (!LeftWorkArea(Test, WorkArea)) return false;
        const FVector2D Size = Window->GetSizeInScreen();
        if (!Test->TestTrue(TEXT("The actual review window fits inside the left monitor's work area."), Size.X > 0 && Size.Y > 0 && Size.X <= WorkArea.Right - WorkArea.Left && Size.Y <= WorkArea.Bottom - WorkArea.Top)) return false;
        const FVector2D Current = Window->GetPositionInScreen();
        const FVector2D Position(FMath::Clamp(Current.X, static_cast<double>(WorkArea.Left), static_cast<double>(WorkArea.Right) - Size.X), FMath::Clamp(Current.Y, static_cast<double>(WorkArea.Top), static_cast<double>(WorkArea.Bottom) - Size.Y));
        if (!Current.Equals(Position, 0.5)) Window->MoveWindowTo(Position);
        const FVector2D Actual = Window->GetPositionInScreen();
        return Test->TestTrue(TEXT("The full review window stays on the actual left monitor."), Actual.X >= WorkArea.Left - 1 && Actual.Y >= WorkArea.Top - 1 && Actual.X + Size.X <= WorkArea.Right + 1 && Actual.Y + Size.Y <= WorkArea.Bottom + 1);
    }
}
