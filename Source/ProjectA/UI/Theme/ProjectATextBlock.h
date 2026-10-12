#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "ProjectATextBlock.generated.h"

// Text block that wraps Korean and other spaced languages only at word boundaries.
// 한국어 등 띄어쓰기 언어를 단어 경계에서만 줄바꿈하는 텍스트 블록입니다.
UCLASS(meta = (DisplayName = "ProjectA Text"))
class PROJECTA_API UProjectATextBlock : public UTextBlock
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
