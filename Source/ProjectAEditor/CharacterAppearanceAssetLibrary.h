#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterAppearanceAssetLibrary.generated.h"

class USkeletalMesh;

UCLASS()
class PROJECTAEDITOR_API UCharacterAppearanceAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Restore only merged material sections of the six original ROG Manny body parts without copying geometry.
    // 지오메트리를 복제하지 않고 ROG Manny 원본 신체 6부위의 합쳐진 재질 섹션만 복원합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static TArray<int32> RestoreBodyMaterialSlots(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart);

    // Return each body material slot's original Manny slot after checking every source and built triangle.
    // 소스와 빌드된 모든 삼각형을 검사한 뒤 신체 재질 슬롯별 원본 Manny 슬롯을 반환합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<int32> ValidateBodyMaterialSlots(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart);
};
