#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EncounterSignWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECTA_API UEncounterSignWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetTitle(const FText& InTitle);

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Title;

private:
    FText Title;
};
