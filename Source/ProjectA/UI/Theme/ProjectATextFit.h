#pragma once

#include "CoreMinimal.h"

class UScaleBox;
class UTextBlock;
class UWidgetTree;

namespace ProjectATextFit
{
    // Keep a compact label on one line and shrink it only when the control is too narrow; it never enlarges or splits a word.
    // 좁은 컨트롤의 라벨을 한 줄로 유지하고 공간이 부족할 때만 축소하며 확대하거나 단어를 나누지 않습니다.
    PROJECTA_API UScaleBox* ShrinkToFit(UWidgetTree& Tree, UTextBlock* Label);
}
