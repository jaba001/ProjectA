#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class URunSaveGame;

namespace RunContentMigration
{
    PROJECTA_API bool IsRemovedSkill(const FSoftObjectPath& Path);
    PROJECTA_API bool IsRemovedSkillId(FName SkillId);
    PROJECTA_API bool RemoveDeletedSkills(URunSaveGame& Save, FText& OutError);
}
