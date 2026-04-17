#include "CombatGridTile.h"
#include "Engine/World.h"

#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/CombatManager.h"
#include "Unit/UnitBase.h"

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
        AUnitBase* ActiveUnit = PC->GetActiveUnit();

        if (!ActiveUnit)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTile] Skill click failed | ActiveUnit null"));
            return;
        }

        USkillDefinitionDataAsset* SkillData = PC->GetPendingSkillData();

        if (!SkillData)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTile] Skill click failed | PendingSkillData is null"));
            return;
        }

        if (!SkillData->AbilityClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTile] Skill click failed | AbilityClass is null"));
            return;
        }

        if (!PC->IsValidTileForPendingSkill(this))
        {
            return;
        }

        //UE_LOG(LogTemp, Log, TEXT("[GridTile] PlayerSkillClick | Unit=%s | SkillData=%s | Ability=%s | TargetTile=(%d,%d) | OccupyingUnit=%s"), *GetNameSafe(ActiveUnit), *GetNameSafe(SkillData), *GetNameSafe(SkillData->AbilityClass), GridCoord.X, GridCoord.Y, *GetNameSafe(GetOccupyingUnit()));

        PC->SetSelectedTile(this);
        PC->CancelTileInputMode();
        ActiveUnit->StartSkill(SkillData, this);
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

    //if (TileSprite)
    //{
    //    TileSprite->SetSpriteColor(OriginalColor);
    //}

    UpdateTileVisual();
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

    if (bSkillTargetHighlighted && ActiveSprite)
    {
        TileSprite->SetSprite(ActiveSprite);
    }
    else if (bMovableHighlighted && MovableSprite)
    {
        TileSprite->SetSprite(MovableSprite);
    }
    else if (!OccupyingUnit)
    {
        TileSprite->SetSprite(EmptySprite);
    }
    else
    {
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

    if (bProtectedByFront)
    {
        TileSprite->SetSpriteColor(ProtectedByFrontColor);
    }
    else
    {
        TileSprite->SetSpriteColor(OriginalColor);
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

    bMovableHighlighted = true;
    bSkillTargetHighlighted = false;
    UpdateTileVisual();
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

    bSkillTargetHighlighted = true;
    bMovableHighlighted = false;
    UpdateTileVisual();
}

void ACombatGridTile::ClearHighlightVisual()
{
    bMovableHighlighted = false;
    bSkillTargetHighlighted = false;
    UpdateTileVisual();
}

void ACombatGridTile::SetProtectedByFront(bool bInProtectedByFront)
{
    bProtectedByFront = bInProtectedByFront;
    
    //UE_LOG(LogTemp, Log, TEXT("[GridTile] SetProtectedByFront | Coord=(%d,%d) | Protected=%s"), GridCoord.X, GridCoord.Y, bProtectedByFront ? TEXT("true") : TEXT("false"));
   
    UpdateTileVisual();
}
