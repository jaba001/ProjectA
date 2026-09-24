#pragma once

#include "CoreMinimal.h"
#include "CharacterAppearanceTypes.generated.h"

// Persist catalog item identifiers instead of user-provided asset paths.
// 사용자가 제공한 에셋 경로 대신 카탈로그 항목 식별자를 저장합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCharacterAppearanceSelection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Appearance")
    TArray<FName> ItemIds;

    bool IsEmpty() const { return ItemIds.IsEmpty(); }
    bool operator==(const FCharacterAppearanceSelection& Other) const { return ItemIds == Other.ItemIds; }
    bool operator!=(const FCharacterAppearanceSelection& Other) const { return !(*this == Other); }
};
