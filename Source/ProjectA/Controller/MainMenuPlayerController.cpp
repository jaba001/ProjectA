#include "Controller/MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
    bShowMouseCursor = true;
    bAutoManageActiveCameraTarget = false;
    GameplayLevelName = TEXT("/Game/User_JeHoon/LEVEL/Gameplay");
}

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = true;

    FInputModeUIOnly InputMode;
    SetInputMode(InputMode);

    MainMenuPreviewStage = Cast<AMainMenuPreviewStage>(UGameplayStatics::GetActorOfClass(this, AMainMenuPreviewStage::StaticClass()));

    if (MainMenuPreviewStage)
    {
        bAutoManageActiveCameraTarget = false;
        SetViewTarget(MainMenuPreviewStage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] MainMenuPreviewStage was not found in the level."));
    }

    if (!MainMenuRootWidgetClass)
    {
        MainMenuRootWidgetClass = UMainMenuRootWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] MainMenuRootWidgetClass is not set. Using native fallback."));
    }

    if (!MainMenuScreenWidgetClass)
    {
        MainMenuScreenWidgetClass = UMainMenuScreenWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] MainMenuScreenWidgetClass is not set. Using native fallback."));
    }

    if (!CharacterCreationWidgetClass)
    {
        CharacterCreationWidgetClass = UCharacterCreationWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] CharacterCreationWidgetClass is not set. Using native fallback."));
    }

    MainMenuRootWidget = CreateWidget<UMainMenuRootWidget>(this, MainMenuRootWidgetClass);

    if (!MainMenuRootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create MainMenuRootWidget."));
        return;
    }

    MainMenuRootWidget->AddToViewport();
    ShowMainMenuScreen();
}

UMainMenuRootWidget* AMainMenuPlayerController::GetMainMenuRootWidget() const
{
    return MainMenuRootWidget;
}

AMainMenuPreviewStage* AMainMenuPlayerController::GetPreviewStage() const
{
    return MainMenuPreviewStage;
}

void AMainMenuPlayerController::ShowMainMenuScreen()
{
    if (!MainMenuRootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuRootWidget is not available."));
        return;
    }

    MainMenuRootWidget->PushMainScreen(MainMenuScreenWidgetClass);
}

void AMainMenuPlayerController::ShowCharacterCreationScreen()
{
    if (!MainMenuRootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuRootWidget is not available."));
        return;
    }

    ActiveCharacterCreationWidget = Cast<UCharacterCreationWidget>(MainMenuRootWidget->PushMenuScreen(CharacterCreationWidgetClass));
}

void AMainMenuPlayerController::StartNewGameFromCharacterCreation(const FText& CharacterName, FName CharacterClassId)
{
    if (ActiveCharacterCreationWidget)
    {
        FText Error;
        StartNewGameFromParty(ActiveCharacterCreationWidget->GetPartyMembers(), Error);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] Start requires an active character creation screen with party slots."));
}

bool AMainMenuPlayerController::StartNewGameFromParty(const TArray<FRunPartyMember>& PartyMembers, FText& OutError)
{
    if (GameplayLevelName.IsNone() || !FPackageName::DoesPackageExist(GameplayLevelName.ToString()))
    {
        OutError = FText::FromString(TEXT("Gameplay level is missing. Create the configured Gameplay level first. / Gameplay 레벨을 먼저 생성해 주세요."));
        UE_LOG(LogTemp, Error, TEXT("[MainMenuPlayerController] GameplayLevelName does not exist: %s"), *GameplayLevelName.ToString());
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    URunStateSubsystem* RunState = nullptr;

    if (GameInstance)
    {
        RunState = GameInstance->GetSubsystem<URunStateSubsystem>();
    }

    if (!RunState)
    {
        OutError = FText::FromString(TEXT("Run state is unavailable. / 진행 상태를 사용할 수 없습니다."));
        return false;
    }

    if (!RunState->InitializeRun(PartyMembers, OutError))
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] Party validation failed: %s"), *OutError.ToString());
        return false;
    }

    if (!RunState->SaveCheckpoint(OutError))
    {
        return false;
    }
    RunState->EnableCheckpointSaving();
    UGameplayStatics::OpenLevel(this, GameplayLevelName);
    return true;
}

bool AMainMenuPlayerController::ContinueSavedGame(FText& OutError)
{
    if (GameplayLevelName.IsNone() || !FPackageName::DoesPackageExist(GameplayLevelName.ToString()))
    {
        OutError = FText::FromString(TEXT("Gameplay 레벨을 찾을 수 없습니다."));
        return false;
    }
    URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    if (!Run || !Run->LoadStandaloneCheckpoint(OutError))
    {
        return false;
    }
    UGameplayStatics::OpenLevel(this, GameplayLevelName);
    return true;
}

bool AMainMenuPlayerController::GetManagedResumePreview(FManagedRunPreview& OutPreview, FText& OutError) const
{
    OutError = NSLOCTEXT("ManagedRunMenu", "Unavailable", "이어갈 파티 저장과 본인 참가 정보가 필요합니다.");
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (!HasAuthority() || !IsLocalController() || GetNetMode() != NM_Standalone || !Run || !Run->GetManagedResumeTarget().IsValid()) return false;
    if (Run->HasManagedLease())
    {
        OutError = NSLOCTEXT("ManagedRunMenu", "Running", "현재 진행을 종료한 뒤 저장에서 이어갈 수 있습니다.");
        return false;
    }
    return Run->ReadManagedRun(Run->GetManagedResumeTarget(), OutPreview, OutError);
}

bool AMainMenuPlayerController::ConvertManagedRunToSolo(FText& OutError)
{
    return StartManagedSolo(true, OutError);
}

bool AMainMenuPlayerController::ContinueManagedSoloRun(FText& OutError)
{
    return StartManagedSolo(false, OutError);
}

bool AMainMenuPlayerController::StartManagedSolo(bool bConvert, FText& OutError)
{
    if (GameplayLevelName.IsNone() || !FPackageName::DoesPackageExist(GameplayLevelName.ToString()))
    {
        OutError = NSLOCTEXT("ManagedRunMenu", "MissingMap", "Gameplay 레벨을 찾을 수 없습니다.");
        return false;
    }
    FManagedRunPreview Preview;
    if (!GetManagedResumePreview(Preview, OutError)) return false;
    URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    const FRunAccountId Caller = Run->GetLocalCaller();
    if (!Preview.Participation.HumanParticipants.Contains(Caller) || (bConvert ? Preview.Participation.HumanParticipants.Num() <= 1 : Preview.Participation.HumanParticipants.Num() != 1))
    {
        OutError = NSLOCTEXT("ManagedRunMenu", "RosterChanged", "참가 상태가 바뀌었거나 이미 AI로 전환된 캐릭터입니다. 해당 Run에서는 인간 조작으로 복귀할 수 없습니다.");
        return false;
    }
    // Compare the freshly read stamp again during acquisition before publishing a new Host or opening gameplay.
    // 새 Host를 공개하거나 Gameplay를 열기 전에 획득 과정에서 방금 읽은 기록을 다시 대조합니다.
    if (!Run->ResumeManagedRun(Preview.Stamp, { Caller }, OutError)) return false;
    if (!Run->BeginManagedMenuTravel(OutError))
    {
        Run->CloseManagedRun();
        return false;
    }
    UGameplayStatics::OpenLevel(this, GameplayLevelName);
    return true;
}
