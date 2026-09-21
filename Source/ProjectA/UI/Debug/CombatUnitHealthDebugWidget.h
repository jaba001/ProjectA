#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatUnitHealthDebugWidget.generated.h"

// Read-only development overlay; unit attributes remain authoritative for combat.
// 전투 어트리뷰트를 변경하지 않는 개발용 읽기 전용 오버레이입니다.
UCLASS()
class PROJECTA_API UCombatUnitHealthDebugWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
