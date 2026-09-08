#include "UI/Combat/CombatHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/CombatManager.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/Turn/TurnManager.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "Unit/UnitBase.h"

TOptional<FUIInputConfig> UCombatHUDWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false);
}

void UCombatHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!CommandPanelBackground || !Text_Turn || !Text_Action || !SkillList || !Button_Move || !Button_EndTurn || !Button_Cancel)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = Root;
        CommandPanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CommandPanelBackground"));
        CommandPanelBackground->SetBrushColor(FLinearColor(0.015f, 0.022f, 0.035f, 0.92f));
        CommandPanelBackground->SetPadding(FMargin(16.0f, 12.0f));
        UOverlaySlot* PanelSlot = Root->AddChildToOverlay(CommandPanelBackground);
        PanelSlot->SetHorizontalAlignment(HAlign_Center);
        PanelSlot->SetVerticalAlignment(VAlign_Bottom);
        PanelSlot->SetPadding(FMargin(20.0f));
        UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
        CommandPanelBackground->SetContent(Content);
        Text_Turn = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Turn"));
        Text_Action = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Action"));
        SkillList = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillList"));
        Content->AddChildToVerticalBox(Text_Turn);
        Content->AddChildToVerticalBox(Text_Action);
        Content->AddChildToVerticalBox(SkillList);
        UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Actions"));
        Content->AddChildToVerticalBox(Actions);

        const auto CreateAction = [this, Actions](FName Name, const TCHAR* Label)
        {
            UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
            Text->SetText(FText::FromString(Label));
            UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(Text));
            ButtonSlot->SetPadding(FMargin(20.0f, 12.0f));
            Actions->AddChildToHorizontalBox(Button);
            return Button;
        };

        Button_Move = CreateAction(TEXT("Button_Move"), TEXT("Move / 이동"));
        Button_EndTurn = CreateAction(TEXT("Button_EndTurn"), TEXT("End Turn / 턴 종료"));
        Button_Cancel = CreateAction(TEXT("Button_Cancel"), TEXT("Cancel / 취소"));
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Button_Move->OnClicked.AddUniqueDynamic(this, &UCombatHUDWidget::HandleMoveClicked);
    Button_EndTurn->OnClicked.AddUniqueDynamic(this, &UCombatHUDWidget::HandleEndTurnClicked);
    Button_Cancel->OnClicked.AddUniqueDynamic(this, &UCombatHUDWidget::HandleCancelClicked);
}

void UCombatHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshControls();
}

void UCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshControls();
}

void UCombatHUDWidget::RefreshControls()
{
    APartyPlayerController* Controller = Cast<APartyPlayerController>(GetOwningPlayer());

    if (!Controller)
    {
        return;
    }

    CachedCombatManager = Controller->GetCombatManager();
    AUnitBase* ActiveUnit = Controller->GetActiveUnit();

    if (DisplayedUnit.Get() != ActiveUnit)
    {
        DisplayedUnit = ActiveUnit;
        RebuildSkills(ActiveUnit);
    }

    Text_Turn->SetText(GetTurnInfoText());
    Text_Action->SetText(FText::FromString(TEXT("Waiting for combat. / 전투 대기 중")));

    if (ActiveUnit && ActiveUnit->GetAttributeSet())
    {
        Text_Action->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f | AP %d | Move AP %d | Select an action, then a tile. / 행동 선택 후 타일 클릭"), ActiveUnit->GetAttributeSet()->GetHP(), ActiveUnit->GetCurrentActionPoint(), ActiveUnit->GetCurrentSubActionPoint())));
    }

    Button_Move->SetIsEnabled(Controller->CanUseActiveUnitSubActionPoint(1));
    Button_EndTurn->SetIsEnabled(Controller->CanUseActiveUnitAction());
    Button_Cancel->SetIsEnabled(Controller->GetTileInputMode() != ETileInputMode::None && Controller->CanUseActiveUnitAction());

    for (const TPair<FName, TObjectPtr<UGameplayActionButton>>& Pair : SkillButtons)
    {
        USkillDefinitionDataAsset* Skill = DisplayedSkills.FindRef(Pair.Key);
        Pair.Value->SetIsEnabled(Skill && Controller->CanUseActiveUnitActionPoint(Skill->ActionPointCost));
    }
}

void UCombatHUDWidget::RebuildSkills(AUnitBase* ActiveUnit)
{
    SkillList->ClearChildren();
    DisplayedSkills.Reset();
    SkillButtons.Reset();

    if (!ActiveUnit || ActiveUnit->GetTeam() != ETeam::Player)
    {
        return;
    }

    int32 SkillIndex = 0;

    for (TSubclassOf<UGameplayAbility> AbilityClass : ActiveUnit->GetAvailableSkillAbilityClasses())
    {
        USkillDefinitionDataAsset* Skill = ActiveUnit->FindSkillDataByAbilityClass(AbilityClass);

        if (!Skill)
        {
            continue;
        }

        const FName ActionId(*FString::Printf(TEXT("Skill_%d"), SkillIndex++));
        FText Label = Skill->SkillName;

        if (Label.IsEmpty())
        {
            Label = FText::FromName(Skill->SkillId);
        }

        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
        Button->Configure(ActionId, Label);
        Button->OnActionRequested.AddUObject(this, &UCombatHUDWidget::HandleSkillSelected);
        SkillList->AddChildToHorizontalBox(Button);
        DisplayedSkills.Add(ActionId, Skill);
        SkillButtons.Add(ActionId, Button);
    }
}

FText UCombatHUDWidget::GetTurnInfoText() const
{
    if (!CachedCombatManager.IsValid() || !CachedCombatManager->GetTurnManager())
    {
        return FText::FromString(TEXT("TURN: 0  Current Unit: None"));
    }

    UTurnManager* TurnManager = CachedCombatManager->GetTurnManager();
    return FText::FromString(FString::Printf(TEXT("TURN: %d  Current Unit: %s"), TurnManager->GetTurnCounter(), *TurnManager->GetCurrentUnitName()));
}

void UCombatHUDWidget::HandleSkillSelected(FName SkillId)
{
    if (APartyPlayerController* Controller = Cast<APartyPlayerController>(GetOwningPlayer()))
    {
        Controller->EnterSkillMode(DisplayedSkills.FindRef(SkillId));
    }
}

void UCombatHUDWidget::HandleMoveClicked()
{
    if (APartyPlayerController* Controller = Cast<APartyPlayerController>(GetOwningPlayer()))
    {
        Controller->EnterMoveMode();
    }
}

void UCombatHUDWidget::HandleEndTurnClicked()
{
    if (APartyPlayerController* Controller = Cast<APartyPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestEndTurn();
    }
}

void UCombatHUDWidget::HandleCancelClicked()
{
    if (APartyPlayerController* Controller = Cast<APartyPlayerController>(GetOwningPlayer()))
    {
        Controller->CancelTileInputMode();
    }
}
