#include "UI/Theme/ProjectATextFit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"

UScaleBox* ProjectATextFit::ShrinkToFit(UWidgetTree& Tree, UTextBlock* Label)
{
    UScaleBox* Box = Tree.ConstructWidget<UScaleBox>();
    Box->SetStretch(EStretch::ScaleToFit);
    Box->SetStretchDirection(EStretchDirection::DownOnly);
    if (Label)
    {
        Label->SetAutoWrapText(false);
        Box->SetContent(Label);
    }
    return Box;
}
