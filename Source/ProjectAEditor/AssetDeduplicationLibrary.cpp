#include "AssetDeduplicationLibrary.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimCurveMetadata.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Animation/IAnimationSequenceCompiler.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "MeshDescription.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Package.h"
#include "UObject/Class.h"

DEFINE_LOG_CATEGORY_STATIC(LogAssetDeduplication, Log, All);

bool UAssetDeduplicationLibrary::SetAnimationBlueprintPreviewMesh(UAnimBlueprint* Blueprint, USkeletalMesh* Mesh)
{
    if (!IsInGameThread() || !IsValid(Blueprint) || !IsValid(Mesh) || !Blueprint->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/")) || Blueprint->TargetSkeleton != Mesh->GetSkeleton()) return false;
    Blueprint->SetPreviewMesh(Mesh);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr && Blueprint->GetPreviewMesh() == Mesh;
}

USkeletalMesh* UAssetDeduplicationLibrary::GetAnimationBlueprintPreviewMesh(UAnimBlueprint* Blueprint)
{
    return IsValid(Blueprint) ? Blueprint->GetPreviewMesh() : nullptr;
}

namespace
{
    bool HaveIdenticalPoses(const TArray<FTransform>& Source, const TArray<FTransform>& Target)
    {
        if (Source.Num() != Target.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < Source.Num(); ++Index)
        {
            if (!Source[Index].IsValid() || !Target[Index].IsValid() || !Source[Index].Equals(Target[Index], 0.0))
            {
                return false;
            }
        }
        return true;
    }

    bool HaveIdenticalSockets(const USkeleton* Source, const USkeleton* Target)
    {
        if (Source->Sockets.Num() != Target->Sockets.Num())
        {
            return false;
        }
        TSet<FName> SocketNames;
        for (const USkeletalMeshSocket* SourceSocket : Source->Sockets)
        {
            if (!IsValid(SourceSocket) || SocketNames.Contains(SourceSocket->SocketName))
            {
                return false;
            }
            SocketNames.Add(SourceSocket->SocketName);
            const USkeletalMeshSocket* TargetSocket = Target->FindSocket(SourceSocket->SocketName);
            if (!IsValid(TargetSocket) || SourceSocket->BoneName != TargetSocket->BoneName || SourceSocket->RelativeLocation != TargetSocket->RelativeLocation || SourceSocket->RelativeRotation != TargetSocket->RelativeRotation || SourceSocket->RelativeScale != TargetSocket->RelativeScale || SourceSocket->bForceAlwaysAnimated != TargetSocket->bForceAlwaysAnimated)
            {
                return false;
            }
        }
        return true;
    }

    bool IsProjectAnimation(const UAnimationAsset* Animation)
    {
        return IsValid(Animation) && !Animation->IsTemplate() && Animation->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"));
    }
}

bool UAssetDeduplicationLibrary::HaveIdenticalSkeletalMeshGeometry(USkeletalMesh* Source, USkeletalMesh* Target)
{
    if (!IsInGameThread() || !IsValid(Source) || !IsValid(Target) || Source->IsTemplate() || Target->IsTemplate() || Source->GetLODNum() <= 0 || Source->GetLODNum() != Target->GetLODNum())
    {
        return false;
    }
    const FReferenceSkeleton& SourceBones = Source->GetRefSkeleton();
    const FReferenceSkeleton& TargetBones = Target->GetRefSkeleton();
    if (SourceBones.GetRawBoneNum() != TargetBones.GetRawBoneNum() || !HaveIdenticalPoses(SourceBones.GetRawRefBonePose(), TargetBones.GetRawRefBonePose()))
    {
        UE_LOG(LogAssetDeduplication, Warning, TEXT("Mesh reference skeleton differs: %s -> %s"), *Source->GetPathName(), *Target->GetPathName());
        return false;
    }
    for (int32 Index = 0; Index < SourceBones.GetRawBoneNum(); ++Index)
    {
        const FMeshBoneInfo& SourceBone = SourceBones.GetRawRefBoneInfo()[Index];
        const FMeshBoneInfo& TargetBone = TargetBones.GetRawRefBoneInfo()[Index];
        if (SourceBone.Name != TargetBone.Name || SourceBone.ParentIndex != TargetBone.ParentIndex)
        {
            return false;
        }
    }
    for (int32 LodIndex = 0; LodIndex < Source->GetLODNum(); ++LodIndex)
    {
        FMeshDescription* SourceDescription = Source->GetMeshDescription(LodIndex);
        FMeshDescription* TargetDescription = Target->GetMeshDescription(LodIndex);
        if (!SourceDescription || !TargetDescription)
        {
            const FSkeletalMeshLODInfo* SourceInfo = Source->GetLODInfo(LodIndex);
            const FSkeletalMeshLODInfo* TargetInfo = Target->GetLODInfo(LodIndex);
            // Generated LODs must derive from an already verified lower LOD with identical complete settings.
            // 자동 LOD는 이미 검증된 하위 번호 LOD를 기준으로 생성되며 전체 설정이 같아야 합니다.
            if (!SourceDescription && !TargetDescription && LodIndex > 0 && SourceInfo && TargetInfo && SourceInfo->bHasBeenSimplified && TargetInfo->bHasBeenSimplified && !SourceInfo->bImportWithBaseMesh && !TargetInfo->bImportWithBaseMesh && !SourceInfo->bHasPerLODVertexColors && !TargetInfo->bHasPerLODVertexColors && SourceInfo->SourceImportFilename.IsEmpty() && TargetInfo->SourceImportFilename.IsEmpty() && SourceInfo->ReductionSettings.BaseLOD >= 0 && SourceInfo->ReductionSettings.BaseLOD < LodIndex && FSkeletalMeshLODInfo::StaticStruct()->CompareScriptStruct(SourceInfo, TargetInfo, 0))
            {
                UE_LOG(LogAssetDeduplication, Display, TEXT("Mesh generated LOD %d uses identical settings and verified base LOD %d: %s -> %s"), LodIndex, SourceInfo->ReductionSettings.BaseLOD, *Source->GetPathName(), *Target->GetPathName());
                continue;
            }
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Mesh source geometry unavailable at LOD %d: %s -> %s"), LodIndex, *Source->GetPathName(), *Target->GetPathName());
            return false;
        }
        TArray<uint8> SourceBytes;
        TArray<uint8> TargetBytes;
        FMemoryWriter SourceWriter(SourceBytes, true);
        FMemoryWriter TargetWriter(TargetBytes, true);
        SourceDescription->Serialize(SourceWriter);
        TargetDescription->Serialize(TargetWriter);
        if (SourceWriter.IsError() || TargetWriter.IsError() || SourceBytes != TargetBytes)
        {
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Mesh source geometry differs at LOD %d (%d/%d bytes): %s -> %s"), LodIndex, SourceBytes.Num(), TargetBytes.Num(), *Source->GetPathName(), *Target->GetPathName());
            return false;
        }
    }
    return true;
}

bool UAssetDeduplicationLibrary::CanReplaceSkeleton(USkeleton* Source, USkeleton* Target)
{
    return CanReplaceSkeletonInternal(Source, Target, false);
}

bool UAssetDeduplicationLibrary::CanReplaceSkeletonInternal(USkeleton* Source, USkeleton* Target, bool bIgnoreMissingSlots)
{
    if (!IsValid(Source) || !IsValid(Target) || Source->IsTemplate() || Target->IsTemplate())
    {
        return false;
    }
    if (Source == Target)
    {
        return true;
    }
    const FReferenceSkeleton& SourceBones = Source->GetReferenceSkeleton();
    const FReferenceSkeleton& TargetBones = Target->GetReferenceSkeleton();
    if (SourceBones.GetRawBoneNum() <= 0 || SourceBones.GetRawBoneNum() != TargetBones.GetRawBoneNum() || SourceBones.GetNum() != TargetBones.GetNum() || !HaveIdenticalPoses(SourceBones.GetRawRefBonePose(), TargetBones.GetRawRefBonePose()) || !HaveIdenticalPoses(SourceBones.GetRefBonePose(), TargetBones.GetRefBonePose()))
    {
        UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton differs: %s -> %s; raw bones %d/%d, all bones %d/%d, raw poses equal %d, all poses equal %d"), *Source->GetPathName(), *Target->GetPathName(), SourceBones.GetRawBoneNum(), TargetBones.GetRawBoneNum(), SourceBones.GetNum(), TargetBones.GetNum(), HaveIdenticalPoses(SourceBones.GetRawRefBonePose(), TargetBones.GetRawRefBonePose()), HaveIdenticalPoses(SourceBones.GetRefBonePose(), TargetBones.GetRefBonePose()));
        return false;
    }
    for (int32 Index = 0; Index < SourceBones.GetNum(); ++Index)
    {
        if (SourceBones.GetBoneName(Index) != TargetBones.GetBoneName(Index) || SourceBones.GetParentIndex(Index) != TargetBones.GetParentIndex(Index))
        {
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton bone differs at %d: %s/%d -> %s/%d"), Index, *SourceBones.GetBoneName(Index).ToString(), SourceBones.GetParentIndex(Index), *TargetBones.GetBoneName(Index).ToString(), TargetBones.GetParentIndex(Index));
            return false;
        }
        if (Index < SourceBones.GetRawBoneNum() && Source->GetBoneTranslationRetargetingMode(Index) != Target->GetBoneTranslationRetargetingMode(Index))
        {
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton retarget mode differs for %s: %d -> %d"), *SourceBones.GetBoneName(Index).ToString(), Source->GetBoneTranslationRetargetingMode(Index), Target->GetBoneTranslationRetargetingMode(Index));
            return false;
        }
    }
    const TArray<FVirtualBone>& SourceVirtualBones = Source->GetVirtualBones();
    const TArray<FVirtualBone>& TargetVirtualBones = Target->GetVirtualBones();
    if (SourceVirtualBones.Num() != TargetVirtualBones.Num())
    {
        UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton virtual bone count differs: %d -> %d"), SourceVirtualBones.Num(), TargetVirtualBones.Num());
        return false;
    }
    for (int32 Index = 0; Index < SourceVirtualBones.Num(); ++Index)
    {
        if (SourceVirtualBones[Index].SourceBoneName != TargetVirtualBones[Index].SourceBoneName || SourceVirtualBones[Index].TargetBoneName != TargetVirtualBones[Index].TargetBoneName || SourceVirtualBones[Index].VirtualBoneName != TargetVirtualBones[Index].VirtualBoneName)
        {
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton virtual bone differs at %d: %s -> %s"), Index, *SourceVirtualBones[Index].VirtualBoneName.ToString(), *TargetVirtualBones[Index].VirtualBoneName.ToString());
            return false;
        }
    }
    for (const FAnimSlotGroup& Group : Source->GetSlotGroups())
    {
        for (FName SlotName : Group.SlotNames)
        {
            if ((Target->ContainsSlotName(SlotName) && Target->GetSlotGroupName(SlotName) != Group.GroupName) || (!Target->ContainsSlotName(SlotName) && !bIgnoreMissingSlots))
            {
                UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton target lacks slot or group: %s.%s -> %s"), *Group.GroupName.ToString(), *SlotName.ToString(), *Target->GetPathName());
                return false;
            }
        }
    }
    if (Source->AnimRetargetSources.Num() != Target->AnimRetargetSources.Num())
    {
        UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton retarget source count differs: %d -> %d"), Source->AnimRetargetSources.Num(), Target->AnimRetargetSources.Num());
        return false;
    }
    for (const TPair<FName, FReferencePose>& SourcePose : Source->AnimRetargetSources)
    {
        const FReferencePose* TargetPose = Target->AnimRetargetSources.Find(SourcePose.Key);
        if (!TargetPose || SourcePose.Value.PoseName != TargetPose->PoseName || !HaveIdenticalPoses(SourcePose.Value.ReferencePose, TargetPose->ReferencePose))
        {
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton retarget source differs: %s"), *SourcePose.Key.ToString());
            return false;
        }
    }
    TArray<FName> SourceCurves;
    TArray<FName> TargetCurves;
    Source->GetCurveMetaDataNames(SourceCurves);
    Target->GetCurveMetaDataNames(TargetCurves);
    if (SourceCurves.Num() != TargetCurves.Num())
    {
        UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton curve metadata count differs: %d -> %d"), SourceCurves.Num(), TargetCurves.Num());
        return false;
    }
    for (FName CurveName : SourceCurves)
    {
        const FCurveMetaData* SourceCurve = Source->GetCurveMetaData(CurveName);
        const FCurveMetaData* TargetCurve = Target->GetCurveMetaData(CurveName);
        if (!SourceCurve || !TargetCurve || SourceCurve->MaxLOD != TargetCurve->MaxLOD || SourceCurve->Type.bMaterial != TargetCurve->Type.bMaterial || SourceCurve->Type.bMorphtarget != TargetCurve->Type.bMorphtarget || SourceCurve->LinkedBones.Num() != TargetCurve->LinkedBones.Num())
        {
            UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton curve metadata differs: %s"), *CurveName.ToString());
            return false;
        }
        for (int32 Index = 0; Index < SourceCurve->LinkedBones.Num(); ++Index)
        {
            if (SourceCurve->LinkedBones[Index].BoneName != TargetCurve->LinkedBones[Index].BoneName)
            {
                UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton curve linked bone differs: %s[%d]"), *CurveName.ToString(), Index);
                return false;
            }
        }
    }
    if (!HaveIdenticalSockets(Source, Target))
    {
        UE_LOG(LogAssetDeduplication, Warning, TEXT("Skeleton sockets differ: %s (%d) -> %s (%d)"), *Source->GetPathName(), Source->Sockets.Num(), *Target->GetPathName(), Target->Sockets.Num());
        return false;
    }
    return true;
}

bool UAssetDeduplicationLibrary::MergeCopiedSkeletonSlots(USkeleton* Source, USkeleton* Target)
{
    if (!IsInGameThread() || !IsValid(Source) || !IsValid(Target))
    {
        return false;
    }
    static const TSet<FString> AllowedOriginalPackages = {TEXT("/Game/GKnight/Meshes/SK_GothicKnight_Skeleton"), TEXT("/Game/Skeleton_Guard/Demoscene_UE4/Mesh/UE4_Mannequin_Skeleton")};
    const FString TargetPackage = Target->GetOutermost()->GetName();
    const FString ExpectedSourcePackage = TEXT("/Game/User_JeHoon/") + TargetPackage.RightChop(6);
    if (!AllowedOriginalPackages.Contains(TargetPackage) || Source->GetOutermost()->GetName() != ExpectedSourcePackage || Source->GetFName() != Target->GetFName() || !CanReplaceSkeletonInternal(Source, Target, true))
    {
        return false;
    }

    // Complete structural validation before touching either original skeleton's slot registry.
    // 원본 스켈레톤의 슬롯 등록 정보를 변경하기 전에 구조 검사를 모두 완료합니다.
    for (const FAnimSlotGroup& Group : Source->GetSlotGroups())
    {
        for (FName SlotName : Group.SlotNames)
        {
            if (!Target->ContainsSlotName(SlotName))
            {
                Target->Modify();
                Target->SetSlotGroupName(SlotName, Group.GroupName);
                Target->MarkPackageDirty();
                UE_LOG(LogAssetDeduplication, Display, TEXT("Preserved copied skeleton slot %s.%s in %s"), *Group.GroupName.ToString(), *SlotName.ToString(), *Target->GetPathName());
            }
        }
    }
    return CanReplaceSkeleton(Source, Target);
}

bool UAssetDeduplicationLibrary::ReplaceAnimationSkeleton(UAnimationAsset* Animation, USkeleton* Target)
{
    if (!IsInGameThread() || !IsProjectAnimation(Animation) || !CanReplaceSkeleton(Animation->GetSkeleton(), Target))
    {
        return false;
    }

    // The engine also replaces referenced animations, so validate the complete set before any write.
    // 엔진이 참조 애니메이션도 교체하므로 변경 전에 전체 대상을 검사합니다.
    TArray<UAnimationAsset*> Animations = {Animation};
    Animation->GetAllAnimationSequencesReferred(Animations, true);
    TArray<UAnimSequence*> Sequences;
    for (UAnimationAsset* Referenced : Animations)
    {
        if (!IsProjectAnimation(Referenced) || !CanReplaceSkeleton(Referenced->GetSkeleton(), Target))
        {
            return false;
        }
        if (UAnimSequence* Sequence = Cast<UAnimSequence>(Referenced))
        {
            Sequences.AddUnique(Sequence);
        }
    }
    // Finish source compilation before reading data models or capturing transaction state.
    // 데이터 모델을 읽거나 트랜잭션 상태를 기록하기 전에 원본 컴파일을 완료합니다.
    UE_LOG(LogAssetDeduplication, Verbose, TEXT("Skeleton replacement phase FinishSourceCompilation: %s"), *Animation->GetPathName());
    UE::Anim::IAnimSequenceCompilingManager::FinishCompilation(Sequences);
    UE_LOG(LogAssetDeduplication, Verbose, TEXT("Skeleton replacement phase HashSource: %s"), *Animation->GetPathName());
    TMap<UAnimSequence*, FGuid> SourceData;
    for (UAnimSequence* Sequence : Sequences)
    {
        if (!Sequence->GetDataModel())
        {
            return false;
        }
        SourceData.Add(Sequence, Sequence->GetDataModel()->GenerateGuid());
    }
    if (Animation->GetSkeleton() == Target && Animation->GetSkeletonGuid() == Target->GetGuid())
    {
        return true;
    }
    UE_LOG(LogAssetDeduplication, Verbose, TEXT("Skeleton replacement phase Modify: %s"), *Animation->GetPathName());
    for (UAnimationAsset* Referenced : Animations)
    {
        Referenced->Modify();
    }
    UE_LOG(LogAssetDeduplication, Verbose, TEXT("Skeleton replacement phase ReplaceSkeleton: %s"), *Animation->GetPathName());
    if (!Animation->ReplaceSkeleton(Target, false))
    {
        return false;
    }
    UE_LOG(LogAssetDeduplication, Verbose, TEXT("Skeleton replacement phase FinishTargetCompilation: %s"), *Animation->GetPathName());
    UE::Anim::IAnimSequenceCompilingManager::FinishCompilation(Sequences);
    UE_LOG(LogAssetDeduplication, Verbose, TEXT("Skeleton replacement phase VerifySource: %s"), *Animation->GetPathName());
    for (const TPair<UAnimSequence*, FGuid>& Entry : SourceData)
    {
        if (!Entry.Key->GetDataModel() || Entry.Key->GetDataModel()->GenerateGuid() != Entry.Value)
        {
            UE_LOG(LogAssetDeduplication, Error, TEXT("Skeleton replacement changed animation source data; do not save this session: %s"), *Entry.Key->GetPathName());
            return false;
        }
    }
    for (UAnimationAsset* Referenced : Animations)
    {
        if (Referenced->GetSkeleton() != Target)
        {
            return false;
        }
        Referenced->MarkPackageDirty();
    }
    return true;
}
