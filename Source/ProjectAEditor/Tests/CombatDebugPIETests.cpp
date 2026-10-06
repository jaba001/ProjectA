#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/CapsuleComponent.h"
#include "Components/ComboBoxString.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Debug/CombatDebugWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace CombatDebugPIE
{
// Observe particle simulation on real frames and save viewport captures for a separate visual review.
// 실제 프레임의 파티클 시뮬레이션을 관찰하고 화면 가시성은 별도로 확인하도록 뷰포트 캡처를 저장합니다.
class FAuthoredToolsAndVfx : public IAutomationLatentCommand
{
public:
    explicit FAuthoredToolsAndVfx(FAutomationTestBase* InTest) : Test(InTest)
    {
        OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/CombatDebugPIE") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*OutputDirectory, true);
        ViewportRenderedHandle = UGameViewportClient::OnViewportRendered().AddRaw(this, &FAuthoredToolsAndVfx::CaptureRenderedViewport);
    }

    virtual ~FAuthoredToolsAndVfx() override
    {
        UGameViewportClient::OnViewportRendered().Remove(ViewportRenderedHandle);
    }

    virtual bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (Stage == 9)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && FPlatformTime::Seconds() - Started < 30.0) return false;
                if (Context.WorldType == EWorldType::PIE) Test->AddError(TEXT("Debug PIE world did not close."));
            }
            return true;
        }
        if (FPlatformTime::Seconds() - Started > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Debug PIE timed out at stage %d, sample %d."), Stage, SampleIndex));
            return End();
        }
        if (Stage == 0)
        {
            Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            Settings->SetPlayNetMode(PIE_Standalone);
            Settings->SetPlayNumberOfClients(1);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;
            Settings->NewWindowWidth = 1280;
            Settings->NewWindowHeight = 720;
            Settings->SetClientWindowSize(FIntPoint(1280, 720));
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = Settings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat");
            GEditor->RequestPlaySession(Params);
            Advance(1);
            return false;
        }
        if (Stage == 1)
        {
            if (!Connect()) return false;
            TArray<UUserWidget*> Screens;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Screens, UCombatRoundPlanningWidget::StaticClass(), false);
            if (!Screens.ContainsByPredicate([this](UUserWidget* Screen) { return Screen->GetOwningPlayer() == Controller && Cast<UCombatRoundPlanningWidget>(Screen)->IsActivated(); })) return false;
            if (!SelectReadyDebugAlly()) return false;
            if (!VerifySkillMethodFilters() || !PrepareRoster()) return End();
            RevivedUnit->Die();
            Advance(2);
            return false;
        }
        ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
        if (!Check(IsValid(Round) && IsValid(Mode), TEXT("The debug combat context remains available throughout verification."))) return End();
        if (Stage == 2)
        {
            FText Error;
            if (!Check(!RevivedUnit->IsUnitAlive() && RevivedUnit->GetMesh()->IsSimulatingPhysics(), TEXT("The authored character enters death ragdoll on a real frame."))) return End();
            if (!Check(Round->ReviveDebugUnit(Controller, RevivedUnit->UnitIndex, Error), FString::Printf(TEXT("The authored ally revives: %s"), *Error.ToString()))) return End();
            Advance(3);
            return false;
        }
        if (Stage == 3)
        {
            if (FPlatformTime::Seconds() - Started < 0.2) return false;
            const FRunPartyMember* Equipment = Controller->GetDebugLoadout()->GetEquipmentMember(RevivedUnit->UnitIndex);
            if (!Check(RevivedUnit->IsUnitAlive() && !RevivedUnit->GetMesh()->IsSimulatingPhysics() && RevivedUnit->GetMesh()->GetAnimInstance() && RevivedUnit->GetMesh()->GetAttachParent() == RevivedUnit->GetCapsuleComponent() && RevivedUnit->GetMesh()->GetRelativeTransform().Equals(AliveMeshTransform), TEXT("Revival restores animated mesh attachment and pose without ragdoll simulation."))) return End();
            if (!Check(RevivedUnit->GetAttributeSet()->GetHP() == RevivedUnit->GetAttributeSet()->GetMaxHP() && RevivedUnit->GetCurrentActionPoint() == RevivedUnit->GetMaxActionPoint() && RevivedUnit->GetCurrentSubActionPoint() == RevivedUnit->GetMaxSubActionPoint() && RevivedUnit->GetEquippedSkillDataAssets() == OriginalSkills && Equipment && FRunPartyMember::StaticStruct()->CompareScriptStruct(Equipment, &OriginalEquipment, 0), TEXT("Authored revival restores resources and retains equipped assets and debug equipment."))) return End();
            if (!Check(Mode->RestartCombat(), TEXT("Each VFX sample starts in a fresh debug combat."))) return End();
            Advance(4);
            return false;
        }
        if (Stage == 4)
        {
            if (Round->GetView().Phase != ECombatRoundPhase::Planning) return false;
            if (!PrepareSkill()) return End();
            Advance(5);
            return false;
        }
        if (Stage == 5)
        {
            ObserveVfx();
            if (bCaptureFailed) return End();
            if (Round->GetView().Phase == ECombatRoundPhase::Resolving) return false;
            if (Round->GetView().RoundNumber == SampleRound && Round->GetView().Phase != ECombatRoundPhase::Finished) return false;
            if (!PendingScreenshot.IsEmpty()) return false;
            Test->AddInfo(FString::Printf(TEXT("%s: active=%d, ready=%d, max particles=%d, max age=%.3f; GPU particle counts use the engine's delayed count/estimate."), *Samples[SampleIndex], bSawActive, bSawReady, MaxParticles, MaxAge));
            if (!Check(bSawActive && bSawReady && MaxParticles > 0 && MaxAge > 0.f, TEXT("The real authored skill produces active, ready, advancing particle simulation."))) return End();
            if (!Check(bFirstScreenshot && bMiddleScreenshot, TEXT("Viewport captures were requested on two frames with active particle simulation."))) return End();
            Advance(6);
            return false;
        }
        if (Stage == 6)
        {
            if (!PendingScreenshot.IsEmpty() || FScreenshotRequest::IsScreenshotRequested()) return false;
            if (!Check(!bCaptureFailed && bFirstCaptureCompleted && bMiddleCaptureCompleted && IFileManager::Get().FileSize(*FirstScreenshot) > 0 && IFileManager::Get().FileSize(*MiddleScreenshot) > 0, TEXT("Both captures completed on rendered PIE viewport frames and were written before restart or teardown."))) return End();
            Test->AddInfo(TEXT("VFX screenshots: ") + FirstScreenshot + TEXT(" ; ") + MiddleScreenshot + TEXT(". The _middle file is the second particle observation at least 0.1 seconds after the first; it does not identify the effect lifetime midpoint."));
            ++SampleIndex;
            if (SampleIndex == Samples.Num())
            {
                Check(CompletedCaptures == Samples.Num() * 2, TEXT("Every authored sample has two completed captures from its real PIE viewport."));
                return End();
            }
            if (!Check(Mode->RestartCombat(), TEXT("Restart the disposable combat for the next authored effect."))) return End();
            Advance(4);
        }
        return false;
    }

private:
    bool Connect()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || World->GetNetMode() != NM_Standalone) continue;
            Mode = World->GetAuthGameMode<ACombatDebugGameMode>();
            Controller = Cast<ACombatDebugPlayerController>(World->GetFirstPlayerController());
            if (Mode && Controller && Controller->GetRoundCoordinator() && Controller->GetRoundCoordinator()->GetView().Phase == ECombatRoundPhase::Planning) return true;
        }
        return false;
    }

    bool PrepareRoster()
    {
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        if (!Check(Round->GetView().Units.Num() == 2 && Round->GetView().Units.FilterByPredicate([](const FCombatRoundUnitView& Unit) { return Unit.bEnemy; }).Num() == 1, TEXT("The authored debug roster starts with one ally and one enemy."))) return false;
        for (const FCombatRoundUnitView& Entry : Round->GetView().Units)
        {
            const UAS_Unit* Attributes = IsValid(Entry.Unit) ? Entry.Unit->GetAttributeSet() : nullptr;
            if (!Check(Attributes && Attributes->GetHP() == 10000.f && Attributes->GetMaxHP() == 10000.f, TEXT("Both authored debug units start with 10,000 current and maximum HP."))) return false;
        }
        const FCombatRoundUnitView* Existing = Round->GetView().Units.FindByPredicate([](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && IsValid(Unit.Unit); });
        if (!Check(Existing != nullptr, TEXT("The authored debug roster contains an allied character."))) return false;
        RevivedUnit = Existing->Unit;
        OriginalSkills = RevivedUnit->GetEquippedSkillDataAssets();
        AliveMeshTransform = RevivedUnit->GetMesh()->GetRelativeTransform();
        const FRunPartyMember* Equipment = Controller->GetDebugLoadout()->GetEquipmentMember(RevivedUnit->UnitIndex);
        if (!Check(Equipment != nullptr, TEXT("The authored original ally has a debug equipment record."))) return false;
        OriginalEquipment = *Equipment;
        const FGuid CombatId = Round->GetView().CombatId;
        const FGuid UnitId = Mode->GetCombatManager()->GetRuntimeUnitId(RevivedUnit);
        const int32 OriginalCount = Round->GetView().Units.Num();
        for (bool bEnemy : {false, true})
        {
            TArray<FName> Ids;
            TArray<FText> Names;
            Mode->GetDebugSpawnOptions(bEnemy, Ids, Names);
            if (!Check(!Ids.IsEmpty() && Ids.Num() == Names.Num(), TEXT("The authored spawn catalog resolves names and options."))) return false;
            int32 AddedId = INDEX_NONE;
            FText Error;
            if (!Check(Mode->SpawnDebugUnit(Controller, bEnemy, Ids[0], AddedId, Error), FString::Printf(TEXT("Spawn authored %s: %s"), bEnemy ? TEXT("enemy") : TEXT("profession"), *Error.ToString()))) return false;
            const FCombatRoundUnitView* Added = Round->GetView().Units.FindByPredicate([AddedId](const FCombatRoundUnitView& Unit) { return Unit.UnitId == AddedId; });
            if (!Check(Added && Added->bEnemy == bEnemy && Added->Unit && Added->Unit->GetMesh()->GetSkeletalMeshAsset() && Added->Unit->GetMesh()->GetAnimInstance(), TEXT("Added authored units have the requested team, skeletal mesh and animation instance."))) return false;
            const UAS_Unit* AddedAttributes = Added->Unit->GetAttributeSet();
            if (!Check(AddedAttributes && AddedAttributes->GetHP() == 10000.f && AddedAttributes->GetMaxHP() == 10000.f, TEXT("Added allies and enemies use the same 10,000 current and maximum HP debug defaults."))) return false;
            if (!bEnemy && !Check(Controller->GetDebugLoadout()->GetEquipmentMember(AddedId) != nullptr, TEXT("A newly added ally receives an editable equipment record in the same combat."))) return false;
        }
        return Check(Round->GetView().Units.Num() == OriginalCount + 2 && Round->GetView().CombatId == CombatId && Mode->GetCombatManager()->GetRuntimeUnitId(RevivedUnit) == UnitId && RevivedUnit->GetEquippedSkillDataAssets() == OriginalSkills, TEXT("Authored spawning preserves the current combat, original character and loadout."));
    }

    UCombatDebugWidget* FindDebugTools() const
    {
        TArray<UUserWidget*> Screens;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Screens, UCombatDebugWidget::StaticClass(), false);
        for (UUserWidget* Screen : Screens)
        {
            if (Screen->GetOwningPlayer() == Controller) return Cast<UCombatDebugWidget>(Screen);
        }
        return nullptr;
    }

    bool SelectReadyDebugAlly()
    {
        UCombatDebugWidget* Tools = FindDebugTools();
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const FCombatRoundUnitView* Ally = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && Unit.OwnerSlot == Controller->GetRoundParticipantSlot() && IsValid(Unit.Unit); });
        if (!Tools || !Tools->WidgetTree || !Ally) return false;
        TArray<UWidget*> Widgets;
        Tools->WidgetTree->GetAllWidgets(Widgets);
        const FString Prefix = FString::Printf(TEXT("[아군] %d · "), Ally->UnitId);
        for (UWidget* Widget : Widgets)
        {
            UComboBoxString* Choice = Cast<UComboBoxString>(Widget);
            if (!Choice) continue;
            for (int32 Index = 0; Index < Choice->GetOptionCount(); ++Index)
            {
                const FString Option = Choice->GetOptionAtIndex(Index);
                if (!Option.StartsWith(Prefix)) continue;
                // Wait for the widget's normal roster refresh, then use its real selection delegate.
                // 위젯의 정상 명단 갱신을 기다린 뒤 실제 선택 델리게이트를 사용합니다.
                Choice->SetSelectedOption(Option);
                return true;
            }
        }
        return false;
    }

    bool VerifySkillMethodFilters()
    {
        UCombatDebugWidget* Tools = FindDebugTools();
        if (!Check(Tools && Tools->WidgetTree, TEXT("The live combat exposes its authored debug tool widget."))) return false;
        TArray<UWidget*> Widgets;
        Tools->WidgetTree->GetAllWidgets(Widgets);
        TMap<int32, UCombatDebugActionButton*> Methods;
        TMap<int32, UCombatDebugActionButton*> Categories;
        for (UWidget* Widget : Widgets)
        {
            UCombatDebugActionButton* Button = Cast<UCombatDebugActionButton>(Widget);
            if (!Button) continue;
            if (Button->Action == ECombatDebugAction::SelectSkillMethod) Methods.Add(Button->Index, Button);
            if (Button->Action == ECombatDebugAction::SelectSkillCategory) Categories.Add(Button->Index, Button);
        }
        if (!Check(Methods.Num() == 7 && Categories.Num() == 8, TEXT("The live catalog exposes seven method tabs and eight element tabs."))) return false;
        const TArray<FString> Labels{TEXT("전체"), TEXT("투사체"), TEXT("범위형"), TEXT("체인"), TEXT("근접공격"), TEXT("지원형"), TEXT("미분류")};
        for (int32 Index = 0; Index < Labels.Num(); ++Index)
        {
            UCombatDebugActionButton* const* Button = Methods.Find(Index);
            const UTextBlock* Label = Button ? Cast<UTextBlock>((*Button)->GetContent()) : nullptr;
            if (!Check(Label && Label->GetText().ToString().Contains(Labels[Index]), TEXT("Every live method tab has its declared order and Korean label."))) return false;
        }
        if (!Check(Categories.Contains(0), TEXT("The element catalog provides an all-elements tab."))) return false;
        Categories.FindChecked(0)->OnClicked.Broadcast();
        Methods.FindChecked(3)->OnClicked.Broadcast();
        Widgets.Reset();
        Tools->WidgetTree->GetAllWidgets(Widgets);
        TSet<FSoftObjectPath> ChainAssets;
        UCombatDebugLoadout* Loadout = Controller->GetDebugLoadout();
        if (!Check(IsValid(Loadout), TEXT("Live method filtering uses the shared resolved skill catalog."))) return false;
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const FCombatRoundUnitView* Ally = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && Unit.OwnerSlot == Controller->GetRoundParticipantSlot() && IsValid(Unit.Unit); });
        if (!Check(Ally != nullptr, TEXT("The method-filter fixture selects an actual owned ally."))) return false;
        const int32 AllyId = Ally->UnitId;
        AUnitBase* Source = Ally->Unit;
        const TArray<TObjectPtr<USkillDefinitionDataAsset>> SavedSkills = Source->GetEquippedSkillDataAssets();
        TSet<FSoftObjectPath> CatalogChains;
        TSet<FSoftObjectPath> ExpectedUnownedChains;
        FSoftObjectPath OwnershipProbe;
        for (const FSoftObjectPath& Asset : Loadout->GetSkillAssets())
        {
            const FGameplayTagContainer& Tags = Loadout->GetSkillTags(Asset);
            if (!Tags.HasTag(ProjectACombatTags::Skill_Shape_Chain)) continue;
            if (!Check(!Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal) && !Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield), TEXT("Every current catalog chain is an attack profile."))) return false;
            CatalogChains.Add(Asset);
            if (SavedSkills.ContainsByPredicate([&Asset](const USkillDefinitionDataAsset* Skill) { return Skill && FSoftObjectPath(Skill) == Asset; })) continue;
            ExpectedUnownedChains.Add(Asset);
            if (OwnershipProbe.IsNull()) OwnershipProbe = Asset;
        }
        if (!Check(CatalogChains.Num() == 5, TEXT("The resolved complete catalog contains all five authored chain skills."))) return false;
        for (UWidget* Widget : Widgets)
        {
            const UCombatDebugActionButton* Button = Cast<UCombatDebugActionButton>(Widget);
            if (!Button || Button->Action != ECombatDebugAction::AddSkill) continue;
            const FGameplayTagContainer& Tags = Loadout->GetSkillTags(Button->Asset);
            if (!Check(Tags.HasTag(ProjectACombatTags::Skill_Shape_Chain) && !Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal) && !Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield), TEXT("The clicked chain tab displays chain attacks and excludes support effects."))) return false;
            ChainAssets.Add(Button->Asset);
        }
        if (!Check(ChainAssets.Num() == ExpectedUnownedChains.Num() && ChainAssets.Includes(ExpectedUnownedChains), TEXT("The clicked chain tab exactly matches all five catalog chains after excluding the selected ally's owned skills."))) return false;
        Test->AddInfo(FString::Printf(TEXT("Chain filter: complete catalog=%d, already owned=%d, displayed unowned=%d."), CatalogChains.Num(), CatalogChains.Num() - ExpectedUnownedChains.Num(), ChainAssets.Num()));
        USkillDefinitionDataAsset* ProbeSkill = Cast<USkillDefinitionDataAsset>(OwnershipProbe.TryLoad());
        if (!Check(ProbeSkill != nullptr, TEXT("A current unowned chain is available for an isolated ownership filter probe."))) return false;
        TArray<TObjectPtr<USkillDefinitionDataAsset>> ProbeSkills = SavedSkills;
        ProbeSkills.Add(ProbeSkill);
        FText Error;
        if (!Check(Round->SetDebugUnitSkills(Controller, AllyId, ProbeSkills, Error), TEXT("The public debug API temporarily equips the original chain asset: ") + Error.ToString())) return false;
        bool bRestored = false;
        ON_SCOPE_EXIT
        {
            if (!bRestored)
            {
                Round->SetDebugUnitSkills(Controller, AllyId, SavedSkills, Error);
                Methods.FindChecked(3)->OnClicked.Broadcast();
            }
        };
        Methods.FindChecked(3)->OnClicked.Broadcast();
        Widgets.Reset();
        Tools->WidgetTree->GetAllWidgets(Widgets);
        TSet<FSoftObjectPath> ProbeUnowned;
        bool bProbeOwnedVisible = false;
        for (UWidget* Widget : Widgets)
        {
            const UCombatDebugActionButton* Button = Cast<UCombatDebugActionButton>(Widget);
            if (!Button) continue;
            if (Button->Action == ECombatDebugAction::AddSkill) ProbeUnowned.Add(Button->Asset);
            if (Button->Action == ECombatDebugAction::RemoveSkill && Button->Asset == OwnershipProbe) bProbeOwnedVisible = true;
        }
        TSet<FSoftObjectPath> ExpectedProbe = ExpectedUnownedChains;
        ExpectedProbe.Remove(OwnershipProbe);
        if (!Check(bProbeOwnedVisible && ProbeUnowned.Num() == ExpectedProbe.Num() && ProbeUnowned.Includes(ExpectedProbe) && !ProbeUnowned.Contains(OwnershipProbe), TEXT("Equipping a real chain moves it to the owned list and excludes only that asset from the chain acquisition list."))) return false;
        bRestored = Round->SetDebugUnitSkills(Controller, AllyId, SavedSkills, Error);
        if (!Check(bRestored && Source->GetEquippedSkillDataAssets() == SavedSkills, TEXT("The disposable ownership probe restores the exact original loadout: ") + Error.ToString())) return false;
        Methods.FindChecked(3)->OnClicked.Broadcast();
        Methods.FindChecked(2)->OnClicked.Broadcast();
        Widgets.Reset();
        Tools->WidgetTree->GetAllWidgets(Widgets);
        int32 AreaAssets = 0;
        for (UWidget* Widget : Widgets)
        {
            const UCombatDebugActionButton* Button = Cast<UCombatDebugActionButton>(Widget);
            if (!Button || Button->Action != ECombatDebugAction::AddSkill) continue;
            if (!Check(!CatalogChains.Contains(Button->Asset) && !Loadout->GetSkillTags(Button->Asset).HasTag(ProjectACombatTags::Skill_Shape_Chain), TEXT("The clicked area tab excludes all chain-classified link attacks."))) return false;
            ++AreaAssets;
        }
        if (!Check(AreaAssets > 0, TEXT("Area filtering retains ordinary area and beam attacks."))) return false;
        Methods.FindChecked(0)->OnClicked.Broadcast();
        Test->AddInfo(TEXT("Verified live method-tab delegates, the complete five-chain catalog, exact unowned filtering and owned-list transfer/restoration; the transient probe neither changes authored assets nor saves a loadout."));
        return true;
    }

    bool PrepareSkill()
    {
        const FString Name = FPaths::GetCleanFilename(Samples[SampleIndex]);
        const FString Path = TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/") + Samples[SampleIndex] + TEXT(".") + Name;
        Skill.Reset(LoadObject<USkillDefinitionDataAsset>(nullptr, *Path));
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const FCombatRoundUnitView* Ally = Round->GetView().Units.FindByPredicate([](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive(); });
        const FCombatRoundUnitView* Enemy = Round->GetView().Units.FindByPredicate([](const FCombatRoundUnitView& Unit) { return Unit.bEnemy && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive(); });
        if (!Check(Skill.IsValid() && Ally && Enemy, TEXT("The representative authored skill and both teams are available."))) return false;
        FText Error;
        // Stabilize real-frame observation in this disposable combat without changing content or normal difficulty.
        // 콘텐츠나 정상 난이도를 변경하지 않고 폐기할 전투에서 실제 프레임 관찰을 안정화합니다.
        if (!Check(Round->SetDebugUnitHealth(Controller, Ally->UnitId, 10000.f, 10000.f, Error), FString::Printf(TEXT("Apply transient observation HP fixture: %s"), *Error.ToString()))) return false;
        Test->AddInfo(TEXT("The observation fixture retains the disposable debug ally's 10,000 HP; this is not a normal-difficulty or persistent Run test."));
        if (!Check(Skill->ResolveRoundSkill(Definition, Error) && Round->SetDebugUnitSkills(Controller, Ally->UnitId, {Skill.Get()}, Error), FString::Printf(TEXT("Equip authored skill %s: %s"), *Samples[SampleIndex], *Error.ToString()))) return false;
        if (!Check(CombatRoundRules::IsValidSkill(Definition) && Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Damage) && !Definition.Vfx.Niagara.IsNull(), TEXT("Each current representative uses its resolved authored target, collision, tags and Niagara profile."))) return false;
        FCombatRoundCommand Command;
        Command.UnitId = Ally->UnitId;
        Command.SkillId = Definition.SkillId;
        Command.TargetUnitId = Definition.TargetRule == ESkillTargetRule::EnemyTile ? INDEX_NONE : Enemy->UnitId;
        Command.TargetCoord = Enemy->HomeCoord;
        Command.DestinationCoord = Ally->HomeCoord;
        const FCombatRoundView& View = Round->GetView();
        SampleRound = View.RoundNumber;
        if (!Check(Round->SubmitPlan(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, Command, Error) && Round->SetParticipantReady(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, true, Error), FString::Printf(TEXT("Execute the authored command: %s"), *Error.ToString()))) return false;
        bSawActive = bSawReady = bFirstScreenshot = bMiddleScreenshot = false;
        bFirstCaptureCompleted = bMiddleCaptureCompleted = bCaptureFailed = false;
        MaxParticles = 0;
        MaxAge = FirstParticleAge = 0.f;
        FirstScreenshot = OutputDirectory / (Name + TEXT("_first.png"));
        MiddleScreenshot = OutputDirectory / (Name + TEXT("_middle.png"));
        return true;
    }

    void ObserveVfx()
    {
        int32 Particles = 0;
        float Age = 0.f;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* Component = *It;
            if (Component->GetWorld() != Controller->GetWorld() || !Component->GetAsset() || Component->GetAsset() != Definition.Vfx.Niagara.Get() || !Component->IsActive()) continue;
            bSawActive = true;
            bSawReady |= Component->GetAsset()->IsReadyToRun();
            const FNiagaraSystemInstanceControllerPtr Instance = Component->GetSystemInstanceController();
            if (!Instance || !Instance->IsValid()) continue;
            // Finish concurrent work before inspecting the emitter data; never advance simulation manually.
            // 이미 진행 중인 동시 작업을 마친 뒤 이미터 데이터를 읽고 시뮬레이션을 수동 진행하지 않습니다.
            Instance->WaitForConcurrentTickAndFinalize();
            Age = FMath::Max(Age, Instance->GetAge());
            for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetSystemInstance_Unsafe()->GetEmitters()) Particles += FMath::Max(Emitter->GetNumParticles(), 0);
        }
        for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
        {
            UParticleSystemComponent* Component = *It;
            if (Component->GetWorld() != Controller->GetWorld() || !Component->Template || Component->Template != Definition.Vfx.Cascade.Get() || !Component->IsActive()) continue;
            bSawActive = bSawReady = true;
            Particles += Component->GetNumActiveParticles();
            if (Particles > 0) Age = FMath::Max(Age, static_cast<float>(FPlatformTime::Seconds() - Started));
        }
        MaxParticles = FMath::Max(MaxParticles, Particles);
        MaxAge = FMath::Max(MaxAge, Age);
        if (Particles <= 0 || !PendingScreenshot.IsEmpty() || FScreenshotRequest::IsScreenshotRequested()) return;
        if (!bFirstScreenshot)
        {
            FirstParticleAge = Age;
            // Capture the rendered battlefield so the tool overlay cannot hide the effect under inspection.
            // 검사할 이펙트가 도구 패널에 가려지지 않도록 렌더링된 전장을 캡처합니다.
            bFirstScreenshot = QueueViewportCapture(FirstScreenshot, Age);
        }
        else if (!bMiddleScreenshot && Age - FirstParticleAge >= 0.1f)
        {
            bMiddleScreenshot = QueueViewportCapture(MiddleScreenshot, Age);
        }
    }

    bool QueueViewportCapture(const FString& Filename, float ParticleAge)
    {
        UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
        UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
        if (!Check(World && World->WorldType == EWorldType::PIE && IsValid(ViewportClient) && ViewportClient->GetWorld() == World && ViewportClient->Viewport, TEXT("Each capture request targets the live PIE game viewport.")))
        {
            bCaptureFailed = true;
            return false;
        }
        CaptureViewportClient = ViewportClient;
        ExpectedScreenshotSize = ViewportClient->Viewport->GetRenderTargetTextureSizeXY();
        PendingScreenshot = Filename;
        PendingParticleAge = ParticleAge;
        return true;
    }

    void CaptureRenderedViewport(FViewport* RenderedViewport)
    {
        if (PendingScreenshot.IsEmpty()) return;
        UGameViewportClient* ViewportClient = CaptureViewportClient.Get();
        if (!IsValid(ViewportClient) || RenderedViewport != ViewportClient->Viewport) return;
        UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
        const FIntPoint Size = RenderedViewport->GetRenderTargetTextureSizeXY();
        TArray<FColor> Pixels;
        // Read only the matching PIE render target after its draw event, excluding Slate UI and editor viewports.
        // Slate UI와 에디터 뷰포트를 제외하고 그리기 이벤트가 끝난 해당 PIE 렌더 타깃만 읽습니다.
        const bool bPixelsValid = World && World->WorldType == EWorldType::PIE && ViewportClient->GetWorld() == World && Size == ExpectedScreenshotSize && Size.X > 0 && Size.Y > 0 && GetViewportScreenShot(RenderedViewport, Pixels) && Pixels.Num() == int64(Size.X) * Size.Y;
        if (Check(bPixelsValid, TEXT("The capture event supplied the expected live PIE viewport and complete pixels at its requested resolution.")))
        {
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
            if (Check(!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *PendingScreenshot) && IFileManager::Get().FileSize(*PendingScreenshot) == Png.Num(), TEXT("The rendered PIE capture was fully saved before completion was published.")))
            {
                bFirstCaptureCompleted |= PendingScreenshot == FirstScreenshot;
                bMiddleCaptureCompleted |= PendingScreenshot == MiddleScreenshot;
                ++CompletedCaptures;
                Test->AddInfo(FString::Printf(TEXT("Rendered PIE capture: %s; viewport=%dx%d; particle observation age=%.3f; world=%s."), *PendingScreenshot, Size.X, Size.Y, PendingParticleAge, *World->GetName()));
            }
            else bCaptureFailed = true;
        }
        else bCaptureFailed = true;
        PendingScreenshot.Reset();
        CaptureViewportClient.Reset();
    }

    bool Check(bool bValue, const FString& Message) { return Test->TestTrue(Message, bValue); }
    void Advance(int32 Next) { Stage = Next; Started = FPlatformTime::Seconds(); }
    bool End() { GEditor->RequestEndPlayMap(); Advance(9); return false; }
    FAutomationTestBase* Test;
    ACombatDebugPlayerController* Controller = nullptr;
    ACombatDebugGameMode* Mode = nullptr;
    AUnitBase* RevivedUnit = nullptr;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TStrongObjectPtr<USkillDefinitionDataAsset> Skill;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> OriginalSkills;
    FRunPartyMember OriginalEquipment;
    FTransform AliveMeshTransform;
    FCombatRoundSkill Definition;
    TArray<FString> Samples{TEXT("__AoeVFX/DA_DrGame_AoeVFX_AOE_BlazeBlast"), TEXT("ProjectileHitVFX/DA_DrGame_ProjectileHitVFX_Arrow"), TEXT("SlashHitVFX/DA_DrGame_SlashHitVFX_Slash_Katana"), TEXT("___LinkChainVFX/DA_DrGame_LinkChainVFX_Link_Electric")};
    FString OutputDirectory;
    FString FirstScreenshot;
    FString MiddleScreenshot;
    FString PendingScreenshot;
    TWeakObjectPtr<UGameViewportClient> CaptureViewportClient;
    FDelegateHandle ViewportRenderedHandle;
    FIntPoint ExpectedScreenshotSize = FIntPoint::ZeroValue;
    int32 Stage = 0;
    int32 SampleIndex = 0;
    int32 SampleRound = 0;
    int32 MaxParticles = 0;
    int32 CompletedCaptures = 0;
    double Started = 0.0;
    float MaxAge = 0.f;
    float FirstParticleAge = 0.f;
    float PendingParticleAge = 0.f;
    bool bSawActive = false;
    bool bSawReady = false;
    bool bFirstScreenshot = false;
    bool bMiddleScreenshot = false;
    bool bFirstCaptureCompleted = false;
    bool bMiddleCaptureCompleted = false;
    bool bCaptureFailed = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatDebugAuthoredPIETest, "ProjectA.CombatDebugPIE.AuthoredToolsAndVfx", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatDebugAuthoredPIETest::RunTest(const FString& Parameters)
{
    if (!GEditor || !GEngine || !FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("Authored VFX PIE verification requires an editor with a real rendering device."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            AddError(TEXT("End the existing PIE session before running isolated debug verification."));
            return false;
        }
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatDebugPIE::FAuthoredToolsAndVfx>(this));
    return true;
}

#endif
