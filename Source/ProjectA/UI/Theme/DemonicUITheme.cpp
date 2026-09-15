#include "UI/Theme/DemonicUITheme.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FLinearColor BodyColor(0.91f, 0.87f, 0.81f);
const FLinearColor HeadingColor(0.96f, 0.89f, 0.74f);
const FLinearColor MutedColor(0.48f, 0.45f, 0.42f);

FSlateBrush TextureBrush(UTexture2D* Texture, FVector2D Size)
{
    FSlateBrush Brush;
    Brush.SetResourceObject(Texture);
    Brush.SetImageSize(Size);
    // Preserve the complete artwork because Slate box margins use source pixels instead of brush image size.
    // Slate Box 여백은 브러시 크기가 아닌 원본 픽셀로 계산되므로 전체 문양을 이미지로 보존합니다.
    Brush.DrawAs = ESlateBrushDrawType::Image;
    return Brush;
}

bool UsesDefaultTextColor(const UTextBlock* Text)
{
    const FSlateColor Color = Text->GetColorAndOpacity();
    return !Color.IsColorSpecified() || Color.GetSpecifiedColor().Equals(FLinearColor::White) || Color.GetSpecifiedColor().Equals(BodyColor);
}

FScrollBarStyle ThemeScrollBar(FScrollBarStyle Style)
{
    Style.NormalThumbImage.TintColor = MutedColor;
    Style.HoveredThumbImage.TintColor = BodyColor;
    Style.DraggedThumbImage.TintColor = HeadingColor;
    return Style;
}
}

UDemonicUITheme::UDemonicUITheme()
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Ready(TEXT("/Game/DemonicUI/GUI_Elements/Button_A_Long_Ready.Button_A_Long_Ready"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Hovered(TEXT("/Game/DemonicUI/GUI_Elements/Button_A_Long_aimed.Button_A_Long_aimed"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Pressed(TEXT("/Game/DemonicUI/GUI_Elements/Button_A_Long_active.Button_A_Long_active"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> SmallReady(TEXT("/Game/DemonicUI/GUI_Elements/Button_A_Lil_Ready.Button_A_Lil_Ready"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> SmallHovered(TEXT("/Game/DemonicUI/GUI_Elements/Button_A_Lil_aimed.Button_A_Lil_aimed"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> SmallPressed(TEXT("/Game/DemonicUI/GUI_Elements/Button_A_Lil_active.Button_A_Lil_active"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Panel(TEXT("/Game/DemonicUI/GUI_Elements/pop_up_window_B.pop_up_window_B"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Backdrop(TEXT("/Game/DemonicUI/Backgrounds/Background.Background"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Unchecked(TEXT("/Game/DemonicUI/Red_buttons/Button_tiny_ready.Button_tiny_ready"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Checked(TEXT("/Game/DemonicUI/Red_buttons/Button_tiny_ok.Button_tiny_ok"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Divider(TEXT("/Game/DemonicUI/GUI_Elements/Top_frame_m.Top_frame_m"));
    ButtonReady = Ready.Object;
    ButtonHovered = Hovered.Object;
    ButtonPressed = Pressed.Object;
    CompactReady = SmallReady.Object;
    CompactHovered = SmallHovered.Object;
    CompactPressed = SmallPressed.Object;
    PanelTexture = Panel.Object;
    BackdropTexture = Backdrop.Object;
    CheckReady = Unchecked.Object;
    CheckSelected = Checked.Object;
    DividerTexture = Divider.Object;
}

const UDemonicUITheme& UDemonicUITheme::Get()
{
    return *GetDefault<UDemonicUITheme>();
}

FButtonStyle UDemonicUITheme::MakeButtonStyle(const FButtonStyle& Existing, bool bPrimary, bool bCompact) const
{
    FButtonStyle Style = Existing;
    UTexture2D* Ready = bCompact ? CompactReady.Get() : ButtonReady.Get();
    UTexture2D* Hovered = bCompact ? CompactHovered.Get() : ButtonHovered.Get();
    UTexture2D* Pressed = bCompact ? CompactPressed.Get() : ButtonPressed.Get();
    if (!Ready || !Hovered || !Pressed) return Style;
    const FVector2D Size = bCompact ? FVector2D(40.0f, 40.0f) : FVector2D(256.0f, 48.0f);
    Style.Normal = TextureBrush(bPrimary ? Pressed : Ready, Size);
    Style.Hovered = TextureBrush(bPrimary ? Pressed : Hovered, Size);
    Style.Pressed = TextureBrush(Pressed, Size);
    Style.Pressed.TintColor = FLinearColor(0.8f, 0.8f, 0.8f);
    Style.Disabled = TextureBrush(Ready, Size);
    Style.Disabled.TintColor = FLinearColor(0.38f, 0.38f, 0.38f);
    Style.NormalForeground = BodyColor;
    Style.HoveredForeground = FLinearColor::White;
    Style.PressedForeground = FLinearColor::White;
    Style.DisabledForeground = MutedColor;
    return Style;
}

void UDemonicUITheme::StyleButton(UButton* Button, bool bPrimary) const
{
    if (!Button) return;
    const UTextBlock* DirectLabel = Cast<UTextBlock>(Button->GetContent());
    const FString Label = DirectLabel ? DirectLabel->GetText().ToString() : FString();
    const bool bCompact = Label == TEXT("<") || Label == TEXT(">") || Label == TEXT("X") || Label == TEXT("×") || Label == TEXT("+") || Label == TEXT("-");
    Button->SetStyle(MakeButtonStyle(Button->GetStyle(), bPrimary, bCompact));
    Button->SetBackgroundColor(FLinearColor::White);
    UWidgetTree::ForWidgetAndChildren(Button, [this](UWidget* Widget)
    {
        if (UTextBlock* Text = Cast<UTextBlock>(Widget); Text && UsesDefaultTextColor(Text))
        {
            StyleText(Text);
            Text->SetColorAndOpacity(FSlateColor::UseForeground());
        }
    });
}

void UDemonicUITheme::StyleText(UTextBlock* Text, bool bHeading, int32 FontSize) const
{
    if (!Text) return;
    Text->SetColorAndOpacity(bHeading ? HeadingColor : BodyColor);
    Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
    Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    if (FontSize > 0)
    {
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
    }
}

void UDemonicUITheme::ApplyControls(UWidgetTree* Tree) const
{
    if (!Tree) return;
    Tree->ForEachWidget([this](UWidget* Widget)
    {
        if (UButton* Button = Cast<UButton>(Widget))
        {
            StyleButton(Button);
        }
        else if (UComboBoxString* Combo = Cast<UComboBoxString>(Widget))
        {
            FComboBoxStyle Style = Combo->GetWidgetStyle();
            Style.ComboButtonStyle.ButtonStyle = MakeButtonStyle(Style.ComboButtonStyle.ButtonStyle, false);
            Style.ComboButtonStyle.DownArrowImage.TintColor = BodyColor;
            Style.ComboButtonStyle.MenuBorderBrush = TextureBrush(PanelTexture, FVector2D(128.0f, 128.0f));
            Style.ComboButtonStyle.MenuBorderPadding = FMargin(8.0f);
            Combo->SetWidgetStyle(Style);
            FTableRowStyle Row = Combo->GetItemStyle();
            Row.TextColor = BodyColor;
            Row.SelectedTextColor = FLinearColor::White;
            Row.EvenRowBackgroundBrush = FSlateColorBrush(FLinearColor(0.018f, 0.022f, 0.03f));
            Row.OddRowBackgroundBrush = Row.EvenRowBackgroundBrush;
            Row.EvenRowBackgroundHoveredBrush = FSlateColorBrush(FLinearColor(0.10f, 0.14f, 0.19f));
            Row.OddRowBackgroundHoveredBrush = Row.EvenRowBackgroundHoveredBrush;
            Row.ActiveBrush = Row.EvenRowBackgroundHoveredBrush;
            Row.ActiveHoveredBrush = Row.EvenRowBackgroundHoveredBrush;
            Row.InactiveBrush = Row.EvenRowBackgroundHoveredBrush;
            Row.InactiveHoveredBrush = Row.EvenRowBackgroundHoveredBrush;
            Combo->SetItemStyle(Row);
        }
        else if (UCheckBox* Check = Cast<UCheckBox>(Widget))
        {
            FCheckBoxStyle Style = Check->GetWidgetStyle();
            Style.UncheckedImage = TextureBrush(CheckReady, FVector2D(28.0f, 28.0f));
            Style.UncheckedHoveredImage = Style.UncheckedImage;
            Style.UncheckedPressedImage = Style.UncheckedImage;
            Style.CheckedImage = TextureBrush(CheckSelected, FVector2D(28.0f, 28.0f));
            Style.CheckedHoveredImage = Style.CheckedImage;
            Style.CheckedPressedImage = Style.CheckedImage;
            Style.ForegroundColor = BodyColor;
            Style.HoveredForeground = FLinearColor::White;
            Style.CheckedForeground = BodyColor;
            Check->SetWidgetStyle(Style);
        }
        else if (UEditableTextBox* Input = Cast<UEditableTextBox>(Widget))
        {
            FEditableTextBoxStyle Style = Input->GetWidgetStyle();
            const FButtonStyle ButtonStyle = MakeButtonStyle(FButtonStyle(), false);
            Style.BackgroundImageNormal = ButtonStyle.Normal;
            Style.BackgroundImageHovered = ButtonStyle.Hovered;
            Style.BackgroundImageFocused = ButtonStyle.Hovered;
            Style.BackgroundImageReadOnly = ButtonStyle.Disabled;
            Style.ForegroundColor = BodyColor;
            Style.ReadOnlyForegroundColor = MutedColor;
            Style.FocusedForegroundColor = FLinearColor::White;
            Input->SetWidgetStyle(Style);
            Input->SetForegroundColor(BodyColor);
        }
        else if (UScrollBox* Scroll = Cast<UScrollBox>(Widget))
        {
            Scroll->SetWidgetBarStyle(ThemeScrollBar(Scroll->GetWidgetBarStyle()));
        }
        else if (UTextBlock* Text = Cast<UTextBlock>(Widget); Text && UsesDefaultTextColor(Text))
        {
            // Preserve inherited button-state colors and explicit warning or profession colors.
            // 버튼 상태의 상속 색상과 명시적인 경고 또는 직업 색상을 보존합니다.
            const bool bInherited = !Text->GetColorAndOpacity().IsColorSpecified();
            StyleText(Text);
            if (bInherited) Text->SetColorAndOpacity(FSlateColor::UseForeground());
        }
    });
}

void UDemonicUITheme::StylePanel(UBorder* Panel) const
{
    if (!Panel || !PanelTexture) return;
    Panel->SetBrush(TextureBrush(PanelTexture, FVector2D(128.0f, 128.0f)));
    Panel->SetBrushColor(FLinearColor::White);
}

void UDemonicUITheme::StyleBackdrop(UBorder* Background) const
{
    if (!Background || !BackdropTexture) return;
    Background->SetBrush(TextureBrush(BackdropTexture, FVector2D(1920.0f, 1080.0f)));
    Background->SetBrushColor(FLinearColor(0.35f, 0.35f, 0.35f));
}

void UDemonicUITheme::StyleBackgroundImage(UImage* Background) const
{
    if (!Background || !BackdropTexture) return;
    Background->SetBrush(TextureBrush(BackdropTexture, FVector2D(1920.0f, 1080.0f)));
    Background->SetColorAndOpacity(FLinearColor::White);
}

void UDemonicUITheme::AddDivider(UWidgetTree* Tree, UVerticalBox* Parent) const
{
    if (!Tree || !Parent || !DividerTexture) return;
    USizeBox* Size = Tree->ConstructWidget<USizeBox>();
    Size->SetHeightOverride(10.0f);
    UImage* Image = Tree->ConstructWidget<UImage>();
    Image->SetBrush(TextureBrush(DividerTexture, FVector2D(320.0f, 10.0f)));
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    Size->SetContent(Image);
    Parent->AddChildToVerticalBox(Size)->SetPadding(FMargin(0.0f, 8.0f));
}

UDemonicComboBoxString::UDemonicComboBoxString()
{
    // Initialize the inherited text color before Slate builds the selected item and popup rows.
    // Slate가 선택 항목과 펼침 목록을 만들기 전에 상속 글자색을 초기화합니다.
    InitForegroundColor(BodyColor);
    InitScrollBarStyle(ThemeScrollBar(GetScrollBarStyle()));
}
