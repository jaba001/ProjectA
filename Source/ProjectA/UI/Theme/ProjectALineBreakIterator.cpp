#include "UI/Theme/ProjectALineBreakIterator.h"
#include "Internationalization/BreakIterator.h"

namespace
{
    // Scripts written without spaces must still wrap between characters.
    // 띄어쓰기 없이 쓰는 문자는 글자 사이에서 줄바꿈할 수 있어야 합니다.
    bool IsPerCharacterScript(TCHAR Character)
    {
        const uint32 Code = static_cast<uint32>(Character);
        return (Code >= 0x3000 && Code <= 0x30FF) || (Code >= 0x3400 && Code <= 0x4DBF) || (Code >= 0x4E00 && Code <= 0x9FFF) || (Code >= 0xF900 && Code <= 0xFAFF) || (Code >= 0xFF00 && Code <= 0xFFEF);
    }
}

FProjectALineBreakIterator::FProjectALineBreakIterator()
    : Inner(FBreakIterator::CreateLineBreakIterator())
{
}

bool FProjectALineBreakIterator::IsBreakAllowed(FStringView Text, int32 Position)
{
    if (Position <= 0 || Position >= Text.Len()) return true;
    const TCHAR Previous = Text[Position - 1];
    const TCHAR Next = Text[Position];
    return FChar::IsWhitespace(Previous) || FChar::IsWhitespace(Next) || IsPerCharacterScript(Previous) || IsPerCharacterScript(Next);
}

void FProjectALineBreakIterator::SetString(FString&& InString)
{
    OwnedString = MoveTemp(InString);
    String = OwnedString;
    Inner->SetStringRef(String);
    ResetToBeginning();
}

void FProjectALineBreakIterator::SetStringRef(FStringView InString)
{
    OwnedString.Reset();
    String = InString;
    Inner->SetStringRef(String);
    ResetToBeginning();
}

int32 FProjectALineBreakIterator::GetCurrentPosition() const
{
    return CurrentPosition;
}

int32 FProjectALineBreakIterator::ResetToBeginning()
{
    Inner->ResetToBeginning();
    return CurrentPosition = 0;
}

int32 FProjectALineBreakIterator::ResetToEnd()
{
    Inner->ResetToEnd();
    return CurrentPosition = String.Len();
}

int32 FProjectALineBreakIterator::MoveToPrevious()
{
    return MoveToCandidateBefore(CurrentPosition);
}

int32 FProjectALineBreakIterator::MoveToNext()
{
    return MoveToCandidateAfter(CurrentPosition);
}

int32 FProjectALineBreakIterator::MoveToCandidateBefore(const int32 InIndex)
{
    // Keep the engine's prohibited positions and only remove breaks inside a word.
    // 엔진의 금지 위치는 유지하고 단어 내부의 줄바꿈만 제거합니다.
    int32 Position = Inner->MoveToCandidateBefore(InIndex);
    while (Position != INDEX_NONE && !IsBreakAllowed(String, Position)) Position = Inner->MoveToCandidateBefore(Position);
    CurrentPosition = Position == INDEX_NONE ? Inner->GetCurrentPosition() : Position;
    return Position;
}

int32 FProjectALineBreakIterator::MoveToCandidateAfter(const int32 InIndex)
{
    int32 Position = Inner->MoveToCandidateAfter(InIndex);
    while (Position != INDEX_NONE && !IsBreakAllowed(String, Position)) Position = Inner->MoveToCandidateAfter(Position);
    CurrentPosition = Position == INDEX_NONE ? Inner->GetCurrentPosition() : Position;
    return Position;
}
