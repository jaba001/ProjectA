#include "UI/Combat/CombatHUDWidget.h"

void UCombatHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsEnabled(false);
    SetVisibility(ESlateVisibility::Collapsed);
}

FText UCombatHUDWidget::GetTurnInfoText() const
{
    return FText::FromString(TEXT("라운드 계획 화면을 사용하세요."));
}
