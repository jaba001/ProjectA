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
    if (bInitialized || bResolved)
    {
        return;
    }
    bInitialized = true;
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
    if (!bInitialized || bResolved || bImpactHandled)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillActorBase] RequestImpact Ignored | Reason=AlreadyHandled | Actor=%s"), *GetNameSafe(this));
        return;
    }

    bImpactHandled = true;

    HandleImpact();
}

void ASkillActorBase::RequestFinish()
{
    // Finishing without impact reports failure to the owning ability.
    // 임팩트 없는 종료는 소유 어빌리티에 실패로 보고합니다.
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
    ResolveSkillActor(bImpactHandled);
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
