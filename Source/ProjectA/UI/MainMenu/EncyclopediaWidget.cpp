#include "UI/MainMenu/EncyclopediaWidget.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Modules/ModuleManager.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/Theme/DemonicUITheme.h"

void UEncyclopediaChoiceButton::BindSelection()
{
    OnClicked.AddUniqueDynamic(this, &UEncyclopediaChoiceButton::HandleClicked);
}

void UEncyclopediaChoiceButton::HandleClicked()
{
    Selected.ExecuteIfBound();
}

UEncyclopediaWidget::UEncyclopediaWidget()
{
    bIsBackHandler = true;
    bAutoRestoreFocus = true;
}

TOptional<FUIInputConfig> UEncyclopediaWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UTextBlock* UEncyclopediaWidget::MakeText(const FText& Text, int32 FontSize, bool bHeading)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    Label->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    UDemonicUITheme::Get().StyleText(Label, bHeading, FontSize);
    return Label;
}

UEncyclopediaChoiceButton* UEncyclopediaWidget::MakeButton(const FText& Text)
{
    UEncyclopediaChoiceButton* Button = WidgetTree->ConstructWidget<UEncyclopediaChoiceButton>();
    UTextBlock* Label = MakeText(Text, 16);
    CastChecked<UButtonSlot>(Button->AddChild(Label))->SetPadding(FMargin(12.0f, 10.0f));
    Button->BindSelection();
    UDemonicUITheme::Get().StyleButton(Button);
    return Button;
}

void UEncyclopediaWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleBackdrop(Background);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(1180.0f);
    Size->SetHeightOverride(760.0f);
    UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    Fit->SetContent(Size);
    UOverlaySlot* FitSlot = Root->AddChildToOverlay(Fit);
    FitSlot->SetHorizontalAlignment(HAlign_Fill);
    FitSlot->SetVerticalAlignment(VAlign_Fill);
    FitSlot->SetPadding(FMargin(24.0f));
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
    Theme.StylePanel(Frame);
    Frame->SetPadding(FMargin(32.0f));
    Size->SetContent(Frame);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Frame->SetContent(Content);
    Content->AddChildToVerticalBox(MakeText(NSLOCTEXT("Encyclopedia", "Title", "백과사전"), 30, true))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    Content->AddChildToVerticalBox(MakeText(NSLOCTEXT("Encyclopedia", "Scope", "현재 콘텐츠와 새 Run의 기본 규칙을 안내합니다. 이어하기의 저장 수치와 실제 전투 조건은 다를 수 있습니다."), 14))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    UHorizontalBox* TabBar = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(TabBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    for (int32 Index = 0; Index < static_cast<int32>(EEncyclopediaCategory::Count); ++Index)
    {
        UEncyclopediaChoiceButton* Tab = MakeButton(EncyclopediaCatalog::CategoryName(static_cast<EEncyclopediaCategory>(Index)));
        Tab->Selected.BindUObject(this, &UEncyclopediaWidget::SelectCategory, Index);
        UHorizontalBoxSlot* TabSlot = TabBar->AddChildToHorizontalBox(Tab);
        TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TabSlot->SetPadding(FMargin(2.0f, 0.0f));
        Tabs.Add(Tab);
    }
    Search = WidgetTree->ConstructWidget<UEditableTextBox>();
    Search->SetHintText(NSLOCTEXT("Encyclopedia", "SearchHint", "현재 탭에서 이름 또는 설명 검색"));
    Search->OnTextChanged.AddUniqueDynamic(this, &UEncyclopediaWidget::HandleSearchChanged);
    Content->AddChildToVerticalBox(Search)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    Count = MakeText(FText::GetEmpty(), 14);
    Content->AddChildToVerticalBox(Count)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
    ListSize->SetWidthOverride(300.0f);
    Columns->AddChildToHorizontalBox(ListSize)->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
    List = WidgetTree->ConstructWidget<UScrollBox>();
    List->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
    List->SetNavigationDestination(EDescendantScrollDestination::IntoView);
    ListSize->SetContent(List);
    UBorder* DetailFrame = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(DetailFrame);
    DetailFrame->SetPadding(FMargin(20.0f));
    Columns->AddChildToHorizontalBox(DetailFrame)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Details = WidgetTree->ConstructWidget<UScrollBox>();
    DetailFrame->SetContent(Details);
    UVerticalBox* DetailContent = WidgetTree->ConstructWidget<UVerticalBox>();
    Details->AddChild(DetailContent);
    DetailTitle = MakeText(FText::GetEmpty(), 24, true);
    DetailContent->AddChildToVerticalBox(DetailTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
    DetailBody = MakeText(FText::GetEmpty(), 18);
    DetailContent->AddChildToVerticalBox(DetailBody);
    Back = MakeButton(NSLOCTEXT("Encyclopedia", "Back", "메인메뉴로 돌아가기"));
    Back->Selected.BindUObject(this, &UEncyclopediaWidget::HandleBack);
    Content->AddChildToVerticalBox(Back)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
    Theme.ApplyControls(WidgetTree);
    SelectCategory(static_cast<int32>(ActiveCategory));
}

void UEncyclopediaWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (UMainMenuRootWidget* Root = Controller->GetMainMenuRootWidget()) Root->SetMainStackHiddenByMenu(true);
    }
    BeginLoad();
}

void UEncyclopediaWidget::BeginLoad()
{
    StopLoad();
    bLoading = true;
    bLoadedRuleDependencies = false;
    Entries.Reset();
    RefreshEntries();
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets()) RegistryLoadedHandle = Registry.OnFilesLoaded().AddUObject(this, &UEncyclopediaWidget::LoadDefinitions);
    else LoadDefinitions();
}

void UEncyclopediaWidget::LoadDefinitions()
{
    if (!bLoading || !IsActivated()) return;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (RegistryLoadedHandle.IsValid()) Registry.OnFilesLoaded().Remove(RegistryLoadedHandle);
    RegistryLoadedHandle.Reset();
    EncyclopediaCatalog::GetSkillPaths(SkillPaths);
    URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    Party = Run ? Run->PartyDefinition : nullptr;
    TArray<FSoftObjectPath> Paths = SkillPaths;
    Paths.AddUnique(Party ? FSoftObjectPath(Party) : EncyclopediaCatalog::DefaultPartyPath());
    // Keep the menu responsive while definition packages load, without spawning units or abilities.
    // 유닛이나 어빌리티를 생성하지 않고 정의 패키지를 불러오는 동안 메뉴 응답을 유지합니다.
    LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &UEncyclopediaWidget::FinishLoad));
    if (!LoadHandle) FinishLoad();
}

void UEncyclopediaWidget::FinishLoad()
{
    if (!bLoading || !IsActivated()) return;
    if (!Party) Party = Cast<UPartyDefinitionDataAsset>(EncyclopediaCatalog::DefaultPartyPath().ResolveObject());
    if (!bLoadedRuleDependencies)
    {
        bLoadedRuleDependencies = true;
        TArray<FSoftObjectPath> Paths = SkillPaths;
        const URunWeaponSkillRulesDataAsset* Rules = Party && Party->WeaponSkillRules ? Party->WeaponSkillRules.Get() : GetDefault<URunWeaponSkillRulesDataAsset>();
        bool bNeedsLoad = false;
        for (const FRunWeaponSkillCandidate& Candidate : Rules->Rules.Candidates)
        {
            if (!Candidate.Skill.IsValid()) continue;
            bNeedsLoad |= !Candidate.Skill.ResolveObject();
            Paths.AddUnique(Candidate.Skill);
            SkillPaths.AddUnique(Candidate.Skill);
        }
        if (bNeedsLoad)
        {
            // Retain the first batch while custom rule dependencies load and replace it with a complete handle.
            // 사용자 규칙 의존성을 읽는 동안 첫 묶음을 유지하고 전체 경로를 보존하는 핸들로 교체합니다.
            const TSharedPtr<FStreamableHandle> PreviousHandle = LoadHandle;
            if (Party) Paths.AddUnique(FSoftObjectPath(Party));
            LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &UEncyclopediaWidget::FinishLoad));
            if (LoadHandle) return;
        }
    }
    EncyclopediaCatalog::Build(Party, SkillPaths, Entries);
    bLoading = false;
    RefreshEntries();
}

void UEncyclopediaWidget::StopLoad()
{
    bLoading = false;
    if (RegistryLoadedHandle.IsValid())
    {
        if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"))) Module->Get().OnFilesLoaded().Remove(RegistryLoadedHandle);
        RegistryLoadedHandle.Reset();
    }
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
}

void UEncyclopediaWidget::NativeOnDeactivated()
{
    StopLoad();
    Party = nullptr;
    Super::NativeOnDeactivated();
}

void UEncyclopediaWidget::NativeDestruct()
{
    StopLoad();
    Super::NativeDestruct();
}

void UEncyclopediaWidget::SelectCategory(int32 Index)
{
    if (Index < 0 || Index >= static_cast<int32>(EEncyclopediaCategory::Count)) return;
    ActiveCategory = static_cast<EEncyclopediaCategory>(Index);
    SelectedIndex = INDEX_NONE;
    for (int32 TabIndex = 0; TabIndex < Tabs.Num(); ++TabIndex) UDemonicUITheme::Get().StyleButton(Tabs[TabIndex], TabIndex == Index);
    RefreshEntries();
}

void UEncyclopediaWidget::RefreshEntries()
{
    if (!List || !Search) return;
    List->ClearChildren();
    List->ScrollToStart();
    Rows.Reset();
    VisibleIndices.Reset();
    const FString Query = Search->GetText().ToString().TrimStartAndEnd();
    int32 CategoryCount = 0;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        const FEncyclopediaEntry& Entry = Entries[Index];
        if (Entry.Category != ActiveCategory) continue;
        ++CategoryCount;
        if (!Query.IsEmpty() && !Entry.Title.ToString().Contains(Query) && !Entry.Body.ToString().Contains(Query)) continue;
        UEncyclopediaChoiceButton* Row = MakeButton(Entry.Title);
        Row->Selected.BindUObject(this, &UEncyclopediaWidget::SelectEntry, Index);
        CastChecked<UScrollBoxSlot>(List->AddChild(Row))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
        Rows.Add(Row);
        VisibleIndices.Add(Index);
    }
    Count->SetText(bLoading ? NSLOCTEXT("Encyclopedia", "Loading", "백과사전 데이터를 불러오는 중…") : FText::Format(NSLOCTEXT("Encyclopedia", "ResultCount", "{0} · {1} / {2}개 항목"), EncyclopediaCatalog::CategoryName(ActiveCategory), FText::AsNumber(Rows.Num()), FText::AsNumber(CategoryCount)));
    if (!VisibleIndices.IsEmpty()) SelectEntry(VisibleIndices.Contains(SelectedIndex) ? SelectedIndex : VisibleIndices[0]);
    else
    {
        SelectedIndex = INDEX_NONE;
        DetailTitle->SetText(bLoading ? NSLOCTEXT("Encyclopedia", "LoadingTitle", "잠시 기다려 주세요") : NSLOCTEXT("Encyclopedia", "NoResults", "검색 결과가 없습니다"));
        DetailBody->SetText(bLoading ? FText::GetEmpty() : NSLOCTEXT("Encyclopedia", "SearchHelp", "검색어를 줄이거나 다른 탭을 선택해 주세요."));
    }
}

void UEncyclopediaWidget::SelectEntry(int32 Index)
{
    if (!Entries.IsValidIndex(Index) || !VisibleIndices.Contains(Index)) return;
    SelectedIndex = Index;
    DetailTitle->SetText(Entries[Index].Title);
    DetailBody->SetText(Entries[Index].Body);
    Details->ScrollToStart();
    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex) UDemonicUITheme::Get().StyleButton(Rows[RowIndex], VisibleIndices[RowIndex] == Index);
}

void UEncyclopediaWidget::HandleSearchChanged(const FText& Text)
{
    RefreshEntries();
}

UWidget* UEncyclopediaWidget::NativeGetDesiredFocusTarget() const
{
    return Tabs.IsEmpty() ? Back.Get() : Tabs[0].Get();
}

bool UEncyclopediaWidget::NativeOnHandleBackAction()
{
    HandleBack();
    return true;
}

void UEncyclopediaWidget::HandleBack()
{
    if (!IsActivated()) return;
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (UMainMenuRootWidget* Root = Controller->GetMainMenuRootWidget())
        {
            Root->ClearMenuStack();
            return;
        }
    }
    DeactivateWidget();
}
