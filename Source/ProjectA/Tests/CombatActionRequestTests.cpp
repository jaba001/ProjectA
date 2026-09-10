#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

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
            UGameplayAbility* Ability = First->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Skill->AbilityClass)->GetPrimaryInstance();
            FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("DamageEffectClass"))->SetObjectPropertyValue_InContainer(Ability, UGE_Damage::StaticClass());
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
            Combat->StartCombat_Internal();
            return Combat->IsCombatActive() && Combat->GetCurrentUnit() == First && Authority()->GetCombatInstanceId().IsValid();
        }

        UCombatActionAuthority* Authority() const
        {
            return Combat->GetActionAuthority();
        }

        bool BindOwners()
        {
            return Authority()->BindParticipant(Host, Identity.OriginalParticipants[0].AccountId) && Authority()->BindParticipant(Guest, Identity.OriginalParticipants[1].AccountId);
        }

        FCombatActionRequest Make(ECombatActionKind Kind, ACombatGridTile* Target = nullptr, USkillDefinitionDataAsset* RequestedSkill = nullptr, APartyPlayerController* Sender = nullptr)
        {
            FCombatActionRequest Request;
            (Sender ? Sender : Host)->BuildCombatActionRequest(Kind, RequestedSkill, Target, Request);
            return Request;
        }

        FCombatActionResponse Submit(const FCombatActionRequest& Request, APartyPlayerController* Sender = nullptr)
        {
            return (Sender ? Sender : Host)->SubmitCombatActionRequest(Request);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRequestDispatchTest, "ProjectA.Combat.Requests.DispatchAndReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRequestDispatchTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Identified authority fixture initializes"), Fixture.Initialize()))
    {
        AddError(Fixture.Error.ToString());
        return false;
    }
    const FCombatActionRequest Skill = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    const FDelegateHandle SelectionChanged = Fixture.First->OnActionCompleted.AddLambda([Controller = Fixture.Host](AUnitBase*, EUnitActionType, EUnitActionResult)
    {
        Controller->SetTileInputMode(ETileInputMode::Move);
    });
    TestTrue(TEXT("Built command contains a unit ID"), Skill.UnitId.IsValid());
    TestEqual(TEXT("Skill request dispatches"), Fixture.Submit(Skill).Result, ECombatRequestResult::Accepted);
    Fixture.First->OnActionCompleted.Remove(SelectionChanged);
    TestTrue(TEXT("A selection made during synchronous completion survives the response"), Fixture.Host->IsMoveInputMode());
    TestEqual(TEXT("Controller receives the same authority response"), Fixture.Host->GetLastCombatActionResponse().Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("Dispatched skill applies actual GAS damage"), Fixture.Enemy->GetAttributeSet()->GetHP(), 90.0f);
    TestEqual(TEXT("Dispatched skill charges server AP once"), Fixture.First->GetCurrentActionPoint(), 3);
    TestEqual(TEXT("Replayed skill is rejected"), Fixture.Submit(Skill).Result, ECombatRequestResult::DuplicateRequest);
    TestEqual(TEXT("Replay cannot damage again"), Fixture.Enemy->GetAttributeSet()->GetHP(), 90.0f);

    Fixture.First->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 20.0f);
    const FCombatActionRequest Item = Fixture.Make(ECombatActionKind::HealingItem, Fixture.FirstTile);
    TestEqual(TEXT("Healing item dispatches through the controller"), Fixture.Submit(Item).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("Healing item changes real HP"), Fixture.First->GetAttributeSet()->GetHP(), 60.0f);
    TestEqual(TEXT("Healing item consumes one stock"), Fixture.First->HealingItemCount, 0);
    TestEqual(TEXT("Healing item consumes one SubAP"), Fixture.First->GetCurrentSubActionPoint(), 1);
    TestEqual(TEXT("Replayed item is rejected"), Fixture.Submit(Item).Result, ECombatRequestResult::DuplicateRequest);
    Fixture.Host->SetTileInputMode(ETileInputMode::Move);
    FCombatActionResponse OldResponse;
    OldResponse.CombatInstanceId = Skill.CombatInstanceId;
    OldResponse.RequestSequence = Skill.RequestSequence;
    OldResponse.Result = ECombatRequestResult::Accepted;
    Fixture.Host->ClientReceiveCombatActionResponse_Implementation(OldResponse);
    TestTrue(TEXT("An old accepted response preserves a newer selection"), Fixture.Host->IsMoveInputMode());
    TestEqual(TEXT("An old response cannot replace newer response feedback"), Fixture.Host->GetLastCombatActionResponse().RequestSequence, Item.RequestSequence);

    const FCombatActionRequest Move = Fixture.Make(ECombatActionKind::Move, Fixture.MoveTile);
    TestEqual(TEXT("Reachable movement request dispatches"), Fixture.Submit(Move).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("Movement charges one SubAP"), Fixture.First->GetCurrentSubActionPoint(), 0);
    // Navigation is absent in this fixture; verify dispatch and failure recovery, not travel success.
    // 이 테스트에는 내비게이션이 없으므로 실제 도착이 아닌 요청 실행과 실패 복구를 검증합니다.
    if (Fixture.First->IsBusy())
    {
        Fixture.First->HandleMoveFailed();
    }
    TestFalse(TEXT("Failed movement releases action state"), Fixture.First->IsBusy());
    TestEqual(TEXT("Failed movement preserves original tile occupancy"), Fixture.FirstTile->GetOccupyingUnit(), Fixture.First);
    TestEqual(TEXT("Movement replay cannot charge twice"), Fixture.Submit(Move).Result, ECombatRequestResult::DuplicateRequest);
    const int32 TurnBefore = Fixture.Combat->GetTurnManager()->GetTurnCounter();
    const FCombatActionRequest End = Fixture.Make(ECombatActionKind::EndTurn);
    TestEqual(TEXT("End-turn request dispatches"), Fixture.Submit(End).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("End turn advances exactly once"), Fixture.Combat->GetTurnManager()->GetTurnCounter(), TurnBefore + 1);
    TestEqual(TEXT("The next participant's unit becomes active"), Fixture.Combat->GetCurrentUnit(), Fixture.Second);
    TestTrue(TEXT("Old end-turn request is rejected"), Fixture.Submit(End).Result != ECombatRequestResult::Accepted);
    TestEqual(TEXT("End-turn replay does not advance another unit"), Fixture.Combat->GetTurnManager()->GetTurnCounter(), TurnBefore + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRequestOwnershipTest, "ProjectA.Combat.Requests.OriginalOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRequestOwnershipTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Ownership fixture initializes"), Fixture.Initialize()))
    {
        return false;
    }
    TestEqual(TEXT("Registered runtime unit ID resolves"), Fixture.Authority()->ResolveUnit(Fixture.Authority()->GetUnitId(Fixture.First)), Fixture.First);
    TestNull(TEXT("Unknown runtime unit ID cannot resolve"), Fixture.Authority()->ResolveUnit(FGuid::NewGuid()));
    TestTrue(TEXT("Original Host controls its active character"), Fixture.Authority()->CanControllerControl(Fixture.Host, Fixture.First));
    TestFalse(TEXT("Guest cannot control the Host's character"), Fixture.Authority()->CanControllerControl(Fixture.Guest, Fixture.First));
    const FGuid Binding = Fixture.Authority()->GetParticipantBindingId(Fixture.Host);
    TestTrue(TEXT("Identified connection receives a server binding nonce"), Binding.IsValid());
    TestTrue(TEXT("Repeated binding of the same connection is idempotent"), Fixture.Authority()->BindParticipant(Fixture.Host, Fixture.Identity.HostAccountId));
    TestTrue(TEXT("Idempotent binding preserves its nonce"), Fixture.Authority()->GetParticipantBindingId(Fixture.Host) == Binding);
    TestFalse(TEXT("A bound connection cannot switch account identities"), Fixture.Authority()->BindParticipant(Fixture.Host, Fixture.Identity.OriginalParticipants[1].AccountId));
    APartyPlayerController* Extra = Fixture.AddController();
    TestFalse(TEXT("The same account cannot bind a second controller"), Fixture.Authority()->BindParticipant(Extra, Fixture.Identity.HostAccountId));
    FRunAccountId Replacement = Fixture.Identity.HostAccountId;
    Replacement.Subject = TEXT("Replacement");
    TestFalse(TEXT("Replacement account cannot bind to the Run"), Fixture.Authority()->BindParticipant(Extra, Replacement));
    TestEqual(TEXT("Unbound controller cannot submit a command"), Fixture.Submit(Fixture.Make(ECombatActionKind::EndTurn, nullptr, nullptr, Extra), Extra).Result, ECombatRequestResult::UnboundParticipant);
    TestEqual(TEXT("Other owner cannot submit the active character's command"), Fixture.Submit(Fixture.Make(ECombatActionKind::EndTurn, nullptr, nullptr, Fixture.Guest), Fixture.Guest).Result, ECombatRequestResult::NotOwner);
    TestEqual(TEXT("Rejected ownership requests preserve the turn"), Fixture.Combat->GetCurrentUnit(), Fixture.First);
    TestTrue(TEXT("Internal completion remains separate from human ownership"), Fixture.Combat->RequestEndTurnForUnit(Fixture.First));
    TestTrue(TEXT("Guest controls its own active character"), Fixture.Authority()->CanControllerControl(Fixture.Guest, Fixture.Second));
    TestEqual(TEXT("Host has no exception for another participant's character"), Fixture.Submit(Fixture.Make(ECombatActionKind::EndTurn)).Result, ECombatRequestResult::NotOwner);
    Fixture.Guest->SetCombatContext(Fixture.Combat, false);
    TestTrue(TEXT("Disabled human input rejects a direct command"), Fixture.Submit(Fixture.Make(ECombatActionKind::EndTurn, nullptr, nullptr, Fixture.Guest), Fixture.Guest).Result != ECombatRequestResult::Accepted);
    TestTrue(TEXT("Disabled human input does not block internal completion"), Fixture.Combat->RequestEndTurnForUnit(Fixture.Second));
    TestEqual(TEXT("Internal completion reaches the enemy turn"), Fixture.Combat->GetCurrentUnit(), Fixture.Enemy);
    TestTrue(TEXT("Internal enemy completion returns to the first owner"), Fixture.Combat->RequestEndTurnForUnit(Fixture.Enemy));
    const FCombatActionRequest PreviousConnection = Fixture.Make(ECombatActionKind::EndTurn);
    Fixture.Host->Destroy();
    Fixture.Host = Fixture.AddController();
    Fixture.Host->SetAsLocalPlayerController();
    TestTrue(TEXT("Original account can bind a replacement connection"), Fixture.Authority()->BindParticipant(Fixture.Host, Fixture.Identity.HostAccountId));
    TestTrue(TEXT("Replacement connection receives a different binding nonce"), Fixture.Authority()->GetParticipantBindingId(Fixture.Host) != Binding);
    TestEqual(TEXT("Old connection body cannot execute after reconnection"), Fixture.Submit(PreviousConnection).Result, ECombatRequestResult::InvalidContext);
    TestEqual(TEXT("Reconnection accepts a fresh sequence with the new binding"), Fixture.Submit(Fixture.Make(ECombatActionKind::EndTurn)).Result, ECombatRequestResult::Accepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRequestContextTest, "ProjectA.Combat.Requests.ContextAndSequence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRequestContextTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Context fixture initializes"), Fixture.Initialize()))
    {
        return false;
    }
    const FCombatActionRequest Valid = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    FCombatActionRequest Invalid = Valid;
    Invalid.RunId = FGuid::NewGuid();
    TestEqual(TEXT("Wrong Run rejects the request"), Fixture.Submit(Invalid).Result, ECombatRequestResult::InvalidContext);
    Invalid = Valid;
    ++Invalid.HostEpoch;
    TestEqual(TEXT("Wrong Host generation rejects the request"), Fixture.Submit(Invalid).Result, ECombatRequestResult::InvalidContext);
    Invalid = Valid;
    Invalid.CombatInstanceId = FGuid::NewGuid();
    TestEqual(TEXT("Wrong combat instance rejects the request"), Fixture.Submit(Invalid).Result, ECombatRequestResult::InvalidContext);
    TestEqual(TEXT("Wrong-context requests do not consume a valid sequence"), Fixture.Submit(Valid).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("Context rejections cause no extra damage"), Fixture.Enemy->GetAttributeSet()->GetHP(), 90.0f);

    const FCombatActionRequest Locked = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    const int32 APBeforeLock = Fixture.First->GetCurrentActionPoint();
    const int32 TurnBeforeLock = Fixture.Combat->GetTurnManager()->GetTurnCounter();
    const float HPBeforeLock = Fixture.Enemy->GetAttributeSet()->GetHP();
    int32 LockedCompletions = 0;
    const FDelegateHandle LockedAction = Fixture.First->OnActionCompleted.AddLambda([&LockedCompletions](AUnitBase*, EUnitActionType, EUnitActionResult)
    {
        ++LockedCompletions;
    });
    Fixture.Host->SetCombatContext(Fixture.Combat, false);
    TestEqual(TEXT("Input lock rejects an otherwise valid command"), Fixture.Submit(Locked).Result, ECombatRequestResult::InvalidContext);
    Fixture.Host->SetCombatContext(Fixture.Combat, true);
    TestEqual(TEXT("Unlocking input cannot replay a rejected command"), Fixture.Submit(Locked).Result, ECombatRequestResult::DuplicateRequest);
    Fixture.First->OnActionCompleted.Remove(LockedAction);
    TestEqual(TEXT("Input-lock rejection and replay dispatch no unit action"), LockedCompletions, 0);
    TestEqual(TEXT("Input-lock rejection and replay preserve AP"), Fixture.First->GetCurrentActionPoint(), APBeforeLock);
    TestEqual(TEXT("Input-lock rejection and replay preserve target HP"), Fixture.Enemy->GetAttributeSet()->GetHP(), HPBeforeLock);
    TestEqual(TEXT("Input-lock rejection and replay preserve turn serial"), Fixture.Combat->GetTurnManager()->GetTurnCounter(), TurnBeforeLock);
    TestEqual(TEXT("Input-lock rejection and replay preserve the active unit"), Fixture.Combat->GetCurrentUnit(), Fixture.First);
    TestFalse(TEXT("Input-lock rejection and replay leave the unit idle"), Fixture.First->IsBusy());

    FCombatActionRequest Turn = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    const int32 CurrentTurn = Turn.TurnSerial;
    Turn.TurnSerial += 100;
    TestTrue(TEXT("Another turn's request is rejected"), Fixture.Submit(Turn).Result != ECombatRequestResult::Accepted);
    Turn.TurnSerial = CurrentTurn;
    TestEqual(TEXT("Rejected same-context request cannot be replayed after correction"), Fixture.Submit(Turn).Result, ECombatRequestResult::DuplicateRequest);
    FCombatActionRequest Unit = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    Unit.UnitId = FGuid::NewGuid();
    TestTrue(TEXT("Unregistered acting unit is rejected"), Fixture.Submit(Unit).Result != ECombatRequestResult::Accepted);
    FCombatActionRequest Version = Fixture.Make(ECombatActionKind::EndTurn);
    Version.Version = 2;
    TestEqual(TEXT("Unknown command schema is rejected"), Fixture.Submit(Version).Result, ECombatRequestResult::InvalidRequest);
    FCombatActionRequest Fields = Fixture.Make(ECombatActionKind::EndTurn);
    Fields.TargetUnitId = Fixture.Authority()->GetUnitId(Fixture.Enemy);
    TestEqual(TEXT("Unused end-turn payload is rejected"), Fixture.Submit(Fields).Result, ECombatRequestResult::InvalidRequest);
    FCombatActionRequest Sequence = Fixture.Make(ECombatActionKind::EndTurn);
    Sequence.RequestSequence = 0;
    TestTrue(TEXT("Zero request sequence is rejected"), Fixture.Submit(Sequence).Result != ECombatRequestResult::Accepted);
    TestEqual(TEXT("Invalid requests preserve AP"), Fixture.First->GetCurrentActionPoint(), 3);
    TestEqual(TEXT("Invalid requests preserve target HP"), Fixture.Enemy->GetAttributeSet()->GetHP(), 90.0f);
    FCombatActionRequest Ended = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    Fixture.Combat->EndCombat();
    TestEqual(TEXT("Ended combat rejects an otherwise valid packet"), Fixture.Submit(Ended).Result, ECombatRequestResult::InvalidContext);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRequestServerDataTest, "ProjectA.Combat.Requests.ServerLoadoutAndTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRequestServerDataTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Server data fixture initializes"), Fixture.Initialize()))
    {
        return false;
    }
    USkillDefinitionDataAsset* Unknown = DuplicateObject<USkillDefinitionDataAsset>(Fixture.Skill, Fixture.Combat, TEXT("UnequippedRequestSkill"));
    Fixture.Host->EnterSkillMode(Unknown);
    TestFalse(TEXT("Client selection rejects an unowned definition with a granted ability class"), Fixture.Host->IsSkillInputMode());
    TestEqual(TEXT("Server rejects an unowned skill identifier"), Fixture.Submit(Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Unknown)).Result, ECombatRequestResult::InvalidSkill);
    USkillDefinitionDataAsset* CheapCopy = DuplicateObject<USkillDefinitionDataAsset>(Fixture.Skill, Fixture.Combat, Fixture.Skill->GetFName());
    CheapCopy->ActionPointCost = 1;
    Fixture.Skill->ActionPointCost = 5;
    TestTrue(TEXT("Spoofed local definition uses the authored skill identifier"), CheapCopy->GetPrimaryAssetId() == Fixture.Skill->GetPrimaryAssetId());
    const FCombatActionRequest Unaffordable = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, CheapCopy);
    TestEqual(TEXT("Authority resolves actual server cost instead of a cheap client copy"), Fixture.Submit(Unaffordable).Result, ECombatRequestResult::InsufficientResources);
    Fixture.Skill->ActionPointCost = 2;
    TestEqual(TEXT("Rejected unaffordable packet stays consumed after data changes"), Fixture.Submit(Unaffordable).Result, ECombatRequestResult::DuplicateRequest);
    TestEqual(TEXT("New request uses current server cost"), Fixture.Submit(Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill)).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("Actual server AP cost is charged"), Fixture.First->GetCurrentActionPoint(), 2);

    const FCombatActionRequest Occupancy = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    Fixture.EnemyTile->SetOccupyingUnit(Fixture.Second);
    TestEqual(TEXT("Changed target occupancy rejects a stale unit-target packet"), Fixture.Submit(Occupancy).Result, ECombatRequestResult::InvalidTarget);
    Fixture.EnemyTile->SetOccupyingUnit(Fixture.Enemy);
    TestEqual(TEXT("Restoring occupancy does not make an old packet reusable"), Fixture.Submit(Occupancy).Result, ECombatRequestResult::DuplicateRequest);
    FCombatActionRequest WrongCoordinate = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    WrongCoordinate.TargetCoord = FIntPoint(99, 99);
    TestEqual(TEXT("Coordinates outside the server grid are rejected"), Fixture.Submit(WrongCoordinate).Result, ECombatRequestResult::InvalidTarget);
    const FCombatActionRequest Protection = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    Fixture.EnemyTile->SetProtectedByFront(true);
    TestEqual(TEXT("Target protection is checked at dispatch"), Fixture.Submit(Protection).Result, ECombatRequestResult::InvalidTarget);
    Fixture.EnemyTile->SetProtectedByFront(false);
    FCombatActionRequest Item = Fixture.Make(ECombatActionKind::HealingItem, Fixture.FirstTile);
    Item.TargetUnitId = Fixture.Authority()->GetUnitId(Fixture.Second);
    TestEqual(TEXT("Healing target ID must match the selected tile occupant"), Fixture.Submit(Item).Result, ECombatRequestResult::InvalidTarget);

    const FCombatActionRequest Move = Fixture.Make(ECombatActionKind::Move, Fixture.MoveTile);
    Fixture.MoveTile->SetOccupyingUnit(Fixture.Second);
    TestEqual(TEXT("Movement recalculates reachability after occupancy changes"), Fixture.Submit(Move).Result, ECombatRequestResult::InvalidTarget);
    Fixture.MoveTile->SetOccupyingUnit(nullptr);
    TestEqual(TEXT("Clearing the destination cannot replay a rejected move"), Fixture.Submit(Move).Result, ECombatRequestResult::DuplicateRequest);
    TestEqual(TEXT("Rejected movement preserves SubAP"), Fixture.First->GetCurrentSubActionPoint(), 2);
    Fixture.First->GetAbilitySystemComponent()->ClearAllAbilities();
    TestEqual(TEXT("A listed skill without a granted GAS ability is rejected"), Fixture.Submit(Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill)).Result, ECombatRequestResult::InvalidSkill);
    TestEqual(TEXT("Rejected target and loadout requests preserve AP"), Fixture.First->GetCurrentActionPoint(), 2);
    TestEqual(TEXT("Rejected target and loadout requests preserve enemy HP"), Fixture.Enemy->GetAttributeSet()->GetHP(), 90.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRequestResetTest, "ProjectA.Combat.Requests.ResetAndFailClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRequestResetTest::RunTest(const FString& Parameters)
{
    using namespace CombatActionRequestTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Reset fixture initializes"), Fixture.Initialize()))
    {
        return false;
    }
    const FCombatActionRequest Old = Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill);
    Fixture.Combat->RegisterUnits({Fixture.First, Fixture.Second, Fixture.Enemy});
    if (!TestTrue(TEXT("Next combat configures the same original ownership"), Fixture.Authority()->ConfigureRun(Fixture.Identity, Fixture.Party, Fixture.PartyActors, Fixture.Error)) || !TestTrue(TEXT("Original controllers bind to the next combat"), Fixture.BindOwners()))
    {
        return false;
    }
    Fixture.Combat->StartCombat_Internal();
    TestTrue(TEXT("Restarted combat receives a new nonce"), Fixture.Authority()->GetCombatInstanceId() != Old.CombatInstanceId);
    TestEqual(TEXT("Restarted combat can reuse the same turn number"), Fixture.Combat->GetTurnManager()->GetTurnCounter(), Old.TurnSerial);
    TestEqual(TEXT("Previous combat packet cannot execute in a new combat"), Fixture.Submit(Old).Result, ECombatRequestResult::InvalidContext);
    TestEqual(TEXT("Old packet does not damage the new encounter"), Fixture.Enemy->GetAttributeSet()->GetHP(), 100.0f);
    TestEqual(TEXT("Fresh packet executes in the new combat"), Fixture.Submit(Fixture.Make(ECombatActionKind::Skill, Fixture.EnemyTile, Fixture.Skill)).Result, ECombatRequestResult::Accepted);

    Fixture.Combat->RegisterUnits({Fixture.First, Fixture.Second, Fixture.Enemy});
    FRunIdentityData InvalidIdentity = Fixture.Identity;
    InvalidIdentity.HostAccountId.Subject = TEXT("NotAnOriginalMember");
    TestFalse(TEXT("Malformed identified Run configuration is rejected"), Fixture.Authority()->ConfigureRun(InvalidIdentity, Fixture.Party, Fixture.PartyActors, Fixture.Error));
    TestFalse(TEXT("Malformed configuration explains its failure"), Fixture.Error.IsEmpty());
    Fixture.Combat->StartCombat_Internal();
    TestFalse(TEXT("Rejected identified configuration cannot silently enable legacy input"), Fixture.Authority()->CanControllerControl(Fixture.Host, Fixture.First));
    TestTrue(TEXT("Rejected configuration never accepts a new command"), Fixture.Submit(Fixture.Make(ECombatActionKind::EndTurn)).Result != ECombatRequestResult::Accepted);

    FFixture Legacy;
    TestTrue(TEXT("Existing standalone TestMap-style context starts without Run ownership"), Legacy.Initialize(false));
    TestTrue(TEXT("Standalone local controller retains legacy combat input"), Legacy.Authority()->CanControllerControl(Legacy.Host, Legacy.First));
    TestEqual(TEXT("Standalone legacy action uses the same dispatch path"), Legacy.Submit(Legacy.Make(ECombatActionKind::Skill, Legacy.EnemyTile, Legacy.Skill)).Result, ECombatRequestResult::Accepted);
    return true;
}

#endif
