#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DevelopmentCoopSubsystem.generated.h"

class UNetDriver;

// Local transport UI state never supplies an authenticated account or restores a managed Run.
// 로컬 연결 UI 상태는 인증 계정을 제공하거나 관리 Run을 복원하지 않습니다.
UCLASS()
class PROJECTA_API UDevelopmentCoopSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static bool IsAvailable();
    static bool NormalizeAddress(const FString& Input, FString& Output);
    bool Host(APlayerController* Controller, int32 Capacity, FText& Error);
    bool Join(APlayerController* Controller, const FString& Address, FText& Error);
    void Leave(APlayerController* Controller);
    void ArriveAtMenu();
    void Connected();
    bool IsPending() const { return bPending; }
    const FText& GetStatus() const { return Status; }

private:
    bool CanTravel(APlayerController* Controller, FText& Error) const;
    void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Message);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Message);
    void RecordFailure(UWorld* World, const FString& Message);
    bool bPending = false;
    bool bActive = false;
    bool bResetAtMenu = false;
    FText Status;
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
};
