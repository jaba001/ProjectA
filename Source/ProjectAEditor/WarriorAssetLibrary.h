#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WarriorAssetLibrary.generated.h"

class UAnimBlueprint;
class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;
class UBlueprint;
class USkeleton;
class USkeletalMesh;
class UIKRetargeter;

// Restrict authored asset changes to project-owned copies while using engine editor APIs.
// 엔진 에디터 API를 사용하며 작성 에셋 변경을 프로젝트 작업 사본으로 제한합니다.
UCLASS()
class PROJECTAEDITOR_API UWarriorAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static UObject* LoadSavedAssetReference(const FSoftObjectPath& Path);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RemoveUnusedAssetRedirector(FName PackageName);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool AssignMeshSkeleton(USkeletalMesh* Mesh, USkeleton* Skeleton);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool SetSkeletonPreviewMesh(USkeleton* Skeleton, USkeletalMesh* Mesh);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RetargetAnimations(const TArray<UObject*>& Assets, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UIKRetargeter* Retargeter, const FString& Destination, const FString& Suffix);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool SetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName, FName SocketName);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static FName GetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<FName> GetAnimationSlotNames(UAnimBlueprint* Blueprint);

    // Return segment animations in slot order, preserving duplicates; reject missing references.
    // 슬롯 순서대로 구간 애니메이션을 반환하며 중복을 보존하고 누락된 참조는 거부합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<UAnimSequenceBase*> GetMontageAnimations(UAnimMontage* Montage);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ConfigureSwordMontage(UAnimMontage* Montage, UAnimSequence* Attack, UAnimSequence* Recovery, float RecoveryStartTime);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateSwordMontage(UAnimMontage* Montage, UAnimSequence* Attack, UAnimSequence* Recovery, float RecoveryStartTime);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool IsOutputSlotConnected(UAnimBlueprint* Blueprint, FName SlotName);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool EnsureOutputSlot(UAnimBlueprint* Blueprint, FName SlotName);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RemoveLegacyFootIK(UAnimBlueprint* Blueprint);
};
