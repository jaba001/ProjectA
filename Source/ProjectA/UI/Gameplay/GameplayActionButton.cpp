#include "UI/Gameplay/GameplayActionButton.h"

#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"

void UGameplayActionButton::Configure(FName InActionId, const FText& Label)
{
    ActionId = InActionId;
    UTextBlock* Text = NewObject<UTextBlock>(this);
    Text->SetText(Label);
    UButtonSlot* ButtonSlot = Cast<UButtonSlot>(AddChild(Text));

    if (ButtonSlot)
    {
        ButtonSlot->SetPadding(FMargin(20.0f, 12.0f));
    }

    OnClicked.AddUniqueDynamic(this, &UGameplayActionButton::HandleClicked);
}

void UGameplayActionButton::HandleClicked()
{
    OnActionRequested.Broadcast(ActionId);
}
