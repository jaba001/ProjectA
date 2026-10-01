#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatContentAssetLibrary.generated.h"

class UBlueprint;

UCLASS()
class PROJECTAEDITOR_API UCombatContentAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Clear only retired skill defaults on the legacy command input, rejecting unexpected graph references.
    // 이전 명령 입력의 삭제 스킬 기본값만 해제하며 예상하지 않은 그래프 참조는 거절합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RemoveRetiredSkillPins(UBlueprint* Blueprint, int32& OutRemovedPins, TArray<FString>& OutDetails);
};
