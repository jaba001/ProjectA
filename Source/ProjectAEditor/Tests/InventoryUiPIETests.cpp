#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SObjectWidget.h"
#include "Slate/UMGDragDropOp.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Gameplay/CharacterEquipmentPanel.h"
#include "UI/Gameplay/CharacterInventoryPanel.h"
#include "UI/Gameplay/EquipmentDragDropOperation.h"
#include "UI/Gameplay/EquipmentItemSlotWidget.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/GameplayRootWidget.h"
#include "UI/Gameplay/InventoryWidget.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace InventoryUiPIE
{
template <typename T>
T* Active(UWorld* World)
{
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        T* Typed = Cast<T>(Widget);
        if (Typed && Typed->IsActivated()) return Typed;
    }
    return nullptr;
}

template <typename T>
T* Child(UUserWidget* Parent)
{
    T* Result = nullptr;
    if (Parent && Parent->WidgetTree) Parent->WidgetTree->ForEachWidget([&Result](UWidget* Widget) { if (!Result) Result = Cast<T>(Widget); });
    return Result;
}

void Texts(UWidget* Widget, TArray<FString>& Result)
{
    if (!Widget) return;
    if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Result.Add(Text->GetText().ToString());
    if (UUserWidget* User = Cast<UUserWidget>(Widget))
    {
        if (User->WidgetTree) Texts(User->WidgetTree->RootWidget, Result);
    }
    else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
    {
        for (UWidget* Item : Panel->GetAllChildren()) Texts(Item, Result);
    }
}

FString Text(UWidget* Widget)
{
    TArray<FString> Values;
    Texts(Widget, Values);
    return FString::Join(Values, TEXT("\n"));
}

// Preserve both runtime configuration streams and exact original files while preventing review-only video preferences from being written.
// 검수 전용 화면 설정의 파일 기록을 차단하고 런타임 설정 스트림과 원본 파일 바이트를 모두 보존합니다.
struct FConfigSnapshot
{
    FString Filename;
    FConfigFile InMemory;
    FConfigCommandStream Saved;
    FConfigCommandStream Runtime;
    TArray<uint8> Bytes;
    bool bExisted = false;
};

// Drive saved screens and Slate input while keeping all disposable inventory data in an isolated Run save.
// 저장된 화면과 Slate 입력을 사용하며 일회성 인벤토리 데이터는 격리된 Run 저장에만 보관합니다.
class FInventoryReview : public IAutomationLatentCommand
{
public:
    FInventoryReview(FAutomationTestBase* InTest, FString InSlot, FIntPoint InSize) : Test(InTest), Slot(MoveTemp(InSlot)), Size(InSize)
    {
        Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TodoReview/InventoryUI") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*Output, true);
    }

    virtual ~FInventoryReview() override
    {
        RestoreInventoryPointer();
        RestoreOptionsReviewSettings();
        RestoreOriginalPreferences();
        ReleaseRetainedViewport();
    }

    virtual bool Update() override
    {
        if (Started == 0.0) Started = StageStarted = FPlatformTime::Seconds();
        if (Stage == 99)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE)
                {
                    if (FPlatformTime::Seconds() - StageStarted < 30.0) return false;
                    Test->AddError(TEXT("Inventory review PIE did not close within 30 seconds; its isolated save was preserved."));
                    return true;
                }
            }
            ReleaseRetainedViewport();
            RestoreOriginalPreferences();
            if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Only the disposable inventory review save is deleted after PIE closes."));
            return true;
        }
        if (FPlatformTime::Seconds() - Started > 180.0 || FPlatformTime::Seconds() - StageStarted > 45.0)
        {
            Test->AddError(FString::Printf(TEXT("Inventory UI review timed out at stage %d, viewport %dx%d, stageSeconds=%.3f totalSeconds=%.3f videoStage=%d videoFrames=%d options=%s active=%d system=%dx%d mode=%d."), Stage, Size.X, Size.Y, FPlatformTime::Seconds() - StageStarted, FPlatformTime::Seconds() - Started, VideoReviewStage, VideoReviewFrames, *GetNameSafe(VideoOptions.Get()), VideoOptions.IsValid() && VideoOptions->IsActivated(), GSystemResolution.ResX, GSystemResolution.ResY, static_cast<int32>(GSystemResolution.WindowMode)));
            return End();
        }
        if (Stage == 0)
        {
            Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            Settings->SetPlayNetMode(PIE_Standalone);
            Settings->SetPlayNumberOfClients(1);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;
            Settings->NewWindowWidth = Size.X;
            Settings->NewWindowHeight = Size.Y;
            Settings->SetClientWindowSize(Size);
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = Settings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/MainMenu");
            GEditor->RequestPlaySession(Params);
            Advance(1);
            return false;
        }
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->GetFirstPlayerController()) return false;
        if (Stage == 1)
        {
            UMainMenuScreenWidget* Menu = Active<UMainMenuScreenWidget>(World);
            if (!Menu) return false;
            if (!Click(Menu, TEXT("Button_NewGame"))) return End();
            Advance(2);
            return false;
        }
        if (Stage == 2)
        {
            UGameModeSelectionWidget* Selection = Active<UGameModeSelectionWidget>(World);
            if (!Selection) return false;
            if (!Click(Selection, TEXT("Button_SinglePlayer"))) return End();
            Advance(3);
            return false;
        }
        if (Stage == 3)
        {
            UCharacterCreationWidget* Creation = Active<UCharacterCreationWidget>(World);
            if (!Creation) return false;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                if (!Click(Creation, FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index)))) return End();
                UWidget* Details = Creation->GetWidgetFromName(TEXT("ProfessionDetailPanel"));
                const TArray<FRunPartyMember> Members = Creation->GetPartyMembers();
                if (!Check(Details && Details->GetVisibility() == ESlateVisibility::Collapsed && Members.IsValidIndex(Index) && Members[Index].bCreated && !Members[Index].CharacterName.IsEmpty(), TEXT("Each real Create button immediately commits its character while the optional editor stays closed."))) return End();
            }
            if (!Click(Creation, TEXT("Button_Slot0_PlayerControl"))) return End();
            if (!Check(Creation->GetPartyMembers().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled; }).Num() == 1, TEXT("The existing party retains one directly controlled character and three AI companions."))) return End();
            if (!Click(Creation, TEXT("Button_StartGame"))) return End();
            Advance(4);
            return false;
        }
        Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
        if (!Controller.IsValid() || !Controller->GetGameInstance()) return false;
        URunStateSubsystem* Run = Controller->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        if (!Run) return false;
        if (Stage == 4)
        {
            if (!Active<URunMapWidget>(World) || Run->GetPhase() != ERunPhase::Map) return false;
            if (!PrepareViewport()) return bViewportFailed ? End() : false;
            if (!InstallFixture(Run)) return End();
            SendKey(EKeys::I);
            Advance(16);
            return false;
        }
        if (Stage == 16)
        {
            UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
            if (!Inventory || !Warm()) return false;
            if (!Check(Run->GetPhase() == ERunPhase::Map, TEXT("The outside-shop rejection uses the actual Run map phase."))) return End();
            if (!VerifyRejectedChange(Inventory, Run, CharacterId, DuplicateIndex, TEXT("InventoryOutsideShop"), false)) return End();
            SendKey(EKeys::I);
            Advance(17);
            return false;
        }
        if (Stage == 17)
        {
            if (Active<UInventoryWidget>(World)) return false;
            Advance(5);
            return false;
        }
        if (Stage == 5)
        {
            // Prepare a disposable completed encounter through public Run transitions; this is not combat gameplay coverage.
            // 공개 Run 전이로 일회성 완료 인카운터를 준비하며 전투 플레이 검증으로 기록하지 않습니다.
            if (!Check(!Run->GetNodes().IsEmpty() && Run->BeginEncounter(Run->GetNodes()[0].NodeId) && Run->MarkCombatStarted() && Run->CompleteEncounter(ECombatResult::Victory), TEXT("Public Run state transitions publish the isolated inventory review result."))) return End();
            FText Error;
            for (const FGuid& Recipient : Run->GetGoldRewardRecipientIds())
            {
                const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([Recipient](const FRunPartyMember& Item) { return Item.CharacterId == Recipient; });
                if (!Check(Member && Run->SelectGoldReward(Member->OwnerAccountId, Recipient, Run->GetCurrentNodeId(), 0, Error), TEXT("The disposable result claims the actual Run reward before Continue."))) return End();
            }
            Controller->RequestContinueRun();
            if (!Check(Run->GetPhase() == ERunPhase::EncounterChoice, TEXT("The real controller Continue request reaches the shop choice phase."))) return End();
            Advance(6);
            return false;
        }
        if (Stage == 6)
        {
            URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
            if (!Screen) return false;
            UVerticalBox* Choices = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("EncounterActions")));
            UButton* ItemShop = Choices && Choices->GetChildrenCount() > 1 ? Cast<UButton>(Choices->GetChildAt(1)) : nullptr;
            if (!Check(ItemShop && ItemShop->GetIsEnabled(), TEXT("The actual encounter screen exposes its item shop choice."))) return End();
            ItemShop->OnClicked.Broadcast();
            if (!Check(Run->GetPhase() == ERunPhase::Shop && Run->GetEncounterProgress().SelectedEncounterId == FRunItemShopState::GetEncounterId(), TEXT("The real shop button selects Shop_02 without changing ownership."))) return End();
            Advance(7);
            return false;
        }
        if (Stage == 7)
        {
            URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
            if (!Screen || !Warm()) return false;
            if (!Check(Run->GetItemShopState().Catalog.Num() == 289 && Run->GetItemShopState().Offers.Num() == 5, TEXT("The authored item shop loads all 289 CSV definitions and five actual offers."))) return End();
            if (!Capture(TEXT("Shop"))) return End();
            if (!Purchase(Screen, Run)) return End();
            SendKey(EKeys::I);
            Advance(8);
            return false;
        }
        if (Stage == 8)
        {
            UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
            if (!Inventory || !Warm()) return false;
            UCharacterInventoryPanel* Panel = Child<UCharacterInventoryPanel>(Inventory);
            if (!bInventoryCatalogReviewed)
            {
                if (!CheckCatalogAndTabs(Panel, Run) || !Capture(TEXT("InventoryAll"))) return End();
                bInventoryCatalogReviewed = true;
            }
            if (!ReviewPointerEquip(Inventory, Run)) return bPointerReviewFailed ? End() : false;
            const FRunPartyMember* Member = FindMember(Run);
            if (!Check(Member && RunEquipmentRules::FindItemIndexAtSlot(*Member, URunEquipmentCatalog::GetWeaponSlot(0)) == DuplicateIndex && !RunEquipmentRules::IsItemEquipped(*Member, OriginalIndex) && Member->Items.Num() == Catalog.Num() + 2, TEXT("Equipping a duplicate changes only its stable inventory index and preserves the original copy and item count."))) return End();
            Advance(9);
            return false;
        }
        if (Stage == 9)
        {
            UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
            if (!Inventory || !Warm()) return false;
            UCharacterInventoryPanel* Panel = Child<UCharacterInventoryPanel>(Inventory);
            UCharacterEquipmentPanel* Equipment = Child<UCharacterEquipmentPanel>(Inventory);
            UEquipmentItemSlotWidget* Source = Child<UEquipmentItemSlotWidget>(Equipment);
            UVerticalBox* Rows = Panel ? Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("InventoryItems"))) : nullptr;
            if (!Check(Rows && Rows->GetChildrenCount() == Catalog.Num() + 1, TEXT("The equipped duplicate disappears from the bag while its separate copy remains."))) return End();
            if (!Capture(TEXT("InventoryEquipped"))) return End();
            if (!Check(Source && Panel && Drop(Source, Panel, Run, DuplicateIndex), TEXT("The actual inventory panel NativeOnDrop returns the selected equipped copy to the bag."))) return End();
            const FRunPartyMember* Member = FindMember(Run);
            if (!Check(Member && !RunEquipmentRules::IsItemEquipped(*Member, DuplicateIndex) && Member->Items.Num() == Catalog.Num() + 2, TEXT("Unequipping preserves both individual copies and the append-only inventory."))) return End();
            SendKey(EKeys::O);
            Advance(10);
            return false;
        }
        if (Stage == 10)
        {
            UOptionsWidget* Options = VideoReviewStage > 0 ? VideoOptions.Get() : Active<UOptionsWidget>(World);
            if (VideoReviewStage > 0 && !Check(Options != nullptr, TEXT("The active O video review retains its real options widget and PIE session after routed input."))) return End();
            if (!Options || !Warm()) return false;
            UGameplayRootWidget* Root = FindRoot(World);
            UWidget* Layer = Root ? Root->GetWidgetFromName(TEXT("RunLayer")) : nullptr;
            if (!Check(!Active<UInventoryWidget>(World) && Root && Root->IsUtilityMenuOpen() && Layer && !Layer->GetIsEnabled(), TEXT("Actual O input replaces inventory with options and disables the underlying Run layer."))) return End();
            if (!bOptionsReplacementReviewed)
            {
                if (!Capture(TEXT("OptionsFromInventory"))) return End();
                SendKey(EKeys::I);
                if (!Check(Options->IsActivated() && !Active<UInventoryWidget>(World), TEXT("I cannot open inventory behind the active options screen."))) return End();
                bOptionsReplacementReviewed = true;
            }
            if (!ReviewVideoCancellation(Options, EKeys::O)) return bVideoReviewFailed ? End() : false;
            Advance(11);
            return false;
        }
        if (Stage == 11)
        {
            if (Active<UOptionsWidget>(World)) return false;
            SendKey(EKeys::I);
            Advance(12);
            return false;
        }
        if (Stage == 12)
        {
            if (!Active<UInventoryWidget>(World)) return false;
            SendKey(EKeys::I);
            Advance(13);
            return false;
        }
        if (Stage == 13)
        {
            if (Active<UInventoryWidget>(World)) return false;
            SendKey(EKeys::O);
            Advance(14);
            return false;
        }
        if (Stage == 14)
        {
            UOptionsWidget* Options = VideoReviewStage > 0 ? VideoOptions.Get() : Active<UOptionsWidget>(World);
            if (VideoReviewStage > 0 && !Check(Options != nullptr, TEXT("The active Esc video review retains its real options widget and PIE session after routed input."))) return End();
            if (!Options || !Warm()) return false;
            if (!ReviewVideoCancellation(Options, EKeys::Escape)) return bVideoReviewFailed ? End() : false;
            Advance(15);
            return false;
        }
        if (Stage == 15)
        {
            if (Active<UOptionsWidget>(World)) return false;
            UGameplayRootWidget* Root = FindRoot(World);
            UWidget* Layer = Root ? Root->GetWidgetFromName(TEXT("RunLayer")) : nullptr;
            if (!Check(Root && !Root->IsUtilityMenuOpen() && Layer && Layer->GetIsEnabled(), TEXT("Actual Esc closes gameplay options and restores the underlying Run layer."))) return End();
            SendKey(EKeys::I);
            Advance(18);
            return false;
        }
        if (Stage == 18)
        {
            UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
            if (!Inventory) return false;
            // Query the original AI character through the public widget API without transferring human control or ownership.
            // 인간 조작이나 소유권을 이전하지 않고 공개 위젯 API로 원래 AI 캐릭터를 조회합니다.
            Inventory->RefreshInventory(FGameplayViewState::FromRun(Run, FText::GetEmpty()), ReadOnlyCharacterId);
            Advance(19);
            return false;
        }
        if (Stage == 19)
        {
            UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
            if (!Inventory || !Warm()) return false;
            if (!VerifyRejectedChange(Inventory, Run, ReadOnlyCharacterId, ReadOnlyItemIndex, TEXT("InventoryReadOnlyAI"), false)) return End();
            SendKey(EKeys::I);
            Advance(20);
            return false;
        }
        if (Stage == 20)
        {
            if (Active<UInventoryWidget>(World)) return false;
            if (!InstallDeadOwnerFixture(Run)) return End();
            SendKey(EKeys::I);
            Advance(21);
            return false;
        }
        if (Stage == 21)
        {
            UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
            if (!Inventory || !Warm()) return false;
            if (!Check(Run->GetPhase() == ERunPhase::Shop, TEXT("The dead-owner rejection retains the actual shop phase and surviving companions."))) return End();
            if (!VerifyRejectedChange(Inventory, Run, CharacterId, DuplicateIndex, TEXT("InventoryDeadOwner"), true)) return End();
            SendKey(EKeys::I);
            Advance(22);
            return false;
        }
        if (Stage == 22)
        {
            if (Active<UInventoryWidget>(World)) return false;
            Test->AddInfo(TEXT("Inventory UI screenshots: ") + Output + TEXT(". Rejections cover actual Map, dead owner in Shop and an original AI read-only widget query. Equip uses actual Slate cursor drag routing; unequip/rejection drops are synthetic. Combat result and death were isolated state fixtures; full combat and spectator networking were not exercised. O/Esc verify actual preferences, Run input blocking and native PIE restoration with original config writes disabled; option-file persistence is separate game-test evidence."));
            return End();
        }
        return false;
    }

private:
    bool Check(bool Value, const FString& Message)
    {
        return Test->TestTrue(*Message, Value);
    }

    void Advance(int32 Next)
    {
        Stage = Next;
        StageStarted = FPlatformTime::Seconds();
        Frames = 0;
    }

    bool Warm()
    {
        return ++Frames >= 8 && FPlatformTime::Seconds() - StageStarted >= 0.5 && FAssetCompilingManager::Get().GetNumRemainingAssets() == 0;
    }

    bool End()
    {
        RetainViewport();
        RestoreInventoryPointer();
        RestoreOptionsReviewSettings();
        GEditor->RequestEndPlayMap();
        Advance(99);
        return false;
    }

    // Keep a game-thread owner while PIE removes its window so queued Slate draws cannot destroy the viewport on the render thread.
    // PIE가 창을 제거하는 동안 게임 스레드 소유 참조를 유지하여 대기 중인 Slate 렌더 작업이 뷰포트를 렌더 스레드에서 파괴하지 않게 합니다.
    void RetainViewport()
    {
        check(IsInGameThread());
        if (RetainedViewport.IsValid() || !GEditor || !GEditor->PlayWorld) return;
        UGameViewportClient* Viewport = GEditor->PlayWorld->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
    }

    // Drain queued Slate work after teardown, then release the last fixture reference on the required game thread.
    // 종료 후 대기 중인 Slate 작업을 비운 뒤 검수 참조를 필수 게임 스레드에서 해제합니다.
    void ReleaseRetainedViewport()
    {
        check(IsInGameThread());
        if (!RetainedViewport.IsValid()) return;
        FlushRenderingCommands();
        RetainedViewport.Reset();
    }

    bool Click(UUserWidget* Screen, FName Name)
    {
        UButton* Button = Screen ? Cast<UButton>(Screen->GetWidgetFromName(Name)) : nullptr;
        if (!Check(Button && Button->GetIsEnabled(), TEXT("The saved screen exposes an enabled actual button: ") + Name.ToString())) return false;
        Button->OnClicked.Broadcast();
        return true;
    }

    const FRunPartyMember* FindMember(const URunStateSubsystem* Run) const
    {
        return Run->GetPartyMembers().FindByPredicate([this](const FRunPartyMember& Member) { return Member.CharacterId == CharacterId; });
    }

    UGameplayRootWidget* FindRoot(UWorld* World) const
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UGameplayRootWidget::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (Widget->GetOwningPlayer() == Controller.Get() && Widget->IsInViewport()) return Cast<UGameplayRootWidget>(Widget);
        return nullptr;
    }

    bool InstallFixture(URunStateSubsystem* Run)
    {
        FText Error;
        if (!Check(RunItemShopCatalog::Load(Catalog, Error) && Catalog.Num() == 289, TEXT("The real runtime CSV loader reads all 289 catalog definitions."))) return false;
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
        FRunPartyMember* Member = Saved.IsValid() ? Saved->Party.FindByPredicate([](const FRunPartyMember& Item) { return Item.bCreated && Item.bPlayerControlled; }) : nullptr;
        if (!Check(Member && Saved->Party.FilterByPredicate([](const FRunPartyMember& Item) { return Item.bCreated; }).Num() == 4, TEXT("The newly created Run durably retains the real four-character party."))) return false;
        for (FRunPartyMember& Companion : Saved->Party) if (Companion.bCreated && Companion.CurrentHP < 0.f) Companion.CurrentHP = 1000.f;
        CharacterId = Member->CharacterId;
        const FGameplayTag MainHand = URunEquipmentCatalog::GetWeaponSlot(0);
        for (int32 Index = 0; Index < Catalog.Num(); ++Index)
        {
            const FRunEquipmentProfile* Profile = URunEquipmentCatalog::Get().ResolveProfile(Catalog[Index]);
            if (Profile && URunEquipmentCatalog::ResolveSlot(*Profile, MainHand) == MainHand && URunEquipmentCatalog::GetOccupiedSlots(*Profile, MainHand).Num() == 1)
            {
                OriginalIndex = Index;
                break;
            }
        }
        if (!Check(OriginalIndex != INDEX_NONE, TEXT("The authored equipment catalog provides a one-handed item for duplicate-copy verification."))) return false;
        Member->Items = Catalog;
        DuplicateIndex = Member->Items.Add(Catalog[OriginalIndex]);
        Member->Equipment = FRunEquipmentState();
        Member->Equipment.bHasLoadout = true;
        Member->Gold = 1000;
        Member->CurrentHP = 1000.f;
        Member->Skills.AddUnique(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack")));
        FRunPartyMember* ReadOnlyMember = Saved->Party.FindByPredicate([](const FRunPartyMember& Item) { return Item.bCreated && !Item.bPlayerControlled; });
        if (!Check(ReadOnlyMember != nullptr, TEXT("The original party retains a distinct AI companion for read-only UI inspection."))) return false;
        ReadOnlyCharacterId = ReadOnlyMember->CharacterId;
        ReadOnlyItemIndex = ReadOnlyMember->Items.Add(Catalog[OriginalIndex]);
        if (!Check(UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0) && Run->LoadCheckpoint(Error), TEXT("Disposable items and the existing melee skill survive actual save/load in the guarded slot."))) return false;
        return Check(FindMember(Run) && FindMember(Run)->Items.Num() == 290, TEXT("Reload retains every CSV definition plus one separate duplicate copy."));
    }

    bool Purchase(URunEncounterWidget* Screen, URunStateSubsystem* Run)
    {
        const TArray<FRunItemShopOffer> BeforeOffers = Run->GetItemShopState().Offers;
        UVerticalBox* Actions = Screen ? Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("ShopActions"))) : nullptr;
        if (!Check(Actions && BeforeOffers.Num() == 5 && Actions->GetChildrenCount() == BeforeOffers.Num(), TEXT("The actual merchant has one card for each original item offer in its displayed order."))) return false;
        const FRunItemShopOffer Offer = BeforeOffers[0];
        UWidget* Card = Actions->GetChildAt(0);
        TArray<FString> CardLabels;
        Texts(Card, CardLabels);
        const FString Price = FText::AsNumber(Offer.Item.Price).ToString() + TEXT("G");
        const bool bCatalogItem = Catalog.ContainsByPredicate([&Offer](const FRunItemDefinition& Item) { return RunItemShopCatalog::IsSameDefinition(Item, Offer.Item); });
        if (!Check(Card && Card->IsVisible() && bCatalogItem && CardLabels.Contains(Offer.Item.DisplayName.ToString()) && CardLabels.Contains(Price), TEXT("The original offer's actual merchant card displays its exact CSV name and price beside the purchase button."))) return false;
        UGameplayActionButton* Product = nullptr;
        int32 ButtonCount = 0;
        Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            UGameplayActionButton* Button = Cast<UGameplayActionButton>(Widget);
            if (!Button) return;
            for (UWidget* Parent = Button->GetParent(); Parent; Parent = Parent->GetParent())
            {
                if (Parent != Card) continue;
                Product = Button;
                ++ButtonCount;
                break;
            }
        });
        if (!Check(ButtonCount == 1 && Product && Product->IsVisible() && Product->GetIsEnabled() && Product->GetToolTipText().ToString() == Offer.Item.DisplayName.ToString(), TEXT("The exact displayed CSV offer has one actual enabled purchase button with its matching item tooltip."))) return false;
        const int32 Gold = FindMember(Run)->Gold;
        FName ClickedOffer;
        int32 ActionCount = 0;
        const FDelegateHandle Observer = Product->OnActionRequested.AddLambda([&ClickedOffer, &ActionCount](FName OfferId) { ClickedOffer = OfferId; ++ActionCount; });
        Product->OnClicked.Broadcast();
        Product->OnActionRequested.Remove(Observer);
        if (!Check(ActionCount == 1 && ClickedOffer == Offer.OfferId, TEXT("The actual purchase button dispatches its original offer identifier exactly once."))) return false;
        const FRunPartyMember* Member = FindMember(Run);
        const FRunItemShopOffer* Updated = Run->GetItemShopState().Offers.FindByPredicate([&Offer](const FRunItemShopOffer& Item) { return Item.OfferId == Offer.OfferId; });
        bool bOtherOffersUnchanged = Run->GetItemShopState().Offers.Num() == BeforeOffers.Num();
        for (const FRunItemShopOffer& Before : BeforeOffers)
        {
            const FRunItemShopOffer* After = Run->GetItemShopState().Offers.FindByPredicate([&Before](const FRunItemShopOffer& Item) { return Item.OfferId == Before.OfferId; });
            bOtherOffersUnchanged &= After && RunItemShopCatalog::IsSameDefinition(After->Item, Before.Item) && (Before.OfferId == Offer.OfferId || After->bSold == Before.bSold);
        }
        return Check(Member && Member->Items.Num() == 291 && RunItemShopCatalog::IsSameDefinition(Member->Items.Last(), Offer.Item) && Member->Gold == Gold - Offer.Item.Price && Updated && Updated->bSold && bOtherOffersUnchanged, TEXT("The actual merchant click appends its item, charges its CSV price and marks only its offer sold."));
    }

    bool CheckCatalogAndTabs(UCharacterInventoryPanel* Panel, URunStateSubsystem* Run)
    {
        UHorizontalBox* Tabs = Panel ? Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("InventoryCategoryTabs"))) : nullptr;
        UVerticalBox* Rows = Panel ? Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("InventoryItems"))) : nullptr;
        UVerticalBox* Skills = Panel ? Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("InventorySkills"))) : nullptr;
        UWidget* Details = Panel ? Panel->GetWidgetFromName(TEXT("SelectedInventoryItem")) : nullptr;
        if (!Check(Tabs && Tabs->GetChildrenCount() == 6 && Rows && Rows->GetChildrenCount() == 291 && Skills && Details, TEXT("Actual I opens all six inventory tabs and preserves 291 individual unequipped rows."))) return false;
        bool bNames = true;
        bool bPrices = true;
        bool bSupport = true;
        for (int32 Index = 0; Index < Catalog.Num(); ++Index)
        {
            UEquipmentItemSlotWidget* Row = Cast<UEquipmentItemSlotWidget>(Rows->GetChildAt(Index));
            if (!Row) return Check(false, TEXT("Every CSV item has an actual inventory row."));
            Row->ItemSelected.ExecuteIfBound(Index);
            const FString Detail = Text(Details);
            bNames &= Text(Row).Contains(Catalog[Index].DisplayName.ToString()) && Detail.Contains(Catalog[Index].DisplayName.ToString());
            bPrices &= Detail.Contains(TEXT("카탈로그 기준 가격 ") + FText::AsNumber(Catalog[Index].Price).ToString() + TEXT("G"));
            bSupport &= URunEquipmentCatalog::Get().ResolveProfile(Catalog[Index]) ? Detail.Contains(TEXT("장착 위치:")) : Detail.Contains(TEXT("장착을 지원하지"));
        }
        if (!Check(bNames && bPrices && bSupport, TEXT("All 289 actual row selection delegates display their CSV name, reference price and authored equipment support."))) return false;
        int32 PartitionTotal = 0;
        for (int32 Index = 1; Index < 5; ++Index)
        {
            UButton* Tab = Cast<UButton>(Tabs->GetChildAt(Index));
            if (!Check(Tab && Tab->GetIsEnabled(), TEXT("Each item category tab has its actual enabled button."))) return false;
            Tab->OnClicked.Broadcast();
            const int32 Count = Rows->GetChildrenCount();
            PartitionTotal += Count;
            TArray<FString> Labels;
            Texts(Tab, Labels);
            if (!Check(Labels.Contains(FText::AsNumber(Count).ToString()), TEXT("The category counter matches its actual individual-copy rows."))) return false;
        }
        if (!Check(PartitionTotal == 291, TEXT("Weapon shield ammo and other tab rows partition every bag copy exactly once."))) return false;
        CastChecked<UButton>(Tabs->GetChildAt(5))->OnClicked.Broadcast();
        if (!Check(Skills->GetChildrenCount() == FindMember(Run)->Skills.Num() && Text(Skills).Contains(TEXT("근접 공격")), TEXT("The actual skill tab matches the saved loadout and displays the preserved melee skill name."))) return false;
        CastChecked<UButton>(Tabs->GetChildAt(0))->OnClicked.Broadcast();
        UEquipmentItemSlotWidget* Duplicate = Cast<UEquipmentItemSlotWidget>(Rows->GetChildAt(DuplicateIndex));
        if (!Check(Duplicate != nullptr, TEXT("The appended duplicate remains a separate selectable row."))) return false;
        FSlateApplication::Get().SetKeyboardFocus(Duplicate->TakeWidget(), EFocusCause::SetDirectly);
        SendKey(EKeys::Enter, false);
        return Check(Text(Details).Contains(Catalog[OriginalIndex].DisplayName.ToString()), TEXT("Actual Slate Enter on the duplicate row updates the real detail panel."));
    }

    void SendKey(FKey Key, bool bFocusViewport = true)
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        if (bFocusViewport)
        {
            UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (Widget.IsValid())
            {
                const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Widget.ToSharedRef());
                if (Window.IsValid()) Window->BringToFront(true);
                Slate.SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
            }
        }
        TSharedPtr<FUICommandInfo> StopCommand;
        FInputChord PrimaryBefore;
        FInputChord SecondaryBefore;
        bool bRestorePrimary = false;
        bool bRestoreSecondary = false;
        // CommonUI intentionally lets the editor's StopPlaySession chord escape gameplay routing in PIE; suspend only that chord for this input.
        // CommonUI가 PIE에서 에디터 StopPlaySession 단축키를 게임 입력보다 먼저 허용하므로 이번 입력 동안 해당 단축키만 해제합니다.
        if (Key == EKeys::Escape)
        {
            StopCommand = FInputBindingManager::Get().FindCommandInContext(TEXT("PlayWorld"), TEXT("StopPlaySession"));
            if (!Check(bPreferencesCaptured && GConfig && GConfig->AreFileOperationsDisabled() && StopCommand.IsValid(), TEXT("A real PIE Escape input has a scoped original-preference snapshot, suspended config writes and the official editor stop command."))) return;
            PrimaryBefore = *StopCommand->GetActiveChord(EMultipleKeyBindingIndex::Primary);
            SecondaryBefore = *StopCommand->GetActiveChord(EMultipleKeyBindingIndex::Secondary);
            bRestorePrimary = PrimaryBefore == FInputChord(EKeys::Escape);
            bRestoreSecondary = SecondaryBefore == FInputChord(EKeys::Escape);
            if (bRestorePrimary) StopCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
            if (bRestoreSecondary) StopCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Secondary);
        }
        ON_SCOPE_EXIT
        {
            if (StopCommand.IsValid())
            {
                if (bRestorePrimary) StopCommand->SetActiveChord(PrimaryBefore, EMultipleKeyBindingIndex::Primary);
                if (bRestoreSecondary) StopCommand->SetActiveChord(SecondaryBefore, EMultipleKeyBindingIndex::Secondary);
                Check(*StopCommand->GetActiveChord(EMultipleKeyBindingIndex::Primary) == PrimaryBefore && *StopCommand->GetActiveChord(EMultipleKeyBindingIndex::Secondary) == SecondaryBefore, TEXT("The routed Escape immediately restores both original active editor stop chords without saving input bindings."));
            }
        };
        if (StopCommand.IsValid() && !Check(!StopCommand->HasActiveChord(FInputChord(EKeys::Escape)), TEXT("The actual plain Escape reaches gameplay CommonUI while the editor stop chord is scoped out."))) return;
        const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
        Slate.ProcessKeyDownEvent(Event);
        Slate.ProcessKeyUpEvent(Event);
    }

    bool Drop(UUserWidget* Source, UUserWidget* Target, URunStateSubsystem* Run, int32 ItemIndex, FGuid SubjectId = FGuid())
    {
        const FGuid ResolvedId = SubjectId.IsValid() ? SubjectId : CharacterId;
        const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([ResolvedId](const FRunPartyMember& Item) { return Item.CharacterId == ResolvedId; });
        if (!Source || !Target || !Member) return false;
        TStrongObjectPtr<UEquipmentDragDropOperation> Payload(NewObject<UEquipmentDragDropOperation>());
        Payload->CharacterId = ResolvedId;
        Payload->ItemIndex = ItemIndex;
        Payload->ExpectedRevision = Member->Equipment.Revision;
        const FVector2D Position = Target->GetCachedGeometry().GetAbsolutePosition();
        const FPointerEvent Pointer(0, Position, Position, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
        const TSharedRef<FUMGDragDropOp> Operation = FUMGDragDropOp::New(Payload.Get(), 0, Position, Source->GetCachedGeometry().GetAbsolutePosition(), 1.f, StaticCastSharedRef<SObjectWidget>(Source->TakeWidget()));
        const FReply Reply = Target->TakeWidget()->OnDrop(Target->GetCachedGeometry(), FDragDropEvent(Pointer, Operation));
        return Reply.IsEventHandled();
    }

    // Observe rejected UI and controller requests using authoritative data, revisions and the exact guarded save bytes.
    // 권위 데이터·버전·격리된 저장 바이트로 UI 및 컨트롤러 요청의 변경 거절을 관찰합니다.
    bool VerifyRejectedChange(UInventoryWidget* Inventory, URunStateSubsystem* Run, FGuid SubjectId, int32 ItemIndex, const FString& CaptureName, bool bDeadOwner)
    {
        const FRunPartyMember* Subject = Run->GetPartyMembers().FindByPredicate([SubjectId](const FRunPartyMember& Item) { return Item.CharacterId == SubjectId; });
        if (!Check(Subject && Subject->Items.IsValidIndex(ItemIndex) && !RunEquipmentRules::IsItemEquipped(*Subject, ItemIndex), TEXT("The rejected request selects a real owned and unequipped item copy."))) return false;
        const FRunPartyMember SubjectBefore = *Subject;
        const FGameplayViewState View = FGameplayViewState::FromRun(Run, FText::GetEmpty());
        if (!Check(!Controller->CanChangeEquipment(View, SubjectId) && (bDeadOwner ? SubjectBefore.CurrentHP == 0.f : SubjectBefore.CurrentHP > 0.f), TEXT("The actual presentation derives its read-only state from the authoritative phase, life state and control permission."))) return false;
        UCharacterInventoryPanel* Panel = Child<UCharacterInventoryPanel>(Inventory);
        UCharacterEquipmentPanel* Equipment = Child<UCharacterEquipmentPanel>(Inventory);
        UEquipmentItemSlotWidget* Target = Child<UEquipmentItemSlotWidget>(Equipment);
        UVerticalBox* Rows = Panel ? Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("InventoryItems"))) : nullptr;
        UWidget* Details = Panel ? Panel->GetWidgetFromName(TEXT("SelectedInventoryItem")) : nullptr;
        UEquipmentItemSlotWidget* Source = nullptr;
        const FString ItemName = SubjectBefore.Items[ItemIndex].DisplayName.ToString();
        if (Rows)
        {
            for (UWidget* Row : Rows->GetAllChildren())
            {
                if (Text(Row).Contains(ItemName)) Source = Cast<UEquipmentItemSlotWidget>(Row);
            }
        }
        if (!Check(Source && Target && Details, TEXT("The read-only inventory retains its selectable actual row, detail panel and equipment drop target."))) return false;
        FSlateApplication::Get().SetKeyboardFocus(Source->TakeWidget(), EFocusCause::SetDirectly);
        SendKey(EKeys::Enter, false);
        if (!Check(Text(Details).Contains(ItemName), TEXT("Actual Slate Enter still opens the item's details while equipment changes are unavailable."))) return false;
        if (bDeadOwner)
        {
            // The equipment status retains death; the bag status gives current controller feedback priority over its death hint.
            // 장비 상태는 사망 표시를 유지하고 가방 상태는 사망 안내보다 현재 컨트롤러 피드백을 우선 표시합니다.
            const FString EquipmentText = Text(Equipment);
            const FString InventoryText = Text(Panel);
            const FString Feedback = Controller->GetEquipmentMessage().ToString();
            const bool bEquipmentDeath = EquipmentText.Contains(TEXT("사망 · 보관 정보 보기"));
            const bool bInventoryStatus = Feedback.IsEmpty() ? InventoryText.Contains(TEXT("사망 · 보유 아이템과 스킬 보기")) : InventoryText.Contains(Feedback);
            Test->AddInfo(FString::Printf(TEXT("Dead-owner UI inspection: character=%s authoritativeHP=%.1f equipmentDeathVisible=%d inventoryDeathHintVisible=%d currentFeedback=%s."), *SubjectId.ToString(), SubjectBefore.CurrentHP, bEquipmentDeath, InventoryText.Contains(TEXT("사망 · 보유 아이템과 스킬 보기")), *Feedback));
            if (!Check(bEquipmentDeath && bInventoryStatus, TEXT("The dead owner's actual equipment displays death while the bag displays its production death hint or exact current rejection feedback."))) return false;
        }
        const TArray<FRunPartyMember> BeforeParty = Run->GetPartyMembers();
        const FRunIdentityData BeforeIdentity = Run->GetRunIdentity();
        const ERunPhase BeforePhase = Run->GetPhase();
        TArray<uint8> BeforeBytes;
        if (!Check(UGameplayStatics::LoadDataFromSlot(BeforeBytes, Slot, 0) && !BeforeBytes.IsEmpty(), TEXT("The rejection baseline reads only the explicitly guarded temporary save."))) return false;
        int32 StateEvents = 0;
        const FDelegateHandle Observer = Run->OnRunStateChanged.AddLambda([&StateEvents]() { ++StateEvents; });
        bool bValid = Check(Drop(Source, Target, Run, ItemIndex, SubjectId), TEXT("The actual equipment target consumes the synthetic payload even when its change is rejected."));
        bValid &= Check(Text(Details).Contains(ItemName) && !Controller->IsEquipmentChangePending(), TEXT("A rejected drop preserves the selected details without starting an equipment request."));
        bValid &= Capture(CaptureName);
        FRunEquipmentCommand Command;
        Command.CharacterId = SubjectId;
        Command.ItemIndex = ItemIndex;
        Command.TargetSlot = URunEquipmentCatalog::GetWeaponSlot(0);
        Command.ExpectedRevision = SubjectBefore.Equipment.Revision;
        Controller->RequestChangeEquipment(Command);
        // Equipment feedback is intentionally cleared outside Shop by the production flow refresh; authority state remains the rejection evidence.
        // 제품 흐름 갱신은 상점 밖에서 장비 메시지를 지우므로 권위 상태의 불변을 변경 거절 근거로 사용합니다.
        const bool bPhaseFeedback = BeforePhase == ERunPhase::Shop ? !Controller->GetEquipmentMessage().IsEmpty() : Controller->GetEquipmentMessage().IsEmpty();
        bValid &= Check(!Controller->IsEquipmentChangePending() && bPhaseFeedback, TEXT("The public controller request finishes without a pending change and retains only the feedback appropriate to its actual phase."));
        Run->OnRunStateChanged.Remove(Observer);
        const TArray<FRunPartyMember>& AfterParty = Run->GetPartyMembers();
        bool bSameParty = BeforeParty.Num() == AfterParty.Num();
        for (int32 Index = 0; bSameParty && Index < BeforeParty.Num(); ++Index) bSameParty &= FRunPartyMember::StaticStruct()->CompareScriptStruct(&BeforeParty[Index], &AfterParty[Index], 0);
        TArray<uint8> AfterBytes;
        bValid &= Check(bSameParty && BeforePhase == Run->GetPhase() && FRunIdentityData::StaticStruct()->CompareScriptStruct(&BeforeIdentity, &Run->GetRunIdentity(), 0) && StateEvents == 0 && UGameplayStatics::LoadDataFromSlot(AfterBytes, Slot, 0) && BeforeBytes == AfterBytes, TEXT("Rejected UI and authority requests preserve every member, item copy, equipment revision, phase, original identity and the exact saved bytes without publishing Run state changes."));
        Test->AddInfo(FString::Printf(TEXT("Inventory rejection %s: subject=%s phase=%d HP=%.1f baselineRevision=%d stateEvents=%d guardedBytes=%d."), *CaptureName, *SubjectId.ToString(), static_cast<int32>(Run->GetPhase()), SubjectBefore.CurrentHP, SubjectBefore.Equipment.Revision, StateEvents, AfterBytes.Num()));
        return bValid;
    }

    bool InstallDeadOwnerFixture(URunStateSubsystem* Run)
    {
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
        FRunPartyMember* Member = Saved.IsValid() ? Saved->Party.FindByPredicate([this](const FRunPartyMember& Item) { return Item.CharacterId == CharacterId; }) : nullptr;
        if (!Check(Member && Saved->Party.ContainsByPredicate([this](const FRunPartyMember& Item) { return Item.bCreated && Item.CharacterId != CharacterId && Item.CurrentHP > 0.f; }), TEXT("The disposable death fixture keeps the original owner and surviving AI companions."))) return false;
        // Only the temporary saved HP changes; original ownership, control flags, equipment and production stats remain intact.
        // 임시 저장의 HP만 변경하며 원래 소유권·조작 플래그·장비·제품 수치는 유지합니다.
        Member->CurrentHP = 0.f;
        FText Error;
        return Check(UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0) && Run->LoadCheckpoint(Error) && FindMember(Run) && FindMember(Run)->CurrentHP == 0.f, TEXT("The guarded temporary save reloads the dead owner for actual inventory inspection."));
    }

    bool CheckGeometry(UUserWidget* Screen, const FGeometry& ViewportGeometry)
    {
        const FSlateRect Bounds = ViewportGeometry.GetLayoutBoundingRect();
        TArray<UWidget*> Controls;
        if (UInventoryWidget* Inventory = Cast<UInventoryWidget>(Screen))
        {
            Controls.Add(Inventory->GetWidgetFromName(TEXT("Button_CloseInventory")));
            if (UCharacterInventoryPanel* Panel = Child<UCharacterInventoryPanel>(Inventory))
            {
                Controls.Add(Panel->GetWidgetFromName(TEXT("InventoryCategoryTabs")));
                Controls.Add(Panel->GetWidgetFromName(TEXT("SelectedInventoryItem")));
                Controls.Add(Panel->GetWidgetFromName(TEXT("InventoryListScroll")));
            }
            if (UCharacterEquipmentPanel* Equipment = Child<UCharacterEquipmentPanel>(Inventory)) Controls.Add(Equipment->WidgetTree->RootWidget);
        }
        else if (URunEncounterWidget* Shop = Cast<URunEncounterWidget>(Screen))
        {
            Controls.Add(Shop->GetWidgetFromName(TEXT("Text_EncounterTitle")));
            Controls.Add(Shop->GetWidgetFromName(TEXT("Button_LeaveShop")));
        }
        for (UWidget* Control : Controls)
        {
            if (!Check(Control != nullptr, TEXT("The saved review screen retains its required layout control."))) return false;
            const FGeometry& Geometry = Control->GetCachedGeometry();
            const FSlateRect Rect = Geometry.GetLayoutBoundingRect();
            if (!Check(Geometry.GetLocalSize().X > 0.f && Geometry.GetLocalSize().Y > 0.f && Rect.Left >= Bounds.Left - 1.f && Rect.Top >= Bounds.Top - 1.f && Rect.Right <= Bounds.Right + 1.f && Rect.Bottom <= Bounds.Bottom + 1.f, TEXT("Visible review controls fit inside the actual viewport: ") + Control->GetName())) return false;
        }
        return true;
    }

    // Release only the review's pointer gesture and restore its original cursor on every exit.
    // 모든 종료 경로에서 검수 포인터 제스처만 해제하고 원래 커서를 복원합니다.
    void RestoreInventoryPointer()
    {
        if (!FSlateApplication::IsInitialized()) return;
        FSlateApplication& Slate = FSlateApplication::Get();
        if (bInventoryDragOwned && Slate.IsDragDropping()) Slate.CancelDragDrop();
        bInventoryDragOwned = false;
        if (bInventoryPointerPressed)
        {
            const FVector2D Cursor = Slate.GetCursorPos();
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, Cursor, Cursor, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
            bInventoryPointerPressed = false;
        }
        if (bInventoryCursorMoved) Slate.SetCursorPos(InventoryPreviousCursor);
        bInventoryCursorMoved = false;
    }

    // Start dragging through the row's actual Slate DetectDrag path and let its authored UMG operation reach the real slot on mouse release.
    // 행의 실제 Slate DetectDrag 경로로 시작하고 원래 UMG 작업이 마우스 해제 시 실제 장비 슬롯에 도달하게 합니다.
    bool ReviewPointerEquip(UInventoryWidget* Inventory, URunStateSubsystem* Run)
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? Slate.FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        const auto Fail = [this]() { bPointerReviewFailed = true; return false; };
        if (!Check(Window.IsValid() && Window->GetNativeWindow().IsValid() && FindMember(Run), TEXT("Real duplicate dragging retains its native review window and authoritative owner."))) return Fail();
        const TSet<FKey> Pressed{EKeys::LeftMouseButton};
        const TSet<FKey> Released;
        if (PointerReviewStage == 0)
        {
            UCharacterInventoryPanel* Panel = Child<UCharacterInventoryPanel>(Inventory);
            UCharacterEquipmentPanel* Equipment = Child<UCharacterEquipmentPanel>(Inventory);
            UVerticalBox* Rows = Panel ? Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("InventoryItems"))) : nullptr;
            UEquipmentItemSlotWidget* Source = Rows && Rows->GetChildrenCount() > DuplicateIndex ? Cast<UEquipmentItemSlotWidget>(Rows->GetChildAt(DuplicateIndex)) : nullptr;
            UEquipmentItemSlotWidget* Target = Child<UEquipmentItemSlotWidget>(Equipment);
            UScrollBox* Scroll = Panel ? Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("InventoryListScroll"))) : nullptr;
            if (!Check(Source && Target && Scroll && !Slate.IsDragDropping() && !Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton), TEXT("The appended duplicate has a real row, equipment target and scroll container before an isolated LMB drag."))) return Fail();
            InventoryDragSource = Source;
            InventoryDragTarget = Target;
            InventoryDragScroll = Scroll;
            InventoryDragRevision = FindMember(Run)->Equipment.Revision;
            if (!Check(UGameplayStatics::LoadDataFromSlot(InventorySaveBeforeDrag, Slot, 0), TEXT("The guarded save bytes are observed before the actual pointer drag."))) return Fail();
            InventoryPreviousCursor = Slate.GetCursorPos();
            bInventoryCursorMoved = true;
            Window->BringToFront(true);
            Scroll->ScrollWidgetIntoView(Source, false, EDescendantScrollDestination::Center);
            PointerReviewFrames = 0;
            PointerReviewStage = 1;
            return false;
        }
        if (PointerReviewStage == 1)
        {
            if (++PointerReviewFrames < 3) return false;
            UEquipmentItemSlotWidget* Source = InventoryDragSource.Get();
            UScrollBox* Scroll = InventoryDragScroll.Get();
            if (!Check(Source && Scroll && InventoryDragTarget.IsValid(), TEXT("Scrolling retains the same actual row and equipment target rather than reconstructing a drag source."))) return Fail();
            const FGeometry& Geometry = Source->GetCachedGeometry();
            const FSlateRect Bounds = Geometry.GetLayoutBoundingRect();
            const FSlateRect Clip = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
            if (Geometry.GetLocalSize().IsNearlyZero() || Bounds.Top < Clip.Top || Bounds.Bottom > Clip.Bottom) return false;
            InventoryDragStart = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
            Slate.SetCursorPos(InventoryDragStart);
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, InventoryDragStart, InventoryPreviousCursor, Released, EKeys::Invalid, 0, FModifierKeysState()));
            const FWidgetPath Path = Slate.LocateWindowUnderMouse(InventoryDragStart, Slate.GetInteractiveTopLevelWindows());
            if (!Path.IsValid() || !Path.ContainsWidget(&Source->TakeWidget().Get())) return false;
            bInventoryPointerPressed = true;
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), FPointerEvent(FSlateApplication::CursorPointerIndex, InventoryDragStart, InventoryDragStart, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
            PointerReviewStage = 2;
            return false;
        }
        if (PointerReviewStage == 2)
        {
            const FVector2D ThresholdPosition = InventoryDragStart + FVector2D(FMath::Max(20.f, Slate.GetDragTriggerDistance() * 2.f), 0.0);
            Slate.SetCursorPos(ThresholdPosition);
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, ThresholdPosition, InventoryDragStart, Pressed, EKeys::Invalid, 0, FModifierKeysState()));
            const TSharedPtr<FDragDropOperation> Content = Slate.GetDragDroppingContent();
            const TSharedPtr<FUMGDragDropOp> UMG = Content.IsValid() && Content->IsOfType<FUMGDragDropOp>() ? StaticCastSharedPtr<FUMGDragDropOp>(Content) : nullptr;
            const UEquipmentDragDropOperation* Operation = UMG.IsValid() ? Cast<UEquipmentDragDropOperation>(UMG->GetOperation()) : nullptr;
            bInventoryDragOwned = Slate.IsDragDropping();
            if (!Check(bInventoryDragOwned && Operation && Operation->CharacterId == CharacterId && Operation->ItemIndex == DuplicateIndex && Operation->ExpectedRevision == InventoryDragRevision, TEXT("Real LMB threshold crossing invokes NativeOnDragDetected and creates the authored stable owner/index/revision payload without a synthetic operation."))) return Fail();
            InventoryDragLast = ThresholdPosition;
            PointerReviewStage = 3;
            return false;
        }
        if (PointerReviewStage == 3)
        {
            UEquipmentItemSlotWidget* Target = InventoryDragTarget.Get();
            if (!Check(Target && Slate.IsDragDropping(), TEXT("The actual UMG drag remains active until its real target move."))) return Fail();
            const FGeometry& Geometry = Target->GetCachedGeometry();
            if (Geometry.GetLocalSize().IsNearlyZero()) return false;
            InventoryDragEnd = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
            Slate.SetCursorPos(InventoryDragEnd);
            Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, InventoryDragEnd, InventoryDragLast, Pressed, EKeys::Invalid, 0, FModifierKeysState()));
            const FWidgetPath Path = Slate.LocateWindowUnderMouse(InventoryDragEnd, Slate.GetInteractiveTopLevelWindows());
            if (!Check(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get()) && FindMember(Run)->Equipment.Revision == InventoryDragRevision, TEXT("Actual target hit testing routes the live drag over its equipment slot without changing authority before release."))) return Fail();
            PointerReviewStage = 4;
            return false;
        }
        if (PointerReviewStage == 4)
        {
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex, InventoryDragEnd, InventoryDragEnd, Released, EKeys::LeftMouseButton, 0, FModifierKeysState()));
            bInventoryPointerPressed = false;
            if (!Check(!Slate.IsDragDropping() && !Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton), TEXT("Actual LMB release finishes native UMG dropping and releases its pressed-button state."))) return Fail();
            bInventoryDragOwned = false;
            PointerReviewStage = 5;
            return false;
        }
        const FRunPartyMember* Member = FindMember(Run);
        if (Controller->IsEquipmentChangePending()) return false;
        TArray<uint8> SaveAfter;
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
        const FRunPartyMember* Stored = Saved.IsValid() ? Saved->Party.FindByPredicate([this](const FRunPartyMember& Item) { return Item.CharacterId == CharacterId; }) : nullptr;
        if (!Check(Member && Member->Equipment.Revision == InventoryDragRevision + 1 && RunEquipmentRules::FindItemIndexAtSlot(*Member, URunEquipmentCatalog::GetWeaponSlot(0)) == DuplicateIndex && !RunEquipmentRules::IsItemEquipped(*Member, OriginalIndex) && Stored && FRunPartyMember::StaticStruct()->CompareScriptStruct(Member, Stored, 0) && UGameplayStatics::LoadDataFromSlot(SaveAfter, Slot, 0) && SaveAfter != InventorySaveBeforeDrag, TEXT("The real pointer drop equips only the specific duplicate once and its new authoritative revision and complete owner data survive guarded save reload."))) return Fail();
        Test->AddInfo(FString::Printf(TEXT("Actual routed LMB duplicate equip: owner=%s index=%d originalCopy=%d revision=%d->%d savedBytes=%d->%d source=%s target=%s. Unguarded saves and ownership stay untouched."), *CharacterId.ToString(), DuplicateIndex, OriginalIndex, InventoryDragRevision, Member->Equipment.Revision, InventorySaveBeforeDrag.Num(), SaveAfter.Num(), *InventoryDragStart.ToString(), *InventoryDragEnd.ToString()));
        RestoreInventoryPointer();
        return true;
    }

    // Copy every engine config property, including confirmed video mode and stored window positions, without saving either object.
    // 확인된 화면 모드와 저장 창 위치를 포함한 엔진 설정 속성을 모두 복사하며 어느 객체도 저장하지 않습니다.
    static void CopyConfigProperties(UGameUserSettings* Target, const UGameUserSettings* Source)
    {
        for (TFieldIterator<FProperty> Property(Target->GetClass()); Property; ++Property)
        {
            if (Property->HasAnyPropertyFlags(CPF_Config)) Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Target), Property->ContainerPtrToValuePtr<void>(Source));
        }
        Target->ScalabilityQuality = Source->ScalabilityQuality;
    }

    // Take a scoped memory-and-file snapshot before the synthetic aspect-ratio resize; normal SaveSettings remains the production path.
    // 시험용 종횡비 변경 전에 메모리·파일 스냅샷을 보관하며 일반 SaveSettings 호출은 제품 경로를 유지합니다.
    bool BeginPreferencesReview(UGameUserSettings* User)
    {
        if (bPreferencesCaptured) return true;
        if (!Check(User && GConfig && !User->OnUpdateCloudDataFromGameUserSettings.IsBound() && !User->OnUpdateGameUserSettingsFileFromCloud.IsBound(), TEXT("The isolated editor preferences review has a config cache and no connected cloud preference callbacks."))) return false;
        OriginalSettings.Reset(DuplicateObject<UGameUserSettings>(User, GetTransientPackage()));
        OriginalDefaults.Reset(DuplicateObject<UGameUserSettings>(User->GetClass()->GetDefaultObject<UGameUserSettings>(), GetTransientPackage()));
        OriginalSettings->ScalabilityQuality = User->ScalabilityQuality;
        OriginalDefaults->ScalabilityQuality = User->GetClass()->GetDefaultObject<UGameUserSettings>()->ScalabilityQuality;
        OriginalUserIni = GGameUserSettingsIni;
        OriginalEditorIni = GEditorSettingsIni;
        OriginalKeyBindingsIni = GEditorKeyBindingsIni;
        if (!Check(!OriginalKeyBindingsIni.IsEmpty(), TEXT("The actual original editor key-binding INI path is available before any scoped Escape routing."))) return false;
        bOriginalKeyBindingsFileExists = IFileManager::Get().FileExists(*OriginalKeyBindingsIni);
        if (bOriginalKeyBindingsFileExists && !Check(FFileHelper::LoadFileToArray(OriginalKeyBindingsBytes, *OriginalKeyBindingsIni), TEXT("The original editor key-binding file bytes are retained without exposing their contents."))) return false;
        for (const FString& Filename : {OriginalUserIni, OriginalEditorIni})
        {
            FConfigBranch* Branch = GConfig->FindBranch(NAME_None, Filename);
            if (!Check(Branch && !Branch->bIsSafeUnloaded, TEXT("Both original preference config branches are already loaded before file operations are suspended."))) return false;
            FConfigSnapshot& Snapshot = OriginalConfigs.AddDefaulted_GetRef();
            Snapshot.Filename = Filename;
            Snapshot.InMemory = Branch->InMemoryFile;
            Snapshot.Saved = Branch->SavedLayer;
            Snapshot.Runtime = Branch->RuntimeChanges;
            Snapshot.bExisted = IFileManager::Get().FileExists(*Filename);
            if (Snapshot.bExisted && !Check(FFileHelper::LoadFileToArray(Snapshot.Bytes, *Filename), TEXT("The original preference INI bytes are readable before the isolated review."))) return false;
        }
        bConfigOperationsWereDisabled = GConfig->AreFileOperationsDisabled();
        if (const IConsoleVariable* Fullscreen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FullScreenMode")))
        {
            OriginalPreferredFullscreen = Fullscreen->GetInt();
            OriginalPreferredFullscreenPriority = static_cast<EConsoleVariableFlags>(Fullscreen->GetFlags() & ECVF_SetByMask);
        }
        bPreferencesCaptured = true;
        GConfig->DisableFileOperations();
        const Scalability::FQualityLevels Quality = User->ScalabilityQuality;
        User->SetFullscreenMode(EWindowMode::Windowed);
        User->SetScreenResolution(Size);
        User->ScalabilityQuality = Quality;
        User->ConfirmVideoMode();
        Test->AddInfo(FString::Printf(TEXT("Scoped inventory video baseline: original=%s mode=%d confirmed=%s confirmedMode=%d review=%s mode=%d. GConfig disk operations are suspended only for this review; actual option persistence is not claimed."), *OriginalSettings->GetScreenResolution().ToString(), static_cast<int32>(OriginalSettings->GetFullscreenMode()), *OriginalSettings->GetLastConfirmedScreenResolution().ToString(), static_cast<int32>(OriginalSettings->GetLastConfirmedFullscreenMode()), *User->GetScreenResolution().ToString(), static_cast<int32>(User->GetFullscreenMode())));
        return true;
    }

    // Restore all original config properties, defaults and dirty/runtime streams before re-enabling any config writes.
    // 설정 파일 쓰기를 다시 허용하기 전에 원래 설정 속성·기본 객체·변경 스트림을 모두 복구합니다.
    void RestoreOriginalPreferences()
    {
        if (!bPreferencesCaptured) return;
        RestoreOptionsReviewSettings();
        if (UGameUserSettings* User = UGameUserSettings::GetGameUserSettings())
        {
            CopyConfigProperties(User, OriginalSettings.Get());
            User->ApplyNonResolutionSettings();
            UGameUserSettings::RequestResolutionChange(OriginalSettings->GetScreenResolution().X, OriginalSettings->GetScreenResolution().Y, OriginalSettings->GetFullscreenMode(), false);
            if (IConsoleVariable* Fullscreen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FullScreenMode")))
            {
                Fullscreen->Set(OriginalPreferredFullscreen, OriginalPreferredFullscreenPriority);
                Check(Fullscreen->GetInt() == OriginalPreferredFullscreen && (Fullscreen->GetFlags() & ECVF_SetByMask) == OriginalPreferredFullscreenPriority, TEXT("The original preferred fullscreen console value and setter priority are restored."));
            }
            CopyConfigProperties(User, OriginalSettings.Get());
            CopyConfigProperties(User->GetClass()->GetDefaultObject<UGameUserSettings>(), OriginalDefaults.Get());
            bool bExactConfig = User->ScalabilityQuality == OriginalSettings->ScalabilityQuality && User->GetClass()->GetDefaultObject<UGameUserSettings>()->ScalabilityQuality == OriginalDefaults->ScalabilityQuality;
            for (TFieldIterator<FProperty> Property(User->GetClass()); Property; ++Property)
            {
                if (Property->HasAnyPropertyFlags(CPF_Config)) bExactConfig &= Property->Identical_InContainer(User, OriginalSettings.Get()) && Property->Identical_InContainer(User->GetClass()->GetDefaultObject<UGameUserSettings>(), OriginalDefaults.Get());
            }
            Check(bExactConfig, TEXT("Review teardown restores every original UserSettings and CDO config property, including confirmed modes, plus the original custom quality."));
        }
        else Test->AddError(TEXT("Review teardown could not restore the original GameUserSettings object."));
        TArray<uint8> CurrentKeyBindingsBytes;
        const bool bKeyBindingsExist = IFileManager::Get().FileExists(*OriginalKeyBindingsIni);
        const bool bKeyBindingsPreserved = bKeyBindingsExist == bOriginalKeyBindingsFileExists && (!bKeyBindingsExist || (FFileHelper::LoadFileToArray(CurrentKeyBindingsBytes, *OriginalKeyBindingsIni) && CurrentKeyBindingsBytes == OriginalKeyBindingsBytes));
        Check(GEditorKeyBindingsIni == OriginalKeyBindingsIni && bKeyBindingsPreserved, TEXT("Scoped editor Escape handling preserves the original key-binding INI path, existence and exact bytes; user-override metadata in disposable command memory is not claimed unchanged."));
        if (GConfig)
        {
            for (const FConfigSnapshot& Snapshot : OriginalConfigs)
            {
                if (FConfigBranch* Branch = GConfig->FindBranchWithNoReload(NAME_None, Snapshot.Filename))
                {
                    Branch->InMemoryFile = Snapshot.InMemory;
                    Branch->SavedLayer = Snapshot.Saved;
                    Branch->RuntimeChanges = Snapshot.Runtime;
                    Check(Branch->InMemoryFile.Dirty == Snapshot.InMemory.Dirty && Branch->InMemoryFile.NoSave == Snapshot.InMemory.NoSave, TEXT("The original preference branch dirty/save flags are restored before config writes resume."));
                }
                else Test->AddError(TEXT("Review teardown could not restore the original preference config branch: ") + Snapshot.Filename);
                TArray<uint8> Current;
                const bool bExistsNow = IFileManager::Get().FileExists(*Snapshot.Filename);
                Check(bExistsNow == Snapshot.bExisted && (!bExistsNow || (FFileHelper::LoadFileToArray(Current, *Snapshot.Filename) && Current == Snapshot.Bytes)), TEXT("The original preference INI existence and every file byte remain unchanged: ") + Snapshot.Filename);
            }
            Check(GGameUserSettingsIni == OriginalUserIni && GEditorSettingsIni == OriginalEditorIni, TEXT("The review preserves the original engine preference INI targets."));
            if (!bConfigOperationsWereDisabled) GConfig->EnableFileOperations();
        }
        bPreferencesCaptured = false;
        OriginalSettings.Reset();
        OriginalDefaults.Reset();
        OriginalConfigs.Reset();
    }

    // Restore a pending review preview on every early exit without retaining a changed user video or graphics preference.
    // 조기 종료 시 대기 중인 검수 시험 적용을 복구하여 변경된 사용자 화면·그래픽 설정이 남지 않게 합니다.
    void RestoreOptionsReviewSettings()
    {
        if (!bVideoSnapshotCaptured) return;
        if (VideoOptions.IsValid()) VideoOptions->RevertOptions();
        if (UGameUserSettings* User = UGameUserSettings::GetGameUserSettings())
        {
            if (User->GetScreenResolution() != VideoBeforeResolution || User->GetFullscreenMode() != VideoBeforeMode || User->ScalabilityQuality != VideoBeforeQuality || User->IsVSyncEnabled() != bVideoBeforeVSync)
            {
                User->SetFullscreenMode(VideoBeforeMode);
                User->SetScreenResolution(VideoBeforeResolution);
                User->ScalabilityQuality = VideoBeforeQuality;
                User->SetVSyncEnabled(bVideoBeforeVSync);
                User->ApplyResolutionSettings(false);
                User->ApplyNonResolutionSettings();
                User->ConfirmVideoMode();
                User->SaveSettings();
            }
        }
        bVideoSnapshotCaptured = false;
    }

    // Use real option selectors and the Apply delegate, then cancel through the gameplay Slate shortcut while preserving the blocked Run layer.
    // 실제 설정 선택기와 적용 델리게이트를 사용한 뒤 Run 입력 차단을 유지한 채 게임플레이 Slate 단축키로 취소합니다.
    bool ReviewVideoCancellation(UOptionsWidget* Options, FKey Key)
    {
        UWorld* World = Controller->GetWorld();
        UGameUserSettings* User = UGameUserSettings::GetGameUserSettings();
        UGameplayRootWidget* Root = FindRoot(World);
        UWidget* Layer = Root ? Root->GetWidgetFromName(TEXT("RunLayer")) : nullptr;
        UGameViewportClient* Viewport = World->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        UWidget* Confirmation = Options->GetWidgetFromName(TEXT("VideoConfirmationPanel"));
        UComboBoxString* Mode = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("WindowModeSelect")));
        UComboBoxString* Resolution = Cast<UComboBoxString>(Options->GetWidgetFromName(TEXT("ResolutionSelect")));
        UButton* Apply = Cast<UButton>(Options->GetWidgetFromName(TEXT("ApplyOptionsButton")));
        const auto Fail = [this]() { bVideoReviewFailed = true; return false; };
        if (!Check(User && Root && Layer && Viewport && Viewport->Viewport && Window.IsValid() && Window->GetNativeWindow().IsValid() && Confirmation && Mode && Resolution && Apply && !Layer->GetIsEnabled(), TEXT("The actual gameplay options retain their video controls, native viewport and disabled underlying Run layer."))) return Fail();
        if (VideoReviewStage == 0)
        {
            VideoOptions = Options;
            VideoBeforeResolution = User->GetScreenResolution();
            VideoBeforeMode = User->GetFullscreenMode();
            VideoBeforeQuality = User->ScalabilityQuality;
            bVideoBeforeVSync = User->IsVSyncEnabled();
            VideoBeforeViewport = Viewport->Viewport->GetSizeXY();
            VideoBeforeClient = Window->GetClientSizeInScreen();
            VideoBeforeOuter = Window->GetSizeInScreen();
            VideoBeforeNativeMode = Window->GetWindowMode();
            bVideoBeforeAltEnter = GetDefault<UInputSettings>()->bAltEnterTogglesFullscreen;
            bVideoBeforeF11 = GetDefault<UInputSettings>()->bF11TogglesFullscreen;
            bVideoSnapshotCaptured = true;
            if (!Check(bPreferencesCaptured && GConfig->AreFileOperationsDisabled() && VideoBeforeViewport == Size && FVector2D(VideoBeforeResolution.X, VideoBeforeResolution.Y).Equals(VideoBeforeOuter, 0.01) && GSystemResolution.ResX == VideoBeforeResolution.X && GSystemResolution.ResY == VideoBeforeResolution.Y && GSystemResolution.WindowMode == VideoBeforeMode && VideoBeforeMode == EWindowMode::Windowed && VideoBeforeNativeMode == VideoBeforeMode, TEXT("Video cancellation starts with the exact review viewport and settled transient system resolution matching the custom-title PIE outer window while original preference files are protected."))) return Fail();
            const EWindowMode::Type PreviewMode = Key == EKeys::O ? EWindowMode::Windowed : VideoBeforeMode == EWindowMode::WindowedFullscreen ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen;
            Mode->SetSelectedIndex(static_cast<int32>(PreviewMode));
            if (Key == EKeys::O || PreviewMode == EWindowMode::Windowed)
            {
                const FString PreviousLabel = FString::Printf(TEXT("%d × %d"), VideoBeforeResolution.X, VideoBeforeResolution.Y);
                int32 DifferentIndex = INDEX_NONE;
                for (int32 Index = 0; Index < Resolution->GetOptionCount(); ++Index)
                {
                    if (Resolution->GetOptionAtIndex(Index) == PreviousLabel) continue;
                    DifferentIndex = Index;
                    break;
                }
                if (!Check(Resolution->GetIsEnabled() && DifferentIndex != INDEX_NONE, TEXT("The actual windowed resolution combo exposes a different supported preview size."))) return Fail();
                Resolution->SetSelectedIndex(DifferentIndex);
            }
            if (!Check(Apply->GetIsEnabled(), TEXT("The actual Apply button is enabled for the selected preview."))) return Fail();
            Apply->OnClicked.Broadcast();
            if (!Check(Confirmation->GetVisibility() == ESlateVisibility::Visible && (User->GetScreenResolution() != VideoBeforeResolution || User->GetFullscreenMode() != VideoBeforeMode) && !GetDefault<UInputSettings>()->bAltEnterTogglesFullscreen && !GetDefault<UInputSettings>()->bF11TogglesFullscreen, TEXT("Actual combo selection and Apply preview different video settings, expose confirmation and suspend fullscreen shortcuts."))) return Fail();
            Test->AddInfo(FString::Printf(TEXT("Gameplay %s video preview: originalSettings=%s mode=%d selected=%s selectedMode=%d actualViewport=%s nativeMode=%d. Editor PIE preview and uncooked game display transitions are separate evidence."), *Key.ToString(), *VideoBeforeResolution.ToString(), static_cast<int32>(VideoBeforeMode), *Resolution->GetSelectedOption(), static_cast<int32>(User->GetFullscreenMode()), *Viewport->Viewport->GetSizeXY().ToString(), static_cast<int32>(Window->GetWindowMode())));
            Test->AddInfo(FString::Printf(TEXT("Gameplay %s video baseline: requestedReview=%s settings=%s settingsMode=%d prePreviewActualViewport=%s prePreviewNativeMode=%d prePreviewClient=%s prePreviewOuter=%s currentClient=%s currentOuter=%s currentSlateGeometry=%s offscreen=%d. SceneViewport::ResizeFrame ReshapeWindow uses outer size for this custom-title PIE window."), *Key.ToString(), *Size.ToString(), *VideoBeforeResolution.ToString(), static_cast<int32>(VideoBeforeMode), *VideoBeforeViewport.ToString(), static_cast<int32>(VideoBeforeNativeMode), *VideoBeforeClient.ToString(), *VideoBeforeOuter.ToString(), *Window->GetClientSizeInScreen().ToString(), *Window->GetSizeInScreen().ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), FSlateApplication::Get().IsRenderingOffScreen()));
            VideoPreviewResolution = User->GetScreenResolution();
            VideoPreviewMode = User->GetFullscreenMode();
            VideoPreviewStarted = FPlatformTime::Seconds();
            VideoPreviewStableFrames = 0;
            VideoPreviewLastViewport = FIntPoint::ZeroValue;
            VideoReviewFrames = 0;
            VideoReviewStage = 1;
            return false;
        }
        if (VideoReviewStage == 1)
        {
            if (++VideoReviewFrames < 2) return false;
            if (!Check(Options->IsActivated() && Confirmation->GetVisibility() == ESlateVisibility::Visible && !Layer->GetIsEnabled(), TEXT("Pending confirmation keeps actual gameplay options open and the underlying Run blocked before the shortcut."))) return Fail();
            const FIntPoint PreviewViewport = Viewport->Viewport->GetSizeXY();
            const bool bPreviewApplied = GSystemResolution.ResX == VideoPreviewResolution.X && GSystemResolution.ResY == VideoPreviewResolution.Y && GSystemResolution.WindowMode == VideoPreviewMode && Window->GetWindowMode() == VideoPreviewMode && FVector2D(Window->GetSizeInScreen()).Equals(FVector2D(VideoPreviewResolution.X, VideoPreviewResolution.Y), 0.01) && FVector2D(Widget->GetCachedGeometry().GetLocalSize()).Equals(FVector2D(PreviewViewport.X, PreviewViewport.Y), 0.01) && (PreviewViewport != VideoBeforeViewport || VideoPreviewMode != VideoBeforeNativeMode);
            VideoPreviewStableFrames = bPreviewApplied && PreviewViewport == VideoPreviewLastViewport ? VideoPreviewStableFrames + 1 : 0;
            VideoPreviewLastViewport = PreviewViewport;
            if (VideoPreviewStableFrames < 2)
            {
                if (FPlatformTime::Seconds() - VideoPreviewStarted < 3.0) return false;
                Check(false, FString::Printf(TEXT("The actual requested video preview must settle before cancellation; requested=%s mode=%d system=%dx%d mode=%d render=%s client=%s outer=%s Slate=%s stable=%d."), *VideoPreviewResolution.ToString(), static_cast<int32>(VideoPreviewMode), GSystemResolution.ResX, GSystemResolution.ResY, static_cast<int32>(GSystemResolution.WindowMode), *PreviewViewport.ToString(), *Window->GetClientSizeInScreen().ToString(), *Window->GetSizeInScreen().ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), VideoPreviewStableFrames));
                return Fail();
            }
            Test->AddInfo(FString::Printf(TEXT("Gameplay %s actual native preview settled: requested=%s mode=%d render=%s client=%s outer=%s Slate=%s stable=%d."), *Key.ToString(), *VideoPreviewResolution.ToString(), static_cast<int32>(VideoPreviewMode), *PreviewViewport.ToString(), *Window->GetClientSizeInScreen().ToString(), *Window->GetSizeInScreen().ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), VideoPreviewStableFrames));
            SendKey(Key);
            VideoRevertStarted = FPlatformTime::Seconds();
            VideoRevertLastDiagnostic = 0.0;
            VideoRevertStableFrames = 0;
            VideoReviewStage = 2;
            VideoReviewFrames = 0;
            return false;
        }
        if (VideoReviewStage == 2)
        {
            if (++VideoReviewFrames < 2) return false;
            if (!Check(Options->IsActivated() && Confirmation->GetVisibility() == ESlateVisibility::Collapsed && Root->IsUtilityMenuOpen() && !Layer->GetIsEnabled(), TEXT("The first real O or Esc cancels pending confirmation while keeping the options screen and Run input block active."))) return Fail();
            if (!Check(User->GetScreenResolution() == VideoBeforeResolution && User->GetFullscreenMode() == VideoBeforeMode && User->ScalabilityQuality == VideoBeforeQuality && User->IsVSyncEnabled() == bVideoBeforeVSync && GetDefault<UInputSettings>()->bAltEnterTogglesFullscreen == bVideoBeforeAltEnter && GetDefault<UInputSettings>()->bF11TogglesFullscreen == bVideoBeforeF11, TEXT("The actual gameplay cancellation restores all prior video, graphics, VSync and fullscreen shortcut settings."))) return Fail();
            const FIntPoint ActualViewport = Viewport->Viewport->GetSizeXY();
            const EWindowMode::Type NativeMode = Window->GetWindowMode();
            const double RevertElapsed = FPlatformTime::Seconds() - VideoRevertStarted;
            const bool bNativeRestored = ActualViewport == VideoBeforeViewport && NativeMode == VideoBeforeNativeMode && FVector2D(Window->GetClientSizeInScreen()).Equals(VideoBeforeClient, 0.01) && FVector2D(Window->GetSizeInScreen()).Equals(VideoBeforeOuter, 0.01) && FVector2D(Widget->GetCachedGeometry().GetLocalSize()).Equals(FVector2D(VideoBeforeViewport.X, VideoBeforeViewport.Y), 0.01) && GSystemResolution.ResX == VideoBeforeResolution.X && GSystemResolution.ResY == VideoBeforeResolution.Y && GSystemResolution.WindowMode == VideoBeforeMode;
            VideoRevertStableFrames = bNativeRestored ? VideoRevertStableFrames + 1 : 0;
            if (bNativeRestored && VideoRevertStableFrames < 2) return false;
            if (!bNativeRestored)
            {
                if (VideoRevertLastDiagnostic == 0.0 || FPlatformTime::Seconds() - VideoRevertLastDiagnostic >= 0.5)
                {
                    VideoRevertLastDiagnostic = FPlatformTime::Seconds();
                    Test->AddInfo(FString::Printf(TEXT("Gameplay %s cancellation native wait: frames=%d elapsed=%.3f settingsBefore=%s settingsNow=%s settingsModeBefore=%d settingsModeNow=%d viewportBefore=%s viewportNow=%s nativeModeBefore=%d nativeModeNow=%d clientNow=%s SlateNow=%s offscreen=%d. No fixture resize or window-mode change is applied during this observation."), *Key.ToString(), VideoReviewFrames, RevertElapsed, *VideoBeforeResolution.ToString(), *User->GetScreenResolution().ToString(), static_cast<int32>(VideoBeforeMode), static_cast<int32>(User->GetFullscreenMode()), *VideoBeforeViewport.ToString(), *ActualViewport.ToString(), static_cast<int32>(VideoBeforeNativeMode), static_cast<int32>(NativeMode), *Window->GetClientSizeInScreen().ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), FSlateApplication::Get().IsRenderingOffScreen()));
                }
                if (RevertElapsed < 3.0) return false;
            }
            if (!Check(bNativeRestored, FString::Printf(TEXT("The actual native PIE viewport and window mode restore their exact pre-preview state within three real seconds; before=%s mode=%d after=%s mode=%d settings=%s mode=%d."), *VideoBeforeViewport.ToString(), static_cast<int32>(VideoBeforeNativeMode), *ActualViewport.ToString(), static_cast<int32>(NativeMode), *User->GetScreenResolution().ToString(), static_cast<int32>(User->GetFullscreenMode())))) return Fail();
            Test->AddInfo(FString::Printf(TEXT("Gameplay %s cancellation restored settings=%s mode=%d actualViewport=%s nativeMode=%d while RunLayer remains blocked."), *Key.ToString(), *User->GetScreenResolution().ToString(), static_cast<int32>(User->GetFullscreenMode()), *Viewport->Viewport->GetSizeXY().ToString(), static_cast<int32>(Window->GetWindowMode())));
            if (!Capture(Key == EKeys::O ? TEXT("OptionsVideoRevertedO") : TEXT("OptionsVideoRevertedEsc"))) return Fail();
            bVideoSnapshotCaptured = false;
            VideoOptions.Reset();
            SendKey(Key);
            VideoReviewStage = 0;
            return true;
        }
        return false;
    }

    // Resize the actual PIE client after travel, then observe two Slate frames before checking its physical dimensions.
    // 트래블 이후 실제 PIE 클라이언트 크기를 조정하고 Slate 두 프레임을 기다린 뒤 물리 해상도를 검사합니다.
    bool PrepareViewport()
    {
        if (bViewportReady) return true;
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        if (!Window.IsValid() || !Viewport || !Viewport->Viewport) return false;
        if (!bViewportResizeRequested)
        {
            if (!BeginPreferencesReview(UGameUserSettings::GetGameUserSettings()))
            {
                bViewportFailed = true;
                return false;
            }
            Window->SetWindowMode(EWindowMode::Windowed);
            const FIntPoint Before = Viewport->Viewport->GetSizeXY();
            Test->AddInfo(FString::Printf(TEXT("Inventory UI viewport before resize: requested=%dx%d physical=%dx%d window=%s localGeometry=%s DPI=%.3f."), Size.X, Size.Y, Before.X, Before.Y, *FVector2D(Window->GetSizeInScreen()).ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), Window->GetDPIScaleFactor()));
            Window->Resize(FVector2D(Size.X, Size.Y));
            bViewportResizeRequested = true;
            return false;
        }
        if (++ViewportWarmFrames < 2) return false;
        const FIntPoint After = Viewport->Viewport->GetSizeXY();
        if (bViewportNativeBaselineRequested)
        {
            if (ViewportWarmFrames < 4 || FPlatformTime::Seconds() - ViewportNativeBaselineStarted < 0.25) return false;
            UGameUserSettings* User = UGameUserSettings::GetGameUserSettings();
            const bool bSettled = User && After == Size && FVector2D(Window->GetSizeInScreen()).Equals(FVector2D(ViewportNativeResolution.X, ViewportNativeResolution.Y), 0.01) && FVector2D(Widget->GetCachedGeometry().GetLocalSize()).Equals(FVector2D(Size.X, Size.Y), 0.01) && User->GetScreenResolution() == ViewportNativeResolution && User->GetFullscreenMode() == EWindowMode::Windowed && Window->GetWindowMode() == EWindowMode::Windowed && GSystemResolution.ResX == ViewportNativeResolution.X && GSystemResolution.ResY == ViewportNativeResolution.Y && GSystemResolution.WindowMode == EWindowMode::Windowed;
            if (!bSettled && FPlatformTime::Seconds() - ViewportNativeBaselineStarted < 3.0) return false;
            bViewportReady = Check(bSettled, TEXT("The production resolution request settles the measured custom-title PIE outer baseline while preserving the exact review render viewport and Windowed mode."));
            Test->AddInfo(FString::Printf(TEXT("Inventory native baseline after engine resolution request: render=%s nativeClient=%s outer=%s preference=%s nativeMode=%d preferenceMode=%d system=%dx%d mode=%d Slate=%s DPI=%.3f."), *After.ToString(), *Window->GetClientSizeInScreen().ToString(), *Window->GetSizeInScreen().ToString(), User ? *User->GetScreenResolution().ToString() : TEXT("missing"), static_cast<int32>(Window->GetWindowMode()), User ? static_cast<int32>(User->GetFullscreenMode()) : -1, GSystemResolution.ResX, GSystemResolution.ResY, static_cast<int32>(GSystemResolution.WindowMode), *Widget->GetCachedGeometry().GetLocalSize().ToString(), Window->GetDPIScaleFactor()));
            bViewportFailed = !bViewportReady;
            return bViewportReady;
        }
        if (After != Size && ViewportResizeRetries < 3)
        {
            const FVector2D ClientSize = Window->GetClientSizeInScreen();
            const FVector2D Correction(Size.X - After.X, Size.Y - After.Y);
            Test->AddInfo(FString::Printf(TEXT("Inventory UI viewport resize retry=%d requested=%dx%d physical=%dx%d client=%s correction=%s."), ViewportResizeRetries + 1, Size.X, Size.Y, After.X, After.Y, *ClientSize.ToString(), *Correction.ToString()));
            Window->Resize(ClientSize + Correction);
            ++ViewportResizeRetries;
            ViewportWarmFrames = 0;
            return false;
        }
        Test->AddInfo(FString::Printf(TEXT("Inventory UI viewport after resize: requested=%dx%d physical=%dx%d window=%s localGeometry=%s DPI=%.3f warmFrames=%d."), Size.X, Size.Y, After.X, After.Y, *FVector2D(Window->GetSizeInScreen()).ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), Window->GetDPIScaleFactor(), ViewportWarmFrames));
        UGameUserSettings* User = UGameUserSettings::GetGameUserSettings();
        const FVector2D ClientSize = Window->GetClientSizeInScreen();
        const FVector2D OuterSize = Window->GetSizeInScreen();
        ViewportNativeResolution = FIntPoint(FMath::RoundToInt(OuterSize.X), FMath::RoundToInt(OuterSize.Y));
        if (!Check(After == Size && User && !Window->HasOSWindowBorder() && FVector2D(ViewportNativeResolution.X, ViewportNativeResolution.Y).Equals(OuterSize, 0.01) && User->GetFullscreenMode() == Window->GetWindowMode() && Window->GetWindowMode() == EWindowMode::Windowed, TEXT("The exact review viewport belongs to a measured custom-title PIE window before its transient production-resolution baseline is aligned.")))
        {
            bViewportFailed = true;
            return false;
        }
        // ResizeFrame uses ReshapeWindow with outer size when centering or changing this custom-title PIE window's mode.
        // ResizeFrame이 이 사용자 제목 표시줄 PIE 창을 중앙 이동하거나 모드 변경할 때 ReshapeWindow에 외부 크기를 전달합니다.
        const Scalability::FQualityLevels Quality = User->ScalabilityQuality;
        User->SetScreenResolution(ViewportNativeResolution);
        User->ScalabilityQuality = Quality;
        User->ConfirmVideoMode();
        // Begin off the measured work-area center so the engine takes its documented ReshapeWindow branch, not Resize(client).
        // 측정 작업 영역 중앙에서 벗어나 시작하여 엔진이 Resize(client)가 아닌 명시된 ReshapeWindow 분기를 선택하도록 합니다.
        const FSlateRect WorkArea = FSlateApplication::Get().GetWorkArea(FSlateRect::FromPointAndExtent(Window->GetPositionInScreen(), Window->GetClientSizeInScreen()));
        const FVector2D WorkAreaSize = WorkArea.GetSize();
        const FVector2D CenteredPosition = WorkArea.GetTopLeft() + FVector2D(FMath::Max(0.0, (WorkAreaSize.X - OuterSize.X) * 0.5), FMath::Max(0.0, (WorkAreaSize.Y - OuterSize.Y) * 0.5));
        Window->MoveWindowTo(CenteredPosition + FVector2D(16.0, 16.0));
        UGameUserSettings::RequestResolutionChange(ViewportNativeResolution.X, ViewportNativeResolution.Y, EWindowMode::Windowed, false);
        Test->AddInfo(FString::Printf(TEXT("Inventory measured native baseline request: render=%s client=%s outer=%s requestedSystem=%s; no fixed border/title offsets are assumed."), *After.ToString(), *ClientSize.ToString(), *OuterSize.ToString(), *ViewportNativeResolution.ToString()));
        bViewportNativeBaselineRequested = true;
        ViewportNativeBaselineStarted = FPlatformTime::Seconds();
        ViewportWarmFrames = 0;
        return false;
    }

    bool Capture(const FString& Name)
    {
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (!Check(Viewport && Viewport->Viewport && Viewport->Viewport->GetSizeXY() == Size && Widget.IsValid(), TEXT("The real PIE viewport has the requested review aspect ratio."))) return false;
        UUserWidget* Screen = Active<UInventoryWidget>(Controller->GetWorld());
        if (!Screen) Screen = Active<URunEncounterWidget>(Controller->GetWorld());
        if (!CheckGeometry(Screen, Widget->GetCachedGeometry())) return false;
        TArray<FColor> Pixels;
        FIntVector Dimensions;
        if (!Check(FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Dimensions) && Dimensions.X == Size.X && Dimensions.Y == Size.Y && Pixels.Num() == Size.X * Size.Y, TEXT("The Slate capture contains the actual rendered UI at the requested dimensions."))) return false;
        for (FColor& Pixel : Pixels) Pixel.A = 255;
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        const FString Filename = Output / FString::Printf(TEXT("%dx%d_%s.png"), Size.X, Size.Y, *Name);
        if (!Check(!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *Filename) && IFileManager::Get().FileSize(*Filename) == Png.Num(), TEXT("The complete UI capture is saved before PIE teardown."))) return false;
        Test->AddInfo(TEXT("Inventory UI capture: ") + Filename);
        return true;
    }

    FAutomationTestBase* Test;
    FString Slot;
    FString Output;
    FIntPoint Size;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TSharedPtr<ISlateViewport> RetainedViewport;
    TWeakObjectPtr<AGameplayPlayerController> Controller;
    TArray<FRunItemDefinition> Catalog;
    FGuid CharacterId;
    FGuid ReadOnlyCharacterId;
    int32 OriginalIndex = INDEX_NONE;
    int32 DuplicateIndex = INDEX_NONE;
    int32 ReadOnlyItemIndex = INDEX_NONE;
    int32 Stage = 0;
    int32 Frames = 0;
    int32 ViewportWarmFrames = 0;
    int32 ViewportResizeRetries = 0;
    bool bViewportResizeRequested = false;
    bool bViewportNativeBaselineRequested = false;
    double ViewportNativeBaselineStarted = 0.0;
    FIntPoint ViewportNativeResolution = FIntPoint::ZeroValue;
    bool bViewportReady = false;
    bool bViewportFailed = false;
    bool bOptionsReplacementReviewed = false;
    bool bVideoReviewFailed = false;
    bool bInventoryCatalogReviewed = false;
    bool bPointerReviewFailed = false;
    bool bInventoryPointerPressed = false;
    bool bInventoryDragOwned = false;
    bool bInventoryCursorMoved = false;
    int32 PointerReviewStage = 0;
    int32 PointerReviewFrames = 0;
    int32 InventoryDragRevision = INDEX_NONE;
    FVector2D InventoryPreviousCursor = FVector2D::ZeroVector;
    FVector2D InventoryDragStart = FVector2D::ZeroVector;
    FVector2D InventoryDragLast = FVector2D::ZeroVector;
    FVector2D InventoryDragEnd = FVector2D::ZeroVector;
    TWeakObjectPtr<UEquipmentItemSlotWidget> InventoryDragSource;
    TWeakObjectPtr<UEquipmentItemSlotWidget> InventoryDragTarget;
    TWeakObjectPtr<UScrollBox> InventoryDragScroll;
    TArray<uint8> InventorySaveBeforeDrag;
    bool bPreferencesCaptured = false;
    bool bConfigOperationsWereDisabled = false;
    int32 OriginalPreferredFullscreen = 1;
    EConsoleVariableFlags OriginalPreferredFullscreenPriority = ECVF_SetByConstructor;
    FString OriginalUserIni;
    FString OriginalEditorIni;
    FString OriginalKeyBindingsIni;
    TArray<uint8> OriginalKeyBindingsBytes;
    bool bOriginalKeyBindingsFileExists = false;
    TArray<FConfigSnapshot> OriginalConfigs;
    TStrongObjectPtr<UGameUserSettings> OriginalSettings;
    TStrongObjectPtr<UGameUserSettings> OriginalDefaults;
    bool bVideoSnapshotCaptured = false;
    bool bVideoBeforeVSync = false;
    bool bVideoBeforeAltEnter = false;
    bool bVideoBeforeF11 = false;
    int32 VideoReviewStage = 0;
    int32 VideoReviewFrames = 0;
    int32 VideoPreviewStableFrames = 0;
    int32 VideoRevertStableFrames = 0;
    double VideoPreviewStarted = 0.0;
    FIntPoint VideoPreviewLastViewport = FIntPoint::ZeroValue;
    FIntPoint VideoPreviewResolution = FIntPoint::ZeroValue;
    EWindowMode::Type VideoPreviewMode = EWindowMode::Windowed;
    double VideoRevertStarted = 0.0;
    double VideoRevertLastDiagnostic = 0.0;
    FIntPoint VideoBeforeResolution = FIntPoint::ZeroValue;
    FIntPoint VideoBeforeViewport = FIntPoint::ZeroValue;
    FVector2D VideoBeforeClient = FVector2D::ZeroVector;
    FVector2D VideoBeforeOuter = FVector2D::ZeroVector;
    EWindowMode::Type VideoBeforeMode = EWindowMode::Windowed;
    EWindowMode::Type VideoBeforeNativeMode = EWindowMode::Windowed;
    Scalability::FQualityLevels VideoBeforeQuality;
    TWeakObjectPtr<UOptionsWidget> VideoOptions;
    double Started = 0.0;
    double StageStarted = 0.0;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FInventoryUiReviewTest, "ProjectA.TodoReview.InventoryUI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FInventoryUiReviewTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const FString& Name : {FString(TEXT("4x3")), FString(TEXT("16x9")), FString(TEXT("21x9"))})
    {
        Names.Add(Name);
        Commands.Add(Name);
    }
}

bool FInventoryUiReviewTest::RunTest(const FString& Parameters)
{
    const FIntPoint Size = Parameters == TEXT("4x3") ? FIntPoint(1024, 768) : Parameters == TEXT("16x9") ? FIntPoint(1280, 720) : FIntPoint(1680, 720);
    if (Parameters != TEXT("4x3") && Parameters != TEXT("16x9") && Parameters != TEXT("21x9")) return false;
    if (!GEditor || !GEngine || !FApp::CanEverRender() || !FSlateApplication::IsInitialized() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("Inventory UI review requires a rendering editor and Slate; omit -nullrhi."));
        return false;
    }
    if (FSlateApplication::Get().IsRenderingOffScreen())
    {
        AddError(TEXT("Inventory UI video cancellation requires a real Windows window; omit -RenderOffscreen. UE FNullWindow fixes its mode to Fullscreen and cannot verify native Windowed restoration."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            AddError(TEXT("Close existing PIE before running inventory UI review."));
            return false;
        }
    }
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_InventoryUI_");
    bool bFreshName = Slot.Len() > Prefix.Len();
    for (TCHAR Character : Slot.Mid(Prefix.Len())) if (!FChar::IsAlnum(Character) && Character != TEXT('_')) bFreshName = false;
    if (!Slot.StartsWith(Prefix) || !bFreshName || UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        AddError(TEXT("Use -ProjectASaveSlot=ProjectA_Automation_InventoryUI_<fresh alphanumeric suffix>; user saves are never loaded or changed."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<InventoryUiPIE::FInventoryReview>(this, Slot, Size));
    return true;
}

#endif
