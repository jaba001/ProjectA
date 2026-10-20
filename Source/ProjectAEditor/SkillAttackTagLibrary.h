#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillAttackTagLibrary.generated.h"

UCLASS()
class PROJECTAEDITOR_API USkillAttackTagLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Add attack category tags to skill assets once; existing tags and later manual edits are never removed.
    // 스킬 에셋에 공격 분류 태그를 한 번 추가하며 기존 태그와 이후 수동 편집은 제거하지 않습니다.
    // Melee: round definitions that approach as melee. Magic: staff and spellbook weapon pool skills. Bows stay untagged.
    // 근접: 근접 실행 라운드 정의. 마법: 지팡이·마법서 무기 풀 스킬. 활 계열은 태그를 붙이지 않습니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Skills")
    static bool TagSkillAttackCategories(bool bSave, FString& OutReport);
};
