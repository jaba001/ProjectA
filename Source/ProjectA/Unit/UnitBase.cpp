#include "UnitBase.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Controller/UnitAIController.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GAS/Ability/GA_DefaultAttack.h"

AUnitBase::AUnitBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bIsActiveTurn = false;
    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    OriginalTileBeforeAction = nullptr;
    MovePhase = EUnitMovePhase::None;

    GetCharacterMovement()->MaxWalkSpeed = 350.f;

	// AI Controller 설정
    AIControllerClass = AUnitAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    // GAS 핵심 컴포넌트
    // Ability / GameplayEffect / Attribute를 관리하는 시스템
    // 모든 GAS 기능은 AbilitySystemComponent를 통해 동작한다.
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    
    // 유닛의 능력치 데이터를 보관하는 객체
    // HP / MaxHP / Attack 등 모든 Attribute는 여기서 관리된다.
    AttributeSet = CreateDefaultSubobject<UAS_Unit>(TEXT("AttributeSet"));

    // AbilitySystem 네트워크 복제 설정
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // 네트워크 복제 활성화
    // - 서버에서 생성된 이 Actor를 클라이언트에도 자동 생성
    // - 서버에서 위치 변경 시 클라이언트에 자동 동기화
    bReplicates = true;
    SetReplicateMovement(true);
}

void AUnitBase::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystem)
    {
        AbilitySystem->InitAbilityActorInfo(this, this);

        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UGA_DefaultAttack::StaticClass(), 1, 0));
    }

    if (AttributeSet)
    {
        AttributeSet->InitMaxHP(InitMaxHP);
        AttributeSet->InitHP(InitMaxHP);
    }

    if (AbilitySystem)
    {
        AbilitySystem->TryActivateAbilityByClass(UGA_DefaultAttack::StaticClass());
    }

    DefaultBattleRotation = GetActorRotation();
}

void AUnitBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

UAbilitySystemComponent* AUnitBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void AUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    //DOREPLIFETIME 는 서버값을 클라이언트로 복제
    DOREPLIFETIME(AUnitBase, Team);
}

void AUnitBase::SetTeam(ETeam NewTeam)
{
    // 서버 전용 변경
    if (!HasAuthority())
        return;  

    Team = NewTeam;

    if (CurrentTile)
    {
        CurrentTile->UpdateTileVisual();
    }
}

void AUnitBase::SetCurrentTile(ACombatGridTile* NewTile)
{
    if (!NewTile || CurrentTile == NewTile)
        return;

    // 이전 타일 정리
    if (CurrentTile)
    {
        CurrentTile->SetOccupyingUnit(nullptr);
    }

    CurrentTile = NewTile;

    // 새 타일 등록
    CurrentTile->SetOccupyingUnit(this);
}


void AUnitBase::MoveToTile(ACombatGridTile* TargetTile)
{
    if (!TargetTile)
    {
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        return;
    }

    PendingTile = TargetTile;
    PendingTargetUnit = nullptr;
    MovePhase = EUnitMovePhase::MovingToTile;

    FVector TargetLocation = TargetTile->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    AICon->MoveUnitToLocation(TargetLocation);
}

void AUnitBase::MoveToTarget(AUnitBase* TargetUnit)
{
    if (!TargetUnit || !TargetUnit->IsUnitAlive())
    {
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        return;
    }

    OriginalTileBeforeAction = CurrentTile;
    PendingTargetUnit = TargetUnit;
    PendingTile = nullptr;
    MovePhase = EUnitMovePhase::MovingToTarget;

    FVector TargetLocation = TargetUnit->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    AICon->MoveUnitToLocation(TargetLocation, 50.f);
}

void AUnitBase::ReturnToOriginalTile()
{
    if (!OriginalTileBeforeAction)
    {
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        return;
    }

    PendingTile = OriginalTileBeforeAction;
    PendingTargetUnit = nullptr;
    MovePhase = EUnitMovePhase::ReturningToOriginalTile;

    FVector TargetLocation = OriginalTileBeforeAction->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    AICon->MoveUnitToLocation(TargetLocation);
}

void AUnitBase::OnTurnStart()
{
    bIsActiveTurn = true;
}

void AUnitBase::OnTurnEnd()
{
    bIsActiveTurn = false;
}

bool AUnitBase::IsUnitAlive() const
{
    return !bIsDead;
}

void AUnitBase::Die()
{
    if (bIsDead)
        return;

    bIsDead = true;
    bIsActiveTurn = false;

    UE_LOG(LogTemp, Warning, TEXT("[Death] %s died"), *GetName());

    GetCharacterMovement()->DisableMovement();

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetAllBodiesSimulatePhysics(true);

    FVector Impulse;

    if (!DeathImpulse.IsNearlyZero())
    {
        // 블루프린트에서 지정된 값 사용
        Impulse = DeathImpulse;
    }
    else
    {
        // 기본 랜덤 Impulse
        Impulse = FMath::VRand() * 2000.0f;
        Impulse.Z = FMath::Abs(Impulse.Z) + 500.0f;
    }
    GetMesh()->AddImpulse(Impulse, NAME_None, true);

    //SetLifeSpan(3.f);

    UE_LOG(LogTemp, Warning, TEXT("[Death] %s died"), *GetName());
}

AUnitAIController* AUnitBase::GetOrCreateAIController()
{
    AUnitAIController* AICon = Cast<AUnitAIController>(GetController());

    if (!AICon)
    {
        SpawnDefaultController();
        AICon = Cast<AUnitAIController>(GetController());
    }

    return AICon;
}

void AUnitBase::HandleMoveCompleted()
{
    switch (MovePhase)
    {
    case EUnitMovePhase::MovingToTile:
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            PendingTile = nullptr;
        }
        MovePhase = EUnitMovePhase::None;
        break;

    case EUnitMovePhase::MovingToTarget:
        ReturnToOriginalTile();
        break;

    case EUnitMovePhase::ReturningToOriginalTile:
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            PendingTile = nullptr;
        }

        SetActorRotation(DefaultBattleRotation);
        MovePhase = EUnitMovePhase::None;
        break;

    default:
        break;
    }
}

void AUnitBase::DealDamage(AUnitBase* TargetUnit, float Damage)
{
    if (!TargetUnit || !TargetUnit->AttributeSet)
        return;

    float CurrentHP = TargetUnit->AttributeSet->GetHP();
    float NewHP = CurrentHP - Damage;

    TargetUnit->AttributeSet->SetHP(NewHP);

    UE_LOG(LogTemp, Warning,
        TEXT("[Combat] %s -> %s | Damage=%.1f | HP %.1f -> %.1f"),
        *GetName(),
        *TargetUnit->GetName(),
        Damage,
        CurrentHP,
        NewHP
    );

    if (NewHP <= 0.f)
    {
        TargetUnit->Die();
    }
}

