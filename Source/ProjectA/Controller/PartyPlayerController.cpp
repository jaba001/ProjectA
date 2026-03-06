#include "PartyPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Combat/CombatManager.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridTile.h"

APartyPlayerController::APartyPlayerController()
{
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bShowMouseCursor = true;     
}

void APartyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);

    // HUD »ý¼º
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
        }
    }

    CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        //UE_LOG(LogTemp, Warning, TEXT("CombatManager Found"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CombatManager NOT Found"));
    }
}

AUnitBase* APartyPlayerController::GetActiveUnit() const
{
    if (!CombatManager)
        return nullptr;

    return CombatManager->GetCurrentUnit();
}

void APartyPlayerController::RequestEndTurn()
{
    if (CombatManager)
    {
        CombatManager->RequestEndTurn();
        //UE_LOG(LogTemp, Warning, TEXT("RequestEndTurn Called PC"));
    }
}

void APartyPlayerController::SetSelectedTile(ACombatGridTile* InTile)
{
    if (!InTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PC] SetSelectedTile null"));
        return;
    }

    SelectedTile = InTile;

    UE_LOG(LogTemp, Warning,
        TEXT("[PC] Selected Tile (%d,%d)"),
        InTile->GridCoord.X,
        InTile->GridCoord.Y);
}