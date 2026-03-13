#include "UI/BattleHUD_Widget.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/CombatManager.h"
#include "Game/Turn/TurnManager.h"

void UBattleHUD_Widget::NativeConstruct()
{
    Super::NativeConstruct();

    CachedCombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));
}

FText UBattleHUD_Widget::GetTurnInfoText() const
{
    if (!CachedCombatManager)
    {
        return FText::FromString(TEXT("TURN: 0  Current Unit: None"));
    }

    UTurnManager* TurnManager = CachedCombatManager->GetTurnManager();

    if (!TurnManager)
    {
        return FText::FromString(TEXT("TURN: 0  Current Unit: None"));
    }

    const int32 CurrentTurnCounter = TurnManager->GetTurnCounter();
    const FString CurrentUnitName = TurnManager->GetCurrentUnitName();
    const FString DisplayString = FString::Printf(TEXT("TURN: %d  Current Unit: %s"), CurrentTurnCounter, *CurrentUnitName);

    return FText::FromString(DisplayString);
}