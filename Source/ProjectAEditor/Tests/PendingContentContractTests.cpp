#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Combat/Presentation/CombatShoulderFocus.h"
#include "Combat/Round/CombatRoundProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunLevelDesign.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Unit/PlayerUnit.h"
#include "Unit/EnemyUnit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPendingProfessionAndProjectileReview, "ProjectA.Review.CurrentProfessionStatsAndProjectileOrigin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPendingProfessionAndProjectileReview::RunTest(const FString& Parameters)
{
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestNotNull(TEXT("The actual authored profession catalog resolves."), Catalog)) return false;
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
    if (!TestNotNull(TEXT("An isolated runtime world supports native GAS and projectile checks."), World.Get())) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };
    const FName Ids[] = {TEXT("Warrior"), TEXT("Mage"), TEXT("Archer"), TEXT("Rogue")};
    const float Strength[] = {10.f, 1.f, 5.f, 5.f};
    const float Speed[] = {5.f, 5.f, 7.f, 10.f};
    const float Intelligence[] = {1.f, 10.f, 4.f, 1.f};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ids); ++Index)
    {
        FProfessionDefinition Definition;
        FText Error;
        if (!TestTrue(TEXT("Each actual profession resolves its current combat data."), Catalog->ResolveProfession(Ids[Index], Definition, Error))) return false;
        TestTrue(TEXT("Authored Strength, Speed and Intelligence match the current profession contract."),
            Definition.Strength == Strength[Index] && Definition.Speed == Speed[Index] && Definition.Intelligence == Intelligence[Index]);
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AUnitBase* Unit = World->SpawnActor<AUnitBase>(Definition.CombatClass, FVector(Index * 2000.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("The original profession Blueprint spawns without an asset copy."), Unit)) return false;
        Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
        Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
        const bool bConfigured = Unit->ConfigureProfession(Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints,
            Definition.StartingSkills, Definition.Speed, Definition.Strength, Definition.Intelligence);
        if (!TestTrue(TEXT("Current authored stats configure the actual spawned GAS attributes."), bConfigured)) return false;
        TestTrue(TEXT("The live GAS attributes preserve all three authored stats."), Unit->GetAttributeSet()->GetStrength() == Strength[Index]
            && Unit->GetAttributeSet()->GetSpeed() == Speed[Index] && Unit->GetAttributeSet()->GetIntelligence() == Intelligence[Index]);
        if (!TestTrue(TEXT("The actual body exposes the configured projectile hand socket."), Unit->GetMesh()->DoesSocketExist(Unit->ProjectileVisualSocket))) return false;
        AEnemyUnit* Enemy = World->SpawnActor<AEnemyUnit>(Unit->GetActorLocation() + FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
        ACombatRoundProjectile* Projectile = World->SpawnActor<ACombatRoundProjectile>(Unit->GetActorLocation(), FRotator::ZeroRotator, Spawn);
        if (!Enemy || !Projectile) return false;
        Enemy->SetTeam(ETeam::Enemy);
        Enemy->GetAbilitySystemComponent()->InitAbilityActorInfo(Enemy, Enemy);
        Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(Enemy->GetAttributeSet());
        Enemy->GetAttributeSet()->InitMaxHP(100.f);
        Enemy->GetAttributeSet()->InitHP(100.f);
        Projectile->SetAllowedTargets({Enemy});
        Projectile->InitializeProjectile(Unit, Enemy, Enemy->GetActorLocation(), 500.f, 10.f, 10.f, 5.f, false, true);
        const FVector Hand = Unit->GetMesh()->GetSocketTransform(Unit->ProjectileVisualSocket).TransformPosition(Unit->ProjectileVisualSocketOffset);
        USceneComponent* Visual = nullptr;
        TInlineComponentArray<USceneComponent*> Components(Projectile);
        for (USceneComponent* Component : Components) if (Component->GetFName() == TEXT("VisualOrigin")) Visual = Component;
        TestTrue(TEXT("The real projectile visual launches at the hand while authoritative collision retains the actor origin."),
            Visual && Visual->GetComponentLocation().Equals(Hand, 0.1f) && Projectile->GetActorLocation().Equals(Unit->GetActorLocation(), 0.1f));
        AddInfo(FString::Printf(TEXT("%s: Strength=%.0f Speed=%.0f Intelligence=%.0f; hand=%s collision=%s."), *Ids[Index].ToString(),
            Definition.Strength, Definition.Speed, Definition.Intelligence, *Hand.ToString(), *Projectile->GetActorLocation().ToString()));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPendingCurrentShoulderGateReview, "ProjectA.Review.CurrentShoulderActionGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPendingCurrentShoulderGateReview::RunTest(const FString& Parameters)
{
    for (ECombatRoundActionPhase Terminal : {ECombatRoundActionPhase::Cancelled, ECombatRoundActionPhase::Complete})
    {
        FCombatRoundView View;
        View.CombatId = FGuid::NewGuid();
        View.RoundNumber = 1;
        View.Phase = ECombatRoundPhase::Planning;
        FCombatRoundUnitView& Unit = View.Units.AddDefaulted_GetRef();
        Unit.UnitId = 1;
        Unit.OwnerSlot = 1;
        Unit.HP = 100.f;
        Unit.bReady = true;
        Unit.Command.UnitId = 1;
        Unit.Command.SkillId = TEXT("ReviewAttack");
        FCombatShoulderFocus Focus;
        TestEqual(TEXT("Ready alone retains the tactical view."), Focus.Update(View, 1, 1), INDEX_NONE);
        View.Phase = ECombatRoundPhase::Resolving;
        Unit.ActionPhase = ECombatRoundActionPhase::Waiting;
        TestEqual(TEXT("An accepted action's start delay retains the tactical view."), Focus.Update(View, 1, 1), INDEX_NONE);
        Unit.ActionPhase = Terminal;
        TestEqual(TEXT("Cancelled or skipped actions cannot enter shoulder view."), Focus.Update(View, 1, 1), INDEX_NONE);
        Focus.Reset();
        Unit.ActionPhase = ECombatRoundActionPhase::Approaching;
        TestEqual(TEXT("An owned action enters shoulder view when its own approach begins."), Focus.Update(View, 1, 1), 1);
        Unit.ActionPhase = ECombatRoundActionPhase::Casting;
        TestEqual(TEXT("An owned action retains shoulder view while actually casting."), Focus.Update(View, 1, 1), 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPendingStandaloneSaveReview, "ProjectA.Review.StandaloneSavedBytes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPendingStandaloneSaveReview::RunTest(const FString& Parameters)
{
    FString Path;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ProjectAReviewSavedBytes="), Path))
    {
        AddInfo(TEXT("An explicit disposable review save is required; no user save is selected by default."));
        return true;
    }
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The explicitly selected standalone review save is readable."), FFileHelper::LoadFileToArray(Bytes, *Path))) return false;
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Standalone bytes deserialize as the current Run SaveGame class."), Save.Get())) return false;
    FText MigrationError;
    const bool bMigrated = RunContentMigration::RemoveDeletedSkills(*Save, MigrationError);
    TestTrue(*FString::Printf(TEXT("Content migration succeeds: %s"), *MigrationError.ToString()), bMigrated);
    FText PolicyError;
    const bool bPolicyValid = RunLevelDesign::Validate(Save->TargetRun, PolicyError);
    TestTrue(*FString::Printf(TEXT("Frozen PvE policy validates: %s"), *PolicyError.ToString()), bPolicyValid);
    TestEqual(TEXT("The actual standalone save retains the two-enemy minimum."), Save->TargetRun.LevelDesign.MinimumEnemyCount, 2);
    return true;
}

#endif
