#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Combat/Round/CombatRoundProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

namespace CombatRoundTests
{
    // Native fixtures exercise the actual server runtime without authored maps or gameplay startup.
    // 네이티브 픽스처로 제작 맵이나 게임 시작 흐름 없이 실제 서버 런타임을 검증합니다.
    struct FFixture
    {
        TStrongObjectPtr<UWorld> World;
        ACombatManager* Combat = nullptr;
        ACombatGridManager* Grid = nullptr;
        ACombatArena* Arena = nullptr;
        ACombatRoundCoordinator* Round = nullptr;
        TArray<AUnitBase*> Humans;
        TArray<AUnitBase*> Enemies;
        TArray<APartyPlayerController*> Controllers;
        FName EnemySkillId;
        FText Error;

        FFixture()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
            if (!World.IsValid()) return;
            Combat = World->SpawnActor<ACombatManager>();
            Grid = World->SpawnActor<ACombatGridManager>();
            Arena = World->SpawnActor<ACombatArena>();
            if (!Combat || !Grid || !Arena) return;
            Arena->Grid = Grid;
            Combat->SetCombatGrid(Grid);
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    FActorSpawnParameters Params;
                    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(FVector(Row * 200.0f, Column * 200.0f, 0.0f), FRotator::ZeroRotator, Params);
                    if (!Tile) continue;
                    const FIntPoint Coord(Row, Column);
                    Tile->InitializeGridTile(Grid, Coord, Column < 2 ? ETileTerritory::Player : ETileTerritory::Enemy);
                    Tile->SetActorEnableCollision(false);
                    Grid->TileMap.Add(Coord, Tile);
                }
            }
        }

        ~FFixture()
        {
            if (!World.IsValid()) return;
            for (TActorIterator<ACombatRoundProjectile> It(World.Get()); It; ++It)
            {
                It->OnImpact.Clear();
                It->OnResolved.Clear();
            }
            if (IsValid(Combat)) Combat->ResetCombat();
            World->DestroyWorld(false);
        }

        AUnitBase* AddUnit(FIntPoint Coord, ETeam Team)
        {
            ACombatGridTile* Tile = Grid ? Grid->GetTileAtCoord(Coord) : nullptr;
            if (!Tile) return nullptr;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnitBase* Unit = World->SpawnActor<AUnitBase>(Tile->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Params);
            if (!Unit) return nullptr;
            Unit->SetTeam(Team);
            Unit->SetCurrentTile(Tile);
            Unit->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
            Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Unit->GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            return Unit;
        }

        bool GiveEnemySkill(AUnitBase* Unit, const FCombatRoundSkill* Definition)
        {
            USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Unit);
            Skill->AbilityClass = UGA_DefaultAttack::StaticClass();
            Skill->SkillName = FText::FromString(Definition ? TEXT("Fixture authored action") : TEXT("Fixture wait"));
            Skill->bUseRoundDefinition = true;
            Skill->RoundDefinition.Kind = ECombatRoundSkillKind::Wait;
            Skill->RoundDefinition.Approach = ECombatRoundApproach::None;
            Skill->RoundDefinition.ActionPointCost = 0;
            Skill->RoundDefinition.Power = 0.0f;
            if (Definition) Skill->RoundDefinition = *Definition;
            EnemySkillId = FName(*Skill->GetPrimaryAssetId().ToString());
            return Unit->ConfigureProfession(100.0f, 2, 2, {Skill});
        }

        bool Initialize(int32 HumanCount = 1, int32 EnemySpeed = 10, const FCombatRoundSkill* EnemySkill = nullptr, FIntPoint EnemyCoord = FIntPoint(0, 3))
        {
            if (!World.IsValid() || !Combat || !Arena || !Grid || Grid->TileMap.Num() != 16) return false;
            FRunIdentityData Identity;
            Identity.Origin = ERunIdentityOrigin::AccountProvider;
            Identity.RunId = FGuid::NewGuid();
            Identity.HostEpoch = 1;
            TArray<FRunPartyMember> Members;
            TMap<int32, TObjectPtr<AUnitBase>> PartyActors;
            TArray<AUnitBase*> Units;
            for (int32 Index = 0; Index < HumanCount; ++Index)
            {
                AUnitBase* Unit = AddUnit(FIntPoint(Index * 2, 0), ETeam::Player);
                APartyPlayerController* Controller = World->SpawnActor<APartyPlayerController>();
                if (!Unit || !Controller) return false;
                Unit->CombatSpeed = 20 - Index * 2;
                World->AddController(Controller);
                Controller->SetCombatContext(Combat, true);
                if (Index == 0) Controller->SetAsLocalPlayerController();
                Humans.Add(Unit);
                Controllers.Add(Controller);
                Units.Add(Unit);
                FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
                Participant.AccountId.Provider = TEXT("FixtureProvider");
                Participant.AccountId.Subject = FString::Printf(TEXT("round-owner-%d"), Index);
                FRunPartyMember& Member = Members.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.bCreated = true;
                Member.ClassId = TEXT("Hunter");
                Member.CharacterName = FText::FromString(Participant.AccountId.Subject);
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
                PartyActors.Add(Index, Unit);
            }
            Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
            AUnitBase* Enemy = AddUnit(EnemyCoord, ETeam::Enemy);
            if (!Enemy || !GiveEnemySkill(Enemy, EnemySkill)) return false;
            Enemy->CombatSpeed = EnemySpeed;
            Enemies.Add(Enemy);
            Units.Add(Enemy);
            Combat->RegisterUnits(Units);
            UCombatActionAuthority* Authority = Combat->GetActionAuthority();
            if (!Authority->ConfigureRun(Identity, Members, PartyActors, Error)) return false;
            for (int32 Index = 0; Index < Controllers.Num(); ++Index)
            {
                if (!Authority->BindParticipant(Controllers[Index], Identity.OriginalParticipants[Index].AccountId)) return false;
            }
            Combat->StartCombat_Internal();
            Round = Combat->GetRoundCoordinator();
            return Round && Round->GetView().CombatId.IsValid() && Round->GetView().Phase == ECombatRoundPhase::Planning;
        }

        FCombatRoundCommand Command(AUnitBase* Unit, FName Skill, AUnitBase* Target = nullptr) const
        {
            FCombatRoundCommand Result;
            Result.UnitId = Unit->UnitIndex;
            Result.SkillId = Skill;
            Result.DestinationCoord = Unit->GetCurrentTile()->GridCoord;
            Result.TargetCoord = Target ? Target->GetCurrentTile()->GridCoord : Result.DestinationCoord;
            Result.TargetUnitId = Target ? Target->UnitIndex : INDEX_NONE;
            return Result;
        }

        bool Submit(int32 ControllerIndex, const FCombatRoundCommand& Command)
        {
            const FCombatRoundView& View = Round->GetView();
            return Round->SubmitPlan(Controllers[ControllerIndex], View.CombatId, View.RoundNumber, View.PlanRevision, Command, Error);
        }

        bool Ready(int32 ControllerIndex)
        {
            const FCombatRoundView& View = Round->GetView();
            return Round->SetParticipantReady(Controllers[ControllerIndex], View.CombatId, View.RoundNumber, View.PlanRevision, true, Error);
        }

        bool AdvanceUntilNextRound(int32 InitialRound)
        {
            for (int32 Step = 0; Step < 1000 && Round->GetView().RoundNumber == InitialRound && Round->IsRoundSessionActive(); ++Step)
            {
                Round->Tick(0.01f);
            }
            return Round->GetView().RoundNumber == InitialRound + 1 && Round->GetView().Phase == ECombatRoundPhase::Planning;
        }
    };

    bool SameCommand(const FCombatRoundCommand& Left, const FCombatRoundCommand& Right)
    {
        return FCombatRoundCommand::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundPlanningTargetsTest, "ProjectA.Combat.Round.PlanningUnitTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundPlanningTargetsTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Two-owner planning fixture initializes"), Fixture.Initialize(2))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    AUnitBase* Source = Fixture.Humans[0];
    AUnitBase* Friend = Fixture.Humans[1];
    AUnitBase* Enemy = Fixture.Enemies[0];
    TestTrue(TEXT("Attack offers a living enemy"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Strike"), Enemy->UnitIndex));
    TestFalse(TEXT("Attack excludes a living ally"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Strike"), Friend->UnitIndex));
    TestTrue(TEXT("Guard offers a living ally"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Guard"), Friend->UnitIndex));
    TestTrue(TEXT("Guard keeps the existing self-target option"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Guard"), Source->UnitIndex));
    TestFalse(TEXT("Guard excludes enemies"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Guard"), Enemy->UnitIndex));
    TestFalse(TEXT("Unknown source cannot produce target candidates"), Round->IsValidUnitTarget(INDEX_NONE, TEXT("Strike"), Enemy->UnitIndex));
    TestFalse(TEXT("Unknown skill cannot produce target candidates"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Unknown"), Enemy->UnitIndex));
    TestFalse(TEXT("Wait has no unit target choices"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Wait"), Enemy->UnitIndex));
    TestFalse(TEXT("Ground attacks have no unit target choices"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("GroundStrike"), Enemy->UnitIndex));
    FText Error;
    TestTrue(TEXT("Wait is valid without a target"), Round->CanPlanCommand(Fixture.Command(Source, TEXT("Wait")), Error));
    FCombatRoundCommand Ground = Fixture.Command(Source, TEXT("GroundStrike"));
    Ground.TargetCoord = Enemy->GetCurrentTile()->GridCoord;
    Ground.DestinationCoord = Ground.TargetCoord;
    TestTrue(TEXT("Ground attack uses valid coordinates without a unit target"), Round->CanPlanCommand(Ground, Error));
    TestEqual(TEXT("Common tile prototypes do not replace the passive enemy plan"), Round->GetView().Units.Last().Command.SkillId, FName(TEXT("Wait")));
    const FCombatRoundCommand Attack = Fixture.Command(Source, TEXT("Strike"), Enemy);
    Enemy->Die();
    TestFalse(TEXT("A dead enemy disappears from attack candidates before another plan is submitted"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Strike"), Enemy->UnitIndex));
    TestFalse(TEXT("A stale attack draft fails preview after target death"), Round->CanPlanCommand(Attack, Error));
    TestFalse(TEXT("The server also rejects the stale target"), Fixture.Submit(0, Attack));
    Friend->Die();
    TestFalse(TEXT("A dead ally disappears from guard candidates"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Guard"), Friend->UnitIndex));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundPlanningValidationTest, "ProjectA.Combat.Round.PreviewAndServerValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundPlanningValidationTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Planning fixture initializes"), Fixture.Initialize())) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    AUnitBase* Source = Fixture.Humans[0];
    AUnitBase* Enemy = Fixture.Enemies[0];
    const auto CheckRejected = [&](const FString& Reason, const FCombatRoundCommand& Command)
    {
        const int32 Revision = Round->GetView().PlanRevision;
        const FCombatRoundCommand Previous = Round->GetView().Units[0].Command;
        const int32 AP = Source->GetCurrentActionPoint();
        const int32 SubAP = Source->GetCurrentSubActionPoint();
        FText PreviewError;
        TestFalse(Reason + TEXT(" fails preview"), Round->CanPlanCommand(Command, PreviewError));
        TestFalse(Reason + TEXT(" provides an explanation"), PreviewError.IsEmpty());
        TestFalse(Reason + TEXT(" fails server submission"), Fixture.Submit(0, Command));
        TestEqual(Reason + TEXT(" has the same preview and server explanation"), PreviewError.ToString(), Fixture.Error.ToString());
        TestEqual(Reason + TEXT(" preserves revision"), Round->GetView().PlanRevision, Revision);
        TestTrue(Reason + TEXT(" preserves the applied command"), SameCommand(Previous, Round->GetView().Units[0].Command));
        TestEqual(Reason + TEXT(" does not consume AP"), Source->GetCurrentActionPoint(), AP);
        TestEqual(Reason + TEXT(" does not consume SubAP"), Source->GetCurrentSubActionPoint(), SubAP);
    };
    const FCombatRoundCommand Attack = Fixture.Command(Source, TEXT("Strike"), Enemy);
    if (!TestTrue(TEXT("Fixture can spend its AP"), Source->ConsumeActionPoint(Source->GetCurrentActionPoint()))) return false;
    CheckRejected(TEXT("Insufficient AP"), Attack);
    FText Error;
    TestTrue(TEXT("Zero-cost wait remains available without AP"), Round->CanPlanCommand(Fixture.Command(Source, TEXT("Wait")), Error));
    Source->ResetActionPoint();
    if (!TestTrue(TEXT("Fixture can spend its SubAP"), Source->ConsumeSubActionPoint(Source->GetCurrentSubActionPoint()))) return false;
    CheckRejected(TEXT("Insufficient SubAP"), Fixture.Command(Source, TEXT("MoveShot"), Enemy));
    Source->ResetSubActionPoint();
    FCombatRoundCommand Ground = Fixture.Command(Source, TEXT("GroundStrike"));
    Ground.TargetCoord = Enemy->GetCurrentTile()->GridCoord;
    Ground.DestinationCoord = FIntPoint(-1, 0);
    CheckRejected(TEXT("Invalid approach coordinate"), Ground);
    Ground.DestinationCoord = Source->GetCurrentTile()->GridCoord;
    Ground.TargetCoord = FIntPoint(4, 0);
    CheckRejected(TEXT("Invalid attack coordinate"), Ground);
    const int32 AP = Source->GetCurrentActionPoint();
    const int32 Revision = Round->GetView().PlanRevision;
    TestTrue(TEXT("A legal attack passes preview"), Round->CanPlanCommand(Attack, Error));
    TestEqual(TEXT("Successful preview does not spend AP"), Source->GetCurrentActionPoint(), AP);
    TestEqual(TEXT("Successful preview does not apply the draft"), Round->GetView().PlanRevision, Revision);
    if (!TestTrue(TEXT("The same attack passes server submission"), Fixture.Submit(0, Attack)) || !TestTrue(TEXT("The legal plan can be locked"), Fixture.Ready(0))) return false;
    TestFalse(TEXT("Preview rejects edits after planning is locked"), Round->CanPlanCommand(Attack, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundAuthoredTileAITest, "ProjectA.Combat.Round.AuthoredTileAIPlan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundAuthoredTileAITest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FCombatRoundSkill Skill;
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.Approach = ECombatRoundApproach::Tile;
    Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
    Skill.Power = 10.0f;
    FFixture Fixture;
    if (!TestTrue(TEXT("An enemy with an authored returning tile attack initializes"), Fixture.Initialize(2, 10, &Skill, FIntPoint(2, 3)))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    const FCombatRoundCommand EnemyPlan = Round->GetView().Units.Last().Command;
    const FIntPoint NearestEnemyHome = Fixture.Humans[1]->GetCurrentTile()->GridCoord;
    TestEqual(TEXT("AI chooses its equipped authored attack"), EnemyPlan.SkillId, Fixture.EnemySkillId);
    TestTrue(TEXT("AI targets the nearest opponent rather than the first roster entry"), EnemyPlan.TargetCoord == NearestEnemyHome);
    TestTrue(TEXT("AI approaches that same coordinate before returning"), EnemyPlan.DestinationCoord == NearestEnemyHome);
    TestTrue(TEXT("AI plan is ready before human edits"), Round->GetView().Units.Last().bReady);
    FText Error;
    TestTrue(TEXT("The generated enemy plan passes the same planning validation"), Round->CanPlanCommand(EnemyPlan, Error));
    if (!TestTrue(TEXT("Host applies a human wait"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("Wait"))))) return false;
    TestTrue(TEXT("First human edit leaves the enemy plan fixed"), SameCommand(EnemyPlan, Round->GetView().Units.Last().Command));
    if (!TestTrue(TEXT("Guest applies a human guard"), Fixture.Submit(1, Fixture.Command(Fixture.Humans[1], TEXT("Guard"), Fixture.Humans[0])))) return false;
    TestTrue(TEXT("Second human edit leaves the enemy plan fixed"), SameCommand(EnemyPlan, Round->GetView().Units.Last().Command));
    if (!TestTrue(TEXT("Host readies the plan"), Fixture.Ready(0)) || !TestTrue(TEXT("Guest readies the plan"), Fixture.Ready(1))) return false;
    TestTrue(TEXT("Authored tile AI command can be locked with the human plans"), Round->GetView().Phase == ECombatRoundPhase::Resolving);
    TestTrue(TEXT("Locking preserves the fixed enemy coordinates"), SameCommand(EnemyPlan, Round->GetView().Units.Last().Command));
    TestEqual(TEXT("Locking charges the authored action cost once"), Fixture.Enemies[0]->GetCurrentActionPoint(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundAuthoredAIScopeTest, "ProjectA.Combat.Round.AuthoredAIScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundAuthoredAIScopeTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 2; ++Case)
    {
        FCombatRoundSkill Skill;
        Skill.Kind = Case == 0 ? ECombatRoundSkillKind::GroundAttack : ECombatRoundSkillKind::Guard;
        Skill.Approach = Case == 0 ? ECombatRoundApproach::Tile : ECombatRoundApproach::None;
        Skill.bRemainAtDestination = Case == 0;
        FFixture Fixture;
        if (!TestTrue(TEXT("Deferred AI tactic fixture initializes"), Fixture.Initialize(1, 10, &Skill))) return false;
        TestEqual(Case == 0 ? TEXT("Authored resident movement is not selected automatically") : TEXT("Authored support is not selected automatically"), Fixture.Round->GetView().Units.Last().Command.SkillId, FName(TEXT("Wait")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundPlanOwnershipTest, "ProjectA.Combat.Round.OwnershipRevisionAndLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundPlanOwnershipTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Identified two-owner round initializes"), Fixture.Initialize(2))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    const FCombatRoundCommand EnemyPlan = Round->GetView().Units.Last().Command;
    const int32 InitialRevision = Round->GetView().PlanRevision;
    TestFalse(TEXT("Host cannot plan the guest character"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[1], TEXT("Wait"))));
    TestEqual(TEXT("Rejected ownership does not change revision"), Round->GetView().PlanRevision, InitialRevision);
    TestFalse(TEXT("Unknown skills are rejected"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("UnregisteredSkill"))));
    const FCombatRoundCommand FirstPlan = Fixture.Command(Fixture.Humans[0], TEXT("Wait"));
    if (!TestTrue(TEXT("Owner applies own plan"), Fixture.Submit(0, FirstPlan))) return false;
    FText Error;
    TestFalse(TEXT("Stale revisions cannot overwrite a plan"), Round->SubmitPlan(Fixture.Controllers[0], Round->GetView().CombatId, Round->GetView().RoundNumber, InitialRevision, FirstPlan, Error));
    TestFalse(TEXT("Another combat identity is rejected"), Round->SubmitPlan(Fixture.Controllers[0], FGuid::NewGuid(), Round->GetView().RoundNumber, Round->GetView().PlanRevision, FirstPlan, Error));
    if (!TestTrue(TEXT("Guest applies own plan"), Fixture.Submit(1, Fixture.Command(Fixture.Humans[1], TEXT("Wait")))) || !TestTrue(TEXT("Host becomes ready"), Fixture.Ready(0))) return false;
    TestTrue(TEXT("Host readiness is visible"), Round->GetView().Units[0].bReady);
    if (!TestTrue(TEXT("Guest edit resets human readiness"), Fixture.Submit(1, Fixture.Command(Fixture.Humans[1], TEXT("Wait"))))) return false;
    TestFalse(TEXT("Earlier human readiness is invalidated"), Round->GetView().Units[0].bReady);
    TestTrue(TEXT("Human edits never change the fixed enemy command"), SameCommand(EnemyPlan, Round->GetView().Units.Last().Command));
    if (!Fixture.Ready(0) || !Fixture.Ready(1)) return false;
    TestTrue(TEXT("All ready transitions to real-time resolution"), Round->GetView().Phase == ECombatRoundPhase::Resolving);
    TestFalse(TEXT("Locked plans reject even current-revision edits"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("Guard"), Fixture.Humans[1])));
    TestTrue(TEXT("Highest speed begins at zero"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Complete);
    Round->Tick(0.19f);
    TestTrue(TEXT("Two speed points preserve a 0.2 second delay"), Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Waiting);
    Round->Tick(0.02f);
    TestTrue(TEXT("Slower action begins after its configured delay"), Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Complete);
    TestTrue(TEXT("Resolution does not mutate the locked command"), SameCommand(FirstPlan, Round->GetView().Units[0].Command));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMovementTest, "ProjectA.Combat.Round.ActualApproachAndReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMovementTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Round initializes"), Fixture.Initialize())) return false;
    AUnitBase* Source = Fixture.Humans[0];
    AUnitBase* Target = Fixture.Enemies[0];
    const FVector Origin = Source->GetActorLocation();
    ACombatGridTile* Home = Source->GetCurrentTile();
    if (!Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Target)) || !Fixture.Ready(0)) return false;
    Fixture.Round->Tick(0.25f);
    TestTrue(TEXT("Approach changes the authoritative world position"), FVector::Dist2D(Origin, Source->GetActorLocation()) > 50.0f);
    TestEqual(TEXT("Home remains reserved during the attack excursion"), Source->GetCurrentTile(), Home);
    TestTrue(TEXT("Melee has not damaged a distant target before approach and windup"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f));
    if (!TestTrue(TEXT("The round waits through attack and return"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestTrue(TEXT("The survivor physically returns home"), FVector::Dist2D(Origin, Source->GetActorLocation()) <= 2.0f);
    TestTrue(TEXT("Actual close-range hit applies damage"), Target->GetAttributeSet()->GetHP() < 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundPendingProjectileTest, "ProjectA.Combat.Round.PendingProjectileBlocksNextRound", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundPendingProjectileTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Round initializes"), Fixture.Initialize(1, 20))) return false;
    if (!Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("Arrow"), Fixture.Enemies[0])) || !Fixture.Ready(0)) return false;
    Fixture.Round->Tick(0.25f);
    TestTrue(TEXT("Released missile remains in the world"), Fixture.Round->GetView().PendingProjectiles > 0);
    TestTrue(TEXT("Shooter action can complete before its missile"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Complete);
    TestTrue(TEXT("The other action is already complete"), Fixture.Round->GetView().Units.Last().ActionPhase == ECombatRoundActionPhase::Complete);
    TestTrue(TEXT("Resolution remains active while the missile is pending"), Fixture.Round->GetView().Phase == ECombatRoundPhase::Resolving);
    if (!TestTrue(TEXT("Settling all work advances the round"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("Next planning phase has no pending missile"), Fixture.Round->GetView().PendingProjectiles, 0);
    TestTrue(TEXT("The in-flight attack eventually applied its damage"), Fixture.Enemies[0]->GetAttributeSet()->GetHP() < 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundRevokedContextTest, "ProjectA.Combat.Round.RevokedSessionStopsSimulation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundRevokedContextTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Round initializes"), Fixture.Initialize())) return false;
    if (!Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("Strike"), Fixture.Enemies[0])) || !Fixture.Ready(0)) return false;
    const FVector Before = Fixture.Humans[0]->GetActorLocation();
    Fixture.Combat->GetActionAuthority()->Reset();
    Fixture.Round->Tick(0.25f);
    TestTrue(TEXT("Revoked combat authority suspends resolution"), Fixture.Round->GetView().Phase == ECombatRoundPhase::Suspended);
    TestTrue(TEXT("No movement is executed under the old session"), Fixture.Humans[0]->GetActorLocation().Equals(Before));
    TestTrue(TEXT("No damage is executed under the old session"), FMath::IsNearlyEqual(Fixture.Enemies[0]->GetAttributeSet()->GetHP(), 100.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundProjectileCasterDeathTest, "ProjectA.Combat.Round.ProjectileSurvivesCasterDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundProjectileCasterDeathTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FIntPoint(0, 0), ETeam::Player);
    AUnitBase* Target = Fixture.AddUnit(FIntPoint(0, 3), ETeam::Enemy);
    if (!TestNotNull(TEXT("Source exists"), Source) || !TestNotNull(TEXT("Target exists"), Target)) return false;
    ACombatRoundProjectile* Projectile = Fixture.World->SpawnActor<ACombatRoundProjectile>(Source->GetActorLocation(), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Projectile exists"), Projectile)) return false;
    int32 ImpactCount = 0;
    int32 ResolveCount = 0;
    bool bCasterWasDead = false;
    FVector ImpactLocation = FVector::ZeroVector;
    Projectile->OnImpact.AddLambda([&](AUnitBase* Caster, AUnitBase* Hit, float Damage)
    {
        ++ImpactCount;
        bCasterWasDead = !Caster->IsUnitAlive();
        ImpactLocation = Projectile->GetActorLocation();
        UCombatEffectLibrary::ApplyDamageToUnit(Caster, Hit, UGE_Damage::StaticClass(), Damage);
    });
    Projectile->OnResolved.AddLambda([&ResolveCount](ACombatRoundProjectile*) { ++ResolveCount; });
    Projectile->InitializeProjectile(Source, Target, Target->GetActorLocation(), 200.0f, 17.0f, 12.0f, 5.0f, false, true);
    Projectile->AdvanceProjectile(0.25f);
    TestFalse(TEXT("Missile is still in flight before caster death"), Projectile->HasResolved());
    Source->Die();
    for (int32 Step = 0; Step < 30 && !Projectile->HasResolved(); ++Step) Projectile->AdvanceProjectile(0.1f);
    TestTrue(TEXT("Released missile resolves after caster death"), Projectile->HasResolved());
    TestEqual(TEXT("Damage is delivered once"), ImpactCount, 1);
    TestEqual(TEXT("Completion is delivered once"), ResolveCount, 1);
    TestTrue(TEXT("The impact callback retained the dead caster"), bCasterWasDead);
    TestTrue(TEXT("Existing server damage accepts the released attack"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f));
    TestTrue(TEXT("Impact is at the swept contact before the target center"), ImpactLocation.Y < Target->GetActorLocation().Y);
    Projectile->AdvanceProjectile(0.1f);
    TestEqual(TEXT("Further steps cannot repeat impact"), ImpactCount, 1);
    TestEqual(TEXT("Further steps cannot repeat completion"), ResolveCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundProjectileCollisionTest, "ProjectA.Combat.Round.ProjectileCollisionFilteringAndLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundProjectileCollisionTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FIntPoint(0, 0), ETeam::Player);
    AUnitBase* Friend = Fixture.AddUnit(FIntPoint(0, 1), ETeam::Player);
    AUnitBase* LowerId = Fixture.AddUnit(FIntPoint(0, 2), ETeam::Enemy);
    AUnitBase* HigherId = Fixture.AddUnit(FIntPoint(1, 2), ETeam::Enemy);
    if (!Source || !Friend || !LowerId || !HigherId) return false;
    LowerId->UnitIndex = 10;
    HigherId->UnitIndex = 20;
    HigherId->SetActorLocation(LowerId->GetActorLocation(), false);
    for (int32 Case = 0; Case < 3; ++Case)
    {
        const bool bStartOverlapping = Case == 1;
        const bool bTargetOnly = Case == 2;
        const FVector Start = bStartOverlapping ? LowerId->GetActorLocation() : Source->GetActorLocation();
        ACombatRoundProjectile* Projectile = Fixture.World->SpawnActor<ACombatRoundProjectile>(Start, FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("Collision fixture missile exists"), Projectile)) return false;
        AUnitBase* HitUnit = nullptr;
        int32 Resolved = 0;
        Projectile->OnImpact.AddLambda([&HitUnit](AUnitBase*, AUnitBase* Hit, float) { HitUnit = Hit; });
        Projectile->OnResolved.AddLambda([&Resolved](ACombatRoundProjectile*) { ++Resolved; });
        Projectile->InitializeProjectile(Source, HigherId, FVector(0.0f, 700.0f, 100.0f), 1000.0f, 10.0f, 12.0f, 2.0f, false, bTargetOnly);
        Projectile->AdvanceProjectile(1.0f);
        TestEqual(TEXT("Friend is transparent and equal-time enemies use the declared target policy"), HitUnit, bTargetOnly ? HigherId : LowerId);
        TestEqual(TEXT("Initial overlap and swept collision both resolve once"), Resolved, 1);
    }
    ACombatRoundProjectile* Expiring = Fixture.World->SpawnActor<ACombatRoundProjectile>(Source->GetActorLocation(), FRotator::ZeroRotator);
    if (!Expiring) return false;
    int32 Expired = 0;
    int32 UnexpectedHits = 0;
    Expiring->OnImpact.AddLambda([&UnexpectedHits](AUnitBase*, AUnitBase*, float) { ++UnexpectedHits; });
    Expiring->OnResolved.AddLambda([&Expired](ACombatRoundProjectile*) { ++Expired; });
    Expiring->InitializeProjectile(Source, nullptr, FVector(-10000.0f, 0.0f, 100.0f), 100.0f, 10.0f, 12.0f, 0.1f, false, false);
    Expiring->AdvanceProjectile(1.0f);
    TestEqual(TEXT("Missed projectile has a bounded lifetime"), Expired, 1);
    TestEqual(TEXT("Expiration never fabricates a hit"), UnexpectedHits, 0);
    return true;
}

#endif
