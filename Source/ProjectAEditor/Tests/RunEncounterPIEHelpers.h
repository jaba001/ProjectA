#pragma once

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Misc/AutomationTest.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/Gameplay/RunMapWidget.h"

namespace RunEncounterPIE
{
    template <typename T>
    T* FindActiveScreen(AGameplayPlayerController* Controller)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller->GetWorld(), Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets)
        {
            if (Widget->GetOwningPlayer() == Controller && Cast<T>(Widget)->IsActivated()) return Cast<T>(Widget);
        }
        return nullptr;
    }

    inline URunEncounterWidget* FindScreen(AGameplayPlayerController* Controller)
    {
        return FindActiveScreen<URunEncounterWidget>(Controller);
    }

    inline bool PresentationReady(AGameplayPlayerController* Controller, UUserWidget* Screen)
    {
        // Root enabled state follows camera transition only; client choice permissions disable individual buttons.
        // 루트 활성 상태는 카메라 전환만 따르며 클라이언트 선택 권한은 개별 버튼을 비활성화합니다.
        return Controller && Controller->IsLocalController() && Screen && Screen->GetIsEnabled() && Controller->PlayerCameraManager && !Controller->PlayerCameraManager->PendingViewTarget.Target;
    }

    inline bool VerifyShopPresentation(FAutomationTestBase* Test, AGameplayPlayerController* Controller, URunEncounterWidget* Screen, const FRunEncounterProgress& Progress, bool bTargetRun, int32 CompletedSteps)
    {
        const FRunEncounterOffer* Offer = Progress.FindSelectedOffer();
        const AEncounterPrototypeStage* Stage = Cast<AEncounterPrototypeStage>(Controller->GetViewTarget());
        bool bValid = Test->TestTrue(TEXT("Each local controller finishes on the NPC stage matching its own committed or replicated selected offer."), Offer && Stage && Stage->GetWorld() == Controller->GetWorld() && Stage->MatchesOffer(*Offer));
        UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_EncounterTitle")));
        FText ExpectedTitle = Offer ? Offer->GetDisplayName() : FText::GetEmpty();
        if (bTargetRun) ExpectedTitle = FText::Format(NSLOCTEXT("RunEncounter", "TargetProgressTitle", "{0} · {1}/80 완료"), ExpectedTitle, FText::AsNumber(CompletedSteps));
        bValid &= Test->TestTrue(TEXT("Each visible shop title matches its resolved saved label and current Target progress when applicable."), Title && Title->GetText().ToString() == ExpectedTitle.ToString());
        return bValid;
    }

    // Wait for every local camera and replicated screen before selecting shop two, leaving, or starting combat.
    // 상점2 선택·퇴장·전투 시작 전에 모든 로컬 카메라와 복제 화면의 전환 완료를 기다립니다.
    inline bool TickToMap(FAutomationTestBase* Test, AGameplayPlayerController* Host, const TArray<AGameplayPlayerController*>& Clients, bool& bFailed)
    {
        URunStateSubsystem* Run = Host->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        const ERunPhase Phase = Run->GetPhase();
        if (Phase == ERunPhase::Map)
        {
            TArray<AGameplayPlayerController*> Controllers = Clients;
            Controllers.Insert(Host, 0);
            for (AGameplayPlayerController* Controller : Controllers)
            {
                if (!Controller) return false;
                if (Controller != Host)
                {
                    const AGameplayGameState* State = Controller->GetWorld()->GetGameState<AGameplayGameState>();
                    if (!State || State->GetViewState().Phase != ERunPhase::Map) return false;
                }
                if (!PresentationReady(Controller, FindActiveScreen<URunMapWidget>(Controller))) return false;
                if (!Test->TestTrue(TEXT("Every local camera returns to the authored overview before the next combat starts."), Controller->GetViewTarget() && Controller->GetViewTarget()->ActorHasTag(TEXT("GameplayEncounterOverview")))) bFailed = true;
            }
            return !bFailed;
        }
        if (Phase != ERunPhase::EncounterChoice && Phase != ERunPhase::Shop) return false;
        URunEncounterWidget* HostScreen = FindScreen(Host);
        if (!PresentationReady(Host, HostScreen)) return false;
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
            if (!State || !PresentationReady(Client, Screen) || State->GetViewState().Phase != Phase) return false;
            const FRunEncounterProgress& Received = State->GetViewState().EncounterProgress;
            if (Received.Offers.Num() != 3 || Received.SelectedEncounterId != Progress.SelectedEncounterId || Received.bCompleted != Progress.bCompleted || Received.AfterCompletedNodeCount != Progress.AfterCompletedNodeCount || Received.VisitIndex != Progress.VisitIndex) return false;
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
            if (Phase == ERunPhase::Shop && !VerifyShopPresentation(Test, Client, Screen, Received, State->GetViewState().bTargetRun, State->GetViewState().TargetCompletedSteps)) bFailed = true;
            Client->RequestSelectRunEncounter(TEXT("Shop_03"));
            Client->RequestLeaveRunEncounter();
            if (!Test->TestTrue(TEXT("Client encounter commands preserve the server choice and phase."), Run->GetPhase() == Phase && Run->GetEncounterProgress().SelectedEncounterId == Progress.SelectedEncounterId)) bFailed = true;
        }
        UVerticalBox* Actions = Cast<UVerticalBox>(HostScreen->GetWidgetFromName(TEXT("EncounterActions")));
        UButton* Button = Phase == ERunPhase::EncounterChoice ? (Actions ? Cast<UButton>(Actions->GetChildAt(1)) : nullptr) : Cast<UButton>(HostScreen->GetWidgetFromName(TEXT("Button_LeaveShop")));
        if (!Button || !Button->GetIsEnabled()) return false;
        if (Phase == ERunPhase::Shop)
        {
            if (!VerifyShopPresentation(Test, Host, HostScreen, Progress, Run->IsTargetRun(), Run->GetCompletedNodes().Num() + Run->GetTargetRunState().CompletedEncounterChoices.Num())) bFailed = true;
        }
        if (bFailed) return false;
        Button->OnClicked.Broadcast();
        const ERunPhase Expected = Phase == ERunPhase::EncounterChoice ? ERunPhase::Shop : ERunPhase::Map;
        if (!Test->TestTrue(TEXT("Host shop buttons commit the expected phase without changing character ownership."), Run->GetPhase() == Expected && Run->GetEncounterProgress().SelectedEncounterId == TEXT("Shop_02"))) bFailed = true;
        return false;
    }
}
