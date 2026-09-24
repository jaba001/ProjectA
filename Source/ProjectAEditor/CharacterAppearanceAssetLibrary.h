#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterAppearanceAssetLibrary.generated.h"

class USkeletalMesh;
class USkeleton;

UCLASS()
class PROJECTAEDITOR_API UCharacterAppearanceAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Check both skeleton hierarchies and the body's actual bind pose before sharing animations.
    // 애니메이션 공유 전에 두 뼈대 계층과 몸체의 실제 바인드 자세를 검사합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateBodyAnimationSkeleton(USkeletalMesh* BodyMesh, USkeleton* AnimationSkeleton);

    // Enable the selected Primitive skeleton to share Manny animations with per-mesh body proportions.
    // 선택한 Primitive 뼈대가 메시별 체형을 유지하며 Manny 애니메이션을 공유하도록 설정합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ConfigureBodyAnimationSkeleton(USkeletalMesh* BodyMesh, USkeleton* AnimationSkeleton);

    // Restore only merged material sections of the six original ROG Manny body parts without copying geometry.
    // 지오메트리를 복제하지 않고 ROG Manny 원본 신체 6부위의 합쳐진 재질 섹션만 복원합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static TArray<int32> RestoreBodyMaterialSlots(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart);

    // Return each body material slot's original Manny slot after checking every source and built triangle.
    // 소스와 빌드된 모든 삼각형을 검사한 뒤 신체 재질 슬롯별 원본 Manny 슬롯을 반환합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<int32> ValidateBodyMaterialSlots(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart);
};
