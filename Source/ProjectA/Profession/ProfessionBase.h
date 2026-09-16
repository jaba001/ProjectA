#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "ProfessionBase.generated.h"

// Profession definitions hold class defaults independently of the shared combat actor and its visuals.
// 직업 정의는 공통 전투 액터와 외형에서 분리하여 클래스 기본값을 보관합니다.
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROJECTA_API UProfessionBase : public UObject
{
    GENERATED_BODY()

public:
    UProfessionBase();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profession")
    FName ClassId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profession")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profession", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float MaxHP;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float Strength;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float Dexterity;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float Intelligence;

    // Keep one ordered catalog for character creation and runtime profession lookup.
    // 캐릭터 생성과 런타임 직업 조회에 동일한 순서의 목록을 사용합니다.
    static TArray<TSubclassOf<UProfessionBase>> GetPlayableClasses();
    static const UProfessionBase* FindProfession(FName InClassId);
    static TArray<FName> GetPlayableIds();
};
