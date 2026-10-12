#include "Misc/AutomationTest.h"
#include "UI/Theme/ProjectALineBreakIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectALineBreakTest, "ProjectA.UI.LineBreak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectALineBreakTest::RunTest(const FString& Parameters)
{
    const auto Breaks = [](const FString& Text)
    {
        FProjectALineBreakIterator Iterator;
        Iterator.SetString(FString(Text));
        TArray<int32> Result;
        for (int32 Position = Iterator.MoveToNext(); Position != INDEX_NONE; Position = Iterator.MoveToNext()) Result.Add(Position);
        return Result;
    };

    // Numbers, units and symbols stay attached to Korean words; only spaces open a break.
    // 숫자·단위·기호는 한글 단어와 붙어 있고 공백에서만 줄을 바꿀 수 있습니다.
    const TArray<int32> Korean = Breaks(TEXT("무행동 1명"));
    TestFalse(TEXT("1명 is never split."), Korean.Contains(5));
    TestTrue(TEXT("The space before 1명 is a break."), Korean.Contains(4));
    TestFalse(TEXT("AP·SAP stays together."), Breaks(TEXT("비용 AP·SAP")).Contains(6));
    TestFalse(TEXT("Hangul syllables never split."), Breaks(TEXT("행동계획")).ContainsByPredicate([](int32 Position) { return Position > 0 && Position < 4; }));

    // English words keep their letters together.
    // 영어 단어의 글자는 함께 유지됩니다.
    TestTrue(TEXT("English breaks only after spaces."), Breaks(TEXT("Ready to act")).ContainsByPredicate([](int32 Position) { return Position == 6 || Position == 9; }));
    TestFalse(TEXT("No break inside an English word."), Breaks(TEXT("Ready")).ContainsByPredicate([](int32 Position) { return Position > 0 && Position < 5; }));

    // Japanese without spaces still wraps between characters.
    // 띄어쓰기가 없는 일본어는 글자 사이에서 줄바꿈합니다.
    TestTrue(TEXT("Kana keeps per-character breaks."), FProjectALineBreakIterator::IsBreakAllowed(TEXT("じゅんび"), 2));
    return true;
}

#endif
