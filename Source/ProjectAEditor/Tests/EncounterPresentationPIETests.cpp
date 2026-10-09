#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/EncounterStageVisualCatalog.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Run/RunEncounterPool.h"
#include "Game/Run/RunDungeonPlan.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "RunEncounterPIEHelpers.h"
#include "TodoReviewWindowPlacement.h"
#include "UI/Gameplay/InventoryWidget.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace EncounterPresentationPIE
{
    template <typename T>
    T* Active(UWorld* World)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (T* Screen = Cast<T>(Widget); Screen && Screen->IsActivated()) return Screen;
        return nullptr;
    }

    bool IsOwnedSlot(const FString& Slot)
    {
        const FString Prefix = TEXT("ProjectA_Automation_EncounterPresentation_");
        if (!Slot.StartsWith(Prefix) || Slot.Len() != Prefix.Len() + 32) return false;
        for (TCHAR Character : Slot.Mid(Prefix.Len())) if (!FChar::IsHexDigit(Character)) return false;
        return true;
    }

    // Only disposable encounter seeds are selected; normal-run balance and random frequencies are not exercised.
    // 일회성 인카운터 시드만 선택하며 정상 Run 밸런스나 무작위 출현 빈도는 검수하지 않습니다.
    class FReview : public IAutomationLatentCommand
    {
    public:
        FReview(FAutomationTestBase* InTest, FString InSlot, FString InOutput) : Test(InTest), Slot(MoveTemp(InSlot)), Output(MoveTemp(InOutput))
        {
            Groups = {FRunEncounterOffer::GetBasicItemShopTag(), FRunEncounterOffer::GetRarityItemShopTag(), FRunEncounterOffer::GetTagItemShopTag(), FRunEncounterOffer::GetRecoveryTag(), FRunEncounterOffer::GetConsumableShopTag(), FRunEncounterOffer::GetRevivalTag()};
            Sizes = {FIntPoint(960, 720), FIntPoint(1280, 720), FIntPoint(1260, 540)};
        }

        virtual ~FReview() override
        {
            ReleaseViewport();
        }

        virtual bool Update() override
        {
            if (Started == 0.0) Started = FPlatformTime::Seconds();
            if (Step == 99)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.WorldType != EWorldType::PIE) continue;
                    if (FPlatformTime::Seconds() - Started < 30.0) return false;
                    Check(false, TEXT("Presentation PIE teardown completed within its bounded wait."));
                    return Finish(false);
                }
                ReleaseViewport();
                if (IsOwnedSlot(Slot) && UGameplayStatics::DoesSaveGameExist(Slot, 0)) Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Only the owned disposable presentation save is removed after teardown."));
                Initial.Reset();
                Controller.Reset();
                Presented.Reset();
                if (bPassed && ++RatioIndex < Sizes.Num())
                {
                    GroupIndex = 0;
                    bViewportResizeRequested = false;
                    bViewportReady = false;
                    bViewportFailed = false;
                    ViewportResizeRetries = 0;
                    ViewportWarmFrames = 0;
                    Advance(0);
                    return false;
                }
                return Finish(true);
            }
            if (FPlatformTime::Seconds() - Started > 90.0)
            {
                Check(false, FString::Printf(TEXT("Presentation fixture timed out at step %d, ratio %d, group %d."), Step, RatioIndex, GroupIndex));
                return End();
            }
            if (Step == 0)
            {
                Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
                Settings->SetPlayNetMode(PIE_Standalone);
                Settings->SetPlayNumberOfClients(1);
                Settings->SetRunUnderOneProcess(true);
                Settings->bLaunchSeparateServer = false;
                Settings->NewWindowWidth = Sizes[RatioIndex].X;
                Settings->NewWindowHeight = Sizes[RatioIndex].Y;
                Settings->SetClientWindowSize(Sizes[RatioIndex]);
                if (!TodoReviewWindowPlacement::Configure(Test, Settings.Get())) return End();
                FRequestPlaySessionParams Params;
                Params.EditorPlaySettings = Settings.Get();
                Params.SessionDestination = EPlaySessionDestinationType::InProcess;
                Params.WorldType = EPlaySessionWorldType::PlayInEditor;
                Params.bAllowOnlineSubsystem = false;
                Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu");
                GEditor->RequestPlaySession(Params);
                Advance(1);
                return false;
            }
            UWorld* World = GEditor->PlayWorld;
            URunStateSubsystem* Run = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
            if (!World || !Run) return false;
            if (Step == 1)
            {
                UMainMenuScreenWidget* Menu = Active<UMainMenuScreenWidget>(World);
                if (!Menu) return false;
                ObserveViewport(World, TEXT("MainMenu"));
                if (!Click(Menu, TEXT("Button_NewGame"))) return End();
                Advance(2);
                return false;
            }
            if (Step == 2)
            {
                UGameModeSelectionWidget* Selection = Active<UGameModeSelectionWidget>(World);
                if (!Selection) return false;
                if (!Click(Selection, TEXT("Button_SinglePlayer"))) return End();
                Advance(3);
                return false;
            }
            if (Step == 3)
            {
                UCharacterCreationWidget* Creation = Active<UCharacterCreationWidget>(World);
                if (!Creation) return false;
                for (int32 Index = 0; Index < 4; ++Index) if (!Click(Creation, FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index)))) return End();
                if (!Click(Creation, TEXT("Button_Slot0_PlayerControl")) || !Click(Creation, TEXT("Button_StartGame"))) return End();
                Advance(4);
                return false;
            }
            Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            if (!Controller.IsValid()) return false;
            if (Step == 4)
            {
                if (Run->GetPhase() != ERunPhase::EncounterChoice || !Active<URunEncounterWidget>(World)) return false;
                if (!PrepareViewport(World)) return bViewportFailed ? End() : false;
                Initial.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
                if (!Check(Initial.IsValid() && Initial->Phase == ERunPhase::EncounterChoice && Initial->CompletedNodes.IsEmpty() && Initial->TargetRun.EncounterSelectionVersion == 1, TEXT("The fixture starts from the menu-created new Target before any combat or purchase."))) return End();
                Advance(5);
                return false;
            }
            if (Step == 5)
            {
                if (!PrepareOffer(Run)) return End();
                Advance(6);
                return false;
            }
            if (Step == 6)
            {
                URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), Screen) || !Warm()) return false;
                const int32 Index = Run->GetEncounterProgress().Offers.IndexOfByPredicate([this](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag() == Groups[GroupIndex]; });
                UButton* Button = RunEncounterPIE::FindChoiceButton(Screen, Index);
                if (!Check(Button && Button->GetIsEnabled(), TEXT("The real direction card is enabled at the dungeon junction."))) return End();
                const FName SelectedId = Run->GetEncounterProgress().Offers[Index].EncounterId;
                Button->OnClicked.Broadcast();
                if (!Check(Run->GetPhase() == ERunPhase::Shop && Run->GetEncounterProgress().SelectedEncounterId == SelectedId, TEXT("The real direction card commits the original server offer ID at the same index."))) return End();
                const AEncounterDungeonRoute* TravelingRoute = Cast<AEncounterDungeonRoute>(Controller->GetViewTarget());
                URunEncounterWidget* ArrivingShop = Active<URunEncounterWidget>(World);
                if (!Check(TravelingRoute && TravelingRoute->IsTraveling() && Controller->IsEncounterPresentationTransitioning() && ArrivingShop && !ArrivingShop->GetIsEnabled(), TEXT("A committed junction choice starts local corridor travel while shop actions remain disabled."))) return End();
                Advance(7);
                return false;
            }
            if (Step == 7)
            {
                URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), Screen) || !Warm()) return false;
                Presented = Cast<AEncounterPrototypeStage>(Controller->GetViewTarget());
                const FRunEncounterOffer* Offer = Run->GetEncounterProgress().FindSelectedOffer();
                if (!Check(Presented.IsValid() && Offer && Presented->MatchesOffer(*Offer) && Screen->GetIsEnabled(), TEXT("The committed shop finishes blending to its matching native NPC stage before enabling its panel."))) return End();
                const AEncounterDungeonRoute* ArrivedRoute = Cast<AEncounterDungeonRoute>(Presented->GetOwner());
                const int32 Direction = Run->GetEncounterProgress().Offers.IndexOfByPredicate([Offer](const FRunEncounterOffer& Entry) { return Entry.EncounterId == Offer->EncounterId; });
                const TArray<FVector> ArrivalPath = EncounterDungeonLayout::GetPath(Direction, ArrivedRoute ? ArrivedRoute->GetLayoutVariant() : INDEX_NONE);
                if (!Check(ArrivedRoute && !ArrivedRoute->IsTraveling() && ArrivedRoute->GetPresentedStage() == Presented.Get() && Presented->Camera && !ArrivalPath.IsEmpty() && Presented->Camera->GetComponentLocation().Equals(ArrivedRoute->GetActorTransform().TransformPosition(ArrivalPath.Last()), 1.f), TEXT("The displayed NPC and final camera belong to the selected direction's exact route endpoint."))) return End();
                const FRunDungeonState& Plan = Run->GetDungeonState();
                const int32 Visit = RunDungeonPlan::FindVisit(Plan, Run->GetEncounterProgress());
                const int32 NextVisit = Plan.Visits.IsValidIndex(Visit + 1) ? Visit + 1 : INDEX_NONE;
                if (!Check(Plan.Visits.IsValidIndex(Visit) && ArrivedRoute->GetLayoutVariant() == Plan.Visits[Visit].LayoutVariant && Controller->GetResidentDungeonRouteCount() >= 1 && Controller->GetResidentDungeonRouteCount() <= 2 && Controller->GetPreparedDungeonVisitIndex() == NextVisit, TEXT("The arrived room uses its frozen variant and keeps only the current and next logical visit resident."))) return End();
                if (!CheckLibraryVisuals() || !Capture(World, TEXT("Shop")) || !CheckFraming(World, Screen)) return End();
                BeforeInventory = Run->GetItemShopState();
                if (!Click(Screen, TEXT("Button_ShopInventory"))) return End();
                Advance(8);
                return false;
            }
            if (Step == 8)
            {
                UInventoryWidget* Inventory = Active<UInventoryWidget>(World);
                if (!Inventory || !Warm()) return false;
                if (!Check(Controller->GetViewTarget() == Presented.Get(), TEXT("Opening inventory preserves the active NPC camera.")) || !Capture(World, TEXT("Inventory")) || !Click(Inventory, TEXT("Button_CloseInventory"))) return End();
                Advance(9);
                return false;
            }
            if (Step == 9)
            {
                if (Active<UInventoryWidget>(World) || !Warm()) return false;
                URunEncounterWidget* Screen = Active<URunEncounterWidget>(World);
                if (!Check(Screen && Screen->GetIsEnabled() && Controller->GetViewTarget() == Presented.Get() && FRunItemShopState::StaticStruct()->CompareScriptStruct(&BeforeInventory, &Run->GetItemShopState(), 0), TEXT("Closing inventory preserves camera, stock, revision and reroll cost without restarting the shop.")) || !Click(Screen, TEXT("Button_LeaveShop"))) return End();
                Advance(10);
                return false;
            }
            if (Step == 10)
            {
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), Active<URunEncounterWidget>(World)) || !Warm()) return false;
                const AEncounterDungeonRoute* Route = Cast<AEncounterDungeonRoute>(Controller->GetViewTarget());
                if (!Check(Run->GetPhase() == ERunPhase::EncounterChoice && Route && !Route->IsTraveling() && (!Presented.IsValid() || !Presented->IsActorTickEnabled()), TEXT("Leaving returns to the idle dungeon junction and stops or releases the old NPC animation."))) return End();
                if (Presented.IsValid())
                {
                    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Presented.Get());
                    for (USkeletalMeshComponent* Mesh : Meshes)
                    {
                        const UAnimSingleNodeInstance* Animation = Mesh->GetSingleNodeInstance();
                        if (!Check(!Mesh->IsComponentTickEnabled() && (!Animation || !Animation->IsPlaying()), TEXT("A retained NPC also stops its independent skeletal component and idle playback after leaving."))) return End();
                    }
                }
                ++CompletedCases;
                if (++GroupIndex >= Groups.Num()) return End();
                Advance(5);
            }
            return false;
        }

    private:
        bool Check(bool Condition, const FString& Message)
        {
            bPassed &= Test->TestTrue(Message, Condition);
            return Condition;
        }

        void Advance(int32 Next)
        {
            Step = Next;
            Started = FPlatformTime::Seconds();
            WarmFrames = 0;
        }

        bool Warm()
        {
            return ++WarmFrames >= 12 && FPlatformTime::Seconds() - Started >= 1.1 && !IsAsyncLoading();
        }

        bool Click(UUserWidget* Screen, FName Name)
        {
            UButton* Button = Screen ? Cast<UButton>(Screen->GetWidgetFromName(Name)) : nullptr;
            if (!Check(Button && Button->GetIsEnabled() && Button->IsVisible(), TEXT("The actual enabled UI button exists: ") + Name.ToString())) return false;
            Button->OnClicked.Broadcast();
            return true;
        }

        void ObserveViewport(UWorld* World, const FString& Phase)
        {
            UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
            const FIntPoint Actual = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
            const FVector2D LocalSize = Widget.IsValid() ? FVector2D(Widget->GetCachedGeometry().GetLocalSize()) : FVector2D::ZeroVector;
            TSharedRef<FJsonObject> Observation = MakeShared<FJsonObject>();
            Observation->SetNumberField(TEXT("ratio_index"), RatioIndex);
            Observation->SetStringField(TEXT("phase"), Phase);
            Observation->SetStringField(TEXT("requested"), Sizes[RatioIndex].ToString());
            Observation->SetStringField(TEXT("actual"), Actual.ToString());
            Observation->SetStringField(TEXT("slate_local_size"), LocalSize.ToString());
            if (Window.IsValid())
            {
                Observation->SetStringField(TEXT("window_client_size"), Window->GetClientSizeInScreen().ToString());
                Observation->SetStringField(TEXT("window_outer_size"), Window->GetSizeInScreen().ToString());
                Observation->SetNumberField(TEXT("window_dpi"), Window->GetDPIScaleFactor());
            }
            ViewportObservations.Add(MakeShared<FJsonValueObject>(Observation));
            Test->AddInfo(FString::Printf(TEXT("Presentation viewport %s: ratio=%d requested=%s actual=%s Slate=%s client=%s outer=%s DPI=%.3f."), *Phase, RatioIndex, *Sizes[RatioIndex].ToString(), *Actual.ToString(), *LocalSize.ToString(), Window.IsValid() ? *Window->GetClientSizeInScreen().ToString() : TEXT("missing"), Window.IsValid() ? *Window->GetSizeInScreen().ToString() : TEXT("missing"), Window.IsValid() ? Window->GetDPIScaleFactor() : 0.0f));
        }

        // Measure the actual client after travel; correct title/DPI differences without changing saved video settings.
        // 트래블 이후 실제 클라이언트를 측정하고 저장된 영상 설정 변경 없이 제목 표시줄과 DPI 차이를 보정합니다.
        bool PrepareViewport(UWorld* World)
        {
            if (bViewportReady) return true;
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
            if (!Viewport || !Viewport->Viewport || !Window.IsValid()) return false;
            const FIntPoint Requested = Sizes[RatioIndex];
            if (!bViewportResizeRequested)
            {
                ObserveViewport(World, TEXT("GameplayBeforeResize"));
                Window->SetWindowMode(EWindowMode::Windowed);
                Window->Resize(FVector2D(Requested.X, Requested.Y));
                bViewportResizeRequested = true;
                ViewportResizeStarted = FPlatformTime::Seconds();
                return false;
            }
            if (++ViewportWarmFrames < 2 || FPlatformTime::Seconds() - ViewportResizeStarted < 0.25) return false;
            const FIntPoint Actual = Viewport->Viewport->GetSizeXY();
            if (Actual != Requested && ViewportResizeRetries < 3)
            {
                ObserveViewport(World, FString::Printf(TEXT("GameplayResizeRetry%d"), ViewportResizeRetries + 1));
                const FVector2D Correction(Requested.X - Actual.X, Requested.Y - Actual.Y);
                Window->Resize(Window->GetClientSizeInScreen() + Correction);
                ++ViewportResizeRetries;
                ViewportWarmFrames = 0;
                ViewportResizeStarted = FPlatformTime::Seconds();
                return false;
            }
            ObserveViewport(World, TEXT("GameplayAfterResize"));
            bViewportReady = Check(Actual == Requested, FString::Printf(TEXT("The actual post-travel viewport exactly matches the requested size: requested=%s actual=%s retries=%d."), *Requested.ToString(), *Actual.ToString(), ViewportResizeRetries));
            if (bViewportReady) bViewportReady = Check(TodoReviewWindowPlacement::Ensure(Test, World), TEXT("The resized presentation viewport remains on the requested monitor."));
            bViewportFailed = !bViewportReady;
            return bViewportReady;
        }

        bool PrepareOffer(URunStateSubsystem* Run)
        {
            TStrongObjectPtr<URunSaveGame> Fixture(DuplicateObject<URunSaveGame>(Initial.Get(), GetTransientPackage()));
            FText Error;
            bool bFound = false;
            for (int32 Seed = 1; Seed <= 4096; ++Seed)
            {
                Fixture->TargetRun.EncounterSeed = Seed;
                if (!RunEncounterPool::Select(Fixture->TargetRun, 0, 0, Fixture->EncounterProgress.Offers, Error)) return Check(false, TEXT("The original frozen encounter pool remains valid: ") + Error.ToString());
                const int32 Direction = GroupIndex % 3;
                if (!Fixture->EncounterProgress.Offers.IsValidIndex(Direction) || Fixture->EncounterProgress.Offers[Direction].GetResolvedTag() != Groups[GroupIndex]) continue;
                bFound = true;
                break;
            }
            if (!Check(bFound, TEXT("A bounded deterministic seed search exposes the requested authored encounter group."))) return false;
            if (!Check(RunDungeonPlan::Build(*Fixture.Get(), Fixture->DungeonState, Error), TEXT("The disposable seed-selected fixture freezes a matching complete dungeon plan: ") + Error.ToString())) return false;
            if (!Check(UGameplayStatics::SaveGameToSlot(Fixture.Get(), Slot, 0) && Run->LoadCheckpoint(Error), TEXT("Public save/load validation accepts only the owned seed-selected fixture: ") + Error.ToString())) return false;
            TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
            Record->SetStringField(TEXT("group"), Groups[GroupIndex].ToString());
            Record->SetNumberField(TEXT("encounter_seed"), Fixture->TargetRun.EncounterSeed);
            Record->SetStringField(TEXT("requested_viewport"), Sizes[RatioIndex].ToString());
            Records.Add(MakeShared<FJsonValueObject>(Record));
            return true;
        }

        bool CheckLibraryVisuals()
        {
            if (!Check(Presented.IsValid() && Presented->IsUsingLibraryVisuals() && !Presented->IsHidden() && !Presented->GetIsReplicated(), TEXT("The arrived stage uses visible library scenery without replicated gameplay authority."))) return false;
            const UEncounterStageVisualCatalog* Catalog = Presented->VisualCatalog ? Presented->VisualCatalog.Get() : GetDefault<UEncounterStageVisualCatalog>();
            const FEncounterStageVisualProfile* Profile = Catalog->Resolve(Presented->RequiredTags);
            if (!Check(Profile && Presented->GetLibraryProfileId() == Profile->ProfileId && Presented->GetLibraryPropCount() == Profile->Props.Num(), TEXT("The copied stage preserves the tag-selected library profile and full prop count."))) return false;
            TArray<FSoftObjectPath> ExpectedCharacters = {Profile->CharacterMesh.ToSoftObjectPath()};
            for (const auto& Part : Profile->CharacterParts) ExpectedCharacters.Add(Part.ToSoftObjectPath());
            USkeletalMeshComponent* Body = nullptr;
            TInlineComponentArray<USkeletalMeshComponent*> Characters(Presented.Get());
            for (USkeletalMeshComponent* Character : Characters)
            {
                if (!Character->GetSkeletalMeshAsset()) continue;
                const FSoftObjectPath Asset(Character->GetSkeletalMeshAsset());
                const int32 ExpectedIndex = ExpectedCharacters.IndexOfByKey(Asset);
                if (!Check(ExpectedIndex != INDEX_NONE && Character->IsVisible() && Character->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Character->CanEverAffectNavigation(), TEXT("Each rendered NPC body or modular part uses the selected source mesh without collision or navigation."))) return false;
                ExpectedCharacters.RemoveAt(ExpectedIndex);
                if (Asset == Profile->CharacterMesh.ToSoftObjectPath()) Body = Character;
            }
            if (!Check(ExpectedCharacters.IsEmpty() && Body && Body->IsComponentTickEnabled(), TEXT("All selected character parts exist and the actual body animation is ticking."))) return false;
            const UAnimSingleNodeInstance* Animation = Body->GetSingleNodeInstance();
            if (!Check(Animation && Animation->IsPlaying() && Animation->IsLooping() && FSoftObjectPath(Animation->GetAnimationAsset()) == Profile->IdleAnimation.ToSoftObjectPath() && Animation->GetAnimationAsset()->GetSkeleton() && Animation->GetAnimationAsset()->GetSkeleton()->IsCompatibleMesh(Body->GetSkeletalMeshAsset()), TEXT("The actual NPC plays its selected compatible source idle in a loop."))) return false;
            for (USkeletalMeshComponent* Character : Characters)
            {
                if (Character != Body && Character->GetSkeletalMeshAsset() && !Check(Character->LeaderPoseComponent.Get() == Body, TEXT("Every modular outfit part follows the actual animated body pose."))) return false;
            }
            TArray<FSoftObjectPath> ExpectedProps;
            for (const FEncounterStageProp& Prop : Profile->Props) ExpectedProps.Add(Prop.Mesh.ToSoftObjectPath());
            TInlineComponentArray<UStaticMeshComponent*> Shapes(Presented.Get());
            for (UStaticMeshComponent* Shape : Shapes)
            {
                if (!Shape->GetStaticMesh()) continue;
                const FSoftObjectPath Asset(Shape->GetStaticMesh());
                if (!Asset.GetLongPackageName().StartsWith(TEXT("/Game/"))) continue;
                const int32 ExpectedIndex = ExpectedProps.IndexOfByKey(Asset);
                if (!Check(ExpectedIndex != INDEX_NONE && Shape->IsVisible() && Shape->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Shape->CanEverAffectNavigation(), TEXT("Every library prop uses its selected source mesh without collision or navigation."))) return false;
                ExpectedProps.RemoveAt(ExpectedIndex);
            }
            if (!Check(ExpectedProps.IsEmpty(), TEXT("All selected library props are present in the arrived stage."))) return false;
            const TSharedPtr<FJsonObject> Record = Records.Last()->AsObject();
            Record->SetStringField(TEXT("library_profile"), Profile->ProfileId.ToString());
            Record->SetStringField(TEXT("npc_source_mesh"), Profile->CharacterMesh.ToString());
            Record->SetStringField(TEXT("npc_idle"), Profile->IdleAnimation.ToString());
            Record->SetNumberField(TEXT("library_prop_count"), Presented->GetLibraryPropCount());
            Record->SetBoolField(TEXT("library_components_verified"), true);
            return true;
        }

        bool CheckFraming(UWorld* World, URunEncounterWidget* Screen)
        {
            if (!TodoReviewWindowPlacement::Ensure(Test, World)) return Check(false, TEXT("The fixture remains on the requested monitor."));
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            UBorder* Panel = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("EncounterPanel")));
            FVector Focus = FVector::ZeroVector;
            float FocusRadius = 0.f;
            if (!Check(Widget.IsValid() && Viewport && Viewport->Viewport && Panel && Presented->Camera && Presented->GetPresentationFocus(Focus, FocusRadius) && !Focus.ContainsNaN() && FMath::IsFinite(FocusRadius) && FocusRadius > 0.f, TEXT("Actual viewport, merchant panel and active NPC face focus are available for projection."))) return false;
            const FIntPoint Size = Viewport->Viewport->GetSizeXY();
            const FGeometry& ViewGeometry = Widget->GetCachedGeometry();
            const FGeometry& PanelGeometry = Panel->GetCachedGeometry();
            FVector2D HeadPixel = FVector2D::ZeroVector;
            FVector2D HeadRightPixel = FVector2D::ZeroVector;
            const FVector HeadRight = Focus + Presented->Camera->GetRightVector() * FocusRadius;
            const bool bHeadProjected = Controller->ProjectWorldLocationToScreen(Focus, HeadPixel);
            const bool bHeadRightProjected = Controller->ProjectWorldLocationToScreen(HeadRight, HeadRightPixel);
            TSharedPtr<FJsonObject> Record = Records.Last()->AsObject();
            Record->SetStringField(TEXT("stage"), Presented->StageId.ToString());
            Record->SetStringField(TEXT("actual_viewport"), Size.ToString());
            Record->SetStringField(TEXT("viewport_local_size"), ViewGeometry.GetLocalSize().ToString());
            Record->SetStringField(TEXT("panel_local_size"), PanelGeometry.GetLocalSize().ToString());
            Record->SetStringField(TEXT("head_pixel"), HeadPixel.ToString());
            Record->SetStringField(TEXT("head_right_pixel"), HeadRightPixel.ToString());
            Record->SetBoolField(TEXT("head_projected"), bHeadProjected);
            Record->SetBoolField(TEXT("head_right_projected"), bHeadRightProjected);
            Test->AddInfo(FString::Printf(TEXT("Presentation framing: ratio=%d group=%s stage=%s requested=%s actual=%s viewportLocal=%s panelLocal=%s head=%s projected=%d right=%s projected=%d."), RatioIndex, *Groups[GroupIndex].ToString(), *Presented->StageId.ToString(), *Sizes[RatioIndex].ToString(), *Size.ToString(), *ViewGeometry.GetLocalSize().ToString(), *PanelGeometry.GetLocalSize().ToString(), *HeadPixel.ToString(), bHeadProjected, *HeadRightPixel.ToString(), bHeadRightProjected));
            bool bFramingReady = Check(Size == Sizes[RatioIndex], FString::Printf(TEXT("The actual shop viewport exactly matches the requested size: requested=%s actual=%s."), *Sizes[RatioIndex].ToString(), *Size.ToString()));
            bFramingReady &= Check(ViewGeometry.GetLocalSize().X > 0 && ViewGeometry.GetLocalSize().Y > 0, TEXT("The actual viewport has positive Slate geometry."));
            bFramingReady &= Check(PanelGeometry.GetLocalSize().X > 0 && PanelGeometry.GetLocalSize().Y > 0, TEXT("The actual merchant panel has positive Slate geometry."));
            bFramingReady &= Check(bHeadProjected, TEXT("The native NPC face center projects in front of the active camera."));
            bFramingReady &= Check(bHeadRightProjected, TEXT("The native NPC face right edge projects in front of the active camera."));
            if (!bFramingReady) return false;
            const double PanelLeft = ViewGeometry.AbsoluteToLocal(PanelGeometry.GetAbsolutePosition()).X / ViewGeometry.GetLocalSize().X;
            const double HeadX = HeadPixel.X / Size.X;
            const double HeadY = HeadPixel.Y / Size.Y;
            const double HeadRightX = HeadRightPixel.X / Size.X;
            Record->SetNumberField(TEXT("npc_head_x"), HeadX);
            Record->SetNumberField(TEXT("npc_head_y"), HeadY);
            Record->SetNumberField(TEXT("npc_head_right_x"), HeadRightX);
            Record->SetNumberField(TEXT("panel_left_x"), PanelLeft);
            return Check(HeadX > 0.05 && HeadX < 0.48 && HeadY > 0.05 && HeadY < 0.9 && HeadRightX < PanelLeft, FString::Printf(TEXT("The NPC face is on the left and clear of the actual merchant panel: head=(%.4f,%.4f) right=%.4f panelLeft=%.4f."), HeadX, HeadY, HeadRightX, PanelLeft));
        }

        bool Capture(UWorld* World, const FString& Suffix, bool bDiagnostic = false)
        {
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            TArray<FColor> Pixels;
            FIntVector Dimensions = FIntVector::ZeroValue;
            if (!Check(Widget.IsValid() && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Dimensions) && Dimensions.X > 0 && Dimensions.Y > 0 && Pixels.Num() == Dimensions.X * Dimensions.Y, TEXT("A fresh Slate screenshot captures this fixture's actual viewport."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Dimensions.X, Dimensions.Y, Pixels, PNG);
            const FString Path = Output / FString::Printf(TEXT("Ratio%d_Group%d_%s.png"), RatioIndex, GroupIndex, *Suffix);
            if (!Check(!PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *Path), TEXT("Fixture screenshots stay below Saved/Automation."))) return false;
            (bDiagnostic ? FailureCaptures : Captures).Add(MakeShared<FJsonValueString>(FPaths::ConvertRelativePathToFull(Path)));
            return true;
        }

        bool End()
        {
            UGameViewportClient* Viewport = GEditor->PlayWorld ? GEditor->PlayWorld->GetGameViewport() : nullptr;
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (!bPassed && Widget.IsValid()) Capture(GEditor->PlayWorld, FString::Printf(TEXT("Failure_Step%d"), Step), true);
            if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
            GEditor->RequestEndPlayMap();
            Advance(99);
            return false;
        }

        void ReleaseViewport()
        {
            if (!RetainedViewport.IsValid()) return;
            FlushRenderingCommands();
            RetainedViewport.Reset();
        }

        bool Finish(bool bCleaned)
        {
            TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
            const int32 ExpectedCases = Groups.Num() * Sizes.Num();
            Report->SetBoolField(TEXT("passed"), bPassed && CompletedCases == ExpectedCases && Captures.Num() == ExpectedCases * 2);
            Report->SetBoolField(TEXT("owned_slot_cleaned"), bCleaned && !UGameplayStatics::DoesSaveGameExist(Slot, 0));
            // Projection does not prove rendered visibility; decorative meshes can still cover the NPC face.
            // 투영 성공은 렌더링 가시성을 증명하지 않으며 장식 메시가 NPC 얼굴을 가릴 수 있습니다.
            Report->SetBoolField(TEXT("rendered_face_occlusion_verified"), false);
            const FString VisualReviewRequirement = TEXT("Directly inspect the saved Shop PNGs for all 6 encounter groups at all 3 ratios. Automatic projection/panel checks do not detect opaque stage decorations covering the NPC face. A passing fixture alone is not a completed visual review.");
            Report->SetStringField(TEXT("required_visual_review"), VisualReviewRequirement);
            Test->AddInfo(VisualReviewRequirement);
            Report->SetStringField(TEXT("scope"), TEXT("18 isolated UI fixtures: menu-created new Target, 6 authored encounter groups (basic, rarity, tag, recovery, consumable, revival) sharing 5 NPC stages x 3 actual viewport ratios, with 36 expected screenshots. Only each disposable initial save's EncounterSeed, seed-derived Offers and matching frozen dungeon plan are changed; pool, weights, party, gold, items and balance remain authored. Public save/load validates each fixture. Actual card/inventory/leave delegates, dungeon/NPC camera, selected source character/idle/props, skeletal cleanup, face/panel geometry and screenshots are observed. No combat, purchase, input-device navigation, random-frequency, multiplayer or normal-run completion claim."));
            Report->SetNumberField(TEXT("expected_cases"), ExpectedCases);
            Report->SetNumberField(TEXT("expected_captures"), ExpectedCases * 2);
            Report->SetNumberField(TEXT("completed_cases"), CompletedCases);
            Report->SetArrayField(TEXT("cases"), Records);
            Report->SetArrayField(TEXT("captures"), Captures);
            Report->SetArrayField(TEXT("failure_captures"), FailureCaptures);
            Report->SetArrayField(TEXT("viewport_observations"), ViewportObservations);
            FString Json;
            Check(FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Output / TEXT("summary.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM), TEXT("The exact fixture scope and results are written under Saved only."));
            return true;
        }

        FAutomationTestBase* Test;
        FString Slot;
        FString Output;
        TArray<FGameplayTag> Groups;
        TArray<FIntPoint> Sizes;
        TArray<TSharedPtr<FJsonValue>> Records;
        TArray<TSharedPtr<FJsonValue>> Captures;
        TArray<TSharedPtr<FJsonValue>> FailureCaptures;
        TArray<TSharedPtr<FJsonValue>> ViewportObservations;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TStrongObjectPtr<URunSaveGame> Initial;
        TWeakObjectPtr<AGameplayPlayerController> Controller;
        TWeakObjectPtr<AEncounterPrototypeStage> Presented;
        TSharedPtr<ISlateViewport> RetainedViewport;
        FRunItemShopState BeforeInventory;
        double Started = 0.0;
        double ViewportResizeStarted = 0.0;
        int32 Step = 0;
        int32 RatioIndex = 0;
        int32 GroupIndex = 0;
        int32 CompletedCases = 0;
        int32 WarmFrames = 0;
        int32 ViewportWarmFrames = 0;
        int32 ViewportResizeRetries = 0;
        bool bPassed = true;
        bool bViewportResizeRequested = false;
        bool bViewportReady = false;
        bool bViewportFailed = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterPresentationPIETest, "ProjectA.TodoReview.EncounterPresentationUI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterPresentationPIETest::RunTest(const FString& Parameters)
{
    FString Slot;
    FString RuntimeSlot;
    FString Output;
    if (!GEditor || !GEngine || !FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("EncounterPresentationUI requires a rendering Editor."));
        return false;
    }
    if (!FParse::Value(FCommandLine::Get(), TEXT("ProjectAEncounterPresentationSlot="), Slot) || !FParse::Value(FCommandLine::Get(), TEXT("ProjectASaveSlot="), RuntimeSlot) || !EncounterPresentationPIE::IsOwnedSlot(Slot) || Slot != RuntimeSlot || UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        AddError(TEXT("Supply one fresh identical ProjectA_Automation_EncounterPresentation_<32hex> slot through -ProjectAEncounterPresentationSlot and -ProjectASaveSlot; existing slots are refused."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE)
    {
        AddError(TEXT("Close the current PIE before the isolated presentation fixture."));
        return false;
    }
    if (!TodoReviewWindowPlacement::OutputRoot(this, Output)) return false;
    Output /= Slot;
    if (!IFileManager::Get().MakeDirectory(*Output, true)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<EncounterPresentationPIE::FReview>(this, Slot, Output));
    return true;
}

#endif
