#include "CombatGridTile.h"
#include "Engine/World.h"

#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/CombatManager.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ACombatGridTile::ACombatGridTile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);

    OccupyingUnit = nullptr;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // Collision box (scaled up by 2x)
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(RootComponent);
    CollisionBox->SetBoxExtent(FVector(90.f, 90.f, 10.f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Sprite (scaled up by 2x and rotated 90 degrees on X axis)
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

    // Cache original color
    if (TileSprite && !bOriginalColorCached)
    {
        OriginalColor = TileSprite->GetSpriteColor();
        bOriginalColorCached = true;
    }
    OnRep_GridIndex();
    UpdateTileVisual();
}

void ACombatGridTile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatGridTile, GridCoord);
    DOREPLIFETIME(ACombatGridTile, GridManager);
    DOREPLIFETIME(ACombatGridTile, OccupyingUnit);
    DOREPLIFETIME(ACombatGridTile, Territory);
    DOREPLIFETIME(ACombatGridTile, bProtectedByFront);
}

void ACombatGridTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (RegisteredGridManager.IsValid())
    {
        RegisteredGridManager->UnregisterReplicatedTile(this);
    }
    RegisteredGridManager.Reset();
    Super::EndPlay(EndPlayReason);
}

void ACombatGridTile::InitializeGridTile(ACombatGridManager* Manager, const FIntPoint& Coord, ETileTerritory NewTerritory)
{
    if (!HasAuthority() || !IsValid(Manager) || Manager->GetWorld() != GetWorld())
    {
        return;
    }

    GridManager = Manager;
    GridCoord = Coord;
    Territory = NewTerritory;
    OnRep_GridIndex();
    UpdateTileVisual();
    ForceNetUpdate();
}

void ACombatGridTile::OnRep_GridIndex()
{
    if (RegisteredGridManager.IsValid() && RegisteredGridManager.Get() != GridManager.Get())
    {
        RegisteredGridManager->UnregisterReplicatedTile(this);
    }
    RegisteredGridManager = GridManager.Get();
    if (IsValid(GridManager))
    {
        GridManager->RegisterReplicatedTile(this);
    }
}

void ACombatGridTile::OnRep_TileState()
{
    UpdateTileVisual();
}

void ACombatGridTile::SetTerritory(ETileTerritory NewTerritory)
{
    if (!HasAuthority())
    {
        return;
    }
    Territory = NewTerritory;
    UpdateTileVisual();
    ForceNetUpdate();
}

//void ACombatGridTile::Tick(float DeltaTime)
//{
//    Super::Tick(DeltaTime);
//}

void ACombatGridTile::NotifyActorOnClicked(FKey ButtonPressed)
{
    Super::NotifyActorOnClicked(ButtonPressed);

    if (APartyPlayerController* Controller = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        Controller->HandleTileClicked(this);
    }
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
    if (!HasAuthority())
    {
        return;
    }

    OccupyingUnit = NewUnit;

    UpdateTileVisual();
    ForceNetUpdate();
}

void ACombatGridTile::UpdateTileVisual()
{
    if (!TileSprite)
    {
        return;
    }

    // Initial replication may update visuals before BeginPlay caches the color.
    // 초기 복제가 BeginPlay의 색상 캐시보다 먼저 화면을 갱신할 수 있습니다.
    if (!bOriginalColorCached)
    {
        OriginalColor = TileSprite->GetSpriteColor();
        bOriginalColorCached = true;
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
    if (!HasAuthority())
    {
        return;
    }

    bProtectedByFront = bInProtectedByFront;

    //UE_LOG(LogTemp, Log, TEXT("[GridTile] SetProtectedByFront | Coord=(%d,%d) | Protected=%s"), GridCoord.X, GridCoord.Y, bProtectedByFront ? TEXT("true") : TEXT("false"));

    UpdateTileVisual();
    ForceNetUpdate();
}
