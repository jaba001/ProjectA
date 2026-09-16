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
            Skill = FCombatRoundSkill();
            Skill.Kind = ECombatRoundSkillKind::Guard;
            Skill.Approach = ECombatRoundApproach::None;
            Skill.Power = 25.0f;
            Skill.WindupSeconds = 0.15f;
            Skill.HitRange = 1500.0f;
            Skills.Add(MakeSkill(Unit, TEXT("Guard"), Skill));
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
    TestTrue(TEXT("Guard offers a living ally"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Guard")), Friend->UnitIndex));
    TestTrue(TEXT("Guard keeps the existing self-target option"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Guard")), Source->UnitIndex));
    TestFalse(TEXT("Guard excludes enemies"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Guard")), Enemy->UnitIndex));
    TestFalse(TEXT("Unknown source cannot produce target candidates"), Round->IsValidUnitTarget(INDEX_NONE, Fixture.SkillId(Source, TEXT("Strike")), Enemy->UnitIndex));
    TestFalse(TEXT("Unknown skill cannot produce target candidates"), Round->IsValidUnitTarget(Source->UnitIndex, TEXT("Unknown"), Enemy->UnitIndex));
    TestFalse(TEXT("Wait has no unit target choices"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Wait")), Enemy->UnitIndex));
    TestFalse(TEXT("Ground attacks have no unit target choices"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("GroundStrike")), Enemy->UnitIndex));
    FText Error;
    TestTrue(TEXT("Wait is valid without a target"), Round->CanPlanCommand(Fixture.Command(Source, TEXT("Wait")), Error));
    FCombatRoundCommand Ground = Fixture.Command(Source, TEXT("GroundStrike"));
    Ground.TargetCoord = Enemy->GetCurrentTile()->GridCoord;
    Ground.DestinationCoord = Ground.TargetCoord;
    TestTrue(TEXT("Ground attack uses valid coordinates without a unit target"), Round->CanPlanCommand(Ground, Error));
    TestTrue(TEXT("Other units' equipped actions do not replace the passive enemy's internal wait"), Round->GetView().Units.Last().Command.SkillId.IsNone());
    const FCombatRoundCommand Attack = Fixture.Command(Source, TEXT("Strike"), Enemy);
    Enemy->Die();
    TestFalse(TEXT("A dead enemy disappears from attack candidates before another plan is submitted"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Strike")), Enemy->UnitIndex));
    TestFalse(TEXT("A stale attack draft fails preview after target death"), Round->CanPlanCommand(Attack, Error));
    TestFalse(TEXT("The server also rejects the stale target"), Fixture.Submit(0, Attack));
    Friend->Die();
    TestFalse(TEXT("A dead ally disappears from guard candidates"), Round->IsValidUnitTarget(Source->UnitIndex, Fixture.SkillId(Source, TEXT("Guard")), Friend->UnitIndex));
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
    if (!TestTrue(TEXT("Movement fixture starts with zero AP and one SUP"), Source->ConsumeActionPoint(2) && Source->ConsumeSubActionPoint(1))) return false;
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
    if (!TestTrue(TEXT("The original owner can reposition with zero AP and one SUP"), Fixture.Move(0, Source, Destination->GridCoord))) return false;
    TestTrue(TEXT("Accepted movement starts immediately and changes the plan revision"), Round->IsPlanningMoveInProgress() && Round->GetView().PlanRevision > Revision);
    TestEqual(TEXT("SUP movement never consumes AP"), Source->GetCurrentActionPoint(), 0);
    TestEqual(TEXT("Movement consumes exactly one SUP on approval"), Source->GetCurrentSubActionPoint(), 0);
    TestTrue(TEXT("Repositioning clears every human participant's ready state"), !Round->GetView().Units[0].bReady && !Round->GetView().Units[1].bReady);
    TestFalse(TEXT("Commands cannot change during repositioning"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Wait"))));
    TestFalse(TEXT("The moving participant cannot ready early"), Fixture.Ready(0));
    TestFalse(TEXT("Another participant cannot ready during repositioning"), Fixture.Ready(1));
    TestFalse(TEXT("Only one planning movement can run at a time"), Fixture.Move(1, Friend, FIntPoint(3, 1)));
    Round->Tick(0.05f);
    TestTrue(TEXT("Movement advances through world space before reaching its destination"), !Source->GetActorLocation().Equals(OriginalLocation, 1.0f) && Round->IsPlanningMoveInProgress());
    TestTrue(TEXT("The origin remains occupied until movement completes"), Origin->GetOccupyingUnit() == Source && Source->GetCurrentTile() == Origin && Destination->GetOccupyingUnit() == nullptr);
    for (int32 Step = 0; Step < 200 && Round->IsPlanningMoveInProgress(); ++Step) Round->Tick(0.01f);
    if (!TestFalse(TEXT("The short movement finishes in bounded time"), Round->IsPlanningMoveInProgress())) return false;
    const FVector NewHome = Destination->GetActorLocation() + FVector(0.0f, 0.0f, OriginalLocation.Z);
    TestTrue(TEXT("Arrival transfers grid occupancy and the unit's tile together"), Origin->GetOccupyingUnit() == nullptr && Destination->GetOccupyingUnit() == Source && Source->GetCurrentTile() == Destination);
    TestTrue(TEXT("Arrival publishes the new home coordinate"), Round->GetView().Units[0].HomeCoord == Destination->GridCoord);
    TestTrue(TEXT("Arrival preserves height and restores the original facing"), Source->GetActorLocation().Equals(NewHome, 2.0f) && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
    TestFalse(TEXT("A second move is unavailable after SUP is spent"), Round->CanMoveUnit(Source->UnitIndex, Origin->GridCoord, Error));
    if (!TestTrue(TEXT("The mover reapplies a wait at the new home"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Wait")))) || !TestTrue(TEXT("The second owner reapplies its wait"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The mover readies after arriving"), Fixture.Ready(0)) || !TestTrue(TEXT("The second owner readies after arriving"), Fixture.Ready(1))) return false;
    if (!TestTrue(TEXT("Repositioning does not prevent the round from settling"), Fixture.AdvanceUntilNextRound(1))) return false;
    TestEqual(TEXT("Planning movement does not deal attack damage"), Enemy->GetAttributeSet()->GetHP(), 100.0f);
    if (!TestTrue(TEXT("A later attack starts from the repositioned home"), Fixture.Submit(0, Fixture.Command(Source, TEXT("Strike"), Enemy))) || !TestTrue(TEXT("The second owner waits for the attack"), Fixture.Submit(1, Fixture.Command(Friend, TEXT("Wait")))) || !TestTrue(TEXT("The attacking owner readies"), Fixture.Ready(0)) || !TestTrue(TEXT("The waiting owner readies"), Fixture.Ready(1))) return false;
    if (!TestTrue(TEXT("The attack approaches, releases, and returns within a bounded round"), Fixture.AdvanceUntilNextRound(2))) return false;
    TestEqual(TEXT("The attack still applies its authored damage once"), Enemy->GetAttributeSet()->GetHP(), 75.0f);
    TestTrue(TEXT("The attack returns to the SUP destination rather than the previous home"), Source->GetActorLocation().Equals(NewHome, 2.0f) && Source->GetCurrentTile() == Destination && Round->GetView().Units[0].HomeCoord == Destination->GridCoord);
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
    const int32 OriginalSUP = Source->GetCurrentSubActionPoint();
    if (!TestTrue(TEXT("The reachable route starts moving"), Fixture.Move(0, Source, Destination->GridCoord))) return false;
    Round->Tick(0.05f);
    if (!TestTrue(TEXT("Suspension happens during real movement"), Round->IsPlanningMoveInProgress() && !Source->GetActorLocation().Equals(OriginalLocation, 1.0f))) return false;
    Round->SuspendRound();
    TestTrue(TEXT("Suspension ends the active move"), Round->GetView().Phase == ECombatRoundPhase::Suspended && !Round->IsPlanningMoveInProgress());
    TestTrue(TEXT("Interrupted movement restores the original position and facing"), Source->GetActorLocation().Equals(OriginalLocation, 0.1f) && Source->GetActorRotation().Equals(OriginalRotation, 0.1f));
    TestTrue(TEXT("Interrupted movement retains its original home and occupancy"), Source->GetCurrentTile() == Origin && Origin->GetOccupyingUnit() == Source && Destination->GetOccupyingUnit() == nullptr && Round->GetView().Units[0].HomeCoord == Origin->GridCoord);
    TestEqual(TEXT("An interrupted move does not refund the spent SUP"), Source->GetCurrentSubActionPoint(), OriginalSUP - 1);
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
        TestTrue(Case == 0 ? TEXT("Authored resident movement leaves only an internal AI wait") : TEXT("Authored support leaves only an internal AI wait"), Fixture.Round->GetView().Units.Last().Command.SkillId.IsNone());
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
    AddExpectedError(TEXT("[RoundAnimation] Montage playback failed"), EAutomationExpectedErrorFlags::Contains, 4, false);
    for (int32 Case = 0; Case < 5; ++Case)
    {
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
        if (Case == 4)
        {
            Fixture.Round->Tick(2.0f);
            if (!TestTrue(TEXT("The hitch leaves simulation debt while still approaching"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Approaching && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 100.0f))) return false;
            Fixture.Round->Tick(0.01f);
            if (!TestTrue(TEXT("Catching up debt can start the montage and release one hit in the next frame"), Fixture.Round->GetView().Units[0].ActionPhase == ECombatRoundActionPhase::Recovery && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), 83.0f))) return false;
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
