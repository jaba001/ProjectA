#include "CharacterAppearanceAssetLibrary.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterAppearanceAssets, Log, All);

namespace
{
    constexpr float PositionTolerance = 0.01f;
    constexpr float UVTolerance = 0.0001f;

    struct FMaterialTriangle
    {
        FVector3f Positions[3];
        FVector2f UVs[3];
        int32 Material = INDEX_NONE;

        FIntVector Cell() const
        {
            const FVector3f Center = (Positions[0] + Positions[1] + Positions[2]) / 3.0f;
            return FIntVector(FMath::FloorToInt(Center.X), FMath::FloorToInt(Center.Y), FMath::FloorToInt(Center.Z));
        }
    };

    struct FBodyMaterialPlan
    {
        TArray<FMaterialTriangle> SourceTriangles;
        TMap<FIntVector, TArray<int32>> SourceCells;
        TMap<FPolygonID, int32> PolygonMaterials;
        TArray<int32> UsedMaterials;
        TArray<int32> TriangleCounts = {0, 0};
    };

    bool Fail(const TCHAR* Reason)
    {
        UE_LOG(LogCharacterAppearanceAssets, Error, TEXT("%s / 신체 재질 복원 검사를 통과하지 못했습니다."), Reason);
        return false;
    }

    bool AllowedMeshes(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart)
    {
        if (!IsValid(SourceMesh) || !IsValid(BodyPart) || SourceMesh->GetPathName() != TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")) return Fail(TEXT("The original TopDown Manny mesh is required"));
        static const TArray<FString> AllowedParts = {TEXT("Head"), TEXT("Chest"), TEXT("Arms"), TEXT("Hands"), TEXT("Legs"), TEXT("Feet")};
        for (const FString& Part : AllowedParts)
        {
            const FString Name = TEXT("SK_Manny_") + Part;
            if (BodyPart->GetPathName() == TEXT("/Game/ROG_Modular_Armor/Characters/UE5_Mannequins/Manny/Body_parts/") + Name + TEXT(".") + Name) return true;
        }
        return Fail(TEXT("The target must be one of the six original ROG Manny body parts"));
    }

    FMaterialTriangle ReadTriangle(const FMeshDescription& Description, const FSkeletalMeshConstAttributes& Attributes, FTriangleID Triangle)
    {
        FMaterialTriangle Result;
        const auto Instances = Description.GetTriangleVertexInstances(Triangle);
        const auto UVs = Attributes.GetVertexInstanceUVs();
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Result.Positions[Corner] = Description.GetVertexPosition(Description.GetVertexInstanceVertex(Instances[Corner]));
            Result.UVs[Corner] = UVs.Get(Instances[Corner], 0);
        }
        return Result;
    }

    bool SameTriangle(const FMaterialTriangle& A, const FMaterialTriangle& B)
    {
        static constexpr int32 Permutations[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {0, 2, 1}, {2, 1, 0}, {1, 0, 2}};
        for (const auto& Permutation : Permutations)
        {
            bool bMatches = true;
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                if (!A.Positions[Corner].Equals(B.Positions[Permutation[Corner]], PositionTolerance) || !A.UVs[Corner].Equals(B.UVs[Permutation[Corner]], UVTolerance)) bMatches = false;
            }
            if (bMatches) return true;
        }
        return false;
    }

    int32 FindMaterial(const FBodyMaterialPlan& Plan, const FMaterialTriangle& Triangle)
    {
        int32 Material = INDEX_NONE;
        const FIntVector Cell = Triangle.Cell();
        for (int32 X = -1; X <= 1; ++X)
        {
            for (int32 Y = -1; Y <= 1; ++Y)
            {
                for (int32 Z = -1; Z <= 1; ++Z)
                {
                    const TArray<int32>* Candidates = Plan.SourceCells.Find(Cell + FIntVector(X, Y, Z));
                    if (!Candidates) continue;
                    for (int32 Candidate : *Candidates)
                    {
                        const FMaterialTriangle& Source = Plan.SourceTriangles[Candidate];
                        if (!SameTriangle(Triangle, Source)) continue;
                        if (Material != INDEX_NONE && Material != Source.Material) return INDEX_NONE;
                        Material = Source.Material;
                    }
                }
            }
        }
        return Material;
    }

    int32 MaterialForGroup(const USkeletalMesh* Mesh, const FSkeletalMeshConstAttributes& Attributes, FPolygonGroupID Group)
    {
        const FName Name = Attributes.GetPolygonGroupMaterialSlotNames()[Group];
        if (Name.IsNone()) return INDEX_NONE;
        int32 Result = INDEX_NONE;
        for (int32 Index = 0; Index < Mesh->GetMaterials().Num(); ++Index)
        {
            const FSkeletalMaterial& Material = Mesh->GetMaterials()[Index];
            if (Material.ImportedMaterialSlotName != Name && Material.MaterialSlotName != Name) continue;
            if (Result != INDEX_NONE) return INDEX_NONE;
            Result = Index;
        }
        return Result;
    }

    bool MakePlan(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart, FBodyMaterialPlan& Plan)
    {
        if (!AllowedMeshes(SourceMesh, BodyPart)) return false;
        FSkinnedAssetCompilingManager::Get().FinishCompilation({SourceMesh, BodyPart});
        const FMeshDescription* Source = SourceMesh->GetMeshDescription(0);
        const FMeshDescription* Body = BodyPart->GetMeshDescription(0);
        if (!Source || !Body || SourceMesh->GetMaterials().Num() != 2 || BodyPart->GetLODNum() != 1 || BodyPart->GetNumSourceModels() != 1 || BodyPart->IsReductionActive(0) || BodyPart->GetMeshClothingAssets().Num() != 0) return Fail(TEXT("Expected two source materials and one unreduced body LOD without cloth binding"));
        if (!BodyPart->GetImportedModel() || BodyPart->GetImportedModel()->LODModels.Num() != 1 || !BodyPart->GetLODInfo(0)->LODMaterialMap.IsEmpty()) return Fail(TEXT("Body LOD data is missing or contains a material remap"));
        const FSkeletalMeshConstAttributes SourceAttributes(*Source);
        const FSkeletalMeshConstAttributes BodyAttributes(*Body);
        if (SourceAttributes.GetVertexInstanceUVs().GetNumChannels() < 1 || BodyAttributes.GetVertexInstanceUVs().GetNumChannels() < 1 || Source->Triangles().Num() == 0 || Body->Triangles().Num() == 0) return Fail(TEXT("Nonempty geometry and UV0 are required"));
        for (FTriangleID Triangle : Source->Triangles().GetElementIDs())
        {
            FMaterialTriangle Entry = ReadTriangle(*Source, SourceAttributes, Triangle);
            Entry.Material = MaterialForGroup(SourceMesh, SourceAttributes, Source->GetTrianglePolygonGroup(Triangle));
            if (Entry.Material < 0 || Entry.Material > 1) return Fail(TEXT("Source polygon material cannot be resolved"));
            const int32 Index = Plan.SourceTriangles.Add(Entry);
            Plan.SourceCells.FindOrAdd(Entry.Cell()).Add(Index);
        }
        for (FTriangleID Triangle : Body->Triangles().GetElementIDs())
        {
            const int32 Material = FindMaterial(Plan, ReadTriangle(*Body, BodyAttributes, Triangle));
            if (Material == INDEX_NONE) return Fail(TEXT("A body triangle has no unique original position and UV match"));
            const FPolygonID Polygon = Body->GetTrianglePolygon(Triangle);
            const int32* Existing = Plan.PolygonMaterials.Find(Polygon);
            if (Existing && *Existing != Material) return Fail(TEXT("A polygon spans different original materials"));
            Plan.PolygonMaterials.Add(Polygon, Material);
            Plan.UsedMaterials.AddUnique(Material);
            ++Plan.TriangleCounts[Material];
        }
        Plan.UsedMaterials.Sort();
        return Plan.PolygonMaterials.Num() == Body->Polygons().Num() || Fail(TEXT("Some body polygons have no matched triangles"));
    }

    TArray<int32> ReadSlotMap(USkeletalMesh* BodyPart, const FBodyMaterialPlan& Plan)
    {
        const FMeshDescription* Body = BodyPart->GetMeshDescription(0);
        const FSkeletalMeshConstAttributes Attributes(*Body);
        TArray<int32> Slots;
        Slots.Init(INDEX_NONE, BodyPart->GetMaterials().Num());
        for (const auto& Pair : Plan.PolygonMaterials)
        {
            const int32 Slot = MaterialForGroup(BodyPart, Attributes, Body->GetPolygonPolygonGroup(Pair.Key));
            if (!Slots.IsValidIndex(Slot) || (Slots[Slot] != INDEX_NONE && Slots[Slot] != Pair.Value)) return {};
            Slots[Slot] = Pair.Value;
        }
        if (Slots.Contains(INDEX_NONE) || Slots.Num() != Plan.UsedMaterials.Num()) return {};
        return Slots;
    }

    bool ValidateBuiltSections(USkeletalMesh* BodyPart, const FBodyMaterialPlan& Plan, const TArray<int32>& Slots)
    {
        const FSkeletalMeshLODModel& LOD = BodyPart->GetImportedModel()->LODModels[0];
        TArray<FSoftSkinVertex> Vertices;
        LOD.GetVertices(Vertices);
        TArray<int32> Counts = {0, 0};
        for (const FSkelMeshSection& Section : LOD.Sections)
        {
            if (!Slots.IsValidIndex(Section.MaterialIndex) || Section.HasClothingData() || Section.BaseIndex + Section.NumTriangles * 3 > static_cast<uint32>(LOD.IndexBuffer.Num())) return Fail(TEXT("Built body sections contain invalid material or geometry data"));
            for (uint32 Index = Section.BaseIndex; Index < Section.BaseIndex + Section.NumTriangles * 3; Index += 3)
            {
                FMaterialTriangle Triangle;
                for (int32 Corner = 0; Corner < 3; ++Corner)
                {
                    const uint32 Vertex = LOD.IndexBuffer[Index + Corner];
                    if (!Vertices.IsValidIndex(Vertex)) return Fail(TEXT("Built triangle vertex is out of range"));
                    Triangle.Positions[Corner] = Vertices[Vertex].Position;
                    Triangle.UVs[Corner] = Vertices[Vertex].UVs[0];
                }
                const int32 Material = FindMaterial(Plan, Triangle);
                if (Material == INDEX_NONE || Slots[Section.MaterialIndex] != Material) return Fail(TEXT("Built triangle material does not match the original Manny surface"));
                ++Counts[Material];
            }
        }
        return Counts == Plan.TriangleCounts || Fail(TEXT("Built triangle counts differ from the original body geometry"));
    }

    template<typename ElementID>
    void AppendAttributeHashes(const TAttributesSet<ElementID>& Attributes, TArray<uint32>& Result)
    {
        TArray<FName> Names;
        Attributes.GetAttributeNames(Names);
        Names.Sort(FNameLexicalLess());
        for (FName Name : Names) Result.Add(Attributes.GetHash(Name));
    }

    TArray<uint32> GeometryHashes(const FMeshDescription& Description)
    {
        TArray<uint32> Result = {static_cast<uint32>(Description.Vertices().Num()), static_cast<uint32>(Description.VertexInstances().Num()), static_cast<uint32>(Description.Triangles().Num())};
        AppendAttributeHashes(Description.VertexAttributes(), Result);
        AppendAttributeHashes(Description.VertexInstanceAttributes(), Result);
        AppendAttributeHashes(Description.EdgeAttributes(), Result);
        return Result;
    }
}

bool UCharacterAppearanceAssetLibrary::ValidateBodyAnimationSkeleton(USkeletalMesh* BodyMesh, USkeleton* AnimationSkeleton)
{
    if (!IsValid(BodyMesh) || !IsValid(BodyMesh->GetSkeleton()) || !IsValid(AnimationSkeleton)) return Fail(TEXT("Body mesh and animation skeleton are required"));
    FSkinnedAssetCompilingManager::Get().FinishCompilation({BodyMesh});
    const FReferenceSkeleton& Source = AnimationSkeleton->GetReferenceSkeleton();
    const auto Matches = [&Source](const FReferenceSkeleton& Target, bool bAllowBodyProportions)
    {
        for (int32 Index = 0; Index < Target.GetRawBoneNum(); ++Index)
        {
            const FName Name = Target.GetBoneName(Index);
            const int32 SourceIndex = Source.FindBoneIndex(Name);
            if (SourceIndex == INDEX_NONE) return Fail(*FString::Printf(TEXT("Animation skeleton is missing body bone %s"), *Name.ToString()));
            const int32 Parent = Target.GetParentIndex(Index);
            const int32 SourceParent = Source.GetParentIndex(SourceIndex);
            if ((Parent == INDEX_NONE) != (SourceParent == INDEX_NONE) || (Parent != INDEX_NONE && Target.GetBoneName(Parent) != Source.GetBoneName(SourceParent))) return Fail(*FString::Printf(TEXT("Body bone %s has a different parent"), *Name.ToString()));
            const FTransform& TargetPose = Target.GetRawRefBonePose()[Index];
            const FTransform& SourcePose = Source.GetRawRefBonePose()[SourceIndex];
            // Compatible skeleton translation retargeting handles body lengths while rotations and scales stay aligned.
            // 호환 뼈대의 이동 리타기팅으로 체형별 길이를 처리하며 회전과 배율의 일치는 유지합니다.
            if (TargetPose.ContainsNaN() || TargetPose.GetTranslation().GetAbsMax() > 500.0 || (!bAllowBodyProportions && !TargetPose.GetTranslation().Equals(SourcePose.GetTranslation(), 0.01)) || !TargetPose.GetRotation().GetNormalized().Equals(SourcePose.GetRotation().GetNormalized(), 0.0001) || !TargetPose.GetScale3D().Equals(SourcePose.GetScale3D(), 0.0001)) return Fail(*FString::Printf(TEXT("Body bone %s has an incompatible bind pose: %s / %s"), *Name.ToString(), *TargetPose.ToString(), *SourcePose.ToString()));
        }
        return Target.GetRawBoneNum() > 0;
    };
    const FReferenceSkeleton& Target = BodyMesh->GetSkeleton()->GetReferenceSkeleton();
    if (Target.GetRawBoneNum() != Source.GetRawBoneNum()) return Fail(TEXT("Body and animation skeleton bone counts differ"));
    if (!Matches(Target, false) || !Matches(BodyMesh->GetRefSkeleton(), true)) return false;
    if (!AnimationSkeleton->IsCompatibleMesh(BodyMesh)) return Fail(TEXT("The animation skeleton rejects the body mesh hierarchy"));
    for (FName Bone : {FName(TEXT("ball_r")), FName(TEXT("calf_r")), FName(TEXT("upperarm_l")), FName(TEXT("pelvis"))})
    {
        const int32 Index = Source.FindBoneIndex(Bone);
        if (Index != INDEX_NONE) UE_LOG(LogCharacterAppearanceAssets, Display, TEXT("Translation retarget mode / 이동 리타기팅 모드 %s: %d"), *Bone.ToString(), static_cast<int32>(AnimationSkeleton->GetBoneTranslationRetargetingMode(Index)));
    }
    UE_LOG(LogCharacterAppearanceAssets, Display, TEXT("Verified animation skeleton for %s: %d bones / 뼈대 계층과 회전·배율 호환을 확인했으며 이동은 체형별 리타기팅을 사용합니다."), *BodyMesh->GetName(), Target.GetRawBoneNum());
    return true;
}

bool UCharacterAppearanceAssetLibrary::ConfigureBodyAnimationSkeleton(USkeletalMesh* BodyMesh, USkeleton* AnimationSkeleton)
{
    if (!ValidateBodyAnimationSkeleton(BodyMesh, AnimationSkeleton)) return false;
    USkeleton* Skeleton = BodyMesh->GetSkeleton();
    if (Skeleton->GetPathName() != TEXT("/Game/Primitive_Characters_Pack/Demoscene_UE5/Mesh/SKM_Manny_Skeleton.SKM_Manny_Skeleton")) return Fail(TEXT("Only the selected Primitive skeleton can be configured"));
    if (!Skeleton->GetCompatibleSkeletons().Contains(AnimationSkeleton) || !Skeleton->GetUseRetargetModesFromCompatibleSkeleton() || !Skeleton->ContainsSlotName(TEXT("DefaultSlot")))
    {
        Skeleton->Modify();
        Skeleton->AddCompatibleSkeleton(AnimationSkeleton);
        Skeleton->SetUseRetargetModesFromCompatibleSkeleton(true);
        if (!Skeleton->ContainsSlotName(TEXT("DefaultSlot"))) Skeleton->SetSlotGroupName(TEXT("DefaultSlot"), TEXT("DefaultGroup"));
        Skeleton->MarkPackageDirty();
    }
    return true;
}

TArray<int32> UCharacterAppearanceAssetLibrary::ValidateBodyMaterialSlots(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart)
{
    FBodyMaterialPlan Plan;
    if (!MakePlan(SourceMesh, BodyPart, Plan)) return {};
    const TArray<int32> Slots = ReadSlotMap(BodyPart, Plan);
    if (Slots.IsEmpty())
    {
        Fail(TEXT("Body material slots still merge different original Manny surfaces"));
        return {};
    }
    if (!ValidateBuiltSections(BodyPart, Plan, Slots)) return {};
    UE_LOG(LogCharacterAppearanceAssets, Display, TEXT("Verified %s: %d triangles, %d material slots / 원본 텍스처의 삼각형별 재질 대응을 확인했습니다."), *BodyPart->GetName(), Plan.TriangleCounts[0] + Plan.TriangleCounts[1], Slots.Num());
    return Slots;
}

TArray<int32> UCharacterAppearanceAssetLibrary::RestoreBodyMaterialSlots(USkeletalMesh* SourceMesh, USkeletalMesh* BodyPart)
{
    FBodyMaterialPlan Plan;
    if (!MakePlan(SourceMesh, BodyPart, Plan)) return {};
    const TArray<int32> ExistingSlots = ReadSlotMap(BodyPart, Plan);
    if (!ExistingSlots.IsEmpty()) return ValidateBuiltSections(BodyPart, Plan, ExistingSlots) ? ExistingSlots : TArray<int32>();
    FMeshDescription* Body = BodyPart->GetMeshDescription(0);
    FSkeletalMeshLODModel& LOD = BodyPart->GetImportedModel()->LODModels[0];
    if (Plan.UsedMaterials.Num() != 2 || BodyPart->GetMaterials().Num() != 1 || Body->PolygonGroups().Num() != 1 || LOD.Sections.Num() != 1 || LOD.Sections[0].HasClothingData())
    {
        Fail(TEXT("Only the original single-section mixed-material body parts can be repaired"));
        return {};
    }
    const TArray<uint32> OriginalGeometry = GeometryHashes(*Body);
    const TArray<FSkeletalMaterial> OriginalMaterials = BodyPart->GetMaterials();
    FMeshDescription OriginalDescription(*Body);
    const auto OriginalSections = LOD.UserSectionsData;
    auto PreservedSections = OriginalSections;
    const FSkelMeshSourceSectionUserData PreservedSection = FSkelMeshSourceSectionUserData::GetSourceSectionUserData(PreservedSections, LOD.Sections[0]);
    const auto* OriginalSkeleton = BodyPart->GetSkeleton();
    const auto* OriginalPhysics = BodyPart->GetPhysicsAsset();
    const TArray<FTransform> OriginalPose = BodyPart->GetRefSkeleton().GetRawRefBonePose();
    const TArray<FMeshBoneInfo> OriginalBones = BodyPart->GetRefSkeleton().GetRawRefBoneInfo();

    // Keep every vertex attribute and the pack's default material; only restore the original surface boundaries.
    // 모든 정점 속성과 팩의 기본 재질을 유지하며 원본 표면의 경계만 복원합니다.
    BodyPart->Modify();
    const FPolygonGroupID FirstGroup = *Body->PolygonGroups().GetElementIDs().begin();
    const FPolygonGroupID SecondGroup = Body->CreatePolygonGroup();
    const TArray<FPolygonGroupID> Groups = {FirstGroup, SecondGroup};
    FSkeletalMeshAttributes Attributes(*Body);
    auto GroupNames = Attributes.GetPolygonGroupMaterialSlotNames();
    TArray<FSkeletalMaterial> Materials;
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        FSkeletalMaterial Material = OriginalMaterials[0];
        Material.MaterialSlotName = SourceMesh->GetMaterials()[Slot].MaterialSlotName;
        Material.ImportedMaterialSlotName = SourceMesh->GetMaterials()[Slot].ImportedMaterialSlotName.IsNone() ? Material.MaterialSlotName : SourceMesh->GetMaterials()[Slot].ImportedMaterialSlotName;
        Materials.Add(Material);
        GroupNames[Groups[Slot]] = Material.ImportedMaterialSlotName;
    }
    for (const auto& Pair : Plan.PolygonMaterials) Body->SetPolygonPolygonGroup(Pair.Key, Groups[Pair.Value]);
    BodyPart->SetMaterials(Materials);
    LOD.UserSectionsData.Reset();
    LOD.UserSectionsData.Add(0, PreservedSection);
    LOD.UserSectionsData.Add(1, PreservedSection);
    bool bValid = BodyPart->CommitMeshDescription(0);
    if (bValid)
    {
        BodyPart->PostEditChange();
        FSkinnedAssetCompilingManager::Get().FinishCompilation({BodyPart});
        const FMeshDescription* Updated = BodyPart->GetMeshDescription(0);
        const bool bGeometryUnchanged = Updated && GeometryHashes(*Updated) == OriginalGeometry;
        const TArray<FTransform>& UpdatedPose = BodyPart->GetRefSkeleton().GetRawRefBonePose();
        bool bReferencesUnchanged = BodyPart->GetSkeleton() == OriginalSkeleton && BodyPart->GetPhysicsAsset() == OriginalPhysics && UpdatedPose.Num() == OriginalPose.Num();
        for (int32 Index = 0; bReferencesUnchanged && Index < OriginalPose.Num(); ++Index) bReferencesUnchanged = UpdatedPose[Index].Equals(OriginalPose[Index], 0.0);
        bool bBonesUnchanged = BodyPart->GetRefSkeleton().GetRawBoneNum() == OriginalBones.Num();
        for (int32 Bone = 0; bBonesUnchanged && Bone < OriginalBones.Num(); ++Bone)
        {
            const FMeshBoneInfo& UpdatedBone = BodyPart->GetRefSkeleton().GetRawRefBoneInfo()[Bone];
            bBonesUnchanged = UpdatedBone.Name == OriginalBones[Bone].Name && UpdatedBone.ParentIndex == OriginalBones[Bone].ParentIndex;
        }
        UE_LOG(LogCharacterAppearanceAssets, Display, TEXT("%s invariants: vertex/UV/normal/weight attributes unchanged=%d, skeleton/physics/pose unchanged=%d, bone hierarchy unchanged=%d / 원본 신체 데이터 보존 검사"), *BodyPart->GetName(), bGeometryUnchanged, bReferencesUnchanged, bBonesUnchanged);
        bValid = bGeometryUnchanged && bReferencesUnchanged && bBonesUnchanged;
        if (bValid) bValid = ValidateBuiltSections(BodyPart, Plan, {0, 1});
        for (const FSkelMeshSection& Section : BodyPart->GetImportedModel()->LODModels[0].Sections)
        {
            bValid &= Section.bCastShadow == PreservedSection.bCastShadow && Section.bVisibleInRayTracing == PreservedSection.bVisibleInRayTracing && Section.bDisabled == PreservedSection.bDisabled && Section.bRecomputeTangent == PreservedSection.bRecomputeTangent && Section.RecomputeTangentsVertexMaskChannel == PreservedSection.RecomputeTangentsVertexMaskChannel && Section.GenerateUpToLodIndex == PreservedSection.GenerateUpToLodIndex;
        }
    }
    if (!bValid)
    {
        BodyPart->CreateMeshDescription(0, MoveTemp(OriginalDescription));
        BodyPart->SetMaterials(OriginalMaterials);
        BodyPart->GetImportedModel()->LODModels[0].UserSectionsData = OriginalSections;
        BodyPart->CommitMeshDescription(0);
        BodyPart->PostEditChange();
        FSkinnedAssetCompilingManager::Get().FinishCompilation({BodyPart});
        Fail(TEXT("Body invariants failed; the original mesh description and materials were restored without saving"));
        return {};
    }
    UE_LOG(LogCharacterAppearanceAssets, Display, TEXT("Restored %s: two material sections, unchanged geometry and weights / 지오메트리와 가중치를 유지하며 재질 섹션 2개를 복원했습니다."), *BodyPart->GetName());
    return {0, 1};
}
