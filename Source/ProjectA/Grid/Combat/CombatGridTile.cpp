#include "Grid/Combat/CombatGridTile.h"
#include "Controller/PartyPlayerController.h"
#include "Combat/CombatManager.h"
#include "Unit/UnitBase.h"
#include "Engine/World.h"

// Sets default values
ACombatGridTile::ACombatGridTile()
{
    PrimaryActorTick.bCanEverTick = false;

    OccupyingUnit = nullptr;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // Collision Box (2배 확장)
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(RootComponent);
    CollisionBox->SetBoxExtent(FVector(90.f, 90.f, 10.f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Sprite (2배 확대 + X축 90도 회전)
    TileSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("TileSprite"));
    TileSprite->SetupAttachment(RootComponent);
    TileSprite->SetRelativeScale3D(FVector(2.f, 1.f, 2.f));
    TileSprite->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
    TileSprite->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
    TileSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACombatGridTile::BeginPlay()
{
    Super::BeginPlay();
    // 초기 컬러 저장
    if (TileSprite)
    {
        OriginalColor = TileSprite->GetSpriteColor();
    }
}

//void ACombatGridTile::Tick(float DeltaTime)
//{
//    Super::Tick(DeltaTime);
//}

void ACombatGridTile::NotifyActorOnClicked(FKey ButtonPressed)
{
    Super::NotifyActorOnClicked(ButtonPressed);

    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());

    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GridTile] PlayerController null"));
        return;
    }

    ACombatManager* CombatManager = PC->GetCombatManager();

    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GridTile] CombatManager null"));
        return;
    }

    if (PC->IsSkillInputMode())
    {
        if (!CombatManager->IsSkillTargetTile(this))
        {
            return;
        }

        AUnitBase* ActiveUnit = PC->GetActiveUnit();

        if (!ActiveUnit)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTile] Skill click failed | ActiveUnit null"));
            return;
        }

        AUnitBase* TargetUnit = GetOccupyingUnit();

        if (!TargetUnit)
        {
            return;
        }

        if (TargetUnit->GetTeam() == ActiveUnit->GetTeam())
        {
            return;
        }

        TSubclassOf<UGameplayAbility> AbilityClass = PC->GetPendingSkillInputAbilityClass();
        const bool bMoveToTarget = PC->GetPendingSkillInputMoveToTarget();

        if (!AbilityClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTile] Skill click failed | Pending AbilityClass is null"));
            return;
        }

        PC->SetSelectedTile(this);
        CombatManager->ClearSkillTargetTilesHighlight();
        PC->SetTileInputMode(ETileInputMode::None);

        ActiveUnit->StartSkill(AbilityClass, TargetUnit, bMoveToTarget);
        return;
    }

    if (PC->IsMoveInputMode())
    {
        if (!CombatManager->IsReachableMoveTile(this))
        {
            return;
        }

        AUnitBase* ActiveUnit = PC->GetActiveUnit();

        if (!ActiveUnit)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTile] Move click failed | ActiveUnit null"));
            return;
        }

        PC->SetSelectedTile(this);
        CombatManager->ClearMovableTilesHighlight();
        PC->SetTileInputMode(ETileInputMode::None);

        ActiveUnit->StartMoveAction(this);
        return;
    }

    PC->SetSelectedTile(this);
}

void ACombatGridTile::NotifyActorBeginCursorOver()
{
    Super::NotifyActorBeginCursorOver();

    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());

    if (!PC)
    {
        return;
    }

    //if (PC->GetTileInputMode() != ETileInputMode::Move)
    //{
    //    return;
    //}

    if (TileSprite)
    {
        TileSprite->SetSpriteColor(FLinearColor::Gray);
    }
}

void ACombatGridTile::NotifyActorEndCursorOver()
{
    Super::NotifyActorEndCursorOver();

    if (TileSprite)
    {
        TileSprite->SetSpriteColor(OriginalColor);
    }
}

void ACombatGridTile::SetOccupyingUnit(AUnitBase* NewUnit)
{
    OccupyingUnit = NewUnit;

    UpdateTileVisual();
}

void ACombatGridTile::UpdateTileVisual()
{
    if (!TileSprite)
    {
        return;
    }

    if (!OccupyingUnit)
    {
        TileSprite->SetSprite(EmptySprite);
        return;
    }

    switch (OccupyingUnit->GetTeam())
    {
    case ETeam::Player:
        TileSprite->SetSprite(PlayerSprite);
        break;

    case ETeam::Enemy:
        TileSprite->SetSprite(EnemySprite);
        break;

    default:
        TileSprite->SetSprite(EmptySprite);
        break;
    }
}

void ACombatGridTile::ApplyMovableTileVisual()
{
    if (!TileSprite)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GridTile] ApplyMovableTileVisual failed | TileSprite is null"));
        return;
    }

    if (!MovableSprite)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GridTile] ApplyMovableTileVisual failed | MovableSprite is null | Coord=(%d,%d)"), GridCoord.X, GridCoord.Y);
        return;
    }

    TileSprite->SetSprite(MovableSprite);

    //UE_LOG(LogTemp, Log, TEXT("[GridTile] ApplyMovableTileVisual success | Coord=(%d,%d)"), GridCoord.X, GridCoord.Y);
}

void ACombatGridTile::ApplySkillTargetTileVisual()
{
    if (!TileSprite)
    {
        return;
    }

    if (!ActiveSprite)
    {
        return;
    }

    TileSprite->SetSprite(ActiveSprite);
}
