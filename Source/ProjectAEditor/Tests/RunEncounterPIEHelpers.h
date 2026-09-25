#pragma once

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Misc/AutomationTest.h"
#include "UI/Gameplay/RunEncounterWidget.h"

namespace RunEncounterPIE
{
    inline URunEncounterWidget* FindScreen(AGameplayPlayerController* Controller)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller->GetWorld(), Widgets, URunEncounterWidget::StaticClass(), false);
        for (UUserWidget* Widget : Widgets)
        {
            if (Widget->GetOwningPlayer() == Controller && Cast<URunEncounterWidget>(Widget)->IsActivated()) return Cast<URunEncounterWidget>(Widget);
        }
        return nullptr;
    }

    // Wait for each replicated screen before selecting shop two and leaving through the real buttons.
    // 각 복제 화면을 기다린 뒤 실제 버튼으로 상점2를 선택하고 퇴장합니다.
    inline bool TickToMap(FAutomationTestBase* Test, AGameplayPlayerController* Host, const TArray<AGameplayPlayerController*>& Clients, bool& bFailed)
    {
        URunStateSubsystem* Run = Host->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        const ERunPhase Phase = Run->GetPhase();
        if (Phase == ERunPhase::Map) return true;
        if (Phase != ERunPhase::EncounterChoice && Phase != ERunPhase::Shop) return false;
        URunEncounterWidget* HostScreen = FindScreen(Host);
        if (!HostScreen) return false;
        const FRunEncounterProgress Progress = Run->GetEncounterProgress();
        if (!Test->TestEqual(TEXT("The prototype exposes three distinct offers."), Progress.Offers.Num(), 3))
        {
            bFailed = true;
            return false;
        }
        for (AGameplayPlayerController* Client : Clients)
        {
            if (!Client) return false;
            AGameplayGameState* State = Client->GetWorld()->GetGameState<AGameplayGameState>();
            URunEncounterWidget* Screen = FindScreen(Client);
            if (!State || !Screen || State->GetViewState().Phase != Phase) return false;
            const FRunEncounterProgress& Received = State->GetViewState().EncounterProgress;
            if (Received.Offers.Num() != 3 || Received.SelectedEncounterId != Progress.SelectedEncounterId || Received.bCompleted != Progress.bCompleted) return false;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                if (Received.Offers[Index].EncounterId != Progress.Offers[Index].EncounterId || Received.Offers[Index].DisplayName.ToString() != Progress.Offers[Index].DisplayName.ToString()) return false;
            }
            UVerticalBox* Actions = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("EncounterActions")));
            if (!Actions || Actions->GetChildrenCount() < 3) return false;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                UButton* Choice = Cast<UButton>(Actions->GetChildAt(Index));
                if (!Test->TestTrue(TEXT("Clients cannot activate encounter choices."), Choice && !Choice->GetIsEnabled())) bFailed = true;
            }
            UButton* Exit = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_LeaveShop")));
            if (!Test->TestTrue(TEXT("Clients cannot leave the shop."), Exit && !Exit->GetIsEnabled())) bFailed = true;
            Client->RequestSelectRunEncounter(TEXT("Shop_03"));
            Client->RequestLeaveRunEncounter();
            if (!Test->TestTrue(TEXT("Client encounter commands preserve the server choice and phase."), Run->GetPhase() == Phase && Run->GetEncounterProgress().SelectedEncounterId == Progress.SelectedEncounterId)) bFailed = true;
        }
        UVerticalBox* Actions = Cast<UVerticalBox>(HostScreen->GetWidgetFromName(TEXT("EncounterActions")));
        UButton* Button = Phase == ERunPhase::EncounterChoice ? (Actions ? Cast<UButton>(Actions->GetChildAt(1)) : nullptr) : Cast<UButton>(HostScreen->GetWidgetFromName(TEXT("Button_LeaveShop")));
        if (!Button || !Button->GetIsEnabled()) return false;
        if (Phase == ERunPhase::Shop)
        {
            UTextBlock* Title = Cast<UTextBlock>(HostScreen->GetWidgetFromName(TEXT("Text_EncounterTitle")));
            if (!Test->TestTrue(TEXT("The selected shop title is visible."), Title && Title->GetText().ToString() == TEXT("아이템상점"))) bFailed = true;
        }
        if (bFailed) return false;
        Button->OnClicked.Broadcast();
        const ERunPhase Expected = Phase == ERunPhase::EncounterChoice ? ERunPhase::Shop : ERunPhase::Map;
        if (!Test->TestTrue(TEXT("Host shop buttons commit the expected phase without changing character ownership."), Run->GetPhase() == Expected && Run->GetEncounterProgress().SelectedEncounterId == TEXT("Shop_02"))) bFailed = true;
        return false;
    }
}
