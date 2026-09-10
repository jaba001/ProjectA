#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Components/Button.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "Kismet/GameplayStatics.h"

class FContinueSavedMenu : public IAutomationLatentCommand
{
public:
    explicit FContinueSavedMenu(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 30.0)
        {
            Test->AddError(TEXT("Packaged Continue timed out."));
            return true;
        }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || !World->IsGameWorld() || !World->GetGameInstance())
            {
                continue;
            }
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, bClicked ? UEncounterResultWidget::StaticClass() : UMainMenuScreenWidget::StaticClass(), false);
            for (UUserWidget* Widget : Widgets)
            {
                UButton* Continue = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_Continue")));
                if (!Continue || !Continue->GetIsEnabled())
                {
                    continue;
                }
                if (!bClicked)
                {
                    bClicked = true;
                    Continue->OnClicked.Broadcast();
                    return false;
                }
                URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
                Test->TestTrue(TEXT("Menu Continue opens saved result"), Run->GetPhase() == ERunPhase::Result);
                if (!Test->TestEqual(TEXT("Restored party count"), Run->GetPartyMembers().Num(), 1))
                {
                    return true;
                }
                Test->TestEqual(TEXT("Menu restores name"), Run->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Restart Scholar")));
                Test->TestEqual(TEXT("Menu restores HP"), Run->GetPartyMembers()[0].CurrentHP, 61.0f);
                Continue->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Result Continue unlocks the next node"), Run->CanStartNode(TEXT("Combat_02")));
                UGameplayStatics::DeleteGameInSlot(TEXT("T11_ProcessRestart"), 0);
                return true;
            }
        }
        return false;
    }

private:
    FAutomationTestBase* Test;
    double Started;
    bool bClicked = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedContinueTest, "ProjectA.Menu.PackagedContinue", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedContinueTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FContinueSavedMenu(this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedMenuQuitTest, "ProjectA.Menu.PackagedQuit", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedMenuQuitTest::RunTest(const FString& Parameters)
{
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (!World || !World->IsGameWorld())
        {
            continue;
        }
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UMainMenuScreenWidget::StaticClass(), false);
        for (UUserWidget* Widget : Widgets)
        {
            if (UButton* Quit = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_Quit"))))
            {
                const TWeakObjectPtr<UButton> WeakQuit = Quit;
                World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakQuit]()
                {
                    if (WeakQuit.IsValid())
                    {
                        UE_LOG(LogTemp, Display, TEXT("[T11] Invoking packaged main-menu Quit button."));
                        WeakQuit->OnClicked.Broadcast();
                    }
                }));
                return true;
            }
        }
    }
    AddError(TEXT("Packaged MainMenu Quit button was not found."));
    return false;
}

#endif
