#include "UI/MainMenu/OptionsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "Controller/MainMenuPlayerController.h"
#include "UI/MainMenu/MainMenuRootWidget.h"

void UOptionsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
    Root->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 1.0f));
    Root->SetPadding(FMargin(48.0f));
    Root->SetHorizontalAlignment(HAlign_Center);
    Root->SetVerticalAlignment(VAlign_Center);
    WidgetTree->RootWidget = Root;
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Root->SetContent(Content);
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("옵션 · 그래픽 품질")));
    Content->AddChild(Title);
    Quality = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("QualitySelect"));
    for (const TCHAR* Label : { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("최고"), TEXT("시네마틱") })
    {
        Quality->AddOption(Label);
    }
    Content->AddChild(Quality);
    VSync = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("VSyncCheck"));
    UTextBlock* VSyncLabel = WidgetTree->ConstructWidget<UTextBlock>();
    VSyncLabel->SetText(FText::FromString(TEXT("수직 동기화")));
    VSync->AddChild(VSyncLabel);
    Content->AddChild(VSync);
    UButton* Apply = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApplyOptionsButton"));
    UTextBlock* ApplyLabel = WidgetTree->ConstructWidget<UTextBlock>();
    ApplyLabel->SetText(FText::FromString(TEXT("적용 및 저장")));
    Apply->AddChild(ApplyLabel);
    Apply->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::ApplyOptions);
    Content->AddChild(Apply);
    UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseOptionsButton"));
    UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CloseLabel->SetText(FText::FromString(TEXT("닫기 / 적용 전 취소")));
    Close->AddChild(CloseLabel);
    Close->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::CloseOptions);
    Content->AddChild(Close);
}

void UOptionsWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
    {
        const int32 Level = Settings->GetOverallScalabilityLevel();
        Quality->SetSelectedIndex(Level);
        VSync->SetIsChecked(Settings->IsVSyncEnabled());
    }
}

void UOptionsWidget::ApplyOptions()
{
    if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
    {
        if (Quality->GetSelectedIndex() >= 0)
        {
            Settings->SetOverallScalabilityLevel(Quality->GetSelectedIndex());
        }
        Settings->SetVSyncEnabled(VSync->IsChecked());
        Settings->ApplySettings(false);
    }
}

void UOptionsWidget::CloseOptions()
{
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        Controller->GetMainMenuRootWidget()->ClearMenuStack();
    }
}
