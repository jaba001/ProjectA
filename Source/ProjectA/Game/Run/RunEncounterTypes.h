#pragma once

#include "CoreMinimal.h"
#include "RunEncounterTypes.generated.h"

UENUM(BlueprintType)
enum class ERunEncounterType : uint8
{
    Shop
};

// Freeze presentation and identity as values instead of retaining an encounter actor or widget.
// 인카운터 액터나 위젯 대신 표시 정보와 식별자를 값으로 보관합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunEncounterOffer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
    FName EncounterId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
    ERunEncounterType Type = ERunEncounterType::Shop;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunEncounterProgress
{
    GENERATED_BODY()

    // Zero preserves the two-combat route of saves created before encounter choices existed.
    // 0은 인카운터 선택 도입 전에 생성한 저장의 두 전투 경로를 유지합니다.
    UPROPERTY()
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    TArray<FRunEncounterOffer> Offers;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    FName SelectedEncounterId;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    bool bCompleted = false;
};
