#include "UI/ProjectALocalization.h"

FText ProjectALocalization::Content(const FString& StableKey, const FText& Source)
{
    if (StableKey.IsEmpty() || Source.IsEmpty()) return Source;
    // Content.json explicitly gathers these stable identities; retain the source to reject mismatched custom names.
    // Content.json에서 이 고정 식별자를 명시적으로 수집하며 다른 사용자 제작 이름과 일치하지 않도록 원문을 유지합니다.
    return FText::AsLocalizable_Advanced(TEXT("ProjectA.Content"), StableKey, Source.BuildSourceString());
}

FText ProjectALocalization::AssetName(const FSoftObjectPath& Asset, const FText& Source)
{
    return Asset.IsValid() ? Content(Asset.ToString() + TEXT(".Name"), Source) : Source;
}

FText ProjectALocalization::SkillName(FName SkillId, const FText& Source)
{
    return SkillId.IsNone() ? Source : Content(TEXT("Skill.") + SkillId.ToString() + TEXT(".Name"), Source);
}

FText ProjectALocalization::SkillDescription(FName SkillId, const FText& Source)
{
    return SkillId.IsNone() ? Source : Content(TEXT("Skill.") + SkillId.ToString() + TEXT(".Description"), Source);
}
