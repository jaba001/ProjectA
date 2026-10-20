#include "UI/Gameplay/EncounterSignWidget.h"

#include "Components/TextBlock.h"

void UEncounterSignWidget::SetTitle(const FText& InTitle)
{
    Title = InTitle;
    if (Text_Title) Text_Title->SetText(Title);
}

void UEncounterSignWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (!Title.IsEmpty()) SetTitle(Title);
}
