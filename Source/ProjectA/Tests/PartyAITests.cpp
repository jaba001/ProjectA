#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/AI/PartyAutoCombatComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"

namespace PartyAITests
{
    struct FFixture
    {
        TStrongObjectPtr<UWorld> World;
        ACombatManager* Combat = nullptr;
        ACombatGridManager* Grid = nullptr;
        APlayerUnit* First = nullptr;
        APlayerUnit* Second = nullptr;
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
            Combat->ResetCombat();
            World->DestroyWorld(false);
        }

        ACombatGridTile* AddTile(FIntPoint Coord, ETileTerritory Territory)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(FVector(Coord.X * 200.0f, Coord.Y * 200.0f, 0.0f), FRotator::ZeroRotator, Params);
            Tile->InitializeGridTile(Grid, Coord, Territory);
            return Tile;
        }

        template <typename T>
        T* AddUnit(ACombatGridTile* Tile, ETeam Team)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            T* Unit = World->SpawnActor<T>(Tile->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Params);
            Unit->SetTeam(Team);
            Unit->SetCurrentTile(Tile);
            UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
            ASC->InitAbilityActorInfo(Unit, Unit);
            ASC->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            return Unit;
        }

        APartyPlayerController* AddController(bool bLocal)
        {
            APartyPlayerController* Controller = World->SpawnActor<APartyPlayerController>();
            World->AddController(Controller);
            if (bLocal) Controller->SetAsLocalPlayerController();
            Controller->SetCombatContext(Combat, true);
            return Controller;
        }

        bool Initialize(ERunAIConsent Consent = ERunAIConsent::Unknown)
        {
            FirstTile = AddTile(FIntPoint(0, 0), ETileTerritory::Player);
            First = AddUnit<APlayerUnit>(FirstTile, ETeam::Player);
            Second = AddUnit<APlayerUnit>(AddTile(FIntPoint(2, 0), ETileTerritory::Player), ETeam::Player);
            EnemyTile = AddTile(FIntPoint(0, 3), ETileTerritory::Enemy);
            Enemy = AddUnit<AUnitBase>(EnemyTile, ETeam::Enemy);
            MoveTile = AddTile(FIntPoint(0, 1), ETileTerritory::Player);
            Skill = NewObject<USkillDefinitionDataAsset>(First);
            Skill->AbilityClass = UGA_DefaultAttack::StaticClass();
            Skill->TargetRule = ESkillTargetRule::EnemyUnit;
            Skill->ActionPointCost = 1;
            if (!First->ConfigureProfession(100.0f, 4, 2, { Skill })) return false;
            Host = AddController(true);
            Guest = AddController(false);
            Identity.Origin = ERunIdentityOrigin::AccountProvider;
            Identity.RunId = FGuid::NewGuid();
            Identity.HostEpoch = 1;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
                Participant.AccountId.Provider = TEXT("PartyAIFixture");
                Participant.AccountId.Subject = Index == 0 ? TEXT("OriginalHost") : TEXT("OriginalGuest");
                Participant.AIConsent = Consent;
                Participant.ConsentPolicyVersion = Consent == ERunAIConsent::Unknown ? 0 : 1;
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.CharacterName = FText::FromString(Participant.AccountId.Subject);
                Member.ClassId = TEXT("Archer");
                Member.bCreated = true;
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
                PartyActors.Add(Index, Index == 0 ? First : Second);
            }
            Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
            Combat->RegisterUnits({ First, Second, Enemy });
            return Reconfigure();
        }

        bool Reconfigure()
        {
            return Authority()->ConfigureRun(Identity, Party, PartyActors, Error) && Authority()->BindParticipant(Host, Identity.OriginalParticipants[0].AccountId) && Authority()->BindParticipant(Guest, Identity.OriginalParticipants[1].AccountId);
        }

        UCombatActionAuthority* Authority() const { return Combat->GetActionAuthority(); }
        UPartyAutoCombatComponent* Brain() const { return First->FindComponentByClass<UPartyAutoCombatComponent>(); }

    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAIControlTest, "ProjectA.Coop.PartyAI.ControlAndOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartyAIControlTest::RunTest(const FString& Parameters)
{
    using namespace PartyAITests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Identified ownership fixture initializes"), Fixture.Initialize(ERunAIConsent::Unknown))) return false;
    const FRunAccountId Owner = Fixture.Authority()->GetOwnerAccountId(Fixture.First);
    const FGuid Character = Fixture.Authority()->GetCharacterId(Fixture.First);
    TestTrue(TEXT("Server can prepare AI with Unknown legacy consent"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    TestTrue(TEXT("Idle preparation can restore Human before combat starts"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::Human, Fixture.Error));
    Fixture.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Declined;
    Fixture.Identity.OriginalParticipants[0].ConsentPolicyVersion = 1;
    TestTrue(TEXT("Declined legacy consent remains valid ownership data"), Fixture.Reconfigure());
    Fixture.Combat->SetRole(ROLE_SimulatedProxy);
    TestFalse(TEXT("Client manager cannot change control mode"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    Fixture.Combat->SetRole(ROLE_Authority);
    TestTrue(TEXT("Server can prepare AI with Declined legacy consent"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    const FGuid Session = Fixture.First->GetAIControlSessionId();
    TestTrue(TEXT("AI mode carries a distinct server control session"), Session.IsValid());
    TestEqual(TEXT("AI keeps original character identity"), Fixture.Authority()->GetCharacterId(Fixture.First), Character);
    TestTrue(TEXT("AI keeps original owner"), Fixture.Authority()->GetOwnerAccountId(Fixture.First) == Owner);
    TestEqual(TEXT("AI remains on player team"), Fixture.First->GetTeam(), ETeam::Player);
    TestFalse(TEXT("Original owner cannot directly command its AI character"), Fixture.Authority()->CanControllerControl(Fixture.Host, Fixture.First));
    TestFalse(TEXT("Another participant cannot directly command the AI character"), Fixture.Authority()->CanControllerControl(Fixture.Guest, Fixture.First));
    TestNull(TEXT("Old sequential AI component is no longer instantiated"), Fixture.Brain());
    const float HP = Fixture.Enemy->GetAttributeSet()->GetHP();
    Fixture.First->OnTurnStart();
    TestFalse(TEXT("Legacy OnTurnStart cannot start sequential AI"), Fixture.First->IsActiveTurn());
    TestEqual(TEXT("Legacy AI entry causes no immediate damage"), Fixture.Enemy->GetAttributeSet()->GetHP(), HP);
    FCombatActionRequest Request;
    Request.UnitId = Fixture.Authority()->GetUnitId(Fixture.First);
    Request.Kind = ECombatActionKind::Skill;
    TestEqual(TEXT("Legacy AI execution API rejects"), Fixture.Authority()->ExecuteServerAI(Fixture.First, Request, Session).Result, ECombatRequestResult::InvalidRequest);
    Fixture.Combat->SuspendCombatForRecovery();
    TestFalse(TEXT("Suspended combat cannot change AI mode"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::Human, Fixture.Error));
    TestFalse(TEXT("Suspended combat cannot rewrite Run ownership"), Fixture.Authority()->ConfigureRun(Fixture.Identity, Fixture.Party, Fixture.PartyActors, Fixture.Error));
    TestTrue(TEXT("Rejected changes preserve original owner"), Fixture.Authority()->GetOwnerAccountId(Fixture.First) == Owner);
    TestEqual(TEXT("Rejected changes preserve AI session"), Fixture.First->GetAIControlSessionId(), Session);
    return true;
}

#endif
