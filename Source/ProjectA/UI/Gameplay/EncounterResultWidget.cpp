#include "UI/Gameplay/EncounterResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/EngineBaseTypes.h"
#include "UI/Theme/DemonicUITheme.h"

TOptional<FUIInputConfig> UEncounterResultWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UEncounterResultWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("RootOverlay")));
    UBorder* Background = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Background")));
    UVerticalBox* Content = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ContentBox")));

    if (!Text_Result || !Button_Continue)
    {
        Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = Root;
        Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
        Root->AddChildToOverlay(Background);
        Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
        Root->AddChildToOverlay(Content);
        Text_Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Result"));
        Button_Continue = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Continue"));
        UTextBlock* ContinueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Continue"));
        ContinueText->SetText(FText::FromString(TEXT("Continue / 계속")));
        UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button_Continue->AddChild(ContinueText));
        ButtonSlot->SetPadding(FMargin(24.0f, 12.0f));
        Content->AddChildToVerticalBox(Text_Result);
        Theme.AddDivider(WidgetTree, Content);
        Content->AddChildToVerticalBox(Button_Continue)->SetPadding(FMargin(48.0f, 12.0f, 48.0f, 0.0f));
    }

    if (Root && Background && Background->GetParent() == Root)
    {
        Theme.StyleBackdrop(Background);
        UOverlaySlot* BackgroundSlot = CastChecked<UOverlaySlot>(Background->Slot);
        BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
        BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // Frame the known scaffold while retaining its bound widgets and any custom layouts.
    // 바인딩된 위젯과 별도 사용자 레이아웃을 유지하며 알려진 생성 구조만 프레임으로 감쌉니다.
    if (Root && Content && Content->GetParent() == Root)
    {
        Content->RemoveFromParent();
        UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultPanel"));
        Theme.StylePanel(Panel);
        Panel->SetPadding(FMargin(40.0f));
        UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Panel);
        ContentSlot->SetHorizontalAlignment(HAlign_Center);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
        ContentSlot->SetPadding(FMargin(24.0f));
        USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>();
        ContentSize->SetMinDesiredWidth(540.0f);
        ContentSize->SetMaxDesiredWidth(640.0f);
        Panel->SetContent(ContentSize);
        ContentSize->SetContent(Content);
        Text_Result->SetJustification(ETextJustify::Center);
        Text_Result->SetAutoWrapText(true);
        Text_Result->SetWrapTextAt(540.0f);
    }

    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Text_Result, true, 26);
    Theme.StyleButton(Button_Continue, true);
    Button_Continue->OnClicked.AddUniqueDynamic(this, &UEncounterResultWidget::HandleContinueClicked);
}

void UEncounterResultWidget::ShowResult(ECombatResult Result, const FText& Message)
{
    DisplayedResult = Result;
    const bool bVictory = Result == ECombatResult::Victory;
    Button_Continue->SetIsEnabled(bVictory && bContinueAllowed);
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

void UEncounterResultWidget::SetContinueEnabled(bool bEnabled)
{
    bContinueAllowed = bEnabled;
    if (Button_Continue)
    {
        Button_Continue->SetIsEnabled(bContinueAllowed && DisplayedResult == ECombatResult::Victory);
    }
}

void UEncounterResultWidget::HandleContinueClicked()
{
    if (!bContinueAllowed || DisplayedResult != ECombatResult::Victory)
    {
        return;
    }
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestContinueRun();
    }
}
