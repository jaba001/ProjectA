#include "UI/Combat/CombatUnitHealthDebugWidget.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/CapsuleComponent.h"
#include "Controller/CombatRoundPlayerController.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Unit/UnitBase.h"

namespace
{
    TAutoConsoleVariable<int32> CVarDebugUnitHP(TEXT("projecta.Debug.UnitHP"), 1, TEXT("Show combat unit HP labels: 0=off, 1=on. / 전투 유닛 HP 표시: 0=끄기, 1=켜기."), ECVF_Cheat);
}
#endif

void UCombatUnitHealthDebugWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetClipping(EWidgetClipping::ClipToBounds);
    ForceVolatile(true);
}

int32 UCombatUnitHealthDebugWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (CVarDebugUnitHP.GetValueOnGameThread() == 0 || !FSlateApplication::IsInitialized()) return BaseLayer;
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!IsValid(Coordinator) || !Controller->IsLocalController()) return BaseLayer;
    const FVector2D Bounds = AllottedGeometry.GetLocalSize();
    if (Bounds.X <= 0.0 || Bounds.Y <= 0.0) return BaseLayer;
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13);
    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    static const FSlateRoundedBoxBrush Background(FLinearColor::White, 5.f);
    const FSlateBrush* BarBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    FNumberFormattingOptions Numbers;
    Numbers.SetUseGrouping(false).SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(1);
    for (const FCombatRoundUnitView& Entry : Coordinator->GetView().Units)
    {
        const AUnitBase* Unit = Entry.Unit;
        if (!IsValid(Unit) || Unit->IsActorBeingDestroyed() || Unit->IsHidden()) continue;
        const UAS_Unit* Attributes = Unit->GetAttributeSet();
        const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
        if (!IsValid(Attributes) || !IsValid(Capsule) || !FMath::IsFinite(Attributes->GetHP()) || !FMath::IsFinite(Attributes->GetMaxHP()) || Attributes->GetMaxHP() <= 0.f) continue;
        const float HP = Unit->IsUnitAlive() ? FMath::Max(0.f, Attributes->GetHP()) : 0.f;
        const float MaxHP = Attributes->GetMaxHP();
        const FVector Anchor = Capsule->GetComponentLocation() + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 24.f);
        FVector2D ScreenPosition;
        if (Anchor.ContainsNaN() || !Controller->ProjectWorldLocationToScreen(Anchor, ScreenPosition, false)) continue;
        FVector2D Position;
        // Convert viewport pixels through Slate geometry once, including DPI and window offsets.
        // DPI와 창 위치를 포함해 뷰포트 픽셀을 Slate 지오메트리로 한 번만 변환합니다.
        USlateBlueprintLibrary::ScreenToWidgetLocal(this, AllottedGeometry, ScreenPosition, Position);
        if (Position.ContainsNaN() || Position.X < 0.0 || Position.Y < 0.0 || Position.X > Bounds.X || Position.Y > Bounds.Y) continue;
        const FString Label = FString::Printf(TEXT("HP %s / %s"), *FText::AsNumber(HP, &Numbers).ToString(), *FText::AsNumber(MaxHP, &Numbers).ToString());
        const FVector2D TextSize = FontMeasure->Measure(Label, Font);
        const FVector2D PanelSize(FMath::Max(116.0, TextSize.X + 16.0), TextSize.Y + 19.0);
        const FVector2D PanelPosition = Position - FVector2D(PanelSize.X * 0.5, PanelSize.Y);
        const FLinearColor TeamColor = Entry.bEnemy ? FLinearColor(1.f, 0.36f, 0.16f) : FLinearColor(0.18f, 0.85f, 0.64f);
        const FVector2D BarPosition = PanelPosition + FVector2D(8.0, PanelSize.Y - 10.0);
        const FVector2D BarSize(PanelSize.X - 16.0, 4.0);
        FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(PanelPosition)), &Background, ESlateDrawEffect::None, FLinearColor(0.008f, 0.012f, 0.02f, 0.92f));
        FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 2, AllottedGeometry.ToPaintGeometry(BarSize, FSlateLayoutTransform(BarPosition)), BarBrush, ESlateDrawEffect::None, FLinearColor(0.12f, 0.14f, 0.17f));
        const float Fraction = FMath::Clamp(HP / MaxHP, 0.f, 1.f);
        if (Fraction > 0.f) FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 3, AllottedGeometry.ToPaintGeometry(FVector2D(BarSize.X * Fraction, BarSize.Y), FSlateLayoutTransform(BarPosition)), BarBrush, ESlateDrawEffect::None, TeamColor);
        const FVector2D TextPosition = PanelPosition + FVector2D((PanelSize.X - TextSize.X) * 0.5, 4.0);
        FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 3, AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPosition)), Label, Font, ESlateDrawEffect::None, HP > 0.f ? FLinearColor::White : FLinearColor(1.f, 0.45f, 0.4f));
    }
    return BaseLayer + 3;
#else
    return BaseLayer;
#endif
}
