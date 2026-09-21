#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Combat/Round/CombatRoundProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

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
        FName HumanSkillId;
        TMap<TWeakObjectPtr<AUnitBase>, TMap<FName, FName>> SkillAliases;
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

        AUnitBase* AddUnit(FIntPoint Coord, ETeam Team, TSubclassOf<AUnitBase> UnitClass = AUnitBase::StaticClass())
        {
            ACombatGridTile* Tile = Grid ? Grid->GetTileAtCoord(Coord) : nullptr;
            if (!Tile) return nullptr;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnitBase* Unit = World->SpawnActor<AUnitBase>(UnitClass, Tile->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Params);
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

        USkillDefinitionDataAsset* MakeSkill(AUnitBase* Unit, FName Alias, const FCombatRoundSkill& Definition)
        {
            USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Unit, FName(*(TEXT("RoundFixture_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            Skill->AbilityClass = UGA_DefaultAttack::StaticClass();
            Skill->SkillName = FText::FromName(Alias);
            Skill->bUseRoundDefinition = true;
            Skill->RoundDefinition = Definition;
            SkillAliases.FindOrAdd(Unit).Add(Alias, FName(*Skill->GetPrimaryAssetId().ToString()));
            return Skill;
        }

        FCombatRoundSkill WaitDefinition() const
        {
            FCombatRoundSkill Skill;
            Skill.Kind = ECombatRoundSkillKind::Wait;
            Skill.Approach = ECombatRoundApproach::None;
            Skill.ActionPointCost = 0;
            Skill.Power = 0.0f;
            return Skill;
        }

        FName SkillId(AUnitBase* Unit, FName Alias) const
        {
            const TMap<FName, FName>* Aliases = SkillAliases.Find(Unit);
            const FName* Id = Aliases ? Aliases->Find(Alias) : nullptr;
            return Id ? *Id : Alias;
        }

        bool GiveRoundSkill(AUnitBase* Unit, const FCombatRoundSkill* Definition, FName& OutSkillId, bool bIncludeWait = false)
        {
            USkillDefinitionDataAsset* Skill = MakeSkill(Unit, Definition ? FName(TEXT("Authored")) : FName(TEXT("Wait")), Definition ? *Definition : WaitDefinition());
            OutSkillId = FName(*Skill->GetPrimaryAssetId().ToString());
            TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills{Skill};
            if (Definition && bIncludeWait) Skills.Add(MakeSkill(Unit, TEXT("Wait"), WaitDefinition()));
            return Unit->ConfigureProfession(100.0f, 2, 2, Skills);
        }

        // Historical test actions are explicit equipped assets, never production coordinator defaults.
        // 기존 시험 행동은 실전 조정자 기본값이 아니라 명시적으로 장착한 에셋입니다.
        bool GiveFixtureSkills(AUnitBase* Unit)
        {
            TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
            FCombatRoundSkill Skill;
            Skills.Add(MakeSkill(Unit, TEXT("Strike"), Skill));
            Skill = FCombatRoundSkill();
            Skill.Kind = ECombatRoundSkillKind::Projectile;
            Skill.Approach = ECombatRoundApproach::None;
            Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
            Skill.bTargetOnly = false;
            Skill.Power = 20.0f;
            Skill.WindupSeconds = 0.2f;
            Skills.Add(MakeSkill(Unit, TEXT("Arrow"), Skill));
            Skill = FCombatRoundSkill();
            Skill.Kind = ECombatRoundSkillKind::GroundAttack;
            Skill.Approach = ECombatRoundApproach::Tile;
            Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
            Skill.Power = 20.0f;
            Skill.HitRange = 170.0f;
            Skill.WindupSeconds = 0.45f;
            Skills.Add(MakeSkill(Unit, TEXT("GroundStrike"), Skill));
            Skill = FCombatRoundSkill();
            Skill.Kind = ECombatRoundSkillKind::Projectile;
            Skill.Approach = ECombatRoundApproach::Tile;
            Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
            Skill.bRemainAtDestination = true;
            Skill.Power = 15.0f;
            Skill.bTargetOnly = false;
            Skill.SubActionPointCost = 1;
            Skills.Add(MakeSkill(Unit, TEXT("MoveShot"), Skill));
            Skills.Add(MakeSkill(Unit, TEXT("Wait"), WaitDefinition()));
            return Unit->ConfigureProfession(100.0f, 2, 2, Skills);
        }

        bool Initialize(int32 HumanCount = 1, float EnemySpeed = 10.0f, const FCombatRoundSkill* EnemySkill = nullptr, FIntPoint EnemyCoord = FIntPoint(0, 3), int32 EnemyCount = 1, const FCombatRoundSkill* FirstHumanSkill = nullptr, float FirstHumanSpeed = 20.0f, bool bUseFixtureSkills = true, bool bEnemyWithoutSkills = false, int32 FirstHumanMoveRange = 1)
        {
            if (!World.IsValid() || !Combat || !Arena || !Grid || Grid->TileMap.Num() != 16 || EnemyCount < 1 || EnemyCount > 4) return false;
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
                if (Index == 0 && !Unit->ConfigureMoveRange(FirstHumanMoveRange)) return false;
                if (Index == 0 && FirstHumanSkill)
                {
                    if (!GiveRoundSkill(Unit, FirstHumanSkill, HumanSkillId, bUseFixtureSkills)) return false;
                }
                else if (bUseFixtureSkills && !GiveFixtureSkills(Unit)) return false;
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), FirstHumanSpeed - Index * 2.0f);
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
                Member.ClassId = TEXT("Archer");
                Member.CharacterName = FText::FromString(Participant.AccountId.Subject);
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
                PartyActors.Add(Index, Unit);
            }
            Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
            for (int32 Index = 0; Index < EnemyCount; ++Index)
            {
                AUnitBase* Enemy = AddUnit(EnemyCoord + FIntPoint(Index, 0), ETeam::Enemy);
                if (!Enemy || (!bEnemyWithoutSkills && !GiveRoundSkill(Enemy, EnemySkill, EnemySkillId))) return false;
                Enemy->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), EnemySpeed);
                Enemies.Add(Enemy);
                Units.Add(Enemy);
            }
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
            Result.SkillId = SkillId(Unit, Skill);
            Result.DestinationCoord = Unit->GetCurrentTile()->GridCoord;
            Result.TargetCoord = Target ? Target->GetCurrentTile()->GridCoord : Result.DestinationCoord;
            Result.TargetUnitId = Target ? Target->UnitIndex : INDEX_NONE;
            return Result;
        }

        bool InitializeStandalone(bool bSelectedCharacterDead)
        {
            if (!World.IsValid() || !Combat || !Arena || !Grid || Grid->TileMap.Num() != 16) return false;
            APartyPlayerController* Controller = World->SpawnActor<APartyPlayerController>();
            if (!Controller) return false;
            World->AddController(Controller);
            Controller->SetAsLocalPlayerController();
            Controller->SetCombatContext(Combat, true);
            Controllers.Add(Controller);
            FRunIdentityData Identity;
            Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
            Identity.RunId = FGuid::NewGuid();
            Identity.HostEpoch = 1;
            FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("Development");
            Participant.AccountId.Subject = TEXT("round-single-owner");
            Identity.HostAccountId = Participant.AccountId;
            FCombatRoundSkill Skill;
            Skill.Kind = ECombatRoundSkillKind::GroundAttack;
            Skill.Approach = ECombatRoundApproach::None;
            Skill.HitRange = 1000.0f;
            Skill.Power = 7.0f;
            Skill.WindupSeconds = 0.1f;
            TArray<FRunPartyMember> Members;
            TMap<int32, TObjectPtr<AUnitBase>> PartyActors;
            TArray<AUnitBase*> Units;
            Humans.SetNumZeroed(2);
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunPartyMember& Member = Members.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.bCreated = true;
                Member.bPlayerControlled = Index == 1;
                Member.ClassId = TEXT("Archer");
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Single party %d"), Index));
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
                Member.CurrentHP = bSelectedCharacterDead && Member.bPlayerControlled ? 0.0f : 100.0f;
                if (Member.CurrentHP == 0.0f) continue;
                AUnitBase* Unit = AddUnit(FIntPoint(Index * 2, 0), ETeam::Player, APlayerUnit::StaticClass());
                if (!Unit || !GiveRoundSkill(Unit, &Skill, HumanSkillId, true)) return false;
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), 20.0f);
                Humans[Index] = Unit;
                PartyActors.Add(Index, Unit);
                Units.Add(Unit);
            }
            AUnitBase* Enemy = AddUnit(FIntPoint(0, 3), ETeam::Enemy);
            if (!Enemy || !GiveRoundSkill(Enemy, nullptr, EnemySkillId)) return false;
            Enemy->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), 0.0f);
            Enemies.Add(Enemy);
            Units.Add(Enemy);
            Combat->RegisterUnits(Units);
            UCombatActionAuthority* Authority = Combat->GetActionAuthority();
            int32 SelectedSlot = INDEX_NONE;
            if (!URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, SelectedSlot, Error) || !Authority->ConfigureRun(Identity, Members, PartyActors, Error)) return false;
            for (const TPair<int32, TObjectPtr<AUnitBase>>& Entry : PartyActors)
            {
                if (!Authority->SetPartyControlMode(Cast<APlayerUnit>(Entry.Value), Entry.Key == SelectedSlot ? EPartyControlMode::Human : EPartyControlMode::ServerAI, Error)) return false;
            }
            if (!Authority->BindParticipant(Controller, Participant.AccountId)) return false;
            Combat->StartCombat_Internal();
            Round = Combat->GetRoundCoordinator();
            return Round && Round->GetView().CombatId.IsValid() && Round->GetView().Phase == ECombatRoundPhase::Planning;
        }

        UBoxComponent* AddObstacle(FVector Location, FVector HalfExtent, ECollisionResponse Response = ECR_Block, ECollisionChannel ObjectType = ECC_WorldStatic)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            if (!Actor) return nullptr;
            UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Shape);
            Shape->SetBoxExtent(HalfExtent);
            Shape->SetCollisionObjectType(ObjectType);
            Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
            Shape->SetCollisionResponseToChannel(ECC_Pawn, Response);
            Shape->SetCollisionResponseToChannel(ECC_WorldDynamic, Response);
            Shape->RegisterComponent();
            Actor->SetActorLocation(Location);
            return Shape;
        }

        UBoxComponent* AddPawnSensor(AUnitBase* Unit, FVector RelativeLocation, FVector HalfExtent)
        {
            UBoxComponent* Sensor = NewObject<UBoxComponent>(Unit);
            Sensor->SetupAttachment(Unit->GetRootComponent());
            Sensor->SetRelativeLocation(RelativeLocation);
            Sensor->SetBoxExtent(HalfExtent);
            Sensor->SetCollisionObjectType(ECC_Pawn);
            Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Sensor->SetCollisionResponseToAllChannels(ECR_Block);
            Sensor->RegisterComponent();
            return Sensor;
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

        bool Move(int32 ControllerIndex, AUnitBase* Unit, FIntPoint Destination)
        {
            const FCombatRoundView& View = Round->GetView();
            return Round->SubmitMove(Controllers[ControllerIndex], View.CombatId, View.RoundNumber, View.PlanRevision, Unit->UnitIndex, Destination, Error);
        }

        bool CancelMove(int32 ControllerIndex, AUnitBase* Unit)
        {
            const FCombatRoundView& View = Round->GetView();
            return Round->CancelMove(Controllers[ControllerIndex], View.CombatId, View.RoundNumber, View.PlanRevision, Unit->UnitIndex, Error);
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
    TestTrue(TEXT("Attack offers a living enemy"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Strike")), Enemy->UnitIndex));
    TestFalse(TEXT("Attack excludes a living ally"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Strike")), Friend->UnitIndex));
    TestFalse(TEXT("Removed support actions have no selectable targets"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Guard"), Friend->UnitIndex));
    TestFalse(TEXT("Unknown source cannot produce target candidates"), Round->IsValidUnitTarget(INDEX_NONE, Fixture.SkillId(Source, TEXT("Strike")), Enemy->UnitIndex));
    TestFalse(TEXT("Unknown skill cannot produce target candidates"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Unknown"), Enemy->UnitIndex));
    TestFalse(TEXT("Wait has no unit target choices"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Wait")), Enemy->UnitIndex));
    TestFalse(TEXT("Ground attacks have no unit target choices"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("GroundStrike")), Enemy->UnitIndex));
    FText Error;
    TestTrue(TEXT("Wait is valid without a target"), Round->CanPlanCommand(Fixture.Command(Source, TEXT("Wait")), Error));
    FCombatRoundCommand Ground = Fixture.Command(Source, TEXT("GroundStrike"));
    Ground.TargetCoord = Enemy->GetCurrentTile()->GridCoord;
    Ground.DestinationCoord = FIntPoint(0, 2);
    TestTrue(TEXT("Ground attack uses valid coordinates without a unit target"), Round->CanPlanCommand(Ground, Error));
    TestTrue(TEXT("Other units' equipped actions do not replace the passive enemy's internal wait"), Round->GetView().Units.Last().Command.SkillId.IsNone());
    const FCombatRoundCommand Attack = Fixture.Command(Source, TEXT("Strike"), Enemy);
    Enemy->Die();
    TestFalse(TEXT("A dead enemy disappears from attack candidates before another plan is submitted"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Strike")), Enemy->UnitIndex));
    TestFalse(TEXT("A stale attack draft fails preview after target death"), Round->CanPlanCommand(Attack, Error));
    TestFalse(TEXT("The server also rejects the stale target"), Fixture.Submit(0, Attack));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundEquippedSkillsTest, "ProjectA.Combat.Round.OnlyEquippedSkillsAreSelectable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundEquippedSkillsTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FCombatRoundSkill Authored;
    Authored.Kind = ECombatRoundSkillKind::GroundAttack;
    Authored.Approach = ECombatRoundApproach::None;
    Authored.HitRange = 1000.0f;
    Authored.Power = 7.0f;
    Authored.WindupSeconds = 0.1f;
    FFixture Fixture;
    if (!TestTrue(TEXT("The real-content fixture starts with one equipped attack and an unarmed AI"), Fixture.Initialize(1, 10.0f, nullptr, FIntPoint(0, 3), 1, &Authored, 20.0f, false, true))) return false;
    AUnitBase* Human = Fixture.Humans[0];
    AUnitBase* Enemy = Fixture.Enemies[0];
    ACombatRoundCoordinator* Round = Fixture.Round;
    TestEqual(TEXT("The coordinator publishes only the actually equipped definition"), Round->GetSkills().Num(), 1);
    TestEqual(TEXT("The human has exactly its one equipped selection"), Round->GetView().Units[0].SkillIds.Num(), 1);
    TestTrue(TEXT("The published skill ID is the equipped DataAsset's primary ID"), Round->GetView().Units[0].SkillIds.Contains(Fixture.HumanSkillId) && Round->FindSkill(Fixture.HumanSkillId));
    TestTrue(TEXT("An unarmed AI receives no synthetic selectable skill"), Round->GetView().Units[1].SkillIds.IsEmpty() && Enemy->GetEquippedSkillDataAssets().IsEmpty());
    TestNull(TEXT("The private AI wait is absent from the selectable catalog"), Round->FindSkill(NAME_None));
    FText Error;
    const FCombatRoundCommand EnemyWait = Round->GetView().Units[1].Command;
    TestTrue(TEXT("The unarmed AI receives a ready internal wait"), EnemyWait.SkillId.IsNone() && Round->GetView().Units[1].bReady && Round->CanPlanCommand(EnemyWait, Error));
    TestFalse(TEXT("A human cannot submit the private AI wait"), Fixture.Submit(0, Fixture.Command(Human, NAME_None)));
    for (const TCHAR* RemovedId : {TEXT("Strike"), TEXT("Arrow"), TEXT("Guard"), TEXT("Wait"), TEXT("MoveShot"), TEXT("GroundStrike")})
    {
        TestNull(FString::Printf(TEXT("No implicit %s definition exists"), RemovedId), Round->FindSkill(FName(RemovedId)));
        const FCombatRoundCommand Removed = Fixture.Command(Human, FName(RemovedId), Enemy);
        TestFalse(FString::Printf(TEXT("Preview rejects unequipped %s"), RemovedId), Round->CanPlanCommand(Removed, Error));
        TestFalse(FString::Printf(TEXT("The server rejects unequipped %s"), RemovedId), Fixture.Submit(0, Removed));
    }
    if (!TestTrue(TEXT("The actual equipped attack is accepted"), Fixture.Submit(0, Fixture.Command(Human, Fixture.HumanSkillId, Enemy))) || !TestTrue(TEXT("The real attack locks alongside the internal AI wait"), Fixture.Ready(0))) return false;
    if (!TestTrue(TEXT("An AI without any equipped skill still settles the round"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("The single equipped attack applies its authored damage once"), Enemy->GetAttributeSet()->GetHP(), 93.0f);
    TestEqual(TEXT("The unarmed AI does not acquire a hidden attack"), Human->GetAttributeSet()->GetHP(), 100.0f);
    TestTrue(TEXT("The next round still exposes no added human or AI actions"), Round->GetSkills().Num() == 1 && Round->GetView().Units[0].SkillIds.Num() == 1 && Round->GetView().Units[1].SkillIds.IsEmpty());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundPlanningMoveTest, "ProjectA.Combat.Round.PlanningSUPMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundPlanningMoveTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Two-owner movement fixture initializes"), Fixture.Initialize(2))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    AUnitBase* Source = Fixture.Humans[0];
    AUnitBase* Friend = Fixture.Humans[1];
    AUnitBase* Enemy = Fixture.Enemies[0];
    ACombatGridTile* Origin = Source->GetCurrentTile();
    ACombatGridTile* Destination = Fixture.Grid->GetTileAtCoord(FIntPoint(1, 1));
    const FVector OriginalLocation = Source->GetActorLocation();
    const FRotator OriginalRotation = Source->GetActorRotation();
    if (!TestTrue(TEXT("Movement fixture starts with zero AP and one SAP"), Source->ConsumeActionPoint(2) && Source->ConsumeSubActionPoint(1))) return false;
    FText Error;
    TestTrue(TEXT("A diagonal empty allied tile is reachable with the default one-tile range"), Round->CanMoveUnit(Source->UnitIndex, Destination->GridCoord, Error));
    TestFalse(TEXT("The current tile is not a movement destination"), Round->CanMoveUnit(Source->UnitIndex, Origin->GridCoord, Error));
    TestFalse(TEXT("An empty tile beyond movement range is rejected"), Round->CanMoveUnit(Source->UnitIndex, FIntPoint(3, 1), Error));
    TestFalse(TEXT("An empty enemy-territory tile is rejected"), Round->CanMoveUnit(Source->UnitIndex, FIntPoint(0, 2), Error));
    TestFalse(TEXT("An occupied allied tile is rejected"), Round->CanMoveUnit(Source->UnitIndex, Friend->GetCurrentTile()->GridCoord, Error));
    TestFalse(TEXT("Another participant cannot move the original owner's character"), Fixture.Move(1, Source, Destination->GridCoord));
    TestEqual(TEXT("Rejected requests preserve the movement resource"), Source->GetCurrentSubActionPoint(), 1);
    if (!TestTrue(TEXT("The mover applies a zero-cost wait"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Wait")))) || !TestTrue(TEXT("The second owner applies a wait"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The second owner can ready before repositioning"), Fixture.Ready(1))) return false;
    TestTrue(TEXT("The second owner is ready while the mover is still planning"), Round->GetView().Units[1].bReady && Round->GetView().Phase == ECombatRoundPhase::Planning);
    const int32 Revision = Round->GetView().PlanRevision;
    if (!TestTrue(TEXT("The original owner can reserve movement with zero AP and one SAP"), Fixture.Move(0, Source, Destination->GridCoord))) return false;
    TestTrue(TEXT("The reservation is published without starting movement"), Round->GetView().Units[0].bHasMovePlan && Round->GetView().Units[0].MoveDestinationCoord == Destination->GridCoord && !Round->IsPlanningMoveInProgress() && Round->GetView().PlanRevision > Revision);
    TestEqual(TEXT("Reserving movement never consumes AP"), Source->GetCurrentActionPoint(), 0);
    TestEqual(TEXT("Reserving movement does not consume SAP"), Source->GetCurrentSubActionPoint(), 1);
    TestTrue(TEXT("Reserving movement clears only the editing owner's ready state"), !Round->GetView().Units[0].bReady && Round->GetView().Units[1].bReady);
    Round->Tick(0.5f);
    TestTrue(TEXT("An unready reservation leaves the actor and occupancy at its origin"), Source->GetActorLocation().Equals(OriginalLocation, 0.1f) && Source->GetCurrentTile() == Origin && Origin->GetOccupyingUnit() == Source && Round->GetView().Phase == ECombatRoundPhase::Planning);
    TestFalse(TEXT("A stale move request cannot replace the reservation"), Round->SubmitMove(Fixture.Controllers[0], Round->GetView().CombatId, Round->GetView().RoundNumber, Revision, Source->UnitIndex, FIntPoint(0, 1), Error));
    TestFalse(TEXT("Another participant cannot cancel the reservation"), Fixture.CancelMove(1, Source));
    if (!TestTrue(TEXT("The owner can edit the reservation"), Fixture.Move(0, Source, FIntPoint(0, 1)))) return false;
    TestTrue(TEXT("Editing changes only the destination, not the actor or resources"), Round->GetView().Units[0].MoveDestinationCoord == FIntPoint(0, 1) && Source->GetActorLocation().Equals(OriginalLocation, 0.1f) && Source->GetCurrentSubActionPoint() == 1);
    TestTrue(TEXT("Editing movement preserves the other participant's readiness"), Round->GetView().Units[1].bReady);
    TestFalse(TEXT("A stale cancellation cannot clear the edited reservation"), Round->CancelMove(Fixture.Controllers[0], Round->GetView().CombatId, Round->GetView().RoundNumber, Revision, Source->UnitIndex, Error));
    if (!TestTrue(TEXT("The owner can cancel before locking"), Fixture.CancelMove(0, Source))) return false;
    TestTrue(TEXT("Cancellation clears the reservation without moving or spending SAP"), !Round->GetView().Units[0].bHasMovePlan && Source->GetActorLocation().Equals(OriginalLocation, 0.1f) && Source->GetCurrentSubActionPoint() == 1);
    TestTrue(TEXT("Cancelling movement preserves the other participant's readiness"), Round->GetView().Units[1].bReady);
    if (!TestTrue(TEXT("The owner can reserve the original destination again"), Fixture.Move(0, Source, Destination->GridCoord)) || !TestTrue(TEXT("The first owner readies the reserved action"), Fixture.Ready(0))) return false;
    TestTrue(TEXT("Preserved teammate readiness lets the editing owner lock without another confirmation"), Round->IsPlanningMoveInProgress() && Source->GetActorLocation().Equals(OriginalLocation, 0.1f));
    TestTrue(TEXT("All participants ready starts the SAP stage inside resolving"), Round->IsPlanningMoveInProgress() && Round->GetView().Phase == ECombatRoundPhase::Resolving);
    TestEqual(TEXT("Locking consumes exactly one SAP for the move"), Source->GetCurrentSubActionPoint(), 0);
    TestEqual(TEXT("A movement and wait plan still works with zero AP"), Source->GetCurrentActionPoint(), 0);
    TestFalse(TEXT("Commands cannot change after locking"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Wait"))));
    TestFalse(TEXT("Ready cannot change after locking"), Fixture.Ready(0));
    TestFalse(TEXT("Movement cannot change after locking"), Fixture.Move(0, Source, FIntPoint(0, 1)));
    TestFalse(TEXT("Movement cannot be cancelled after locking"), Fixture.CancelMove(0, Source));
    Round->Tick(0.05f);
    TestTrue(TEXT("Movement advances through world space before reaching its destination"), !Source->GetActorLocation().Equals(OriginalLocation, 1.0f) && Round->IsPlanningMoveInProgress());
    TestTrue(TEXT("The origin remains occupied until movement completes"), Origin->GetOccupyingUnit() == Source && Source->GetCurrentTile() == Origin && Destination->GetOccupyingUnit() == nullptr);
    for (int32 Step = 0; Step < 200 && Round->IsPlanningMoveInProgress(); ++Step) Round->Tick(0.01f);
    if (!TestFalse(TEXT("The short movement finishes in bounded time"), Round->IsPlanningMoveInProgress())) return false;
    const FVector NewHome = Destination->GetActorLocation() + FVector(0.0f, 0.0f, OriginalLocation.Z);
    TestTrue(TEXT("Arrival transfers grid occupancy and the unit's tile together"), Origin->GetOccupyingUnit() == nullptr && Destination->GetOccupyingUnit() == Source && Source->GetCurrentTile() == Destination);
    TestTrue(TEXT("Arrival publishes the new home coordinate"), Round->GetView().Units[0].HomeCoord == Destination->GridCoord);
    TestTrue(TEXT("Arrival preserves height and restores the original facing"), Source->GetActorLocation().Equals(NewHome, 2.0f) && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
    TestFalse(TEXT("A second move cannot be added during the AP stage"), Round->CanMoveUnit(Source->UnitIndex, Origin->GridCoord, Error));
    if (!TestTrue(TEXT("Repositioning does not prevent the round from settling"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("Planning movement does not deal attack damage"), Enemy->GetAttributeSet()->GetHP(), 100.0f);
    if (!TestTrue(TEXT("A later attack starts from the repositioned home"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Enemy))) || !TestTrue(TEXT("The second owner waits for the attack"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The attacking owner readies"), Fixture.Ready(0)) || !TestTrue(TEXT("The waiting owner readies"), Fixture.Ready(1))) return false;
    if (!TestTrue(TEXT("The attack approaches, releases, and returns within a bounded round"), Fixture.AdvanceUntilNextRound(2))) return false;
    TestEqual(TEXT("The attack still applies its authored damage once"), Enemy->GetAttributeSet()->GetHP(), 75.0f);
    TestTrue(TEXT("The attack returns to the SAP destination rather than the previous home"), Source->GetActorLocation().Equals(NewHome, 2.0f) && Source->GetCurrentTile() == Destination && Round->GetView().Units[0].HomeCoord == Destination->GridCoord);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundPlanningMoveCancellationTest, "ProjectA.Combat.Round.PlanningSUPMovementCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundPlanningMoveCancellationTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Movement cancellation fixture initializes with a two-tile range"), Fixture.Initialize(1, 10.0f, nullptr, FIntPoint(0, 3), 1, nullptr, 20.0f, true, false, 2))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    AUnitBase* Source = Fixture.Humans[0];
    ACombatGridTile* Origin = Source->GetCurrentTile();
    ACombatGridTile* Destination = Fixture.Grid->GetTileAtCoord(FIntPoint(2, 1));
    FText Error;
    TestTrue(TEXT("The destination is reachable through empty allied tiles"), Round->CanMoveUnit(Source->UnitIndex, Destination->GridCoord, Error));
    AUnitBase* BlockerA = Fixture.AddUnit(FIntPoint(1, 0), ETeam::Player);
    AUnitBase* BlockerB = Fixture.AddUnit(FIntPoint(1, 1), ETeam::Player);
    if (!TestNotNull(TEXT("The first occupied-path blocker exists"), BlockerA) || !TestNotNull(TEXT("The second occupied-path blocker exists"), BlockerB)) return false;
    TestFalse(TEXT("Occupied intermediate tiles block every BFS route to an empty destination"), Round->CanMoveUnit(Source->UnitIndex, Destination->GridCoord, Error));
    TestFalse(TEXT("The server also rejects the blocked route"), Fixture.Move(0, Source, Destination->GridCoord));
    BlockerB->SetCurrentTile(Fixture.Grid->GetTileAtCoord(FIntPoint(3, 1)));
    TestTrue(TEXT("Opening one diagonal route restores reachability"), Round->CanMoveUnit(Source->UnitIndex, Destination->GridCoord, Error));
    const FVector OriginalLocation = Source->GetActorLocation();
    const FRotator OriginalRotation = Source->GetActorRotation();
    const int32 OriginalSAP = Source->GetCurrentSubActionPoint();
    if (!TestTrue(TEXT("The mover applies its AP wait"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Wait")))) || !TestTrue(TEXT("The reachable route can be reserved"), Fixture.Move(0, Source, Destination->GridCoord)) || !TestTrue(TEXT("Ready begins the reserved route"), Fixture.Ready(0))) return false;
    Round->Tick(0.05f);
    if (!TestTrue(TEXT("Suspension happens during real movement"), Round->IsPlanningMoveInProgress() && !Source->GetActorLocation().Equals(OriginalLocation, 1.0f))) return false;
    Round->SuspendRound();
    TestTrue(TEXT("Suspension ends the active move"), Round->GetView().Phase == ECombatRoundPhase::Suspended && !Round->IsPlanningMoveInProgress());
    TestTrue(TEXT("Interrupted movement restores the original position and facing"), Source->GetActorLocation().Equals(OriginalLocation, 0.1f) && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
    TestTrue(TEXT("Interrupted movement retains its original home and occupancy"), Source->GetCurrentTile() == Origin && Origin->GetOccupyingUnit() == Source && Destination->GetOccupyingUnit() == nullptr && Round->GetView().Units[0].HomeCoord == Origin->GridCoord);
    TestEqual(TEXT("An interrupted locked move does not refund the spent SAP"), Source->GetCurrentSubActionPoint(), OriginalSAP - 1);
    FFixture Standalone;
    if (!TestTrue(TEXT("The standalone companion fixture initializes"), Standalone.InitializeStandalone(false))) return false;
    AUnitBase* Companion = Standalone.Humans[0];
    TestFalse(TEXT("An AI companion has no human SAP move preview"), Standalone.Round->CanMoveUnit(Companion->UnitIndex, FIntPoint(1, 1), Error));
    TestFalse(TEXT("The local owner cannot reserve movement for an AI companion"), Standalone.Move(0, Companion, FIntPoint(1, 1)));
    TestFalse(TEXT("The local owner cannot cancel an AI companion's movement"), Standalone.CancelMove(0, Companion));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMoveBudgetTest, "ProjectA.Combat.Round.ReservedMovementBudgetsAndConflicts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMoveBudgetTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FCombatRoundSkill Attack;
    Attack.SubActionPointCost = 1;
    FFixture Fixture;
    if (!TestTrue(TEXT("The movement budget fixture initializes"), Fixture.Initialize(2, 10.0f, nullptr, FIntPoint(0, 3), 1, &Attack))) return false;
    AUnitBase* Source = Fixture.Humans[0];
    AUnitBase* Friend = Fixture.Humans[1];
    AUnitBase* Enemy = Fixture.Enemies[0];
    ACombatRoundCoordinator* Round = Fixture.Round;
    const FIntPoint Destination(1, 1);
    const FCombatRoundCommand AuthoredAttack = Fixture.Command(Source, Fixture.HumanSkillId, Enemy);
    if (!TestTrue(TEXT("The fixture can retain only one SAP"), Source->ConsumeSubActionPoint(1)) || !TestTrue(TEXT("The authored SAP attack is affordable without a move"), Fixture.Submit(0, AuthoredAttack))) return false;
    FText Error;
    TestFalse(TEXT("Move preview includes the already planned attack's SAP cost"), Round->CanMoveUnit(Source->UnitIndex, Destination, Error));
    TestFalse(TEXT("The server rejects an attack-plus-move budget above available SAP"), Fixture.Move(0, Source, Destination));
    TestTrue(TEXT("An over-budget reservation preserves the existing attack and SAP"), !Round->GetView().Units[0].bHasMovePlan && SameCommand(Round->GetView().Units[0].Command, AuthoredAttack) && Source->GetCurrentSubActionPoint() == 1);
    if (!TestTrue(TEXT("A free wait releases the attack's reserved SAP budget"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Wait")))) || !TestTrue(TEXT("The freed SAP can reserve movement"), Fixture.Move(0, Source, Destination))) return false;
    TestFalse(TEXT("Attack preview includes an existing movement reservation"), Round->CanPlanCommand(AuthoredAttack, Error));
    TestFalse(TEXT("The reverse submission order also rejects the combined excess cost"), Fixture.Submit(0, AuthoredAttack));
    Source->ResetSubActionPoint();
    if (!TestTrue(TEXT("Two SAP cover both the movement and authored attack"), Round->CanPlanCommand(AuthoredAttack, Error)) || !TestTrue(TEXT("The combined affordable plan is accepted"), Fixture.Submit(0, AuthoredAttack))) return false;
    TestTrue(TEXT("Planning reserves both costs without spending either resource"), Round->GetView().Units[0].bHasMovePlan && Source->GetCurrentActionPoint() == 2 && Source->GetCurrentSubActionPoint() == 2);
    TestFalse(TEXT("A second participant cannot reserve the same SAP destination"), Fixture.Move(1, Friend, Destination));
    TestFalse(TEXT("The mover cannot exchange places with another unit's reserved home"), Fixture.Move(0, Source, Friend->GetCurrentTile()->GridCoord));
    TestFalse(TEXT("The other owner cannot reserve the mover's home while its move is pending"), Fixture.Move(1, Friend, Source->GetCurrentTile()->GridCoord));
    FCombatRoundCommand ReturningAttack = Fixture.Command(Friend, TEXT("GroundStrike"), Enemy);
    ReturningAttack.DestinationCoord = Source->GetCurrentTile()->GridCoord;
    TestFalse(TEXT("A returning tile attack cannot enter another unit's home"), Fixture.Submit(1, ReturningAttack));
    ReturningAttack.DestinationCoord = Destination;
    TestFalse(TEXT("A returning tile attack cannot enter another SAP destination"), Fixture.Submit(1, ReturningAttack));
    FCombatRoundCommand ResidentAttack = Fixture.Command(Friend, TEXT("MoveShot"), Enemy);
    ResidentAttack.DestinationCoord = Destination;
    TestFalse(TEXT("An AP action that remains on a reserved SAP destination is rejected"), Fixture.Submit(1, ResidentAttack));
    if (!TestTrue(TEXT("Cancelling the first move releases its destination"), Fixture.CancelMove(0, Source)) || !TestTrue(TEXT("A returning tile action can reserve the released empty tile"), Fixture.Submit(1, ReturningAttack))) return false;
    TestFalse(TEXT("A SAP move cannot enter another returning AP destination"), Fixture.Move(0, Source, Destination));
    if (!TestTrue(TEXT("The resident AP action can replace its owner's returning destination"), Fixture.Submit(1, ResidentAttack))) return false;
    TestFalse(TEXT("A SAP move cannot claim another unit's resident AP destination"), Fixture.Move(0, Source, Destination));
    if (!TestTrue(TEXT("Replacing the resident action releases its destination"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The SAP destination becomes reservable again"), Fixture.Move(0, Source, Destination)) || !TestTrue(TEXT("The first participant readies both affordable costs"), Fixture.Ready(0)) || !TestTrue(TEXT("The second participant locks the round"), Fixture.Ready(1))) return false;
    TestEqual(TEXT("Locking charges the authored AP cost once"), Source->GetCurrentActionPoint(), 1);
    TestEqual(TEXT("Locking charges movement SAP and attack SAP together"), Source->GetCurrentSubActionPoint(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMoveBarrierTest, "ProjectA.Combat.Round.MovementBarrierBeforeAttacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMoveBarrierTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("The two-owner movement barrier fixture initializes"), Fixture.Initialize(2))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    AUnitBase* Source = Fixture.Humans[0];
    AUnitBase* Friend = Fixture.Humans[1];
    AUnitBase* Enemy = Fixture.Enemies[0];
    ACombatGridTile* SourceDestination = Fixture.Grid->GetTileAtCoord(FIntPoint(1, 1));
    ACombatGridTile* FriendDestination = Fixture.Grid->GetTileAtCoord(FIntPoint(3, 1));
    const FVector NewHome = SourceDestination->GetActorLocation() + FVector(0.0f, 0.0f, Source->GetActorLocation().Z);
    Source->GetCharacterMovement()->MaxWalkSpeed = 100.0f;
    if (!TestTrue(TEXT("The first participant applies an attack"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Enemy))) || !TestTrue(TEXT("The second participant applies a wait"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The first participant reserves a move"), Fixture.Move(0, Source, SourceDestination->GridCoord)) || !TestTrue(TEXT("The second participant reserves a distinct move"), Fixture.Move(1, Friend, FriendDestination->GridCoord)) || !TestTrue(TEXT("The first participant readies"), Fixture.Ready(0)) || !TestTrue(TEXT("The last participant locks the staged round"), Fixture.Ready(1))) return false;
    TestTrue(TEXT("The round begins with its reserved SAP stage"), Round->IsPlanningMoveInProgress());
    TestEqual(TEXT("AP timing starts at zero before any SAP movement"), Round->GetView().ElapsedSeconds, 0.0f);
    const FVector MoveStart = Source->GetActorLocation();
    Round->Tick(0.1f);
    TestTrue(TEXT("SAP movement covers 35 centimeters in 0.1 seconds despite a different MaxWalkSpeed"), FMath::IsNearlyEqual(FVector::Dist2D(MoveStart, Source->GetActorLocation()), 35.0, 0.01));
    Source->GetCharacterMovement()->MaxWalkSpeed = 0.0f;
    Source->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), 40.0f);
    const FVector BeforeAttributeChange = Source->GetActorLocation();
    Round->Tick(0.1f);
    TestTrue(TEXT("Changing Dexterity and disabling MaxWalkSpeed keeps SAP movement at 350 centimeters per second"), FMath::IsNearlyEqual(FVector::Dist2D(BeforeAttributeChange, Source->GetActorLocation()), 35.0, 0.01) && FMath::IsNearlyEqual(Source->GetVelocity().Size2D(), 350.0, 0.01));
    bool bObservedFirstArrival = false;
    bool bAPClockPaused = true;
    bool bNoEarlyDamage = true;
    bool bFirstArrivalWaited = true;
    for (int32 Step = 0; Step < 600 && Round->IsPlanningMoveInProgress(); ++Step)
    {
        Round->Tick(0.01f);
        if (!Round->IsPlanningMoveInProgress()) break;
        bAPClockPaused &= Round->GetView().ElapsedSeconds == 0.0f;
        bNoEarlyDamage &= Enemy->GetAttributeSet()->GetHP() == 100.0f;
        if (Source->GetCurrentTile() == SourceDestination && Friend->GetCurrentTile() != FriendDestination)
        {
            bObservedFirstArrival = true;
            bFirstArrivalWaited &= Source->GetActorLocation().Equals(NewHome, 2.0f);
        }
    }
    TestTrue(TEXT("SAP movement does not advance the AP simulation clock"), bAPClockPaused);
    TestTrue(TEXT("No attack releases before every SAP movement ends"), bNoEarlyDamage);
    TestTrue(TEXT("The first arrival waits at its new home for the remaining move"), bFirstArrivalWaited);
    if (!TestTrue(TEXT("One participant arrives while the other is still moving"), bObservedFirstArrival) || !TestFalse(TEXT("Every reserved move finishes in bounded time"), Round->IsPlanningMoveInProgress())) return false;
    TestTrue(TEXT("The AP stage starts only after both new homes are occupied"), Source->GetCurrentTile() == SourceDestination && Friend->GetCurrentTile() == FriendDestination && SourceDestination->GetOccupyingUnit() == Source && FriendDestination->GetOccupyingUnit() == Friend);
    TestEqual(TEXT("Completing movement does not release the wound-up attack immediately"), Enemy->GetAttributeSet()->GetHP(), 100.0f);
    TestTrue(TEXT("AP speed delays remain based on the planned Dexterity values"), FMath::IsNearlyEqual(Round->GetView().Units[0].StartDelay, 0.0f) && FMath::IsNearlyEqual(Round->GetView().Units[1].StartDelay, 0.2f) && FMath::IsNearlyEqual(Round->GetView().Units[2].StartDelay, 1.0f));
    Round->Tick(0.05f);
    TestTrue(TEXT("The attack approaches its enemy only after the complete movement stage"), !Source->GetActorLocation().Equals(NewHome, 2.0f));
    if (!TestTrue(TEXT("The staged movement and attack round settles"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("The staged attack applies its damage exactly once"), Enemy->GetAttributeSet()->GetHP(), 75.0f);
    TestTrue(TEXT("The attack returns to the home established by its SAP move"), Source->GetActorLocation().Equals(NewHome, 2.0f) && Source->GetCurrentTile() == SourceDestination && Round->GetView().Units[0].HomeCoord == SourceDestination->GridCoord);
    TestTrue(TEXT("The next round clears both old movement reservations"), !Round->GetView().Units[0].bHasMovePlan && !Round->GetView().Units[1].bHasMovePlan);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMoveFailureTest, "ProjectA.Combat.Round.MovementFailureDoesNotBlockSurvivors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMoveFailureTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 2; ++Case)
    {
        FFixture Fixture;
        if (!TestTrue(TEXT("The failed-movement fixture initializes"), Fixture.Initialize(2))) return false;
        ACombatRoundCoordinator* Round = Fixture.Round;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Friend = Fixture.Humans[1];
        AUnitBase* Enemy = Fixture.Enemies[0];
        ACombatGridTile* Origin = Source->GetCurrentTile();
        const FVector OriginalLocation = Source->GetActorLocation();
        const FCombatRoundCommand FriendCommand = Fixture.Command(Friend, Case == 0 ? FName(TEXT("Wait")) : FName(TEXT("Strike")), Case == 0 ? nullptr : Enemy);
        if (!TestTrue(TEXT("The mover reserves an attack"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Enemy))) || !TestTrue(TEXT("The second participant applies its action"), Fixture.Submit(1, FriendCommand)) || !TestTrue(TEXT("The mover reserves an allied destination"), Fixture.Move(0, Source, FIntPoint(1, 1))) || !TestTrue(TEXT("The mover readies"), Fixture.Ready(0)) || !TestTrue(TEXT("The second participant locks the stage"), Fixture.Ready(1))) return false;
        Round->Tick(0.05f);
        if (!TestTrue(TEXT("The failure occurs during actual SAP movement"), Round->IsPlanningMoveInProgress() && !Source->GetActorLocation().Equals(OriginalLocation, 1.0f))) return false;
        if (Case == 0)
        {
            if (!TestNotNull(TEXT("A new occupant blocks the destination after movement starts"), Fixture.AddUnit(FIntPoint(1, 1), ETeam::Player))) return false;
        }
        else Source->Die();
        Round->Tick(0.01f);
        TestFalse(TEXT("A failed or dead mover does not block the AP stage"), Round->IsPlanningMoveInProgress());
        TestTrue(TEXT("The AP stage preserves the failed movement's no-refund explanation"), Round->GetView().Message.ToString().Contains(TEXT("환불")));
        if (Case == 0)
        {
            TestTrue(TEXT("A living failed mover returns to its reserved origin before its attack"), Source->GetActorLocation().Equals(OriginalLocation, 0.1f) && Source->GetCurrentTile() == Origin && Origin->GetOccupyingUnit() == Source);
            TestEqual(TEXT("Failure preserves the already charged movement SAP"), Source->GetCurrentSubActionPoint(), 1);
        }
        else TestTrue(TEXT("The dead mover's planned AP action is cancelled"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Cancelled);
        if (!TestTrue(TEXT("Surviving AP actions finish despite the movement failure"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestEqual(TEXT("Exactly the living planned attack applies damage"), Enemy->GetAttributeSet()->GetHP(), 75.0f);
        if (Case == 0) TestTrue(TEXT("The failed mover's attack returns to the restored original home"), Source->GetActorLocation().Equals(OriginalLocation, 2.0f) && Source->GetCurrentTile() == Origin && Round->GetView().Units[0].HomeCoord == Origin->GridCoord);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundLocomotionInputsTest, "ProjectA.Combat.Round.LocomotionAnimationInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundLocomotionInputsTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    // Verify the native inputs consumed by the animation blueprint, not animation playback or visual quality.
    // 애니메이션 재생이나 시각 품질이 아니라 애니메이션 블루프린트가 사용하는 네이티브 입력을 검증합니다.
    const auto HasMotionInputs = [](AUnitBase* Unit)
    {
        const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
        return Movement->Velocity.SizeSquared2D() > 1.0f && Movement->GetCurrentAcceleration().SizeSquared2D() > 1.0f && FVector::DotProduct(Movement->Velocity, Movement->GetCurrentAcceleration()) > 0.0f;
    };
    const auto HasStoppedInputs = [](AUnitBase* Unit)
    {
        const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
        return Movement->Velocity.IsNearlyZero(0.01f) && Movement->GetCurrentAcceleration().IsNearlyZero(0.01f);
    };
    {
        FFixture Fixture;
        if (!TestTrue(TEXT("The locomotion input fixture initializes"), Fixture.Initialize(2))) return false;
        ACombatRoundCoordinator* Round = Fixture.Round;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Friend = Fixture.Humans[1];
        if (!TestTrue(TEXT("The first participant plans an attack"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Fixture.Enemies[0]))) || !TestTrue(TEXT("The second participant plans a wait"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The first participant reserves SAP movement"), Fixture.Move(0, Source, FIntPoint(1, 1))) || !TestTrue(TEXT("The second participant reserves SAP movement"), Fixture.Move(1, Friend, FIntPoint(3, 1))) || !TestTrue(TEXT("The first participant readies"), Fixture.Ready(0)) || !TestTrue(TEXT("The last participant starts resolution"), Fixture.Ready(1))) return false;
        Round->Tick(0.05f);
        if (!TestTrue(TEXT("SAP movement supplies velocity and forward acceleration together"), Round->IsSAPMovementInProgress() && HasMotionInputs(Source))) return false;
        TestTrue(TEXT("A participant still waiting for SAP movement has stopped inputs"), HasStoppedInputs(Friend));
        for (int32 Step = 0; Step < 200 && Source->GetCurrentTile()->GridCoord != FIntPoint(1, 1); ++Step) Round->Tick(0.01f);
        if (!TestTrue(TEXT("The first mover arrives while another SAP move is pending"), Source->GetCurrentTile()->GridCoord == FIntPoint(1, 1) && Round->IsSAPMovementInProgress())) return false;
        TestTrue(TEXT("Arrival clears both motion inputs while waiting for other movers"), HasStoppedInputs(Source));
        Round->Tick(0.05f);
        TestTrue(TEXT("The next SAP mover receives both motion inputs"), HasMotionInputs(Friend));
        TestTrue(TEXT("The next participant uses the same fixed SAP speed"), FMath::IsNearlyEqual(Friend->GetVelocity().Size2D(), 350.0, 0.01));
        TestTrue(TEXT("The arrived participant stays stopped during the next move"), HasStoppedInputs(Source));
        for (int32 Step = 0; Step < 600 && Round->IsSAPMovementInProgress(); ++Step) Round->Tick(0.01f);
        if (!TestFalse(TEXT("SAP motion settles before the attack phase"), Round->IsSAPMovementInProgress())) return false;
        Round->Tick(0.05f);
        TestTrue(TEXT("AP approach supplies both locomotion inputs"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Approaching && HasMotionInputs(Source));
        for (int32 Step = 0; Step < 200 && Round->GetView().Units[0].ActionPhase != ECombatRoundActionPhase::Casting; ++Step) Round->Tick(0.01f);
        if (!TestTrue(TEXT("The attack reaches its windup"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Casting)) return false;
        TestTrue(TEXT("Casting clears velocity and acceleration together"), HasStoppedInputs(Source));
        for (int32 Step = 0; Step < 100 && Round->GetView().Units[0].ActionPhase != ECombatRoundActionPhase::Returning; ++Step) Round->Tick(0.01f);
        if (!TestTrue(TEXT("The released attack enters return movement"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning)) return false;
        Round->Tick(0.01f);
        TestTrue(TEXT("AP return supplies both locomotion inputs"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning && HasMotionInputs(Source));
        if (!TestTrue(TEXT("The completed return reaches the next planning phase"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(TEXT("Completed SAP and AP movement leave both participants stopped"), HasStoppedInputs(Source) && HasStoppedInputs(Friend));
    }
    for (int32 Case = 0; Case < 2; ++Case)
    {
        FFixture Fixture;
        if (!TestTrue(TEXT("The interrupted locomotion fixture initializes"), Fixture.Initialize(2))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        if (!TestTrue(TEXT("The mover plans an attack before interruption"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Fixture.Enemies[0]))) || !TestTrue(TEXT("The other participant waits before interruption"), Fixture.Submit(1, Fixture.Command(Fixture.Humans[1], TEXT("Wait")))) || !TestTrue(TEXT("The mover reserves its route before interruption"), Fixture.Move(0, Source, FIntPoint(1, 1))) || !TestTrue(TEXT("The mover readies before interruption"), Fixture.Ready(0)) || !TestTrue(TEXT("The other participant locks the movement"), Fixture.Ready(1))) return false;
        Fixture.Round->Tick(0.05f);
        if (!TestTrue(TEXT("Interruption happens with active movement inputs"), Fixture.Round->IsSAPMovementInProgress() && HasMotionInputs(Source))) return false;
        if (Case == 0) Fixture.Round->SuspendRound();
        else Source->Die();
        TestTrue(Case == 0 ? TEXT("Suspension immediately clears velocity and acceleration") : TEXT("Death immediately clears velocity and acceleration"), HasStoppedInputs(Source));
    }
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
    ACombatGridTile* ApproachTile = Fixture.Grid->GetTileAtCoord(EnemyPlan.DestinationCoord);
    TestTrue(TEXT("AI approaches a vacant tile while preserving the target's return home"), ApproachTile && !ApproachTile->GetOccupyingUnit() && EnemyPlan.DestinationCoord != NearestEnemyHome);
    TestTrue(TEXT("AI plan is ready before human edits"), Round->GetView().Units.Last().bReady);
    FText Error;
    TestTrue(TEXT("The generated enemy plan passes the same planning validation"), Round->CanPlanCommand(EnemyPlan, Error));
    if (!TestTrue(TEXT("Host applies a human wait"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("Wait"))))) return false;
    TestTrue(TEXT("First human edit leaves the enemy plan fixed"), SameCommand(EnemyPlan, Round->GetView().Units.Last().Command));
    if (!TestTrue(TEXT("Guest applies a human wait"), Fixture.Submit(1, Fixture.Command(Fixture.Humans[1], TEXT("Wait"))))) return false;
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
    FCombatRoundSkill Skill;
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.Approach = ECombatRoundApproach::Tile;
    Skill.bRemainAtDestination = true;
    FFixture Fixture;
    if (!TestTrue(TEXT("Deferred AI tactic fixture initializes"), Fixture.Initialize(1, 10, &Skill))) return false;
    TestTrue(TEXT("Authored resident movement leaves only an internal AI wait"), Fixture.Round->GetView().Units.Last().Command.SkillId.IsNone());
    Skill.SkillId = TEXT("RemovedSupport");
    Skill.Kind = static_cast<ECombatRoundSkillKind>(3);
    Skill.Approach = ECombatRoundApproach::None;
    Skill.bRemainAtDestination = false;
    TestFalse(TEXT("The removed support enum value cannot become a different valid action"), CombatRoundRules::IsValidSkill(Skill));
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
    if (!TestTrue(TEXT("A ready owner can edit its own plan"), Fixture.Submit(0, FirstPlan))) return false;
    TestFalse(TEXT("The editing owner's readiness is invalidated"), Round->GetView().Units[0].bReady);
    if (!TestTrue(TEXT("The editing owner can ready again"), Fixture.Ready(0)) || !TestTrue(TEXT("Guest edits its own plan"), Fixture.Submit(1, Fixture.Command(Fixture.Humans[1], TEXT("Wait"))))) return false;
    TestTrue(TEXT("Earlier teammate readiness is preserved"), Round->GetView().Units[0].bReady && !Round->GetView().Units[1].bReady);
    TestTrue(TEXT("Human edits never change the fixed enemy command"), SameCommand(EnemyPlan, Round->GetView().Units.Last().Command));
    if (!Fixture.Ready(1)) return false;
    TestTrue(TEXT("All ready transitions to real-time resolution"), Round->GetView().Phase == ECombatRoundPhase::Resolving);
    TestFalse(TEXT("Locked plans reject even current-revision edits"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], TEXT("Strike"), Fixture.Enemies[0])));
    TestTrue(TEXT("Highest speed begins at zero"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Complete);
    Round->Tick(0.19f);
    TestTrue(TEXT("Two speed points preserve a 0.2 second delay"), Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Waiting);
    Round->Tick(0.02f);
    TestTrue(TEXT("Slower action begins after its configured delay"), Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Complete);
    TestTrue(TEXT("Resolution does not mutate the locked command"), SameCommand(FirstPlan, Round->GetView().Units[0].Command));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundDexterityScheduleTest, "ProjectA.Combat.Round.DexteritySchedulesRoundActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundDexterityScheduleTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("The ten versus five Dexterity fixture initializes"), Fixture.Initialize(1, 5.0f, nullptr, FIntPoint(0, 3), 1, nullptr, 10.0f))) return false;
    ACombatRoundCoordinator* Round = Fixture.Round;
    AUnitBase* Human = Fixture.Humans[0];
    AUnitBase* Enemy = Fixture.Enemies[0];
    TestEqual(TEXT("One human Dexterity point provides one combat speed point"), Human->GetCombatSpeed(), 10.0f);
    TestEqual(TEXT("Enemy combat speed uses its own Dexterity"), Enemy->GetCombatSpeed(), 5.0f);
    TestEqual(TEXT("The planning view exposes human Dexterity as speed"), Round->GetView().Units[0].Speed, 10.0f);
    TestEqual(TEXT("The planning view exposes enemy Dexterity as speed"), Round->GetView().Units[1].Speed, 5.0f);
    TestEqual(TEXT("The fastest planned action starts immediately"), Round->GetView().Units[0].StartDelay, 0.0f);
    TestEqual(TEXT("A five-point Dexterity difference schedules half a second"), Round->GetView().Units[1].StartDelay, 0.5f);

    // Attribute changes take effect in the next planning snapshot, preserving already scheduled actions.
    // 어트리뷰트 변경은 다음 계획 스냅샷에 반영하여 이미 예약한 행동 시각을 유지합니다.
    Human->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), 21.25f);
    TestEqual(TEXT("Live combat speed preserves fractional Dexterity"), Human->GetCombatSpeed(), 21.25f);
    if (!TestTrue(TEXT("The owner submits a wait through the public planning API"), Fixture.Submit(0, Fixture.Command(Human, TEXT("Wait")))) || !TestTrue(TEXT("The owner locks the planned actions"), Fixture.Ready(0))) return false;
    TestEqual(TEXT("Planning edits do not replace the human speed snapshot"), Round->GetView().Units[0].Speed, 10.0f);
    TestTrue(TEXT("The scheduled fastest wait completes at time zero"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Complete);
    TestTrue(TEXT("The slower enemy remains scheduled"), Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Waiting);
    Enemy->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), 10.0f);
    Round->Tick(0.49f);
    TestEqual(TEXT("Resolution keeps the original enemy speed snapshot"), Round->GetView().Units[1].Speed, 5.0f);
    TestEqual(TEXT("Resolution keeps the original half-second start time"), Round->GetView().Units[1].StartDelay, 0.5f);
    TestTrue(TEXT("The enemy has not started before its half-second deadline"), Round->GetView().RoundNumber == 1 && Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Waiting);
    Round->Tick(0.02f);
    if (!TestTrue(TEXT("The enemy wait settles the round immediately after its original deadline"), Round->GetView().RoundNumber == 2 && Round->GetView().Phase == ECombatRoundPhase::Planning)) return false;
    TestEqual(TEXT("The next planning snapshot keeps all fractional human Dexterity"), Round->GetView().Units[0].Speed, 21.25f);
    TestEqual(TEXT("The next planning snapshot includes the enemy Dexterity change"), Round->GetView().Units[1].Speed, 10.0f);
    TestEqual(TEXT("Fractional Dexterity contributes to the next scheduled delay"), Round->GetView().Units[1].StartDelay, 1.125f);
    if (!TestTrue(TEXT("The owner submits the next wait"), Fixture.Submit(0, Fixture.Command(Human, TEXT("Wait")))) || !TestTrue(TEXT("The next round locks successfully"), Fixture.Ready(0))) return false;
    Round->Tick(0.5f);
    Round->Tick(0.5f);
    Round->Tick(0.12f);
    TestTrue(TEXT("Fractional speed is not truncated to an earlier 1.1 second action"), Round->GetView().RoundNumber == 2 && Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Waiting);
    Round->Tick(0.02f);
    TestTrue(TEXT("The next action starts on the simulation step after 1.125 seconds"), Round->GetView().RoundNumber == 3 && Round->GetView().Phase == ECombatRoundPhase::Planning);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundStandaloneCompanionsTest, "ProjectA.Combat.Round.StandaloneCompanionsKeepAIControl", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundStandaloneCompanionsTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("One local owner starts with a selected character and an AI companion"), Fixture.InitializeStandalone(false))) return false;
    AUnitBase* Companion = Fixture.Humans[0];
    AUnitBase* Human = Fixture.Humans[1];
    UCombatActionAuthority* Authority = Fixture.Combat->GetActionAuthority();
    const FGuid CompanionCharacter = Authority->GetCharacterId(Companion);
    const FRunAccountId CompanionOwner = Authority->GetOwnerAccountId(Companion);
    TestTrue(TEXT("Both characters retain the same original owner"), CompanionOwner == Authority->GetOwnerAccountId(Human));
    TestTrue(TEXT("Only the explicitly selected later slot is directly controllable"), Authority->CanControllerControl(Fixture.Controllers[0], Human) && !Authority->CanControllerControl(Fixture.Controllers[0], Companion));
    TestTrue(TEXT("The companion already has a ready AI plan before player input"), Fixture.Round->GetView().Units[0].OwnerSlot == 0 && Fixture.Round->GetView().Units[0].bReady);
    TestEqual(TEXT("The selected character belongs to the sole input participant"), Fixture.Round->GetView().Units[1].OwnerSlot, 1);
    const FCombatRoundCommand CompanionPlan = Fixture.Round->GetView().Units[0].Command;
    const int32 Revision = Fixture.Round->GetView().PlanRevision;
    TestFalse(TEXT("The original owner cannot submit commands for an AI companion"), Fixture.Submit(0, Fixture.Command(Companion, TEXT("Wait"))));
    TestTrue(TEXT("Rejected companion input preserves its AI plan and revision"), SameCommand(CompanionPlan, Fixture.Round->GetView().Units[0].Command) && Fixture.Round->GetView().PlanRevision == Revision);
    if (!TestTrue(TEXT("The player submits only the selected character's wait"), Fixture.Submit(0, Fixture.Command(Human, TEXT("Wait")))) || !TestTrue(TEXT("One human readiness resolves the entire party"), Fixture.Ready(0))) return false;
    if (!TestTrue(TEXT("The first round settles through the real AI action"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("The companion independently damages the opposing team once"), Fixture.Enemies[0]->GetAttributeSet()->GetHP(), 93.0f);

    Human->Die();
    Fixture.Round->Tick(0.01f);
    TestTrue(TEXT("Surviving AI starts the next round without readiness from the dead character"), Fixture.Round->GetView().Phase == ECombatRoundPhase::Resolving);
    TestFalse(TEXT("Death never transfers human input to the companion"), Authority->CanControllerControl(Fixture.Controllers[0], Companion));
    if (!TestTrue(TEXT("AI-only survivors complete another round without human input"), Fixture.AdvanceUntilNextRound(2))) return false;
    TestEqual(TEXT("The surviving companion continues attacking once per round"), Fixture.Enemies[0]->GetAttributeSet()->GetHP(), 86.0f);
    TestTrue(TEXT("Continued AI combat preserves original character ownership"), Authority->GetCharacterId(Companion) == CompanionCharacter && Authority->GetOwnerAccountId(Companion) == CompanionOwner && CastChecked<APlayerUnit>(Companion)->IsServerAIControlled());

    // A following encounter omits the dead selected actor but must not promote another party member.
    // 다음 전투에서 사망한 선택 액터가 생성되지 않아도 다른 파티원을 인간 조작으로 승격하지 않습니다.
    FFixture FollowingEncounter;
    if (!TestTrue(TEXT("A fresh encounter initializes with the chosen character already dead"), FollowingEncounter.InitializeStandalone(true))) return false;
    TestNull(TEXT("The dead selected character is not spawned"), FollowingEncounter.Humans[1]);
    TestTrue(TEXT("The fresh companion retains AI control with no input owner slot"), CastChecked<APlayerUnit>(FollowingEncounter.Humans[0])->IsServerAIControlled() && FollowingEncounter.Round->GetView().Units[0].OwnerSlot == 0);
    TestFalse(TEXT("The local owner still cannot command the fresh AI actor"), FollowingEncounter.Combat->GetActionAuthority()->CanControllerControl(FollowingEncounter.Controllers[0], FollowingEncounter.Humans[0]));
    if (!TestTrue(TEXT("The next encounter progresses with no living human input character"), FollowingEncounter.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("A fresh AI-only encounter still executes the companion's attack"), FollowingEncounter.Enemies[0]->GetAttributeSet()->GetHP(), 93.0f);
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
    const FRotator OriginalRotation(0.0f, 37.0f, 0.0f);
    Source->SetActorRotation(OriginalRotation);
    ACombatGridTile* Home = Source->GetCurrentTile();
    if (!Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Target)) || !Fixture.Ready(0)) return false;
    Fixture.Round->Tick(0.25f);
    TestTrue(TEXT("Approach changes the authoritative world position"), FVector::Dist2D(Origin, Source->GetActorLocation()) > 50.0f);
    TestFalse(TEXT("Approach turns away from the saved home facing"), Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
    TestFalse(TEXT("Approach exposes actual movement velocity"), Source->GetVelocity().IsNearlyZero());
    TestEqual(TEXT("Home remains reserved during the attack excursion"), Source->GetCurrentTile(), Home);
    TestTrue(TEXT("Melee has not damaged a distant target before approach and windup"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f));
    if (!TestTrue(TEXT("The round waits through attack and return"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestTrue(TEXT("The survivor physically returns home"), FVector::Dist2D(Origin, Source->GetActorLocation()) <= 2.0f);
    TestTrue(TEXT("Returning restores the rotation captured when the plan was locked"), Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
    TestTrue(TEXT("Returning clears the movement velocity"), Source->GetVelocity().IsNearlyZero());
    TestTrue(TEXT("Actual close-range hit applies damage"), Target->GetAttributeSet()->GetHP() < 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundUnitApproachTrackingTest, "ProjectA.Combat.Round.UnitApproachStopsWithinReach", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundUnitApproachTrackingTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 2; ++Case)
    {
        const FString Context = Case == 0 ? TEXT("Target already within reach") : TEXT("Moving target enters reach");
        FCombatRoundSkill Skill;
        Skill.WindupSeconds = 0.2f;
        FFixture Fixture;
        if (!TestTrue(Context + TEXT(" initializes"), Fixture.Initialize(1, 10.f, nullptr, FIntPoint(0, 3), 1, &Skill))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Target = Fixture.Enemies[0];
        const FVector Origin = Source->GetActorLocation();
        ACombatGridTile* Home = Source->GetCurrentTile();
        if (Case == 0) Target->SetActorLocation(Origin + FVector(0.f, 80.f, 0.f), false);
        if (!TestTrue(Context + TEXT(" submits"), Fixture.Submit(0, Fixture.Command(Source, Fixture.HumanSkillId, Target))) || !TestTrue(Context + TEXT(" locks"), Fixture.Ready(0))) return false;
        if (Case == 1)
        {
            Fixture.Round->Tick(0.1f);
            if (!TestTrue(TEXT("The initial distant target requires approach"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Approaching)) return false;
            Target->SetActorLocation(Source->GetActorLocation() + FVector(250.f, 0.f, 0.f), false);
            const FVector BeforeTracking = Source->GetActorLocation();
            Fixture.Round->Tick(0.05f);
            TestTrue(TEXT("Approach follows the target's current position instead of its reserved tile"), Source->GetActorLocation().X > BeforeTracking.X && FMath::IsNearlyEqual(Source->GetActorLocation().Y, BeforeTracking.Y, 0.01));
            Target->SetActorLocation(Source->GetActorLocation() + FVector(80.f, 0.f, 0.f), false);
        }
        const FVector CastLocation = Source->GetActorLocation();
        Fixture.Round->Tick(0.01f);
        if (!TestTrue(Context + TEXT(" starts casting without retreating to a fixed separation"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Casting && Source->GetActorLocation().Equals(CastLocation, 0.01f))) return false;
        TestTrue(Context + TEXT(" stops locomotion and retains the reserved home"), Source->GetVelocity().IsNearlyZero() && Source->GetCurrentTile() == Home);
        TestEqual(Context + TEXT(" preserves windup before damage"), Target->GetAttributeSet()->GetHP(), 100.f);
        for (int32 Step = 0; Step < 30 && Target->GetAttributeSet()->GetHP() == 100.f; ++Step) Fixture.Round->Tick(0.01f);
        TestEqual(Context + TEXT(" releases exactly one close-range hit before returning"), Target->GetAttributeSet()->GetHP(), 75.f);
        if (!TestTrue(Context + TEXT(" settles into the next round"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(Context + TEXT(" returns to the original home without repeating damage"), Source->GetActorLocation().Equals(Origin, 2.f) && Source->GetCurrentTile() == Home && Target->GetAttributeSet()->GetHP() == 75.f);
    }
    FCombatRoundSkill EnemySkill;
    EnemySkill.HitRange = 100.f;
    EnemySkill.WindupSeconds = 3.f;
    FCombatRoundSkill HumanSkill;
    HumanSkill.WindupSeconds = 0.2f;
    FFixture Mutual;
    if (!TestTrue(TEXT("Two melee attackers with different start delays initialize"), Mutual.Initialize(1, 20.f, &EnemySkill, FIntPoint(0, 3), 1, &HumanSkill, 0.f))) return false;
    AUnitBase* Human = Mutual.Humans[0];
    AUnitBase* Enemy = Mutual.Enemies[0];
    const FVector HumanOrigin = Human->GetActorLocation();
    const FVector EnemyOrigin = Enemy->GetActorLocation();
    if (!TestTrue(TEXT("The slower unit submits its attack"), Mutual.Submit(0, Mutual.Command(Human, Mutual.HumanSkillId, Enemy))) || !TestTrue(TEXT("Both melee plans lock"), Mutual.Ready(0))) return false;
    for (int32 Step = 0; Step < 210 && Mutual.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Waiting; ++Step) Mutual.Round->Tick(0.01f);
    TestTrue(TEXT("The faster opponent approaches and casts first"), Mutual.Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Casting && FVector::Dist2D(EnemyOrigin, Enemy->GetActorLocation()) > 500.f);
    TestTrue(TEXT("The delayed attacker casts in place when the opponent has already approached"), Mutual.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Casting && Human->GetActorLocation().Equals(HumanOrigin, 0.01f));
    if (!TestTrue(TEXT("Mutual attacks settle through their separate windups and returns"), Mutual.AdvanceUntilNextRound(1))) return false;
    TestTrue(TEXT("Both attacks release once and both survivors retain their original homes"), Human->GetAttributeSet()->GetHP() == 75.f && Enemy->GetAttributeSet()->GetHP() == 75.f && Human->GetActorLocation().Equals(HumanOrigin, 2.f) && Enemy->GetActorLocation().Equals(EnemyOrigin, 2.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMeleeSpeedTest, "ProjectA.Combat.Round.MeleeMovementUsesRoundSpeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMeleeSpeedTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FCombatRoundSkill BoundarySkill;
    TestEqual(TEXT("Fractional round speed preserves fractional melee movement"), CombatRoundRules::AttackMoveSpeed(BoundarySkill, 10.5f), 358.75f);
    TestEqual(TEXT("Extreme finite round speed is bounded before conversion to float"), CombatRoundRules::AttackMoveSpeed(BoundarySkill, std::numeric_limits<float>::max()), 100000.0f);
    for (float InvalidSpeed : {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        TestEqual(TEXT("Negative or nonfinite round speed retains the zero-speed movement floor"), CombatRoundRules::AttackMoveSpeed(BoundarySkill, InvalidSpeed), 175.0f);
    }

    // Measure authoritative displacement over fixed elapsed time instead of reproducing the speed formula.
    // 속도 수식을 다시 계산하지 않고 정해진 경과 시간 동안의 서버 실제 이동 거리를 측정합니다.
    const float RoundSpeeds[] = {0.0f, 5.0f, 10.0f, 10.0f};
    const double ExpectedDistances[] = {17.5, 26.25, 35.0, 70.0};
    for (int32 Case = 0; Case < UE_ARRAY_COUNT(RoundSpeeds); ++Case)
    {
        const bool bGroundAttack = Case == 3;
        const FString Context = bGroundAttack ? TEXT("Non-melee tile movement") : FString::Printf(TEXT("Melee speed %.0f"), RoundSpeeds[Case]);
        FCombatRoundSkill Skill;
        Skill.MoveSpeed = 700.0f;
        if (bGroundAttack)
        {
            Skill.Kind = ECombatRoundSkillKind::GroundAttack;
            Skill.Approach = ECombatRoundApproach::Tile;
            Skill.HitRange = 1000.0f;
        }
        FFixture Fixture;
        if (!TestTrue(Context + TEXT(" initializes"), Fixture.Initialize(1, 0.0f, nullptr, FIntPoint(0, 3), 1, &Skill, RoundSpeeds[Case]))) return false;
        ACombatRoundCoordinator* Round = Fixture.Round;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Target = Fixture.Enemies[0];
        ACombatGridTile* Home = Source->GetCurrentTile();
        const FVector Origin = Source->GetActorLocation();
        FCombatRoundCommand Command = Fixture.Command(Source, Fixture.HumanSkillId, Target);
        if (bGroundAttack) Command.DestinationCoord = FIntPoint(0, 1);
        if (!TestTrue(Context + TEXT(" submits"), Fixture.Submit(0, Command)) || !TestTrue(Context + TEXT(" locks"), Fixture.Ready(0))) return false;
        Round->Tick(0.1f);
        TestTrue(Context + TEXT(" covers its expected approach distance in 0.1 seconds"), FMath::IsNearlyEqual(FVector::Dist2D(Origin, Source->GetActorLocation()), ExpectedDistances[Case], 0.01));
        TestTrue(Context + TEXT(" keeps damage behind approach and windup"), Target->GetAttributeSet()->GetHP() == 100.0f && Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Approaching);
        if (Case == 2)
        {
            Source->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), 20.0f);
            const FVector BeforeChange = Source->GetActorLocation();
            Round->Tick(0.1f);
            TestEqual(TEXT("Live Dexterity changes immediately while the round snapshot stays fixed"), Source->GetCombatSpeed(), 20.0f);
            TestEqual(TEXT("The active round retains its planned speed"), Round->GetView().Units[0].Speed, 10.0f);
            TestTrue(TEXT("Changing Dexterity mid-approach does not accelerate the active movement"), FMath::IsNearlyEqual(FVector::Dist2D(BeforeChange, Source->GetActorLocation()), 35.0, 0.01));
        }
        for (int32 Step = 0; Step < 400 && Round->GetView().Phase == ECombatRoundPhase::Resolving && Round->GetView().Units[0].ActionPhase != ECombatRoundActionPhase::Returning; ++Step) Round->Tick(0.01f);
        if (!TestTrue(Context + TEXT(" releases one attack before returning"), Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning && Target->GetAttributeSet()->GetHP() == 75.0f)) return false;
        const FVector ReturnStart = Source->GetActorLocation();
        Round->Tick(0.1f);
        TestTrue(Context + TEXT(" uses the same planned speed for 0.1 seconds of return"), FMath::IsNearlyEqual(FVector::Dist2D(ReturnStart, Source->GetActorLocation()), ExpectedDistances[Case], 0.01));
        TestTrue(Context + TEXT(" moves toward its reserved home during return"), FVector::DistSquared2D(Origin, Source->GetActorLocation()) < FVector::DistSquared2D(Origin, ReturnStart));
        if (!TestTrue(Context + TEXT(" settles into the next round"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(Context + TEXT(" returns to the original tile with one damage application"), Source->GetCurrentTile() == Home && Source->GetActorLocation().Equals(Origin, 2.0f) && Target->GetAttributeSet()->GetHP() == 75.0f);
        if (Case == 2)
        {
            TestEqual(TEXT("The next planning snapshot adopts the changed Dexterity"), Round->GetView().Units[0].Speed, 20.0f);
            if (!TestTrue(TEXT("The next melee plan submits"), Fixture.Submit(0, Command)) || !TestTrue(TEXT("The next melee round locks"), Fixture.Ready(0))) return false;
            const FVector NextOrigin = Source->GetActorLocation();
            Round->Tick(0.1f);
            TestTrue(TEXT("The next round travels 52.5 units in 0.1 seconds at speed 20"), FMath::IsNearlyEqual(FVector::Dist2D(NextOrigin, Source->GetActorLocation()), 52.5, 0.01));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundReturnFacingTest, "ProjectA.Combat.Round.ReturnFacingBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundReturnFacingTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 4; ++Case)
    {
        const FString Context = Case == 0 ? TEXT("Enemy return") : Case == 1 ? TEXT("Stationary projectile") : Case == 2 ? TEXT("Return timeout") : TEXT("Resident movement");
        FCombatRoundSkill Skill;
        Skill.Kind = Case == 1 ? ECombatRoundSkillKind::Projectile : Case == 3 ? ECombatRoundSkillKind::GroundAttack : ECombatRoundSkillKind::Melee;
        Skill.Approach = Case == 1 ? ECombatRoundApproach::None : Case == 3 ? ECombatRoundApproach::Tile : ECombatRoundApproach::Unit;
        Skill.bRemainAtDestination = Case == 3;
        Skill.WindupSeconds = 0.2f;
        Skill.Power = 5.0f;
        Skill.HitRange = Case == 3 ? 500.0f : 150.0f;
        Skill.ProjectileSpeed = 1000.0f;
        FFixture Fixture;
        if (!TestTrue(Context + TEXT(" initializes"), Fixture.Initialize(1, 20, Case == 0 ? &Skill : nullptr, FIntPoint(0, 3), 1, Case == 0 ? nullptr : &Skill))) return false;
        AUnitBase* Source = Case == 0 ? Fixture.Enemies[0] : Fixture.Humans[0];
        AUnitBase* Target = Case == 0 ? Fixture.Humans[0] : Fixture.Enemies[0];
        const int32 SourceIndex = Case == 0 ? 1 : 0;
        const FVector Origin = Source->GetActorLocation();
        const FRotator OriginalRotation(0.0f, Case == 0 ? -143.0f : 37.0f, 0.0f);
        Source->SetActorRotation(OriginalRotation);
        ACombatGridTile* Home = Source->GetCurrentTile();
        ACombatGridTile* Destination = Fixture.Grid->GetTileAtCoord(FIntPoint(0, 1));
        FCombatRoundCommand Command = Case == 0 ? Fixture.Command(Fixture.Humans[0], TEXT("Wait")) : Fixture.Command(Source, Fixture.HumanSkillId, Target);
        if (Case == 3) Command.DestinationCoord = Destination->GridCoord;
        if (!TestTrue(Context + TEXT(" submits"), Fixture.Submit(0, Command)) || !TestTrue(Context + TEXT(" locks"), Fixture.Ready(0))) return false;

        if (Case == 0 || Case == 2)
        {
            for (int32 Step = 0; Step < 400 && Fixture.Round->GetView().Phase == ECombatRoundPhase::Resolving && Fixture.Round->GetView().Units[SourceIndex].ActionPhase != ECombatRoundActionPhase::Returning; ++Step) Fixture.Round->Tick(0.01f);
            if (!TestTrue(Context + TEXT(" reaches the return phase after attacking away from home"), Fixture.Round->GetView().Units[SourceIndex].ActionPhase == ECombatRoundActionPhase::Returning && FVector::Dist2D(Origin, Source->GetActorLocation()) > 50.0f)) return false;
            TestFalse(Context + TEXT(" has changed the original facing"), Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
        }
        if (Case == 1)
        {
            Fixture.Round->Tick(0.1f);
            TestTrue(TEXT("Stationary casting remains at its home position"), Source->GetActorLocation().Equals(Origin, 0.1f));
            TestFalse(TEXT("Stationary casting temporarily faces the target"), Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
            Fixture.Round->Tick(0.15f);
            TestTrue(TEXT("A released projectile restores facing before the projectile settles"), Fixture.Round->GetView().Units[SourceIndex].ActionPhase == ECombatRoundActionPhase::Complete && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
        }
        if (Case == 2)
        {
            // Hold the unit away from home after each move so the real return timeout must recover it.
            // 매 이동 뒤 유닛을 원위치 밖에 유지하여 실제 복귀 제한 시간이 복구를 수행하게 합니다.
            AddExpectedError(TEXT("[Round] Return timeout"), EAutomationExpectedErrorFlags::Contains, 1, false);
            const FVector InterruptedReturnLocation = Source->GetActorLocation();
            for (int32 Step = 0; Step < 650 && Fixture.Round->GetView().RoundNumber == 1 && Fixture.Round->GetView().Phase == ECombatRoundPhase::Resolving; ++Step)
            {
                Source->SetActorLocation(InterruptedReturnLocation, false);
                Fixture.Round->Tick(0.01f);
            }
        }
        if (!TestTrue(Context + TEXT(" settles into the next planning round"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(Context + TEXT(" applies its attack before settling"), Target->GetAttributeSet()->GetHP() < 100.0f);
        if (Case == 3)
        {
            FVector Facing = Target->GetActorLocation() - Source->GetActorLocation();
            Facing.Z = 0.0f;
            TestEqual(TEXT("Successful resident movement updates the reserved tile"), Source->GetCurrentTile(), Destination);
            TestTrue(TEXT("Successful resident movement stays at the destination"), Source->GetActorLocation().Equals(Destination->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), 0.1f));
            TestTrue(TEXT("Resident movement preserves its aimed facing"), Source->GetActorRotation().Equals(Facing.Rotation(), 0.1f));
            TestFalse(TEXT("Resident movement does not restore the departed home's facing"), Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
        }
        else
        {
            TestTrue(Context + TEXT(" stops movement at completion"), Source->GetVelocity().IsNearlyZero());
            TestEqual(Context + TEXT(" keeps its reserved home"), Source->GetCurrentTile(), Home);
            TestTrue(Context + TEXT(" restores its original position"), Source->GetActorLocation().Equals(Origin, Case == 0 ? 2.0f : 0.1f));
            TestTrue(Context + TEXT(" restores its original facing"), Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMontageRecoveryTest, "ProjectA.Combat.Round.MontageRecoveryBeforeReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMontageRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    UAnimMontage* AuthoredMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/Unit/Animation/Montage/MM_Attack_01_Montage.MM_Attack_01_Montage"));
    if (!TestNotNull(TEXT("The authored montage supplies real animation metadata"), AuthoredMontage)) return false;
    TStrongObjectPtr<UAnimMontage> Montage(DuplicateObject<UAnimMontage>(AuthoredMontage, GetTransientPackage()));
    if (!TestTrue(TEXT("The transient montage has a finite positive length"), Montage.IsValid() && FMath::IsFinite(Montage->GetPlayLength()) && Montage->GetPlayLength() > 0.0f)) return false;
    // Normalize a transient copy to one second; the server-only fixture has no animated mesh instance.
    // 임시 복사본을 1초 재생으로 맞추며 서버 픽스처에는 애니메이션 메시 인스턴스가 없습니다.
    Montage->RateScale = Montage->GetPlayLength();
    Montage->BlendOut.SetBlendTime(0.1f);
    Montage->bEnableAutoBlendOut = true;
    for (FCompositeSection& Section : Montage->CompositeSections) Section.NextSectionName = NAME_None;
    for (int32 Case = 0; Case < 7; ++Case)
    {
        Montage->RateScale = Case == 6 ? 0.f : Montage->GetPlayLength();
        for (FCompositeSection& Section : Montage->CompositeSections) Section.NextSectionName = Case == 5 ? Section.SectionName : NAME_None;
        const FString Context = Case == 0 ? TEXT("Montage recovery") : Case == 1 ? TEXT("Windup already consumes montage duration") : Case == 2 ? TEXT("No montage") : Case == 3 ? TEXT("Target dies before release") : TEXT("Hitch before montage starts");
        FCombatRoundSkill Skill;
        Skill.CastMontage = Case == 2 ? nullptr : Montage.Get();
        Skill.WindupSeconds = Case == 1 ? 1.5f : 0.1f;
        Skill.Power = 17.0f;
        FFixture Fixture;
        if (!TestTrue(Context + TEXT(" initializes"), Fixture.Initialize(1, 10, nullptr, FIntPoint(0, 3), Case == 3 ? 2 : 1, &Skill))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Target = Fixture.Enemies[0];
        const FVector Origin = Source->GetActorLocation();
        const FRotator OriginalRotation(0.0f, 37.0f, 0.0f);
        Source->SetActorRotation(OriginalRotation);
        if (!TestNull(Context + TEXT(" uses the no-AnimInstance recovery path"), Source->GetMesh()->GetAnimInstance())) return false;
        if (!TestTrue(Context + TEXT(" submits"), Fixture.Submit(0, Fixture.Command(Source, Fixture.HumanSkillId, Target))) || !TestTrue(Context + TEXT(" locks"), Fixture.Ready(0))) return false;
        if (Case >= 5)
        {
            if (!TestTrue(TEXT("A looping or invalid-rate montage settles before the finite deadline."), Fixture.AdvanceUntilNextRound(1))) return false;
            TestTrue(TEXT("Bounded recovery restores home without duplicate damage."), Source->GetActorLocation().Equals(Origin, 2.f) && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.f));
            continue;
        }
        if (Case == 4)
        {
            Fixture.Round->Tick(2.0f);
            if (!TestTrue(TEXT("The hitch leaves simulation debt while still approaching"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Approaching && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f))) return false;
            for (int32 Frame = 0; Frame < 3 && Fixture.Round->GetView().Units[0].ActionPhase != ECombatRoundActionPhase::Recovery; ++Frame) Fixture.Round->Tick(0.01f);
            if (!TestTrue(TEXT("Catching up debt starts the montage and releases one hit after the slower approach"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Recovery && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f))) return false;
            const FVector RecoveryLocation = Source->GetActorLocation();
            const FRotator RecoveryRotation = Source->GetActorRotation();
            for (int32 Step = 0; Step < 30; ++Step) Fixture.Round->Tick(0.01f);
            TestTrue(TEXT("Simulation debt does not consume the newly started montage recovery time"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Recovery);
            TestTrue(TEXT("The hitch recovery keeps the released attack stationary without repeating damage"), Source->GetActorLocation().Equals(RecoveryLocation, 0.1f) && Source->GetActorRotation().Equals(RecoveryRotation, 0.1f) && Source->GetVelocity().IsNearlyZero() && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f));
            TestEqual(TEXT("The hitch still charges the action cost only once"), Source->GetCurrentActionPoint(), 1);
            Fixture.Round->Tick(1.06f);
            TestTrue(TEXT("Actual elapsed montage time releases the return after the hitch"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning);
            if (!TestTrue(Context + TEXT(" settles into the next planning round"), Fixture.AdvanceUntilNextRound(1))) return false;
            TestTrue(Context + TEXT(" restores the home position and original facing"), Source->GetActorLocation().Equals(Origin, 2.0f) && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
            TestTrue(Context + TEXT(" applies no extra damage while returning"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f));
            continue;
        }
        for (int32 Step = 0; Step < 200 && Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Approaching; ++Step) Fixture.Round->Tick(0.01f);
        if (!TestTrue(Context + TEXT(" reaches casting after a real approach"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Casting && FVector::Dist2D(Origin, Source->GetActorLocation()) > 400.0f)) return false;
        const FVector CastLocation = Source->GetActorLocation();
        const FRotator CastRotation = Source->GetActorRotation();
        TestTrue(Context + TEXT(" has not delivered damage before windup"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f));
        TestFalse(Context + TEXT(" has no actual montage instance to drive completion"), Source->HasRoundCastMontageInstance());
        if (Case == 3)
        {
            Target->Die();
            Fixture.Round->Tick(0.01f);
            TestTrue(TEXT("Pre-release cancellation returns without waiting for montage recovery"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning);
            TestTrue(TEXT("The cancelled attack has not damaged the target or another living enemy"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f) && FMath::IsNearlyEqual(Fixture.Enemies[1]->GetAttributeSet()->GetHP(), 100.0f));
        }
        else
        {
            int32 CastingSteps = 0;
            for (; CastingSteps < 200 && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f); ++CastingSteps) Fixture.Round->Tick(0.01f);
            if (!TestTrue(Context + TEXT(" releases exactly one hit at windup rather than montage completion"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f) && FMath::IsNearlyEqual(CastingSteps * 0.01f, Skill.WindupSeconds, 0.02f))) return false;
            TestEqual(Context + TEXT(" charges the action cost once"), Source->GetCurrentActionPoint(), 1);
            if (Case == 0)
            {
                TestTrue(TEXT("Released damage enters recovery before movement home"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Recovery);
                for (; CastingSteps < 130; ++CastingSteps) Fixture.Round->Tick(0.01f);
                TestTrue(TEXT("Recovery includes the rate-adjusted montage, blend-out and grace from casting start"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Recovery);
                TestTrue(TEXT("Recovery preserves the attack position and facing with zero movement"), Source->GetActorLocation().Equals(CastLocation, 0.1f) && Source->GetActorRotation().Equals(CastRotation, 0.1f) && Source->GetVelocity().IsNearlyZero());
                TestTrue(TEXT("Recovery ticks do not repeat the released damage"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f));
                Fixture.Round->Tick(0.06f);
                TestTrue(TEXT("The finite no-AnimInstance deadline releases the return without restarting a full wait"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning);
            }
            else if (Case == 1)
            {
                Fixture.Round->Tick(0.02f);
                TestTrue(TEXT("A long windup does not add the montage duration again after damage"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning);
            }
            else
            {
                TestTrue(TEXT("A skill without a montage keeps its immediate return behavior"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Returning);
            }
        }
        if (!TestTrue(Context + TEXT(" settles into the next planning round"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(Context + TEXT(" restores the home position and original facing"), Source->GetActorLocation().Equals(Origin, 2.0f) && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
        TestTrue(Context + TEXT(" applies no extra damage while returning"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), Case == 3 ? 100.0f : 83.0f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMeleeCollisionTest, "ProjectA.Combat.Round.MeleePhysicalContacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMeleeCollisionTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 9; ++Case)
    {
        FCombatRoundSkill Skill;
        Skill.Approach = ECombatRoundApproach::None;
        Skill.WindupSeconds = 0.05f;
        Skill.Power = 17.0f;
        FFixture Fixture;
        if (!TestTrue(TEXT("The physical melee fixture initializes"), Fixture.Initialize(2, 10, nullptr, FIntPoint(0, 3), 2, &Skill))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Friend = Fixture.Humans[1];
        AUnitBase* Selected = Fixture.Enemies[0];
        AUnitBase* Interceptor = Fixture.Enemies[1];
        AUnitBase* OtherCombat = Fixture.AddUnit(FIntPoint(3, 2), ETeam::Enemy);
        if (!OtherCombat) return false;
        OtherCombat->UnitIndex = Interceptor->UnitIndex;
        OtherCombat->SetActorLocation(FVector(0.0f, 75.0f, 100.0f), false);
        Friend->SetActorLocation(FVector(0.0f, 40.0f, 100.0f), false);
        Selected->SetActorLocation(FVector(0.0f, Case == 7 ? 145.0f : Case == 8 ? 130.0f : 600.0f, 100.0f), false);
        const float InterceptorDistance = Case == 1 ? Skill.HitRange + Interceptor->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f : Case == 8 ? 600.0f : 130.0f;
        Interceptor->SetActorLocation(FVector(0.0f, InterceptorDistance, Case == 2 ? 400.0f : 100.0f), false);
        Fixture.AddPawnSensor(Interceptor, FVector::ZeroVector, FVector(90.0f));
        if (Case == 3 || Case == 4)
        {
            if (!Fixture.AddObstacle(FVector(0.0f, Case == 3 ? 75.0f : 120.0f, 100.0f), FVector(120.0f, 2.0f, 120.0f))) return false;
        }
        if (Case == 5) Interceptor->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (Case == 6)
        {
            Interceptor->Die();
            Interceptor->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        const float BeforeInterceptorHP = Interceptor->GetAttributeSet()->GetHP();
        const bool bExpectedHit = Case == 0 || Case == 1 || Case == 4 || Case == 7;
        if (!Fixture.Submit(0, Fixture.Command(Source, Fixture.HumanSkillId, Selected)) || !Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait"))) || !Fixture.Ready(0) || !Fixture.Ready(1)) return false;
        if (Case == 8) Selected->SetActorLocation(FVector(0.0f, 600.0f, 100.0f), false);
        Fixture.Round->Tick(0.1f);
        const FString Context = FString::Printf(TEXT("Melee collision case %d"), Case);
        TestTrue(Context + TEXT(" damages only an intersecting living capsule before a wall"), FMath::IsNearlyEqual(Interceptor->GetAttributeSet()->GetHP(), BeforeInterceptorHP - (bExpectedHit ? Skill.Power : 0.0f)));
        TestTrue(Context + TEXT(" does not guarantee the selected target or pierce the first enemy"), FMath::IsNearlyEqual(Selected->GetAttributeSet()->GetHP(), 100.0f));
        TestTrue(Context + TEXT(" excludes the caster, ally and another combat's same-index enemy"), FMath::IsNearlyEqual(Source->GetAttributeSet()->GetHP(), 100.0f) && FMath::IsNearlyEqual(Friend->GetAttributeSet()->GetHP(), 100.0f) && FMath::IsNearlyEqual(OtherCombat->GetAttributeSet()->GetHP(), 100.0f));
        if (!TestTrue(Context + TEXT(" settles after the attack"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(Context + TEXT(" delivers at most one damage event despite multiple pawn components"), FMath::IsNearlyEqual(Interceptor->GetAttributeSet()->GetHP(), BeforeInterceptorHP - (bExpectedHit ? Skill.Power : 0.0f)));
    }
    FCombatRoundSkill Invalid;
    Invalid.SkillId = TEXT("InvalidMeleeRadius");
    for (float Radius : {-1.0f, 0.0f, 1001.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        Invalid.MeleeRadius = Radius;
        TestFalse(TEXT("Nonpositive, oversized and nonfinite melee collision radii are rejected"), CombatRoundRules::IsValidSkill(Invalid));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMeleeAreaCollisionTest, "ProjectA.Combat.Round.MeleeAreaPhysicalContacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMeleeAreaCollisionTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 9; ++Case)
    {
        FCombatRoundSkill Skill;
        Skill.bUseMeleeAreaCollision = true;
        Skill.Approach = ECombatRoundApproach::None;
        Skill.Power = 17.f;
        Skill.WindupSeconds = 0.05f;
        if (Case == 7) Skill.MeleeAreaHalfExtent.Y = 75.f;
        FFixture Fixture;
        if (!TestTrue(TEXT("Physical area melee initializes"), Fixture.Initialize(2, 10, nullptr, FIntPoint(0, 3), 4, &Skill))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Friend = Fixture.Humans[1];
        const FQuat Rotation = FRotator(0.f, Case == 1 ? 90.f : 0.f, 0.f).Quaternion();
        const auto Position = [&Rotation](float X, float Y, float Z = 100.f) { return Rotation.RotateVector(FVector(X, Y, Z)); };
        // World contacts deliberately differ from reserved tile locations, including for the selected target.
        // 선택 대상을 포함해 월드 충돌 위치를 예약 타일 위치와 다르게 배치합니다.
        Fixture.Enemies[0]->SetActorLocation(Position(0, 100), false);
        Fixture.Enemies[1]->SetActorLocation(Position(200, Case == 2 ? 300 : 100, Case == 3 ? 400 : 100), false);
        Fixture.Enemies[2]->SetActorLocation(Position(-200, 100), false);
        Fixture.Enemies[3]->SetActorLocation(Position(400, 100), false);
        Friend->SetActorLocation(Position(50, 100), false);
        AUnitBase* OtherCombat = Fixture.AddUnit(FIntPoint(3, 2), ETeam::Enemy);
        if (!OtherCombat) return false;
        OtherCombat->SetActorLocation(Position(100, 100), false);
        OtherCombat->UnitIndex = Fixture.Enemies[0]->UnitIndex;
        if (Case == 4 && !Fixture.AddObstacle(Position(100, 50), FVector(2, 25, 100))) return false;
        if (Case == 5) Fixture.Enemies[1]->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (Case == 6) Fixture.Enemies[1]->Die();
        Fixture.AddPawnSensor(Fixture.Enemies[0], FVector::ZeroVector, FVector(150));
        if (Case == 8) Fixture.AddPawnSensor(Fixture.Enemies[3], FVector(-200, 0, 0), FVector(150));
        if (!Fixture.Submit(0, Fixture.Command(Source, Fixture.HumanSkillId, Fixture.Enemies[0])) || !Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait"))) || !Fixture.Ready(0) || !Fixture.Ready(1)) return false;
        TestEqual(TEXT("Multiple physical contacts still consume one AP"), Source->GetCurrentActionPoint(), 1);
        if (!TestTrue(TEXT("Area melee settles after one release"), Fixture.AdvanceUntilNextRound(1))) return false;
        const FString Context = FString::Printf(TEXT("Physical melee area case %d"), Case);
        TestEqual(Context + TEXT(" hits the selected capsule exactly once"), Fixture.Enemies[0]->GetAttributeSet()->GetHP(), 83.f);
        const float RightHP = Case == 6 ? 0.f : Case >= 2 && Case <= 7 ? 100.f : 83.f;
        TestEqual(Context + TEXT(" respects depth, height, walls and capsule state"), Fixture.Enemies[1]->GetAttributeSet()->GetHP(), RightHP);
        TestEqual(Context + TEXT(" width alone controls the other side"), Fixture.Enemies[2]->GetAttributeSet()->GetHP(), Case == 7 ? 100.f : 83.f);
        TestEqual(Context + TEXT(" excludes out-of-volume capsules despite attached sensors"), Fixture.Enemies[3]->GetAttributeSet()->GetHP(), 100.f);
        TestEqual(Context + TEXT(" excludes allies"), Friend->GetAttributeSet()->GetHP(), 100.f);
        TestEqual(Context + TEXT(" excludes other combat rosters"), OtherCombat->GetAttributeSet()->GetHP(), 100.f);
    }
    FCombatRoundSkill Moving;
    Moving.bUseMeleeAreaCollision = true;
    FFixture Fixture;
    if (!Fixture.Initialize(1, 10, nullptr, FIntPoint(0, 3), 1, &Moving)) return false;
    AUnitBase* Source = Fixture.Humans[0];
    const FVector Origin = Source->GetActorLocation();
    const FRotator Facing = Source->GetActorRotation();
    if (!Fixture.Submit(0, Fixture.Command(Source, Fixture.HumanSkillId, Fixture.Enemies[0])) || !Fixture.Ready(0)) return false;
    Fixture.Round->Tick(0.2f);
    TestFalse(TEXT("Wide melee still approaches physically"), Source->GetActorLocation().Equals(Origin, 1.f));
    TestTrue(TEXT("Wide melee completes its approach and return"), Fixture.AdvanceUntilNextRound(1));
    TestEqual(TEXT("Approaching wide melee delivers the authored damage"), Fixture.Enemies[0]->GetAttributeSet()->GetHP(), 75.f);
    TestTrue(TEXT("Wide melee restores its home and facing"), Source->GetActorLocation().Equals(Origin, 2.f) && Source->GetActorRotation().Equals(Facing, 0.1f));
    Moving.SkillId = TEXT("InvalidArea");
    Moving.MeleeArea = ESkillAreaType::TargetAndSides;
    TestFalse(TEXT("Tile coverage and physical area collision cannot be mixed implicitly"), CombatRoundRules::IsValidSkill(Moving));
    Moving.MeleeArea = ESkillAreaType::Single;
    for (double Extent : {0.0, -1.0, 1001.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        Moving.MeleeAreaHalfExtent.Y = Extent;
        TestFalse(TEXT("Invalid collision dimensions are rejected before execution"), CombatRoundRules::IsValidSkill(Moving));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundMeleeSidesTest, "ProjectA.Combat.Round.MeleeTargetAndSides", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundMeleeSidesTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 10; ++Case)
    {
        FCombatRoundSkill Skill;
        Skill.MeleeArea = ESkillAreaType::TargetAndSides;
        Skill.Power = 17.f;
        Skill.WindupSeconds = 0.1f;
        FFixture Fixture;
        if (!TestTrue(TEXT("The melee side fixture initializes"), Fixture.Initialize(2, 10, nullptr, FIntPoint(0, 3), 4, &Skill))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Friend = Fixture.Humans[1];
        const int32 SelectedIndex = Case == 1 ? 0 : Case == 2 ? 3 : 1;
        AUnitBase* Selected = Fixture.Enemies[SelectedIndex];
        const FVector Origin = Source->GetActorLocation();
        const FRotator Facing = Source->GetActorRotation();
        Friend->SetActorLocation(Selected->GetActorLocation(), false);
        AUnitBase* OtherCombat = Fixture.AddUnit(FIntPoint(3, 2), ETeam::Enemy);
        if (!OtherCombat) return false;
        OtherCombat->SetActorLocation(Selected->GetActorLocation(), false);
        OtherCombat->UnitIndex = Selected->UnitIndex;
        if (Case == 3) Fixture.Enemies[0]->AddActorWorldOffset(FVector(0, -200, 0), false);
        if (Case == 4) Fixture.Enemies[2]->AddActorWorldOffset(FVector(0, 0, 400), false);
        if (Case == 5 && !Fixture.AddObstacle(FVector(300, 600, 100), FVector(2, 25, 90))) return false;
        if (Case == 6) Fixture.AddPawnSensor(Fixture.Enemies[3], FVector(-200, 0, 0), FVector(150));
        if (Case == 7) Fixture.Enemies[2]->Die();
        if (Case == 8) Fixture.Enemies[2]->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (!Fixture.Submit(0, Fixture.Command(Source, Fixture.HumanSkillId, Selected)) || !Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait"))) || !Fixture.Ready(0) || !Fixture.Ready(1)) return false;
        TestEqual(TEXT("One melee sweep costs one AP regardless of target count"), Source->GetCurrentActionPoint(), 1);
        if (Case == 9) Selected->Die();
        bool bApproached = false;
        bool bReturned = false;
        for (int32 Step = 0; Step < 1000 && Fixture.Round->GetView().RoundNumber == 1; ++Step)
        {
            Fixture.Round->Tick(0.01f);
            bApproached |= !Source->GetActorLocation().Equals(Origin, 5.f);
            for (const auto& Unit : Fixture.Round->GetView().Units)
            {
                if (Unit.Unit == Source && Unit.ActionPhase == ECombatRoundActionPhase::Returning) bReturned = true;
            }
        }
        const FString Context = FString::Printf(TEXT("Melee sides case %d"), Case);
        TestEqual(Context + TEXT(" settles at the next planning phase"), Fixture.Round->GetView().RoundNumber, 2);
        if (Case != 9) TestTrue(Context + TEXT(" approaches and returns through the melee movement flow"), bApproached && bReturned);
        TestTrue(Context + TEXT(" restores the original position and facing"), Source->GetActorLocation().Equals(Origin, 2.f) && Source->GetActorRotation().Equals(Facing, 0.1f));
        for (int32 Index = 0; Index < Fixture.Enemies.Num(); ++Index)
        {
            const bool bDead = (Case == 7 && Index == 2) || (Case == 9 && Index == SelectedIndex);
            const bool bExcluded = (Case == 3 && Index == 0) || ((Case == 4 || Case == 5 || Case == 8) && Index == 2);
            const bool bHit = Case != 9 && !bExcluded && FMath::Abs(Index - SelectedIndex) <= 1;
            const float Expected = bDead ? 0.f : bHit ? 83.f : 100.f;
            TestEqual(Context + FString::Printf(TEXT(" enemy %d gets one physical hit only inside the adjacent span"), Index), Fixture.Enemies[Index]->GetAttributeSet()->GetHP(), Expected);
        }
        TestEqual(Context + TEXT(" excludes allies"), Friend->GetAttributeSet()->GetHP(), 100.f);
        TestEqual(Context + TEXT(" excludes another combat's same-index capsule"), OtherCombat->GetAttributeSet()->GetHP(), 100.f);
    }
    FCombatRoundSkill Invalid;
    Invalid.SkillId = TEXT("InvalidSides");
    Invalid.MeleeArea = ESkillAreaType::Row;
    TestFalse(TEXT("An unimplemented full row cannot silently become a single attack"), CombatRoundRules::IsValidSkill(Invalid));
    Invalid.MeleeArea = ESkillAreaType::TargetAndSides;
    Invalid.Approach = ECombatRoundApproach::None;
    TestFalse(TEXT("TargetAndSides requires melee approach"), CombatRoundRules::IsValidSkill(Invalid));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundGroundCollisionTest, "ProjectA.Combat.Round.GroundPhysicalContacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundGroundCollisionTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 7; ++Case)
    {
        FCombatRoundSkill Skill;
        Skill.Kind = ECombatRoundSkillKind::GroundAttack;
        Skill.Approach = ECombatRoundApproach::None;
        Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
        Skill.HitRange = 100.0f;
        Skill.WindupSeconds = 0.05f;
        Skill.Power = 17.0f;
        FFixture Fixture;
        if (!TestTrue(TEXT("The physical ground attack fixture initializes"), Fixture.Initialize(2, 10, nullptr, FIntPoint(0, 3), 3, &Skill))) return false;
        AUnitBase* Source = Fixture.Humans[0];
        AUnitBase* Friend = Fixture.Humans[1];
        AUnitBase* Boundary = Fixture.Enemies[0];
        AUnitBase* Second = Fixture.Enemies[1];
        AUnitBase* Dead = Fixture.Enemies[2];
        AUnitBase* OtherCombat = Fixture.AddUnit(FIntPoint(3, 2), ETeam::Enemy);
        if (!OtherCombat) return false;
        ACombatGridTile* AimTile = Fixture.Grid->GetTileAtCoord(FIntPoint(1, 2));
        const FVector Center = AimTile->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
        AimTile->SetActorEnableCollision(true);
        Boundary->SetActorLocation(Center + FVector(125.0f, 0.0f, 0.0f), false);
        Second->SetActorLocation(Center + FVector(-60.0f, 0.0f, Case == 1 ? 350.0f : 0.0f), false);
        Friend->SetActorLocation(Center + FVector(0.0f, -30.0f, 0.0f), false);
        OtherCombat->SetActorLocation(Center + FVector(0.0f, 30.0f, 0.0f), false);
        OtherCombat->UnitIndex = Boundary->UnitIndex;
        Dead->SetActorLocation(Center, false);
        Dead->Die();
        Dead->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        const float DeadHP = Dead->GetAttributeSet()->GetHP();
        Fixture.AddPawnSensor(Boundary, FVector::ZeroVector, FVector(120.0f));
        if (Case >= 2 && Case <= 4)
        {
            if (!Fixture.AddObstacle(Center + FVector(60.0f, 0.0f, 0.0f), FVector(2.0f, 25.0f, 90.0f), Case == 4 ? ECR_Overlap : ECR_Block, Case == 3 ? ECC_WorldDynamic : ECC_WorldStatic)) return false;
        }
        if (Case == 5) Boundary->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (Case == 6 && !Fixture.AddObstacle(Center, FVector(10.0f))) return false;
        FCombatRoundCommand Attack = Fixture.Command(Source, Fixture.HumanSkillId);
        Attack.TargetCoord = AimTile->GridCoord;
        if (!Fixture.Submit(0, Attack) || !Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait"))) || !Fixture.Ready(0) || !Fixture.Ready(1)) return false;
        Fixture.Round->Tick(0.1f);
        const FString Context = FString::Printf(TEXT("Ground collision case %d"), Case);
        const float ExpectedBoundaryHP = Case == 2 || Case == 3 || Case == 5 || Case == 6 ? 100.0f : 83.0f;
        const float ExpectedSecondHP = Case == 1 || Case == 6 ? 100.0f : 83.0f;
        TestTrue(Context + TEXT(" uses the exposed capsule edge beyond the center-distance radius"), FMath::IsNearlyEqual(Boundary->GetAttributeSet()->GetHP(), ExpectedBoundaryHP));
        TestTrue(Context + TEXT(" overlaps a second capsule in three dimensions independently of the first"), FMath::IsNearlyEqual(Second->GetAttributeSet()->GetHP(), ExpectedSecondHP));
        TestTrue(Context + TEXT(" excludes allies, dead capsules and other combat rosters"), FMath::IsNearlyEqual(Friend->GetAttributeSet()->GetHP(), 100.0f) && FMath::IsNearlyEqual(Dead->GetAttributeSet()->GetHP(), DeadHP) && FMath::IsNearlyEqual(OtherCombat->GetAttributeSet()->GetHP(), 100.0f));
        if (!TestTrue(Context + TEXT(" settles after one attack"), Fixture.AdvanceUntilNextRound(1))) return false;
        TestTrue(Context + TEXT(" does not repeat damage for extra pawn components or later steps"), FMath::IsNearlyEqual(Boundary->GetAttributeSet()->GetHP(), ExpectedBoundaryHP) && FMath::IsNearlyEqual(Second->GetAttributeSet()->GetHP(), ExpectedSecondHP));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundSequentialDeathTest, "ProjectA.Combat.Round.SequentialDeathCancelsUnreleasedAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundSequentialDeathTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FCombatRoundSkill Skill;
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.Approach = ECombatRoundApproach::None;
    Skill.Power = 100.0f;
    Skill.HitRange = 1000.0f;
    Skill.WindupSeconds = 0.1f;
    Skill.SubActionPointCost = 1;
    FFixture Fixture;
    if (!TestTrue(TEXT("Equal-time lethal attacks initialize"), Fixture.Initialize(1, 20.0f, &Skill, FIntPoint(0, 3), 1, &Skill))) return false;
    AUnitBase* Human = Fixture.Humans[0];
    AUnitBase* Enemy = Fixture.Enemies[0];
    if (!TestTrue(TEXT("The human selects its lethal action"), Fixture.Submit(0, Fixture.Command(Human, Fixture.HumanSkillId, Enemy))) || !TestTrue(TEXT("Readiness locks both equal-time attacks"), Fixture.Ready(0))) return false;
    TestTrue(TEXT("All AP and SAP costs are paid before either release"), Human->GetCurrentActionPoint() == 1 && Human->GetCurrentSubActionPoint() == 1 && Enemy->GetCurrentActionPoint() == 1 && Enemy->GetCurrentSubActionPoint() == 1);
    for (int32 Step = 0; Step < 100 && Fixture.Round->IsRoundSessionActive(); ++Step) Fixture.Round->Tick(0.01f);
    TestTrue(TEXT("The first processed lethal hit immediately kills the later caster"), Human->IsUnitAlive() && !Enemy->IsUnitAlive() && FMath::IsNearlyEqual(Human->GetAttributeSet()->GetHP(), 100.0f));
    TestTrue(TEXT("The dead caster's unreleased action is cancelled"), Fixture.Round->GetView().Units[1].ActionPhase == ECombatRoundActionPhase::Cancelled);
    TestTrue(TEXT("Death does not refund the cancelled action's paid AP or SAP"), Enemy->GetCurrentActionPoint() == 1 && Enemy->GetCurrentSubActionPoint() == 1);
    TestTrue(TEXT("The surviving team receives the normal victory result"), Fixture.Combat->GetCombatResult() == ECombatResult::Victory);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundDoubleWipeTest, "ProjectA.Combat.Round.ReleasedProjectilesDoubleWipeIsDefeat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundDoubleWipeTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    FCombatRoundSkill Skill;
    Skill.Kind = ECombatRoundSkillKind::Projectile;
    Skill.Approach = ECombatRoundApproach::None;
    Skill.Power = 100.0f;
    Skill.WindupSeconds = 0.1f;
    Skill.ProjectileSpeed = 1000.0f;
    FFixture Fixture;
    if (!TestTrue(TEXT("Opposing lethal projectile fixture initializes"), Fixture.Initialize(1, 20.0f, &Skill, FIntPoint(0, 3), 1, &Skill))) return false;
    int32 ResultCount = 0;
    ECombatResult Result = ECombatResult::None;
    Fixture.Combat->OnCombatResult.AddLambda([&ResultCount, &Result](ECombatResult NewResult)
    {
        ++ResultCount;
        Result = NewResult;
    });
    if (!TestTrue(TEXT("The human plans the lethal projectile"), Fixture.Submit(0, Fixture.Command(Fixture.Humans[0], Fixture.HumanSkillId, Fixture.Enemies[0]))) || !TestTrue(TEXT("The same-time projectiles are locked"), Fixture.Ready(0))) return false;
    Fixture.Round->Tick(0.15f);
    TestEqual(TEXT("Both projectiles are released while both casters live"), Fixture.Round->GetView().PendingProjectiles, 2);
    for (int32 Step = 0; Step < 200 && Fixture.Round->IsRoundSessionActive(); ++Step) Fixture.Round->Tick(0.01f);
    TestTrue(TEXT("Already released attacks may kill both teams"), !Fixture.Humans[0]->IsUnitAlive() && !Fixture.Enemies[0]->IsUnitAlive());
    TestTrue(TEXT("Double wipe finishes with defeat instead of suspending"), Fixture.Round->GetView().Phase == ECombatRoundPhase::Finished && Result == ECombatResult::Defeat && Fixture.Combat->GetCombatResult() == ECombatResult::Defeat);
    Fixture.Round->Tick(1.0f);
    TestEqual(TEXT("The defeat result is emitted exactly once"), ResultCount, 1);
    Fixture.Combat->OnCombatResult.Clear();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundProjectileObstacleTest, "ProjectA.Combat.Round.ProjectileWorldObstacles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundProjectileObstacleTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 4; ++Case)
    {
        FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FIntPoint(0, 0), ETeam::Player);
        AUnitBase* Target = Fixture.AddUnit(FIntPoint(0, 3), ETeam::Enemy);
        if (!Source || !Target) return false;
        const bool bTargetBeforeWall = Case == 2;
        const bool bInitialWallOverlap = Case == 3;
        const FVector WallLocation(0.0f, bInitialWallOverlap ? 0.0f : bTargetBeforeWall ? 580.0f : 300.0f, 100.0f);
        if (!TestNotNull(TEXT("The physical obstacle exists"), Fixture.AddObstacle(WallLocation, FVector(120.0f, 5.0f, 120.0f), ECR_Block, Case == 1 ? ECC_WorldDynamic : ECC_WorldStatic))) return false;
        ACombatRoundProjectile* Projectile = Fixture.World->SpawnActor<ACombatRoundProjectile>(Source->GetActorLocation(), FRotator::ZeroRotator);
        if (!Projectile) return false;
        int32 Impacts = 0;
        int32 Resolutions = 0;
        Projectile->OnImpact.AddLambda([&Impacts](AUnitBase* Caster, AUnitBase* Hit, float Damage)
        {
            ++Impacts;
            UCombatEffectLibrary::ApplyDamageToUnit(Caster, Hit, UGE_Damage::StaticClass(), Damage);
        });
        Projectile->OnResolved.AddLambda([&Resolutions](ACombatRoundProjectile*) { ++Resolutions; });
        Projectile->InitializeProjectile(Source, Target, Target->GetActorLocation(), 1000.0f, 17.0f, 12.0f, 2.0f, false, false);
        Projectile->AdvanceProjectile(1.0f);
        TestEqual(TEXT("The earliest unit or world obstruction resolves the projectile once"), Resolutions, 1);
        TestEqual(TEXT("Only a capsule contact before the wall delivers damage"), Impacts, bTargetBeforeWall ? 1 : 0);
        TestTrue(TEXT("Static, dynamic and initial-overlap walls preserve the protected target's HP"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), bTargetBeforeWall ? 83.0f : 100.0f));
        if (bInitialWallOverlap) TestTrue(TEXT("A projectile already inside a wall does not advance"), Projectile->GetActorLocation().Equals(Source->GetActorLocation()));
        Projectile->AdvanceProjectile(1.0f);
        TestEqual(TEXT("Settled collision never repeats completion"), Resolutions, 1);
        TestEqual(TEXT("Settled collision never repeats damage"), Impacts, bTargetBeforeWall ? 1 : 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundProjectileShapeTest, "ProjectA.Combat.Round.ProjectileCapsuleAndNonblockingShapes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundProjectileShapeTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 4; ++Case)
    {
        FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FIntPoint(0, 0), ETeam::Player);
        AUnitBase* Target = Fixture.AddUnit(FIntPoint(0, 3), ETeam::Enemy);
        if (!Source || !Target) return false;
        const bool bOnlySensorInPath = Case == 2;
        if (!Fixture.AddObstacle(FVector(0.0f, 200.0f, 100.0f), FVector(60.0f, 10.0f, 80.0f), Case == 0 ? ECR_Ignore : ECR_Overlap)) return false;
        ACombatGridTile* PickingTile = Fixture.Grid->GetTileAtCoord(FIntPoint(2, 2));
        PickingTile->SetActorLocation(FVector(0.0f, 300.0f, 100.0f));
        PickingTile->SetActorEnableCollision(true);
        if (bOnlySensorInPath)
        {
            Target->SetActorLocation(FVector(250.0f, 600.0f, 100.0f), false);
            Fixture.AddPawnSensor(Target, FVector(-250.0f, -200.0f, 0.0f), FVector(40.0f));
        }
        if (Case == 3) Fixture.AddPawnSensor(Target, FVector(0.0f, -200.0f, 0.0f), FVector(40.0f));
        ACombatRoundProjectile* Projectile = Fixture.World->SpawnActor<ACombatRoundProjectile>(Source->GetActorLocation(), FRotator::ZeroRotator);
        if (!Projectile) return false;
        AUnitBase* HitUnit = nullptr;
        int32 Resolutions = 0;
        Projectile->OnImpact.AddLambda([&HitUnit](AUnitBase*, AUnitBase* Hit, float) { HitUnit = Hit; });
        Projectile->OnResolved.AddLambda([&Resolutions](ACombatRoundProjectile*) { ++Resolutions; });
        Projectile->InitializeProjectile(Source, Target, FVector(0.0f, 700.0f, 100.0f), 1000.0f, 17.0f, 12.0f, 2.0f, false, false);
        Projectile->AdvanceProjectile(1.0f);
        TestEqual(TEXT("Ignored shapes, trigger overlaps and visibility-only tiles do not block a capsule hit"), HitUnit, bOnlySensorInPath ? nullptr : Target);
        TestEqual(TEXT("A sensor without a capsule intersection cannot create extra impacts"), Resolutions, 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundProjectileRosterTest, "ProjectA.Combat.Round.ProjectileRosterIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundProjectileRosterTest::RunTest(const FString& Parameters)
{
    using namespace CombatRoundTests;
    for (int32 Case = 0; Case < 3; ++Case)
    {
        FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FIntPoint(0, 0), ETeam::Player);
        AUnitBase* Friend = Fixture.AddUnit(FIntPoint(0, 1), ETeam::Player);
        AUnitBase* OtherCombat = Fixture.AddUnit(FIntPoint(0, 2), ETeam::Enemy);
        AUnitBase* Target = Fixture.AddUnit(FIntPoint(0, 3), ETeam::Enemy);
        AUnitBase* Dead = Fixture.AddUnit(FIntPoint(1, 2), ETeam::Enemy);
        if (!Source || !Friend || !OtherCombat || !Target || !Dead) return false;
        Target->UnitIndex = OtherCombat->UnitIndex = 10;
        Dead->SetActorLocation(FVector(0.0f, 300.0f, 100.0f), false);
        Dead->Die();
        Dead->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        OtherCombat->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Dead->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        const float DeadHP = Dead->GetAttributeSet()->GetHP();
        ACombatRoundProjectile* Projectile = Fixture.World->SpawnActor<ACombatRoundProjectile>(Source->GetActorLocation(), FRotator::ZeroRotator);
        if (!Projectile) return false;
        TArray<AUnitBase*> Allowed;
        if (Case != 1) Allowed = {Source, Friend, Dead, Target};
        Projectile->SetAllowedTargets(Allowed);
        AUnitBase* HitUnit = nullptr;
        int32 Resolutions = 0;
        Projectile->OnImpact.AddLambda([&HitUnit](AUnitBase* Caster, AUnitBase* Hit, float Damage)
        {
            HitUnit = Hit;
            UCombatEffectLibrary::ApplyDamageToUnit(Caster, Hit, UGE_Damage::StaticClass(), Damage);
        });
        Projectile->OnResolved.AddLambda([&Resolutions](ACombatRoundProjectile*) { ++Resolutions; });
        Projectile->InitializeProjectile(Source, Target, FVector(0.0f, 700.0f, 100.0f), 1000.0f, 17.0f, 12.0f, 2.0f, false, false);
        if (Case == 2) Projectile->SetAllowedTargets({OtherCombat});
        Projectile->AdvanceProjectile(1.0f);
        TestEqual(TEXT("Only the frozen encounter roster can receive the projectile"), HitUnit, Case == 1 ? nullptr : Target);
        TestEqual(TEXT("A restricted or empty roster still resolves flight once"), Resolutions, 1);
        TestTrue(TEXT("A same-index external enemy and eligible-roster ally or corpse never absorb the attack"), FMath::IsNearlyEqual(OtherCombat->GetAttributeSet()->GetHP(), 100.0f) && FMath::IsNearlyEqual(Friend->GetAttributeSet()->GetHP(), 100.0f) && FMath::IsNearlyEqual(Dead->GetAttributeSet()->GetHP(), DeadHP));
        TestTrue(TEXT("The permitted capsule receives damage once and an empty roster receives none"), FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), Case == 1 ? 100.0f : 83.0f));
    }
    return true;
}

#endif
