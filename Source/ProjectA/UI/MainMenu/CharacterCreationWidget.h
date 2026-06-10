#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CharacterCreationWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

// Character creation screen widget base with temporary selection data.
// 임시 선택 데이터를 관리하는 캐릭터 생성 화면 위젯 기반 클래스입니다.
UCLASS()
class PROJECTA_API UCharacterCreationWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
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

protected:
    // Initializes fallback character creation layout and events.
    // 대체 캐릭터 생성 레이아웃과 이벤트를 초기화합니다.
    virtual void NativeOnInitialized() override;

    // Enables native C++ layout creation when designer widgets are absent.
    // 디자이너 위젯이 없을 때 네이티브 C++ 레이아웃 생성을 활성화합니다.
    UPROPERTY(EditDefaultsOnly, Category = "Character Creation|Code UI")
    bool bCreateLayoutInCode = true;

    // Name input optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 이름 입력창입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> EditableTextBox_Name;

    // Warrior class button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 전사 클래스 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Warrior;

    // Archer class button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 궁수 클래스 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Archer;

    // Mage class button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 마법사 클래스 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Mage;

    // Selected class preview text optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 선택 클래스 표시 텍스트입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_SelectedClass;

    // Stat preview text optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 스탯 미리보기 텍스트입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_StatPreview;

    // Back button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 뒤로가기 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Back;

    // Start game button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 게임 시작 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_StartGame;

private:
    // Creates the fallback character creation layout in C++.
    // C++에서 대체 캐릭터 생성 레이아웃을 생성합니다.
    void EnsureCodeGeneratedLayout();

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
    void HandleStartGameClicked();

    // Current character name entered by the user.
    // 사용자가 입력한 현재 캐릭터 이름입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation", meta = (AllowPrivateAccess = "true"))
    FText CurrentCharacterName;

    // Current character class id selected by the user.
    // 사용자가 선택한 현재 캐릭터 클래스 ID입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation", meta = (AllowPrivateAccess = "true"))
    FName CurrentCharacterClassId;
};
