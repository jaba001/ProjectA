#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "UI/Debug/CombatUnitHealthDebugWidget.h"
#include "TodoRenderReviewTestTypes.generated.h"

class SWindow;

struct FTodoHealthPaintObservation
{
    TArray<FString> Labels;
    TArray<FSlateRect> LabelBounds;
    TArray<FSlateRect> PanelBounds;
    int32 BoxCount = 0;
};

// Observe production draw elements with a real local controller and the actual paint-space geometry.
// 실제 로컬 컨트롤러와 실제 그리기 좌표 지오메트리로 실전 그리기 요소를 관찰합니다.
UCLASS(Transient, NotBlueprintable)
class UTodoHealthPaintProbe : public UCombatUnitHealthDebugWidget
{
    GENERATED_BODY()

public:
    FTodoHealthPaintObservation ObservePaint(const FGeometry& Geometry, const TSharedPtr<SWindow>& Window);
};
