#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/CombatManager.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "CommonGameViewportClient.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Layout/WidgetPath.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Misc/Paths.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatHUDWidget.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace ProjectAVerticalSliceTests
{
template <typename T>
T* FindActiveWidget(UWorld* World)
{
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        T* TypedWidget = Cast<T>(Widget);
        if (TypedWidget && TypedWidget->IsActivated())
        {
            return TypedWidget;
        }
    }
    return nullptr;
}

// Exercise saved maps, real Slate combat clicks, navigation, GAS and encounter cleanup in PIE.
// PIE에서 저장된 맵, 실제 Slate 전투 클릭, 내비게이션, GAS 및 인카운터 정리를 실행합니다.
class FPlayVerticalSlice : public IAutomationLatentCommand
{
public:
    explicit FPlayVerticalSlice(FAutomationTestBase* InTest) : Test(InTest), StageStarted(FPlatformTime::Seconds())
    {
    }

    virtual ~FPlayVerticalSlice() override
    {
        if (Combat.IsValid())
        {
            Combat->OnCombatResult.RemoveAll(this);
        }
        if (Player.IsValid())
        {
            Player->OnActionCompleted.RemoveAll(this);
        }
        if (InputViewport.IsValid())
        {
            InputViewport->OnInputKey().RemoveAll(this);
        }
    }

    virtual bool Update() override
    {
        double StageTimeout = 60.0;
        if (Stage == 6)
        {
            StageTimeout = 180.0;
        }
        if (FPlatformTime::Seconds() - StageStarted > StageTimeout)
        {
            Test->AddError(FString::Printf(TEXT("Vertical slice PIE timed out at stage %d."), Stage));
            return true;
        }
        if (bPointerClickPending)
        {
            // Mouse-over events cache the clickable primitive during the following player tick.
            // 마우스 오버 이벤트는 다음 플레이어 틱에 클릭 가능한 프리미티브를 캐시합니다.
            if (GFrameCounter <= PointerQueuedFrame || FPlatformTime::Seconds() - PointerQueuedTime < 0.1)
            {
                return false;
            }
            return !CompletePointerClick();
        }
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->GetFirstPlayerController())
        {
            return false;
        }

        if (Stage == 0)
        {
            AMainMenuPlayerController* Menu = Cast<AMainMenuPlayerController>(World->GetFirstPlayerController());
            if (!Menu || !Menu->GetMainMenuRootWidget())
            {
                return false;
            }
            if (!CheckMenuLifecycle(Menu))
            {
                return true;
            }
            Menu->ShowCharacterCreationScreen();
            UOptionsWidget* Options = CreateWidget<UOptionsWidget>(Menu, UOptionsWidget::StaticClass());
            Options->ActivateWidget();
            UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
            const auto PreviousQuality = Settings->ScalabilityQuality;
            const bool bPreviousVSync = Settings->IsVSyncEnabled();
            UComboBoxString* Quality = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("QualitySelect")));
            UCheckBox* VSync = Cast<UCheckBox>(Options->GetWidgetFromName(TEXT("VSyncCheck")));
            Quality->SetSelectedIndex(1);
            VSync->SetIsChecked(!bPreviousVSync);
            Test->TestEqual(TEXT("Options remain unchanged before Apply."), Settings->IsVSyncEnabled(), bPreviousVSync);
            Options->ApplyOptions();
            Settings->LoadSettings(true);
            Test->TestEqual(TEXT("Quality persists after reload."), Settings->GetOverallScalabilityLevel(), 1);
            Test->TestEqual(TEXT("VSync persists after reload."), Settings->IsVSyncEnabled(), !bPreviousVSync);
            Settings->ScalabilityQuality = PreviousQuality;
            Settings->SetVSyncEnabled(bPreviousVSync);
            Settings->ApplySettings(false);
            Options->DeactivateWidget();
            Advance();
            return false;
        }
        if (Stage == 1)
        {
            UCharacterCreationWidget* Creation = FindActiveWidget<UCharacterCreationWidget>(World);
            if (!Creation)
            {
                return false;
            }
            UButton* CreateButton = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Create")));
            if (!Require(CreateButton != nullptr, TEXT("CharacterCreation contains the slot zero create button.")))
            {
                return true;
            }
            if (!bProfessionPanelTested)
            {
                if (PreviewCaptureStage < 3)
                {
                    if (PreviewCaptureStage == 0)
                    {
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index))))->OnClicked.Broadcast();
                        }
                        PreviewCaptureStage = 1;
                        ProfessionPanelTime = FPlatformTime::Seconds();
                        return false;
                    }
                    if (FPlatformTime::Seconds() - ProfessionPanelTime < 0.5)
                    {
                        return false;
                    }
                    if (PreviewCaptureStage == 1)
                    {
                        const float ScreenBottom = Creation->GetCachedGeometry().LocalToAbsolute(Creation->GetCachedGeometry().GetLocalSize()).Y;
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            UWidget* Info = Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_ClassInfo"), Index)));
                            const FGeometry& Geometry = Info->GetCachedGeometry();
                            Test->TestTrue(TEXT("Every ClassInfo button fits inside the creation screen."), Geometry.LocalToAbsolute(Geometry.GetLocalSize()).Y <= ScreenBottom + 1.0f);
                        }
                        Capture(TEXT("00-FourPreviews.png"));
                        PreviewCaptureStage = 2;
                        ProfessionPanelTime = FPlatformTime::Seconds();
                        return false;
                    }
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        Cast<UButton>(Creation->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Delete"), Index))))->OnClicked.Broadcast();
                    }
                    PreviewCaptureStage = 3;
                }
                CreateButton->OnClicked.Broadcast();
                UButton* Edit = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_Edit")));
                UButton* Info = Cast<UButton>(Creation->GetWidgetFromName(TEXT("Button_Slot0_ClassInfo")));
                UEditableTextBox* NameInput = Cast<UEditableTextBox>(Creation->GetWidgetFromName(TEXT("ProfessionNameInput")));
                UComboBoxString* ClassSelect = Cast<UComboBoxString>(Creation->GetWidgetFromName(TEXT("ProfessionClassSelect")));
                if (!Require(Edit && Info && NameInput && ClassSelect, TEXT("Slot detail controls exist.")))
                {
                    return true;
                }
                Edit->OnClicked.Broadcast();
                NameInput->SetText(FText::FromString(TEXT("   ")));
                Creation->SaveSlotDetails();
                Test->TestFalse(TEXT("Blank name cannot be saved."), Creation->GetPartyMembers()[0].CharacterName.ToString().TrimStartAndEnd().IsEmpty());
                NameInput->SetText(FText::FromString(TEXT("Vertical Slice Hero")));
                ClassSelect->SetSelectedIndex(3);
                Creation->SaveSlotDetails();
                Test->TestEqual(TEXT("Edited class is stored in slot zero."), Creation->GetPartyMembers()[0].ClassId, FName(TEXT("Hunter")));
                Edit->OnClicked.Broadcast();
                NameInput->SetText(FText::FromString(TEXT("Discard this name")));
                ClassSelect->SetSelectedIndex(1);
                Creation->CloseSlotDetails();
                Test->TestEqual(TEXT("Cancel preserves the saved class."), Creation->GetPartyMembers()[0].ClassId, FName(TEXT("Hunter")));
                Test->TestEqual(TEXT("Cancel preserves the saved name."), Creation->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Vertical Slice Hero")));
                Info->OnClicked.Broadcast();
                UTextBlock* Details = Cast<UTextBlock>(Creation->GetWidgetFromName(TEXT("ProfessionDetailText")));
                Test->TestEqual(TEXT("ClassInfo uses the shared catalog."), Details->GetText().ToString(), Creation->PartyDefinition->GetProfessionDetails(TEXT("Hunter")).ToString());

                bProfessionPanelTested = true;
                ProfessionPanelTime = FPlatformTime::Seconds();
                return false;
            }
            if (FPlatformTime::Seconds() - ProfessionPanelTime < 0.5)
            {
                return false;
            }
            if (!bProfessionCaptured)
            {
                Capture(TEXT("00-ProfessionDetails.png"));
                bProfessionCaptured = true;
                ProfessionPanelTime = FPlatformTime::Seconds();
                return false;
            }
            Creation->CloseSlotDetails();
            const TArray<FRunPartyMember> Members = Creation->GetPartyMembers();
            if (!Require(Members.Num() == 4 && Members[0].bCreated && !Members[1].bCreated && !Members[2].bCreated && !Members[3].bCreated, TEXT("Character creation exports one created member and three empty slots.")))
            {
                return true;
            }
            Creation->RequestStartGame();
            Advance();
            return false;
        }

        AGameplayGameModeBase* Mode = Cast<AGameplayGameModeBase>(World->GetAuthGameMode());
        AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
        URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        if (!Mode || !Controller || !Run || !Mode->GetEncounterManager())
        {
            return false;
        }
        AEncounterManager* Encounter = Mode->GetEncounterManager();

        if (Stage == 2)
        {
            URunMapWidget* MapWidget = FindActiveWidget<URunMapWidget>(World);
            if (Run->GetPhase() != ERunPhase::Map || !MapWidget || FPlatformTime::Seconds() - StageStarted < 1.0)
            {
                return false;
            }
            if (!Require(World->GetMapName().Contains(TEXT("Gameplay")), TEXT("Start Game traveled to Gameplay.")))
            {
                return true;
            }
            if (!bMapCaptured)
            {
                Capture(TEXT("01-RunMap.png"));
                bMapCaptured = true;
                StageStarted = FPlatformTime::Seconds();
                return false;
            }
            InputViewport = Cast<UCommonGameViewportClient>(World->GetGameViewport());
            if (!Require(InputViewport.IsValid(), TEXT("PIE uses the actual CommonUI game viewport.")) || !CheckInputPolicy(Controller, false))
            {
                return true;
            }
            InputViewport->OnInputKey().AddRaw(this, &FPlayVerticalSlice::HandleGameViewportInput);
            Test->TestEqual(TEXT("Edited profession survives travel."), Run->GetPartyMembers()[0].ClassId, FName(TEXT("Hunter")));
            Test->TestEqual(TEXT("Party name survives travel."), Run->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Vertical Slice Hero")));
            GameplayWorld = World;
            Combat = Encounter->GetCombatManager();
            if (!Require(Combat.IsValid(), TEXT("Gameplay owns a combat manager.")))
            {
                return true;
            }
            Combat->OnCombatResult.AddRaw(this, &FPlayVerticalSlice::HandleResult);
            if (!ClickNode(MapWidget, 0))
            {
                return true;
            }
            Advance();
            return false;
        }
        if (Stage == 3 || Stage == 9)
        {
            if (Run->GetPhase() != ERunPhase::Combat || FPlatformTime::Seconds() - StageStarted < 1.0)
            {
                return false;
            }
            if (Stage == 3 && PointerStep == 1)
            {
                if (!Require(Controller->IsMoveInputMode(), TEXT("A real Slate click on HUD Move enters move mode.")) || !QueueTileClick(Controller, MoveTile.Get()))
                {
                    return true;
                }
                PointerStep = 2;
                return false;
            }
            if (Stage == 3 && PointerStep == 2)
            {
                if (!Require(GameMouseDownCount == GameMouseDownBeforeTile + 1, TEXT("The first tile mouse-down passes CommonUI to the game viewport.")) || !Require(Controller->GetSelectedTile() == MoveTile.Get() && !Controller->IsMoveInputMode(), TEXT("The player controller dispatches the real click to the move tile.")) || !Require(Player->IsBusy() || Player->GetCurrentTile() == MoveTile.Get(), TEXT("The world tile click starts the movement action.")))
                {
                    return true;
                }
                Advance();
                return false;
            }
            if (!CheckInputPolicy(Controller, true))
            {
                return true;
            }
            if (!Require(Encounter->GetSpawnedUnits().Num() == 2 && Combat->GetRegisteredUnits().Num() == 2, TEXT("Only one created party member and one enemy spawn/register.")))
            {
                return true;
            }
            Player = nullptr;
            Enemy = nullptr;
            for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
            {
                if (Unit->GetTeam() == ETeam::Player)
                {
                    Player = Unit;
                }
                else
                {
                    Enemy = Unit;
                }
                FirstEncounterUnits.Add(Unit);
            }
            if (!Require(Player.IsValid() && Enemy.IsValid(), TEXT("Both encounter teams have actors.")))
            {
                return true;
            }
            FProfessionDefinition SpawnDefinition;
            if (!Require(Run->PartyDefinition && Run->PartyDefinition->ResolveProfession(Run->GetPartyMembers()[0].ClassId, SpawnDefinition), TEXT("Travel retains the exact profession catalog.")))
            {
                return true;
            }
            Test->TestEqual(TEXT("Spawned max HP matches detail preview."), Player->GetAttributeSet()->GetMaxHP(), SpawnDefinition.MaxHP);
            Test->TestEqual(TEXT("Spawned AP matches detail preview."), Player->GetMaxActionPoint(), SpawnDefinition.ActionPoints);
            Test->TestEqual(TEXT("Spawned sub AP matches detail preview."), Player->GetMaxSubActionPoint(), SpawnDefinition.SubActionPoints);
            Test->TestEqual(TEXT("Spawned name matches edited slot."), Player->RuntimeCharacterName.ToString(), FString(TEXT("Vertical Slice Hero")));
            if (Stage == 9)
            {
                Test->TestTrue(TEXT("The second encounter uses the same persistent world."), GameplayWorld.Get() == World);
                Test->TestEqual(TEXT("Next encounter restores checkpoint HP."), Player->GetAttributeSet()->GetHP(), Run->GetPartyMembers()[0].CurrentHP);
                if (!Require(UCombatEffectLibrary::ApplyDamageToUnit(Enemy.Get(), Player.Get(), UGE_Damage::StaticClass(), 100000.0f), TEXT("GAS lethal damage applied to the party for Defeat coverage.")))
                {
                    return true;
                }
                Advance();
                return false;
            }
            for (TActorIterator<ACombatGridManager> It(World); It; ++It)
            {
                Grid = *It;
                break;
            }
            if (!Require(Grid.IsValid() && Grid->TileMap.Num() == 16, TEXT("The runtime grid contains sixteen tiles.")))
            {
                return true;
            }
            Test->TestFalse(TEXT("Run Map deactivates for combat."), FindActiveWidget<URunMapWidget>(World) != nullptr);
            Capture(TEXT("02-Combat.png"));
            MoveTile = Grid->GetTileAtCoord(FIntPoint(1, 1));
            UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
            if (!Require(Navigation != nullptr, TEXT("Gameplay creates a navigation system.")))
            {
                return true;
            }
            UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Player->GetActorLocation(), MoveTile->GetActorLocation(), Player.Get());
            UE_LOG(LogTemp, Display, TEXT("[VerticalSliceNavigation] System=%s DefaultData=%s Building=%d Locked=%d PathValid=%d Player=%s Goal=%s"), *GetNameSafe(Navigation), *GetNameSafe(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)), UNavigationSystemV1::IsNavigationBeingBuilt(World), UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World), Path && Path->IsValid(), *Player->GetActorLocation().ToString(), *MoveTile->GetActorLocation().ToString());
            UCombatHUDWidget* HUD = FindActiveWidget<UCombatHUDWidget>(World);
            UButton* MoveButton = nullptr;
            if (HUD)
            {
                MoveButton = Cast<UButton>(HUD->GetWidgetFromName(TEXT("Button_Move")));
            }
            if (!QueueWidgetClick(MoveButton))
            {
                return true;
            }
            PointerStep = 1;
            return false;
        }
        if (Stage == 4)
        {
            if (PointerStep == 1)
            {
                if (!Require(Controller->IsSkillInputMode() && Controller->GetPendingSkillData() == Player->FindSkillDataByAbilityClass(Player->GetDefaultAttackAbilityClass()), TEXT("A real Slate click on the HUD skill selects the configured basic attack.")) || !QueueTileClick(Controller, Enemy->GetCurrentTile()))
                {
                    return true;
                }
                PointerStep = 2;
                return false;
            }
            if (PointerStep == 2)
            {
                if (!Require(GameMouseDownCount == GameMouseDownBeforeTile + 1, TEXT("The skill target mouse-down passes CommonUI to the game viewport.")) || !Require(!Controller->IsSkillInputMode() && Player->IsBusy(), TEXT("The actual enemy tile click consumes skill selection and starts the approach action.")))
                {
                    return true;
                }
                Advance();
                return false;
            }
            if (!Player.IsValid() || Player->IsBusy())
            {
                return false;
            }
            if (!Require(Player->GetCurrentTile() == MoveTile.Get() && MoveTile->GetOccupyingUnit() == Player.Get(), TEXT("Existing movement updates tile ownership.")))
            {
                return true;
            }
            Test->TestTrue(TEXT("Movement snaps the actor to its owned tile."), FVector::DistSquared2D(Player->GetActorLocation(), MoveTile->GetActorLocation()) < 1.0f);
            EnemyHPBeforeSkill = Enemy->GetAttributeSet()->GetHP();
            UCombatHUDWidget* HUD = FindActiveWidget<UCombatHUDWidget>(World);
            UHorizontalBox* Skills = nullptr;
            if (HUD)
            {
                Skills = Cast<UHorizontalBox>(HUD->GetWidgetFromName(TEXT("SkillList")));
            }
            UButton* SkillButton = nullptr;
            if (Skills && Skills->GetChildrenCount() > 0)
            {
                SkillButton = Cast<UButton>(Skills->GetChildAt(0));
            }
            if (SkillButton)
            {
                const USkillDefinitionDataAsset* Skill = Player->FindSkillDataByAbilityClass(Player->GetDefaultAttackAbilityClass());
                const UTextBlock* Label = Cast<UTextBlock>(SkillButton->GetContent());
                if (!Require(Skill && Label && Label->GetText().ToString().Contains(Skill->GetActionPointCostText().ToString()), TEXT("The real HUD displays the selected definition's AP cost.")))
                {
                    return true;
                }
                Test->TestEqual(TEXT("HUD skill availability matches the definition cost."), SkillButton->GetIsEnabled(), Controller->CanUseActiveUnitActionPoint(Skill->ActionPointCost));
            }
            if (!QueueWidgetClick(SkillButton))
            {
                return true;
            }
            PointerStep = 1;
            return false;
        }
        if (Stage == 5)
        {
            if (!Player.IsValid() || Player->IsBusy() || FPlatformTime::Seconds() - StageStarted < 0.1)
            {
                return false;
            }
            if (!Require(Enemy.IsValid() && Enemy->GetAttributeSet()->GetHP() < EnemyHPBeforeSkill, TEXT("Existing approach skill deals GAS damage and completes.")))
            {
                return true;
            }
            Test->TestTrue(TEXT("Skill return restores actor and occupied tile."), Player->GetCurrentTile() == MoveTile.Get() && MoveTile->GetOccupyingUnit() == Player.Get() && FVector::DistSquared2D(Player->GetActorLocation(), MoveTile->GetActorLocation()) < 1.0f);
            PlayerHPBeforeAI = Player->GetAttributeSet()->GetHP();
            TurnBeforeAI = Combat->GetTurnManager()->GetTurnCounter();
            Controller->RequestEndTurn();
            Advance();
            return false;
        }
        if (Stage == 6)
        {
            if (Run->GetPhase() == ERunPhase::Result && Run->GetLastResult() == ECombatResult::Victory)
            {
                Advance();
                return false;
            }
            if (!Require(Run->GetPhase() != ERunPhase::Defeat, TEXT("Unmodified party and skill content can reach natural Victory.")))
            {
                return true;
            }
            if (!Player.IsValid() || Combat->GetCurrentUnit() != Player.Get())
            {
                return false;
            }
            if (bStationaryPending)
            {
                if (Player->IsBusy())
                {
                    Test->TestFalse(TEXT("Stationary montage blocks additional player actions."), Controller->CanUseActiveUnitAction());
                    return false;
                }
                Test->TestEqual(TEXT("Stationary montage completes exactly once."), StationaryCompletionCount, 1);
                Test->TestTrue(TEXT("Stationary montage completes successfully without movement."), bStationarySucceeded && Player->GetActorLocation().Equals(StationaryLocation, 1.0f));
                Player->OnActionCompleted.RemoveAll(this);
                StationarySkill.Reset();
                bStationaryPending = false;
                bStationaryVerified = true;
            }
            if (Player->IsBusy())
            {
                return false;
            }
            if (!bEnemyTurnVerified)
            {
                if (!Require(Combat->GetTurnManager()->GetTurnCounter() > TurnBeforeAI && Player->GetAttributeSet()->GetHP() < PlayerHPBeforeAI, TEXT("Enemy AI deals damage and returns control to the player.")))
                {
                    return true;
                }
                bEnemyTurnVerified = true;
            }
            if (Player->HasEnoughActionPoint(1) && Enemy.IsValid())
            {
                USkillDefinitionDataAsset* Skill = Player->FindSkillDataByAbilityClass(Player->GetDefaultAttackAbilityClass());
                if (!Require(Skill != nullptr, TEXT("Player retains its configured basic skill definition.")))
                {
                    return true;
                }
                if (!bStationaryVerified)
                {
                    // Change only a transient copy to cover the existing montage as a stationary skill.
                    // 기존 몽타주의 제자리 스킬 경로를 검증하도록 임시 복사본만 변경합니다.
                    StationarySkill.Reset(DuplicateObject<USkillDefinitionDataAsset>(Skill, Player.Get()));
                    StationarySkill->bMoveToTarget = false;
                    StationaryLocation = Player->GetActorLocation();
                    bStationaryPending = true;
                    Player->OnActionCompleted.AddRaw(this, &FPlayVerticalSlice::HandleStationaryCompleted);
                    Player->StartSkill(StationarySkill.Get(), Enemy->GetCurrentTile());
                    Test->TestTrue(TEXT("Stationary montage enters the action busy state."), Player->IsBusy());
                }
                else
                {
                    Player->StartSkill(Skill, Enemy->GetCurrentTile());
                }
            }
            else
            {
                Controller->RequestEndTurn();
            }
            return false;
        }
        if (Stage == 7 || Stage == 10)
        {
            UEncounterResultWidget* Result = FindActiveWidget<UEncounterResultWidget>(World);
            const bool bDefeat = Stage == 10;
            ERunPhase ExpectedPhase = ERunPhase::Result;
            ECombatResult ExpectedResult = ECombatResult::Victory;
            int32 ExpectedCount = 1;
            if (bDefeat)
            {
                ExpectedPhase = ERunPhase::Defeat;
                ExpectedResult = ECombatResult::Defeat;
                ExpectedCount = 2;
            }
            if (Run->GetPhase() != ExpectedPhase || !Result)
            {
                return false;
            }
            if (FPlatformTime::Seconds() - StageStarted < 1.0)
            {
                return false;
            }
            if ((!bDefeat && !bVictoryCaptured) || (bDefeat && !bDefeatCaptured))
            {
                if (bDefeat)
                {
                    Capture(TEXT("04-Defeat.png"));
                    bDefeatCaptured = true;
                }
                else
                {
                    Capture(TEXT("03-Victory.png"));
                    bVictoryCaptured = true;
                }
                StageStarted = FPlatformTime::Seconds();
                return false;
            }
            if (FPlatformTime::Seconds() - StageStarted < 0.25)
            {
                return false;
            }
            Test->TestTrue(TEXT("The run receives the expected result."), Run->GetLastResult() == ExpectedResult);
            Test->TestTrue(TEXT("The settled combat result is saved successfully."), Run->GetSaveError().IsEmpty());
            if (!CheckInputPolicy(Controller, false))
            {
                return true;
            }
            Test->TestEqual(TEXT("Each combat broadcasts its result once."), ResultCount, ExpectedCount);
            CheckCleanup(Encounter);
            Test->TestFalse(TEXT("Result screen blocks further player actions."), Controller->CanUseActiveUnitAction());
            const int32 FinishedTurn = Combat->GetCurrentTurnIndex();
            Controller->RequestEndTurn();
            Test->TestEqual(TEXT("Player input cannot advance finished combat."), Combat->GetCurrentTurnIndex(), FinishedTurn);
            if (bDefeat)
            {
                Test->TestFalse(TEXT("Defeat cannot continue the run."), Encounter->ContinueRun());
                Advance();
                return false;
            }
            UButton* ContinueButton = Cast<UButton>(Result->GetWidgetFromName(TEXT("Button_Continue")));
            if (!Require(ContinueButton && ContinueButton->GetIsEnabled(), TEXT("Victory exposes an enabled Continue button.")))
            {
                return true;
            }
            ContinueButton->OnClicked.Broadcast();
            Advance();
            return false;
        }
        if (Stage == 8)
        {
            URunMapWidget* MapWidget = FindActiveWidget<URunMapWidget>(World);
            if (Run->GetPhase() != ERunPhase::Map || !MapWidget)
            {
                return false;
            }
            CheckCleanup(Encounter);
            Test->TestEqual(TEXT("Victory completes one node."), Run->GetCompletedNodes().Num(), 1);
            if (!ClickNode(MapWidget, 1))
            {
                return true;
            }
            Advance();
            return false;
        }
        if (Stage == 11 && FPlatformTime::Seconds() - StageStarted > 0.25)
        {
            Test->TestEqual(TEXT("No duplicate result is broadcast after Defeat settles."), ResultCount, 2);
            Test->TestTrue(TEXT("Defeat remains terminal."), Run->GetPhase() == ERunPhase::Defeat && !Combat->IsCombatActive());
            return true;
        }
        return false;
    }

private:
    bool Require(bool bCondition, const TCHAR* Message)
    {
        return Test->TestTrue(Message, bCondition);
    }

    void Advance()
    {
        ++Stage;
        PointerStep = 0;
        StageStarted = FPlatformTime::Seconds();
        Test->AddInfo(FString::Printf(TEXT("Vertical slice PIE stage %d."), Stage));
    }

    bool CheckMenuLifecycle(AMainMenuPlayerController* Menu)
    {
        AMainMenuPreviewStage* Preview = Menu->GetPreviewStage();
        Test->TestNull(TEXT("MainMenu does not spawn an extra combat pawn."), Menu->GetPawn());
        if (!Require(Preview && Menu->GetViewTarget() == Preview, TEXT("Saved MainMenu has an active preview camera.")))
        {
            return false;
        }
        UMainMenuRootWidget* NativeRoot = CreateWidget<UMainMenuRootWidget>(Menu, UMainMenuRootWidget::StaticClass());
        NativeRoot->AddToViewport();
        for (const TCHAR* StackName : { TEXT("MainStack"), TEXT("MenuStack"), TEXT("ModalStack") })
        {
            Test->TestNotNull(TEXT("Native root supplies each stack."), NativeRoot->GetWidgetFromName(StackName));
        }
        Test->TestNotNull(TEXT("Native menu pushes to stack."), NativeRoot->PushMainScreen(UMainMenuScreenWidget::StaticClass()));
        UClass* Designer = LoadClass<UCharacterCreationWidget>(nullptr, TEXT("/Game/User_JeHoon/UI/MainMenu/WBP_CharacterCreationWidget.WBP_CharacterCreationWidget_C"));
        for (UClass* WidgetClass : { UCharacterCreationWidget::StaticClass(), Designer })
        {
            UCharacterCreationWidget* Draft = CreateWidget<UCharacterCreationWidget>(Menu, WidgetClass);
            if (!Require(Draft != nullptr, TEXT("Native and Designer creation widgets instantiate.")))
            {
                return false;
            }
            Draft->AddToViewport();
            for (int32 Cycle = 0; Cycle < 2; ++Cycle)
            {
                Draft->ActivateWidget();
                for (const FRunPartyMember& Member : Draft->GetPartyMembers())
                {
                    Test->TestFalse(TEXT("Every new visit starts with an empty draft."), Member.bCreated);
                }
                TArray<TWeakObjectPtr<AActor>> PreviousPreviews;
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    UButton* Create = Cast<UButton>(Draft->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index))));
                    if (!Require(Create != nullptr, TEXT("All four creation controls are bound.")))
                    {
                        return false;
                    }
                    Create->OnClicked.Broadcast();
                    AActor* Actor = Preview->GetPreviewActorForSlot(Index);
                    Test->TestNotNull(TEXT("Each profession has a visible preview class."), Actor);
                    Test->TestNull(TEXT("Preview cannot execute pawn AI or combat."), Cast<APawn>(Actor));
                    PreviousPreviews.Add(Actor);
                }
                if (Cycle == 0)
                {
                    UButton* Close = Cast<UButton>(Draft->GetWidgetFromName(TEXT("Button_Close")));
                    if (!Require(Close != nullptr, TEXT("Close button exists.")))
                    {
                        return false;
                    }
                    Close->OnClicked.Broadcast();
                }
                else
                {
                    Draft->RequestBack();
                }
                Test->TestFalse(TEXT("Back and X deactivate the creation screen."), Draft->IsActivated());
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    Test->TestNull(TEXT("Closing releases every preview slot."), Preview->GetPreviewActorForSlot(Index));
                    Test->TestFalse(TEXT("Closing destroys the previous preview actor."), PreviousPreviews[Index].IsValid());
                }
                Test->TestTrue(TEXT("Back keeps the menu preview camera."), Menu->GetViewTarget() == Preview);
            }
            Draft->RemoveFromParent();
        }
        Preview->SetPreviewActorForSlot(0, TEXT("Hunter"));
        Preview->SetPreviewActorForSlot(0, TEXT("MissingProfession"));
        Test->TestNull(TEXT("Missing preview class clears the old actor."), Preview->GetPreviewActorForSlot(0));
        NativeRoot->RemoveFromParent();
        return true;
    }

    void HandleResult(ECombatResult Result)
    {
        ++ResultCount;
    }

    void HandleStationaryCompleted(AUnitBase* Unit, EUnitActionType ActionType, EUnitActionResult Result)
    {
        ++StationaryCompletionCount;
        bStationarySucceeded = Result == EUnitActionResult::Succeeded;
    }

    void Capture(const TCHAR* FileName)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VerticalSliceScreenshots"), FileName), true, false);
    }

    bool CheckInputPolicy(AGameplayPlayerController* Controller, bool bCombat)
    {
        ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
        UCommonUIActionRouterBase* Router = nullptr;
        if (LocalPlayer)
        {
            Router = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>();
        }
        if (!Require(Router && InputViewport.IsValid(), TEXT("The local player owns the CommonUI action router and viewport.")))
        {
            return false;
        }
        UCommonActivatableWidget* ActiveScreen = FindActiveWidget<URunMapWidget>(Controller->GetWorld());
        if (!ActiveScreen)
        {
            ActiveScreen = FindActiveWidget<UCombatHUDWidget>(Controller->GetWorld());
        }
        if (!ActiveScreen)
        {
            ActiveScreen = FindActiveWidget<UEncounterResultWidget>(Controller->GetWorld());
        }
        const ERouteUIInputResult ActualRoute = Router->ProcessInput(EKeys::LeftMouseButton, IE_Pressed);
        Router->ProcessInput(EKeys::LeftMouseButton, IE_Released);
        UE_LOG(LogTemp, Display, TEXT("[VerticalSliceInput] Stage=%d Screen=%s PendingTree=%d SupportsFocus=%d Mode=%d Capture=%d IgnoreInput=%d CanProcessGame=%d MouseRoute=%d"), Stage, *GetNameSafe(ActiveScreen), Router->IsPendingTreeChange(), ActiveScreen && ActiveScreen->SupportsActivationFocus(), static_cast<int32>(Router->GetActiveInputMode()), static_cast<int32>(InputViewport->GetMouseCaptureMode()), InputViewport->IgnoreInput(), Router->CanProcessNormalGameInput(), static_cast<int32>(ActualRoute));
        if (!Require(!InputViewport->IgnoreInput(), TEXT("Gameplay clears the UIOnly viewport input gate inherited from MainMenu.")))
        {
            return false;
        }
        ECommonInputMode ExpectedMode = ECommonInputMode::Menu;
        ERouteUIInputResult ExpectedRoute = ERouteUIInputResult::BlockGameInput;
        EMouseCaptureMode ExpectedCapture = EMouseCaptureMode::NoCapture;
        if (bCombat)
        {
            ExpectedMode = ECommonInputMode::All;
            ExpectedRoute = ERouteUIInputResult::Unhandled;
            ExpectedCapture = EMouseCaptureMode::CaptureDuringMouseDown;
        }
        bool bPassed = Require(Router->GetActiveInputMode() == ExpectedMode, TEXT("The active CommonUI mode matches the current run phase."));
        bPassed &= Require(Router->CanProcessNormalGameInput() == bCombat, TEXT("CommonUI permits world input only during combat."));
        bPassed &= Require(ActualRoute == ExpectedRoute, TEXT("CommonUI routes or blocks actual mouse input for the current phase."));
        bPassed &= Require(InputViewport->GetMouseCaptureMode() == ExpectedCapture, TEXT("Combat forwards the first mouse-down and menu screens release capture."));
        return bPassed;
    }

    bool QueueWidgetClick(UButton* Button)
    {
        if (!Require(Button && Button->GetIsEnabled() && Button->GetCachedWidget().IsValid(), TEXT("The combat HUD exposes an enabled, painted action button.")))
        {
            return false;
        }
        const FGeometry& Geometry = Button->GetCachedGeometry();
        if (!Require(Geometry.GetLocalSize().X > 0.0f && Geometry.GetLocalSize().Y > 0.0f, TEXT("The action button has a clickable Slate geometry.")))
        {
            return false;
        }
        return QueuePointerClick(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f), Button->GetCachedWidget());
    }

    bool QueueTileClick(AGameplayPlayerController* Controller, ACombatGridTile* Tile)
    {
        FSceneViewport* Viewport = nullptr;
        TSharedPtr<SViewport> ViewportWidget;
        if (InputViewport.IsValid())
        {
            Viewport = InputViewport->GetGameViewport();
            ViewportWidget = InputViewport->GetGameViewportWidget();
        }
        if (!Require(Viewport && ViewportWidget.IsValid() && Tile, TEXT("The target tile and actual PIE Slate viewport are available.")))
        {
            return false;
        }
        const FIntPoint ViewportSize = Viewport->GetSizeXY();
        if (!Require(ViewportSize.X > 0 && ViewportSize.Y > 0, TEXT("The PIE viewport has rendered dimensions.")))
        {
            return false;
        }
        // Pick an exposed tile surface, avoiding unit capsules and the HUD without changing collision.
        // 충돌 설정을 변경하지 않고 유닛 캡슐과 HUD를 피한 노출된 타일 표면을 선택합니다.
        const FVector Offsets[] = {FVector(0.0f, 0.0f, 10.0f), FVector(65.0f, 0.0f, 10.0f), FVector(-65.0f, 0.0f, 10.0f), FVector(0.0f, -65.0f, 10.0f), FVector(65.0f, -65.0f, 10.0f), FVector(-65.0f, -65.0f, 10.0f), FVector(0.0f, 65.0f, 10.0f)};
        FSlateApplication& Slate = FSlateApplication::Get();
        for (const FVector& Offset : Offsets)
        {
            FVector2D Pixel;
            if (!Controller->ProjectWorldLocationToScreen(Tile->GetActorLocation() + Offset, Pixel) || Pixel.X < 0.0f || Pixel.Y < 0.0f || Pixel.X >= ViewportSize.X || Pixel.Y >= ViewportSize.Y)
            {
                continue;
            }
            FHitResult Hit;
            if (!Controller->GetHitResultAtScreenPosition(Pixel, ECC_Visibility, true, Hit) || Hit.GetActor() != Tile)
            {
                continue;
            }
            const FGeometry& Geometry = Viewport->GetCachedGeometry();
            const FVector2D Position = Geometry.LocalToAbsolute(FVector2D(Pixel.X / ViewportSize.X, Pixel.Y / ViewportSize.Y) * Geometry.GetLocalSize());
            const FWidgetPath Path = Slate.LocateWindowUnderMouse(Position, Slate.GetInteractiveTopLevelWindows(), false, Slate.GetUserIndexForMouse());
            if (!Path.IsValid() || Path.GetLastWidget() != ViewportWidget.ToSharedRef())
            {
                continue;
            }
            Viewport->SetMouse(FMath::RoundToInt(Pixel.X), FMath::RoundToInt(Pixel.Y));
            GameMouseDownBeforeTile = GameMouseDownCount;
            return QueuePointerClick(Slate.GetCursorPos(), ViewportWidget);
        }
        return Require(false, TEXT("A visible tile surface reaches the game viewport through the real Slate hit-test path."));
    }

    bool QueuePointerClick(const FVector2D& Position, const TSharedPtr<SWidget>& ExpectedWidget)
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        const FWidgetPath Path = Slate.LocateWindowUnderMouse(Position, Slate.GetInteractiveTopLevelWindows(), false, Slate.GetUserIndexForMouse());
        if (!Require(ExpectedWidget.IsValid() && Path.IsValid() && Path.ContainsWidget(ExpectedWidget.Get()), TEXT("The pointer hit-test path contains the requested gameplay widget.")))
        {
            return false;
        }
        PointerWindow = Path.GetWindow();
        PointerWidget = ExpectedWidget;
        PointerPosition = Position;
        PointerQueuedFrame = GFrameCounter;
        PointerQueuedTime = FPlatformTime::Seconds();
        bPointerClickPending = true;
        const FVector2D PreviousPosition = Slate.GetCursorPos();
        Slate.SetCursorPos(Position);
        const TSet<FKey> NoButtons;
        const FPointerEvent Move(Slate.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Position, PreviousPosition, NoButtons, EKeys::Invalid, 0.0f, Slate.GetModifierKeys());
        Slate.ProcessMouseMoveEvent(Move);
        return true;
    }

    bool CompletePointerClick()
    {
        bPointerClickPending = false;
        FSlateApplication& Slate = FSlateApplication::Get();
        TSharedPtr<SWindow> Window = PointerWindow.Pin();
        TSharedPtr<SWidget> Widget = PointerWidget.Pin();
        const FWidgetPath Path = Slate.LocateWindowUnderMouse(PointerPosition, Slate.GetInteractiveTopLevelWindows(), false, Slate.GetUserIndexForMouse());
        if (!Require(Window.IsValid() && Widget.IsValid() && Path.IsValid() && Path.ContainsWidget(Widget.Get()), TEXT("The real pointer target remains hit-testable after its hover tick.")))
        {
            return false;
        }
        // Use Slate input dispatch so CommonUI, widget hit testing and actor click handling all participate.
        // CommonUI, 위젯 히트 테스트 및 액터 클릭 처리가 모두 참여하도록 Slate 입력을 전달합니다.
        const TSet<FKey> PressedButtons = {EKeys::LeftMouseButton};
        const TSet<FKey> NoButtons;
        const FPointerEvent Down(Slate.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, PointerPosition, PointerPosition, PressedButtons, EKeys::LeftMouseButton, 0.0f, Slate.GetModifierKeys());
        Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), Down);
        const FPointerEvent Up(Slate.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, PointerPosition, PointerPosition, NoButtons, EKeys::LeftMouseButton, 0.0f, Slate.GetModifierKeys());
        Slate.ProcessMouseButtonUpEvent(Up);
        return true;
    }

    void HandleGameViewportInput(const FInputKeyEventArgs& Event)
    {
        if (Event.Key == EKeys::LeftMouseButton && Event.Event == IE_Pressed)
        {
            ++GameMouseDownCount;
        }
    }

    bool ClickNode(URunMapWidget* Widget, int32 Index)
    {
        UVerticalBox* Nodes = Cast<UVerticalBox>(Widget->GetWidgetFromName(TEXT("NodeList")));
        if (!Require(Nodes && Nodes->GetChildrenCount() > Index, TEXT("Run Map displays the configured node list.")))
        {
            return false;
        }
        UButton* Button = Cast<UButton>(Nodes->GetChildAt(Index));
        if (!Require(Button && Button->GetIsEnabled(), TEXT("The next combat node is selectable.")))
        {
            return false;
        }
        Button->OnClicked.Broadcast();
        return true;
    }

    void CheckCleanup(AEncounterManager* Encounter)
    {
        Test->TestTrue(TEXT("Encounter releases spawned actors."), Encounter->GetSpawnedUnits().IsEmpty());
        Test->TestTrue(TEXT("Combat registration is empty after cleanup."), Combat->GetRegisteredUnits().IsEmpty());
        Test->TestTrue(TEXT("Turn registration is empty or released after cleanup."), !Combat->GetTurnManager() || Combat->GetTurnManager()->GetRegisteredUnitCount() == 0);
        for (const TWeakObjectPtr<AUnitBase>& PreviousUnit : FirstEncounterUnits)
        {
            Test->TestFalse(TEXT("Previous encounter units are destroyed."), PreviousUnit.IsValid());
        }
        FirstEncounterUnits.Reset();
        if (Grid.IsValid())
        {
            for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Grid->TileMap)
            {
                Test->TestNull(TEXT("Tiles have no leftover occupant."), Entry.Value->GetOccupyingUnit());
            }
        }
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    int32 ResultCount = 0;
    int32 TurnBeforeAI = 0;
    int32 PointerStep = 0;
    int32 GameMouseDownCount = 0;
    int32 GameMouseDownBeforeTile = 0;
    double StageStarted;
    double PointerQueuedTime = 0.0;
    uint64 PointerQueuedFrame = 0;
    float EnemyHPBeforeSkill = 0.0f;
    float PlayerHPBeforeAI = 0.0f;
    bool bProfessionPanelTested = false;
    int32 PreviewCaptureStage = 0;
    bool bProfessionCaptured = false;
    double ProfessionPanelTime = 0.0;
    bool bMapCaptured = false;
    bool bVictoryCaptured = false;
    bool bDefeatCaptured = false;
    bool bEnemyTurnVerified = false;
    bool bStationaryPending = false;
    bool bStationaryVerified = false;
    bool bStationarySucceeded = false;
    bool bPointerClickPending = false;
    int32 StationaryCompletionCount = 0;
    FVector StationaryLocation = FVector::ZeroVector;
    FVector2D PointerPosition = FVector2D::ZeroVector;
    TWeakPtr<SWindow> PointerWindow;
    TWeakPtr<SWidget> PointerWidget;
    TWeakObjectPtr<UCommonGameViewportClient> InputViewport;
    TStrongObjectPtr<USkillDefinitionDataAsset> StationarySkill;
    TWeakObjectPtr<UWorld> GameplayWorld;
    TWeakObjectPtr<ACombatManager> Combat;
    TWeakObjectPtr<ACombatGridManager> Grid;
    TWeakObjectPtr<ACombatGridTile> MoveTile;
    TWeakObjectPtr<AUnitBase> Player;
    TWeakObjectPtr<AUnitBase> Enemy;
    TArray<TWeakObjectPtr<AUnitBase>> FirstEncounterUnits;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceFlowTest, "ProjectA.VerticalSlice.SavedMapsPIELoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVerticalSliceFlowTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/MainMenu")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectAVerticalSliceTests::FPlayVerticalSlice>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
