#include "UnitBase.h"
#include "Grid/Combat/CombatGridTile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/Ability/GA_DefaultAttack.h"

AUnitBase::AUnitBase()
{
    PrimaryActorTick.bCanEverTick = true;

    bIsActiveTurn = false;

    GetCharacterMovement()->MaxWalkSpeed = 350.f;
    
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
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

    //테스트용
    if (AttributeSet)
    {
        AttributeSet->InitMaxHP(100.f);
        AttributeSet->InitHP(100.f);
    }

    if (AbilitySystem)
    {
        AbilitySystem->TryActivateAbilityByClass(UGA_DefaultAttack::StaticClass());
    }
}

void AUnitBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

UAbilitySystemComponent* AUnitBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void AUnitBase::SetCurrentTile(ACombatGridTile* NewTile)
{
    if (NewTile == nullptr) return;
    CurrentTile = NewTile;
}

void AUnitBase::MoveToTile(ACombatGridTile* TargetTile)
{
    if (TargetTile == nullptr) return;

    FVector TargetLocation = TargetTile->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    // AIController 가져오기
    AAIController* AICon = Cast<AAIController>(GetController());
    if (AICon == nullptr)
    {
        // UnitBase는 PlayerController가 Possess하지 않으므로 AIController 자동 생성 필요
        SpawnDefaultController();
        AICon = Cast<AAIController>(GetController());
    }

    if (AICon)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(AICon, TargetLocation);

        UE_LOG(LogTemp, Warning, TEXT("[AUnitBase::MoveToTile] Walking to location (%s)"), *TargetLocation.ToString());
    }

    CurrentTile = TargetTile;
}

void AUnitBase::OnTurnStart()
{
    bIsActiveTurn = true;
}

void AUnitBase::OnTurnEnd()
{
    bIsActiveTurn = false;
}

