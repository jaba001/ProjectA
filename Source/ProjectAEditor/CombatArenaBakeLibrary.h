#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatArenaBakeLibrary.generated.h"

UCLASS()
class PROJECTAEDITOR_API UCombatArenaBakeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Bake all difficulty decorations from Gameplay; preserve existing Blueprint edits unless explicitly requested.
    // Gameplay에서 난이도별 장식을 굽고 명시 요청 없이는 기존 Blueprint 편집을 보존합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Arena")
    static bool BakeCombatArenaEnvironments(bool bOverwriteExisting, FString& OutReport);
};
