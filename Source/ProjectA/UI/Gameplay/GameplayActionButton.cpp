#include "UI/Gameplay/GameplayActionButton.h"

#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "UI/Theme/DemonicUITheme.h"

void UGameplayActionButton::Configure(FName InActionId, const FText& Label)
{
    ActionId = InActionId;
    UTextBlock* Text = NewObject<UTextBlock>(this);
    Text->SetText(Label);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    Theme.StyleText(Text, false, 18);
    UButtonSlot* ButtonSlot = Cast<UButtonSlot>(AddChild(Text));

    if (ButtonSlot)
    {
        ButtonSlot->SetPadding(FMargin(20.0f, 12.0f));
        ButtonSlot->SetHorizontalAlignment(HAlign_Center);
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
    }

    Theme.StyleButton(this);
    OnClicked.AddUniqueDynamic(this, &UGameplayActionButton::HandleClicked);
}

void UGameplayActionButton::HandleClicked()
{
    OnActionRequested.Broadcast(ActionId);
}
