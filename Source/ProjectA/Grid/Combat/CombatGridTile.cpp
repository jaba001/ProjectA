#include "Grid/Combat/CombatGridTile.h"
#include "Controller/PartyPlayerController.h"
#include "Unit/UnitBase.h"
#include "Engine/World.h"

// Sets default values
ACombatGridTile::ACombatGridTile()
{
    PrimaryActorTick.bCanEverTick = true;

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

// Called when the game starts or when spawned
void ACombatGridTile::BeginPlay()
{
    Super::BeginPlay();
    // 초기 컬러 저장
    OriginalColor = TileSprite->GetSpriteColor();
}

// Called every frame
void ACombatGridTile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACombatGridTile::NotifyActorOnClicked(FKey ButtonPressed)
{
    Super::NotifyActorOnClicked(ButtonPressed);

    // PlayerController 가져오기
    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ACombatGridTile::NotifyActorOnClicked] PlayerController is null"));
        return;
    }

    // 현재 활성 유닛 가져오기
    AUnitBase* ActiveUnit = PC->GetActiveUnit();
    if (ActiveUnit == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ACombatGridTile::NotifyActorOnClicked] ActiveUnit is null"));
        return;
    }

    //UE_LOG(LogTemp, Warning, TEXT("[ACombatGridTile::NotifyActorOnClicked] Tile=(%d,%d) Actor=%s | ActiveUnit=%s Index=%d | MovingTo=(%d,%d)"), GridCoord.X, GridCoord.Y, *GetName(), *ActiveUnit->GetName(), ActiveUnit->UnitIndex, GridCoord.X, GridCoord.Y);

    if (!ActiveUnit->bIsActiveTurn)
    {
        //UE_LOG(LogTemp, Warning, TEXT("[ACombatGridTile::NotifyActorOnClicked] Unit %s tried to move but not its turn"), *ActiveUnit->GetName());
        return;
    }

    // 유닛 이동
    ActiveUnit->MoveToTile(this);
}

void ACombatGridTile::NotifyActorBeginCursorOver()
{
    Super::NotifyActorBeginCursorOver();

    // 마우스 오버 시 초록색으로 변경
    if (TileSprite)
    {
        TileSprite->SetSpriteColor(FLinearColor::Green);
    }

}

void ACombatGridTile::NotifyActorEndCursorOver()
{
    Super::NotifyActorEndCursorOver();

    // 마우스 오버 종료 시 원래 컬러로 복원
    if (TileSprite)
    {
        TileSprite->SetSpriteColor(OriginalColor);
    }

}