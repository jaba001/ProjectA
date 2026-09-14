#include "Combat/SkillActor/SkillActorBase.h"
#include "Components/SceneComponent.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"

ASkillActorBase::ASkillActorBase()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}

void ASkillActorBase::InitializeSkillActor(const FSkillActorInitData& InitData)
{
    if (!HasAuthority() || bInitialized || bResolved) return;
    bInitialized = true;
    SourceUnit = InitData.SourceUnit;
    SkillData = InitData.SkillData;
    TargetTile = InitData.TargetTile;
    TargetWorldLocation = InitData.TargetWorldLocation;
    // Serialized legacy actors must never restart tile-based impact execution.
    // 직렬화된 기존 액터가 타일 기반 임팩트 실행을 다시 시작하지 않도록 합니다.
    UE_LOG(LogTemp, Warning, TEXT("[SkillActorBase] Legacy skill actors cannot execute combat effects."));
    ResolveSkillActor(false);
    SetLifeSpan(0.1f);
}

void ASkillActorBase::RequestImpact()
{
    if (!HasAuthority()) return;
    ResolveSkillActor(false);
    SetLifeSpan(0.1f);
}

void ASkillActorBase::RequestFinish()
{
    if (!HasAuthority()) return;
    FinishSkillActor();
}

AUnitBase* ASkillActorBase::GetSourceUnit() const
{
    return SourceUnit;
}

USkillDefinitionDataAsset* ASkillActorBase::GetSkillData() const
{
    return SkillData;
}

ACombatGridTile* ASkillActorBase::GetTargetTile() const
{
    return TargetTile;
}

FVector ASkillActorBase::GetTargetWorldLocation() const
{
    return TargetWorldLocation;
}

bool ASkillActorBase::HasImpactHandled() const
{
    return bImpactHandled;
}

void ASkillActorBase::BeginSkillActor()
{
}

void ASkillActorBase::HandleImpact()
{
    ResolveSkillActor(false);
}

void ASkillActorBase::FinishSkillActor()
{
    ResolveSkillActor(false);
    SetLifeSpan(DestroyDelayAfterFinish);
}

void ASkillActorBase::ResolveSkillActor(bool bSucceeded)
{
    if (bResolved)
    {
        return;
    }
    bResolved = true;
    OnSkillActorResolved.Broadcast(this, bSucceeded);
    OnSkillActorResolved.Clear();
}

void ASkillActorBase::Destroyed()
{
    ResolveSkillActor(false);
    Super::Destroyed();
}

void ASkillActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResolveSkillActor(false);
    Super::EndPlay(EndPlayReason);
}
