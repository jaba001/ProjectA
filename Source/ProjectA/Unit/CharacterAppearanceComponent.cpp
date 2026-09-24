#include "Unit/CharacterAppearanceComponent.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAsset/CharacterAppearanceCatalog.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterAppearance, Log, All);

UCharacterAppearanceComponent::UCharacterAppearanceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCharacterAppearanceComponent::OnRegister()
{
    Super::OnRegister();
    RefreshAppearance();
}

void UCharacterAppearanceComponent::BeginPlay()
{
    Super::BeginPlay();
    // Component registration can precede assignment of the owner's skeletal mesh.
    // 컴포넌트 등록이 소유자의 스켈레탈 메시 지정보다 먼저 실행될 수 있습니다.
    RefreshAppearance();
}

void UCharacterAppearanceComponent::OnUnregister()
{
    RemoveModularMeshes();
    RestorePoseLeader();
    RestoreOriginalBody();
    bAppearanceApplied = false;
    Super::OnUnregister();
}

void UCharacterAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCharacterAppearanceComponent, AppearanceCatalog);
    DOREPLIFETIME(UCharacterAppearanceComponent, Selection);
}

void UCharacterAppearanceComponent::OnRep_Appearance()
{
    RefreshAppearance();
}

USkeletalMeshComponent* UCharacterAppearanceComponent::FindPoseLeader() const
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return nullptr;
    if (const ACharacter* Character = Cast<ACharacter>(Owner)) return Character->GetMesh();
    if (IsValid(PoseLeader)) return PoseLeader;
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        if (IsValid(Mesh) && !ModularMeshes.Contains(Mesh)) return Mesh;
    }
    return nullptr;
}

void UCharacterAppearanceComponent::RemoveModularMeshes()
{
    for (USkeletalMeshComponent* Mesh : ModularMeshes)
    {
        if (!IsValid(Mesh)) continue;
        if (AActor* Owner = GetOwner()) Owner->RemoveInstanceComponent(Mesh);
        Mesh->DestroyComponent();
    }
    ModularMeshes.Reset();
}

void UCharacterAppearanceComponent::RestorePoseLeader()
{
    if (bLeaderVisibilitySaved && IsValid(PoseLeader))
    {
        PoseLeader->SetVisibility(bLeaderWasVisible, false);
        PoseLeader->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(LeaderPreviousTickOption);
    }
    bLeaderVisibilitySaved = false;
    PoseLeader = nullptr;
}

void UCharacterAppearanceComponent::SaveOriginalBody(USkeletalMeshComponent* Leader)
{
    if (OriginalBodyLeader == Leader) return;
    OriginalBodyLeader = Leader;
    OriginalBodyMesh = Leader->GetSkeletalMeshAsset();
    OriginalBodyPhysics = Leader->GetPhysicsAsset();
    OriginalAnimationClass = Leader->GetAnimClass();
    OriginalAnimationMode = static_cast<uint8>(Leader->GetAnimationMode());
    OriginalAnimationData = Leader->AnimationData;
    if (UAnimSingleNodeInstance* Animation = Leader->GetSingleNodeInstance()) OriginalAnimationData.PopulateFrom(Animation);
    OriginalMaterialOverrides = Leader->OverrideMaterials;
    OriginalMeshTransform = Leader->GetRelativeTransform();
}

void UCharacterAppearanceComponent::RestoreOriginalBody()
{
    if (bBodyVariantApplied && IsValid(OriginalBodyLeader))
    {
        OriginalBodyLeader->SetSkeletalMeshAsset(OriginalBodyMesh);
        OriginalBodyLeader->EmptyOverrideMaterials();
        for (int32 MaterialIndex = 0; MaterialIndex < OriginalMaterialOverrides.Num(); ++MaterialIndex)
        {
            OriginalBodyLeader->SetMaterial(MaterialIndex, OriginalMaterialOverrides[MaterialIndex]);
        }
        OriginalBodyLeader->SetPhysicsAsset(OriginalBodyPhysics, false);
        OriginalBodyLeader->SetRelativeTransform(OriginalMeshTransform);
        OriginalBodyLeader->SetAnimInstanceClass(OriginalAnimationClass);
        OriginalBodyLeader->SetAnimationMode(static_cast<EAnimationMode::Type>(OriginalAnimationMode));
        if (OriginalAnimationMode == EAnimationMode::AnimationSingleNode)
        {
            OriginalBodyLeader->OverrideAnimationData(OriginalAnimationData.AnimToPlay, OriginalAnimationData.bSavedLooping, OriginalAnimationData.bSavedPlaying, OriginalAnimationData.SavedPosition, OriginalAnimationData.SavedPlayRate);
        }
    }
    bBodyVariantApplied = false;
    OriginalBodyLeader = nullptr;
    OriginalBodyMesh = nullptr;
    OriginalBodyPhysics = nullptr;
    OriginalAnimationClass = nullptr;
    OriginalAnimationData = FSingleAnimationPlayData();
    OriginalMaterialOverrides.Reset();
}

bool UCharacterAppearanceComponent::SetAppearance(UCharacterAppearanceCatalog* InCatalog, const FCharacterAppearanceSelection& InSelection)
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || (Owner->GetNetMode() != NM_Standalone && !Owner->HasAuthority())) return false;
    FText Error;
    if ((!InCatalog && !InSelection.IsEmpty()) || (InCatalog && !InCatalog->ValidateSelection(InSelection, Error))) return false;
    UCharacterAppearanceCatalog* PreviousCatalog = AppearanceCatalog;
    const FCharacterAppearanceSelection PreviousSelection = Selection;
    AppearanceCatalog = InCatalog;
    Selection = InSelection;
    if (!RefreshAppearance())
    {
        AppearanceCatalog = PreviousCatalog;
        Selection = PreviousSelection;
        RefreshAppearance();
        return false;
    }
    Owner->ForceNetUpdate();
    return true;
}

bool UCharacterAppearanceComponent::RefreshAppearance()
{
    AActor* Owner = GetOwner();
    // Runtime previews are game-world actors; leave saved editor actors and templates untouched.
    // 런타임 프리뷰는 게임 월드 액터이며 저장된 에디터 액터와 템플릿은 변경하지 않습니다.
    if (IsTemplate() || !IsValid(Owner) || Owner->IsTemplate() || !Owner->GetWorld() || !Owner->GetWorld()->IsGameWorld()) return false;
    USkeletalMeshComponent* Leader = FindPoseLeader();
    const auto Fail = [this](const FString& Reason)
    {
        UE_LOG(LogCharacterAppearance, Warning, TEXT("%s: %s; retaining the previous appearance."), *GetPathName(), *Reason);
        return false;
    };
    if (!AppearanceCatalog)
    {
        if (!Selection.IsEmpty()) return Fail(TEXT("Appearance selection has no catalog"));
        RemoveModularMeshes();
        RestorePoseLeader();
        RestoreOriginalBody();
        bAppearanceApplied = false;
        ApplyHiddenMeshBones();
        return true;
    }
    if (!IsValid(Leader) || Leader->IsTemplate() || !IsValid(Leader->GetSkeletalMeshAsset())) return Fail(TEXT("Appearance pose leader is not ready"));
    // Replication and BeginPlay can repeat the same selection after death; retain animation and physics state.
    // 복제와 BeginPlay에서 사망 후 같은 선택이 반복되어도 애니메이션과 물리 상태를 유지합니다.
    if (bAppearanceApplied && Leader == PoseLeader && AppliedCatalog == AppearanceCatalog && AppliedSelection == Selection && Leader->GetSkeletalMeshAsset() == AppliedBodyMesh) return true;

    FText Error;
    if (!AppearanceCatalog->ValidateSelection(Selection, Error)) return Fail(Error.ToString());
    const FCharacterAppearanceBodyVariant* Body = AppearanceCatalog->FindBodyVariant(Selection.BodyId);
    const bool bCharacterBody = Cast<ACharacter>(Owner) != nullptr;
    USkeletalMesh* BodyMesh = Body ? Body->Mesh.LoadSynchronous() : OriginalBodyLeader == Leader && bBodyVariantApplied ? OriginalBodyMesh.Get() : Leader->GetSkeletalMeshAsset();
    if (!IsValid(BodyMesh)) return Fail(TEXT("Cannot load the selected body mesh"));
    UClass* AnimationClass = Body && bCharacterBody ? Body->AnimationClass.LoadSynchronous() : nullptr;
    UAnimationAsset* PreviewAnimation = Body && !bCharacterBody ? Body->PreviewAnimation.LoadSynchronous() : nullptr;
    if (Body && bCharacterBody && !Body->AnimationClass.IsNull() && !IsValid(AnimationClass)) return Fail(TEXT("Cannot load the selected body animation class"));
    if (Body && !bCharacterBody && !Body->PreviewAnimation.IsNull() && !IsValid(PreviewAnimation)) return Fail(TEXT("Cannot load the selected body preview animation"));
    if (const IAnimClassInterface* AnimationInterface = IAnimClassInterface::GetFromClass(AnimationClass))
    {
        if (!AnimationInterface->GetTargetSkeleton() || !AnimationInterface->GetTargetSkeleton()->IsCompatibleMesh(BodyMesh)) return Fail(TEXT("The body animation class is incompatible with the selected mesh"));
    }
    if (PreviewAnimation && (!PreviewAnimation->GetSkeleton() || !PreviewAnimation->GetSkeleton()->IsCompatibleMesh(BodyMesh))) return Fail(TEXT("The preview animation is incompatible with the selected mesh"));
    struct FAppearanceMeshPlan
    {
        TSoftObjectPtr<USkeletalMesh> MeshReference;
        TArray<TSoftObjectPtr<UMaterialInterface>> MaterialReferences;
        USkeletalMesh* Mesh = nullptr;
        TArray<UMaterialInterface*> Materials;
    };
    FGameplayTagContainer HiddenParts;
    TArray<FAppearanceMeshPlan> MeshPlans;
    if (AppearanceCatalog->bEnableOutfits)
    {
        for (FName ItemId : Selection.ItemIds)
        {
            const FCharacterAppearanceItem* Item = AppearanceCatalog->FindItem(ItemId);
            if (!Item) return Fail(TEXT("Selected item was removed from its catalog"));
            HiddenParts.AppendTags(Item->HiddenBodyParts);
            for (const TSoftObjectPtr<USkeletalMesh>& Mesh : Item->Meshes)
            {
                FAppearanceMeshPlan& Plan = MeshPlans.AddDefaulted_GetRef();
                Plan.MeshReference = Mesh;
            }
        }
    }
    const bool bUseModularBody = !HiddenParts.IsEmpty();
    // Use the original body when clothing does not require hiding any body region.
    // 의상에서 신체 부위를 숨길 필요가 없으면 원본 전체 신체를 표시합니다.
    if (bUseModularBody)
    {
        for (const FCharacterAppearanceBodyPart& Part : AppearanceCatalog->BodyParts)
        {
            if (HiddenParts.HasTagExact(Part.PartTag)) continue;
            FAppearanceMeshPlan& Plan = MeshPlans.AddDefaulted_GetRef();
            Plan.MeshReference = Part.Mesh;
            Plan.MaterialReferences = Part.MaterialOverrides;
        }
    }

    // Resolve each part's mesh and materials before replacing the visible appearance.
    // 보이는 외형을 교체하기 전에 각 파츠의 메시와 재질을 로드하고 확인합니다.
    const FReferenceSkeleton& LeaderBones = BodyMesh->GetRefSkeleton();
    for (FAppearanceMeshPlan& Plan : MeshPlans)
    {
        USkeletalMesh* Mesh = Plan.MeshReference.LoadSynchronous();
        if (!IsValid(Mesh)) return Fail(FString::Printf(TEXT("Cannot load %s"), *Plan.MeshReference.ToString()));
        const FReferenceSkeleton& FollowerBones = Mesh->GetRefSkeleton();
        for (int32 BoneIndex = 0; BoneIndex < FollowerBones.GetRawBoneNum(); ++BoneIndex)
        {
            const FName BoneName = FollowerBones.GetBoneName(BoneIndex);
            if (LeaderBones.FindBoneIndex(BoneName) == INDEX_NONE) return Fail(FString::Printf(TEXT("%s requires missing leader bone %s"), *Mesh->GetName(), *BoneName.ToString()));
        }
        if (Plan.MaterialReferences.Num() > Mesh->GetMaterials().Num()) return Fail(FString::Printf(TEXT("%s has fewer material slots than its body overrides"), *Mesh->GetName()));
        for (const TSoftObjectPtr<UMaterialInterface>& MaterialReference : Plan.MaterialReferences)
        {
            UMaterialInterface* Material = MaterialReference.LoadSynchronous();
            if (!IsValid(Material)) return Fail(FString::Printf(TEXT("Cannot load %s"), *MaterialReference.ToString()));
            Plan.Materials.Add(Material);
        }
        Plan.Mesh = Mesh;
    }
    if (bUseModularBody && MeshPlans.IsEmpty()) return Fail(TEXT("Appearance has no visible body or clothing meshes"));

    RemoveModularMeshes();
    if (Leader != PoseLeader)
    {
        RestorePoseLeader();
        RestoreOriginalBody();
    }
    if (Body)
    {
        SaveOriginalBody(Leader);
        const bool bMeshChanged = Leader->GetSkeletalMeshAsset() != BodyMesh;
        const FTransform& MeshTransform = bCharacterBody ? Body->MeshTransform : Body->PreviewMeshTransform;
        if (bMeshChanged) Leader->SetSkeletalMeshAsset(BodyMesh);
        if (bMeshChanged || !bBodyVariantApplied) Leader->EmptyOverrideMaterials();
        if (Leader->GetPhysicsAsset() != BodyMesh->GetPhysicsAsset()) Leader->SetPhysicsAsset(BodyMesh->GetPhysicsAsset(), false);
        if (!bBodyVariantApplied || !AppliedMeshTransform.Equals(MeshTransform)) Leader->SetRelativeTransform(MeshTransform);
        if (AnimationClass && (Leader->GetAnimClass() != AnimationClass || Leader->GetAnimationMode() != EAnimationMode::AnimationBlueprint)) Leader->SetAnimInstanceClass(AnimationClass);
        UAnimSingleNodeInstance* SingleAnimation = Leader->GetSingleNodeInstance();
        if (PreviewAnimation && (bMeshChanged || !SingleAnimation || SingleAnimation->GetAnimationAsset() != PreviewAnimation)) Leader->PlayAnimation(PreviewAnimation, true);
        AppliedMeshTransform = MeshTransform;
        bBodyVariantApplied = true;
    }
    else
    {
        RestoreOriginalBody();
    }
    PoseLeader = Leader;
    ApplyHiddenMeshBones();
    if (!bLeaderVisibilitySaved)
    {
        bLeaderWasVisible = Leader->IsVisible();
        LeaderPreviousTickOption = static_cast<uint8>(Leader->VisibilityBasedAnimTickOption);
        bLeaderVisibilitySaved = true;
    }
    for (const FAppearanceMeshPlan& Plan : MeshPlans)
    {
        USkeletalMeshComponent* Follower = NewObject<USkeletalMeshComponent>(Owner, NAME_None, RF_Transient);
        Owner->AddInstanceComponent(Follower);
        ModularMeshes.Add(Follower);
        Follower->SetupAttachment(Leader);
        Follower->SetRelativeTransform(FTransform::Identity);
        Follower->SetSkeletalMeshAsset(Plan.Mesh);
        for (int32 MaterialIndex = 0; MaterialIndex < Plan.Materials.Num(); ++MaterialIndex)
        {
            Follower->SetMaterial(MaterialIndex, Plan.Materials[MaterialIndex]);
        }
        Follower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Follower->SetGenerateOverlapEvents(false);
        Follower->SetCanEverAffectNavigation(false);
        Follower->SetSimulatePhysics(false);
        Follower->SetLeaderPoseComponent(Leader, true, false);
        Follower->RegisterComponent();
    }

    // Keep the original animation and ragdoll leader evaluating even while its surface is hidden.
    // 원본 표면을 숨겨도 애니메이션과 래그돌을 담당하는 리더는 계속 계산합니다.
    Leader->VisibilityBasedAnimTickOption = MeshPlans.IsEmpty() ? static_cast<EVisibilityBasedAnimTickOption>(LeaderPreviousTickOption) : EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Leader->SetVisibility(bUseModularBody ? false : bLeaderWasVisible, false);
    AppliedCatalog = AppearanceCatalog;
    AppliedSelection = Selection;
    AppliedBodyMesh = BodyMesh;
    bAppearanceApplied = true;
    return true;
}

void UCharacterAppearanceComponent::ApplyHiddenMeshBones()
{
    AActor* Owner = GetOwner();
    if (IsTemplate() || !IsValid(Owner) || Owner->IsTemplate() || HiddenMeshBones.IsEmpty()) return;
    USkeletalMeshComponent* Mesh = FindPoseLeader();
    if (!IsValid(Mesh) || Mesh->IsTemplate() || !IsValid(Mesh->GetSkeletalMeshAsset())) return;
    for (FName BoneName : HiddenMeshBones)
    {
        if (BoneName.IsNone() || Mesh->GetBoneIndex(BoneName) == INDEX_NONE || Mesh->IsBoneHiddenByName(BoneName)) continue;
        Mesh->HideBoneByName(BoneName, PBO_None);
    }
}
