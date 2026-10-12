#include "UI/Theme/ProjectATextBlock.h"
#include "UI/Theme/ProjectALineBreakIterator.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UProjectATextBlock::RebuildWidget()
{
    // The line-break policy is a construction argument, so the Slate widget is created here; SynchronizeProperties applies the rest.
    // 줄바꿈 정책은 생성 인자이므로 여기서 Slate 위젯을 만들고 나머지 속성은 SynchronizeProperties가 적용합니다.
    MyTextBlock = SNew(STextBlock).SimpleTextMode(bSimpleTextMode).LineBreakPolicy(MakeShared<FProjectALineBreakIterator>());
    return MyTextBlock.ToSharedRef();
}
