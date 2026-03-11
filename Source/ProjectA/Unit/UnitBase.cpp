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
#include "Kismet/KismetSystemLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

AUnitBase::AUnitBase()
{
    PrimaryActorTick.bCanEverTick = false;

    bIsActiveTurn = false;
    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    OriginalTileBeforeAction = nullptr;
    bActionDamageApplied = false;
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

        if (HasAuthority() && DefaultAttackAbilityClass)
        {
            AbilitySystem->GiveAbility(FGameplayAbilitySpec(DefaultAttackAbilityClass, 1, 0));
        }
    }

    if (AttributeSet)
    {
        AttributeSet->InitMaxHP(InitMaxHP);
        AttributeSet->InitHP(InitMaxHP);
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
    if (!HasAuthority())
    {
        return;
    }

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

    AICon->MoveUnitToLocation(TargetLocation, 1.f);
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

    PendingTargetUnit = TargetUnit;
    PendingTile = nullptr;
    MovePhase = EUnitMovePhase::MovingToTarget;

    FVector TargetLocation = TargetUnit->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    AICon->MoveUnitToLocation(TargetLocation, 75.f);
}

void AUnitBase::ReturnToOriginalTile()
{
    if (!OriginalTileBeforeAction)
    {
        //UE_LOG(LogTemp, Log, TEXT("[Action] ReturnToOriginalTile failed: OriginalTileBeforeAction is null"));
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        //UE_LOG(LogTemp, Log, TEXT("[Action] ReturnToOriginalTile failed: AICon is null"));
        return;
    }

    PendingTile = OriginalTileBeforeAction;
    PendingTargetUnit = nullptr;
    MovePhase = EUnitMovePhase::ReturningToOriginalTile;

    FVector TargetLocation = OriginalTileBeforeAction->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    //UE_LOG(LogTemp, Log, TEXT("[Action] Returning to original tile"));

    AICon->MoveUnitToLocation(TargetLocation, 2.f);
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

    ClearActionContext();

	// 죽음 처리: 이동 불가, 충돌 비활성화, 래그돌 적용
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

    //UE_LOG(LogTemp, Log, TEXT("[Death] %s died"), *GetName());
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
    //UE_LOG(LogTemp, Log, TEXT("[Move] HandleMoveCompleted Phase=%d"), static_cast<uint8>(MovePhase));

    switch (MovePhase)
    {
    case EUnitMovePhase::MovingToTile:
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            SnapToTile(PendingTile, DefaultBattleRotation);
            PendingTile = nullptr;
        }

        MovePhase = EUnitMovePhase::None;
        break;

    case EUnitMovePhase::MovingToTarget:
        MovePhase = EUnitMovePhase::WaitingForAction;
        ExecuteActionAtTarget();
        break;

    case EUnitMovePhase::ReturningToOriginalTile:
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            SnapToTile(PendingTile, DefaultBattleRotation);
            PendingTile = nullptr;
        }

        MovePhase = EUnitMovePhase::None;
        ClearActionContext();
        break;

    default:
        break;
    }
}

void AUnitBase::SnapToTile(ACombatGridTile* Tile, const FRotator& TargetRotation)
{
    // 유효한 타일이 없으면 종료
    if (!Tile)
    {
        return;
    }

    // 타일 중심 좌표를 구한다.
    FVector Center = Tile->GetActorLocation();
    Center.Z = GetActorLocation().Z;

    // MoveComponentTo는 LatentAction이므로 완료 후 복귀할 함수명이 필요하다.
    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.UUID = 1001;
    LatentInfo.Linkage = 0;
    LatentInfo.ExecutionFunction = FName(TEXT("OnSnapToTileFinished"));

    // 캡슐을 타일 중심으로 부드럽게 보정 이동시킨다.
    UKismetSystemLibrary::MoveComponentTo(
        GetCapsuleComponent(),
        Center,
        TargetRotation,
        false,
        false,
        0.5f,
        false,
        EMoveComponentAction::Move,
        LatentInfo
    );
}

void AUnitBase::OnSnapToTileFinished()
{
}

void AUnitBase::ExecuteActionAtTarget()
{
    //UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteActionAtTarget Enter | Unit=%s | Target=%s | MovePhase=%d | AbilityClass=%s"), *GetName(), *GetNameSafe(PendingTargetUnit), static_cast<uint8>(MovePhase), *GetNameSafe(DefaultAttackAbilityClass));

    // 행동 대기 상태가 아니면 공격 실행을 시작할 수 없다.
    if (MovePhase != EUnitMovePhase::WaitingForAction)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteActionAtTarget Return | Reason=InvalidMovePhase"));
        return;
    }

    // 유효한 타겟이 없으면 공격을 종료하고 복귀 흐름으로 넘긴다.
    if (!PendingTargetUnit || !PendingTargetUnit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteActionAtTarget Return | Reason=InvalidTarget"));
        OnActionFinished();
        return;
    }

    // 공격 직전에 타겟을 바라보도록 회전한다.
    FVector Direction = PendingTargetUnit->GetActorLocation() - GetActorLocation();
    Direction.Z = 0.0f;

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    // 실제 공격 실행은 GAS Ability가 담당한다.
    if (!AbilitySystem || !DefaultAttackAbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteActionAtTarget Return | Reason=NoASCOrAbilityClass | ASC=%d | AbilityClass=%s"),
            AbilitySystem ? 1 : 0,
            *GetNameSafe(DefaultAttackAbilityClass));
        OnActionFinished();
        return;
    }

    const bool bActivated = AbilitySystem->TryActivateAbilityByClass(DefaultAttackAbilityClass);

    //UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteActionAtTarget Activate Result | Unit=%s | bActivated=%d | AbilityClass=%s"), *GetName(), bActivated ? 1 : 0,  *GetNameSafe(DefaultAttackAbilityClass));
    
    // Ability 활성화 실패 시 공격을 진행할 수 없으므로 종료한다.
    if (!bActivated)
    {
        OnActionFinished();
    }
}

void AUnitBase::OnActionFinished()
{
    //UE_LOG(LogTemp, Log, TEXT("[Action] OnActionFinished: %s"), *GetName());

    if (bIsDead)
    {
        return;
    }

    if (MovePhase != EUnitMovePhase::WaitingForAction)
    {
        return;
    }

    ReturnToOriginalTile();
}

void AUnitBase::ClearActionContext()
{
    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    OriginalTileBeforeAction = nullptr;
    bActionDamageApplied = false;
}

void AUnitBase::StartAttack(AUnitBase* TargetUnit, bool bMoveToTarget)
{
    //UE_LOG(LogTemp, Warning, TEXT("[Attack] StartAttack Enter | Unit=%s | Target=%s | bMoveToTarget=%d | ActiveTurn=%d | MovePhase=%d"), *GetName(), *GetNameSafe(TargetUnit), bMoveToTarget ? 1 : 0, bIsActiveTurn ? 1 : 0, static_cast<uint8>(MovePhase));

    // 서버 권한에서만 공격 시작
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] StartAttack Return | Reason=NoAuthority | Unit=%s"), *GetName());
        return;
    }

    // 현재 턴이 아니면 공격할 수 없다.
    if (!bIsActiveTurn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] StartAttack Return | Reason=NotActiveTurn | Unit=%s"), *GetName());
        return;
    }

    // 이미 다른 이동/행동 중이면 새 공격을 시작할 수 없다.
    if (MovePhase != EUnitMovePhase::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] StartAttack Return | Reason=Busy | Unit=%s | MovePhase=%d"), *GetName(), static_cast<uint8>(MovePhase));
        return;
    }

    // 유효한 타겟이 없거나 죽어 있으면 종료
    if (!TargetUnit || !TargetUnit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] StartAttack Return | Reason=InvalidTarget | Unit=%s | Target=%s"), *GetName(), *GetNameSafe(TargetUnit));
        return;
    }

    // 현재 타일 정보가 없으면 종료
    if (!CurrentTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] StartAttack Return | Reason=NoCurrentTile | Unit=%s"), *GetName());
        return;
    }

    // 이번 공격 컨텍스트를 저장한다.
    PendingTargetUnit = TargetUnit;
    OriginalTileBeforeAction = CurrentTile;
    bActionDamageApplied = false;

    // 근접공격은 이동 후 공격한다.
    if (bMoveToTarget)
    {
        //UE_LOG(LogTemp, Log, TEXT("[Attack] StartAttack Branch | MoveToTarget"));
        MoveToTarget(TargetUnit);
        return;
    }

    // 원거리공격은 제자리에서 바로 공격을 실행한다.
    //UE_LOG(LogTemp, Log, TEXT("[Attack] StartAttack Branch | InPlaceAttack"));
    MovePhase = EUnitMovePhase::WaitingForAction;
    ExecuteActionAtTarget();
}
