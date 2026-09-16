#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PartyDefinitionDataAsset.generated.h"

class APlayerUnit;
class UTexture2D;
class USkillDefinitionDataAsset;
class URunEncounterPoolDataAsset;
class UProfessionBase;

USTRUCT(BlueprintType)
struct PROJECTA_API FProfessionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UProfessionBase> ProfessionClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
    FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<APlayerUnit> CombatClass;
    // Reuse the unit's AP and starting skills with the profession's initial attributes.
    // 직업의 초기 능력치와 유닛의 AP 및 시작 스킬 기본값을 사용합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bUseUnitClassDefaults = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    float MaxHP = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    float Strength = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    float Dexterity = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    float Intelligence = 10.0f;
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
    // An unset pool uses the native three-shop prototype without requiring generated content assets.
    // 풀 미지정 시 별도 에셋 생성 없이 native 상점 3개 시험 구성을 사용합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run")
    TObjectPtr<URunEncounterPoolDataAsset> RunEncounterPool;
    // Draw one encounter-local skill after profession setup; v3 turn checkpoints preserve the chosen loadout.
    // 직업 설정 후 전투 한정 스킬 하나를 획득하며 v3 턴 체크포인트는 선택된 장착을 보존합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TObjectPtr<class USkillPoolDataAsset> EncounterSkillPool;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TMap<FName, FProfessionDefinition> Professions;

    bool ResolveProfession(FName ClassId, FProfessionDefinition& OutDefinition) const;
    bool ResolveProfession(FName ClassId, FProfessionDefinition& OutDefinition, FText& OutError) const;
    FText GetProfessionDetails(FName ClassId) const;

#if WITH_EDITOR
    // Validate the resolved runtime loadout, including legacy class fallbacks.
    // 기존 클래스 대체 경로를 포함한 실제 런타임 장착 구성을 검증합니다.
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TMap<FName, TSubclassOf<APlayerUnit>> PlayerUnitClasses;

    // Temporary shared combat class until each profession has its own content.
    // 직업별 콘텐츠가 준비되기 전까지 사용하는 임시 공통 전투 클래스입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TSubclassOf<APlayerUnit> FallbackPlayerUnitClass;

    TSubclassOf<APlayerUnit> ResolvePlayerClass(FName ClassId) const;
};
