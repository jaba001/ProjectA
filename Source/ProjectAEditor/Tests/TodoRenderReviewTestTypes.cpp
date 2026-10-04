#include "TodoRenderReviewTestTypes.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"

namespace
{
// Read final window-space bounds from emitted draw elements instead of reconstructing the widget's placement rules.
// 위젯 배치 규칙을 재구현하지 않고 생성된 그리기 요소에서 최종 창 좌표 경계를 읽습니다.
FSlateRect ReadDrawBounds(const FSlateDrawElement& Element)
{
    const FVector2f Size = Element.GetLocalSize();
    FSlateRect Bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const FVector2f Corner : {FVector2f::ZeroVector, FVector2f(Size.X, 0.f), Size, FVector2f(0.f, Size.Y)})
    {
        const FVector2f Point = TransformPoint(Element.GetRenderTransform(), Corner);
        Bounds.Left = FMath::Min(Bounds.Left, Point.X);
        Bounds.Top = FMath::Min(Bounds.Top, Point.Y);
        Bounds.Right = FMath::Max(Bounds.Right, Point.X);
        Bounds.Bottom = FMath::Max(Bounds.Bottom, Point.Y);
    }
    return Bounds;
}
}

FTodoHealthPaintObservation UTodoHealthPaintProbe::ObservePaint(const FGeometry& Geometry, const TSharedPtr<SWindow>& Window)
{
    FTodoHealthPaintObservation Observation;
    if (!Window.IsValid()) return Observation;
    const TSharedRef<SWidget> Widget = TakeWidget();
    FHittestGrid HitTestGrid;
    FSlateWindowElementList Elements(Window);
    const FPaintArgs Args(&Widget.Get(), HitTestGrid, Window->GetPositionInScreen(), FPlatformTime::Seconds(), 0.f);
    NativePaint(Args, Geometry, Geometry.GetLayoutBoundingRect(), Elements, 0, FWidgetStyle(), true);
    const FSlateDrawElementMap& Draws = Elements.GetUncachedDrawElements();
    for (const FSlateTextElement& Text : Draws.Get<static_cast<uint8>(EElementType::ET_Text)>())
    {
        Observation.Labels.Add(Text.GetText());
        Observation.LabelBounds.Add(ReadDrawBounds(Text));
    }
    for (const FSlateRoundedBoxElement& Panel : Draws.Get<static_cast<uint8>(EElementType::ET_RoundedBox)>()) Observation.PanelBounds.Add(ReadDrawBounds(Panel));
    Observation.BoxCount = Draws.Get<static_cast<uint8>(EElementType::ET_Box)>().Num() + Draws.Get<static_cast<uint8>(EElementType::ET_RoundedBox)>().Num();
    return Observation;
}
