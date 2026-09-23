#include "WarriorAssetLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/Library/CombatWeaponTraceLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAnimUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/ISlateNullRendererModule.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "UObject/CoreRedirects.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Unit/UnitBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWarriorAssetLibrary, Log, All);

namespace
{
    bool IsProjectCopy(const UObject* Object)
    {
        return IsValid(Object) && Object->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"));
    }

    bool Fail(const TCHAR* Reason)
    {
        UE_LOG(LogWarriorAssetLibrary, Error, TEXT("%s"), Reason);
        return false;
    }

    USCS_Node* FindComponentNode(UBlueprint* Blueprint, FName ComponentName)
    {
        if (!IsValid(Blueprint) || !Blueprint->SimpleConstructionScript || ComponentName.IsNone()) return nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->GetVariableName() == ComponentName) return Node;
        }
        return nullptr;
    }

    UAnimGraphNode_Root* FindOutputNode(UAnimBlueprint* Blueprint)
    {
        if (!IsValid(Blueprint)) return nullptr;
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph || Graph->GetFName() != TEXT("AnimGraph") || !Cast<UAnimationGraphSchema>(Graph->GetSchema())) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node)) return Root;
            }
        }
        return nullptr;
    }

    UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
    {
        if (!Node) return nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType)) return Pin;
        }
        return nullptr;
    }

    bool IsCompatibleReferencePose(const USkeleton* Source, const USkeleton* Target)
    {
        if (!IsValid(Source) || !IsValid(Target)) return false;
        const FReferenceSkeleton& SourceBones = Source->GetReferenceSkeleton();
        const FReferenceSkeleton& TargetBones = Target->GetReferenceSkeleton();
        if (TargetBones.GetRawBoneNum() == 0) return false;
        for (int32 Index = 0; Index < TargetBones.GetRawBoneNum(); ++Index)
        {
            const FName BoneName = TargetBones.GetBoneName(Index);
            const int32 SourceIndex = SourceBones.FindBoneIndex(BoneName);
            if (SourceIndex == INDEX_NONE) return false;
            const int32 SourceParent = SourceBones.GetParentIndex(SourceIndex);
            const int32 TargetParent = TargetBones.GetParentIndex(Index);
            const FName SourceParentName = SourceParent == INDEX_NONE ? NAME_None : SourceBones.GetBoneName(SourceParent);
            const FName TargetParentName = TargetParent == INDEX_NONE ? NAME_None : TargetBones.GetBoneName(TargetParent);
            if (SourceParentName != TargetParentName || !SourceBones.GetRefBonePose()[SourceIndex].Equals(TargetBones.GetRefBonePose()[Index], 0.0001))
            {
                UE_LOG(LogWarriorAssetLibrary, Error, TEXT("Reference hierarchy or pose differs for bone %s / 본 계층 또는 기준 자세가 다릅니다"), *BoneName.ToString());
                return false;
            }
        }
        return true;
    }

    bool ValidateExternalSkeletonSlots(const TArray<UObject*>& Assets, const USkeleton* Skeleton, bool bIncludeReferencedAssets)
    {
        if (IsProjectCopy(Skeleton)) return true;
        TArray<UAnimationAsset*> Animations;
        TSet<const UAnimBlueprint*> VisitedBlueprints;
        for (UObject* Asset : Assets)
        {
            if (!IsValid(Asset)) return false;
            if (UAnimationAsset* Animation = Cast<UAnimationAsset>(Asset)) Animations.AddUnique(Animation);
            UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Asset);
            while (Blueprint && !VisitedBlueprints.Contains(Blueprint))
            {
                VisitedBlueprints.Add(Blueprint);
                for (FName SlotName : UWarriorAssetLibrary::GetAnimationSlotNames(Blueprint))
                {
                    if (!Skeleton->ContainsSlotName(SlotName)) return Fail(TEXT("Retargeting cannot add animation slots to an external skeleton / 리타깃은 외부 스켈레톤에 애니메이션 슬롯을 추가할 수 없습니다"));
                }
                if (bIncludeReferencedAssets) EditorAnimUtils::GetAllAnimationSequencesReferredInBlueprint(Blueprint, Animations);
                Blueprint = Blueprint->ParentClass ? Cast<UAnimBlueprint>(Blueprint->ParentClass->ClassGeneratedBy) : nullptr;
            }
        }
        for (int32 Index = 0; Index < Animations.Num(); ++Index)
        {
            UAnimationAsset* Animation = Animations[Index];
            if (!IsValid(Animation)) return false;
            if (const UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
            {
                for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
                {
                    if (!Skeleton->ContainsSlotName(Track.SlotName)) return Fail(TEXT("Retargeting cannot add montage slots to an external skeleton / 리타깃은 외부 스켈레톤에 몽타주 슬롯을 추가할 수 없습니다"));
                }
            }
            if (bIncludeReferencedAssets) Animation->HandleAnimReferenceCollection(Animations, true);
        }
        return true;
    }
}

bool UWarriorAssetLibrary::ValidateCharacterPhysics(USkeletalMesh* Mesh)
{
    if (!IsValid(Mesh) || !IsValid(Mesh->GetPhysicsAsset())) return false;
    const UPhysicsAsset* Physics = Mesh->GetPhysicsAsset();
    TSet<FName> Bones;
    for (const USkeletalBodySetup* Body : Physics->SkeletalBodySetups)
    {
        if (!IsValid(Body) || Bones.Contains(Body->BoneName) || Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName) == INDEX_NONE || Body->AggGeom.GetElementCount() == 0) return false;
        Bones.Add(Body->BoneName);
    }
    if (Bones.Num() < 6) return false;
    TSet<FName> Connected = {Physics->SkeletalBodySetups[0]->BoneName};
    for (int32 Pass = 0; Pass < Bones.Num(); ++Pass)
    {
        for (const UPhysicsConstraintTemplate* Constraint : Physics->ConstraintSetup)
        {
            if (!IsValid(Constraint)) return false;
            const FConstraintInstance& Instance = Constraint->DefaultInstance;
            if (!Bones.Contains(Instance.ConstraintBone1) || !Bones.Contains(Instance.ConstraintBone2) || Instance.GetLinearXMotion() != LCM_Locked || Instance.GetLinearYMotion() != LCM_Locked || Instance.GetLinearZMotion() != LCM_Locked) return false;
            if (Connected.Contains(Instance.ConstraintBone1) || Connected.Contains(Instance.ConstraintBone2))
            {
                Connected.Add(Instance.ConstraintBone1);
                Connected.Add(Instance.ConstraintBone2);
            }
        }
    }
    UE_LOG(LogWarriorAssetLibrary, Display, TEXT("Physics structure / 물리 구조: %s bodies=%d constraints=%d connected=%d"), *Mesh->GetName(), Bones.Num(), Physics->ConstraintSetup.Num(), Connected.Num());
    for (FName Bone : Bones)
    {
        if (!Connected.Contains(Bone)) UE_LOG(LogWarriorAssetLibrary, Display, TEXT("Disconnected body / 분리된 바디: %s"), *Bone.ToString());
    }
    return Connected.Num() == Bones.Num();
}

bool UWarriorAssetLibrary::EnsureSelectedProfessionSlot(USkeleton* Skeleton)
{
    if (!IsValid(Skeleton)) return false;
    const FString Path = Skeleton->GetPathName();
    if (Path != TEXT("/Game/Assassin/Mesh/SK_Assassin_Skeleton.SK_Assassin_Skeleton") && Path != TEXT("/Game/stylized_dark_witch_fbx__extracted/Stylized_Dark_Witch_Skeleton.Stylized_Dark_Witch_Skeleton")) return false;
    if (!Skeleton->ContainsSlotName(TEXT("DefaultSlot")))
    {
        Skeleton->Modify();
        Skeleton->SetSlotGroupName(TEXT("DefaultSlot"), TEXT("DefaultGroup"));
        Skeleton->MarkPackageDirty();
    }
    return true;
}

bool UWarriorAssetLibrary::NormalizeWitchImport(USkeletalMesh* Mesh)
{
    if (!IsValid(Mesh) || Mesh->GetPathName() != TEXT("/Game/stylized_dark_witch_fbx__extracted/Stylized_Dark_Witch.Stylized_Dark_Witch") || !IsValid(Mesh->GetSkeleton()) || !IsValid(Mesh->GetPhysicsAsset())) return Fail(TEXT("Only the selected Witch import can be normalized / 선택한 마녀 임포트만 정규화할 수 있습니다"));
    FSkinnedAssetCompilingManager::Get().FinishCompilation({Mesh});
    const FReferenceSkeleton Original = Mesh->GetRefSkeleton();
    if (Original.GetRawBoneNum() == 0 || Original.GetBoneName(0) != TEXT("rig")) return false;
    const bool bUnitScale = Original.GetRefBonePose()[0].GetScale3D().Equals(FVector::OneVector, 0.001);
    const int32 CapeIndex = Original.FindBoneIndex(TEXT("Cape1"));
    if (bUnitScale && CapeIndex != INDEX_NONE && Original.GetParentIndex(CapeIndex) == Original.FindBoneIndex(TEXT("DEF-spine_003"))) return Mesh->GetBounds().BoxExtent.Z > 80.0 && Mesh->GetBounds().BoxExtent.Z < 110.0;
    if (!bUnitScale && !Original.GetRefBonePose()[0].GetScale3D().Equals(FVector(100.0), 0.001)) return Fail(TEXT("Unexpected Witch import scale / 예상하지 못한 마녀 임포트 배율입니다"));
    for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD)
    {
        if (!Mesh->GetMeshDescription(LOD)) return Fail(TEXT("Normalization requires editable source geometry at every LOD / 모든 LOD에 편집 가능한 원본 형상이 필요합니다"));
    }
    const double GeometryScale = bUnitScale ? 1.0 : 0.12;
    TArray<FTransform> OriginalGlobal;
    TArray<FTransform> NormalizedGlobal;
    FReferenceSkeleton Normalized;
    for (int32 Index = 0; Index < Original.GetRawBoneNum(); ++Index)
    {
        FMeshBoneInfo Info = Original.GetRawRefBoneInfo()[Index];
        const FTransform Global = Info.ParentIndex == INDEX_NONE ? Original.GetRefBonePose()[Index] : Original.GetRefBonePose()[Index] * OriginalGlobal[Info.ParentIndex];
        OriginalGlobal.Add(Global);
        NormalizedGlobal.Add(FTransform(Global.GetRotation(), Global.GetTranslation() * GeometryScale, FVector::OneVector));
        // Connect weighted deform bones directly, retaining their world bind pose and all existing names.
        // 가중치가 있는 변형 본을 직접 연결하며 월드 바인드 자세와 기존 본 이름을 유지합니다.
        const FString Name = Info.Name.ToString();
        FName ParentOverride = NAME_None;
        if (Name == TEXT("DEF-thigh_L") || Name == TEXT("DEF-thigh_R")) ParentOverride = TEXT("DEF-spine");
        if (Name == TEXT("DEF-pelvis_L") || Name == TEXT("DEF-pelvis_R")) ParentOverride = TEXT("DEF-spine");
        if (Name == TEXT("DEF-breast_L") || Name == TEXT("DEF-breast_R") || Name == TEXT("Cape1")) ParentOverride = TEXT("DEF-spine_003");
        if (Name == TEXT("DEF-shoulder_L") || Name == TEXT("DEF-shoulder_R")) ParentOverride = TEXT("DEF-spine_003");
        if (Name == TEXT("DEF-upper_arm_L")) ParentOverride = TEXT("DEF-shoulder_L");
        if (Name == TEXT("DEF-upper_arm_R")) ParentOverride = TEXT("DEF-shoulder_R");
        if (Name == TEXT("Eye_L") || Name == TEXT("Eye_R")) ParentOverride = TEXT("DEF-spine_006");
        if (!ParentOverride.IsNone()) Info.ParentIndex = Original.FindBoneIndex(ParentOverride);
        if (Info.ParentIndex >= Index) return Fail(TEXT("Invalid normalized bone ordering / 정규화 본 순서가 올바르지 않습니다"));
        FTransform Local = Info.ParentIndex == INDEX_NONE ? NormalizedGlobal[Index] : NormalizedGlobal[Index].GetRelativeTransform(NormalizedGlobal[Info.ParentIndex]);
        Local.NormalizeRotation();
        FReferenceSkeletonModifier Modifier(Normalized, Mesh->GetSkeleton());
        Modifier.Add(Info, Local);
    }
    Mesh->Modify();
    Mesh->GetSkeleton()->Modify();
    for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD)
    {
        Mesh->ModifyMeshDescription(LOD);
        FMeshDescription* Description = Mesh->GetMeshDescription(LOD);
        FSkeletalMeshAttributes Attributes(*Description);
        Attributes.Register();
        const bool bCreateBoneAttributes = Attributes.Bones().Num() == 0;
        TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
        for (FVertexID Vertex : Description->Vertices().GetElementIDs()) Positions[Vertex] *= GeometryScale;
        for (FName Morph : Attributes.GetMorphTargetNames())
        {
            TVertexAttributesRef<FVector3f> Deltas = Attributes.GetVertexMorphPositionDelta(Morph);
            for (FVertexID Vertex : Description->Vertices().GetElementIDs()) Deltas[Vertex] *= GeometryScale;
        }
        for (int32 Index = 0; Index < Normalized.GetRawBoneNum(); ++Index)
        {
            const FBoneID Bone(Index);
            if (bCreateBoneAttributes)
            {
                Attributes.CreateBone(Bone);
                Attributes.GetBoneNames()[Bone] = Normalized.GetBoneName(Index);
            }
            if (Attributes.GetBoneNames()[Bone] != Normalized.GetBoneName(Index)) return Fail(TEXT("Source geometry bone order differs / 원본 형상의 본 순서가 다릅니다"));
            Attributes.GetBonePoses()[Bone] = Normalized.GetRefBonePose()[Index];
            Attributes.GetBoneParentIndices()[Bone] = Normalized.GetParentIndex(Index);
        }
        if (!Mesh->CommitMeshDescription(LOD)) return false;
    }
    Mesh->SetRefSkeleton(Normalized);
    Mesh->GetRefBasesInvMatrix().Reset();
    Mesh->CalculateInvRefMatrices();
    Mesh->GetSkeleton()->RecreateBoneTree(Mesh);
    Mesh->GetSkeleton()->UpdateReferencePoseFromMesh(Mesh);
    Mesh->SetImportedBounds(FBoxSphereBounds(Mesh->GetImportedBounds().Origin * GeometryScale, Mesh->GetImportedBounds().BoxExtent * GeometryScale, Mesh->GetImportedBounds().SphereRadius * GeometryScale));
    Mesh->PostEditChange();
    FSkinnedAssetCompilingManager::Get().FinishCompilation({Mesh});
    UPhysicsAsset* Physics = Mesh->GetPhysicsAsset();
    Physics->Modify();
    Physics->SkeletalBodySetups.Reset();
    Physics->ConstraintSetup.Reset();
    Physics->CollisionDisableTable.Reset();
    Physics->UpdateBodySetupIndexMap();
    Physics->UpdateBoundsBodiesArray();
    FPhysAssetCreateParams Params;
    Params.MinBoneSize = 5.0f;
    FText Error;
    if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Physics, Mesh, Params, Error, true, false))
    {
        UE_LOG(LogWarriorAssetLibrary, Error, TEXT("Witch physics creation failed / 마녀 물리 생성 실패: %s"), *Error.ToString());
        return false;
    }
    Physics->MarkPackageDirty();
    Mesh->GetSkeleton()->MarkPackageDirty();
    Mesh->MarkPackageDirty();
    return true;
}

UObject* UWarriorAssetLibrary::LoadSavedAssetReference(const FSoftObjectPath& Path)
{
    return Path.TryLoad();
}

bool UWarriorAssetLibrary::RemoveUnusedAssetRedirector(FName PackageName)
{
    const FString PackagePath = PackageName.ToString();
    if (!PackagePath.StartsWith(TEXT("/Game/User_JeHoon/")) || !FPackageName::IsValidLongPackageName(PackagePath)) return Fail(TEXT("Redirector cleanup requires an exact project-owned package / Redirector 정리에는 정확한 작업 사본 패키지가 필요합니다"));
    const FCoreRedirectObjectName OriginalPackage(NAME_None, NAME_None, PackageName);
    if (FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, OriginalPackage) != OriginalPackage) return Fail(TEXT("Remove the package core redirect before cleaning its asset redirector / 에셋 Redirector 정리 전에 해당 패키지의 Core Redirect를 제외해야 합니다"));
    FString PackageFilename;
    if (!FPackageName::DoesPackageExist(PackagePath, &PackageFilename)) return true;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    Registry.ScanFilesSynchronous({PackageFilename}, true);
    TArray<FAssetData> Assets;
    if (!Registry.GetAssetsByPackageName(PackageName, Assets, true, false) || Assets.Num() != 1 || Assets[0].AssetClassPath != UObjectRedirector::StaticClass()->GetClassPathName()) return Fail(TEXT("Only a package containing exactly one asset redirector can be removed / 에셋 Redirector 하나만 포함된 패키지만 제거할 수 있습니다"));
    TArray<FName> Referencers;
    if (!Registry.GetReferencers(PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package) || !Referencers.IsEmpty()) return Fail(TEXT("The redirector still has package references or its reference data is unavailable / Redirector의 패키지 참조가 남아 있거나 참조 정보를 확인할 수 없습니다"));
    const FString ObjectPath = Assets[0].GetObjectPathString();
    const FCoreRedirectObjectName OriginalObject(ObjectPath);
    if (FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Object, OriginalObject) != OriginalObject) return Fail(TEXT("Remove the object core redirect before cleaning its asset redirector / 에셋 Redirector 정리 전에 해당 오브젝트의 Core Redirect를 제외해야 합니다"));
    UObjectRedirector* Redirector = Cast<UObjectRedirector>(StaticLoadObject(UObjectRedirector::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoRedirects));
    if (!IsValid(Redirector) || Redirector->GetClass() != UObjectRedirector::StaticClass() || Redirector->GetPathName() != ObjectPath || Redirector->GetOutermost()->GetFName() != PackageName) return Fail(TEXT("The exact asset redirector could not be loaded without redirection / 경로 변경 없이 정확한 에셋 Redirector를 불러올 수 없습니다"));
    UPackage* Package = Redirector->GetOutermost();
    Package->FullyLoad();
    TArray<UObject*> PackageObjects;
    GetObjectsWithPackage(Package, PackageObjects);
    if (PackageObjects.Num() != 1 || PackageObjects[0] != Redirector) return Fail(TEXT("The loaded package contains objects other than the single redirector / 불러온 패키지에 단일 Redirector 외의 오브젝트가 있습니다"));
    // Registry and exact package checks replace the transient Python reference check for this redirector only.
    // 이 Redirector에 한해 레지스트리 및 정확한 패키지 검사로 Python 임시 참조 검사를 대체합니다.
    if (!ObjectTools::DeleteSingleObject(Redirector, false)) return false;
    ObjectTools::CleanupAfterSuccessfulDelete({Package}, false);
    return !FPackageName::DoesPackageExist(PackagePath);
}

bool UWarriorAssetLibrary::AssignMeshSkeleton(USkeletalMesh* Mesh, USkeleton* Skeleton)
{
    if (!IsProjectCopy(Mesh) || !IsProjectCopy(Skeleton) || !IsCompatibleReferencePose(Mesh->GetSkeleton(), Skeleton)) return Fail(TEXT("Mesh skeleton assignment requires compatible project copies / 호환되는 메시와 스켈레톤 작업 사본이 필요합니다"));
    Mesh->Modify();
    Mesh->SetSkeleton(Skeleton);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    return Mesh->GetSkeleton() == Skeleton;
}

bool UWarriorAssetLibrary::SetSkeletonPreviewMesh(USkeleton* Skeleton, USkeletalMesh* Mesh)
{
    if (!IsProjectCopy(Skeleton) || !IsProjectCopy(Mesh) || Mesh->GetSkeleton() != Skeleton) return Fail(TEXT("Skeleton preview requires project copies sharing the same skeleton / 스켈레톤 미리보기에는 동일한 스켈레톤을 사용하는 작업 사본이 필요합니다"));
    Skeleton->SetPreviewMesh(Mesh);
    return Skeleton->GetPreviewMesh(false) == Mesh;
}

bool UWarriorAssetLibrary::RetargetAnimations(const TArray<UObject*>& Assets, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UIKRetargeter* Retargeter, const FString& Destination, const FString& Suffix, bool bOverwriteExistingFiles, bool bIncludeReferencedAssets)
{
    if (Assets.IsEmpty() || !IsValid(SourceMesh) || !IsValid(SourceMesh->GetSkeleton()) || !IsValid(TargetMesh) || !IsValid(TargetMesh->GetSkeleton()) || !IsProjectCopy(Retargeter) || !Destination.StartsWith(TEXT("/Game/User_JeHoon/")) || !FPackageName::IsValidLongPackageName(Destination) || Destination.Contains(TEXT("..")) || Destination.EndsWith(TEXT("/")) || Suffix.IsEmpty() || Suffix.Contains(TEXT("/"))) return Fail(TEXT("IK batch authoring requires valid meshes and a project-owned output directory / IK 일괄 작성에는 유효한 메시와 작업 사본 출력 폴더가 필요합니다"));
    if (bOverwriteExistingFiles && bIncludeReferencedAssets) return Fail(TEXT("Sequence overwrite must exclude referenced assets / 시퀀스 덮어쓰기에는 참조 에셋을 포함할 수 없습니다"));
    // Engine compilation registers missing slots, so external skeletons must already contain every required slot.
    // 엔진 컴파일은 누락 슬롯을 등록하므로 외부 스켈레톤에는 필요한 모든 슬롯이 이미 있어야 합니다.
    if (!ValidateExternalSkeletonSlots(Assets, TargetMesh->GetSkeleton(), bIncludeReferencedAssets)) return false;
    FIKRetargetBatchOperationContext Context;
    TSet<FString> InputPaths;
    TSet<FString> OutputPaths;
    TMap<FString, TWeakObjectPtr<UAnimSequence>> PreviousOutputs;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    for (const UObject* Asset : Assets)
    {
        if (!IsValid(Asset)) return false;
        InputPaths.Add(Asset->GetPathName());
    }
    for (UObject* Asset : Assets)
    {
        const FString Name = Asset->GetName() + Suffix;
        const FString PackagePath = Destination / Name;
        const FString ObjectPath = PackagePath + TEXT(".") + Name;
        if (!FPackageName::IsValidObjectPath(ObjectPath) || OutputPaths.Contains(ObjectPath) || InputPaths.Contains(ObjectPath)) return Fail(TEXT("Retarget output names must be valid, unique, and distinct from inputs / 리타깃 출력 이름은 유효하고 고유하며 입력과 달라야 합니다"));
        OutputPaths.Add(ObjectPath);
        if (bOverwriteExistingFiles)
        {
            if (!Cast<UAnimSequence>(Asset)) return Fail(TEXT("Overwrite accepts animation sequences only / 덮어쓰기에는 애니메이션 시퀀스만 사용할 수 있습니다"));
            const FCoreRedirectObjectName OriginalPackage(NAME_None, NAME_None, FName(*PackagePath));
            const FCoreRedirectObjectName OriginalObject(ObjectPath);
            if (FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, OriginalPackage) != OriginalPackage || FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Object, OriginalObject) != OriginalObject) return Fail(TEXT("Overwrite requires an output path without core redirects / 덮어쓰기 출력 경로에는 Core Redirect가 없어야 합니다"));
            const FAssetData ExistingData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
            UObject* ExistingObject = ExistingData.IsValid() ? ExistingData.GetAsset() : FindObject<UObject>(nullptr, *ObjectPath);
            if (ExistingData.IsValid() || ExistingObject || FPackageName::DoesPackageExist(PackagePath))
            {
                UAnimSequence* ExistingSequence = Cast<UAnimSequence>(ExistingObject);
                if (!IsProjectCopy(ExistingSequence) || ExistingSequence->GetPathName() != ObjectPath || ExistingSequence->GetClass() != Asset->GetClass() || ExistingSequence->GetSkeleton() != TargetMesh->GetSkeleton()) return Fail(TEXT("Overwrite requires the exact project sequence with the target skeleton / 덮어쓰기에는 대상 스켈레톤을 사용하는 정확한 작업 사본 시퀀스가 필요합니다"));
                PreviousOutputs.Add(ObjectPath, ExistingSequence);
            }
        }
        Context.AssetsToRetarget.Add(Asset);
    }
    Context.SourceMesh = SourceMesh;
    Context.TargetMesh = TargetMesh;
    Context.IKRetargetAsset = Retargeter;
    // The Python convenience API defaults to /Game and invokes Slate; specify the owned folder explicitly.
    // Python 편의 API는 /Game을 기본값으로 사용하고 Slate를 호출하므로 작업 사본 폴더를 명시합니다.
    Context.NameRule.FolderPath = Destination;
    Context.NameRule.Suffix = Suffix;
    Context.bUseSourcePath = false;
    Context.bOverwriteExistingFiles = bOverwriteExistingFiles;
    Context.bIncludeReferencedAssets = bIncludeReferencedAssets;
    // The engine batch utility accesses Slate even in unattended asset authoring; no editor window or gameplay is needed.
    // 엔진 일괄 도구의 무인 에셋 작성에도 Slate가 필요하며 에디터 창이나 게임 실행은 필요하지 않습니다.
    if (IsRunningCommandlet() && !FSlateApplication::IsInitialized())
    {
        ISlateNullRendererModule& Renderer = FModuleManager::LoadModuleChecked<ISlateNullRendererModule>(TEXT("SlateNullRenderer"));
        FSlateApplication::InitializeAsStandaloneApplication(Renderer.CreateSlateNullRenderer());
        FSlateNotificationManager::Get().SetAllowNotifications(false);
    }
    TStrongObjectPtr<UIKRetargetBatchOperation> Operation(NewObject<UIKRetargetBatchOperation>());
    Operation->RunRetarget(Context);
    for (const FString& ObjectPath : OutputPaths)
    {
        UObject* Output = LoadObject<UObject>(nullptr, *ObjectPath);
        if (!IsProjectCopy(Output) || Output->GetPathName() != ObjectPath) return false;
        if (bOverwriteExistingFiles)
        {
            UAnimSequence* Sequence = Cast<UAnimSequence>(Output);
            const TWeakObjectPtr<UAnimSequence>* Previous = PreviousOutputs.Find(ObjectPath);
            if (!Sequence || Sequence->GetSkeleton() != TargetMesh->GetSkeleton() || (Previous && Previous->Get() == Sequence)) return Fail(TEXT("Sequence overwrite did not produce the expected replacement / 시퀀스 덮어쓰기에서 예상한 교체 결과가 생성되지 않았습니다"));
        }
    }
    return true;
}

bool UWarriorAssetLibrary::SetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName, FName SocketName)
{
    if (!IsProjectCopy(Blueprint) || !Blueprint->GeneratedClass || SocketName.IsNone()) return Fail(TEXT("Weapon attachment requires a project-owned Blueprint and named socket / 작업 사본 Blueprint와 소켓 이름이 필요합니다"));
    USCS_Node* Node = FindComponentNode(Blueprint, ComponentName);
    UStaticMeshComponent* Weapon = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
    AActor* Defaults = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    USkeletalMeshComponent* Mesh = Defaults ? Defaults->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Weapon || !Mesh || !Mesh->DoesSocketExist(SocketName) || !Node->bIsParentComponentNative || Node->ParentComponentOrVariableName != Mesh->GetFName()) return Fail(TEXT("Weapon must already be attached to the native skeletal mesh with a valid bone or socket / 무기는 유효한 본 또는 소켓을 가진 네이티브 스켈레탈 메시의 자식이어야 합니다"));
    Blueprint->Modify();
    Node->Modify();
    Weapon->Modify();
    // SCS attachment metadata is authoritative when the Blueprint creates a component instance.
    // Blueprint가 컴포넌트 인스턴스를 생성할 때 SCS 부착 메타데이터를 기준으로 사용합니다.
    Node->AttachToName = SocketName;
    Weapon->SetupAttachment(Mesh, SocketName);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error && GetWeaponAttachment(Blueprint, ComponentName) == SocketName;
}

FName UWarriorAssetLibrary::GetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName)
{
    const USCS_Node* Node = FindComponentNode(Blueprint, ComponentName);
    return Node ? Node->AttachToName : NAME_None;
}

TArray<FVector> UWarriorAssetLibrary::SampleWeaponBlade(UBlueprint* Blueprint, USkillDefinitionDataAsset* SkillAsset, float MontageSeconds)
{
    if (!IsValid(Blueprint) || !Blueprint->GeneratedClass || !IsValid(SkillAsset)) return {};
    FCombatRoundSkill Skill;
    FText Error;
    if (!SkillAsset->ResolveRoundSkill(Skill, Error) || !Skill.bUseWeaponTrace) return {};
    const AUnitBase* Defaults = Cast<AUnitBase>(Blueprint->GeneratedClass->GetDefaultObject());
    const USCS_Node* Node = FindComponentNode(Blueprint, Skill.WeaponComponentName);
    const UStaticMeshComponent* Weapon = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
    const USkeletalMeshComponent* Mesh = Defaults ? Defaults->GetMesh() : nullptr;
    if (!Weapon || !Weapon->GetStaticMesh() || !Mesh) return {};
    const UStaticMeshSocket* Base = Weapon->GetStaticMesh()->FindSocket(Skill.WeaponBaseSocket);
    const UStaticMeshSocket* Tip = Weapon->GetStaticMesh()->FindSocket(Skill.WeaponTipSocket);
    FTransform Hand;
    if (!Base || !Tip || !CombatWeaponTrace::SampleBoneTransform(Mesh->GetSkeletalMeshAsset(), Defaults->ResolveRoundCastMontage(Skill.CastMontage), Skill.WeaponMontageSlot, Node->AttachToName, MontageSeconds, Hand)) return {};
    const FTransform WeaponToActor = Weapon->GetRelativeTransform() * Hand * Mesh->GetRelativeTransform();
    return {WeaponToActor.TransformPosition(Base->RelativeLocation), WeaponToActor.TransformPosition(Tip->RelativeLocation)};
}

TArray<FName> UWarriorAssetLibrary::GetAnimationSlotNames(UAnimBlueprint* Blueprint)
{
    TArray<FName> Names;
    if (!IsValid(Blueprint)) return Names;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (const UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node)) Names.AddUnique(Slot->Node.SlotName);
        }
    }
    return Names;
}

TArray<UAnimSequenceBase*> UWarriorAssetLibrary::GetMontageAnimations(UAnimMontage* Montage)
{
    TArray<UAnimSequenceBase*> Animations;
    if (!IsValid(Montage)) return Animations;
    for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
    {
        for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
        {
            UAnimSequenceBase* Animation = Segment.GetAnimReference();
            if (!IsValid(Animation)) return {};
            Animations.Add(Animation);
        }
    }
    return Animations;
}

bool UWarriorAssetLibrary::ConfigureSwordMontage(UAnimMontage* Montage, UAnimSequence* Attack, UAnimSequence* Recovery, float RecoveryStartTime)
{
    if (!IsProjectCopy(Montage) || !IsValid(Attack) || !IsValid(Recovery) || !IsValid(Montage->GetSkeleton())) return Fail(TEXT("Sword montage authoring requires a project-owned montage and valid source animations and skeleton / 검 몽타주 작성에는 작업 사본 몽타주와 유효한 원본 애니메이션 및 스켈레톤이 필요합니다"));
    if (Attack->GetSkeleton() != Montage->GetSkeleton() || Recovery->GetSkeleton() != Montage->GetSkeleton()) return Fail(TEXT("Sword montage animations must share its exact skeleton / 검 몽타주 애니메이션은 동일한 스켈레톤을 사용해야 합니다"));
    if (!IsProjectCopy(Montage->GetSkeleton()) && !Montage->GetSkeleton()->ContainsSlotName(FAnimSlotGroup::DefaultSlotName)) return Fail(TEXT("The external sword skeleton must already contain DefaultSlot / 외부 검 스켈레톤에 DefaultSlot이 이미 있어야 합니다"));
    const float AttackLength = Attack->GetPlayLength();
    const float RecoveryLength = Recovery->GetPlayLength();
    if (!FMath::IsFinite(AttackLength) || AttackLength <= 0.f || !FMath::IsFinite(RecoveryLength) || RecoveryLength <= 0.f || Attack->RateScale != 1.f || Recovery->RateScale != 1.f || !FMath::IsFinite(RecoveryStartTime) || RecoveryStartTime < 0.f || RecoveryStartTime >= RecoveryLength) return Fail(TEXT("Sword montage source lengths, rates or recovery start are invalid / 검 몽타주 원본 길이, 재생 속도 또는 회복 시작점이 유효하지 않습니다"));
    if (Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].SlotName != FAnimSlotGroup::DefaultSlotName || Montage->CompositeSections.Num() != 1 || Montage->CompositeSections[0].SectionName != TEXT("Default")) return Fail(TEXT("Sword montage authoring requires the single default factory slot and section / 검 몽타주 작성에는 팩토리 기본 슬롯과 섹션 각 하나가 필요합니다"));
    const float ExpectedLength = AttackLength + RecoveryLength - RecoveryStartTime;
    if (!FMath::IsFinite(ExpectedLength) || !Attack->GetSamplingFrameRate().IsValid() || !Recovery->GetSamplingFrameRate().IsValid()) return Fail(TEXT("Sword montage duration or source frame rates are invalid / 검 몽타주 길이 또는 원본 프레임 레이트가 유효하지 않습니다"));
    FAnimSegment AttackSegment;
    AttackSegment.SetAnimReference(Attack, true);
    FAnimSegment RecoverySegment;
    RecoverySegment.SetAnimReference(Recovery, true);
    RecoverySegment.StartPos = AttackLength;
    RecoverySegment.AnimStartTime = RecoveryStartTime;
    Montage->Modify();
    Montage->SlotAnimTracks[0].AnimTrack.AnimSegments = {AttackSegment, RecoverySegment};
    Montage->CompositeSections.Reset();
    FCompositeSection& Section = Montage->CompositeSections.AddDefaulted_GetRef();
    Section.SectionName = TEXT("Default");
    Section.SetTime(0.f);
    Section.NextSectionName = NAME_None;
    Montage->RateScale = 1.f;
    Montage->BlendModeIn = EMontageBlendMode::Standard;
    Montage->BlendModeOut = EMontageBlendMode::Standard;
    Montage->BlendIn.SetBlendTime(0.08f);
    Montage->BlendOut.SetBlendTime(0.12f);
    Montage->BlendOutTriggerTime = -1.f;
    Montage->bEnableAutoBlendOut = true;
    // Update the common sampling rate before quantizing the combined duration into montage frames.
    // 합산 길이를 몽타주 프레임으로 변환하기 전에 공통 샘플링 레이트를 갱신합니다.
    static_cast<UAnimCompositeBase*>(Montage)->UpdateCommonTargetFrameRate();
    Montage->GetController().SetFrameRate(Montage->GetSamplingFrameRate());
    Montage->SetCompositeLength(Montage->CalculateSequenceLength());
    Montage->PostEditChange();
    Montage->MarkPackageDirty();
    return ValidateSwordMontage(Montage, Attack, Recovery, RecoveryStartTime);
}

bool UWarriorAssetLibrary::ValidateSwordMontage(UAnimMontage* Montage, UAnimSequence* Attack, UAnimSequence* Recovery, float RecoveryStartTime)
{
    if (!IsProjectCopy(Montage) || !IsValid(Attack) || !IsValid(Recovery) || !IsValid(Montage->GetSkeleton())) return Fail(TEXT("Sword montage verification requires a project-owned montage and valid source animations and skeleton / 검 몽타주 검증에는 작업 사본 몽타주와 유효한 원본 애니메이션 및 스켈레톤이 필요합니다"));
    if (Attack->GetSkeleton() != Montage->GetSkeleton() || Recovery->GetSkeleton() != Montage->GetSkeleton()) return Fail(TEXT("Sword montage skeleton verification failed / 검 몽타주 스켈레톤 검증 실패"));
    if (!IsProjectCopy(Montage->GetSkeleton()) && !Montage->GetSkeleton()->ContainsSlotName(FAnimSlotGroup::DefaultSlotName)) return Fail(TEXT("The external sword skeleton must already contain DefaultSlot / 외부 검 스켈레톤에 DefaultSlot이 이미 있어야 합니다"));
    const float AttackLength = Attack->GetPlayLength();
    const float RecoveryLength = Recovery->GetPlayLength();
    if (!FMath::IsFinite(AttackLength) || AttackLength <= 0.f || !FMath::IsFinite(RecoveryLength) || RecoveryLength <= 0.f || Attack->RateScale != 1.f || Recovery->RateScale != 1.f || !FMath::IsFinite(RecoveryStartTime) || RecoveryStartTime < 0.f || RecoveryStartTime >= RecoveryLength) return Fail(TEXT("Sword montage source length, rate or recovery start verification failed / 검 몽타주 원본 길이, 재생 속도 또는 회복 시작점 검증 실패"));
    const float ExpectedLength = AttackLength + RecoveryLength - RecoveryStartTime;
    if (Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].SlotName != FAnimSlotGroup::DefaultSlotName || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 2 || Montage->CompositeSections.Num() != 1 || Montage->CompositeSections[0].SectionName != TEXT("Default") || Montage->CompositeSections[0].GetTime() != 0.f || !Montage->CompositeSections[0].NextSectionName.IsNone()) return Fail(TEXT("Sword montage slot or section verification failed / 검 몽타주 슬롯 또는 섹션 검증 실패"));
    const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
    if (Segments[0].GetAnimReference() != Attack || Segments[1].GetAnimReference() != Recovery || Segments[0].StartPos != 0.f || Segments[0].AnimStartTime != 0.f || Segments[0].AnimEndTime != AttackLength || Segments[1].StartPos != AttackLength || Segments[1].AnimStartTime != RecoveryStartTime || Segments[1].AnimEndTime != RecoveryLength || Segments[0].AnimPlayRate != 1.f || Segments[1].AnimPlayRate != 1.f || Segments[0].LoopingCount != 1 || Segments[1].LoopingCount != 1) return Fail(TEXT("Sword montage animation segment verification failed / 검 몽타주 애니메이션 구간 검증 실패"));
    if (Montage->BlendModeIn != EMontageBlendMode::Standard || Montage->BlendModeOut != EMontageBlendMode::Standard || !FMath::IsNearlyEqual(Montage->BlendIn.GetBlendTime(), 0.08f, 0.0001f) || !FMath::IsNearlyEqual(Montage->BlendOut.GetBlendTime(), 0.12f, 0.0001f) || Montage->BlendOutTriggerTime != -1.f || !Montage->bEnableAutoBlendOut) return Fail(TEXT("Sword montage blend verification failed / 검 몽타주 블렌드 검증 실패"));
    return FMath::IsFinite(ExpectedLength) && Montage->RateScale == 1.f && Attack->GetSamplingFrameRate().IsValid() && Recovery->GetSamplingFrameRate().IsValid() && Montage->GetCommonTargetFrameRate().IsValid() && FMath::IsNearlyEqual(Montage->GetPlayLength(), ExpectedLength, 0.0001f) && FMath::IsNearlyEqual(Montage->CalculateSequenceLength(), ExpectedLength, 0.0001f);
}

bool UWarriorAssetLibrary::IsOutputSlotConnected(UAnimBlueprint* Blueprint, FName SlotName)
{
    UAnimGraphNode_Root* Root = FindOutputNode(Blueprint);
    if (!Root || SlotName.IsNone()) return false;
    TArray<const UEdGraphNode*> Pending = {Root};
    TSet<const UEdGraphNode*> Visited;
    while (!Pending.IsEmpty())
    {
        const UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
        if (!Node || Visited.Contains(Node)) continue;
        Visited.Add(Node);
        if (const UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node); Slot && Slot->Node.SlotName == SlotName) return true;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input || !UAnimationGraphSchema::IsPosePin(Pin->PinType)) continue;
            for (const UEdGraphPin* Source : Pin->LinkedTo)
            {
                if (Source) Pending.Add(Source->GetOwningNode());
            }
        }
    }
    return false;
}

bool UWarriorAssetLibrary::EnsureOutputSlot(UAnimBlueprint* Blueprint, FName SlotName)
{
    if (!IsProjectCopy(Blueprint) || !IsValid(Blueprint->TargetSkeleton) || SlotName.IsNone()) return Fail(TEXT("Output slot changes require a project-owned AnimBlueprint and valid skeleton / 슬롯 변경에는 작업 사본 AnimBlueprint와 유효한 스켈레톤이 필요합니다"));
    if (!IsProjectCopy(Blueprint->TargetSkeleton) && !Blueprint->TargetSkeleton->ContainsSlotName(SlotName)) return Fail(TEXT("Output slots must already exist on an external skeleton / 외부 스켈레톤에는 출력 슬롯이 이미 있어야 합니다"));
    if (!ValidateExternalSkeletonSlots({Blueprint}, Blueprint->TargetSkeleton, false)) return false;
    if (IsOutputSlotConnected(Blueprint, SlotName)) return true;
    if (GetAnimationSlotNames(Blueprint).Contains(SlotName)) return Fail(TEXT("An unconnected matching slot already exists; preserve it for explicit repair / 연결되지 않은 동일 슬롯이 있어 명시적 수정을 위해 보존합니다"));
    UAnimGraphNode_Root* Root = FindOutputNode(Blueprint);
    UEdGraphPin* ResultPin = FindPosePin(Root, EGPD_Input);
    if (!ResultPin || ResultPin->LinkedTo.Num() > 1) return Fail(TEXT("The main AnimGraph output has no unambiguous pose input / 기본 AnimGraph 출력의 포즈 입력을 확인할 수 없습니다"));
    UEdGraph* Graph = Root->GetGraph();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    UEdGraphPin* PreviousSource = ResultPin->LinkedTo.IsEmpty() ? nullptr : ResultPin->LinkedTo[0];
    Blueprint->Modify();
    Graph->Modify();
    Root->Modify();
    FGraphNodeCreator<UAnimGraphNode_Slot> Creator(*Graph);
    UAnimGraphNode_Slot* Slot = Creator.CreateNode();
    Slot->Node.SlotName = SlotName;
    Slot->NodePosX = Root->NodePosX - 220;
    Slot->NodePosY = Root->NodePosY;
    Creator.Finalize();
    UEdGraphPin* SlotInput = FindPosePin(Slot, EGPD_Input);
    UEdGraphPin* SlotOutput = FindPosePin(Slot, EGPD_Output);
    if (PreviousSource) Schema->BreakSinglePinLink(PreviousSource, ResultPin);
    const bool bConnected = SlotInput && SlotOutput && (!PreviousSource || Schema->TryCreateConnection(PreviousSource, SlotInput)) && Schema->TryCreateConnection(SlotOutput, ResultPin);
    if (!bConnected)
    {
        Slot->DestroyNode();
        if (PreviousSource) Schema->TryCreateConnection(PreviousSource, ResultPin);
        return Fail(TEXT("Could not insert the montage slot; restored the original pose connection / 몽타주 슬롯 삽입 실패로 원래 포즈 연결을 복구했습니다"));
    }
    if (IsProjectCopy(Blueprint->TargetSkeleton) && !Blueprint->TargetSkeleton->ContainsSlotName(SlotName))
    {
        Blueprint->TargetSkeleton->Modify();
        Blueprint->TargetSkeleton->RegisterSlotNode(SlotName);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error && IsOutputSlotConnected(Blueprint, SlotName);
}

bool UWarriorAssetLibrary::RemoveLegacyFootIK(UAnimBlueprint* Blueprint)
{
    if (!IsProjectCopy(Blueprint)) return Fail(TEXT("Foot IK removal requires a project-owned AnimBlueprint copy / Foot IK 제거에는 AnimBlueprint 작업 사본이 필요합니다"));
    const FString LegacyRigPath(TEXT("/Game/Characters/Mannequins/Rigs/CR_Mannequin_FootIK.CR_Mannequin_FootIK_C"));
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UAnimGraphNode_ControlRig*> LegacyNodes;
    for (UEdGraph* Graph : Graphs)
    {
        const UAnimationGraphSchema* Schema = Graph ? Cast<UAnimationGraphSchema>(Graph->GetSchema()) : nullptr;
        if (!Schema) continue;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UAnimGraphNode_ControlRig* RigNode = Cast<UAnimGraphNode_ControlRig>(Node);
            const UClass* RigClass = RigNode ? RigNode->Node.GetControlRigClass().Get() : nullptr;
            if (!RigClass || RigClass->GetPathName() != LegacyRigPath) continue;
            UEdGraphPin* Input = FindPosePin(RigNode, EGPD_Input);
            UEdGraphPin* Output = FindPosePin(RigNode, EGPD_Output);
            if (!Input || !Output || Input->LinkedTo.Num() != 1 || Output->LinkedTo.Num() != 1) return Fail(TEXT("Legacy Foot IK has an ambiguous pose connection; preserved the graph / 기존 Foot IK 포즈 연결이 명확하지 않아 그래프를 보존합니다"));
            UEdGraphPin* Source = Input->LinkedTo[0];
            UEdGraphPin* Destination = Output->LinkedTo[0];
            if (!Source || !Destination || Source->LinkedTo.Num() != 1 || Destination->LinkedTo.Num() != 1 || !Schema->ArePinsCompatible(Source, Destination, nullptr)) return Fail(TEXT("Legacy Foot IK cannot be bypassed without changing other pose links / 다른 포즈 연결 변경 없이 기존 Foot IK를 우회할 수 없습니다"));
            LegacyNodes.Add(RigNode);
        }
    }
    if (LegacyNodes.IsEmpty()) return true;
    TArray<FName> ConnectedSlots;
    for (FName SlotName : GetAnimationSlotNames(Blueprint))
    {
        if (IsOutputSlotConnected(Blueprint, SlotName)) ConnectedSlots.Add(SlotName);
    }
    Blueprint->Modify();
    for (UAnimGraphNode_ControlRig* RigNode : LegacyNodes)
    {
        UEdGraph* Graph = RigNode->GetGraph();
        const UEdGraphSchema* Schema = Graph->GetSchema();
        UEdGraphPin* Input = FindPosePin(RigNode, EGPD_Input);
        UEdGraphPin* Output = FindPosePin(RigNode, EGPD_Output);
        UEdGraphPin* Source = Input->LinkedTo[0];
        UEdGraphPin* Destination = Output->LinkedTo[0];
        Graph->Modify();
        RigNode->Modify();
        // Bypass only the verified template rig, preserving the existing locomotion and montage slot chain.
        // 확인된 템플릿 리그만 우회하며 기존 이동 및 몽타주 슬롯 연결을 보존합니다.
        if (!Schema->TryCreateConnection(Source, Destination))
        {
            Schema->TryCreateConnection(Source, Input);
            Schema->TryCreateConnection(Output, Destination);
            return Fail(TEXT("Could not bypass the legacy Foot IK pose connection / 기존 Foot IK 포즈 연결 우회에 실패했습니다"));
        }
        FBlueprintEditorUtils::RemoveNode(Blueprint, RigNode, true);
        Graph->NotifyGraphChanged();
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    for (FName SlotName : ConnectedSlots)
    {
        if (!IsOutputSlotConnected(Blueprint, SlotName)) return Fail(TEXT("The existing montage slot connection was not preserved / 기존 몽타주 슬롯 연결이 보존되지 않았습니다"));
    }
    return Blueprint->Status != BS_Error;
}
