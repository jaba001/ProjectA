#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
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
    // Load a level with its external objects through the same engine flag used by asset deletion.
    // 에셋 삭제에서 사용하는 엔진 플래그로 레벨과 External Object를 함께 불러옵니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static UObject* LoadAssetWithExternalObjects(const FAssetData& AssetData);

    // Remove reviewed orphan packages through engine cleanup after their retired demo world is deleted.
    // 폐기된 데모 월드를 삭제한 뒤 검토된 고아 패키지를 엔진 정리 기능으로 제거합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool DeleteRetiredDemoExternalPackages(const TArray<FName>& PackageNames);

    // Finish package cleanup only after reviewed combat assets have disappeared from the registry.
    // 검토된 전투 에셋이 Registry에서 제거된 뒤에만 남은 패키지 정리를 마칩니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool CleanupDeletedCombatAssetPackages(const TArray<FName>& PackageNames);

    // Delete an audited closed combat asset group without retaining Python object wrappers.
    // Python 객체 래퍼를 유지하지 않고 감사한 닫힌 전투 에셋 묶음을 삭제합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool DeleteReviewedCombatAssets(const TArray<FAssetData>& AssetData);

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
