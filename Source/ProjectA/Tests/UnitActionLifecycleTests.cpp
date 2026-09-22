#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/Notify/AN_SkillRelease.h"
#include "Combat/SkillActor/AttackSkillActorBase.h"
#include "Combat/Library/CombatWeaponTraceLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "DataAsset/SkillPoolDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Ability/GA_AreaAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Game/Run/RunTypes.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Profession/ProfessionBase.h"
#include "Profession/WarriorProfession.h"
#include "Profession/MageProfession.h"
#include "Profession/ArcherProfession.h"
#include "Profession/RogueProfession.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "Unit/UnitBase.h"
#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace UnitActionLifecycleTests
{
    struct FScopedWorld
    {
        UWorld* World = nullptr;

        FScopedWorld()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        }

        ~FScopedWorld()
        {
            World->DestroyWorld(false);
        }

        template <typename T>
        T* SpawnUnit(const FVector& Location)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            T* Unit = World->SpawnActor<T>(Location, FRotator::ZeroRotator, Parameters);
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            Unit->ResetActionPoint();
            Unit->ResetSubActionPoint();
            return Unit;
        }

        ACombatGridTile* SpawnTile(AUnitBase* Unit)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(Unit->GetActorLocation(), FRotator::ZeroRotator, Parameters);
            Unit->SetCurrentTile(Tile);
            return Tile;
        }
    };

    USkillDefinitionDataAsset* MakeSkill(UObject* Outer, TSubclassOf<UGameplayAbility> AbilityClass)
    {
        USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Outer);
        Skill->AbilityClass = AbilityClass;
        Skill->ActionPointCost = 1;
        Skill->TargetRule = ESkillTargetRule::EnemyUnit;
        Skill->AreaType = ESkillAreaType::Single;
        return Skill;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitRetiredActionTest, "ProjectA.Combat.Actions.RetiredExecutionIsInert", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnitRetiredActionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Target->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Unit);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_DefaultAttack::StaticClass());
    const FVector Location = Unit->GetActorLocation();
    const int32 AP = Unit->GetCurrentActionPoint();
    const int32 SubAP = Unit->GetCurrentSubActionPoint();
    const int32 Stock = Unit->HealingItemCount;
    Unit->StartSkill(Skill, TargetTile);
    Unit->StartMoveAction(TargetTile);
    Unit->StartItemAction(Unit);
    Unit->OnTurnStart();
    TestFalse(TEXT("Retired turn callback cannot activate sequential actions"), Unit->IsActiveTurn());
    TestFalse(TEXT("Retired actions cannot leave a busy lifecycle"), Unit->IsBusy());
    TestFalse(TEXT("Immediate healing entry is unavailable"), Unit->CanUseHealingItem(Unit));
    TestEqual(TEXT("Retired attack does not apply tile damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
    TestEqual(TEXT("Retired entries preserve AP"), Unit->GetCurrentActionPoint(), AP);
    TestEqual(TEXT("Retired entries preserve sub AP"), Unit->GetCurrentSubActionPoint(), SubAP);
    TestEqual(TEXT("Retired entries preserve item stock"), Unit->HealingItemCount, Stock);
    TestEqual(TEXT("Retired movement cannot relocate actors"), Unit->GetActorLocation(), Location);
    TestFalse(TEXT("Default legacy ability rejects activation"), GetDefault<UGA_DefaultAttack>()->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));
    TestFalse(TEXT("Area legacy ability rejects activation"), GetDefault<UGA_AreaAttack>()->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));

    // Prove the event observer works before verifying that cosmetic montage notifies cannot release attacks.
    // 표현용 몽타주 알림이 공격을 발동하지 못하는지 검사하기 전에 이벤트 관찰자가 동작하는지 확인합니다.
    const FGameplayTag ReleaseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Release")));
    UAbilitySystemComponent* AbilitySystem = Unit->GetAbilitySystemComponent();
    int32 ReleaseEvents = 0;
    const FDelegateHandle ReleaseHandle = AbilitySystem->GenericGameplayEventCallbacks.FindOrAdd(ReleaseTag).AddLambda([&ReleaseEvents](const FGameplayEventData*) { ++ReleaseEvents; });
    FGameplayEventData ReleasePayload;
    ReleasePayload.Instigator = Unit;
    ReleasePayload.EventTag = ReleaseTag;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Unit, ReleaseTag, ReleasePayload);
    TestEqual(TEXT("The fixture observes a directly delivered release event"), ReleaseEvents, 1);
    ReleaseEvents = 0;
    UAN_SkillRelease* ReleaseNotify = NewObject<UAN_SkillRelease>(Unit);
    ReleaseNotify->Notify(Unit->GetMesh(), nullptr);
    ReleaseNotify->Notify(Unit->GetMesh(), nullptr);
    TestEqual(TEXT("Repeated legacy montage notifies emit no release events for round units"), ReleaseEvents, 0);
    TestEqual(TEXT("Legacy montage notifies preserve source HP"), Unit->GetAttributeSet()->GetHP(), 100.0f);
    TestEqual(TEXT("Legacy montage notifies preserve target HP"), Target->GetAttributeSet()->GetHP(), 100.0f);
    TestEqual(TEXT("Legacy montage notifies preserve AP"), Unit->GetCurrentActionPoint(), AP);
    TestEqual(TEXT("Legacy montage notifies preserve sub AP"), Unit->GetCurrentSubActionPoint(), SubAP);
    AbilitySystem->GenericGameplayEventCallbacks.FindChecked(ReleaseTag).Remove(ReleaseHandle);

    AAttackSkillActorBase* LegacyActor = Scope.World->SpawnActor<AAttackSkillActorBase>();
    int32 Resolutions = 0;
    bool bSucceeded = true;
    LegacyActor->OnSkillActorResolved.AddLambda([&](ASkillActorBase*, bool bSuccess)
    {
        ++Resolutions;
        bSucceeded = bSuccess;
    });
    FSkillActorInitData Context;
    Context.SourceUnit = Unit;
    Context.SkillData = Skill;
    Context.TargetTile = TargetTile;
    LegacyActor->InitializeAttackSkillActor(Context, UGE_Damage::StaticClass(), 90.0f);
    LegacyActor->RequestImpact();
    LegacyActor->RequestFinish();
    TestEqual(TEXT("Legacy spawned attack resolves exactly once"), Resolutions, 1);
    TestFalse(TEXT("Legacy spawned attack reports rejection"), bSucceeded);
    TestEqual(TEXT("Legacy impact callback cannot apply damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
    LegacyActor->OnSkillActorResolved.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillAcquisitionTest, "ProjectA.Combat.Content.SkillAcquisitionRetainsRoundData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillAcquisitionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    USkillPoolDataAsset* Pool = NewObject<USkillPoolDataAsset>(Unit);
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_AreaAttack::StaticClass());
    FSkillPoolEntry Entry;
    Entry.Skill = Skill;
    Entry.Weight = 1;
    Pool->Entries.Add(Entry);
    TestEqual(TEXT("Pool reward equips the authored definition"), Unit->AcquireSkillFromPool(Pool), Skill);
    TestEqual(TEXT("Equipped lookup matches acquired data"), Unit->FindSkillDataByAbilityClass(Skill->AbilityClass), Skill);
    TestTrue(TEXT("Acquired data is available to round planning"), Unit->GetEquippedSkillDataAssets().Contains(Skill));
    TestNull(TEXT("Owned skill is excluded from pool"), Unit->AcquireSkillFromPool(Pool));
    TestFalse(TEXT("Duplicate direct acquisition is rejected"), Unit->AcquireAndEquipSkill(Skill));
    TestNull(TEXT("Data acquisition does not grant a retired executable ability"), Unit->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Skill->AbilityClass));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundSkillMigrationTest, "ProjectA.Combat.Content.RoundSkillMigrationValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundSkillMigrationTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    USkillDefinitionDataAsset* Skill = MakeSkill(GetTransientPackage(), UGA_DefaultAttack::StaticClass());
    FCombatRoundSkill Resolved;
    FText Error;
    TestTrue(TEXT("Supported old single enemy attack resolves"), Skill->ResolveRoundSkill(Resolved, Error));
    TestEqual(TEXT("Migration reads authored CDO damage"), Resolved.Power, GetDefault<UGA_DefaultAttack>()->GetAuthoredDamageAmount());
    TestNull(TEXT("Migration keeps an unauthored montage optional"), Resolved.CastMontage.Get());

    // Inspect saved ability defaults while changing only transient skill definitions.
    // 저장된 어빌리티 기본값을 읽으며 임시 스킬 정의만 변경합니다.
    const TPair<const TCHAR*, const TCHAR*> AuthoredMontages[] = {{TEXT("BPDA_DefaulatAttack"), TEXT("MM_Attack_01_Montage")}, {TEXT("BPDA_AreaAttack"), TEXT("MM_Attack_03_Montage")}};
    for (const TPair<const TCHAR*, const TCHAR*>& Authored : AuthoredMontages)
    {
        const FString AssetPath = FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/%s.%s"), Authored.Key, Authored.Key);
        const USkillDefinitionDataAsset* AuthoredSkill = LoadObject<USkillDefinitionDataAsset>(nullptr, *AssetPath);
        if (!TestNotNull(TEXT("The authored skill data asset exists"), AuthoredSkill)) return false;
        const UGA_AttackBase* AuthoredAttack = AuthoredSkill->AbilityClass ? Cast<UGA_AttackBase>(AuthoredSkill->AbilityClass->GetDefaultObject()) : nullptr;
        if (!TestNotNull(TEXT("The authored skill retains its legacy attack metadata"), AuthoredAttack)) return false;
        const FString MontagePath = FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/Unit/Animation/Montage/%s.%s"), Authored.Value, Authored.Value);
        UAnimMontage* AuthoredMontage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
        if (!TestNotNull(TEXT("The authored attack montage exists"), AuthoredMontage)) return false;
        TestEqual(TEXT("Saved attack defaults reference the expected montage"), AuthoredAttack->GetAuthoredAttackMontage(), AuthoredMontage);
        if (!TestTrue(TEXT("The authored skill resolves without activating its ability"), AuthoredSkill->ResolveRoundSkill(Resolved, Error))) return false;
        TestEqual(TEXT("Migration retains the authored attack montage"), Resolved.CastMontage.Get(), AuthoredSkill->bUseRoundDefinition && AuthoredSkill->RoundDefinition.CastMontage ? AuthoredSkill->RoundDefinition.CastMontage.Get() : AuthoredMontage);
        if (FString(Authored.Key) == TEXT("BPDA_AreaAttack"))
        {
            TestTrue(TEXT("The saved area attack retains its legacy enemy tile area definition"), !AuthoredSkill->bUseRoundDefinition && AuthoredSkill->TargetRule == ESkillTargetRule::EnemyTile && AuthoredSkill->AreaType == ESkillAreaType::AroundTarget);
            TestEqual(TEXT("The saved area attack uses its authored damage of 200"), Resolved.Power, 200.f);
            TestEqual(TEXT("The saved area attack resolves as a ground attack"), Resolved.Kind, ECombatRoundSkillKind::GroundAttack);
            TestEqual(TEXT("The saved area attack casts without approaching"), Resolved.Approach, ECombatRoundApproach::None);
            TestEqual(TEXT("The saved area attack retains its selected point"), Resolved.TargetLoss, ECombatRoundTargetLoss::KeepLocation);
            TestEqual(TEXT("The saved area attack converts its one-tile radius to 200 cm"), Resolved.HitRange, 200.f);
        }

        USkillDefinitionDataAsset* PresentationSkill = MakeSkill(GetTransientPackage(), AuthoredSkill->AbilityClass);
        PresentationSkill->bUseRoundDefinition = true;
        TestTrue(TEXT("An explicit profile can reuse the legacy montage"), PresentationSkill->ResolveRoundSkill(Resolved, Error));
        TestEqual(TEXT("An empty explicit montage falls back to the authored attack"), Resolved.CastMontage.Get(), AuthoredMontage);
        UAnimMontage* OverrideMontage = NewObject<UAnimMontage>(PresentationSkill);
        PresentationSkill->RoundDefinition.CastMontage = OverrideMontage;
        TestTrue(TEXT("An explicit montage override resolves"), PresentationSkill->ResolveRoundSkill(Resolved, Error));
        TestEqual(TEXT("The explicit montage takes precedence over the legacy montage"), Resolved.CastMontage.Get(), OverrideMontage);
        PresentationSkill->AbilityClass = nullptr;
        TestTrue(TEXT("An explicit montage needs no ability class"), PresentationSkill->ResolveRoundSkill(Resolved, Error));
        TestEqual(TEXT("The explicit montage remains active without a legacy class"), Resolved.CastMontage.Get(), OverrideMontage);
        PresentationSkill->RoundDefinition.CastMontage = nullptr;
        TestTrue(TEXT("A fully authored profile needs no montage"), PresentationSkill->ResolveRoundSkill(Resolved, Error));
        TestNull(TEXT("An omitted optional montage remains empty"), Resolved.CastMontage.Get());
        PresentationSkill->AbilityClass = UGameplayAbility::StaticClass();
        TestTrue(TEXT("An unrelated legacy ability does not invalidate an explicit profile"), PresentationSkill->ResolveRoundSkill(Resolved, Error));
        TestNull(TEXT("An unrelated legacy ability provides no attack montage"), Resolved.CastMontage.Get());
    }
    Skill->AreaType = ESkillAreaType::TargetAndSides;
    Skill->bMoveToTarget = true;
    TestTrue(TEXT("Target and adjacent sides migrate as an approaching melee attack"), Skill->ResolveRoundSkill(Resolved, Error));
    TestTrue(TEXT("Migration retains the side pattern and melee approach"), Resolved.Kind == ECombatRoundSkillKind::Melee && Resolved.MeleeArea == ESkillAreaType::TargetAndSides && Resolved.Approach == ECombatRoundApproach::Unit);
    Skill->bMoveToTarget = false;
    TestFalse(TEXT("A stationary legacy side attack requires an explicit profile"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->AreaType = ESkillAreaType::AroundSelf;
    TestFalse(TEXT("Legacy self-centered area requires explicit semantics"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->AreaType = ESkillAreaType::AroundTarget;
    Skill->AreaRadius = 2;
    TestTrue(TEXT("Legacy enemy unit areas remain supported"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->TargetRule = ESkillTargetRule::EnemyTile;
    TestTrue(TEXT("Legacy enemy tile areas resolve without an explicit profile"), Skill->ResolveRoundSkill(Resolved, Error));
    TestEqual(TEXT("Enemy tile areas use ground collision"), Resolved.Kind, ECombatRoundSkillKind::GroundAttack);
    TestEqual(TEXT("Stationary enemy tile areas do not approach"), Resolved.Approach, ECombatRoundApproach::None);
    TestEqual(TEXT("Enemy tile areas retain their selected location"), Resolved.TargetLoss, ECombatRoundTargetLoss::KeepLocation);
    TestEqual(TEXT("Enemy tile area radius uses the existing conversion"), Resolved.HitRange, 400.f);
    Skill->bMoveToTarget = true;
    TestTrue(TEXT("Moving enemy tile areas resolve"), Skill->ResolveRoundSkill(Resolved, Error));
    TestEqual(TEXT("Moving enemy tile areas approach a tile instead of tracking a unit"), Resolved.Approach, ECombatRoundApproach::Tile);
    Skill->bMoveToTarget = false;
    Skill->AreaRadius = -1;
    TestFalse(TEXT("Enemy tile areas reject a negative radius"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->AreaRadius = 2;
    Skill->ActionPointCost = 0;
    TestFalse(TEXT("Enemy tile areas reject zero legacy AP cost"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->ActionPointCost = 1;
    for (ESkillAreaType Area : {ESkillAreaType::Single, ESkillAreaType::Row, ESkillAreaType::AroundSelf})
    {
        Skill->AreaType = Area;
        TestFalse(TEXT("Other enemy tile area combinations still require explicit semantics"), Skill->ResolveRoundSkill(Resolved, Error));
        TestTrue(TEXT("Unsupported enemy tile combinations report the required RoundDefinition"), Error.ToString().Contains(TEXT("RoundDefinition")));
    }
    Skill->AreaType = ESkillAreaType::AroundTarget;
    for (ESkillTargetRule Target : {ESkillTargetRule::AllyTile, ESkillTargetRule::AnyTile})
    {
        Skill->TargetRule = Target;
        TestFalse(TEXT("Other tile target rules still require explicit semantics"), Skill->ResolveRoundSkill(Resolved, Error));
    }
    Skill->bUseRoundDefinition = true;
    Skill->AbilityClass = nullptr;
    TestTrue(TEXT("Explicit round definitions do not require an ability class"), Skill->ResolveRoundSkill(Resolved, Error));
    TestFalse(TEXT("Resolved explicit definition has a stable identifier"), Resolved.SkillId.IsNone());
    Skill->RoundDefinition.Kind = ECombatRoundSkillKind::Wait;
    Skill->RoundDefinition.Approach = ECombatRoundApproach::Tile;
    TestFalse(TEXT("Wait cannot silently move to a tile"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->RoundDefinition.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill->RoundDefinition.Approach = ECombatRoundApproach::Unit;
    TestFalse(TEXT("Ground attack cannot silently track a unit"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->RoundDefinition = FCombatRoundSkill();
    Skill->RoundDefinition.Power = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite damage is rejected before simulation"), Skill->ResolveRoundSkill(Resolved, Error));
    TestFalse(TEXT("Invalid definition reports the asset error"), Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundRetargetPolicyTest, "ProjectA.Combat.Content.UnitTargetRetargetPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundRetargetPolicyTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    USkillDefinitionDataAsset* Skill = MakeSkill(GetTransientPackage(), UGA_DefaultAttack::StaticClass());
    FCombatRoundSkill Resolved;
    FText Error;
    for (bool bApproaching : {false, true})
    {
        Skill->bMoveToTarget = bApproaching;
        if (!TestTrue(TEXT("A legacy unit-targeted attack resolves"), Skill->ResolveRoundSkill(Resolved, Error))) return false;
        TestEqual(TEXT("Both legacy melee and projectile attacks use nearest-enemy fallback before release"), Resolved.TargetLoss, ECombatRoundTargetLoss::NearestEnemy);
    }
    Skill->bUseRoundDefinition = true;
    Skill->AbilityClass = nullptr;
    for (ECombatRoundSkillKind Kind : {ECombatRoundSkillKind::Melee, ECombatRoundSkillKind::Projectile, ECombatRoundSkillKind::GroundAttack, ECombatRoundSkillKind::Wait})
    {
        for (ECombatRoundTargetLoss Policy : {ECombatRoundTargetLoss::Cancel, ECombatRoundTargetLoss::KeepLocation, ECombatRoundTargetLoss::NearestEnemy})
        {
            Skill->RoundDefinition = FCombatRoundSkill();
            Skill->RoundDefinition.Kind = Kind;
            Skill->RoundDefinition.Approach = Kind == ECombatRoundSkillKind::Melee ? ECombatRoundApproach::Unit : ECombatRoundApproach::None;
            Skill->RoundDefinition.TargetLoss = Policy;
            if (!TestTrue(TEXT("A valid explicit target-loss profile resolves"), Skill->ResolveRoundSkill(Resolved, Error))) return false;
            const ECombatRoundTargetLoss Expected = Kind == ECombatRoundSkillKind::Melee || Kind == ECombatRoundSkillKind::Projectile ? ECombatRoundTargetLoss::NearestEnemy : Policy;
            TestEqual(TEXT("Unit attacks retarget while ground and wait policies remain authored"), Resolved.TargetLoss, Expected);
            TestEqual(TEXT("Resolving does not mutate the saved target-loss field"), Skill->RoundDefinition.TargetLoss, Policy);
        }
    }
    Skill->RoundDefinition = FCombatRoundSkill();
    Skill->RoundDefinition.TargetLoss = static_cast<ECombatRoundTargetLoss>(255);
    TestFalse(TEXT("An invalid serialized target-loss enum is rejected before normalization"), Skill->ResolveRoundSkill(Resolved, Error));
    TestFalse(TEXT("Invalid target-loss data reports the asset error"), Error.IsEmpty());

    for (const TCHAR* AssetName : {TEXT("BPDA_DefaulatAttack"), TEXT("BPDA_RangedAttack"), TEXT("BPDA_SweepingStrike"), TEXT("BPDA_swoard_attack"), TEXT("BPDA_AreaAttack")})
    {
        const FString AssetPath = FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/%s.%s"), AssetName, AssetName);
        const USkillDefinitionDataAsset* Authored = LoadObject<USkillDefinitionDataAsset>(nullptr, *AssetPath);
        if (!TestNotNull(AssetPath + TEXT(" exists"), Authored)) return false;
        const ECombatRoundTargetLoss SavedPolicy = Authored->RoundDefinition.TargetLoss;
        if (!TestTrue(AssetPath + TEXT(" resolves"), Authored->ResolveRoundSkill(Resolved, Error))) return false;
        if (FString(AssetName) == TEXT("BPDA_AreaAttack"))
        {
            TestEqual(TEXT("The authored area attack remains a fixed ground attack"), Resolved.Kind, ECombatRoundSkillKind::GroundAttack);
            TestEqual(TEXT("The authored area attack preserves its selected location"), Resolved.TargetLoss, ECombatRoundTargetLoss::KeepLocation);
        }
        else
        {
            TestTrue(AssetPath + TEXT(" remains a unit-targeted attack"), Resolved.Kind == ECombatRoundSkillKind::Melee || Resolved.Kind == ECombatRoundSkillKind::Projectile);
            TestEqual(AssetPath + TEXT(" adopts the common retarget policy"), Resolved.TargetLoss, ECombatRoundTargetLoss::NearestEnemy);
        }
        TestEqual(AssetPath + TEXT(" keeps its authored profile unchanged"), Authored->RoundDefinition.TargetLoss, SavedPolicy);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfessionLoadoutTest, "ProjectA.Party.ProfessionLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProfessionLoadoutTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestNotNull(TEXT("Shared party catalog exists"), Catalog))
    {
        return false;
    }
    const TArray<FName> ExpectedIds = {TEXT("Warrior"), TEXT("Mage"), TEXT("Archer"), TEXT("Rogue")};
    const TArray<TSubclassOf<UProfessionBase>> ExpectedClasses = {UWarriorProfession::StaticClass(), UMageProfession::StaticClass(), UArcherProfession::StaticClass(), URogueProfession::StaticClass()};
    const TArray<TSubclassOf<UProfessionBase>> PlayableClasses = UProfessionBase::GetPlayableClasses();
    if (!TestTrue(TEXT("The playable catalog contains exactly the four ordered native professions"), PlayableClasses == ExpectedClasses && UProfessionBase::GetPlayableIds() == ExpectedIds && Catalog->Professions.Num() == 4)) return false;
    FScopedWorld Scope;
    for (int32 Index = 0; Index < ExpectedIds.Num(); ++Index)
    {
        const FName Id = ExpectedIds[Index];
        const UProfessionBase* Profession = UProfessionBase::FindProfession(Id);
        if (!TestNotNull(TEXT("Each profession ID resolves a class default definition"), Profession)) return false;
        TestTrue(TEXT("Each profession is a concrete native UObject child instead of a combat actor"), Profession->GetClass() == ExpectedClasses[Index].Get() && Profession->GetClass()->HasAnyClassFlags(CLASS_Native) && !Profession->GetClass()->HasAnyClassFlags(CLASS_Abstract) && !Profession->GetClass()->IsChildOf(AActor::StaticClass()));
        TestTrue(TEXT("Every profession starts with HP 100 and three attributes at 10"), Profession->MaxHP == 100.0f && Profession->Strength == 10.0f && Profession->Dexterity == 10.0f && Profession->Intelligence == 10.0f);
        FProfessionDefinition Definition;
        if (!TestTrue(TEXT("Profession resolves combat defaults"), Catalog->ResolveProfession(Id, Definition)))
        {
            return false;
        }
        TestFalse(TEXT("Profession has a readable name"), Definition.DisplayName.IsEmpty());
        TestFalse(TEXT("Profession has a description"), Definition.Description.IsEmpty());
        TestTrue(TEXT("The authored catalog uses the matching native profession child"), Definition.ProfessionClass == ExpectedClasses[Index]);
        TestTrue(TEXT("Resolved starting attributes match the profession class"), Definition.MaxHP == 100.0f && Definition.Strength == 10.0f && Definition.Dexterity == 10.0f && Definition.Intelligence == 10.0f);
        TestTrue(TEXT("Preview shows all actual starting attributes"), Catalog->GetProfessionDetails(Id).ToString().Contains(TEXT("HP 100 · 힘 10 · 민첩 10 · 지능 10")));
        APlayerUnit* Unit = Scope.SpawnUnit<APlayerUnit>(FVector::ZeroVector);
        if (!TestTrue(TEXT("Spawn configuration accepts the resolved profession"), Unit->ConfigureProfession(Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, Definition.StartingSkills, Definition.Strength, Definition.Dexterity, Definition.Intelligence))) return false;
        TestEqual(TEXT("Spawn HP equals preview"), Unit->GetAttributeSet()->GetHP(), Definition.MaxHP);
        TestEqual(TEXT("Spawn strength equals preview"), Unit->GetAttributeSet()->GetStrength(), Definition.Strength);
        TestEqual(TEXT("Spawn dexterity equals preview"), Unit->GetAttributeSet()->GetDexterity(), Definition.Dexterity);
        TestEqual(TEXT("Spawn intelligence equals preview"), Unit->GetAttributeSet()->GetIntelligence(), Definition.Intelligence);
        TestEqual(TEXT("Spawn AP equals preview"), Unit->GetMaxActionPoint(), Definition.ActionPoints);
        TestEqual(TEXT("Spawn sub AP equals preview"), Unit->GetMaxSubActionPoint(), Definition.SubActionPoints);
        for (USkillDefinitionDataAsset* Skill : Definition.StartingSkills)
        {
            TestTrue(TEXT("Every displayed skill remains equipped as round data"), Unit->GetEquippedSkillDataAssets().Contains(Skill));
            FCombatRoundSkill RoundSkill;
            FText Error;
            TestTrue(TEXT("Every displayed skill has a valid round definition"), Skill->ResolveRoundSkill(RoundSkill, Error));
        }
    }
    UPartyDefinitionDataAsset* Custom = DuplicateObject<UPartyDefinitionDataAsset>(Catalog, GetTransientPackage());
    FProfessionDefinition& Override = Custom->Professions.FindChecked(TEXT("Mage"));
    FProfessionDefinition Base;
    Catalog->ResolveProfession(TEXT("Mage"), Base);
    AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector::ZeroVector);
    TestTrue(TEXT("The native enemy keeps its HP 150 and AP 2 defaults"), Enemy->GetInitialMaxHP() == 150.0f && Enemy->GetMaxActionPoint() == 2);
    TestTrue(TEXT("The native enemy starts with strength dexterity and intelligence at five"), Enemy->GetAttributeSet()->GetStrength() == 5.0f && Enemy->GetAttributeSet()->GetDexterity() == 5.0f && Enemy->GetAttributeSet()->GetIntelligence() == 5.0f);
    TestEqual(TEXT("Native enemy combat speed follows its default dexterity"), Enemy->GetCombatSpeed(), 5.0f);
    if (!TestTrue(TEXT("Snapshot configuration replaces the native enemy defaults"), Enemy->ConfigureProfession(150.0f, 2, 1, Base.StartingSkills, 13.0f, 17.0f, 19.0f))) return false;
    TestTrue(TEXT("Snapshot enemy attributes preserve their supplied values"), Enemy->GetAttributeSet()->GetStrength() == 13.0f && Enemy->GetAttributeSet()->GetDexterity() == 17.0f && Enemy->GetAttributeSet()->GetIntelligence() == 19.0f);
    TestEqual(TEXT("Snapshot enemy combat speed follows its supplied dexterity"), Enemy->GetCombatSpeed(), 17.0f);
    Override.bUseUnitClassDefaults = false;
    Override.StartingSkills = Base.StartingSkills;
    Override.MaxHP = 137.0f;
    Override.Strength = 12.0f;
    Override.Dexterity = 0.0f;
    Override.Intelligence = 24.0f;
    Override.ActionPoints = 3;
    Override.SubActionPoints = 2;
    FProfessionDefinition Resolved;
    if (!TestTrue(TEXT("Explicit profession tuning resolves including a zero attribute"), Custom->ResolveProfession(TEXT("Mage"), Resolved))) return false;
    APlayerUnit* Tuned = Scope.SpawnUnit<APlayerUnit>(FVector::ZeroVector);
    TestTrue(TEXT("Spawn accepts explicit profession tuning"), Tuned->ConfigureProfession(Resolved.MaxHP, Resolved.ActionPoints, Resolved.SubActionPoints, Resolved.StartingSkills, Resolved.Strength, Resolved.Dexterity, Resolved.Intelligence));
    TestEqual(TEXT("Override applies HP"), Tuned->GetAttributeSet()->GetHP(), 137.0f);
    TestEqual(TEXT("Override applies AP"), Tuned->GetMaxActionPoint(), 3);
    TestTrue(TEXT("Explicit attributes reach GAS"), Tuned->GetAttributeSet()->GetStrength() == 12.0f && Tuned->GetAttributeSet()->GetDexterity() == 0.0f && Tuned->GetAttributeSet()->GetIntelligence() == 24.0f);
    TestTrue(TEXT("Override also changes every preview attribute"), Custom->GetProfessionDetails(TEXT("Mage")).ToString().Contains(TEXT("HP 137 · 힘 12 · 민첩 0 · 지능 24")));
    for (float InvalidValue : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0f, 1000001.0f})
    {
        for (int32 AttributeIndex = 0; AttributeIndex < 3; ++AttributeIndex)
        {
            const float Strength = AttributeIndex == 0 ? InvalidValue : 12.0f;
            const float Dexterity = AttributeIndex == 1 ? InvalidValue : 0.0f;
            const float Intelligence = AttributeIndex == 2 ? InvalidValue : 24.0f;
            TestFalse(TEXT("Invalid runtime attributes reject the entire new loadout"), Tuned->ConfigureProfession(999.0f, 1, 0, Resolved.StartingSkills, Strength, Dexterity, Intelligence));
            TestTrue(TEXT("Rejected configuration preserves HP AP skills and all attributes"), Tuned->GetAttributeSet()->GetHP() == 137.0f && Tuned->GetMaxActionPoint() == 3 && Tuned->GetMaxSubActionPoint() == 2 && Tuned->GetEquippedSkillDataAssets() == Resolved.StartingSkills && Tuned->GetAttributeSet()->GetStrength() == 12.0f && Tuned->GetAttributeSet()->GetDexterity() == 0.0f && Tuned->GetAttributeSet()->GetIntelligence() == 24.0f);
        }
    }
    const TObjectPtr<USkillDefinitionDataAsset> DuplicateSkill = Override.StartingSkills[0];
    Override.StartingSkills.Add(DuplicateSkill);
    TestFalse(TEXT("Duplicate skill asset is rejected"), Custom->ResolveProfession(TEXT("Mage"), Resolved));
    Override.StartingSkills.Pop();
    Override.MaxHP = -1.0f;
    TestFalse(TEXT("Invalid tuning is rejected"), Custom->ResolveProfession(TEXT("Mage"), Resolved));
    for (FName PreviousId : {FName(TEXT("Hunter")), FName(TEXT("StableHand"))})
    {
        TestNull(TEXT("Previous test IDs are not in the playable profession hierarchy"), UProfessionBase::FindProfession(PreviousId));
        TestFalse(TEXT("Previous test professions do not silently use the combat fallback"), Custom->ResolveProfession(PreviousId, Resolved));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunStartingSkillLoadoutTest, "ProjectA.Party.RunStartingSkillLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunStartingSkillLoadoutTest::RunTest(const FString& Parameters)
{
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    USkillDefinitionDataAsset* Unarmed = LoadObject<USkillDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack.BPDA_DefaulatAttack"));
    USkillDefinitionDataAsset* Purchased = LoadObject<USkillDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_RangedAttack.BPDA_RangedAttack"));
    if (!TestTrue(TEXT("Authored catalog, unarmed skill and purchasable skill exist"), Catalog && Unarmed && Purchased)) return false;
    FText Error;
    for (FName ClassId : UProfessionBase::GetPlayableIds())
    {
        TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
        if (!TestTrue(TEXT("Every new profession resolves the same unarmed-only loadout"), Catalog->ResolveStartingSkills(ClassId, Skills, Error) && Skills == TArray<TObjectPtr<USkillDefinitionDataAsset>>{Unarmed})) return false;
        TestFalse(TEXT("New-character preview excludes the purchasable ranged skill"), Catalog->GetProfessionDetails(ClassId).ToString().Contains(Purchased->SkillName.ToString()));
        FRunPartyMember Member;
        Member.ClassId = ClassId;
        FProfessionDefinition Legacy;
        if (!TestTrue(TEXT("An older member preserves historical profession skills"), Catalog->ResolveProfession(ClassId, Legacy, Error) && Catalog->ResolveMemberSkills(Member, Skills, Error) && Skills == Legacy.StartingSkills)) return false;
        Member.bHasSkillLoadout = true;
        Member.Skills = {FSoftObjectPath(Unarmed), FSoftObjectPath(Purchased)};
        TestTrue(TEXT("An explicit saved loadout preserves bought skills and order"), Catalog->ResolveMemberSkills(Member, Skills, Error) && Skills == TArray<TObjectPtr<USkillDefinitionDataAsset>>{Unarmed, Purchased});
        Member.Skills.Add(FSoftObjectPath(Unarmed));
        TestFalse(TEXT("Duplicate saved skills are rejected atomically"), Catalog->ResolveMemberSkills(Member, Skills, Error));
        TestTrue(TEXT("Rejected skills do not expose a partial loadout"), Skills.IsEmpty());
        Member.Skills = {FSoftObjectPath()};
        TestFalse(TEXT("A missing saved skill is not replaced by a free starting skill"), Catalog->ResolveMemberSkills(Member, Skills, Error));
        Member.Skills.Reset();
        TestFalse(TEXT("An explicitly empty saved loadout is rejected"), Catalog->ResolveMemberSkills(Member, Skills, Error));
        Member.Skills = {FSoftObjectPath(Unarmed)};
        Member.bHasSkillLoadout = false;
        TestFalse(TEXT("Legacy data cannot carry an unmarked explicit skill list"), Catalog->ResolveMemberSkills(Member, Skills, Error));
    }
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
    TestTrue(TEXT("Pure Run initialization can resolve unarmed without a combat class catalog"), GetDefault<UPartyDefinitionDataAsset>()->ResolveStartingSkills(TEXT("Warrior"), Skills, Error) && Skills == TArray<TObjectPtr<USkillDefinitionDataAsset>>{Unarmed});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopSwordPresentationTest, "ProjectA.Party.ShopSwordPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopSwordPresentationTest::RunTest(const FString& Parameters)
{
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    USkillDefinitionDataAsset* Sword = LoadObject<USkillDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack"));
    FCombatRoundSkill SwordDefinition;
    FText Error;
    if (!TestTrue(TEXT("The purchasable sword skill resolves"), Catalog && Sword && Sword->ResolveRoundSkill(SwordDefinition, Error))) return false;
    UnitActionLifecycleTests::FScopedWorld Scope;
    for (FName ClassId : UProfessionBase::GetPlayableIds())
    {
        FProfessionDefinition Profession;
        TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
        if (!TestTrue(TEXT("Every profession resolves its combat class and unarmed start"), Catalog->ResolveProfession(ClassId, Profession, Error) && Catalog->ResolveStartingSkills(ClassId, Skills, Error))) return false;
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        APlayerUnit* Unit = Scope.World->SpawnActor<APlayerUnit>(Profession.CombatClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
        if (!TestNotNull(TEXT("Every profession spawns its authored combat class"), Unit)) return false;
        Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
        Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Unit->GetClass(), SwordDefinition.WeaponComponentName);
        UStaticMeshComponent* Weapon = Property ? Cast<UStaticMeshComponent>(Property->GetObjectPropertyValue_InContainer(Unit)) : nullptr;
        if (!TestNotNull(TEXT("Every profession has the authored sword geometry"), Weapon)) return false;
        TestTrue(TEXT("Unarmed-only setup hides the sword"), Unit->ConfigureProfession(100.f, 2, 1, Skills) && !Weapon->IsVisible());
        Skills.Add(Sword);
        TestTrue(TEXT("Purchasing sword reveals its geometry for any profession"), Unit->ConfigureProfession(100.f, 2, 1, Skills) && Weapon->IsVisible());
        CombatWeaponTrace::FBladePose Pose;
        TestTrue(TEXT("Any profession can sample its skeleton-compatible sword montage"), CombatWeaponTrace::SampleBlade(Unit, SwordDefinition, Unit->ResolveRoundCastMontage(SwordDefinition.CastMontage), SwordDefinition.WindupSeconds, Pose));
        Skills.Pop();
        TestTrue(TEXT("Restoring an unarmed-only loadout hides a previously visible sword"), Unit->ConfigureProfession(100.f, 2, 1, Skills) && !Weapon->IsVisible());
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfessionDataValidationTest, "ProjectA.Party.ProfessionDataValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProfessionDataValidationTest::RunTest(const FString& Parameters)
{
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestNotNull(TEXT("Shared party catalog exists"), Catalog)) return false;
    FDataValidationContext ExistingContext;
    TestTrue(TEXT("Existing catalog passes editor validation"), Catalog->IsDataValid(ExistingContext) == EDataValidationResult::Valid);
    FProfessionDefinition Base;
    if (!TestTrue(TEXT("Existing Archer provides a valid loadout"), Catalog->ResolveProfession(TEXT("Archer"), Base))) return false;

    UPartyDefinitionDataAsset* Custom = NewObject<UPartyDefinitionDataAsset>();
    Custom->Professions.Reset();
    FProfessionDefinition& Definition = Custom->Professions.Add(TEXT("Archer"), Base);
    Definition.bUseUnitClassDefaults = false;
    Definition.CombatClass = nullptr;
    Custom->PlayerUnitClasses.Add(TEXT("Archer"), Base.CombatClass);
    const auto Validate = [this, Custom](const TCHAR* Label, bool bExpected, const TCHAR* ExpectedDetail = nullptr)
    {
        FDataValidationContext Context;
        const bool bValid = Custom->IsDataValid(Context) == EDataValidationResult::Valid;
        TestEqual(Label, bValid, bExpected);
        TestEqual(TEXT("Validation result agrees with error presence"), Context.GetNumErrors() == 0, bExpected);
        if (ExpectedDetail)
        {
            TestTrue(TEXT("Validation identifies the failed field or asset"), Context.GetIssues().ContainsByPredicate([ExpectedDetail](const FDataValidationContext::FIssue& Issue) { return Issue.Message.ToString().Contains(ExpectedDetail); }));
        }
    };
    Validate(TEXT("Legacy per-class mapping remains valid without a fallback"), true);
    Custom->PlayerUnitClasses.Reset();
    Custom->FallbackPlayerUnitClass = Base.CombatClass;
    Validate(TEXT("Legacy shared fallback remains valid"), true);
    Custom->FallbackPlayerUnitClass = nullptr;
    Validate(TEXT("Missing all class sources reports the profession"), false, TEXT("Archer"));
    Definition.CombatClass = Base.CombatClass;
    Validate(TEXT("Direct profession class needs no fallback"), true);
    Definition.ProfessionClass = nullptr;
    Validate(TEXT("A missing profession definition class is rejected"), false, TEXT("ProfessionClass"));
    Definition.ProfessionClass = UProfessionBase::StaticClass();
    Validate(TEXT("An abstract profession definition class is rejected"), false, TEXT("ProfessionClass"));
    Definition.ProfessionClass = UWarriorProfession::StaticClass();
    Validate(TEXT("A profession definition class must match the catalog ID"), false, TEXT("ClassId"));
    Definition.ProfessionClass = Base.ProfessionClass;
    Validate(TEXT("Restoring the matching profession class restores validity"), true);
    // Use transient class metadata without changing production classes or spawning an invalid actor.
    // 실제 클래스를 바꾸거나 잘못된 액터를 스폰하지 않고 임시 클래스 메타데이터를 사용합니다.
    UClass* UnspawnableClass = NewObject<UClass>();
    UnspawnableClass->SetSuperStruct(APlayerUnit::StaticClass());
    for (EClassFlags Flag : {CLASS_Abstract, CLASS_Deprecated})
    {
        UnspawnableClass->ClassFlags = Flag;
        Definition.CombatClass = UnspawnableClass;
        Custom->PlayerUnitClasses.Add(TEXT("Archer"), Base.CombatClass);
        Custom->FallbackPlayerUnitClass = Base.CombatClass;
        Validate(TEXT("An unspawnable direct class does not silently fall back"), false, *UnspawnableClass->GetPathName());
        FProfessionDefinition Rejected;
        FText ClassError;
        TestFalse(TEXT("Runtime profession resolution also rejects an unspawnable class"), Custom->ResolveProfession(TEXT("Archer"), Rejected, ClassError));
        Definition.CombatClass = nullptr;
        Custom->PlayerUnitClasses[TEXT("Archer")] = UnspawnableClass;
        Validate(TEXT("An unspawnable legacy mapping does not silently use shared fallback"), false, TEXT("CombatClass"));
        Custom->PlayerUnitClasses.Reset();
        Custom->FallbackPlayerUnitClass = UnspawnableClass;
        Validate(TEXT("An unspawnable shared fallback is rejected"), false, TEXT("CombatClass"));
        Custom->PlayerUnitClasses.Add(TEXT("Archer"), Base.CombatClass);
        Validate(TEXT("A valid legacy mapping takes precedence over an unused invalid fallback"), true);
        Custom->PlayerUnitClasses[TEXT("Archer")] = UnspawnableClass;
        Definition.CombatClass = Base.CombatClass;
        Validate(TEXT("A valid direct class takes precedence over unused invalid fallback sources"), true);
        Custom->PlayerUnitClasses.Reset();
        Custom->FallbackPlayerUnitClass = nullptr;
    }
    Definition.MaxHP = std::numeric_limits<float>::quiet_NaN();
    Validate(TEXT("Nonfinite explicit HP is rejected"), false, TEXT("MaxHP"));
    Definition.MaxHP = Base.MaxHP;
    for (float InvalidValue : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0f, 1000001.0f})
    {
        for (float* Attribute : {&Definition.Strength, &Definition.Dexterity, &Definition.Intelligence})
        {
            const float PreviousValue = *Attribute;
            *Attribute = InvalidValue;
            Validate(TEXT("Each explicit profession attribute must be finite and within range"), false, TEXT("Strength"));
            *Attribute = PreviousValue;
        }
    }
    Definition.Strength = 0.0f;
    Definition.Dexterity = 1000000.0f;
    Validate(TEXT("Attribute range boundaries are inclusive"), true);
    Definition.Strength = Base.Strength;
    Definition.Dexterity = Base.Dexterity;
    Definition.ActionPoints = 0;
    Validate(TEXT("Nonpositive explicit AP is rejected"), false, TEXT("ActionPoints"));
    Definition.ActionPoints = Base.ActionPoints;
    Definition.SubActionPoints = -1;
    Validate(TEXT("Negative explicit sub AP is rejected"), false, TEXT("SubActionPoints"));
    Definition.SubActionPoints = Base.SubActionPoints;
    const TObjectPtr<USkillDefinitionDataAsset> DuplicateSkill = Definition.StartingSkills[0];
    Definition.StartingSkills.Add(DuplicateSkill);
    Validate(TEXT("Duplicate starting skill IDs are rejected"), false, *Base.StartingSkills[0]->GetPrimaryAssetId().ToString());
    Definition.StartingSkills.Pop();
    Definition.StartingSkills[0] = nullptr;
    Validate(TEXT("Missing skill references identify the array entry"), false, TEXT("StartingSkills[0]"));
    Definition.StartingSkills.Reset();
    Validate(TEXT("An empty explicit loadout is rejected"), false);
    USkillDefinitionDataAsset* InvalidSkill = UnitActionLifecycleTests::MakeSkill(Custom, UGA_DefaultAttack::StaticClass());
    InvalidSkill->bUseRoundDefinition = true;
    InvalidSkill->RoundDefinition.WindupSeconds = -1.0f;
    Definition.StartingSkills.Add(InvalidSkill);
    Validate(TEXT("Invalid nested round profiles identify their skill asset"), false, *InvalidSkill->GetPathName());
    Definition.StartingSkills = Base.StartingSkills;

    // Compare against the same class defaults without treating unused authored overrides as active data.
    // 사용하지 않는 명시 설정을 실제 데이터로 취급하지 않고 동일한 클래스 기본값과 비교합니다.
    Definition.bUseUnitClassDefaults = true;
    FProfessionDefinition Resolved;
    const bool bClassDefaultsValid = Custom->ResolveProfession(TEXT("Archer"), Resolved);
    Definition.MaxHP = std::numeric_limits<float>::quiet_NaN();
    Definition.Strength = std::numeric_limits<float>::quiet_NaN();
    Definition.Dexterity = -1.0f;
    Definition.Intelligence = 1000001.0f;
    Definition.ActionPoints = 0;
    Definition.SubActionPoints = -1;
    Definition.StartingSkills.Reset();
    Validate(TEXT("Unused explicit overrides preserve class-default validation"), bClassDefaultsValid);
    Definition = Base;
    Definition.bUseUnitClassDefaults = false;
    Validate(TEXT("Restored explicit loadout passes again"), true);
    FText Error = FText::FromString(TEXT("stale error"));
    TestTrue(TEXT("Diagnostic resolver accepts a valid restored loadout"), Custom->ResolveProfession(TEXT("Archer"), Resolved, Error));
    TestTrue(TEXT("Successful resolution clears old diagnostics"), Error.IsEmpty());
    Custom->Professions.Add(TEXT("Hunter"), Base);
    Validate(TEXT("A previous test profession remains invalid even with a copied valid loadout"), false, TEXT("Hunter"));
    Custom->Professions.Remove(TEXT("Hunter"));
    Custom->Professions.Add(NAME_None, Base);
    Validate(TEXT("None profession IDs cannot be selected by a run"), false, TEXT("None"));
    Custom->Professions.Reset();
    Validate(TEXT("A catalog without professions is rejected"), false);
    return true;
}
#endif

#endif
