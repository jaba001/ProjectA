#include "Combat/SkillActor/AttackSkillActorBase.h"

#include "GAS/Library/CombatEffectLibrary.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "GameplayEffect.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"

AAttackSkillActorBase::AAttackSkillActorBase()
{
}

void AAttackSkillActorBase::InitializeAttackSkillActor(const FSkillActorInitData& InitData, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageAmount)
{
    DamageEffectClass = InDamageEffectClass;
    DamageAmount = InDamageAmount;

    InitializeSkillActor(InitData);

    UE_LOG(LogTemp, Log, TEXT("[AttackSkillActorBase] InitializeAttackSkillActor | Actor=%s | DamageEffect=%s | DamageAmount=%.2f"), *GetNameSafe(this), *GetNameSafe(DamageEffectClass), DamageAmount);
}

TSubclassOf<UGameplayEffect> AAttackSkillActorBase::GetDamageEffectClass() const
{
    return DamageEffectClass;
}

float AAttackSkillActorBase::GetDamageAmount() const
{
    return DamageAmount;
}

void AAttackSkillActorBase::HandleImpact()
{
    ApplyImpactEffect();
    Super::HandleImpact();
}

void AAttackSkillActorBase::ApplyImpactEffect()
{
    if (!HasValidImpactContext())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AttackSkillActorBase] ApplyImpactEffect Failed | MissingContext | Actor=%s | Source=%s | Skill=%s | TargetTile=%s"), *GetNameSafe(this), *GetNameSafe(SourceUnit), *GetNameSafe(SkillData), *GetNameSafe(TargetTile));
        return;
    }

    if (!DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AttackSkillActorBase] ApplyImpactEffect Failed | DamageEffectClassNull | Actor=%s"), *GetNameSafe(this));
        return;
    }

    const TArray<AUnitBase*> TargetUnits = ResolveImpactTargetUnits();
    if (TargetUnits.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AttackSkillActorBase] ApplyImpactEffect Skipped | NoTargets | Actor=%s | TargetTile=%s"), *GetNameSafe(this), *GetNameSafe(TargetTile));
        return;
    }

    const int32 AppliedCount = UCombatEffectLibrary::ApplyDamageToUnits(SourceUnit, TargetUnits, DamageEffectClass, DamageAmount);
    UE_LOG(LogTemp, Log, TEXT("[AttackSkillActorBase] ApplyImpactEffect | Actor=%s | TargetCount=%d | AppliedCount=%d"), *GetNameSafe(this), TargetUnits.Num(), AppliedCount);
}

TArray<AUnitBase*> AAttackSkillActorBase::ResolveImpactTargetUnits() const
{
    TArray<AUnitBase*> Result;
    if (!HasValidImpactContext())
    {
        return Result;
    }

    ACombatGridManager* CombatGridManager = ResolveCombatGridManager();
    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AttackSkillActorBase] ResolveImpactTargetUnits Failed | CombatGridManagerNull | Actor=%s"), *GetNameSafe(this));
        return Result;
    }

    const TArray<ACombatGridTile*> AreaTiles = ResolveImpactAreaTiles(CombatGridManager);
    const TArray<AUnitBase*> RawUnits = UCombatTargetingLibrary::CollectUniqueAliveUnitsFromTiles(AreaTiles, SourceUnit);

    for (AUnitBase* TargetUnit : RawUnits)
    {
        if (IsValidImpactTargetUnit(TargetUnit))
        {
            Result.Add(TargetUnit);
        }
    }

    return Result;
}

bool AAttackSkillActorBase::IsValidImpactTargetUnit(AUnitBase* TargetUnit) const
{
    if (!SourceUnit || !SkillData || !TargetUnit || !TargetUnit->IsUnitAlive())
    {
        return false;
    }

    const ETeam SourceTeam = SourceUnit->GetTeam();
    const ETeam TargetTeam = TargetUnit->GetTeam();

    switch (SkillData->TargetRule)
    {
    case ESkillTargetRule::EnemyUnit:
    case ESkillTargetRule::EnemyTile:
        return SourceTeam != TargetTeam;

    case ESkillTargetRule::AllyUnit:
    case ESkillTargetRule::AllyTile:
        return SourceTeam == TargetTeam;

    case ESkillTargetRule::AnyUnit:
    case ESkillTargetRule::AnyTile:
        return true;

    default:
        return false;
    }
}

bool AAttackSkillActorBase::HasValidImpactContext() const
{
    return SourceUnit && SkillData && TargetTile;
}

ACombatGridManager* AAttackSkillActorBase::ResolveCombatGridManager() const
{
    if (IsValid(CachedCombatGridManager))
    {
        return CachedCombatGridManager.Get();
    }

    CachedCombatGridManager = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatGridManager::StaticClass()));
    return CachedCombatGridManager.Get();
}

TArray<ACombatGridTile*> AAttackSkillActorBase::ResolveImpactAreaTiles(ACombatGridManager* CombatGridManager) const
{
    TArray<ACombatGridTile*> AreaTiles;

    if (!CombatGridManager || !TargetTile || !SkillData)
    {
        return AreaTiles;
    }

    if (SkillData->AreaType == ESkillAreaType::Single)
    {
        AreaTiles.Add(TargetTile);
        return AreaTiles;
    }

    return CombatGridManager->GetTilesInChebyshevRange(TargetTile, SkillData->AreaRadius);
}
