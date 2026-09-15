#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "UObject/Object.h"
#include "DemonicUITheme.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidgetTree;

UCLASS()
class PROJECTA_API UDemonicUITheme : public UObject
{
    GENERATED_BODY()

public:
    UDemonicUITheme();
    static const UDemonicUITheme& Get();

    void ApplyControls(UWidgetTree* Tree) const;
    void StyleButton(UButton* Button, bool bPrimary = false) const;
    void StyleText(UTextBlock* Text, bool bHeading = false, int32 FontSize = 0) const;
    void StylePanel(UBorder* Panel) const;
    void StyleBackdrop(UBorder* Background) const;
    void StyleBackgroundImage(UImage* Background) const;
    void AddDivider(UWidgetTree* Tree, UVerticalBox* Parent) const;

private:
    FButtonStyle MakeButtonStyle(const FButtonStyle& Existing, bool bPrimary, bool bCompact = false) const;

    // Keep native default-object references visible to garbage collection and asset cooking.
    // 네이티브 기본 객체의 에셋 참조를 가비지 수집과 에셋 쿠킹에 유지합니다.
    UPROPERTY()
    TObjectPtr<UTexture2D> ButtonReady;
    UPROPERTY()
    TObjectPtr<UTexture2D> ButtonHovered;
    UPROPERTY()
    TObjectPtr<UTexture2D> ButtonPressed;
    UPROPERTY()
    TObjectPtr<UTexture2D> CompactReady;
    UPROPERTY()
    TObjectPtr<UTexture2D> CompactHovered;
    UPROPERTY()
    TObjectPtr<UTexture2D> CompactPressed;
    UPROPERTY()
    TObjectPtr<UTexture2D> PanelTexture;
    UPROPERTY()
    TObjectPtr<UTexture2D> BackdropTexture;
    UPROPERTY()
    TObjectPtr<UTexture2D> CheckReady;
    UPROPERTY()
    TObjectPtr<UTexture2D> CheckSelected;
    UPROPERTY()
    TObjectPtr<UTexture2D> DividerTexture;
};

UCLASS()
class PROJECTA_API UDemonicComboBoxString : public UComboBoxString
{
    GENERATED_BODY()

public:
    UDemonicComboBoxString();
};
