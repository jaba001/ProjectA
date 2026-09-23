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
class USkillDefinitionDataAsset;

// Author project assets through engine APIs; explicit import repair methods validate the selected original paths.
// 엔진 API로 프로젝트 에셋을 작성하며 명시적인 임포트 수정 함수는 선택한 원본 경로를 검증합니다.
UCLASS()
class PROJECTAEDITOR_API UWarriorAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Normalize the explicitly selected Witch import in place, including its bind pose and regenerated physics.
    // 명시적으로 선택한 마녀 임포트의 바인드 자세와 재생성 물리를 포함해 원래 경로에서 배율을 정리합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool NormalizeWitchImport(USkeletalMesh* Mesh);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool EnsureSelectedProfessionSlot(USkeleton* Skeleton);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateCharacterPhysics(USkeletalMesh* Mesh);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static UObject* LoadSavedAssetReference(const FSoftObjectPath& Path);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RemoveUnusedAssetRedirector(FName PackageName);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool AssignMeshSkeleton(USkeletalMesh* Mesh, USkeleton* Skeleton);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool SetSkeletonPreviewMesh(USkeleton* Skeleton, USkeletalMesh* Mesh);

    // Retargeted outputs and IK tools remain project-owned; external target skeleton slots must already exist.
    // 리타깃 출력과 IK 도구는 작업 사본으로 유지하며 외부 대상 스켈레톤의 슬롯은 이미 있어야 합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RetargetAnimations(const TArray<UObject*>& Assets, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UIKRetargeter* Retargeter, const FString& Destination, const FString& Suffix, bool bOverwriteExistingFiles = false, bool bIncludeReferencedAssets = true);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool SetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName, FName SocketName);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static FName GetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName);

    // Inspect the runtime blade sampler on Blueprint templates without spawning a world or playing a game.
    // 월드 생성이나 게임 실행 없이 Blueprint 템플릿에서 런타임 칼날 샘플러를 검사합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<FVector> SampleWeaponBlade(UBlueprint* Blueprint, USkillDefinitionDataAsset* SkillAsset, float MontageSeconds);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<FName> GetAnimationSlotNames(UAnimBlueprint* Blueprint);

    // Return segment animations in slot order, preserving duplicates; reject missing references.
    // 슬롯 순서대로 구간 애니메이션을 반환하며 중복을 보존하고 누락된 참조는 거부합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static TArray<UAnimSequenceBase*> GetMontageAnimations(UAnimMontage* Montage);

    // Author only the destination montage, preserving native source sequences and their skeleton.
    // 대상 몽타주만 작성하며 원본 시퀀스와 해당 스켈레톤을 보존합니다.
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
