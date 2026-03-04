#include "PartyPlayerController.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Game/GameState/CombatGameState.h"
#include "Blueprint/UserWidget.h"

APartyPlayerController::APartyPlayerController()
{
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bShowMouseCursor = true;     
}

void APartyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    ActiveUnitIndex = 0;

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);

    ACombatGameState* CGS = GetWorld()->GetGameState<ACombatGameState>();
    if (CGS)
    {
        TurnManager = CGS->TurnManager;

        if (TurnManager)
        {
            //UE_LOG(LogTemp, Warning, TEXT("[APartyPlayerController] TurnManager loaded."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[APartyPlayerController] TurnManager is NULL"));
        }
    }

    // HUD 생성
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
        }
    }
}

AUnitBase* APartyPlayerController::GetActiveUnit() const
{
    if (PartyUnits.Num() == 0) return nullptr;
    if (!PartyUnits.IsValidIndex(ActiveUnitIndex)) return nullptr;
    return PartyUnits[ActiveUnitIndex];
}

void APartyPlayerController::SelectNextUnit()
{
    if (PartyUnits.Num() == 0) return;
    ActiveUnitIndex = (ActiveUnitIndex + 1) % PartyUnits.Num();
}

void APartyPlayerController::RegisterPartyUnits(const TArray<AUnitBase*>& Units, const TArray<ACombatGridTile*>& StartTiles)
{
    //서버코드
    if (!HasAuthority()) return;

    // 유닛이 없으면 아무 것도 하지 않음
    if (Units.Num() == 0) return;

    // Case3: 유닛 수가 타일 수보다 많은 경우 → 전체 무시하며 Warning 로그 출력
    if (Units.Num() > StartTiles.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("[APartyPlayerController::RegisterPartyUnits] Units.Num() > StartTiles.Num(). Registration aborted. Units: %d, Tiles: %d"), Units.Num(), StartTiles.Num());
        ensureMsgf(false, TEXT("Units greater than tiles"));
        return;
    }

    PartyUnits.Empty();

    // Case2: 유닛이 타일보다 적은 경우 → 유닛 수만큼만 등록
    const int32 Count = Units.Num();

    for (int32 i = 0; i < Count; i++)
    {
        AUnitBase* Unit = Units[i];
        ACombatGridTile* Tile = StartTiles[i];

        if (Unit == nullptr || Tile == nullptr) continue;

        Unit->UnitIndex = i;
        Unit->SetCurrentTile(Tile);

        FVector NewLocation = Tile->GetActorLocation();
        NewLocation.Z += 100.f;
        Unit->SetActorLocation(NewLocation);

        PartyUnits.Add(Unit);
    }

    ActiveUnitIndex = 0;
    
    //UE_LOG(LogTemp, Warning, TEXT("[APartyPlayerController::RegisterPartyUnits] Units=%d"), Units.Num());

    ACombatGameState* CGS = GetWorld()->GetGameState<ACombatGameState>();
    if (CGS && CGS->TurnManager)
    {
        CGS->TurnManager->InitializeTurnOrder(PartyUnits);
    }

    // TODO: 테스트코드 유닛1개 최소활성화
    if (PartyUnits.Num() > 0)
    {
        PartyUnits[0]->bIsActiveTurn = true;
    }

}
