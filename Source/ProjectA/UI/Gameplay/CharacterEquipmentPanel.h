#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "CharacterEquipmentPanel.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
struct FGameplayViewState;

UCLASS()
class PROJECTA_API UCharacterEquipmentPanel : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void RefreshEquipment(const FGameplayViewState& View, FGuid CharacterId);

protected:
    virtual void NativeOnInitialized() override;

private:
    UTextBlock* AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding = 8.0f);
    void AddEquipmentSlot(FName SlotId, const FText& Label, int32 Row, int32 Column);

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CharacterText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UUniformGridPanel> EquipmentSlots;
};
