#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PartyDefinitionDataAsset.generated.h"

class APlayerUnit;

UCLASS(BlueprintType)
class PROJECTA_API UPartyDefinitionDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TMap<FName, TSubclassOf<APlayerUnit>> PlayerUnitClasses;

    // Temporary shared combat class until each profession has its own content.
    // 직업별 콘텐츠가 준비되기 전까지 사용하는 임시 공통 전투 클래스입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TSubclassOf<APlayerUnit> FallbackPlayerUnitClass;

    TSubclassOf<APlayerUnit> ResolvePlayerClass(FName ClassId) const;
};
