#include "Unit/CharacterEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Unit/UnitBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterEquipment, Log, All);

UCharacterEquipmentComponent::UCharacterEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCharacterEquipmentComponent::OnRegister()
{
    Super::OnRegister();
    RefreshEquipment();
}

void UCharacterEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshEquipment();
}

void UCharacterEquipmentComponent::OnUnregister()
{
    RemoveEquipmentMeshes();
    Super::OnUnregister();
}

void UCharacterEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCharacterEquipmentComponent, Presentation);
}

void UCharacterEquipmentComponent::OnRep_Presentation()
{
    RefreshEquipment();
}

void UCharacterEquipmentComponent::RemoveEquipmentMeshes()
{
    for (UMeshComponent* Mesh : EquipmentMeshes)
    {
        if (!IsValid(Mesh)) continue;
        if (AActor* Owner = GetOwner()) Owner->RemoveInstanceComponent(Mesh);
        Mesh->DestroyComponent();
    }
    EquipmentMeshes.Reset();
}

bool UCharacterEquipmentComponent::SetEquipment(bool bHasLoadout, const TArray<FRunEquipmentVisual>& Visuals)
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !Owner->HasAuthority() || (!bHasLoadout && !Visuals.IsEmpty())) return false;
    const FCharacterEquipmentPresentation Previous = Presentation;
    Presentation.bHasLoadout = bHasLoadout;
    Presentation.Visuals = Visuals;
    if (!RefreshEquipment())
    {
        Presentation = Previous;
        return false;
    }
    Owner->ForceNetUpdate();
    return true;
}

bool UCharacterEquipmentComponent::RefreshEquipment()
{
    AUnitBase* Unit = Cast<AUnitBase>(GetOwner());
    if (IsTemplate() || !IsValid(Unit) || Unit->IsTemplate() || !Unit->GetWorld() || !Unit->GetWorld()->IsGameWorld()) return false;
    if (!Presentation.bHasLoadout)
    {
        RemoveEquipmentMeshes();
        Unit->RefreshSkillPresentation();
        return true;
    }
    USkeletalMeshComponent* Body = Unit->GetMesh();
    if (!IsValid(Body) || !IsValid(Body->GetSkeletalMeshAsset())) return false;
    TArray<UObject*> Assets;
    for (const FRunEquipmentVisual& Visual : Presentation.Visuals)
    {
        UObject* Asset = Visual.Asset.TryLoad();
        if ((!Cast<UStaticMesh>(Asset) && !Cast<USkeletalMesh>(Asset)) || Visual.SocketName.IsNone() || !Body->DoesSocketExist(Visual.SocketName) || Visual.RelativeTransform.ContainsNaN() || !Visual.RelativeTransform.IsRotationNormalized())
        {
            UE_LOG(LogCharacterEquipment, Warning, TEXT("%s cannot attach %s to %s; retaining the previous equipment."), *GetPathName(), *Visual.Asset.ToString(), *Visual.SocketName.ToString());
            return false;
        }
        Assets.Add(Asset);
    }

    // Resolve every mesh and attachment before replacing the visible equipment.
    // 보이는 장비를 교체하기 전에 모든 메시와 부착 지점을 확인합니다.
    TArray<TObjectPtr<UMeshComponent>> NewMeshes;
    for (int32 Index = 0; Index < Presentation.Visuals.Num(); ++Index)
    {
        const FRunEquipmentVisual& Visual = Presentation.Visuals[Index];
        UMeshComponent* Mesh = nullptr;
        if (UStaticMesh* StaticAsset = Cast<UStaticMesh>(Assets[Index]))
        {
            UStaticMeshComponent* StaticMesh = NewObject<UStaticMeshComponent>(Unit, NAME_None, RF_Transient);
            StaticMesh->SetStaticMesh(StaticAsset);
            Mesh = StaticMesh;
        }
        else
        {
            USkeletalMeshComponent* SkeletalMesh = NewObject<USkeletalMeshComponent>(Unit, NAME_None, RF_Transient);
            SkeletalMesh->SetSkeletalMeshAsset(CastChecked<USkeletalMesh>(Assets[Index]));
            Mesh = SkeletalMesh;
        }
        Unit->AddInstanceComponent(Mesh);
        NewMeshes.Add(Mesh);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetupAttachment(Body, Visual.SocketName);
        Mesh->SetRelativeTransform(Visual.RelativeTransform);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetSimulatePhysics(false);
        Mesh->RegisterComponent();
    }
    RemoveEquipmentMeshes();
    EquipmentMeshes = MoveTemp(NewMeshes);
    Unit->RefreshSkillPresentation();
    return true;
}
