#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetDeduplicationLibrary.generated.h"

class UAnimationAsset;
class UAnimBlueprint;
class USkeleton;
class USkeletalMesh;

UCLASS()
class PROJECTAEDITOR_API UAssetDeduplicationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Compare source geometry, generated LOD settings and reference bones; other mesh settings require separate checks.
    // 원본 형상, 자동 LOD 설정, 기준 뼈를 비교하며 기타 메시 설정은 별도 검사가 필요합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool HaveIdenticalSkeletalMeshGeometry(USkeletalMesh* Source, USkeletalMesh* Target);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool CanReplaceSkeleton(USkeleton* Source, USkeleton* Target);

    // Preserve authored montage slots when consolidating the two known mirrored enemy skeletons.
    // 알려진 적 스켈레톤 사본 두 개를 통합할 때 작성된 몽타주 슬롯을 보존합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool MergeCopiedSkeletonSlots(USkeleton* Source, USkeleton* Target);

    // Replace compatible skeleton references only in authored animations without converting animation poses.
    // 애니메이션 포즈를 변환하지 않고 프로젝트 작성 애니메이션의 호환 스켈레톤 참조만 교체합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ReplaceAnimationSkeleton(UAnimationAsset* Animation, USkeleton* Target);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool SetAnimationBlueprintPreviewMesh(UAnimBlueprint* Blueprint, USkeletalMesh* Mesh);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static USkeletalMesh* GetAnimationBlueprintPreviewMesh(UAnimBlueprint* Blueprint);

private:
    static bool CanReplaceSkeletonInternal(USkeleton* Source, USkeleton* Target, bool bIgnoreMissingSlots);
};
