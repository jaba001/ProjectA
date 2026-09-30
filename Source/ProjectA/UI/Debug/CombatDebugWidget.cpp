#include "UI/Debug/CombatDebugWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DefaultValueHelper.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Theme/DemonicUITheme.h"
#include "Unit/UnitBase.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace
{
    struct FDebugSkillCategory
    {
        const TCHAR* Label;
        FGameplayTag Element;
        int32 MinimumElements = 0;
        int32 MaximumElements = MAX_int32;
    };

    const TArray<FDebugSkillCategory>& GetDebugSkillCategories()
    {
        static const TArray<FDebugSkillCategory> Categories =
        {
            { TEXT("전체"), FGameplayTag() },
            { TEXT("물리"), ProjectACombatTags::Skill_Element_Physical },
            { TEXT("화염"), ProjectACombatTags::Skill_Element_Fire },
            { TEXT("냉기"), ProjectACombatTags::Skill_Element_Cold },
            { TEXT("번개"), ProjectACombatTags::Skill_Element_Lightning },
            { TEXT("카오스"), ProjectACombatTags::Skill_Element_Chaos },
            { TEXT("복합"), FGameplayTag(), 2 },
            { TEXT("미분류"), FGameplayTag(), 0, 0 }
        };
        return Categories;
    }

    // Match resolved element tags without inferring combat properties from display names or asset paths.
    // 표시명이나 에셋 경로로 전투 속성을 추론하지 않고 해석된 속성 태그로 분류합니다.
    bool MatchesDebugSkillCategory(const FGameplayTagContainer& Tags, const FDebugSkillCategory& Category)
    {
        if (Category.Element.IsValid()) return Tags.HasTag(Category.Element);
        int32 ElementCount = 0;
        for (const FDebugSkillCategory& ElementCategory : GetDebugSkillCategories())
        {
            if (ElementCategory.Element.IsValid() && Tags.HasTag(ElementCategory.Element)) ++ElementCount;
        }
        return ElementCount >= Category.MinimumElements && ElementCount <= Category.MaximumElements;
    }

    struct FDebugSkillMethod
    {
        const TCHAR* Label;
        const TCHAR* Tooltip;
        FGameplayTagQuery Query;
    };

    FGameplayTagQuery MakeDebugSkillMethodQuery(const TArray<FGameplayTag>& Included, const TArray<FGameplayTag>& Excluded)
    {
        FGameplayTagQueryExpression Root;
        Root.AllExprMatch();
        if (!Included.IsEmpty())
        {
            FGameplayTagQueryExpression Any;
            Any.AnyTagsMatch();
            for (FGameplayTag Tag : Included) Any.AddTag(Tag);
            Root.AddExpr(Any);
        }
        if (!Excluded.IsEmpty())
        {
            FGameplayTagQueryExpression None;
            None.NoTagsMatch();
            for (FGameplayTag Tag : Excluded) None.AddTag(Tag);
            Root.AddExpr(None);
        }
        return FGameplayTagQuery::BuildQuery(Root);
    }

    // Support effects take precedence over their area shape; beam attacks remain area skills.
    // 지원 효과는 범위 형태보다 우선하며 빔 공격은 범위형 스킬로 분류합니다.
    const TArray<FDebugSkillMethod>& GetDebugSkillMethods()
    {
        static const TArray<FDebugSkillMethod> Methods = []
        {
            const TArray<FGameplayTag> Support = { ProjectACombatTags::Skill_Effect_Heal, ProjectACombatTags::Skill_Effect_Shield };
            TArray<FGameplayTag> Classified = Support;
            Classified.Append({ ProjectACombatTags::Skill_Shape_Projectile, ProjectACombatTags::Skill_Shape_Area, ProjectACombatTags::Skill_Shape_Beam, ProjectACombatTags::Skill_Shape_Slash });
            return TArray<FDebugSkillMethod>
            {
                { TEXT("전체"), TEXT("모든 방식의 스킬 · 선택한 속성과 검색 조건 적용"), FGameplayTagQuery() },
                { TEXT("투사체"), TEXT("투사체 형태의 공격 스킬"), MakeDebugSkillMethodQuery({ ProjectACombatTags::Skill_Shape_Projectile }, Support) },
                { TEXT("범위형"), TEXT("범위·직선 빔 공격 스킬 · 치유·보호막 제외"), MakeDebugSkillMethodQuery({ ProjectACombatTags::Skill_Shape_Area, ProjectACombatTags::Skill_Shape_Beam }, Support) },
                { TEXT("근접공격"), TEXT("베기·회전 공격 스킬 · 여러 적을 공격할 수 있음"), MakeDebugSkillMethodQuery({ ProjectACombatTags::Skill_Shape_Slash }, Support) },
                { TEXT("지원형"), TEXT("치유·보호막 스킬"), MakeDebugSkillMethodQuery(Support, {}) },
                { TEXT("미분류"), TEXT("방식·지원 효과 분류 태그가 없는 스킬 · CSV의 보류 에셋과 별개"), MakeDebugSkillMethodQuery({}, Classified) }
            };
        }();
        return Methods;
    }

    bool MatchesDebugSkillMethod(const FGameplayTagContainer& Tags, const FDebugSkillMethod& Method)
    {
        return Method.Query.IsEmpty() || Method.Query.Matches(Tags);
    }

    FString GetDebugUnitDisplayName(const AUnitBase* Unit)
    {
        if (!IsValid(Unit)) return FString();
        if (!Unit->RuntimeCharacterName.IsEmpty()) return Unit->RuntimeCharacterName.ToString();
        FString Name = Unit->GetClass()->GetName();
        Name.RemoveFromEnd(TEXT("_C"));
        return Name;
    }
}

void UCombatDebugActionButton::Initialize(ECombatDebugAction InAction, const FSoftObjectPath& InAsset, int32 InIndex, FGameplayTag InSlot)
{
    Action = InAction;
    Asset = InAsset;
    Index = InIndex;
    EquipmentSlot = InSlot;
    OnClicked.AddUniqueDynamic(this, &UCombatDebugActionButton::HandleClicked);
}

void UCombatDebugActionButton::HandleClicked()
{
    OnAction.Broadcast(this);
}

UTextBlock* UCombatDebugWidget::AddText(UVerticalBox* Box, const FString& Text, int32 Size)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, Size >= 18, Size);
    Box->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 3.f));
    return Label;
}

UButton* UCombatDebugWidget::AddButton(UVerticalBox* Box, const FString& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    UDemonicUITheme::Get().StyleText(Label, false, 16);
    UDemonicUITheme::Get().StyleButton(Button);
    Button->SetContent(Label);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 2.f));
    return Button;
}

void UCombatDebugWidget::AddAction(UVerticalBox* Box, const FString& Label, ECombatDebugAction Action, const FSoftObjectPath& Asset, int32 Index, FGameplayTag EquipmentSlot)
{
    UCombatDebugActionButton* Button = WidgetTree->ConstructWidget<UCombatDebugActionButton>();
    Button->Initialize(Action, Asset, Index, EquipmentSlot);
    Button->OnAction.AddUObject(this, &UCombatDebugWidget::HandleAction);
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Text, false, 15);
    UDemonicUITheme::Get().StyleButton(Button);
    Button->SetContent(Text);
    if (Asset.IsValid()) Button->SetToolTipText(FText::FromString(Asset.ToString()));
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 2.f));
}

void UCombatDebugWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    CombatLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
    UOverlaySlot* CombatSlot = Root->AddChildToOverlay(CombatLayer);
    CombatSlot->SetHorizontalAlignment(HAlign_Fill);
    CombatSlot->SetVerticalAlignment(VAlign_Fill);

    UVerticalBox* Tools = WidgetTree->ConstructWidget<UVerticalBox>();
    UOverlaySlot* ToolsSlot = Root->AddChildToOverlay(Tools);
    ToolsSlot->SetHorizontalAlignment(HAlign_Left);
    ToolsSlot->SetVerticalAlignment(VAlign_Top);
    ToolsSlot->SetPadding(FMargin(24.f, 16.f));
    USizeBox* ToolWidth = WidgetTree->ConstructWidget<USizeBox>();
    ToolWidth->SetWidthOverride(400.f);
    UVerticalBox* Header = WidgetTree->ConstructWidget<UVerticalBox>();
    ToolWidth->SetContent(Header);
    Tools->AddChildToVerticalBox(ToolWidth);
    AddButton(Header, TEXT("디버그 도구 열기 / 접기"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::TogglePanel);
    PanelSize = WidgetTree->ConstructWidget<USizeBox>();
    PanelSize->SetWidthOverride(920.f);
    PanelSize->SetHeightOverride(700.f);
    Tools->AddChildToVerticalBox(PanelSize);
    Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.016f, 0.022f, 0.97f), 8.f));
    Panel->SetPadding(FMargin(10.f));
    PanelSize->SetContent(Panel);
    UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Body);
    AddText(Body, TEXT("전투 디버그 · 저장하지 않음"), 18);
    AddText(Body, TEXT("대상 캐릭터 · 아군은 스킬·장비·체력, 적군은 체력 설정"));
    UnitChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    Body->AddChildToVerticalBox(UnitChoice);
    UnitChoice->OnSelectionChanged.AddDynamic(this, &UCombatDebugWidget::HandleUnit);
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    Body->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 6.f));
    UVerticalBox* SkillTab = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* EquipmentTab = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* UnitTab = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* TimingTab = WidgetTree->ConstructWidget<UVerticalBox>();
    Tabs->AddChildToHorizontalBox(SkillTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Tabs->AddChildToHorizontalBox(EquipmentTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Tabs->AddChildToHorizontalBox(UnitTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Tabs->AddChildToHorizontalBox(TimingTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AddButton(SkillTab, TEXT("스킬 구입"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::ShowSkills);
    AddButton(EquipmentTab, TEXT("장비"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::ShowEquipment);
    AddButton(UnitTab, TEXT("캐릭터·체력"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::ShowUnitTools);
    AddButton(TimingTab, TEXT("스킬 타이밍"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::ShowSkillTiming);

    LoadoutPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    Body->AddChildToVerticalBox(LoadoutPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    // Scroll wrapped filters with the lists to keep the catalog reachable in small viewports.
    // 작은 화면에서도 목록에 접근할 수 있도록 줄바꿈된 분류 영역을 목록과 함께 스크롤합니다.
    UScrollBox* LoadoutScroll = WidgetTree->ConstructWidget<UScrollBox>();
    LoadoutPanel->AddChildToVerticalBox(LoadoutScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* LoadoutBody = WidgetTree->ConstructWidget<UVerticalBox>();
    LoadoutScroll->AddChild(LoadoutBody);
    AddText(LoadoutBody, TEXT("계획 단계에서 무료 추가·제거 · 스킬 최대 5개 · 장비는 외형만 변경"));
    Status = AddText(LoadoutBody, TEXT("전투 준비 중"));
    SkillCategoryTabs = WidgetTree->ConstructWidget<UWrapBox>();
    SkillCategoryTabs->SetInnerSlotPadding(FVector2D(6.f, 4.f));
    LoadoutBody->AddChildToVerticalBox(SkillCategoryTabs)->SetPadding(FMargin(0.f, 4.f, 0.f, 6.f));
    UTextBlock* ElementLabel = WidgetTree->ConstructWidget<UTextBlock>();
    ElementLabel->SetText(FText::FromString(TEXT("속성")));
    UDemonicUITheme::Get().StyleText(ElementLabel, false, 14);
    SkillCategoryTabs->AddChildToWrapBox(ElementLabel);
    const TArray<FDebugSkillCategory>& Categories = GetDebugSkillCategories();
    for (int32 Index = 0; Index < Categories.Num(); ++Index)
    {
        UCombatDebugActionButton* Button = WidgetTree->ConstructWidget<UCombatDebugActionButton>();
        Button->Initialize(ECombatDebugAction::SelectSkillCategory, FSoftObjectPath(), Index, FGameplayTag());
        Button->OnAction.AddUObject(this, &UCombatDebugWidget::HandleAction);
        Button->SetToolTipText(FText::FromString(Index == Categories.Num() - 1 ? TEXT("등록된 다섯 속성의 태그가 없는 스킬") : Index == Categories.Num() - 2 ? TEXT("두 가지 이상 속성 태그가 있는 스킬 · 해당 속성 탭에도 표시") : TEXT("선택한 방식과 검색 조건을 함께 적용하는 속성 필터")));
        Button->SetContent(WidgetTree->ConstructWidget<UTextBlock>());
        CastChecked<UButtonSlot>(Button->GetContent()->Slot)->SetPadding(FMargin(8.f, 4.f));
        SkillCategoryTabs->AddChildToWrapBox(Button);
        SkillCategoryButtons.Add(Button);
    }
    RefreshSkillCategories({});
    SkillMethodTabs = WidgetTree->ConstructWidget<UWrapBox>();
    SkillMethodTabs->SetInnerSlotPadding(FVector2D(6.f, 4.f));
    LoadoutBody->AddChildToVerticalBox(SkillMethodTabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    UTextBlock* MethodLabel = WidgetTree->ConstructWidget<UTextBlock>();
    MethodLabel->SetText(FText::FromString(TEXT("방식")));
    UDemonicUITheme::Get().StyleText(MethodLabel, false, 14);
    SkillMethodTabs->AddChildToWrapBox(MethodLabel);
    const TArray<FDebugSkillMethod>& Methods = GetDebugSkillMethods();
    for (int32 Index = 0; Index < Methods.Num(); ++Index)
    {
        UCombatDebugActionButton* Button = WidgetTree->ConstructWidget<UCombatDebugActionButton>();
        Button->Initialize(ECombatDebugAction::SelectSkillMethod, FSoftObjectPath(), Index, FGameplayTag());
        Button->OnAction.AddUObject(this, &UCombatDebugWidget::HandleAction);
        Button->SetToolTipText(FText::FromString(Methods[Index].Tooltip));
        Button->SetContent(WidgetTree->ConstructWidget<UTextBlock>());
        CastChecked<UButtonSlot>(Button->GetContent()->Slot)->SetPadding(FMargin(8.f, 4.f));
        SkillMethodTabs->AddChildToWrapBox(Button);
        SkillMethodButtons.Add(Button);
    }
    RefreshSkillMethods({});
    // Give owned items and the catalog separate scrolling space so the purchase list stays visible.
    // 구입 목록이 가려지지 않도록 보유 목록과 전체 목록에 독립적인 스크롤 공간을 제공합니다.
    UHorizontalBox* Lists = WidgetTree->ConstructWidget<UHorizontalBox>();
    LoadoutListBounds = WidgetTree->ConstructWidget<USizeBox>();
    LoadoutListBounds->SetHeightOverride(260.f);
    LoadoutListBounds->SetContent(Lists);
    LoadoutBody->AddChildToVerticalBox(LoadoutListBounds)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UScrollBox* OwnedScroll = WidgetTree->ConstructWidget<UScrollBox>();
    UHorizontalBoxSlot* OwnedSlot = Lists->AddChildToHorizontalBox(OwnedScroll);
    FSlateChildSize OwnedSize(ESlateSizeRule::Fill);
    OwnedSize.Value = 0.38f;
    OwnedSlot->SetSize(OwnedSize);
    OwnedSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    OwnedList = WidgetTree->ConstructWidget<UVerticalBox>();
    OwnedScroll->AddChild(OwnedList);
    UVerticalBox* CatalogBody = WidgetTree->ConstructWidget<UVerticalBox>();
    FSlateChildSize CatalogSize(ESlateSizeRule::Fill);
    CatalogSize.Value = 0.62f;
    Lists->AddChildToHorizontalBox(CatalogBody)->SetSize(CatalogSize);
    Search = WidgetTree->ConstructWidget<UEditableTextBox>();
    Search->SetHintText(FText::FromString(TEXT("이름 / 에셋 이름 검색")));
    CatalogBody->AddChildToVerticalBox(Search);
    Search->OnTextChanged.AddDynamic(this, &UCombatDebugWidget::HandleSearch);
    CatalogTitle = AddText(CatalogBody, TEXT("전체 목록"), 18);
    CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>();
    CatalogBody->AddChildToVerticalBox(CatalogScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CatalogList = WidgetTree->ConstructWidget<UVerticalBox>();
    CatalogScroll->AddChild(CatalogList);

    SkillTimingPanel = WidgetTree->ConstructWidget<UScrollBox>();
    SkillTimingPanel->SetVisibility(ESlateVisibility::Collapsed);
    Body->AddChildToVerticalBox(SkillTimingPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* TimingBody = WidgetTree->ConstructWidget<UVerticalBox>();
    SkillTimingPanel->AddChild(TimingBody);
    AddText(TimingBody, TEXT("선택 아군의 보유 스킬 타이밍"), 18);
    AddText(TimingBody, TEXT("같은 스킬을 쓰는 아군·적군 전체에 임시 적용합니다. 원본 에셋은 저장하지 않으며 전투 초기화 시 복원됩니다."));
    TimingSkillChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    TimingBody->AddChildToVerticalBox(TimingSkillChoice);
    TimingSkillChoice->OnSelectionChanged.AddDynamic(this, &UCombatDebugWidget::HandleTimingSkill);
    TimingAssetPath = AddText(TimingBody, TEXT("아군의 보유 스킬을 선택하세요."), 12);
    TimingOriginalValues = AddText(TimingBody, TEXT(""), 12);
    const auto AddTimingInput = [this, TimingBody](const TCHAR* Label)
    {
        AddText(TimingBody, Label);
        UEditableTextBox* Input = WidgetTree->ConstructWidget<UEditableTextBox>();
        Input->SetSelectAllTextWhenFocused(true);
        Input->SetIsEnabled(false);
        TimingBody->AddChildToVerticalBox(Input);
        return Input;
    };
    WindupInput = AddTimingInput(TEXT("시전 준비 · 초 (0~60)"));
    EffectDelayInput = AddTimingInput(TEXT("VFX 발생 뒤 판정 대기 · 초 (0~10, 효과 충돌 스킬)"));
    EffectDurationInput = AddTimingInput(TEXT("효과 충돌 판정 지속 · 초 (0 초과~10)"));
    ProjectileSpeedInput = AddTimingInput(TEXT("기본 투사체 속도 · cm/s (0 초과~100,000, 공통 배율 적용 전)"));
    WeaponDurationInput = AddTimingInput(TEXT("무기 궤적 판정 지속 · 초 (0 초과~5, 준비+지속 60 이하)"));
    AddText(TimingBody, TEXT("회색 항목은 이 스킬에서 사용하지 않습니다. 투사체 속도 변경은 수명을 바꾸지 않으므로 최대 도달거리도 바뀝니다. 효과 지속은 판정 시간이며 원본 VFX 재생 길이는 유지됩니다."), 12);
    TimingApplyButton = AddButton(TimingBody, TEXT("입력값 임시 적용"));
    TimingApplyButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ApplySkillTiming);
    TimingResetButton = AddButton(TimingBody, TEXT("원본값으로 복원"));
    TimingResetButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ResetSkillTiming);
    TimingCopyButton = AddButton(TimingBody, TEXT("에셋 경로 + 입력 설정값 복사"));
    TimingCopyButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::CopySkillTiming);
    TimingStatus = AddText(TimingBody, TEXT("전투 준비 중"), 13);

    UnitToolsPanel = WidgetTree->ConstructWidget<UScrollBox>();
    UnitToolsPanel->SetVisibility(ESlateVisibility::Collapsed);
    Body->AddChildToVerticalBox(UnitToolsPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* UnitBody = WidgetTree->ConstructWidget<UVerticalBox>();
    UnitToolsPanel->AddChild(UnitBody);
    AddText(UnitBody, TEXT("선택 캐릭터 체력 설정"), 18);
    HealthTarget = AddText(UnitBody, TEXT("위 목록에서 아군 또는 적군을 선택하세요."));
    UHorizontalBox* HealthFields = WidgetTree->ConstructWidget<UHorizontalBox>();
    UnitBody->AddChildToVerticalBox(HealthFields);
    UVerticalBox* MaxHealthBox = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* CurrentHealthBox = WidgetTree->ConstructWidget<UVerticalBox>();
    HealthFields->AddChildToHorizontalBox(MaxHealthBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UHorizontalBoxSlot* CurrentHealthSlot = HealthFields->AddChildToHorizontalBox(CurrentHealthBox);
    CurrentHealthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CurrentHealthSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    AddText(MaxHealthBox, TEXT("최대 HP · 1 ~ 1,000,000"));
    MaxHealthInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    MaxHealthInput->SetSelectAllTextWhenFocused(true);
    MaxHealthBox->AddChildToVerticalBox(MaxHealthInput);
    AddText(CurrentHealthBox, TEXT("현재 HP · 1 ~ 최대 HP"));
    CurrentHealthInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    CurrentHealthInput->SetSelectAllTextWhenFocused(true);
    CurrentHealthBox->AddChildToVerticalBox(CurrentHealthInput);
    HealthApplyButton = AddButton(MaxHealthBox, TEXT("입력한 최대·현재 HP 적용"));
    HealthApplyButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ApplyUnitHealth);
    HealButton = AddButton(CurrentHealthBox, TEXT("현재 최대 HP까지 전부 회복"));
    HealButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::HealSelectedUnit);
    HealthStatus = AddText(UnitBody, TEXT("전투 준비 중"));
    SpawnPanel = WidgetTree->ConstructWidget<UBorder>();
    SpawnPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f), 6.f));
    SpawnPanel->SetPadding(FMargin(8.f));
    UnitBody->AddChildToVerticalBox(SpawnPanel)->SetPadding(FMargin(0.f, 8.f));
    UVerticalBox* SpawnBody = WidgetTree->ConstructWidget<UVerticalBox>();
    SpawnPanel->SetContent(SpawnBody);
    AddText(SpawnBody, TEXT("아군·적군 캐릭터 생성"), 18);
    SpawnCount = AddText(SpawnBody, TEXT("전투 준비 중"));
    AddText(SpawnBody, TEXT("계획 단계 또는 전투 종료 후 추가할 수 있습니다. 기존 캐릭터의 스킬·장비는 유지합니다."), 13);
    UHorizontalBox* SpawnColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
    SpawnBody->AddChildToVerticalBox(SpawnColumns);
    UVerticalBox* AllyBody = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* EnemyBody = WidgetTree->ConstructWidget<UVerticalBox>();
    SpawnColumns->AddChildToHorizontalBox(AllyBody)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UHorizontalBoxSlot* EnemySlot = SpawnColumns->AddChildToHorizontalBox(EnemyBody);
    EnemySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    EnemySlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    AddText(AllyBody, TEXT("추가할 아군"));
    AllySpawnChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    AllyBody->AddChildToVerticalBox(AllySpawnChoice);
    AllySpawnButton = AddButton(AllyBody, TEXT("선택 캐릭터를 아군으로 추가"));
    AllySpawnButton->SetIsEnabled(false);
    AllySpawnButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::SpawnAlly);
    AllySpawnStatus = AddText(AllyBody, TEXT("전투 준비 중"), 13);
    AddText(EnemyBody, TEXT("추가할 적군"));
    EnemySpawnChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    EnemyBody->AddChildToVerticalBox(EnemySpawnChoice);
    EnemySpawnButton = AddButton(EnemyBody, TEXT("선택 캐릭터를 적군으로 추가"));
    EnemySpawnButton->SetIsEnabled(false);
    EnemySpawnButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::SpawnEnemy);
    EnemySpawnStatus = AddText(EnemyBody, TEXT("전투 준비 중"), 13);
    ReviveToggleButton = AddButton(UnitBody, TEXT("아군 부활 창 열기"));
    ReviveToggleButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ToggleRevivePanel);
    // Scroll character controls independently from the skill and equipment catalogs.
    // 캐릭터 조작 영역은 스킬·장비 목록과 독립적으로 스크롤합니다.
    RevivePanel = WidgetTree->ConstructWidget<UBorder>();
    RevivePanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f), 6.f));
    RevivePanel->SetPadding(FMargin(8.f));
    RevivePanel->SetVisibility(ESlateVisibility::Collapsed);
    UnitBody->AddChildToVerticalBox(RevivePanel)->SetPadding(FMargin(0.f, 3.f));
    UVerticalBox* ReviveBody = WidgetTree->ConstructWidget<UVerticalBox>();
    RevivePanel->SetContent(ReviveBody);
    ReviveTarget = AddText(ReviveBody, TEXT("위 목록에서 부활할 아군을 선택하세요."));
    ReviveButton = AddButton(ReviveBody, TEXT("선택 아군 부활 · HP 전부 회복"));
    ReviveButton->SetIsEnabled(false);
    ReviveButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ReviveSelectedAlly);
    ReviveStatus = AddText(ReviveBody, TEXT("전투 준비 중"), 13);
    AddButton(UnitBody, TEXT("전투 초기화 · 처음 장착으로 복원"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::RestartCombat);
    RefreshSkillTiming();
    UDemonicUITheme::Get().ApplyControls(WidgetTree);
}

void UCombatDebugWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Add content after the Slate switcher exists, and restore it after Slate resources are rebuilt.
    // Slate 스위처가 생성된 뒤 콘텐츠를 추가하고 Slate 리소스 재생성 후에도 복원합니다.
    if (CombatLayer && CombatLayer->GetNumWidgets() == 0) CombatLayer->AddWidget<UCombatRoundPlanningWidget>(UCombatRoundPlanningWidget::StaticClass());
}

void UCombatDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshElapsed += InDeltaTime;
    if (RefreshElapsed < 0.15f) return;
    RefreshElapsed = 0.f;
    PanelSize->SetWidthOverride(FMath::Clamp(static_cast<float>(MyGeometry.GetLocalSize().X) - 48.f, 260.f, 920.f));
    const float PanelHeight = FMath::Clamp(static_cast<float>(MyGeometry.GetLocalSize().Y) - 380.f, 260.f, 760.f);
    PanelSize->SetHeightOverride(PanelHeight);
    LoadoutListBounds->SetHeightOverride(FMath::Max(220.f, PanelHeight - 300.f));
    RefreshState();
}

void UCombatDebugWidget::RefreshState()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Round)
    {
        ObservedCombatId.Invalidate();
        ObservedRevision = INDEX_NONE;
        SelectedUnitId = INDEX_NONE;
        HealthInputUnitId = INDEX_NONE;
        bRefreshingUnits = true;
        UnitOptions.Reset();
        UnitChoice->ClearOptions();
        bRefreshingUnits = false;
        OwnedList->ClearChildren();
        CatalogList->ClearChildren();
        RefreshSkillCategories({});
        RefreshSkillMethods({});
        OwnedList->SetIsEnabled(false);
        CatalogList->SetIsEnabled(false);
        const ACombatDebugGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatDebugGameMode>();
        Status->SetText(Mode ? Mode->GetStatusMessage() : FText::FromString(TEXT("디버그 전투가 없습니다.")));
        ReviveMessage = FText::GetEmpty();
        AllySpawnMessage = FText::GetEmpty();
        EnemySpawnMessage = FText::GetEmpty();
        HealthMessage = FText::GetEmpty();
        TimingMessage = FText::GetEmpty();
        RefreshReviveState();
        RefreshSpawnState();
        RefreshHealthState();
        RefreshSkillTiming();
        return;
    }
    Controller->GetDebugLoadout();
    const FCombatRoundView& View = Round->GetView();
    if (ObservedCombatId != View.CombatId)
    {
        ObservedCombatId = View.CombatId;
        ObservedRevision = INDEX_NONE;
        SelectedUnitId = INDEX_NONE;
        HealthInputUnitId = INDEX_NONE;
        TimingInputUnitId = INDEX_NONE;
        SelectedTimingSkill = NAME_None;
        TimingInputSkill = NAME_None;
        ActionMessage = FText::GetEmpty();
        ReviveMessage = FText::GetEmpty();
        AllySpawnMessage = FText::GetEmpty();
        EnemySpawnMessage = FText::GetEmpty();
        HealthMessage = FText::GetEmpty();
        TimingMessage = FText::GetEmpty();
        RebuildSpawnOptions();
    }
    const bool bUnitsChanged = RefreshUnitOptions();
    if (bUnitsChanged || ObservedRevision != View.PlanRevision)
    {
        ObservedRevision = View.PlanRevision;
        RebuildLists();
    }
    FText Error;
    const bool bCanEdit = Round->CanEditDebugUnit(Controller, SelectedUnitId, Error);
    OwnedList->SetIsEnabled(bCanEdit);
    CatalogList->SetIsEnabled(bCanEdit);
    Status->SetText(bCanEdit ? (ActionMessage.IsEmpty() ? FText::FromString(TEXT("추가·제거 즉시 반영 / 해당 유닛의 계획 해제")) : ActionMessage) : Error);
    RefreshReviveState();
    RefreshSpawnState();
    RefreshHealthState();
    RefreshSkillTiming();
}

bool UCombatDebugWidget::RefreshUnitOptions()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Round) return false;
    TMap<FString, int32> AvailableOptions;
    TArray<FString> Labels;
    FString SelectedLabel;
    bool bChanged = false;
    for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
    {
        if (!IsValid(Unit.Unit) || (!Unit.bEnemy && Unit.OwnerSlot != Controller->GetRoundParticipantSlot())) continue;
        const FString Label = FString::Printf(TEXT("[%s] %d · %s%s"), Unit.bEnemy ? TEXT("적군") : TEXT("아군"), Unit.UnitId, *GetDebugUnitDisplayName(Unit.Unit), Unit.Unit->IsUnitAlive() ? TEXT("") : TEXT(" · 사망"));
        AvailableOptions.Add(Label, Unit.UnitId);
        Labels.Add(Label);
        const int32* PreviousId = UnitOptions.Find(Label);
        bChanged |= !PreviousId || *PreviousId != Unit.UnitId;
        if (Unit.UnitId == SelectedUnitId) SelectedLabel = Label;
    }
    bChanged |= AvailableOptions.Num() != UnitOptions.Num();
    if (!bChanged && !SelectedLabel.IsEmpty() && UnitChoice->GetSelectedOption() == SelectedLabel) return false;
    // Spawning changes the roster without creating another combat; retain either team's selected unit.
    // 캐릭터 추가는 새 전투를 만들지 않으므로 명단을 갱신하면서 양 진영의 선택 유닛을 유지합니다.
    bRefreshingUnits = true;
    UnitOptions = MoveTemp(AvailableOptions);
    UnitChoice->ClearOptions();
    for (const FString& Label : Labels) UnitChoice->AddOption(Label);
    if (SelectedLabel.IsEmpty() && !Labels.IsEmpty()) SelectedLabel = Labels[0];
    if (!SelectedLabel.IsEmpty()) UnitChoice->SetSelectedOption(SelectedLabel);
    const int32* NewSelectedId = UnitOptions.Find(SelectedLabel);
    SelectedUnitId = NewSelectedId ? *NewSelectedId : INDEX_NONE;
    bRefreshingUnits = false;
    return true;
}

void UCombatDebugWidget::RefreshReviveState()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundUnitView* Selected = Round ? Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; }) : nullptr;
    if (!Selected || !IsValid(Selected->Unit))
    {
        ReviveTarget->SetText(FText::FromString(TEXT("위 목록에서 부활할 아군을 선택하세요.")));
        ReviveButton->SetIsEnabled(false);
        ReviveStatus->SetText(FText::FromString(Round ? TEXT("선택한 아군이 없습니다.") : TEXT("디버그 전투 준비 후 사용할 수 있습니다.")));
        ReviveButton->SetToolTipText(ReviveStatus->GetText());
        return;
    }
    const UAS_Unit* Attributes = Selected->Unit->GetAttributeSet();
    const FString Health = Attributes ? FString::Printf(TEXT("HP %.0f / %.0f"), Attributes->GetHP(), Attributes->GetMaxHP()) : TEXT("HP 정보 없음");
    ReviveTarget->SetText(FText::FromString(FString::Printf(TEXT("부활 대상: %d · %s\n%s · %s"), Selected->UnitId, *GetDebugUnitDisplayName(Selected->Unit), *Health, Selected->Unit->IsUnitAlive() ? TEXT("생존") : TEXT("사망"))));
    FText Error;
    const bool bCanRevive = Round->CanReviveDebugUnit(Controller, SelectedUnitId, Error);
    ReviveButton->SetIsEnabled(bCanRevive);
    const FText Availability = bCanRevive ? FText::FromString(TEXT("선택 아군의 스킬·장비를 유지한 채 HP를 전부 회복하고 부활합니다.")) : Error;
    ReviveButton->SetToolTipText(Availability);
    // Preserve revival feedback separately from ordinary loadout-edit errors for dead or finished units.
    // 사망하거나 전투가 끝난 유닛의 일반 장착 편집 오류와 부활 결과를 별도로 유지합니다.
    ReviveStatus->SetText(ReviveMessage.IsEmpty() || ReviveMessage.EqualTo(Availability) ? Availability : bCanRevive ? ReviveMessage : FText::Format(FText::FromString(TEXT("{0}\n{1}")), ReviveMessage, Availability));
}

void UCombatDebugWidget::RefreshHealthState(bool bResetInput)
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundUnitView* Selected = Round ? Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; }) : nullptr;
    const UAS_Unit* Attributes = Selected && IsValid(Selected->Unit) ? Selected->Unit->GetAttributeSet() : nullptr;
    FText Error;
    const bool bCanEdit = Attributes && Round->CanSetDebugUnitHealth(Controller, SelectedUnitId, Error);
    HealthApplyButton->SetIsEnabled(bCanEdit);
    HealButton->SetIsEnabled(bCanEdit);
    MaxHealthInput->SetIsEnabled(bCanEdit);
    CurrentHealthInput->SetIsEnabled(bCanEdit);
    if (!Attributes)
    {
        HealthInputUnitId = INDEX_NONE;
        HealthTarget->SetText(FText::FromString(TEXT("위 목록에서 아군 또는 적군을 선택하세요.")));
        MaxHealthInput->SetText(FText::GetEmpty());
        CurrentHealthInput->SetText(FText::GetEmpty());
        HealthStatus->SetText(FText::FromString(TEXT("디버그 전투 준비 후 사용할 수 있습니다.")));
        return;
    }
    HealthTarget->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s · HP %s / %s · %s"), Selected->bEnemy ? TEXT("적군") : TEXT("아군"), *GetDebugUnitDisplayName(Selected->Unit), *FString::SanitizeFloat(Attributes->GetHP()), *FString::SanitizeFloat(Attributes->GetMaxHP()), Selected->Unit->IsUnitAlive() ? TEXT("생존") : TEXT("사망"))));
    // Refresh drafts only when changing targets or completing an explicit action, never on every UI tick.
    // UI 갱신마다 입력을 덮어쓰지 않고 대상 변경이나 명시적 조작 완료 시에만 입력값을 갱신합니다.
    if (bResetInput || HealthInputUnitId != SelectedUnitId)
    {
        HealthInputUnitId = SelectedUnitId;
        MaxHealthInput->SetText(FText::FromString(FString::SanitizeFloat(Attributes->GetMaxHP())));
        CurrentHealthInput->SetText(FText::FromString(FString::SanitizeFloat(Attributes->GetHP())));
    }
    const FText Availability = bCanEdit ? FText::FromString(TEXT("계획 단계에서 HP를 적용합니다. 스킬·장비·행동 계획은 유지하고 아군 준비 완료를 해제합니다.")) : Error;
    HealthApplyButton->SetToolTipText(Availability);
    HealButton->SetToolTipText(Availability);
    HealthStatus->SetText(HealthMessage.IsEmpty() ? Availability : FText::Format(FText::FromString(TEXT("{0}\n{1}")), HealthMessage, Availability));
}

void UCombatDebugWidget::ApplyHealth(bool bFullHeal)
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    FText Error;
    if (!Round || !Round->CanSetDebugUnitHealth(Controller, SelectedUnitId, Error))
    {
        HealthMessage = Error;
        RefreshHealthState();
        return;
    }
    float MaxHP = 0.f;
    float CurrentHP = 0.f;
    if (bFullHeal)
    {
        const FCombatRoundUnitView* Selected = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
        if (!Selected || !IsValid(Selected->Unit) || !Selected->Unit->GetAttributeSet()) return;
        MaxHP = Selected->Unit->GetAttributeSet()->GetMaxHP();
        CurrentHP = MaxHP;
    }
    else if (!FDefaultValueHelper::ParseFloat(MaxHealthInput->GetText().ToString().TrimStartAndEnd(), MaxHP) || !FDefaultValueHelper::ParseFloat(CurrentHealthInput->GetText().ToString().TrimStartAndEnd(), CurrentHP))
    {
        HealthMessage = FText::FromString(TEXT("최대 HP와 현재 HP에 쉼표 없이 올바른 숫자를 입력하세요."));
        RefreshHealthState();
        return;
    }
    const bool bSucceeded = Round->SetDebugUnitHealth(Controller, SelectedUnitId, MaxHP, CurrentHP, Error);
    HealthMessage = bSucceeded ? FText::FromString(bFullHeal ? TEXT("선택 캐릭터의 HP를 전부 회복했습니다.") : TEXT("최대 HP와 현재 HP를 적용했습니다.")) : Error;
    RefreshHealthState(bSucceeded);
    RefreshState();
}

void UCombatDebugWidget::RefreshSkillTiming(bool bResetInput)
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundUnitView* Selected = Round ? Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; }) : nullptr;
    TMap<FString, FName> AvailableOptions;
    TArray<FString> Labels;
    FString SelectedLabel;
    bool bOptionsChanged = false;
    if (Selected && !Selected->bEnemy && Selected->OwnerSlot == Round->GetParticipantSlot(Controller) && IsValid(Selected->Unit))
    {
        for (FName SkillId : Selected->SkillIds)
        {
            const FCombatRoundSkill* Skill = Round->FindSkill(SkillId);
            if (!Skill) continue;
            const FString Label = FString::Printf(TEXT("%d · %s"), Labels.Num() + 1, Skill->Name.IsEmpty() ? *SkillId.ToString() : *Skill->Name.ToString());
            Labels.Add(Label);
            AvailableOptions.Add(Label, SkillId);
            const FName* PreviousId = TimingSkillOptions.Find(Label);
            bOptionsChanged |= !PreviousId || *PreviousId != SkillId;
            if (SelectedTimingSkill == SkillId) SelectedLabel = Label;
        }
    }
    bOptionsChanged |= AvailableOptions.Num() != TimingSkillOptions.Num();
    if (SelectedLabel.IsEmpty() && !Labels.IsEmpty()) SelectedLabel = Labels[0];
    if (bOptionsChanged || TimingSkillChoice->GetSelectedOption() != SelectedLabel)
    {
        bRefreshingTimingSkills = true;
        TimingSkillOptions = MoveTemp(AvailableOptions);
        TimingSkillChoice->ClearOptions();
        for (const FString& Label : Labels) TimingSkillChoice->AddOption(Label);
        if (!SelectedLabel.IsEmpty()) TimingSkillChoice->SetSelectedOption(SelectedLabel);
        bRefreshingTimingSkills = false;
    }
    const FName* SelectedId = TimingSkillOptions.Find(SelectedLabel);
    SelectedTimingSkill = SelectedId ? *SelectedId : NAME_None;
    TimingSkillChoice->SetIsEnabled(!TimingSkillOptions.IsEmpty());
    FCombatDebugSkillTiming Current;
    FCombatDebugSkillTiming Original;
    FSoftObjectPath Asset;
    FText Error = FText::FromString(TEXT("위 대상 목록에서 아군을 고르고 보유 스킬을 선택하세요."));
    const bool bHasSkill = Round && !SelectedTimingSkill.IsNone() && Round->GetDebugSkillTiming(SelectedUnitId, SelectedTimingSkill, Current, Original, Asset, Error);
    const FCombatRoundSkill* Skill = bHasSkill ? Round->FindSkill(SelectedTimingSkill) : nullptr;
    const bool bCanEdit = Skill && Round->CanEditDebugUnit(Controller, SelectedUnitId, Error);
    WindupInput->SetIsEnabled(bCanEdit);
    EffectDelayInput->SetIsEnabled(bCanEdit && Skill->bUseEffectCollision);
    EffectDurationInput->SetIsEnabled(bCanEdit && Skill->bUseEffectCollision);
    ProjectileSpeedInput->SetIsEnabled(bCanEdit && Skill->Kind == ECombatRoundSkillKind::Projectile);
    WeaponDurationInput->SetIsEnabled(bCanEdit && Skill->bUseWeaponTrace);
    TimingApplyButton->SetIsEnabled(bCanEdit);
    TimingResetButton->SetIsEnabled(bCanEdit);
    TimingCopyButton->SetIsEnabled(bHasSkill);
    if (!bHasSkill)
    {
        TimingInputUnitId = INDEX_NONE;
        TimingInputSkill = NAME_None;
        TimingMessage = FText::GetEmpty();
        TimingAssetPath->SetText(FText::FromString(TEXT("아군의 보유 스킬을 선택하세요.")));
        TimingOriginalValues->SetText(FText::GetEmpty());
        for (UEditableTextBox* Input : { WindupInput.Get(), EffectDelayInput.Get(), EffectDurationInput.Get(), ProjectileSpeedInput.Get(), WeaponDurationInput.Get() }) Input->SetText(FText::GetEmpty());
        TimingStatus->SetText(Error);
        return;
    }
    TimingAssetPath->SetText(FText::FromString(Asset.ToString()));
    FString OriginalValues = FString::Printf(TEXT("원본값: 준비 %g초 · 효과 대기 %g초 / 지속 %g초 · 투사체 %gcm/s · 무기 판정 %g초"), Original.WindupSeconds, Original.EffectHitDelaySeconds, Original.EffectDuration, Original.ProjectileSpeed, Original.WeaponTraceDuration);
    if (Skill->Kind == ECombatRoundSkillKind::Projectile)
    {
        const IConsoleVariable* SpeedVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("projecta.Combat.ProjectileSpeedScale"));
        const float RequestedScale = SpeedVariable ? SpeedVariable->GetFloat() : 0.5f;
        const float SpeedScale = FMath::IsFinite(RequestedScale) ? FMath::Clamp(RequestedScale, 0.1f, 2.f) : 0.5f;
        OriginalValues += FString::Printf(TEXT("\n현재 전투 비행 속도: %g × %g = %gcm/s"), Current.ProjectileSpeed, SpeedScale, Current.ProjectileSpeed * SpeedScale);
    }
    TimingOriginalValues->SetText(FText::FromString(OriginalValues));
    // Keep drafts stable through polling; only a changed selection or a completed action reloads values.
    // 주기 갱신 중에는 초안을 유지하고 선택 변경 또는 조작 완료 시에만 값을 다시 읽습니다.
    if (bResetInput || TimingInputUnitId != SelectedUnitId || TimingInputSkill != SelectedTimingSkill)
    {
        if (TimingInputUnitId != SelectedUnitId || TimingInputSkill != SelectedTimingSkill) TimingMessage = FText::GetEmpty();
        TimingInputUnitId = SelectedUnitId;
        TimingInputSkill = SelectedTimingSkill;
        WindupInput->SetText(FText::FromString(FString::SanitizeFloat(Current.WindupSeconds)));
        EffectDelayInput->SetText(FText::FromString(FString::SanitizeFloat(Current.EffectHitDelaySeconds)));
        EffectDurationInput->SetText(FText::FromString(FString::SanitizeFloat(Current.EffectDuration)));
        ProjectileSpeedInput->SetText(FText::FromString(FString::SanitizeFloat(Current.ProjectileSpeed)));
        WeaponDurationInput->SetText(FText::FromString(FString::SanitizeFloat(Current.WeaponTraceDuration)));
    }
    const FText Availability = bCanEdit ? FText::FromString(TEXT("계획 단계에서 적용 · 같은 스킬 보유 아군의 계획/준비 해제 · 적 계획 유지")) : Error;
    TimingStatus->SetText(TimingMessage.IsEmpty() ? Availability : FText::Format(FText::FromString(TEXT("{0}\n{1}")), TimingMessage, Availability));
}

bool UCombatDebugWidget::ReadSkillTimingInputs(FCombatDebugSkillTiming& OutTiming, FText& OutError) const
{
    const ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    const ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    FCombatDebugSkillTiming Original;
    FSoftObjectPath Asset;
    if (!Round || !Round->GetDebugSkillTiming(SelectedUnitId, SelectedTimingSkill, OutTiming, Original, Asset, OutError)) return false;
    const FCombatRoundSkill* Skill = Round->FindSkill(SelectedTimingSkill);
    if (!Skill) return false;
    const auto Parse = [](const UEditableTextBox* Input, float& Value)
    {
        return FDefaultValueHelper::ParseFloat(Input->GetText().ToString().TrimStartAndEnd(), Value) && FMath::IsFinite(Value);
    };
    if (!Parse(WindupInput, OutTiming.WindupSeconds) || (Skill->bUseEffectCollision && (!Parse(EffectDelayInput, OutTiming.EffectHitDelaySeconds) || !Parse(EffectDurationInput, OutTiming.EffectDuration))) || (Skill->Kind == ECombatRoundSkillKind::Projectile && !Parse(ProjectileSpeedInput, OutTiming.ProjectileSpeed)) || (Skill->bUseWeaponTrace && !Parse(WeaponDurationInput, OutTiming.WeaponTraceDuration)))
    {
        OutError = FText::FromString(TEXT("사용 중인 항목에 쉼표 없이 올바른 숫자를 입력하세요."));
        return false;
    }
    FCombatRoundSkill Candidate = *Skill;
    Candidate.WindupSeconds = OutTiming.WindupSeconds;
    Candidate.EffectHitDelaySeconds = OutTiming.EffectHitDelaySeconds;
    Candidate.EffectDuration = OutTiming.EffectDuration;
    Candidate.ProjectileSpeed = OutTiming.ProjectileSpeed;
    Candidate.WeaponTraceDuration = OutTiming.WeaponTraceDuration;
    if (!CombatRoundRules::IsValidSkill(Candidate))
    {
        OutError = FText::FromString(TEXT("입력값이 항목의 허용 범위를 벗어났습니다. 무기 판정은 준비+지속 60초 이하입니다."));
        return false;
    }
    return true;
}

void UCombatDebugWidget::ShowSkillTiming()
{
    LoadoutPanel->SetVisibility(ESlateVisibility::Collapsed);
    UnitToolsPanel->SetVisibility(ESlateVisibility::Collapsed);
    SkillTimingPanel->SetVisibility(ESlateVisibility::Visible);
    RefreshSkillTiming();
}

void UCombatDebugWidget::HandleTimingSkill(FString Value, ESelectInfo::Type SelectionType)
{
    if (bRefreshingTimingSkills) return;
    const FName* SkillId = TimingSkillOptions.Find(Value);
    SelectedTimingSkill = SkillId ? *SkillId : NAME_None;
    TimingMessage = FText::GetEmpty();
    RefreshSkillTiming(true);
}

void UCombatDebugWidget::ApplySkillTiming()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    FCombatDebugSkillTiming Timing;
    FText Error;
    const bool bSucceeded = Round && ReadSkillTimingInputs(Timing, Error) && Round->SetDebugSkillTiming(Controller, SelectedUnitId, SelectedTimingSkill, Timing, Error);
    TimingMessage = bSucceeded ? FText::FromString(TEXT("입력값을 현재 전투에 임시 적용했습니다.")) : Error;
    RefreshSkillTiming(bSucceeded);
    RefreshState();
}

void UCombatDebugWidget::ResetSkillTiming()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    FText Error;
    const bool bSucceeded = Round && Round->ResetDebugSkillTiming(Controller, SelectedUnitId, SelectedTimingSkill, Error);
    TimingMessage = bSucceeded ? FText::FromString(TEXT("현재 전투의 타이밍을 원본 에셋 값으로 복원했습니다.")) : Error;
    RefreshSkillTiming(bSucceeded);
    RefreshState();
}

void UCombatDebugWidget::CopySkillTiming()
{
    const ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    const ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    FCombatDebugSkillTiming Timing;
    FCombatDebugSkillTiming Original;
    FSoftObjectPath Asset;
    FText Error;
    if (!Round || !Round->GetDebugSkillTiming(SelectedUnitId, SelectedTimingSkill, Timing, Original, Asset, Error) || !ReadSkillTimingInputs(Timing, Error))
    {
        TimingMessage = Error;
        RefreshSkillTiming();
        return;
    }
    const FString Settings = FString::Printf(TEXT("Asset=%s\nSkillId=%s\nWindupSeconds=%.9g\nEffectHitDelaySeconds=%.9g\nEffectDuration=%.9g\nProjectileSpeed=%.9g\nWeaponTraceDuration=%.9g\n"), *Asset.ToString(), *SelectedTimingSkill.ToString(), Timing.WindupSeconds, Timing.EffectHitDelaySeconds, Timing.EffectDuration, Timing.ProjectileSpeed, Timing.WeaponTraceDuration);
    FPlatformApplicationMisc::ClipboardCopy(*Settings);
    TimingMessage = FText::FromString(TEXT("에셋 경로와 입력 설정값을 클립보드에 복사했습니다. 원본 에셋은 저장하지 않았습니다."));
    RefreshSkillTiming();
}

void UCombatDebugWidget::ApplyUnitHealth()
{
    ApplyHealth(false);
}

void UCombatDebugWidget::HealSelectedUnit()
{
    ApplyHealth(true);
}

void UCombatDebugWidget::RebuildSpawnOptions()
{
    const ACombatDebugGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatDebugGameMode>() : nullptr;
    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        const bool bEnemy = TeamIndex == 1;
        UComboBoxString* Choice = bEnemy ? EnemySpawnChoice : AllySpawnChoice;
        TMap<FString, FName>& Options = bEnemy ? EnemySpawnOptions : AllySpawnOptions;
        const FName SelectedId = Options.FindRef(Choice->GetSelectedOption());
        Options.Reset();
        Choice->ClearOptions();
        TArray<FName> Ids;
        TArray<FText> Names;
        if (Mode) Mode->GetDebugSpawnOptions(bEnemy, Ids, Names);
        FString SelectedLabel;
        for (int32 Index = 0; Index < FMath::Min(Ids.Num(), Names.Num()); ++Index)
        {
            if (Ids[Index].IsNone()) continue;
            const FString Label = FString::Printf(TEXT("%d · %s"), Index + 1, *Names[Index].ToString());
            Options.Add(Label, Ids[Index]);
            Choice->AddOption(Label);
            if (Ids[Index] == SelectedId) SelectedLabel = Label;
        }
        if (!SelectedLabel.IsEmpty()) Choice->SetSelectedOption(SelectedLabel);
        else if (Choice->GetOptionCount() > 0) Choice->SetSelectedIndex(0);
    }
}

void UCombatDebugWidget::RefreshSpawnState()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const ACombatDebugGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatDebugGameMode>() : nullptr;
    int32 AllyCount = 0;
    int32 EnemyCount = 0;
    if (Round)
    {
        for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
        {
            if (Unit.bEnemy) ++EnemyCount;
            else ++AllyCount;
        }
    }
    SpawnCount->SetText(FText::FromString(FString::Printf(TEXT("전체 캐릭터 %d / 8 · 아군 %d · 적군 %d\n사망한 캐릭터도 인원에 포함됩니다."), AllyCount + EnemyCount, AllyCount, EnemyCount)));
    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        const bool bEnemy = TeamIndex == 1;
        UComboBoxString* Choice = bEnemy ? EnemySpawnChoice : AllySpawnChoice;
        UButton* Button = bEnemy ? EnemySpawnButton : AllySpawnButton;
        UTextBlock* Result = bEnemy ? EnemySpawnStatus : AllySpawnStatus;
        const TMap<FString, FName>& Options = bEnemy ? EnemySpawnOptions : AllySpawnOptions;
        const FText& Message = bEnemy ? EnemySpawnMessage : AllySpawnMessage;
        FText Error;
        bool bCanSpawn = Mode && Mode->CanSpawnDebugUnit(Controller, bEnemy, Error);
        if (!Mode) Error = FText::FromString(TEXT("디버그 전투 준비 후 사용할 수 있습니다."));
        if (bCanSpawn && !Options.Contains(Choice->GetSelectedOption()))
        {
            bCanSpawn = false;
            Error = FText::FromString(TEXT("추가할 캐릭터가 없습니다."));
        }
        Button->SetIsEnabled(bCanSpawn);
        Choice->SetIsEnabled(!Options.IsEmpty());
        const FText Availability = bCanSpawn ? FText::FromString(TEXT("빈 진영 칸에 추가하고 체력 설정 대상으로 선택합니다.")) : Error;
        Button->SetToolTipText(Availability);
        Result->SetText(Message.IsEmpty() || Message.EqualTo(Availability) ? Availability : bCanSpawn ? Message : FText::Format(FText::FromString(TEXT("{0}\n{1}")), Message, Availability));
    }
}

void UCombatDebugWidget::SpawnUnit(bool bEnemy)
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatDebugGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatDebugGameMode>() : nullptr;
    UComboBoxString* Choice = bEnemy ? EnemySpawnChoice : AllySpawnChoice;
    const TMap<FString, FName>& Options = bEnemy ? EnemySpawnOptions : AllySpawnOptions;
    const FName* OptionId = Options.Find(Choice->GetSelectedOption());
    FText& Message = bEnemy ? EnemySpawnMessage : AllySpawnMessage;
    int32 NewUnitId = INDEX_NONE;
    FText Error;
    if (!Mode || !OptionId)
    {
        Message = FText::FromString(TEXT("추가할 캐릭터를 선택하세요."));
        RefreshSpawnState();
        return;
    }
    if (Mode->SpawnDebugUnit(Controller, bEnemy, *OptionId, NewUnitId, Error))
    {
        Message = FText::FromString(FString::Printf(TEXT("%s 추가 완료 · %s"), bEnemy ? TEXT("적군") : TEXT("아군"), *Choice->GetSelectedOption()));
        SelectedUnitId = NewUnitId;
        ActionMessage = FText::GetEmpty();
        ReviveMessage = FText::GetEmpty();
        HealthMessage = FText::GetEmpty();
        ObservedRevision = INDEX_NONE;
    }
    else Message = Error;
    RefreshState();
}

void UCombatDebugWidget::RebuildLists()
{
    if (!OwnedList || !CatalogList) return;
    const TArray<FDebugSkillCategory>& Categories = GetDebugSkillCategories();
    if (!Categories.IsValidIndex(SelectedSkillCategory)) SelectedSkillCategory = 0;
    const TArray<FDebugSkillMethod>& Methods = GetDebugSkillMethods();
    if (!Methods.IsValidIndex(SelectedSkillMethod)) SelectedSkillMethod = 0;
    TArray<int32> CategoryCounts;
    CategoryCounts.Init(0, Categories.Num());
    TArray<int32> MethodCounts;
    MethodCounts.Init(0, Methods.Num());
    RefreshSkillCategories(CategoryCounts);
    RefreshSkillMethods(MethodCounts);
    SkillCategoryTabs->SetVisibility(bEquipment ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    SkillMethodTabs->SetVisibility(bEquipment ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    OwnedList->ClearChildren();
    CatalogList->ClearChildren();
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    UCombatDebugLoadout* Loadout = Controller ? Controller->GetDebugLoadout() : nullptr;
    if (!Round || !Loadout) return;
    const FCombatRoundUnitView* Selected = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
    if (!Selected || !IsValid(Selected->Unit)) return;
    if (Selected->bEnemy)
    {
        AddText(OwnedList, TEXT("스킬·장비를 변경할 아군을 선택하세요."), 18);
        CatalogTitle->SetText(FText::FromString(TEXT("적군의 HP는 캐릭터·체력 탭에서 변경할 수 있습니다.")));
        return;
    }
    const FString Query = Search->GetText().ToString().TrimStartAndEnd();
    int32 Count = 0;
    if (!bEquipment)
    {
        const auto& Skills = Selected->Unit->GetEquippedSkillDataAssets();
        AddText(OwnedList, FString::Printf(TEXT("현재 스킬 %d / 5"), Skills.Num()), 18);
        for (int32 Index = 0; Index < Skills.Num(); ++Index)
        {
            if (Skills[Index]) AddAction(OwnedList, TEXT("제거 · ") + Loadout->GetSkillLabel(FSoftObjectPath(Skills[Index])).ToString(), ECombatDebugAction::RemoveSkill, FSoftObjectPath(Skills[Index]), Index);
        }
        for (const FSoftObjectPath& Asset : Loadout->GetSkillAssets())
        {
            const FString DisplayName = Loadout->GetSkillLabel(Asset).ToString();
            if (!Query.IsEmpty() && !Asset.GetAssetName().Contains(Query) && !DisplayName.Contains(Query)) continue;
            const bool bOwned = Skills.ContainsByPredicate([&Asset](const USkillDefinitionDataAsset* Skill) { return Skill && FSoftObjectPath(Skill) == Asset; });
            if (bOwned) continue;
            const FGameplayTagContainer& Tags = Loadout->GetSkillTags(Asset);
            const bool bMatchesElement = MatchesDebugSkillCategory(Tags, Categories[SelectedSkillCategory]);
            const bool bMatchesMethod = MatchesDebugSkillMethod(Tags, Methods[SelectedSkillMethod]);
            // Count each facet against the other selection so counts describe the next filter result.
            // 각 분류의 개수에는 다른 분류의 선택을 적용하여 다음 선택의 결과 수를 표시합니다.
            if (bMatchesMethod)
            {
                for (int32 Index = 0; Index < Categories.Num(); ++Index)
                {
                    if (MatchesDebugSkillCategory(Tags, Categories[Index])) ++CategoryCounts[Index];
                }
            }
            if (bMatchesElement)
            {
                for (int32 Index = 0; Index < Methods.Num(); ++Index)
                {
                    if (MatchesDebugSkillMethod(Tags, Methods[Index])) ++MethodCounts[Index];
                }
            }
            if (!bMatchesElement || !bMatchesMethod) continue;
            AddAction(CatalogList, TEXT("추가 · ") + DisplayName + TEXT("\n") + Asset.GetAssetName(), ECombatDebugAction::AddSkill, Asset);
            ++Count;
        }
        RefreshSkillCategories(CategoryCounts);
        RefreshSkillMethods(MethodCounts);
        if (Count == 0) AddText(CatalogList, TEXT("이 속성·방식·검색어에 맞는 미보유 스킬이 없습니다."));
    }
    else
    {
        AddText(OwnedList, TEXT("현재 보유 장비 · 제거 시 장착도 해제"), 18);
        const FRunPartyMember* Member = Loadout->GetEquipmentMember(SelectedUnitId);
        if (Member)
        {
            for (int32 Index = 0; Index < Member->Items.Num(); ++Index)
            {
                FString Slots;
                for (const FRunEquipmentSlot& EquippedSlot : Member->Equipment.Slots)
                {
                    if (EquippedSlot.ItemIndex == Index) Slots += EquippedSlot.SlotTag == URunEquipmentCatalog::GetWeaponSlot(0) ? TEXT(" [무기 1]") : TEXT(" [무기 2]");
                }
                AddAction(OwnedList, TEXT("제거 · ") + Member->Items[Index].DisplayName.ToString() + Slots, ECombatDebugAction::RemoveEquipment, Member->Items[Index].Asset, Index);
            }
        }
        for (const FRunItemDefinition& Item : Loadout->GetEquipmentItems())
        {
            if (!Query.IsEmpty() && !Item.DisplayName.ToString().Contains(Query) && !Item.Asset.GetAssetName().Contains(Query)) continue;
            const FRunEquipmentProfile* Profile = URunEquipmentCatalog::Get().ResolveProfile(Item);
            if (!Profile) continue;
            AddText(CatalogList, Item.DisplayName.ToString() + TEXT(" · ") + Item.Asset.GetAssetName());
            for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
            {
                const FGameplayTag WeaponSlot = URunEquipmentCatalog::GetWeaponSlot(SlotIndex);
                if (URunEquipmentCatalog::ResolveSlot(*Profile, WeaponSlot).IsValid()) AddAction(CatalogList, FString::Printf(TEXT("획득 + 무기 %d 장착"), SlotIndex + 1), ECombatDebugAction::GrantEquipment, Item.Asset, INDEX_NONE, WeaponSlot);
            }
            ++Count;
        }
    }
    CatalogTitle->SetText(FText::FromString(bEquipment ? FString::Printf(TEXT("장비 목록 · %d개"), Count) : FString::Printf(TEXT("속성 %s · 방식 %s · 미보유 %d개"), Categories[SelectedSkillCategory].Label, Methods[SelectedSkillMethod].Label, Count)));
}

void UCombatDebugWidget::RefreshSkillCategories(const TArray<int32>& Counts)
{
    const TArray<FDebugSkillCategory>& Categories = GetDebugSkillCategories();
    for (int32 Index = 0; Index < SkillCategoryButtons.Num(); ++Index)
    {
        UCombatDebugActionButton* Button = SkillCategoryButtons[Index];
        const bool bSelected = Index == SelectedSkillCategory;
        UDemonicUITheme::Get().StyleButton(Button, bSelected);
        if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
        {
            Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s (%d)"), bSelected ? TEXT("✓ ") : TEXT(""), Categories[Index].Label, Counts.IsValidIndex(Index) ? Counts[Index] : 0)));
            UDemonicUITheme::Get().StyleText(Label, false, 14);
        }
    }
}

void UCombatDebugWidget::RefreshSkillMethods(const TArray<int32>& Counts)
{
    const TArray<FDebugSkillMethod>& Methods = GetDebugSkillMethods();
    for (int32 Index = 0; Index < SkillMethodButtons.Num(); ++Index)
    {
        UCombatDebugActionButton* Button = SkillMethodButtons[Index];
        const bool bSelected = Index == SelectedSkillMethod;
        UDemonicUITheme::Get().StyleButton(Button, bSelected);
        if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
        {
            Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s (%d)"), bSelected ? TEXT("✓ ") : TEXT(""), Methods[Index].Label, Counts.IsValidIndex(Index) ? Counts[Index] : 0)));
            UDemonicUITheme::Get().StyleText(Label, false, 14);
        }
    }
}

void UCombatDebugWidget::HandleAction(UCombatDebugActionButton* Button)
{
    if (Button && Button->Action == ECombatDebugAction::SelectSkillCategory)
    {
        if (!GetDebugSkillCategories().IsValidIndex(Button->Index)) return;
        SelectedSkillCategory = Button->Index;
        CatalogScroll->ScrollToStart();
        RebuildLists();
        return;
    }
    if (Button && Button->Action == ECombatDebugAction::SelectSkillMethod)
    {
        if (!GetDebugSkillMethods().IsValidIndex(Button->Index)) return;
        SelectedSkillMethod = Button->Index;
        CatalogScroll->ScrollToStart();
        RebuildLists();
        return;
    }
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    UCombatDebugLoadout* Loadout = Controller ? Controller->GetDebugLoadout() : nullptr;
    FText Error;
    if (!Button || !Round || !Loadout || !Round->CanEditDebugUnit(Controller, SelectedUnitId, Error))
    {
        ActionMessage = Error;
        return;
    }
    bool bSucceeded = false;
    if (Button->Action == ECombatDebugAction::AddSkill || Button->Action == ECombatDebugAction::RemoveSkill)
    {
        const FCombatRoundUnitView* Selected = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
        if (!Selected || !IsValid(Selected->Unit)) return;
        TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills = Selected->Unit->GetEquippedSkillDataAssets();
        if (Button->Action == ECombatDebugAction::AddSkill && Loadout->GetSkillAssets().Contains(Button->Asset))
        {
            if (USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Button->Asset.TryLoad())) Skills.AddUnique(Skill);
            else Error = FText::FromString(TEXT("스킬 에셋을 불러오지 못했습니다."));
        }
        else if (Skills.IsValidIndex(Button->Index) && FSoftObjectPath(Skills[Button->Index]) == Button->Asset) Skills.RemoveAt(Button->Index);
        else Error = FText::FromString(TEXT("목록이 바뀌었습니다. 다시 선택하세요."));
        if (Error.IsEmpty()) bSucceeded = Round->SetDebugUnitSkills(Controller, SelectedUnitId, Skills, Error);
    }
    else if (Button->Action == ECombatDebugAction::GrantEquipment) bSucceeded = Loadout->GrantEquipment(Controller, SelectedUnitId, Button->Asset, Button->EquipmentSlot, Error);
    else bSucceeded = Loadout->RemoveEquipment(Controller, SelectedUnitId, Button->Index, Error);
    ActionMessage = bSucceeded ? FText::FromString(TEXT("반영했습니다. 전투 화면에서 바로 확인할 수 있습니다.")) : Error;
    RebuildLists();
    RefreshState();
}

void UCombatDebugWidget::TogglePanel()
{
    PanelSize->SetVisibility(PanelSize->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UCombatDebugWidget::ToggleRevivePanel()
{
    const bool bOpen = RevivePanel->GetVisibility() == ESlateVisibility::Collapsed;
    RevivePanel->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (UTextBlock* Label = Cast<UTextBlock>(ReviveToggleButton->GetContent())) Label->SetText(FText::FromString(bOpen ? TEXT("아군 부활 창 접기") : TEXT("아군 부활 창 열기")));
    RefreshReviveState();
}

void UCombatDebugWidget::ReviveSelectedAlly()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Round) return;
    FText Error;
    const bool bSucceeded = Round->ReviveDebugUnit(Controller, SelectedUnitId, Error);
    ReviveMessage = bSucceeded ? Round->GetView().Message : Error;
    if (bSucceeded) RefreshHealthState(true);
    RebuildLists();
    RefreshState();
}

void UCombatDebugWidget::ShowUnitTools()
{
    LoadoutPanel->SetVisibility(ESlateVisibility::Collapsed);
    SkillTimingPanel->SetVisibility(ESlateVisibility::Collapsed);
    UnitToolsPanel->SetVisibility(ESlateVisibility::Visible);
    RebuildSpawnOptions();
    RefreshHealthState(true);
    RefreshSpawnState();
}

void UCombatDebugWidget::SpawnAlly()
{
    SpawnUnit(false);
}

void UCombatDebugWidget::SpawnEnemy()
{
    SpawnUnit(true);
}

void UCombatDebugWidget::ShowSkills()
{
    bEquipment = false;
    SkillTimingPanel->SetVisibility(ESlateVisibility::Collapsed);
    UnitToolsPanel->SetVisibility(ESlateVisibility::Collapsed);
    LoadoutPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    CatalogScroll->ScrollToStart();
    RebuildLists();
}

void UCombatDebugWidget::ShowEquipment()
{
    bEquipment = true;
    SkillTimingPanel->SetVisibility(ESlateVisibility::Collapsed);
    UnitToolsPanel->SetVisibility(ESlateVisibility::Collapsed);
    LoadoutPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    CatalogScroll->ScrollToStart();
    RebuildLists();
}

void UCombatDebugWidget::RestartCombat()
{
    if (ACombatDebugGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatDebugGameMode>())
    {
        Mode->RestartCombat();
        ActionMessage = Mode->GetStatusMessage();
        ReviveMessage = FText::GetEmpty();
        AllySpawnMessage = FText::GetEmpty();
        EnemySpawnMessage = FText::GetEmpty();
        RefreshState();
    }
}

void UCombatDebugWidget::HandleSearch(const FText& Text)
{
    CatalogScroll->ScrollToStart();
    RebuildLists();
}

void UCombatDebugWidget::HandleUnit(FString Value, ESelectInfo::Type SelectionType)
{
    if (bRefreshingUnits) return;
    const int32* UnitId = UnitOptions.Find(Value);
    SelectedUnitId = UnitId ? *UnitId : INDEX_NONE;
    ActionMessage = FText::GetEmpty();
    ReviveMessage = FText::GetEmpty();
    HealthMessage = FText::GetEmpty();
    RebuildLists();
    RefreshReviveState();
    RefreshHealthState(true);
    RefreshSkillTiming();
}
