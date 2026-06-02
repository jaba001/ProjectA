#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CharacterCreationWidget.generated.h"

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

private:
    // Current character name entered by the user.
    // 사용자가 입력한 현재 캐릭터 이름입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation", meta = (AllowPrivateAccess = "true"))
    FText CurrentCharacterName;

    // Current character class id selected by the user.
    // 사용자가 선택한 현재 캐릭터 클래스 ID입니다.
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCreation", meta = (AllowPrivateAccess = "true"))
    FName CurrentCharacterClassId;
};
