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
    SourceUnit = InitData.SourceUnit;
    SkillData = InitData.SkillData;
    TargetTile = InitData.TargetTile;
    TargetWorldLocation = InitData.TargetWorldLocation;
    bImpactHandled = false;

    if (TargetTile && TargetWorldLocation.IsNearlyZero())
    {
        TargetWorldLocation = TargetTile->GetActorLocation();
    }

    //UE_LOG(LogTemp, Log, TEXT("[SkillActorBase] InitializeSkillActor | Actor=%s | Source=%s | Skill=%s | TargetTile=%s | TargetLocation=%s"), *GetNameSafe(this), *GetNameSafe(SourceUnit), *GetNameSafe(SkillData), *GetNameSafe(TargetTile), *TargetWorldLocation.ToString());

    BeginSkillActor();
}

void ASkillActorBase::RequestImpact()
{
    if (bImpactHandled)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillActorBase] RequestImpact Ignored | Reason=AlreadyHandled | Actor=%s"), *GetNameSafe(this));
        return;
    }

    bImpactHandled = true;

    HandleImpact();
}

void ASkillActorBase::RequestFinish()
{
    // TODO: Decide whether spawned skill actors should drive SourceUnit skill completion after impact.
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
    FinishSkillActor();
}

void ASkillActorBase::FinishSkillActor()
{
    SetLifeSpan(DestroyDelayAfterFinish);
}
