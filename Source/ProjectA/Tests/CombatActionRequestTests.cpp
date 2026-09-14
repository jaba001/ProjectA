#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

namespace CombatActionRequestTests
{
    struct FFixture
    {
        TStrongObjectPtr<UWorld> World;
        ACombatManager* Combat = nullptr;
        ACombatGridManager* Grid = nullptr;
        AUnitBase* First = nullptr;
        AUnitBase* Second = nullptr;
        AUnitBase* Enemy = nullptr;
        ACombatGridTile* FirstTile = nullptr;
        ACombatGridTile* EnemyTile = nullptr;
        ACombatGridTile* MoveTile = nullptr;
        APartyPlayerController* Host = nullptr;
        APartyPlayerController* Guest = nullptr;
        USkillDefinitionDataAsset* Skill = nullptr;
        FRunIdentityData Identity;
        TArray<FRunPartyMember> Party;
        TMap<int32, TObjectPtr<AUnitBase>> PartyActors;
        FText Error;

        FFixture()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
            Combat = World->SpawnActor<ACombatManager>();
            Grid = World->SpawnActor<ACombatGridManager>();
            Combat->SetCombatGrid(Grid);
        }

        ~FFixture()
        {
            if (Combat)
            {
                Combat->ResetCombat();
            }
            World->DestroyWorld(false);
        }

        ACombatGridTile* AddTile(FIntPoint Coord, ETileTerritory Territory)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(FVector(Coord.X * 200.0f, Coord.Y * 200.0f, 0.0f), FRotator::ZeroRotator, Parameters);
            Tile->GridCoord = Coord;
            Tile->SetTerritory(Territory);
            Grid->TileMap.Add(Coord, Tile);
            return Tile;
        }

        AUnitBase* AddUnit(ACombatGridTile* Tile, ETeam Team)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnitBase* Unit = World->SpawnActor<AUnitBase>(Tile->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Parameters);
            Unit->SetTeam(Team);
            Unit->SetCurrentTile(Tile);
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            return Unit;
        }

        APartyPlayerController* AddController()
        {
            APartyPlayerController* Controller = World->SpawnActor<APartyPlayerController>();
            World->AddController(Controller);
            Controller->SetCombatContext(Combat, true);
            return Controller;
        }

        bool Initialize(bool bIdentified = true)
        {
            FirstTile = AddTile(FIntPoint(0, 0), ETileTerritory::Player);
            First = AddUnit(FirstTile, ETeam::Player);
            Second = AddUnit(AddTile(FIntPoint(2, 0), ETileTerritory::Player), ETeam::Player);
            EnemyTile = AddTile(FIntPoint(0, 3), ETileTerritory::Enemy);
            Enemy = AddUnit(EnemyTile, ETeam::Enemy);
            MoveTile = AddTile(FIntPoint(1, 0), ETileTerritory::Player);
            Skill = NewObject<USkillDefinitionDataAsset>(First);
            Skill->AbilityClass = UGA_DefaultAttack::StaticClass();
            Skill->bMoveToTarget = false;
            Skill->TargetRule = ESkillTargetRule::EnemyUnit;
            Skill->ActionPointCost = 1;
            if (!First->ConfigureProfession(100.0f, 4, 2, {Skill}))
            {
                return false;
            }
            Host = AddController();
            // The isolated world skips GameMode's local-player setup; only the Host represents local input.
            // 격리 월드는 GameMode의 로컬 플레이어 설정을 생략하므로 Host만 로컬 입력으로 지정합니다.
            Host->SetAsLocalPlayerController();
            Guest = AddController();

            // Provider-shaped fixtures exercise stored ownership, without claiming account authentication.
            // 공급자 형식의 테스트 데이터로 저장 소유권을 검증하며 계정 인증 성공을 뜻하지 않습니다.
            Identity.Origin = ERunIdentityOrigin::AccountProvider;
            Identity.RunId = FGuid::NewGuid();
            Identity.HostEpoch = 1;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
                Participant.AccountId.Provider = TEXT("FixtureProvider");
                Participant.AccountId.Subject = Index == 0 ? TEXT("HostOwner") : TEXT("GuestOwner");
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.CharacterName = FText::FromString(Participant.AccountId.Subject);
                Member.ClassId = TEXT("Hunter");
                Member.bCreated = true;
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
                PartyActors.Add(Index, Index == 0 ? First : Second);
            }
            Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
            Combat->RegisterUnits({First, Second, Enemy});
            if (bIdentified && (!Authority()->ConfigureRun(Identity, Party, PartyActors, Error) || !BindOwners()))
            {
                return false;
            }
            return Authority()->GetUnitId(First).IsValid();
        }

        UCombatActionAuthority* Authority() const
        {
            return Combat->GetActionAuthority();
        }

        bool BindOwners()
        {
            return Authority()->BindParticipant(Host, Identity.OriginalParticipants[0].AccountId) && Authority()->BindParticipant(Guest, Identity.OriginalParticipants[1].AccountId);
        }

    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRequestOwnershipTest, "ProjectA.Combat.Requests.OriginalOwnershipAndConnectionNonce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRequestOwnershipTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Identified ownership fixture initializes"), Fixture.Initialize())) return false;
    UCombatActionAuthority* Authority = Fixture.Authority();
    const FGuid Binding = Authority->GetParticipantBindingId(Fixture.Host);
    TestTrue(TEXT("Binding has a connection nonce"), Binding.IsValid());
    TestTrue(TEXT("Host controls its original character"), Authority->CanControllerControl(Fixture.Host, Fixture.First));
    TestTrue(TEXT("Guest controls its original character"), Authority->CanControllerControl(Fixture.Guest, Fixture.Second));
    TestFalse(TEXT("Host cannot control another participant character"), Authority->CanControllerControl(Fixture.Host, Fixture.Second));
    TestFalse(TEXT("Guest cannot control another participant character"), Authority->CanControllerControl(Fixture.Guest, Fixture.First));
    TestFalse(TEXT("Enemies never accept human ownership"), Authority->CanControllerControl(Fixture.Host, Fixture.Enemy));
    TestEqual(TEXT("Runtime ID resolves to the registered unit"), Authority->ResolveUnit(Authority->GetUnitId(Fixture.First)), Fixture.First);
    TestEqual(TEXT("Character identity remains distinct from runtime identity"), Authority->GetCharacterId(Fixture.First), Fixture.Party[0].CharacterId);
    Fixture.Host->SetCombatContext(Fixture.Combat, false);
    TestTrue(TEXT("Planning ownership is independent of obsolete immediate input enablement"), Authority->CanControllerControl(Fixture.Host, Fixture.First));
    TestTrue(TEXT("Repeated trusted binding is idempotent"), Authority->BindParticipant(Fixture.Host, Fixture.Identity.HostAccountId));
    TestEqual(TEXT("Idempotent binding retains its nonce"), Authority->GetParticipantBindingId(Fixture.Host), Binding);
    TestFalse(TEXT("One connection cannot switch accounts"), Authority->BindParticipant(Fixture.Host, Fixture.Identity.OriginalParticipants[1].AccountId));
    APartyPlayerController* Duplicate = Fixture.AddController();
    TestFalse(TEXT("A second connection cannot take the same account"), Authority->BindParticipant(Duplicate, Fixture.Identity.HostAccountId));
    FRunAccountId Replacement = Fixture.Identity.HostAccountId;
    Replacement.Subject = TEXT("Replacement");
    TestFalse(TEXT("Replacement participants cannot join the original Run"), Authority->BindParticipant(Duplicate, Replacement));
    Fixture.World->RemoveController(Fixture.Host);
    Fixture.Host->Destroy();
    TestTrue(TEXT("Disconnected original owner can bind a fresh connection"), Authority->BindParticipant(Duplicate, Fixture.Identity.HostAccountId));
    TestTrue(TEXT("Reconnection rotates the nonce"), Authority->GetParticipantBindingId(Duplicate).IsValid() && Authority->GetParticipantBindingId(Duplicate) != Binding);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLegacyRequestRejectionTest, "ProjectA.Combat.Requests.RetiredImmediateCommandsReject", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatLegacyRequestRejectionTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Request fixture initializes"), Fixture.Initialize())) return false;
    const float HP = Fixture.Enemy->GetAttributeSet()->GetHP();
    const int32 AP = Fixture.First->GetCurrentActionPoint();
    const FVector Location = Fixture.First->GetActorLocation();
    for (ECombatActionKind Kind : { ECombatActionKind::Move, ECombatActionKind::Skill, ECombatActionKind::HealingItem, ECombatActionKind::EndTurn })
    {
        FCombatActionRequest Request;
        Request.Kind = Kind;
        Request.RunId = Fixture.Identity.RunId;
        Request.HostEpoch = Fixture.Identity.HostEpoch;
        Request.ParticipantBindingId = Fixture.Authority()->GetParticipantBindingId(Fixture.Host);
        Request.UnitId = Fixture.Authority()->GetUnitId(Fixture.First);
        Request.TargetUnitId = Fixture.Authority()->GetUnitId(Fixture.Enemy);
        Request.SkillId = Fixture.Skill->GetPrimaryAssetId();
        Request.TargetCoord = Fixture.EnemyTile->GridCoord;
        Request.RequestSequence = 1;
        const FCombatActionResponse Response = Fixture.Authority()->Execute(Fixture.Host, Request);
        TestEqual(TEXT("Retired command is explicitly rejected"), Response.Result, ECombatRequestResult::InvalidRequest);
        TestFalse(TEXT("Rejection explains unsupported command"), Response.Message.IsEmpty());
        TestTrue(TEXT("Controller cannot dispatch the retired command"), Fixture.Host->SubmitCombatActionRequest(Request).Result != ECombatRequestResult::Accepted);
    }
    TestEqual(TEXT("Rejected commands preserve enemy HP"), Fixture.Enemy->GetAttributeSet()->GetHP(), HP);
    TestEqual(TEXT("Rejected commands preserve AP"), Fixture.First->GetCurrentActionPoint(), AP);
    TestEqual(TEXT("Rejected movement preserves location"), Fixture.First->GetActorLocation(), Location);
    TestFalse(TEXT("Old end-turn adapter cannot advance combat"), Fixture.Combat->RequestEndTurnForUnit(Fixture.First));
    TestNull(TEXT("No sequential turn manager is allocated"), Fixture.Combat->GetTurnManager());
    Fixture.Authority()->Reset();
    TestFalse(TEXT("Reset invalidates original ownership bindings"), Fixture.Authority()->CanControllerControl(Fixture.Host, Fixture.First));
    TestFalse(TEXT("Reset discards runtime IDs"), Fixture.Authority()->GetUnitId(Fixture.First).IsValid());
    TestFalse(TEXT("Reset discards connection nonces"), Fixture.Authority()->GetParticipantBindingId(Fixture.Host).IsValid());
    return true;
}

#endif
