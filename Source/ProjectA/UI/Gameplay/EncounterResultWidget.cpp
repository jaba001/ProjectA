#include "UI/Gameplay/EncounterResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/EngineBaseTypes.h"

TOptional<FUIInputConfig> UEncounterResultWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UEncounterResultWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!Text_Result || !Button_Continue)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = Root;
        UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
        Background->SetBrushColor(FLinearColor(0.025f, 0.04f, 0.06f, 0.98f));
        Root->AddChildToOverlay(Background);
        UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
        UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Content);
        ContentSlot->SetHorizontalAlignment(HAlign_Center);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
        Text_Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Result"));
        Button_Continue = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Continue"));
        UTextBlock* ContinueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Continue"));
        ContinueText->SetText(FText::FromString(TEXT("Continue / 계속")));
        UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button_Continue->AddChild(ContinueText));
        ButtonSlot->SetPadding(FMargin(24.0f, 12.0f));
        Content->AddChildToVerticalBox(Text_Result);
        Content->AddChildToVerticalBox(Button_Continue);
    }

    Button_Continue->OnClicked.AddUniqueDynamic(this, &UEncounterResultWidget::HandleContinueClicked);
}

void UEncounterResultWidget::ShowResult(ECombatResult Result, const FText& Message)
{
    const bool bVictory = Result == ECombatResult::Victory;
    Button_Continue->SetIsEnabled(bVictory);
    Button_Continue->SetVisibility(ESlateVisibility::Collapsed);
    Text_Result->SetText(FText::FromString(TEXT("Defeat / 패배\nThe run has ended. / 진행이 종료되었습니다.")));

    if (bVictory)
    {
        Button_Continue->SetVisibility(ESlateVisibility::Visible);
        Text_Result->SetText(FText::FromString(TEXT("Victory / 승리\nEncounter complete. / 전투를 완료했습니다.")));
    }
    if (!Message.IsEmpty())
    {
        Text_Result->SetText(FText::FromString(Text_Result->GetText().ToString() + TEXT("\n") + Message.ToString()));
    }
}

void UEncounterResultWidget::HandleContinueClicked()
{
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestContinueRun();
    }
}
