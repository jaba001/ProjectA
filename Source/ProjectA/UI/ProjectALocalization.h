#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

namespace ProjectALocalization
{
    // Key authored content only at the presentation boundary, without rewriting saved names or user input.
    // 저장된 이름이나 사용자 입력을 다시 쓰지 않고 표시 경계에서만 제작 콘텐츠에 번역 키를 부여합니다.
    PROJECTA_API FText Content(const FString& StableKey, const FText& Source);
    PROJECTA_API FText AssetName(const FSoftObjectPath& Asset, const FText& Source);
    PROJECTA_API FText SkillName(FName SkillId, const FText& Source);
    PROJECTA_API FText SkillDescription(FName SkillId, const FText& Source);
}
