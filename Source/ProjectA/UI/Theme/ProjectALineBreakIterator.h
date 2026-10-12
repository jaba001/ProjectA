#pragma once

#include "CoreMinimal.h"
#include "Internationalization/IBreakIterator.h"

// Word-preserving line breaks: Korean, Latin, digits and symbols stay together until whitespace,
// while ideographic and kana scripts keep the engine's per-character rules for future translations.
// 단어 보존 줄바꿈: 한글·영문·숫자·기호는 공백까지 붙여 두고, 한자·가나는 이후 번역을 위해 엔진의 글자 단위 규칙을 유지합니다.
class PROJECTA_API FProjectALineBreakIterator : public IBreakIterator
{
public:
    FProjectALineBreakIterator();

    virtual void SetString(FString&& InString) override;
    virtual void SetStringRef(FStringView InString) override;
    virtual int32 GetCurrentPosition() const override;
    virtual int32 ResetToBeginning() override;
    virtual int32 ResetToEnd() override;
    virtual int32 MoveToPrevious() override;
    virtual int32 MoveToNext() override;
    virtual int32 MoveToCandidateBefore(const int32 InIndex) override;
    virtual int32 MoveToCandidateAfter(const int32 InIndex) override;

    static bool IsBreakAllowed(FStringView Text, int32 Position);

private:
    TSharedRef<IBreakIterator> Inner;
    FString OwnedString;
    FStringView String;
    int32 CurrentPosition = 0;
};
