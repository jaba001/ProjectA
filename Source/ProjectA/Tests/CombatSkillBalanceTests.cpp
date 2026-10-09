#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunSkillBalance.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "GAS/CombatGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace CombatSkillBalanceTests
{
    struct FFixture
    {
        TStrongObjectPtr<UPackage> Package;
        TStrongObjectPtr<USkillDefinitionDataAsset> Definition;
        TStrongObjectPtr<UOpponentSnapshotCatalogDataAsset> Opponents;
        FRunWeaponSkillRulesState Rules;
        TArray<FRunPartyMember> Party;

        FFixture()
        {
            Package.Reset(CreatePackage(*(TEXT("/Game/User_JeHoon/Validation/T12/CombatSkillBalance_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            Package->SetFlags(RF_Transient);
            Definition.Reset(NewObject<USkillDefinitionDataAsset>(Package.Get(), TEXT("FrozenProjectile"), RF_Transient));
            Definition->bUseRoundDefinition = true;
            Definition->RoundDefinition.Kind = ECombatRoundSkillKind::Projectile;
            Definition->RoundDefinition.Approach = ECombatRoundApproach::None;
            Definition->RoundDefinition.Power = 19.f;
            Definition->RoundDefinition.ActionPointCost = 1;
            Definition->RoundDefinition.SubActionPointCost = 0;
            Definition->RoundDefinition.WindupSeconds = 0.2f;
            Definition->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
            Definition->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Element_Physical);
            Definition->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Shape_Projectile);
            Rules.SchemaVersion = 1;
            Rules.BalanceVersion = 1;
            Rules.SkillCount = 1;
            Rules.WeaponQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag());
            FRunWeaponSkillCandidate& Candidate = Rules.Candidates.AddDefaulted_GetRef();
            Candidate.Skill = FSoftObjectPath(Definition.Get());
            Candidate.Tags = Definition->RoundDefinition.EffectTags;
            Candidate.AllowedItemQuery = Rules.WeaponQuery;
            Candidate.BaseWeight = 1.f;
            Candidate.Balance.RarityTag = RunSkillBalance::ResolveRarityTag(TEXT("흰색"));
            Candidate.Balance.Power = 43.f;
            Candidate.Balance.ActionPointCost = 3;
            Candidate.Balance.SubActionPointCost = 1;
            Candidate.Balance.WindupSeconds = 0.7f;
            Candidate.SelectionTags.AddTag(Candidate.Balance.RarityTag);
            const TCHAR* RarityNames[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
            for (const TCHAR* EquipmentName : RarityNames)
            {
                FRunWeaponRarityRule& Rarity = Rules.Rarities.AddDefaulted_GetRef();
                Rarity.RarityTag = RunItemShopCatalog::ResolveRarityTag(EquipmentName);
                Rarity.DisplayName = FText::FromString(EquipmentName);
                Rarity.BaseWeight = 1.f;
                for (const TCHAR* SkillName : RarityNames)
                {
                    FRunSkillRarityWeight& Weight = Rules.SkillRarityWeights.AddDefaulted_GetRef();
                    Weight.EquipmentRarityTag = Rarity.RarityTag;
                    Weight.SkillRarityTag = RunSkillBalance::ResolveRarityTag(SkillName);
                    Weight.Weight = 20.f;
                }
            }
            Opponents.Reset(NewObject<UOpponentSnapshotCatalogDataAsset>(Package.Get(), TEXT("FrozenOpponentCatalog"), RF_Transient));
            Opponents->EnemyClasses.Add(TEXT("FrozenOpponent"), AEnemyUnit::StaticClass());
            Opponents->Skills.Add(TEXT("FrozenSkill"), Definition.Get());
            FRunPartyMember& Member = Party.AddDefaulted_GetRef();
            Member.SlotIndex = 0;
            Member.bCreated = true;
            Member.CurrentHP = 100.f;
        }

        FCombatCheckpointData Checkpoint() const
        {
            FCombatCheckpointData Result;
            Result.SchemaVersion = UCombatCheckpointLibrary::CurrentSchemaVersion;
            Result.AttemptId = FGuid::NewGuid();
            Result.Revision = 1;
            Result.NodeId = TEXT("Combat_01");
            Result.EncounterId = TEXT("FrozenSkillEncounter");
            Result.RoundNumber = 1;
            Result.PlanRevision = 1;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FCombatCheckpointUnit& Unit = Result.Units.AddDefaulted_GetRef();
                Unit.UnitId = FGuid::NewGuid();
                Unit.RoundUnitId = Index + 1;
                Unit.PartySlot = Index == 0 ? 0 : INDEX_NONE;
                Unit.Team = Index == 0 ? ETeam::Player : ETeam::Enemy;
                Unit.UnitClass = FSoftObjectPath(Index == 0 ? APlayerUnit::StaticClass() : AEnemyUnit::StaticClass());
                Unit.CharacterName = FText::FromString(Index == 0 ? TEXT("Frozen Player") : TEXT("Frozen Opponent"));
                Unit.MaxAP = 3;
                Unit.AP = 3;
                Unit.MaxSubAP = 1;
                Unit.SubAP = 1;
                Unit.GridCoord = FIntPoint(1, Index == 0 ? 0 : 3);
                Unit.Skills.Add(FSoftObjectPath(Definition.Get()));
                FCombatCheckpointRoundPlan& Plan = Result.RoundPlans.AddDefaulted_GetRef();
                Plan.UnitId = Unit.RoundUnitId;
                Plan.Command.UnitId = Unit.RoundUnitId;
                Plan.Command.SkillId = FName(*Definition->GetPrimaryAssetId().ToString());
                Plan.Command.TargetUnitId = Index == 0 ? 2 : 1;
                Plan.Command.TargetCoord = FIntPoint(1, Index == 0 ? 3 : 0);
                Plan.MoveDestinationCoord = Unit.GridCoord;
            }
            Result.bHasOpponentSnapshot = true;
            Result.OpponentCatalog = FSoftObjectPath(Opponents.Get());
            Result.OpponentSnapshot.SnapshotId = TEXT("FrozenSnapshot");
            FPartySnapshotMember& Opponent = Result.OpponentSnapshot.Members.AddDefaulted_GetRef();
            Opponent.MemberId = TEXT("OpponentOne");
            Opponent.ClassId = TEXT("FrozenOpponent");
            Opponent.CharacterName = TEXT("Frozen Opponent");
            Opponent.Stats.MaxActionPoints = 3;
            Opponent.Stats.MaxSubActionPoints = 1;
            Opponent.SkillIds.Add(TEXT("FrozenSkill"));
            return Result;
        }
    };

    bool SameSkill(const FCombatRoundSkill& Left, const FCombatRoundSkill& Right)
    {
        return FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFrozenSkillContractTest, "ProjectA.Combat.SkillBalance.FrozenProfileContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatFrozenSkillContractTest::RunTest(const FString& Parameters)
{
    CombatSkillBalanceTests::FFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("The synthetic frozen selection state is valid"), RunWeaponSkillRules::Validate(Fixture.Rules, Error))) return false;
    const FCombatRoundSkill Authored = Fixture.Definition->RoundDefinition;
    FCombatRoundSkill Original;
    if (!TestTrue(TEXT("The authored GAS profile resolves"), Fixture.Definition->ResolveRoundSkill(Original, Error))) return false;
    FCombatRoundSkill Expected = Original;
    Expected.Power = 43.f;
    Expected.ActionPointCost = 3;
    Expected.SubActionPointCost = 1;
    Expected.WindupSeconds = 0.7f;
    FCombatRoundSkill Resolved;
    const FSoftObjectPath Path(Fixture.Definition.Get());
    TestTrue(TEXT("Frozen tuning changes only numeric power costs and windup"), RunWeaponSkillRules::ResolveSkill(Path, Fixture.Rules, Resolved, Error) && CombatSkillBalanceTests::SameSkill(Resolved, Expected));
    TestTrue(TEXT("Resolving Run tuning never mutates the source DataAsset"), CombatSkillBalanceTests::SameSkill(Authored, Fixture.Definition->RoundDefinition));
    const FRunWeaponSkillRulesState Legacy;
    TestTrue(TEXT("A legacy Run retains the complete original profile"), RunWeaponSkillRules::ResolveSkill(Path, Legacy, Resolved, Error) && CombatSkillBalanceTests::SameSkill(Resolved, Original));
    FRunWeaponSkillRulesState ExistingWeaponRun = Fixture.Rules;
    ExistingWeaponRun.BalanceVersion = 0;
    ExistingWeaponRun.SkillRarityWeights.Reset();
    ExistingWeaponRun.Candidates[0].SelectionTags.Reset();
    ExistingWeaponRun.Candidates[0].Balance = FRunSkillBalance();
    TestTrue(TEXT("Existing generated-weapon Runs keep asset values at balance version zero"), RunWeaponSkillRules::Validate(ExistingWeaponRun, Error) && RunWeaponSkillRules::ResolveSkill(Path, ExistingWeaponRun, Resolved, Error) && CombatSkillBalanceTests::SameSkill(Resolved, Original));
    TStrongObjectPtr<USkillDefinitionDataAsset> Unlisted(NewObject<USkillDefinitionDataAsset>(Fixture.Package.Get(), TEXT("UnlistedMonsterSkill"), RF_Transient));
    Unlisted->bUseRoundDefinition = true;
    Unlisted->RoundDefinition = Authored;
    FCombatRoundSkill UnlistedOriginal;
    TestTrue(TEXT("Skills outside the frozen weapon candidates keep their authored profile"), Unlisted->ResolveRoundSkill(UnlistedOriginal, Error) && RunWeaponSkillRules::ResolveSkill(FSoftObjectPath(Unlisted.Get()), Fixture.Rules, Resolved, Error) && CombatSkillBalanceTests::SameSkill(Resolved, UnlistedOriginal));
    for (float InvalidPower : {-1.f, std::numeric_limits<float>::quiet_NaN()})
    {
        FRunWeaponSkillRulesState Invalid = Fixture.Rules;
        Invalid.Candidates[0].Balance.Power = InvalidPower;
        Resolved = Expected;
        TestFalse(TEXT("Invalid frozen values cannot become an executable profile"), RunWeaponSkillRules::ResolveSkill(Path, Invalid, Resolved, Error));
        TestTrue(TEXT("Rejected tuning leaves the output profile intact"), CombatSkillBalanceTests::SameSkill(Resolved, Expected) && !Error.IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFrozenSkillCheckpointTest, "ProjectA.Checkpoint.SkillBalance.SharedCostsAndLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatFrozenSkillCheckpointTest::RunTest(const FString& Parameters)
{
    CombatSkillBalanceTests::FFixture Fixture;
    FText Error;
    FCombatCheckpointData Checkpoint = Fixture.Checkpoint();
    if (!TestTrue(TEXT("The Snapshot boundary accepts both teams using the frozen AP and SAP costs"), UCombatCheckpointLibrary::Validate(Checkpoint, Fixture.Party, Error, &Fixture.Rules))) return false;
    for (int32 UnitIndex = 0; UnitIndex < 2; ++UnitIndex)
    {
        FCombatCheckpointData Insufficient = Checkpoint;
        Insufficient.Units[UnitIndex].AP = 1;
        TestTrue(TEXT("The old asset cost still validates without a new Run balance"), UCombatCheckpointLibrary::Validate(Insufficient, Fixture.Party, Error));
        TestFalse(TEXT("Frozen AP is validated for the player and the Snapshot enemy equally"), UCombatCheckpointLibrary::Validate(Insufficient, Fixture.Party, Error, &Fixture.Rules));
        Insufficient = Checkpoint;
        Insufficient.Units[UnitIndex].SubAP = 0;
        TestFalse(TEXT("Frozen SAP cannot be bypassed by checkpoint restoration"), UCombatCheckpointLibrary::Validate(Insufficient, Fixture.Party, Error, &Fixture.Rules));
    }
    Checkpoint.Units.Swap(0, 1);
    Checkpoint.RoundPlans.Swap(0, 1);
    TestTrue(TEXT("Shared skill IDs have identical costs when the enemy registers first"), UCombatCheckpointLibrary::Validate(Checkpoint, Fixture.Party, Error, &Fixture.Rules));
    const FCombatCheckpointData Before = Checkpoint;
    FRunWeaponSkillRulesState Invalid = Fixture.Rules;
    Invalid.Candidates[0].Balance.ActionPointCost = 101;
    TestFalse(TEXT("Malformed tuning fails before restoring any command"), UCombatCheckpointLibrary::Validate(Checkpoint, Fixture.Party, Error, &Invalid));
    TestTrue(TEXT("Validation never rewrites the saved commands or Snapshot"), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Checkpoint, &Before, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFrozenSkillSerializationTest, "ProjectA.Checkpoint.SkillBalance.SerializedFrozenSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatFrozenSkillSerializationTest::RunTest(const FString& Parameters)
{
    CombatSkillBalanceTests::FFixture Fixture;
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->WeaponSkillRules = Fixture.Rules;
    Save->CombatCheckpoint = Fixture.Checkpoint();
    Save->Party = Fixture.Party;
    FText Error;
    FCombatRoundSkill Expected;
    if (!TestTrue(TEXT("The frozen profile resolves before serialization"), RunWeaponSkillRules::ResolveSkill(FSoftObjectPath(Fixture.Definition.Get()), Save->WeaponSkillRules, Expected, Error))) return false;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The frozen Run and Snapshot serialize without a disk write"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The frozen balance restores"), Restored.Get())) return false;
    Fixture.Rules.Candidates[0].Balance.Power = 999.f;
    Fixture.Definition->RoundDefinition.Power = 700.f;
    Fixture.Definition->RoundDefinition.ActionPointCost = 7;
    Fixture.Definition->RoundDefinition.WindupSeconds = 2.f;
    FCombatRoundSkill Resolved;
    TestTrue(TEXT("Restoration uses the serialized Run values despite changed current defaults"), RunWeaponSkillRules::ResolveSkill(FSoftObjectPath(Fixture.Definition.Get()), Restored->WeaponSkillRules, Resolved, Error) && CombatSkillBalanceTests::SameSkill(Resolved, Expected));
    TestTrue(TEXT("Saved planning remains valid with its frozen costs and Snapshot build"), UCombatCheckpointLibrary::Validate(Restored->CombatCheckpoint, Restored->Party, Error, &Restored->WeaponSkillRules));
    TestFalse(TEXT("Falling back to changed asset costs would reject the same saved plans"), UCombatCheckpointLibrary::Validate(Restored->CombatCheckpoint, Restored->Party, Error));
    TestTrue(TEXT("Snapshot skill identifiers remain unchanged by balance resolution"), Restored->CombatCheckpoint.OpponentSnapshot.Members[0].SkillIds == TArray<FName>{TEXT("FrozenSkill")});
    return true;
}

#endif
