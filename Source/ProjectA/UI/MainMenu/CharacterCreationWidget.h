#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Game/Run/RunTypes.h"
#include "Components/ComboBoxString.h"
#include "CharacterCreationWidget.generated.h"

class UPartyDefinitionDataAsset;
class UButton;
class UBorder;
class UEditableTextBox;
class UHorizontalBox;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;

// Character creation screen widget base with temporary selection data.
// 임시 선택 데이터를 관리하는 캐릭터 생성 화면 위젯 기반 클래스입니다.
UCLASS()
class PROJECTA_API UCharacterCreationWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "CharacterCreation")
    TObjectPtr<UPartyDefinitionDataAsset> PartyDefinition;

    UFUNCTION(BlueprintCallable, Category = "CharacterCreation")
    void ShowSlotDetails(int32 SlotIndex, bool bEditable);
    UFUNCTION()
    void SaveSlotDetails();
    UFUNCTION()
    void CloseSlotDetails();

    // Sets the default selected character class.
    // 기본 선택 캐릭터 클래스를 설정합니다.
    UCharacterCreationWidget();

    // Stores the current character name.
    // 현재 캐릭터 이름을 저장합니다.
    UFUNCTION(BlueprintCallable, Category = "CharacterCreation")
    void SetCharacterName(const FText& NewName);

    // Stores the selected character class id.
    // 선택한 캐릭터 클래스 ID를 저장합니다.
    UFUNCTION(BlueprintCallable, Category = "CharacterCreation")
    void SelectCharacterClass(FName CharacterClassId);

    // Returns the current character name.
    // 현재 캐릭터 이름을 반환합니다.
    UFUNCTION(BlueprintPure, Category = "CharacterCreation")
    FText GetCharacterName() const;

    // Returns the selected character class id.
    // 선택한 캐릭터 클래스 ID를 반환합니다.
    UFUNCTION(BlueprintPure, Category = "CharacterCreation")
    FName GetCharacterClassId() const;

    // Returns display text for the selected class.
    // 선택한 클래스의 표시용 텍스트를 반환합니다.
    UFUNCTION(BlueprintPure, Category = "CharacterCreation")
    FText GetSelectedClassText() const;

    // Returns temporary stat preview text for the selected class.
    // 선택한 클래스의 임시 스탯 미리보기 텍스트를 반환합니다.
    UFUNCTION(BlueprintPure, Category = "CharacterCreation")
    FText GetStatPreviewText() const;

    // Requests closing the character creation screen.
    // 캐릭터 생성 화면 닫기를 요청합니다.
    UFUNCTION(BlueprintCallable, Category = "CharacterCreation")
    void RequestBack();

    // Requests starting the game with the current character data.
    // 현재 캐릭터 데이터로 게임 시작을 요청합니다.
    UFUNCTION(BlueprintCallable, Category = "CharacterCreation")
    void RequestStartGame();

    UFUNCTION(BlueprintPure, Category = "CharacterCreation|Party Slots")
    TArray<FRunPartyMember> GetPartyMembers() const;

    UFUNCTION(BlueprintCallable, Category = "CharacterCreation|Party Slots")
    void SetSlotCharacterName(int32 SlotIndex, const FText& NewName);

protected:
    // Initializes fallback character creation layout and events.
    // 대체 캐릭터 생성 레이아웃과 이벤트를 초기화합니다.
    virtual void NativeOnInitialized() override;
    virtual void NativeOnDeactivated() override;

    // Enables native C++ layout creation when designer widgets are absent.
    // 디자이너 위젯이 없을 때 네이티브 C++ 레이아웃 생성을 활성화합니다.
    UPROPERTY(EditDefaultsOnly, Category = "Character Creation|Code UI")
    bool bCreateLayoutInCode = true;

    // Fullscreen blocker optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 전체 화면 입력 차단 배경입니다.
    // TODO: Bind a UBorder named BackgroundBlocker in WBP_CharacterCreationWidget to block clicks behind the popup.
    // TODO: 팝업 뒤쪽 클릭을 차단하려면 WBP_CharacterCreationWidget에 BackgroundBlocker 이름의 UBorder를 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BackgroundBlocker;

    // Center panel background optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 중앙 패널 배경입니다.
    // TODO: Bind a UBorder named CenterPanelBackground in WBP_CharacterCreationWidget to edit the popup panel in Blueprint.
    // TODO: Blueprint에서 팝업 패널을 편집하려면 WBP_CharacterCreationWidget에 CenterPanelBackground 이름의 UBorder를 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CenterPanelBackground;

    // Name input optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 이름 입력창입니다.
    // TODO: Bind a UEditableTextBox named EditableTextBox_Name in WBP_CharacterCreationWidget for character name input.
    // TODO: 캐릭터 이름 입력을 위해 WBP_CharacterCreationWidget에 EditableTextBox_Name 이름의 UEditableTextBox를 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> EditableTextBox_Name;

    // Warrior class button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 전사 클래스 버튼입니다.
    // TODO: Bind a UButton named Button_Warrior in WBP_CharacterCreationWidget for Warrior selection.
    // TODO: Warrior 선택을 위해 WBP_CharacterCreationWidget에 Button_Warrior 이름의 UButton을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Warrior;

    // Archer class button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 궁수 클래스 버튼입니다.
    // TODO: Bind a UButton named Button_Archer in WBP_CharacterCreationWidget for Archer selection.
    // TODO: Archer 선택을 위해 WBP_CharacterCreationWidget에 Button_Archer 이름의 UButton을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Archer;

    // Mage class button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 마법사 클래스 버튼입니다.
    // TODO: Bind a UButton named Button_Mage in WBP_CharacterCreationWidget for Mage selection.
    // TODO: Mage 선택을 위해 WBP_CharacterCreationWidget에 Button_Mage 이름의 UButton을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Mage;

    // Selected class preview text optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 선택 클래스 표시 텍스트입니다.
    // TODO: Bind a UTextBlock named Text_SelectedClass in WBP_CharacterCreationWidget for selected class display.
    // TODO: 선택 클래스 표시를 위해 WBP_CharacterCreationWidget에 Text_SelectedClass 이름의 UTextBlock을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_SelectedClass;

    // Stat preview text optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 스탯 미리보기 텍스트입니다.
    // TODO: Bind a UTextBlock named Text_StatPreview in WBP_CharacterCreationWidget for stat preview updates.
    // TODO: 스탯 미리보기 갱신을 위해 WBP_CharacterCreationWidget에 Text_StatPreview 이름의 UTextBlock을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_StatPreview;

    // Back button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 뒤로가기 버튼입니다.
    // TODO: Bind a UButton named Button_Back in WBP_CharacterCreationWidget for closing the screen.
    // TODO: 화면 닫기를 위해 WBP_CharacterCreationWidget에 Button_Back 이름의 UButton을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Back;

    // Top-right close button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 우측 상단 닫기 버튼입니다.
    // TODO: Bind a UButton named Button_Close in WBP_CharacterCreationWidget for returning to the main menu.
    // TODO: 메인메뉴로 돌아가려면 WBP_CharacterCreationWidget에 Button_Close 이름의 UButton을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Close;

    // Start game button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 게임 시작 버튼입니다.
    // TODO: Bind a UButton named Button_StartGame in WBP_CharacterCreationWidget for starting the game.
    // TODO: 게임 시작을 위해 WBP_CharacterCreationWidget에 Button_StartGame 이름의 UButton을 바인딩합니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_StartGame;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_StartGameStatus;

    // Fullscreen hit-testable layer and bottom party card container bound from the designer.
    // 디자이너에서 바인딩되는 전체 화면 입력 차단 레이어와 하단 파티 카드 컨테이너입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> FullscreenInputBlocker;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BottomPanel;

    // Fixed-height wrapper for the party slots area.
    // 파티 슬롯 영역 높이를 고정하는 래퍼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> PartySlotsFixedHeightBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> BottomHorizontalBox;

    // Optional designer bindings for party slot zero.
    // 파티 슬롯 0의 선택적 디자이너 바인딩입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot0_Create;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> SlotEditorBox_0;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot0_Title;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot0_Prev;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot0_Next;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_Slot0_ClassIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot0_ClassName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot0_Edit;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot0_Delete;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot0_ClassInfo;

    // Optional designer bindings for party slot one.
    // 파티 슬롯 1의 선택적 디자이너 바인딩입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot1_Create;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> SlotEditorBox_1;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot1_Title;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot1_Prev;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot1_Next;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_Slot1_ClassIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot1_ClassName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot1_Edit;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot1_Delete;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot1_ClassInfo;

    // Optional designer bindings for party slot two.
    // 파티 슬롯 2의 선택적 디자이너 바인딩입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot2_Create;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> SlotEditorBox_2;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot2_Title;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot2_Prev;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot2_Next;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_Slot2_ClassIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot2_ClassName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot2_Edit;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot2_Delete;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot2_ClassInfo;

    // Optional designer bindings for party slot three.
    // 파티 슬롯 3의 선택적 디자이너 바인딩입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot3_Create;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> SlotEditorBox_3;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot3_Title;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot3_Prev;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot3_Next;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_Slot3_ClassIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Slot3_ClassName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot3_Edit;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot3_Delete;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Slot3_ClassInfo;

    // Optional class icon textures keyed by the same class ids used by the preview stage.
    // 프리뷰 스테이지와 같은 클래스 ID를 키로 사용하는 선택적 클래스 아이콘 텍스처입니다.
    UPROPERTY(EditDefaultsOnly, Category = "Character Creation|Party Slots")
    TMap<FName, TObjectPtr<UTexture2D>> ClassIconTextures;

private:
    // Creates the fallback character creation layout in C++.
    // C++에서 대체 캐릭터 생성 레이아웃을 생성합니다.
    void EnsureCodeGeneratedLayout();

    // Applies hit-testable background blocker settings.
    // 히트 테스트 가능한 배경 차단 설정을 적용합니다.
    void ConfigureBackgroundBlocker();

    // Applies visual settings for the center panel background.
    // 중앙 패널 배경의 시각 설정을 적용합니다.
    void ConfigureCenterPanelBackground();

    // Applies hit-testable settings to the new fullscreen input blocker.
    // 새 전체 화면 입력 차단기에 히트 테스트 가능한 설정을 적용합니다.
    void ConfigureFullscreenInputBlocker();

    // Applies the fixed logical height used by the party slot panel.
    // 파티 슬롯 패널에 사용하는 고정 logical height를 적용합니다.
    void ConfigurePartySlotsFixedHeight();

    // Initializes party slot bindings, state, and displayed class data.
    // 파티 슬롯 바인딩과 상태 및 표시 클래스 데이터를 초기화합니다.
    void InitializeClassSlotWidgetArrays();
    void InitializeClassSlots();
    void RefreshClassSlotWidgets();
    void ChangeSlotClass(int32 SlotIndex, int32 Direction);
    void SetSlotClass(int32 SlotIndex, FName ClassId);
    void CreateCharacterInSlot(int32 SlotIndex);
    void ClearCharacterSlot(int32 SlotIndex);
    void RefreshSlotVisibility(int32 SlotIndex);
    void SetWidgetVisible(UWidget* Widget, bool bIsVisible) const;
    bool IsSlotCreated(int32 SlotIndex) const;
    bool HasDeferredSlotCreationWidgets() const;
    FText GetDisplayNameForClassId(FName ClassId) const;
    void UpdatePreviewStageSlot(int32 SlotIndex, FName ClassId);
    void ClearPreviewStageSlot(int32 SlotIndex);
    void LogSlotAction(int32 SlotIndex, const TCHAR* ActionName) const;

    // Updates selected class and stat preview text.
    // 선택 클래스와 스탯 미리보기 텍스트를 갱신합니다.
    void RefreshPreview();

    // Creates a button and attaches it to the parent box.
    // 버튼을 생성하고 부모 박스에 추가합니다.
    UButton* CreateButton(UVerticalBox* ParentBox, const FText& ButtonText);

    // Creates button text and attaches it to the parent button.
    // 버튼 텍스트를 생성하고 부모 버튼에 추가합니다.
    UTextBlock* CreateButtonText(UButton* ParentButton, const FText& ButtonText);

    UFUNCTION()
    void HandleNameTextChanged(const FText& NewText);

    UFUNCTION()
    void HandleWarriorClicked();

    UFUNCTION()
    void HandleArcherClicked();

    UFUNCTION()
    void HandleMageClicked();

    UFUNCTION()
    void HandleBackClicked();

    UFUNCTION()
    void HandleCloseClicked();

    UFUNCTION()
    void HandleStartGameClicked();

    UFUNCTION()
    void HandleSlot0CreateClicked();

    UFUNCTION()
    void HandleSlot0PrevClicked();

    UFUNCTION()
    void HandleSlot0NextClicked();

    UFUNCTION()
    void HandleSlot0EditClicked();

    UFUNCTION()
    void HandleSlot0DeleteClicked();

    UFUNCTION()
    void HandleSlot0ClassInfoClicked();

    UFUNCTION()
    void HandleSlot1CreateClicked();

    UFUNCTION()
    void HandleSlot1PrevClicked();

    UFUNCTION()
    void HandleSlot1NextClicked();

    UFUNCTION()
    void HandleSlot1EditClicked();

    UFUNCTION()
    void HandleSlot1DeleteClicked();

    UFUNCTION()
    void HandleSlot1ClassInfoClicked();

    UFUNCTION()
    void HandleSlot2CreateClicked();

    UFUNCTION()
    void HandleSlot2PrevClicked();

    UFUNCTION()
    void HandleSlot2NextClicked();

    UFUNCTION()
    void HandleSlot2EditClicked();

    UFUNCTION()
    void HandleSlot2DeleteClicked();

    UFUNCTION()
    void HandleSlot2ClassInfoClicked();

    UFUNCTION()
    void HandleSlot3CreateClicked();

    UFUNCTION()
    void HandleSlot3PrevClicked();

    UFUNCTION()
    void HandleSlot3NextClicked();

    UFUNCTION()
    void HandleSlot3EditClicked();

    UFUNCTION()
    void HandleSlot3DeleteClicked();

    UFUNCTION()
    void HandleSlot3ClassInfoClicked();

    // Current character name entered by the user.
    // 사용자가 입력한 현재 캐릭터 이름입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation", meta = (AllowPrivateAccess = "true"))
    FText CurrentCharacterName;

    // Current character class id selected by the user.
    // 사용자가 선택한 현재 캐릭터 클래스 ID입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation", meta = (AllowPrivateAccess = "true"))
    FName CurrentCharacterClassId;

    // Current class selection for each of the four party slots.
    // 네 개 파티 슬롯 각각의 현재 클래스 선택입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation|Party Slots", meta = (AllowPrivateAccess = "true"))
    TArray<FName> SlotClassIds;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation|Party Slots", meta = (AllowPrivateAccess = "true"))
    TArray<FText> SlotCharacterNames;

    // Tracks whether each party slot currently has an active character editor.
    // 각 파티 슬롯에 활성 캐릭터 생성 패널이 있는지 추적합니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation|Party Slots", meta = (AllowPrivateAccess = "true"))
    TArray<uint8> SlotCreationStates;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> CreateSlotButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UVerticalBox>> SlotEditorBoxes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SlotTitleTexts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> PreviousClassButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> NextClassButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UImage>> ClassIconImages;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> ClassNameTexts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> EditButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> DeleteButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> ClassInfoButtons;
    void BuildDetailPanel();
    UFUNCTION()
    void HandleDetailClassChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UPROPERTY(Transient)
    TObjectPtr<UBorder> DetailPanel;
    UPROPERTY(Transient)
    TObjectPtr<UWidget> DetailUnderlyingRoot;
    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> DetailName;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> DetailClass;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailError;
    UPROPERTY(Transient)
    TObjectPtr<UButton> DetailSave;
    int32 DetailSlot = INDEX_NONE;
    bool bDetailEditable = false;
};
