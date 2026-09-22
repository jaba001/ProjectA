#include "Unit/CharacterAppearanceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

UCharacterAppearanceComponent::UCharacterAppearanceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterAppearanceComponent::OnRegister()
{
    Super::OnRegister();
    ApplyHiddenMeshBones();
}

void UCharacterAppearanceComponent::BeginPlay()
{
    Super::BeginPlay();
    // Component registration can precede assignment of the owner's skeletal mesh.
    // 컴포넌트 등록이 소유자의 스켈레탈 메시 지정보다 먼저 실행될 수 있습니다.
    ApplyHiddenMeshBones();
}

void UCharacterAppearanceComponent::ApplyHiddenMeshBones()
{
    AActor* Owner = GetOwner();
    if (IsTemplate() || !IsValid(Owner) || Owner->IsTemplate() || HiddenMeshBones.IsEmpty()) return;
    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!IsValid(Mesh) || Mesh->IsTemplate() || !IsValid(Mesh->GetSkeletalMeshAsset())) return;
    for (FName BoneName : HiddenMeshBones)
    {
        if (BoneName.IsNone() || Mesh->GetBoneIndex(BoneName) == INDEX_NONE || Mesh->IsBoneHiddenByName(BoneName)) continue;
        Mesh->HideBoneByName(BoneName, PBO_None);
    }
}
