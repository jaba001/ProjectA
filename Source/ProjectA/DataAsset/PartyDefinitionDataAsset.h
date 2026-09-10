#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PartyDefinitionDataAsset.generated.h"

class APlayerUnit;
class UTexture2D;
class USkillDefinitionDataAsset;

USTRUCT(BlueprintType)
struct PROJECTA_API FProfessionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
    FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<APlayerUnit> CombatClass;
    // Preserve existing class balance until explicit profession overrides are authored.
    // 명시적인 직업 설정을 작성하기 전까지 기존 클래스 밸런스를 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bUseUnitClassDefaults = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    float MaxHP = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 ActionPoints = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 SubActionPoints = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TObjectPtr<USkillDefinitionDataAsset>> StartingSkills;
};

UCLASS(BlueprintType)
class PROJECTA_API UPartyDefinitionDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPartyDefinitionDataAsset();
    // Draw one encounter-local skill after profession setup; v3 turn checkpoints preserve the chosen loadout.
    // 직업 설정 후 전투 한정 스킬 하나를 획득하며 v3 턴 체크포인트는 선택된 장착을 보존합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TObjectPtr<class USkillPoolDataAsset> EncounterSkillPool;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TMap<FName, FProfessionDefinition> Professions;

    bool ResolveProfession(FName ClassId, FProfessionDefinition& OutDefinition) const;
    FText GetProfessionDetails(FName ClassId) const;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TMap<FName, TSubclassOf<APlayerUnit>> PlayerUnitClasses;

    // Temporary shared combat class until each profession has its own content.
    // 직업별 콘텐츠가 준비되기 전까지 사용하는 임시 공통 전투 클래스입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TSubclassOf<APlayerUnit> FallbackPlayerUnitClass;

    TSubclassOf<APlayerUnit> ResolvePlayerClass(FName ClassId) const;
};
