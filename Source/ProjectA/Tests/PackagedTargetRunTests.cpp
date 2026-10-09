#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace
{
    bool ResolvePackagedTargetSlot(FAutomationTestBase& Test, FString& Slot)
    {
        FString IdText;
        FString ExplicitSlot;
        FString UserDirectory;
        FGuid Id;
        if (!Test.TestTrue(TEXT("This content test requires an actual cooked game, never an Editor source-tree fallback."), FPlatformProperties::RequiresCookedData() && !GIsEditor)) return false;
        if (!Test.TestTrue(TEXT("Package persistence requires a fresh explicit UUID, isolated UserDir and left-monitor opt-in."), FParse::Value(FCommandLine::Get(), TEXT("ProjectAPackagedTargetId="), IdText) && FGuid::ParseExact(IdText, EGuidFormats::Digits, Id) && Id.IsValid() && FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) && !UserDirectory.IsEmpty() && FParse::Param(FCommandLine::Get(), TEXT("ProjectAReviewLeftMonitor")) && !FParse::Param(FCommandLine::Get(), TEXT("ProjectAPrototypeRun")))) return false;
        Slot = TEXT("ProjectA_Automation_TargetPackage_") + Id.ToString(EGuidFormats::Digits);
        return Test.TestTrue(TEXT("The explicit application save slot must equal this UUID-owned package fixture."), FParse::Value(FCommandLine::Get(), TEXT("ProjectASaveSlot="), ExplicitSlot) && ExplicitSlot == Slot && URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()) == Slot);
    }

    // Fit only this game's existing window to the enumerated leftmost work area; never save display settings.
    // 디스플레이 설정을 저장하지 않고 이 게임의 기존 창만 실제 가장 왼쪽 작업 영역 안에 배치합니다.
    bool PlacePackagedTargetWindow(FAutomationTestBase& Test, UWorld* World)
    {
        const TSharedPtr<SViewport> Viewport = World->GetGameViewport() ? World->GetGameViewport()->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Viewport && FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindWidgetWindow(Viewport.ToSharedRef()) : nullptr;
        if (!Test.TestTrue(TEXT("Cooked review owns a real window."), Window && Window->GetNativeWindow())) return false;
        FDisplayMetrics Metrics;
        FDisplayMetrics::RebuildDisplayMetrics(Metrics);
        const FMonitorInfo* Left = nullptr;
        for (const FMonitorInfo& Monitor : Metrics.MonitorInfo)
        {
            if (Monitor.WorkArea.Right <= Monitor.WorkArea.Left || Monitor.WorkArea.Bottom <= Monitor.WorkArea.Top) continue;
            if (!Left || Monitor.WorkArea.Left < Left->WorkArea.Left || (Monitor.WorkArea.Left == Left->WorkArea.Left && Monitor.WorkArea.Top < Left->WorkArea.Top)) Left = &Monitor;
        }
        if (!Test.TestNotNull(TEXT("An actual leftmost monitor is available."), Left)) return false;
        const FPlatformRect& Work = Left->WorkArea;
        const FVector2D Size = Window->GetSizeInScreen();
        if (!Test.TestTrue(TEXT("The packaged window fits on the left monitor."), Size.X > 0 && Size.Y > 0 && Size.X <= Work.Right - Work.Left && Size.Y <= Work.Bottom - Work.Top)) return false;
        const FVector2D Before = Window->GetPositionInScreen();
        const FVector2D Position(FMath::Clamp(Before.X, static_cast<double>(Work.Left), static_cast<double>(Work.Right) - Size.X), FMath::Clamp(Before.Y, static_cast<double>(Work.Top), static_cast<double>(Work.Bottom) - Size.Y));
        if (!Before.Equals(Position, 0.5)) Window->MoveWindowTo(Position);
        const FVector2D After = Window->GetPositionInScreen();
        return Test.TestTrue(TEXT("The full packaged window remains in the physical left work area."), After.X >= Work.Left - 1 && After.Y >= Work.Top - 1 && After.X + Size.X <= Work.Right + 1 && After.Y + Size.Y <= Work.Bottom + 1);
    }

    bool CheckCookedTargetContent(FAutomationTestBase& Test, const URunStateSubsystem& Run)
    {
        const FRunTargetState& Target = Run.GetTargetRunState();
        const FSoftObjectPath PotionPath = RunRecoveryRules::GetHealingSkillPath();
        if (!Test.TestTrue(TEXT("The runtime-only recovery package exists in the cooked containers and loads its authored DataAsset."), FPackageName::DoesPackageExist(PotionPath.GetLongPackageName()) && Cast<USkillDefinitionDataAsset>(PotionPath.TryLoad()) != nullptr && Target.Recovery.HealingSkill == PotionPath)) return false;
        if (!Test.TestTrue(TEXT("The cooked Run contains twenty combat nodes, ten frozen groups and its Snapshot catalog."), Run.IsTargetRun() && Run.GetNodes().Num() == 20 && Target.Groups.Num() == 10 && Target.OpponentCatalog.TryLoad() != nullptr)) return false;
        TSet<FSoftClassPath> EnemyClasses;
        for (const FTargetRunGroup& Group : Target.Groups)
        {
            for (const FSoftClassPath& Path : Group.EnemyClasses)
            {
                if (!Test.TestTrue(TEXT("Every later fixed PvE class is packaged and loadable, not only the first enemy."), FPackageName::DoesPackageExist(Path.GetLongPackageName()) && Path.TryLoadClass<AUnitBase>() != nullptr)) return false;
                EnemyClasses.Add(Path);
            }
        }
        for (const TPair<FName, FSoftClassPath>& Entry : Target.SnapshotClasses)
        {
            if (!Test.TestTrue(TEXT("Every frozen Snapshot class mapping resolves in the cooked package."), FPackageName::DoesPackageExist(Entry.Value.GetLongPackageName()) && Entry.Value.TryLoadClass<AUnitBase>() != nullptr)) return false;
        }
        for (const TPair<FName, FSoftObjectPath>& Entry : Target.SnapshotSkills)
        {
            if (!Test.TestTrue(TEXT("Every frozen Snapshot skill mapping resolves in the cooked package."), FPackageName::DoesPackageExist(Entry.Value.GetLongPackageName()) && Cast<USkillDefinitionDataAsset>(Entry.Value.TryLoad()) != nullptr)) return false;
        }
        for (const FRunPartyMember& Member : Run.GetPartyMembers())
        {
            if (!Member.bCreated) continue;
            FCombatRoundSkill Skill;
            FText Error;
            if (!Test.TestTrue(TEXT("Cooked recovery stock uses the real separate HP25/AP1 profile without occupying a learned skill slot."), Member.Consumables.Num() == 1 && Member.Consumables[0].Skill == PotionPath && Member.Consumables[0].Quantity == 1 && !Member.Skills.Contains(PotionPath) && RunRecoveryRules::ResolveStack(Member.Consumables[0], Skill, Error) && Skill.Power == 25.f && Skill.ActionPointCost == 1)) return false;
        }
        Test.AddInfo(FString::Printf(TEXT("Cooked target content: cooked=1; nodes=%d; groups=%d; PvEClassPackages=%d; SnapshotClasses=%d; SnapshotSkills=%d; recovery=%s."), Run.GetNodes().Num(), Target.Groups.Num(), EnemyClasses.Num(), Target.SnapshotClasses.Num(), Target.SnapshotSkills.Num(), *PotionPath.ToString()));
        return true;
    }

    class FPackagedTargetCommand : public IAutomationLatentCommand
    {
    public:
        FPackagedTargetCommand(FAutomationTestBase* InTest, FString InSlot, bool bInWrite) : Test(InTest), Slot(MoveTemp(InSlot)), bWrite(bInWrite), Started(FPlatformTime::Seconds()) {}

        virtual bool Update() override
        {
            if (FPlatformTime::Seconds() - Started > 45.0)
            {
                Test->AddError(FString::Printf(TEXT("Cooked target review timed out at stage=%d visit=%d; slot=%s; %s"), Stage, Visit, *Slot, *LastState));
                return true;
            }
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetGameInstance() && Context.World()->GetFirstPlayerController()) World = Context.World();
            }
            if (!World || !World->GetGameViewport() || !World->GetGameViewport()->GetGameViewportWidget()) return false;
            if (PlacedWorld.Get() != World)
            {
                if (!PlacePackagedTargetWindow(*Test, World)) return true;
                PlacedWorld = World;
            }
            if (bWrite) return Write();
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            if (!Run) return false;
            LastState = FString::Printf(TEXT("world=%s phase=%d choices=%d error=%s"), *World->GetName(), static_cast<int32>(Run->GetPhase()), Run->GetTargetRunState().CompletedEncounterChoices.Num(), *Run->GetSaveError().ToString());
            if (Stage == 0)
            {
                TArray<UUserWidget*> Menus;
                UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Menus, UMainMenuScreenWidget::StaticClass(), false);
                for (UUserWidget* Widget : Menus)
                {
                    UMainMenuScreenWidget* Menu = Cast<UMainMenuScreenWidget>(Widget);
                    UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_Continue")));
                    if (!Menu->IsActivated() || !Button) continue;
                    FText Error;
                    Saved.Reset(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
                    if (!Test->TestTrue(TEXT("The independent cooked reader opens only the writer's fresh target checkpoint."), Saved && Saved->TargetRun.SchemaVersion == 1 && Saved->Phase == ERunPhase::EncounterChoice && Saved->Party.Num() == 4 && Saved->CompletedNodes.IsEmpty() && Saved->TargetRun.CompletedEncounterChoices.IsEmpty() && UGameplayStatics::LoadDataFromSlot(WriterBytes, Slot, 0))) return true;
                    Menu->RefreshResumeActions();
                    if (!Test->TestTrue(TEXT("The actual packaged menu enables Continue for the cooked target writer."), Button->GetIsEnabled())) return true;
                    Button->OnClicked.Broadcast();
                    Stage = 1;
                    return false;
                }
                return false;
            }
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            AGameplayGameState* State = World->GetGameState<AGameplayGameState>();
            if (!Controller || !State) return false;
            if (Stage == 1)
            {
                if (Run->GetPhase() != ERunPhase::EncounterChoice || State->GetViewState().Phase != ERunPhase::EncounterChoice) return false;
                if (!Test->TestTrue(TEXT("Cooked Continue retains the complete frozen dungeon in both authority and presentation."), FRunDungeonState::StaticStruct()->CompareScriptStruct(&Saved->DungeonState, &Run->GetDungeonState(), 0) && FRunDungeonState::StaticStruct()->CompareScriptStruct(&Saved->DungeonState, &State->GetViewState().DungeonState, 0))) return true;
                TArray<uint8> AfterContinue;
                if (!Test->TestTrue(TEXT("Actual menu Continue restores frozen target/identity/encounter values without rewriting the writer's file."), FRunTargetState::StaticStruct()->CompareScriptStruct(&Saved->TargetRun, &Run->GetTargetRunState(), 0) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Saved->Identity, &Run->GetRunIdentity(), 0) && FRunEncounterProgress::StaticStruct()->CompareScriptStruct(&Saved->EncounterProgress, &Run->GetEncounterProgress(), 0) && Saved->Party.Num() == Run->GetPartyMembers().Num() && UGameplayStatics::LoadDataFromSlot(AfterContinue, Slot, 0) && WriterBytes == AfterContinue)) return true;
                for (int32 Index = 0; Index < Saved->Party.Num(); ++Index)
                {
                    if (!Test->TestTrue(TEXT("Cooked Continue preserves each original owner, HP, gold, equipment, skills and separate potion stock."), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Saved->Party[Index], &Run->GetPartyMembers()[Index], 0))) return true;
                }
                if (!CheckCookedTargetContent(*Test, *Run)) return true;
                Stage = 2;
            }
            if (Stage == 2)
            {
                if (Run->GetPhase() == ERunPhase::EncounterChoice)
                {
                    if (!Test->TestTrue(TEXT("The cooked target offers the next three candidates in order."), Run->GetEncounterProgress().Offers.Num() == 3 && Run->GetEncounterProgress().VisitIndex == Visit && Run->GetTargetRunState().CompletedEncounterChoices.Num() == Visit)) return true;
                    Controller->RequestSelectRunEncounter(Run->GetEncounterProgress().Offers[0].EncounterId);
                    if (!Test->TestEqual(TEXT("The production selection request enters the selected cooked service."), Run->GetPhase(), ERunPhase::Shop)) return true;
                    return false;
                }
                if (Run->GetPhase() == ERunPhase::Shop)
                {
                    Controller->RequestLeaveRunEncounter();
                    ++Visit;
                    if (!Test->TestEqual(TEXT("The production leave request persists one completed choice."), Run->GetTargetRunState().CompletedEncounterChoices.Num(), Visit)) return true;
                    return false;
                }
                if (!Test->TestTrue(TEXT("Exactly three completed choices open the first cooked PvE node."), Visit == 3 && Run->GetPhase() == ERunPhase::Map && Run->CanStartNode(TEXT("TargetCombat_01")))) return true;
                const FGameplayTag Difficulty = Run->GetTargetRunState().PveDifficulty.SchemaVersion == 1 ? RunPveDifficulty::GetMediumTag() : FGameplayTag();
                Controller->RequestStartNode(TEXT("TargetCombat_01"), Difficulty);
                Stage = 3;
                return false;
            }
            AEncounterManager* Encounter = nullptr;
            for (TActorIterator<AEncounterManager> It(World); It; ++It) Encounter = *It;
            ACombatRoundCoordinator* Round = Encounter && Encounter->GetCombatManager() ? Encounter->GetCombatManager()->GetRoundCoordinator() : nullptr;
            if (Run->GetPhase() != ERunPhase::Combat || !Round || Round->GetView().Phase != ECombatRoundPhase::Planning || !Run->HasCombatCheckpoint()) return false;
            const FTargetRunGroup& FirstGroup = Run->GetTargetRunState().Groups[0];
            const int32 ExpectedEnemies = FirstGroup.EnemyClasses.Num();
            if (!Test->TestTrue(TEXT("The cooked first PvE encounter uses its nonempty frozen CSV roster."), Run->GetTargetRunState().LevelDesign.SchemaVersion == 1 && ExpectedEnemies > 0 && FirstGroup.EnemyRoster.Num() == ExpectedEnemies)) return true;
            int32 Players = 0;
            int32 Enemies = 0;
            for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
            {
                if (!Test->TestNotNull(TEXT("Every cooked encounter actor exists."), Unit)) return true;
                if (Unit->GetTeam() == ETeam::Player)
                {
                    ++Players;
                    FCombatRoundSkill Skill;
                    FText Error;
                    if (!Test->TestTrue(TEXT("The actual spawned ally retains its cooked recovery profile and one potion."), Unit->Consumables.Num() == 1 && Unit->Consumables[0].Quantity == 1 && RunRecoveryRules::ResolveStack(Unit->Consumables[0], Skill, Error) && Round->FindSkill(Skill.SkillId) != nullptr)) return true;
                }
                else
                {
                    if (!Test->TestTrue(TEXT("Every actual cooked enemy follows its own frozen first-PvE class and slot."), FirstGroup.EnemyClasses.IsValidIndex(Enemies) && FSoftClassPath(Unit->GetClass()) == FirstGroup.EnemyClasses[Enemies] && FirstGroup.EnemyRoster[Enemies].UnitClass == FirstGroup.EnemyClasses[Enemies])) return true;
                    ++Enemies;
                }
            }
            if (!Test->TestTrue(TEXT("Cooked public progression spawns four allies and the complete frozen first target roster without executing combat."), Players == 4 && Enemies == ExpectedEnemies && Run->GetCompletedNodes().IsEmpty() && Run->GetTargetRunState().CompletedEncounterChoices.Num() == 3)) return true;
            FText Error;
            TStrongObjectPtr<URunSaveGame> Checkpoint(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Test->TestTrue(TEXT("The actual cooked Planning boundary persists the target and every spawned ally and frozen enemy."), Checkpoint && Checkpoint->Phase == ERunPhase::Combat && Checkpoint->TargetRun.SchemaVersion == 1 && Checkpoint->CombatCheckpoint.Units.Num() == Players + ExpectedEnemies)) return true;
            if (Run->GetTargetRunState().PveDifficulty.SchemaVersion == 1 && !Test->TestTrue(TEXT("The cooked Planning boundary preserves the explicit medium PvE selection."), Checkpoint->TargetRun.PveDifficulty.SelectedTags == TArray<FGameplayTag>{RunPveDifficulty::GetMediumTag()})) return true;
            if (!Test->TestTrue(TEXT("Only this UUID-owned cooked checkpoint is removed after successful verification."), UGameplayStatics::DeleteGameInSlot(Slot, 0) && !UGameplayStatics::DoesSaveGameExist(Slot, 0))) return true;
            Test->AddInfo(FString::Printf(TEXT("Cooked target Continue passed: slot=%s; actualMenu=1; choices=3; allies=%d; enemies=%d; planningSaved=1; deleted=1; combatExecuted=0; normalCompletion=0."), *Slot, Players, Enemies));
            return true;
        }

    private:
        bool Write()
        {
            if (!Test->TestFalse(TEXT("A new cooked writer never overwrites an existing checkpoint."), UGameplayStatics::DoesSaveGameExist(Slot, 0))) return true;
            TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
            TStrongObjectPtr<URunStateSubsystem> Run(NewObject<URunStateSubsystem>(Instance.Get()));
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            if (!Test->TestNotNull(TEXT("The actual party catalog is included in the cooked game."), Run->PartyDefinition.Get())) return true;
            Run->EnableCheckpointSaving(Slot);
            TArray<FRunPartyMember> Party;
            const FName Classes[] = {TEXT("Warrior"), TEXT("Mage"), TEXT("Archer"), TEXT("Rogue")};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Classes); ++Index)
            {
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.ClassId = Classes[Index];
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Cooked target %d"), Index + 1));
                Member.bCreated = true;
                Member.bPlayerControlled = Index == 0;
            }
            FText Error;
            if (!Test->TestTrue(TEXT("Public target initialization validates cooked dependencies and writes the real initial checkpoint."), Run->InitializeTargetRun(Party, Error)))
            {
                Test->AddError(Error.ToString());
                return true;
            }
            if (!CheckCookedTargetContent(*Test, *Run)) return true;
            TStrongObjectPtr<URunSaveGame> Written(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Test->TestTrue(TEXT("The cooked writer persists a resumable initial target selection with no combat result or earned rewards."), Written && Written->Phase == ERunPhase::EncounterChoice && Written->CompletedNodes.IsEmpty() && Written->TargetRun.CompletedEncounterChoices.IsEmpty() && Written->GoldRewardState.SchemaVersion == 0 && Run->CanContinueStandaloneSavedRun(Error))) return true;
            Test->AddInfo(FString::Printf(TEXT("Cooked target writer passed: slot=%s; cooked=1; party=4; nodes=20; groups=10; recovery=1; phase=EncounterChoice; retainedForIndependentContinue=1."), *Slot));
            return true;
        }

        FAutomationTestBase* Test;
        FString Slot;
        bool bWrite;
        double Started;
        int32 Stage = 0;
        int32 Visit = 0;
        FString LastState;
        TWeakObjectPtr<UWorld> PlacedWorld;
        TStrongObjectPtr<URunSaveGame> Saved;
        TArray<uint8> WriterBytes;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedTargetWriteTest, "ProjectA.Package.TargetWrite", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedTargetWriteTest::RunTest(const FString& Parameters)
{
    FString Slot;
    if (!ResolvePackagedTargetSlot(*this, Slot)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FPackagedTargetCommand(this, Slot, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedTargetContinueTest, "ProjectA.Package.TargetContinue", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedTargetContinueTest::RunTest(const FString& Parameters)
{
    FString Slot;
    if (!ResolvePackagedTargetSlot(*this, Slot)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FPackagedTargetCommand(this, Slot, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagedLongCheckpointTest, "ProjectA.Package.LongCheckpoint", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPackagedLongCheckpointTest::RunTest(const FString& Parameters)
{
    // Exercise real Windows files with long final and temporary names, not a string-only path-format test.
    // 경로 문자열 형식만 검사하지 않고 최종·임시 이름이 모두 긴 실제 Windows 파일을 검증합니다.
    FString Slot;
    if (!ResolvePackagedTargetSlot(*this, Slot)) return false;
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetFirstPlayerController()) World = Context.World();
    }
    if (!TestNotNull(TEXT("The long-path storage review owns the running cooked world."), World) || !PlacePackagedTargetWindow(*this, World)) return false;
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav")));
    if (!TestTrue(TEXT("The isolated UserDir makes the final checkpoint itself exceed the classic 260-character boundary."), Path.Len() >= 260 && Path.Len() < 1024)) return false;
    if (!TestFalse(TEXT("The long-path fixture cannot overwrite an existing save."), UGameplayStatics::DoesSaveGameExist(Slot, 0))) return false;
    TArray<FString> TemporaryFiles;
    IFileManager::Get().FindFiles(TemporaryFiles, *(Path + TEXT(".*.tmp")), true, false);
    if (!TestTrue(TEXT("This fresh UUID has no pre-existing temporary checkpoint."), TemporaryFiles.IsEmpty())) return false;

    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->CurrentNode = TEXT("LongCheckpoint_Original");
    FText Error;
    if (!TestTrue(TEXT("Actual storage writes and atomically publishes a long-path checkpoint."), FRunCheckpointStorage::Save(Save.Get(), Slot, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    FString OriginalToken;
    TStrongObjectPtr<URunSaveGame> Original(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error, &OriginalToken)));
    TArray<uint8> OriginalBytes;
    if (!TestTrue(TEXT("Actual storage reloads the original long-path bytes and confirmation token."), Original && Original->CurrentNode == Save->CurrentNode && !OriginalToken.IsEmpty() && UGameplayStatics::LoadDataFromSlot(OriginalBytes, Slot, 0) && !OriginalBytes.IsEmpty())) return false;

    Save->CurrentNode = TEXT("LongCheckpoint_Replaced");
    if (!TestTrue(TEXT("The same long destination is atomically replaced without deleting it first."), FRunCheckpointStorage::Save(Save.Get(), Slot, Error))) return false;
    FString ReplacementToken;
    TStrongObjectPtr<URunSaveGame> Replacement(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error, &ReplacementToken)));
    TArray<uint8> ReplacementBytes;
    if (!TestTrue(TEXT("Reload observes only the new long-path checkpoint with a changed token."), Replacement && Replacement->CurrentNode == Save->CurrentNode && !ReplacementToken.IsEmpty() && ReplacementToken != OriginalToken && UGameplayStatics::LoadDataFromSlot(ReplacementBytes, Slot, 0) && ReplacementBytes != OriginalBytes)) return false;

    Save->CurrentNode = TEXT("LongCheckpoint_Rejected");
    FRunCheckpointStorage::FailNextWriteForTesting();
    if (!TestFalse(TEXT("An injected write failure prevents long-path replacement publication."), FRunCheckpointStorage::Save(Save.Get(), Slot, Error))) return false;
    TArray<uint8> AfterFailure;
    FString PreservedToken;
    TStrongObjectPtr<URunSaveGame> Preserved(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error, &PreservedToken)));
    IFileManager::Get().FindFiles(TemporaryFiles, *(Path + TEXT(".*.tmp")), true, false);
    if (!TestTrue(TEXT("Failed long-path replacement preserves exact prior bytes, value and token and removes its temporary file."), UGameplayStatics::LoadDataFromSlot(AfterFailure, Slot, 0) && AfterFailure == ReplacementBytes && Preserved && Preserved->CurrentNode == Replacement->CurrentNode && PreservedToken == ReplacementToken && TemporaryFiles.IsEmpty())) return false;
    if (!TestFalse(TEXT("The original stale confirmation cannot delete the replaced long-path checkpoint."), FRunCheckpointStorage::DeleteIfUnchanged(Slot, OriginalToken, Error))) return false;
    if (!TestTrue(TEXT("Stale deletion preserves the replaced bytes."), UGameplayStatics::LoadDataFromSlot(AfterFailure, Slot, 0) && AfterFailure == ReplacementBytes)) return false;
    FRunCheckpointStorage::FailNextDeleteForTesting();
    if (!TestFalse(TEXT("Injected exclusive-handle deletion failure preserves the long-path checkpoint."), FRunCheckpointStorage::DeleteIfUnchanged(Slot, ReplacementToken, Error))) return false;
    if (!TestTrue(TEXT("Failed deletion leaves exact bytes for a valid retry."), UGameplayStatics::LoadDataFromSlot(AfterFailure, Slot, 0) && AfterFailure == ReplacementBytes)) return false;
    if (!TestTrue(TEXT("The current token deletes only the owned long-path checkpoint through the exclusive Win32 handle."), FRunCheckpointStorage::DeleteIfUnchanged(Slot, ReplacementToken, Error) && !UGameplayStatics::DoesSaveGameExist(Slot, 0))) return false;
    IFileManager::Get().FindFiles(TemporaryFiles, *(Path + TEXT(".*.tmp")), true, false);
    if (!TestTrue(TEXT("Successful completion leaves neither the UUID save nor a temporary file."), TemporaryFiles.IsEmpty())) return false;
    AddInfo(FString::Printf(TEXT("Cooked long checkpoint passed: slot=%s; finalPathChars=%d; temporaryPathChars=%d; wrote=1; reloaded=1; replaced=1; writeFailurePreserved=1; staleTokenRejected=1; deleteFailurePreserved=1; deleted=1."), *Slot, Path.Len(), Path.Len() + 37));
    return true;
}

#endif
