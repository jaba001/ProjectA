#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatGridGenerator.generated.h"

class ACombatGridTile;

UCLASS()
class PROJECTA_API ACombatGridGenerator : public AActor
{
    GENERATED_BODY()

public:
    ACombatGridGenerator();

protected:
    virtual void BeginPlay() override;

private:

    UPROPERTY(EditAnywhere, Category = "Grid")
    TSubclassOf<ACombatGridTile> TileClass;

    UPROPERTY(EditAnywhere, Category = "Grid")
    int32 RowCount = 4;

    UPROPERTY(EditAnywhere, Category = "Grid")
    int32 ColCount = 4;

    UPROPERTY(EditAnywhere, Category = "Grid")
    float Spacing = 200.f;

    UPROPERTY(EditAnywhere, Category = "Grid")
    float GapSpacing = 200.f;

    UPROPERTY(EditAnywhere, Category = "Grid")
    int32 GapStartIndex = 2; // Col 2부터 Gap 적용

public:

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void GenerateGrid();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FIntPoint, ACombatGridTile*> TileMap;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<ACombatGridTile*> GetTilesByCoords(const TArray<FIntPoint>& Coords) const;


};
