#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Run/RunTypes.h"
#include "MainMenuPlayerController.generated.h"

class UCharacterCreationWidget;
class UMainMenuRootWidget;
class UMainMenuScreenWidget;
class AMainMenuPreviewStage;
struct FManagedRunPreview;

// Player controller that creates and drives the CommonUI main menu.
// CommonUI 메인메뉴를 생성하고 제어하는 플레이어 컨트롤러입니다.
UCLASS()
class PROJECTA_API AMainMenuPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    // Sets default controller state for menu input.
    // 메뉴 입력을 위한 컨트롤러 기본 상태를 설정합니다.
    AMainMenuPlayerController();

    // Creates the root widget and shows the first main menu screen.
    // 루트 위젯을 생성하고 첫 메인메뉴 화면을 표시합니다.
    virtual void BeginPlay() override;

    // Returns the created main menu root widget.
    // 생성된 메인메뉴 루트 위젯을 반환합니다.
    UFUNCTION(BlueprintPure, Category = "MainMenu")
    UMainMenuRootWidget* GetMainMenuRootWidget() const;

    // Returns the preview stage actor found in L_MainMenu.
    // L_MainMenu에서 찾은 프리뷰 스테이지 액터를 반환합니다.
    UFUNCTION(BlueprintPure, Category = "MainMenu|Preview")
    AMainMenuPreviewStage* GetPreviewStage() const;

    // Pushes the main menu screen to the root main stack.
    // 메인메뉴 화면을 루트의 메인 스택에 추가합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void ShowMainMenuScreen();

    // Pushes the character creation screen to the menu stack.
    // 캐릭터 생성 화면을 메뉴 스택에 추가합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void ShowCharacterCreationScreen();

    // Keeps existing Blueprint calls valid while forwarding the active four-slot party.
    // 기존 Blueprint 호출을 유지하면서 활성 화면의 4개 파티 슬롯을 전달합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu", meta = (DeprecatedFunction, DeprecationMessage = "Use StartNewGameFromParty with the character creation slots."))
    void StartNewGameFromCharacterCreation(const FText& CharacterName, FName CharacterClassId);

    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    bool StartNewGameFromParty(const TArray<FRunPartyMember>& PartyMembers, FText& OutError);

    bool ContinueSavedGame(FText& OutError);

    // Read the selected shared development record using the configured caller, never an account supplied by UI.
    // UI가 입력한 계정 대신 설정된 호출자로 선택된 개발용 공유 기록을 읽습니다.
    bool GetManagedResumePreview(FManagedRunPreview& OutPreview, FText& OutError) const;
    bool ConvertManagedRunToSolo(FText& OutError);
    bool ContinueManagedSoloRun(FText& OutError);

protected:
    // Root widget class assigned by the main menu player controller blueprint.
    // 메인메뉴 플레이어 컨트롤러 블루프린트에서 지정하는 루트 위젯 클래스입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
    TSubclassOf<UMainMenuRootWidget> MainMenuRootWidgetClass;

    // Main menu screen class assigned by blueprint.
    // 블루프린트에서 지정하는 메인메뉴 화면 클래스입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
    TSubclassOf<UMainMenuScreenWidget> MainMenuScreenWidgetClass;

    // Character creation screen class assigned by blueprint.
    // 블루프린트에서 지정하는 캐릭터 생성 화면 클래스입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
    TSubclassOf<UCharacterCreationWidget> CharacterCreationWidgetClass;

    // Runtime instance of the root widget.
    // 런타임에 생성된 루트 위젯 인스턴스입니다.
    UPROPERTY(Transient)
    TObjectPtr<UMainMenuRootWidget> MainMenuRootWidget;

    // Runtime preview stage instance used for the menu camera and party previews.
    // 메뉴 카메라와 파티 프리뷰에 사용하는 런타임 프리뷰 스테이지 인스턴스입니다.
    UPROPERTY(Transient)
    TObjectPtr<AMainMenuPreviewStage> MainMenuPreviewStage;

    // Level name opened after character creation starts the game.
    // 캐릭터 생성 후 게임 시작 시 열릴 레벨 이름입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
    FName GameplayLevelName;

    UPROPERTY(Transient)
    TObjectPtr<UCharacterCreationWidget> ActiveCharacterCreationWidget;

private:
    bool StartManagedSolo(bool bConvert, FText& OutError);
};
