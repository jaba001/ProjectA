#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BalanceSimCommandlet.generated.h"

UCLASS()
class PROJECTAEDITOR_API UBalanceSimCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UBalanceSimCommandlet();
    virtual int32 Main(const FString& Params) override;
};
