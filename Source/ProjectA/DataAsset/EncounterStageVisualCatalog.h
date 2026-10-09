#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EncounterStageVisualCatalog.generated.h"

class UAnimSequence;
class USkeletalMesh;
class UStaticMesh;

USTRUCT(BlueprintType)
struct PROJECTA_API FEncounterStageProp
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FRotator Rotation = FRotator::ZeroRotator;
    // Fit uniformly inside this box, anchoring the original mesh at its bottom center.
    // 원본 메시의 바닥 중심을 기준으로 이 상자 안에 균일 배율로 맞춥니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector MaxSize = FVector(100.f);
};

USTRUCT(BlueprintType)
struct PROJECTA_API FEncounterStageVisualProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ProfileId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagQuery StageQuery;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<USkeletalMesh>> CharacterParts;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector CharacterLocation = FVector(35.f, -130.f, 12.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FRotator CharacterRotation = FRotator(0.f, 90.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float CharacterHeight = 190.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName FaceBone = TEXT("head");
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FEncounterStageProp> Props;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLinearColor LightColor = FLinearColor(1.f, 0.8f, 0.6f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector LightLocation = FVector(-180.f, -210.f, 245.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float LightIntensity = 1200.f;
};

// Native defaults reference installed source assets; optional authored catalogs can override the scenery.
// 네이티브 기본값은 설치된 원본 에셋을 참조하며 선택적 제작 카탈로그로 무대를 재정의할 수 있습니다.
UCLASS(BlueprintType)
class PROJECTA_API UEncounterStageVisualCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UEncounterStageVisualCatalog();

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FEncounterStageVisualProfile> Profiles;

    const FEncounterStageVisualProfile* Resolve(const FGameplayTagContainer& StageTags) const;
    void GetReferencedAssets(TArray<FSoftObjectPath>& OutAssets) const;
};
