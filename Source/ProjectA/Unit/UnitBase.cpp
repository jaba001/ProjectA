#include "UnitBase.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

#include "Combat/CombatManager.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Controller/UnitAIController.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

AUnitBase::AUnitBase()
{
    PrimaryActorTick.bCanEverTick = false;

    bIsActiveTurn = false;
    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    OriginalTileBeforeSkill = nullptr;
    bSkillDamageApplied = false;
    MovePhase = EUnitMovePhase::None;

    GetCharacterMovement()->MaxWalkSpeed = 700.f;

    // 캡슐 콜리전을 CombatUnit 프리셋으로 초기화한다
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));

	// AI Controller 설정
    AIControllerClass = AUnitAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // GAS 핵심 개체 생성
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AttributeSet = CreateDefaultSubobject<UAS_Unit>(TEXT("AttributeSet"));

    // GAS 복제 설정
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 네트워크 Actor 복제 설정
    bReplicates = true;
    SetReplicateMovement(true);
}

UAbilitySystemComponent* AUnitBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void AUnitBase::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystem)
    {
        AbilitySystem->InitAbilityActorInfo(this, this);

        if (HasAuthority())
        {
            const TArray<TSubclassOf<UGameplayAbility>> SkillClasses = GetAvailableSkillAbilityClasses();

            for (const TSubclassOf<UGameplayAbility>& SkillClass : SkillClasses)
            {
                if (!SkillClass)
                {
                    continue;
                }

                AbilitySystem->GiveAbility(FGameplayAbilitySpec(SkillClass, 1, 0));
            }
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

void AUnitBase::OnTurnStart()
{
    bIsActiveTurn = true;
    ResetActionPoint();
    ResetSubActionPoint();
    bTurnMustEndAfterCurrentAction = false;
}

void AUnitBase::OnTurnEnd()
{
    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;
}

bool AUnitBase::HasEnoughActionPoint(int32 Cost) const
{
    return CurrentActionPoint >= Cost;
}

bool AUnitBase::ConsumeActionPoint(int32 Cost)
{
    if (!HasEnoughActionPoint(Cost))
    {
        return false;
    }

    CurrentActionPoint -= Cost;

    if (CurrentActionPoint <= 0)
    {
        bTurnMustEndAfterCurrentAction = true;
    }

    return true;
}

bool AUnitBase::HasEnoughSubActionPoint(int32 Cost) const
{
    return CurrentSubActionPoint >= Cost;
}

bool AUnitBase::ConsumeSubActionPoint(int32 Cost)
{
    if (!HasEnoughSubActionPoint(Cost))
    {
        return false;
    }

    CurrentSubActionPoint -= Cost;

    return true;
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
    bTurnMustEndAfterCurrentAction = false;

    ClearSkillContext();
    ClearMoveContext();
    ClearItemContext();
    CurrentActionType = EUnitActionType::None;
    MovePhase = EUnitMovePhase::None;

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
    // 
    // 타일 점유 상태 변경 이후 전열 보호 상태를 다시 계산한다
    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        CombatManager->RefreshTileProtectedByFront();
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

    // 타일 점유 상태 변경 이후 전열 보호 상태를 다시 계산한다
    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        CombatManager->RefreshTileProtectedByFront();
    }
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
    if (!OriginalTileBeforeSkill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ReturnToOriginalTile failed: OriginalTileBeforeSkill is null"));
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ReturnToOriginalTile failed: AICon is null"));
        return;
    }

    PendingTile = OriginalTileBeforeSkill;
    PendingTargetUnit = nullptr;
    MovePhase = EUnitMovePhase::ReturningToOriginalTile;

    FVector TargetLocation = OriginalTileBeforeSkill->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    //UE_LOG(LogTemp, Log, TEXT("[Skill] Returning to original tile"));

    AICon->MoveUnitToLocation(TargetLocation, 2.f);
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
    if (MovePhase == EUnitMovePhase::ReturningToOriginalTile)
    {
        MovePhase = EUnitMovePhase::None;
        ClearSkillContext();
        OnReturnToOriginalTileFinished();
        return;
    }

    if (MovePhase == EUnitMovePhase::MovingToTile)
    {
        MovePhase = EUnitMovePhase::None;

        if (CurrentActionType == EUnitActionType::Move)
        {
            OnMoveActionFinished();
        }

        return;
    }
}

void AUnitBase::OnReturnToOriginalTileFinished()
{
}

void AUnitBase::HandleMoveCompleted()
{
    switch (MovePhase)
    {
    case EUnitMovePhase::MovingToTile:
    {
        // 일반 타일 이동
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            SnapToTile(PendingTile, DefaultBattleRotation);
            PendingTile = nullptr;
        }

        break;
    }
    case EUnitMovePhase::MovingToTarget:
    {
        // 스킬 대상 앞으로 접근
        MovePhase = EUnitMovePhase::WaitingForSkill;
        ExecuteSkillAtTarget();
        break;
    }
    case EUnitMovePhase::ReturningToOriginalTile:
    {
        // 스킬 종료 후 원위치 복귀
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            SnapToTile(PendingTile, DefaultBattleRotation);
            PendingTile = nullptr;
        }

        break;
    }
    default:
    {
        break;
    }
    }

    //UE_LOG(LogTemp, Warning, TEXT("[UnitBase] HandleMoveCompleted | Unit=%s | MovePhase=%d | PendingSkillTargetTile=%s | PendingTargetUnit=%s"), *GetName(), static_cast<int32>(MovePhase), *GetNameSafe(PendingSkillTargetTile), *GetNameSafe(PendingTargetUnit));
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

void AUnitBase::StartSkill(USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile)
{
    //UE_LOG(LogTemp, Log, TEXT("[UnitBase] StartSkill_1 | Unit=%s | SkillData=%s | Ability=%s | TargetTile=%s | OccupyingUnit=%s | MoveToTarget=%s"), *GetName(), *GetNameSafe(SkillData), SkillData ? *GetNameSafe(SkillData->AbilityClass) : TEXT("None"), *GetNameSafe(TargetTile), TargetTile ? *GetNameSafe(TargetTile->GetOccupyingUnit()) : TEXT("None"), SkillData && SkillData->bMoveToTarget ? TEXT("true") : TEXT("false"));

    if (!SkillData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | SkillData is null"));
        return;
    }

    if (!TargetTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | TargetTile is null"));
        return;
    }

    if (!SkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | AbilityClass is null"));
        return;
    }

    // 현재 실행 대기 중인 스킬 컨텍스트를 저장한다
    PendingSkillData = SkillData;
    PendingSkillAbilityClass = SkillData->AbilityClass;
    PendingSkillTargetTile = TargetTile;
    PendingTargetUnit = TargetTile->GetOccupyingUnit();

    // 스킬 시작 전에 원래 타일을 저장한다
    OriginalTileBeforeSkill = CurrentTile;

    // 현재 행동 타입을 Skill로 설정한다
    CurrentActionType = EUnitActionType::Skill;

    if (SkillData->bMoveToTarget)
    {
        AUnitBase* TargetUnit = TargetTile->GetOccupyingUnit();

        if (!TargetUnit)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | MoveToTarget requires occupied target tile"));
            return;
        }

        MoveToTarget(TargetUnit);
        return;
    }

    if (!AbilitySystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | AbilitySystem is null"));
        return;
    }

    // Ability 활성화 성공 여부를 바로 확인한다.
    const bool bActivated = AbilitySystem->TryActivateAbilityByClass(SkillData->AbilityClass);

    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill Activate | Unit=%s | Ability=%s | Activated=%d | TargetTile=(%d,%d) | TargetUnit=%s"),
        *GetNameSafe(this),
        *GetNameSafe(SkillData->AbilityClass),
        bActivated ? 1 : 0,
        TargetTile->GridCoord.X,
        TargetTile->GridCoord.Y,
        *GetNameSafe(PendingTargetUnit));

}

void AUnitBase::ExecuteSkillAtTarget()
{
    
    // 행동 대기 상태가 아니면 스킬 실행을 시작할 수 없다.
    if (MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteSkillAtTarget Return | Reason=InvalidMovePhase"));
        return;
    }

    if (!PendingSkillData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=PendingSkillDataNull"));
        OnSkillFinished();
        return;
    }

    if (PendingSkillData->TargetingType == ESkillTargetingType::Unit)
    {
        if (!PendingTargetUnit || !PendingTargetUnit->IsUnitAlive())
        {
            UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=InvalidTargetUnit"));
            OnSkillFinished();
            return;
        }
    }
    else if (PendingSkillData->TargetingType == ESkillTargetingType::Tile)
    {
        if (!PendingSkillTargetTile)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=InvalidTargetTile"));
            OnSkillFinished();
            return;
        }
    }

    // 스킬 직전에 타겟을 바라보도록 회전한다.
    FVector LookTargetLocation = GetActorLocation();

    if (PendingTargetUnit)
    {
        LookTargetLocation = PendingTargetUnit->GetActorLocation();
    }
    else if (PendingSkillTargetTile)
    {
        LookTargetLocation = PendingSkillTargetTile->GetActorLocation();
    }

    FVector Direction = LookTargetLocation - GetActorLocation();
    Direction.Z = 0.0f;

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    // 실제 스킬 실행은 GAS Ability가 담당한다.
    if (!AbilitySystem || !PendingSkillData || !PendingSkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=NoASCOrAbilityClass | ASC=%d | SkillData=%s | AbilityClass=%s"), AbilitySystem ? 1 : 0, *GetNameSafe(PendingSkillData), PendingSkillData ? *GetNameSafe(PendingSkillData->AbilityClass) : TEXT("None"));
        OnSkillFinished();
        return;
    }

    const bool bActivated = AbilitySystem->TryActivateAbilityByClass(PendingSkillAbilityClass);
    
    // Ability 활성화 실패 시 스킬을 진행할 수 없으므로 종료한다.
    if (!bActivated)
    {
        OnSkillFinished();
    }
}

TArray<AUnitBase*> AUnitBase::ResolveSkillTargetUnits()
{
    TArray<AUnitBase*> Result;

    if (!PendingSkillData)
    {
        return Result;
    }

    if (PendingSkillData->AreaType == ESkillAreaType::Single)
    {
        AUnitBase* TargetUnit = PendingTargetUnit;

        if (!TargetUnit && PendingSkillTargetTile)
        {
            TargetUnit = PendingSkillTargetTile->GetOccupyingUnit();
        }

        if (!TargetUnit)
        {
            return Result;
        }

        if (TargetUnit == this)
        {
            return Result;
        }

        if (!TargetUnit->IsUnitAlive())
        {
            return Result;
        }

        Result.Add(TargetUnit);
        return Result;
    }

    ACombatGridTile* CenterTile = nullptr;

    if (PendingSkillData->AreaType == ESkillAreaType::AroundTarget)
    {
        CenterTile = PendingSkillTargetTile;
    }
    else if (PendingSkillData->AreaType == ESkillAreaType::AroundSelf)
    {
        CenterTile = CurrentTile;
    }
    else
    {
        return Result;
    }

    if (!CenterTile)
    {
        return Result;
    }

    ACombatGridManager* CombatGridManager = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatGridManager::StaticClass()));

    if (!CombatGridManager)
    {
        return Result;
    }

    const TArray<ACombatGridTile*> AreaTiles = CombatGridManager->GetTilesInChebyshevRange(CenterTile, PendingSkillData->AreaRadius);

    return UCombatTargetingLibrary::CollectUniqueAliveUnitsFromTiles(AreaTiles, this);
}

void AUnitBase::OnSkillFinished()
{
    //UE_LOG(LogTemp, Log, TEXT("[Skill] OnSkillFinished: %s"), *GetName());

    if (bIsDead)
    {
        return;
    }

    if (MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        return;
    }

    ReturnToOriginalTile();
}

void AUnitBase::ClearSkillContext()
{
    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    PendingSkillTargetTile = nullptr;
    PendingSkillData = nullptr;
    PendingSkillAbilityClass = nullptr;
    OriginalTileBeforeSkill = nullptr;
    bSkillDamageApplied = false;
    CurrentActionType = EUnitActionType::None;
}

void AUnitBase::StartMoveAction(ACombatGridTile* TargetTile)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!bIsActiveTurn)
    {
        return;
    }

    if (MovePhase != EUnitMovePhase::None)
    {
        return;
    }

    if (!TargetTile)
    {
        return;
    }
    
    //자기타일로 이동하려는 경우
    if (TargetTile == CurrentTile)
    {
        return;
    }

    if (!HasEnoughSubActionPoint(1))
    {
        return;
    }

    if (!ConsumeSubActionPoint(1))
    {
        return;
    }

    
    CurrentActionType = EUnitActionType::Move;

    MoveToTile(TargetTile);
}

void AUnitBase::OnMoveActionFinished()
{
    CurrentActionType = EUnitActionType::None;
}

void AUnitBase::ClearMoveContext()
{
    PendingTile = nullptr;
}

void AUnitBase::StartItemAction(AUnitBase* TargetUnit)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!bIsActiveTurn)
    {
        return;
    }

    if (MovePhase != EUnitMovePhase::None)
    {
        return;
    }

    if (!HasEnoughSubActionPoint(1))
    {
        return;
    }

    if (!ConsumeSubActionPoint(1))
    {
        return;
    }

    

    CurrentActionType = EUnitActionType::Item;
    PendingTargetUnit = TargetUnit;

    ExecuteItemAtTarget();
}

void AUnitBase::ExecuteItemAtTarget()
{
    OnItemFinished();
}

void AUnitBase::OnItemFinished()
{
    CurrentActionType = EUnitActionType::None;
    ClearItemContext();
}

void AUnitBase::ClearItemContext()
{
    PendingTargetUnit = nullptr;
}

TArray<TSubclassOf<UGameplayAbility>> AUnitBase::GetAvailableSkillAbilityClasses() const
{
    TArray<TSubclassOf<UGameplayAbility>> Result;

    if (DefaultAttackAbilityClass)
    {
        Result.Add(DefaultAttackAbilityClass);
    }

    for (const TSubclassOf<UGameplayAbility>& SkillClass : EquippedSkillAbilityClasses)
    {
        if (!SkillClass)
        {
            continue;
        }

        Result.Add(SkillClass);
    }

    return Result;
}

USkillDefinitionDataAsset* AUnitBase::FindSkillDataByAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const
{
    if (!AbilityClass)
    {
        return nullptr;
    }

    for (USkillDefinitionDataAsset* SkillData : EquippedSkillDataAssets)
    {
        if (!SkillData)
        {
            continue;
        }

        if (SkillData->AbilityClass == AbilityClass)
        {
            return SkillData;
        }
    }

    return nullptr;
}