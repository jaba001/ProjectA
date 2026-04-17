


#include "Combat/SkillActorBase.h"

// Sets default values
ASkillActorBase::ASkillActorBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASkillActorBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASkillActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

