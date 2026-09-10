#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/AI/PartyAutoCombatComponent.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Run/RunSaveGame.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

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
        uint64 TimerFrame = 0;

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

        void TickTimers()
        {
            TimerFrame = FMath::Max(TimerFrame, GFrameCounter) + 1;
            TGuardValue<uint64> FrameGuard(GFrameCounter, TimerFrame);
            World->GetTimerManager().Tick(0.1f);
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

        bool Initialize(ERunAIConsent Consent = ERunAIConsent::Granted)
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
            UGameplayAbility* Ability = First->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Skill->AbilityClass)->GetPrimaryInstance();
            FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("DamageEffectClass"))->SetObjectPropertyValue_InContainer(Ability, UGE_Damage::StaticClass());
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
                Member.ClassId = TEXT("Hunter");
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

        bool StartAI(bool bInitializeBrain = false)
        {
            if (!Authority()->SetPartyControlMode(First, EPartyControlMode::ServerAI, Error)) return false;
            if (bInitializeBrain) First->InitializeAutoCombat(Combat);
            Combat->StartCombat_Internal();
            if (!bInitializeBrain && Brain()) Brain()->Stop();
            return Combat->IsCombatActive() && Combat->GetCurrentUnit() == First;
        }

        FCombatActionRequest Make(int64 Sequence, ECombatActionKind Kind, ACombatGridTile* Tile = nullptr) const
        {
            FCombatActionRequest Request;
            Request.RunId = Identity.RunId;
            Request.HostEpoch = Identity.HostEpoch;
            Request.CombatInstanceId = Combat->GetCombatInstanceId();
            Request.TurnSerial = Combat->GetTurnSerial();
            Request.RequestSequence = Sequence;
            Request.UnitId = Combat->GetRuntimeUnitId(First);
            Request.Kind = Kind;
            if (Tile)
            {
                Request.TargetCoord = Tile->GridCoord;
                if (Kind != ECombatActionKind::Move) Request.TargetUnitId = Combat->GetRuntimeUnitId(Tile->GetOccupyingUnit());
            }
            if (Kind == ECombatActionKind::Skill) Request.SkillId = Skill->GetPrimaryAssetId();
            return Request;
        }

        FCombatActionResponse SubmitAI(const FCombatActionRequest& Request) const
        {
            return Authority()->ExecuteServerAI(First, Request, First->GetAIControlSessionId());
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAIControlTest, "ProjectA.Coop.PartyAI.ControlAndConsent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartyAIControlTest::RunTest(const FString& Parameters)
{
    using namespace PartyAITests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Identified ownership fixture initializes"), Fixture.Initialize(ERunAIConsent::Unknown))) return false;
    const FRunAccountId Owner = Fixture.Authority()->GetOwnerAccountId(Fixture.First);
    const FGuid Character = Fixture.Authority()->GetCharacterId(Fixture.First);
    TestFalse(TEXT("Unknown consent cannot enable AI"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    Fixture.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Declined;
    Fixture.Identity.OriginalParticipants[0].ConsentPolicyVersion = 1;
    TestTrue(TEXT("Declined consent remains valid ownership data"), Fixture.Reconfigure());
    TestFalse(TEXT("Declined consent cannot enable AI"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    Fixture.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Granted;
    TestTrue(TEXT("Granted consent configures"), Fixture.Reconfigure());
    Fixture.Combat->SetRole(ROLE_SimulatedProxy);
    TestFalse(TEXT("A client manager cannot change the control mode"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    Fixture.Combat->SetRole(ROLE_Authority);
    TestTrue(TEXT("Consented server initialization enables AI"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::ServerAI, Fixture.Error));
    const FGuid Session = Fixture.First->GetAIControlSessionId();
    TestTrue(TEXT("AI has a server execution session"), Session.IsValid());
    TestEqual(TEXT("AI keeps the original character"), Fixture.Authority()->GetCharacterId(Fixture.First), Character);
    TestTrue(TEXT("AI keeps the original owner"), Fixture.Authority()->GetOwnerAccountId(Fixture.First) == Owner);
    TestEqual(TEXT("AI remains on the player team"), Fixture.First->GetTeam(), ETeam::Player);
    TestFalse(TEXT("Original owner cannot directly control its AI character"), Fixture.Authority()->CanControllerControl(Fixture.Host, Fixture.First));
    TestFalse(TEXT("Another participant cannot directly control the AI character"), Fixture.Authority()->CanControllerControl(Fixture.Guest, Fixture.First));
    Fixture.Combat->StartCombat_Internal();
    if (Fixture.Brain()) Fixture.Brain()->Stop();
    TestFalse(TEXT("Combat cannot switch the character back to Human"), Fixture.Authority()->SetPartyControlMode(Fixture.First, EPartyControlMode::Human, Fixture.Error));
    TestTrue(TEXT("Rejected transition retains AI execution session"), Fixture.First->GetAIControlSessionId() == Session);
    FCombatActionRequest Human = Fixture.Make(1, ECombatActionKind::Skill, Fixture.EnemyTile);
    Human.ParticipantBindingId = Fixture.Host->GetParticipantBindingId();
    TestEqual(TEXT("Server rejects the original owner's human action while AI controls"), Fixture.Authority()->Execute(Fixture.Host, Human).Result, ECombatRequestResult::NotOwner);
    TestEqual(TEXT("Rejected human packet still reserves its sequence"), Fixture.Authority()->Execute(Fixture.Host, Human).Result, ECombatRequestResult::DuplicateRequest);
    TestEqual(TEXT("Rejected human input does not spend AP"), Fixture.First->GetCurrentActionPoint(), 4);
    TestEqual(TEXT("Rejected human input does not deal damage"), Fixture.Enemy->GetAttributeSet()->GetHP(), 100.0f);
    Fixture.Combat->SuspendCombatForRecovery();
    FRunIdentityData Replacement = Fixture.Identity;
    Replacement.RunId = FGuid::NewGuid();
    TestFalse(TEXT("Suspension cannot reopen Run ownership configuration"), Fixture.Authority()->ConfigureRun(Replacement, Fixture.Party, Fixture.PartyActors, Fixture.Error));
    TestEqual(TEXT("Rejected suspended reconfiguration preserves Run identity"), Fixture.Authority()->GetRunIdentity().RunId, Fixture.Identity.RunId);
    TestEqual(TEXT("Rejected suspended reconfiguration preserves character mapping"), Fixture.Authority()->GetCharacterId(Fixture.First), Character);
    FFixture Waiting;
    if (!TestTrue(TEXT("Waiting fixture initializes"), Waiting.Initialize())) return false;
    Waiting.Combat->CommitTurnBoundary.BindLambda([](int32, int32) { return false; });
    Waiting.Combat->StartCombat_Internal();
    TestTrue(TEXT("Save failure leaves a waiting boundary"), Waiting.Combat->IsAwaitingTurnCheckpoint());
    TestFalse(TEXT("Save waiting cannot reopen Run ownership configuration"), Waiting.Authority()->ConfigureRun(Replacement, Waiting.Party, Waiting.PartyActors, Waiting.Error));
    TestEqual(TEXT("Waiting rejection preserves original Run identity"), Waiting.Authority()->GetRunIdentity().RunId, Waiting.Identity.RunId);
    TestEqual(TEXT("Waiting rejection preserves character mapping"), Waiting.Authority()->GetCharacterId(Waiting.First), Waiting.Party[0].CharacterId);
    TestFalse(TEXT("Save waiting cannot switch control mode"), Waiting.Authority()->SetPartyControlMode(Waiting.First, EPartyControlMode::ServerAI, Waiting.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAICommandTest, "ProjectA.Coop.PartyAI.CommandValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartyAICommandTest::RunTest(const FString& Parameters)
{
    using namespace PartyAITests;
    FFixture Fixture;
    if (!TestTrue(TEXT("AI fixture starts"), Fixture.Initialize() && Fixture.StartAI())) return false;
    FCombatActionRequest Request = Fixture.Make(1, ECombatActionKind::Skill, Fixture.EnemyTile);
    TestTrue(TEXT("Wrong AI session cannot execute"), Fixture.Authority()->ExecuteServerAI(Fixture.First, Request, FGuid::NewGuid()).Result != ECombatRequestResult::Accepted);
    FCombatActionRequest Wrong = Request;
    Wrong.CombatInstanceId = FGuid::NewGuid();
    TestTrue(TEXT("Old combat context cannot execute"), Fixture.SubmitAI(Wrong).Result != ECombatRequestResult::Accepted);
    Wrong = Request;
    Wrong.RunId = FGuid::NewGuid();
    TestTrue(TEXT("Another Run cannot execute"), Fixture.SubmitAI(Wrong).Result != ECombatRequestResult::Accepted);
    Wrong = Request;
    Wrong.ParticipantBindingId = Fixture.Host->GetParticipantBindingId();
    TestTrue(TEXT("AI cannot masquerade as a participant binding"), Fixture.SubmitAI(Wrong).Result != ECombatRequestResult::Accepted);
    Request.SkillId = FPrimaryAssetId(TEXT("SkillDefinitionDataAsset"), TEXT("Unowned"));
    TestEqual(TEXT("AI must resolve a truly equipped skill"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::InvalidSkill);
    Request.SkillId = Fixture.Skill->GetPrimaryAssetId();
    TestEqual(TEXT("Rejected AI skill cannot become valid on replay"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::DuplicateRequest);
    Request = Fixture.Make(2, ECombatActionKind::Skill, Fixture.EnemyTile);
    TestEqual(TEXT("Valid server AI uses shared skill dispatch"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("AI skill applies actual GAS damage"), Fixture.Enemy->GetAttributeSet()->GetHP(), 90.0f);
    TestEqual(TEXT("AI pays the authoritative skill cost"), Fixture.First->GetCurrentActionPoint(), 3);
    TestEqual(TEXT("AI skill replay is rejected"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::DuplicateRequest);
    Request = Fixture.Make(3, ECombatActionKind::HealingItem, Fixture.FirstTile);
    TestEqual(TEXT("Full HP prevents AI item use"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::InsufficientResources);
    Fixture.First->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 20.0f);
    TestEqual(TEXT("A previously rejected item cannot execute after HP changes"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::DuplicateRequest);
    Request.RequestSequence = 4;
    TestEqual(TEXT("New AI item command executes"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("AI item restores real HP"), Fixture.First->GetAttributeSet()->GetHP(), 60.0f);
    TestEqual(TEXT("AI item consumes one inventory unit"), Fixture.First->HealingItemCount, 0);
    TestEqual(TEXT("AI item consumes one SubAP"), Fixture.First->GetCurrentSubActionPoint(), 1);
    Fixture.First->ConsumeActionPoint(3);
    Request = Fixture.Make(5, ECombatActionKind::Skill, Fixture.EnemyTile);
    TestEqual(TEXT("AI cannot spend missing AP"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::InsufficientResources);
    Fixture.First->ResetActionPoint();
    TestEqual(TEXT("Resource rejection cannot become valid on replay"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::DuplicateRequest);
    Request = Fixture.Make(6, ECombatActionKind::Skill, Fixture.EnemyTile);
    Request.TargetUnitId = Fixture.Combat->GetRuntimeUnitId(Fixture.Second);
    TestEqual(TEXT("AI target identity must match current occupancy"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::InvalidTarget);
    Request = Fixture.Make(7, ECombatActionKind::Move, Fixture.MoveTile);
    Request.TargetCoord = FIntPoint(99, 99);
    TestEqual(TEXT("AI cannot move outside the registered Grid"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::InvalidTarget);
    Request = Fixture.Make(8, ECombatActionKind::Move, Fixture.MoveTile);
    TestEqual(TEXT("Reachable AI move dispatches through shared validation"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::Accepted);
    if (Fixture.First->IsBusy()) Fixture.First->HandleMoveFailed();
    TestEqual(TEXT("Failed navigation preserves original occupancy"), Fixture.First->GetCurrentTile(), Fixture.FirstTile);
    TestEqual(TEXT("Move charges SubAP once"), Fixture.First->GetCurrentSubActionPoint(), 0);
    TestEqual(TEXT("AI move replay cannot charge twice"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::DuplicateRequest);
    Request = Fixture.Make(9, ECombatActionKind::EndTurn);
    TestEqual(TEXT("AI end turn dispatches"), Fixture.SubmitAI(Request).Result, ECombatRequestResult::Accepted);
    TestEqual(TEXT("AI ends exactly one turn"), Fixture.Combat->GetTurnSerial(), 2);
    TestTrue(TEXT("Old AI end turn cannot advance another unit"), Fixture.SubmitAI(Request).Result != ECombatRequestResult::Accepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAIContinuationTest, "ProjectA.Coop.PartyAI.DecisionFailureAndStop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartyAIContinuationTest::RunTest(const FString& Parameters)
{
    using namespace PartyAITests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Automatic AI fixture starts"), Fixture.Initialize() && Fixture.StartAI(true)) || !TestNotNull(TEXT("Player owns an AI component"), Fixture.Brain())) return false;
    UPartyAutoCombatComponent* Brain = Fixture.Brain();
    Brain->Stop();
    Brain->HandleActionCompleted(EUnitActionType::Skill, EUnitActionResult::Succeeded);
    Fixture.TickTimers();
    TestEqual(TEXT("Stop invalidates pending initial and late completion callbacks"), Fixture.Enemy->GetAttributeSet()->GetHP(), 100.0f);
    TestEqual(TEXT("Stop does not end a turn itself"), Fixture.Combat->GetTurnSerial(), 1);
    Fixture.First->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 40.0f);
    Brain->StartTurn();
    Fixture.TickTimers();
    TestEqual(TEXT("AI first uses its own healing item"), Fixture.First->GetAttributeSet()->GetHP(), 80.0f);
    TestEqual(TEXT("AI self healing consumes its inventory"), Fixture.First->HealingItemCount, 0);
    TestEqual(TEXT("Self heal does not damage an opponent"), Fixture.Enemy->GetAttributeSet()->GetHP(), 100.0f);
    Brain->Stop();
    Fixture.TickTimers();
    TestEqual(TEXT("Stopping after synchronous completion prevents the queued next action"), Fixture.Enemy->GetAttributeSet()->GetHP(), 100.0f);
    Fixture.First->ConsumeActionPoint(4);
    int32 FailedMoves = 0;
    const FDelegateHandle Observer = Fixture.First->OnActionCompleted.AddLambda([&FailedMoves](AUnitBase*, EUnitActionType Kind, EUnitActionResult Result)
    {
        if (Kind == EUnitActionType::Move && Result != EUnitActionResult::Succeeded) ++FailedMoves;
    });
    Brain->StartTurn();
    for (int32 Index = 0; Index < 8 && Fixture.Combat->GetTurnSerial() == 1; ++Index)
    {
        Fixture.TickTimers();
        if (Fixture.First->IsBusy()) Fixture.First->HandleMoveFailed();
    }
    Fixture.First->OnActionCompleted.Remove(Observer);
    TestEqual(TEXT("Unavailable navigation produces one failed AI move"), FailedMoves, 1);
    TestEqual(TEXT("Failure exits AI once instead of retrying forever"), Fixture.Combat->GetTurnSerial(), 2);
    Brain->HandleActionCompleted(EUnitActionType::Move, EUnitActionResult::Succeeded);
    Fixture.TickTimers();
    TestEqual(TEXT("Late completion cannot end the following owner's turn"), Fixture.Combat->GetTurnSerial(), 2);
    const int64 FirstTurnLastSequence = Brain->GetLastDecision().RequestSequence;
    TestTrue(TEXT("Following Human unit ends its own turn"), Fixture.Combat->RequestEndTurnForUnit(Fixture.Second));
    TestTrue(TEXT("Passive fixture opponent ends its own turn"), Fixture.Combat->RequestEndTurnForUnit(Fixture.Enemy));
    TestEqual(TEXT("The same AI begins a second turn"), Fixture.Combat->GetCurrentUnit(), static_cast<AUnitBase*>(Fixture.First));
    const float HPBeforeSecondAITurn = Fixture.Enemy->GetAttributeSet()->GetHP();
    Fixture.TickTimers();
    TestTrue(TEXT("Second AI turn executes a real skill"), Fixture.Enemy->GetAttributeSet()->GetHP() < HPBeforeSecondAITurn);
    TestTrue(TEXT("AI request sequence increases across turns"), Brain->GetLastDecision().RequestSequence > FirstTurnLastSequence);
    Fixture.Combat->SuspendCombatForRecovery();
    Brain->StartTurn();
    Brain->HandleActionCompleted(EUnitActionType::Skill, EUnitActionResult::Succeeded);
    Fixture.TickTimers();
    TestFalse(TEXT("Recovery suspension cannot restart AI"), Fixture.Combat->IsCombatActive());
    TestEqual(TEXT("Suspension does not produce combat results"), Fixture.Combat->GetCombatResult(), ECombatResult::None);
    for (ESkillTargetRule Rule : { ESkillTargetRule::AllyUnit, ESkillTargetRule::AnyTile })
    {
        FFixture Friendly;
        if (!TestTrue(TEXT("Friendly-target rejection fixture starts"), Friendly.Initialize() && Friendly.StartAI(true))) return false;
        Friendly.Skill->TargetRule = Rule;
        Friendly.Skill->AreaType = Rule == ESkillTargetRule::AnyTile ? ESkillAreaType::AroundSelf : ESkillAreaType::Single;
        Friendly.Skill->AreaRadius = 3;
        Friendly.First->HealingItemCount = 0;
        Friendly.First->ConsumeSubActionPoint(2);
        Friendly.TickTimers();
        TestEqual(TEXT("AI rejects Ally targets and mixed-friendly Any areas"), Friendly.Brain()->GetLastDecision().Kind, ECombatActionKind::EndTurn);
        TestEqual(TEXT("Excluded friendly-damage skill does not spend AP"), Friendly.First->GetCurrentActionPoint(), 4);
        TestEqual(TEXT("Excluded skill preserves allied HP"), Friendly.Second->GetAttributeSet()->GetHP(), 100.0f);
        TestEqual(TEXT("Excluded mixed area preserves enemy HP too"), Friendly.Enemy->GetAttributeSet()->GetHP(), 100.0f);
    }
    return true;
}


#endif
